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
- English, numbers, and common GB2312 Chinese characters render from the font flash.
- Send `openchat:0` to return to the power screen.

## Eye Action

- Send `eyeaction:zoom`.
- GC9A01 plays the embedded GIF.
- ST7789 does not change screens when only an eye action is sent.

## Failure Cases

- Empty Enter does not add a blank message.
- Input longer than `kInputMaxBytes` is truncated to the input buffer.
- UTF-8 Chinese input may render as mojibake because firmware does not transcode.

## Architecture Red Lines

- No network adapter is referenced by firmware.
- No model client is referenced by firmware.
- `AppController` does not include display driver headers.
- Display code does not include network or API configuration.
