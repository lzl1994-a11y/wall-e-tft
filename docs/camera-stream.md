# Camera Preview over Wi-Fi

The ESP32-S3-WROOM-1-N16R8 stays connected to Wi-Fi and maintains a TCP
client connection to the upper computer. No image data is transferred while
idle. When the upper computer receives a photo command, it pushes three
seconds of JPEG frames. The TFT shows frames as they arrive, then keeps the
last frame for three seconds before restoring the previous power/chat screen.

## Configuration

Copy `src/config/Secrets.example.h` to `src/config/Secrets.h` and set up to
three hotspot/router credential pairs plus the upper-computer address.
`Secrets.h` is ignored by Git. Networks are attempted in numeric order; after
the configured reconnect interval (five seconds by default), a failed network
rotates to the next non-empty entry. Empty SSIDs are skipped.

The production target is configured for 16 MB QSPI flash and 8 MB OPI PSRAM.
Two compressed JPEG slots (256 KB each) and the eye animation canvas live in
PSRAM. DMA buffers remain in internal DMA-capable memory.

## Online network configuration

The same WTFT TCP connection can update all three Wi-Fi slots and the image
signal source (`host:port`). The ESP32 maintains an **active** configuration
(valid NVS, or `Secrets.h` defaults), a RAM-only **candidate**, and a temporary
**trial**. SET only stages the candidate. APPLY sends its ACK first, then after
500 ms trials the candidate Wi-Fi list and server for up to 60 seconds. Only a
successful Wi-Fi association plus TCP connection writes active NVS. Failure or
power loss during a trial therefore returns to the previously working config.
Passwords are never logged or returned by QUERY/STATUS. Use only on a trusted
LAN: the control connection has no encryption.

The reference server offers importable helpers:

```python
from camera_stream_server import (
    NetworkConfig,
    apply_network_config,
    set_network_config,
    wait_for_network_config_terminal,
)

candidate = NetworkConfig(
    (("robot-hotspot", "password-1"), ("workshop", "password-2"), ("", "")),
    "192.168.4.1", 9000,
)
set_network_config(connection, candidate)  # stages RAM only
apply_sequence = connection.next_control_sequence()
apply_network_config(connection, sequence=apply_sequence)  # pre-switch ACK
# After accepting the ESP32 on the candidate server (or old server on rollback):
terminal = wait_for_network_config_terminal(new_connection, apply_sequence)
```

The command-line configuration mode is deliberately one-shot: it never applies
the candidate a second time when the ESP32 reconnects. If the signal-server
port stays the same, the script keeps listening for up to 65 seconds and prints
the terminal result from the new connection. When changing the port, start the
destination server before APPLY; that server receives result `2`, while result
`5` or `6` reaches the restored old server. A production server must retain the
APPLY sequence and correlate it across ESP32 reconnects (normally by device ID).

```bash
python tools/camera_stream_server.py --port 9000 --query-network-config
python tools/camera_stream_server.py --port 9000 --set-wifi-1-ssid robot-hotspot --set-wifi-1-password password-1 --set-image-host 192.168.4.1 --set-image-port 9000 --apply-network-config
```

## Test server

Install OpenCV and start the included server on the upper computer:

```bash
python -m pip install opencv-python
python tools/camera_stream_server.py --port 9000 --fps 10
```

After the ESP32 connects, press Enter to trigger one preview. The public
`send_preview()` function can be imported into the real upper-computer
application and called when that application receives its photo command.

## Wire protocol

TCP uses one persistent, full-duplex connection. Every message starts with a
16-byte network-byte-order header:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | Magic `WTFT` |
| 4 | 1 | Version `1` |
| 5 | 1 | Message type |
| 6 | 2 | Flags, currently `0` |
| 8 | 4 | Sequence |
| 12 | 4 | Payload length |

Message types:

