# Manual Test Checklist

## Bring-up

- Serial monitor shows `Wall-E Arduino serial display boot`.
- ST7789 shows the red/green/blue self-test and then stays on the power screen.
- GC9A01 eye screen initializes without snow or random pixels.
- Send `getname:WHO_ARE_YOU`; Serial replies `WALL_E_TFT`.

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
- While the GIF is playing, send `getname:WHO_ARE_YOU`; Serial should still reply `WALL_E_TFT`.
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

## Online Network Configuration

- Query the active configuration and confirm only SSIDs, host and port are
  returned; no password appears in the console or capture.
- SET a candidate with the first SSID empty and a valid second SSID. Verify
  result `0` (staged) and that no Wi-Fi switch happens before APPLY.
- APPLY it; verify result `1` arrives before the old TCP connection drops.
- Verify the candidate connects within 60 seconds, result `2` is delivered to
  the new server, and the configuration remains active after a reboot.
- After a valid NVS configuration has been saved, build once without local
  `Secrets.h`; verify the device still loads NVS and reconnects normally.
- Apply deliberately invalid credentials; verify the original Wi-Fi/server is
  restored and result `6` is delivered after reconnecting it.
- Cut power during the 60-second trial; after boot, verify old active NVS
  settings still apply.

## Failure Cases

- Empty Enter does not add a blank message.
- Input longer than `kInputMaxBytes` is truncated to the input buffer.
- UTF-8 Chinese input may render as mojibake because firmware does not transcode.

## Architecture Red Lines

- Network details stay inside `WifiImageStreamClient`; `AppController` depends
  only on `IImageStreamPort`.
- No model client is referenced by firmware.
- `AppController` does not include display driver headers.
- Display code does not include network or API configuration.
