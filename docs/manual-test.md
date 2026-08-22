# Manual Test Checklist

## Bring-up

- Serial monitor shows `Wall-E Arduino serial display boot`.
- ST7789 shows the red/green/blue self-test and then stays on the power screen.
- GC9A01 eye screen initializes without snow or random pixels.
- Send `getname:WHO_ARE_YOU`; Serial replies `IAM:WALL_E_TFT`.

## Power Screen

- Send `power:0`; only the thick bottom bar is visible.
- Send `power:50`; several thin bars update without a full-screen redraw.
- Send `power:100`; all thin bars plus the thick bottom bar are visible.

## Serial Display

- Send `openchat:1` to enter the chat screen.
- Send `you:` plus one short GBK/GB2312 line through serial with CR/LF.
- Screen renders the input in the `YOU` bubble.
- Send `ai:` plus one short GBK/GB2312 line through serial with CR/LF.
- Screen renders the input in the `AI` bubble.
- Repeated lines scroll through the recent message list.
- With `TextFontSource::ScreenFontFlash`, English, numbers, and common double-byte Chinese glyphs render from the screen font flash.
- With `TextFontSource::ArduinoGfx`, English and numbers render without MISO/CSF; Chinese bytes are shown as placeholders.
- Send `openchat:0` to return to the power screen.

## Eye Action

- Send `eyeaction:zoom`.
- GC9A01 plays the embedded GIF.
- While the GIF is playing, send `getname:WHO_ARE_YOU`; Serial should still reply `IAM:WALL_E_TFT`.
- While the GIF is playing, send `power:` or `openchat:`; the main loop should continue processing new commands.
- ST7789 does not change screens when only an eye action is sent.

## Wi-Fi Camera Preview

- The board reports PSRAM and starts the Wi-Fi image-stream hot standby task.
- Configure only Wi-Fi entry 2; after the first entry times out, the board
  connects through entry 2 without rebooting.
- The ESP32 reconnects automatically when the image server starts later.
- Trigger one preview from `tools/camera_stream_server.py`.
- The ST7789 updates for three seconds at approximately 10 FPS.
- The final frame remains unchanged for three seconds.
- The previous power/chat screen is restored after the hold period.
- Eye animation and PCA9685/serial commands continue during preview.
- Stop the server mid-preview; the final valid frame is held, then the prior
  screen is restored without rebooting.
- Send an oversized, truncated, progressive, or non-240×240 JPEG; it is
  rejected without accessing memory outside the frame slots.

## Serial Network Configuration and Recovery

- Boot once with no valid `Secrets.h` Wi-Fi entries and no NVS config. Verify
  serial, screens, and PCA9685 remain usable; `netcfg:query:1|1` reports port
  `0`, then send a Base64url `netcfg:set` and confirm result `SET|0|0`.
- Send `netcfg:apply:2|1`. Verify serial result `APPLY|1|0` is complete before
  Wi-Fi/image traffic stops, then within 60 seconds get exactly one terminal
  `APPLY|2|0`; reboot and confirm NVS persists it.
- Configure an empty first slot and a valid second slot; QUERY must show the
  three Base64url SSIDs, selected slot, host and port but never a password.
- Apply deliberately wrong credentials or an unreachable server. Verify the
  old active connection returns and serial emits terminal `APPLY|6|0`; when
  there was no old active config, verify it stays ready for another serial SET.
- Simulate NVS write failure if practical and verify result `APPLY|5|0` and
  rollback. Cut power during a trial and verify the prior NVS active config is
  still used on next boot.
- During a switch, send QUERY and verify busy flag bit 2; SET/APPLY must be
  rejected. Confirm no partial/interleaved NETCFG serial lines under logging.
- Confirm TCP accepts only image/keepalive messages; its server has no network
  configuration flags or APIs.

## Failure Cases

- Empty Enter does not add a blank message.
- Input longer than `kInputMaxBytes` is truncated to the input buffer.
- UTF-8 Chinese input may render as mojibake because firmware does not transcode.

## Architecture Red Lines

- Network details stay inside `WifiImageStreamClient`; `AppController` depends
  only on `IImageStreamPort` and the abstract `INetworkConfigPort`, never
  directly on Wi-Fi or Preferences.
- No model client is referenced by firmware.
- `AppController` does not include display driver headers.
- Display code does not include network or API configuration.