| Value | Name | Direction | Payload |
|---:|---|---|---|
| `0x01` | HELLO | ESP32 → host | ASCII device ID |
| `0x02` | PING | Either | Empty |
| `0x03` | PONG | Either | Empty |
| `0x10` | STREAM_START | Host → ESP32 | `duration_ms:u32`, `hold_ms:u32`, `fps:u16`, reserved `u16` |
| `0x11` | JPEG_FRAME | Host → ESP32 | Complete baseline JPEG |
| `0x12` | STREAM_END | Host → ESP32 | Empty |
| `0x20` | NETWORK_CONFIG_SET | Host → ESP32 | Complete candidate configuration |
| `0x21` | NETWORK_CONFIG_RESULT | ESP32 → Host | 4-byte result |
| `0x22` | NETWORK_CONFIG_APPLY | Host → ESP32 | One-byte payload version (`1`) |
| `0x23` | NETWORK_CONFIG_QUERY | Host → ESP32 | One-byte payload version (`1`) |
| `0x24` | NETWORK_CONFIG_STATUS | ESP32 → Host | Visible active configuration only |

All `NETWORK_*` payloads are version `1`, use UTF-8 text without control
characters, and use byte-counted lengths.

### `NETWORK_CONFIG_SET` (`0x20`) payload

| Field | Size | Notes |
|---|---:|---|
| version | u8 | `1` |
| wifi_count | u8 | Exactly `3` |
| Each of 3 slots: ssid_length + ssid | u8 + bytes | Length `0..32`; empty SSID is skipped |
| Each slot: password_length + password | u8 + bytes | Length `0..64` |
| host_length + host | u8 + bytes | Length `1..64`; IPv4 literal or domain |
| tcp_port | u16 | Big-endian, `1..65535` |

The payload length must exactly match those fields and at least one SSID must
be non-empty. SET validates and stages only; it never writes NVS.

### `NETWORK_CONFIG_RESULT` (`0x21`) payload

Exactly `version:u8, operation:u8, result:u8, detail:u8`; its header sequence
matches the request. Result values: `0` staged, `1` apply accepted, `2` trial
TCP connected and saved to NVS, `3` validation error, `4` no candidate, `5`
NVS store error, `6` trial failed and old configuration restored. Detail is
zero on success; validation details are `1` version, `2` Wi-Fi count, `3`
SSID length, `4` password length, `5` host length, `6` port, `7` total
length/state, `8` no usable Wi-Fi. APPLY's result `1` precedes the disconnect;
terminal result `2` is sent to the new server, and rollback result `6` after
the old server reconnects.

### `NETWORK_CONFIG_STATUS` (`0x24`) payload

`version:u8, flags:u8, selected_wifi_index:u8,` followed by the three
`ssid_length:u8 + ssid` fields, then `host_length:u8 + host + tcp_port:u16`.
Flags: bit0 active came from NVS, bit1 candidate staged, bit2 trial
pending/running. Wi-Fi index is `0..2`, or `255` before an attempt. STATUS has
**no password fields**. QUERY returns STATUS with the QUERY sequence.

JPEG frames must be baseline 240×240 images and no larger than 256 KB. The
default sender uses JPEG quality 70 and 10 FPS. There is no application-level
ACK per frame; TCP handles reliability and flow control.

The sender controls the frame rate by pacing complete JPEG messages. The
reference server accepts 1-20 FPS and defaults to a conservative 10 FPS. At
the end of each preview the firmware logs the measured display result, for
example `camera preview: 30 frames, 9.8 fps`, so 15 or 20 FPS can be evaluated
on the actual camera, access point, and TFT wiring without guessing.

## Runtime behavior

1. `STREAM_START` saves the current power/chat mode and shows CAMERA WAITING.
2. Each complete JPEG is decoded from PSRAM into RGB565 blocks.
3. Two internal DMA buffers push those blocks to the ST7789 over SPI2.
4. `STREAM_END` starts the final-frame hold timer.
5. After three seconds, the saved power/chat mode is redrawn.
6. Disconnect, a 750 ms frame stall, or the stream deadline plus one-second
   grace also terminates preview safely.

The GC9A01 eyes use SPI3, while the ST7789 uses SPI2, so their DMA transfers
can run independently. The ST7789 and font flash share SPI2 and are serialized;
font reads do not occur while camera frames own the main display.

The protocol is intentionally lightweight and has no authentication or
encryption. Run it only on the robot's trusted or isolated LAN.
