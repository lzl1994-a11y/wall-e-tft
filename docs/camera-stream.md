# Camera preview and serial network configuration

WTFT/TCP is deliberately image-only. It carries only `HELLO` (`0x01`),
`PING`/`PONG` (`0x02`/`0x03`), `STREAM_START` (`0x10`), `JPEG_FRAME`
(`0x11`), and `STREAM_END` (`0x12`). It has no Wi-Fi, password, server, or
configuration API. The reference `tools/camera_stream_server.py` is therefore
only a camera sender/keepalive server.

The board keeps two 256 KiB PSRAM JPEG slots. A stream defaults to three
seconds at 10 FPS then keeps the final frame for three seconds; the normal
screen is restored afterward. A candidate switch stops image delivery while
Wi-Fi/TCP reconnects.

## Serial NETCFG v1

USB serial is the sole configuration and recovery channel, including on first
boot without valid Wi-Fi, bad credentials, or an unreachable TCP server. Lines
are CR/LF delimited and at most 512 bytes. Prefixes are case-insensitive; the
Base64url fields are UTF-8 bytes, URL-safe, and unpadded. Passwords are never
logged or returned.

```
netcfg:set:<seq>|1|<ssid1>|<pass1>|<ssid2>|<pass2>|<ssid3>|<pass3>|<host>|<port>
netcfg:apply:<seq>|1
netcfg:query:<seq>|1
```

Empty strings are empty fields. SSIDs are 0..32 bytes, passwords 0..64 bytes,
host is 1..64 bytes, port is 1..65535, and at least one SSID is required.
`SET` responds `NETCFG:RESULT:<seq>|SET|0|0` and stages RAM only. `APPLY`
responds `...|APPLY|1|0` completely before its 500 ms grace period. It then
tries for at most 60 seconds. After Wi-Fi association, TCP connect, and a
complete HELLO write, the candidate is saved to NVS and serial emits result 2.
NVS write failure is result 5; a failed trial rolls back to the old active
configuration and emits result 6. With no old active configuration it remains
in serial-configurable waiting state. Result 1 is not terminal.

`QUERY` returns:

```
NETCFG:STATUS:<seq>|1|<flags>|<selected>|<ssid1>|<ssid2>|<ssid3>|<host>|<port>
```

where flags are NVS active (bit 0), candidate present (bit 1), and
pending/running (bit 2); selected is 0..2 or 255. A no-config device reports
empty SSID/host fields and port 0. Error lines are
`NETCFG:RESULT:<seq>|<SET/APPLY/QUERY>|<result>|<detail>`; details 1..8 are
version, field count, SSID, password, host, port, length/state, and no Wi-Fi.

`tools/serial_network_config.py` supplies importable codec helpers and an
optional pyserial one-line CLI. Its `NetworkConfig` repr is intentionally
redacted. Use `encode_set`, `encode_apply`, and `encode_query` rather than
hand-writing credential-bearing commands.

## WTFT header

Every TCP message is `WTFT`, version 1, type, zero flags, u32 sequence, and
u32 payload length in network byte order. `STREAM_START` payload is
`duration_ms:u32, hold_ms:u32, fps:u16, reserved:u16`. A `duration_ms` value of
`0xFFFFFFFF` selects a persistent stream that remains active until
`STREAM_END` or disconnect. Persistent streams retain their last frame across
temporary frame gaps; bounded camera previews still enforce the frame-idle
timeout. Other duration values keep the bounded camera-preview behavior. JPEGs must be baseline,
no larger than 240×240 or 256 KiB. The ESP32 centers frames smaller than the
square TFT, preserving the camera's original aspect ratio with black bars.
