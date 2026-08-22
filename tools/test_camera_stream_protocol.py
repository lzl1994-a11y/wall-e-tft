#!/usr/bin/env python3
"""No-hardware WTFT image and serial NETCFG codec checks."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import camera_stream_server as wtft  # noqa: E402
import serial_network_config as netcfg  # noqa: E402


def expect_value_error(action) -> None:
    try:
        action()
    except ValueError:
        return
    raise AssertionError("ValueError was not raised")


def test_wtft_image_only() -> None:
    assert wtft.HEADER.size == 16
    assert {
        wtft.MSG_HELLO,
        wtft.MSG_PING,
        wtft.MSG_PONG,
        wtft.MSG_STREAM_START,
        wtft.MSG_JPEG_FRAME,
        wtft.MSG_STREAM_END,
    } == {1, 2, 3, 0x10, 0x11, 0x12}
    assert not any(name.startswith("MSG_NETWORK_") for name in dir(wtft))
    start = wtft.STREAM_START_PAYLOAD.pack(3000, 3000, 10, 0)
    assert len(start) == 12
    assert wtft.STREAM_START_PAYLOAD.unpack(start) == (3000, 3000, 10, 0)
    header = wtft.HEADER.pack(
        wtft.MAGIC, wtft.VERSION, wtft.MSG_JPEG_FRAME, 0, 9, 5
    )
    assert wtft.HEADER.unpack(header) == (
        b"WTFT", 1, wtft.MSG_JPEG_FRAME, 0, 9, 5
    )
    jpeg = b"\xff\xd8x\xff\xd9"
    assert jpeg[:2] == b"\xff\xd8" and jpeg[-2:] == b"\xff\xd9"
    assert wtft.MAX_JPEG_BYTES == 256 * 1024


def test_serial_codec_round_trip() -> None:
    config = netcfg.NetworkConfig(
        (("机器人", "秘密"), ("", ""), ("wifi3", "p")),
        "例子.local",
        9000,
    )
    line = netcfg.encode_set(42, config)
    assert len(line.encode("ascii")) <= 512
    assert "秘密" not in line and "秘密" not in repr(config)
    assert netcfg.encode_apply(7) == "netcfg:apply:7|1"
    assert netcfg.encode_query(8) == "netcfg:query:8|1"

    encoded_ssid = netcfg.b64url_encode("机器人", 32, "ssid")
    encoded_host = netcfg.b64url_encode("host", 64, "host")
    status = netcfg.decode_line(
        f"NETCFG:STATUS:8|1|0|255|{encoded_ssid}|||{encoded_host}|9000"
    )
    assert status.ssids == ("机器人", "", "")
    assert status.host == "host" and status.port == 9000
    assert "password" not in status.__dict__

    inactive = netcfg.decode_line("NETCFG:STATUS:9|1|0|255|||||0")
    assert inactive.host == "" and inactive.port == 0
    assert inactive.selected == 255

    accepted = netcfg.decode_line("NETCFG:RESULT:9|APPLY|1|0")
    assert not accepted.terminal
    for result in (2, 5, 6):
        assert netcfg.decode_line(
            f"NETCFG:RESULT:9|APPLY|{result}|0"
        ).terminal


def test_serial_boundaries_and_rejections() -> None:
    maximum = netcfg.NetworkConfig(
        (("s" * 32, "p" * 64),) * 3,
        "h" * 64,
        65535,
    )
    maximum_line = netcfg.encode_set(0xFFFFFFFF, maximum)
    assert len(maximum_line.encode("ascii")) <= 512

    invalid_configs = (
        netcfg.NetworkConfig((("s" * 33, ""), ("", ""), ("", "")), "h", 1),
        netcfg.NetworkConfig((("s", "p" * 65), ("", ""), ("", "")), "h", 1),
        netcfg.NetworkConfig((("s", ""), ("", ""), ("", "")), "h" * 65, 1),
        netcfg.NetworkConfig((("s", ""), ("", ""), ("", "")), "h", 0),
        netcfg.NetworkConfig((("s", ""), ("", ""), ("", "")), "h", 65536),
        netcfg.NetworkConfig((("", ""), ("", ""), ("", "")), "h", 1),
    )
    for config in invalid_configs:
        expect_value_error(lambda config=config: netcfg.encode_set(1, config))

    # Eleven three-byte characters exceed the 32-byte SSID wire limit.
    expect_value_error(
        lambda: netcfg.encode_set(
            1,
            netcfg.NetworkConfig(
                (("机" * 11, ""), ("", ""), ("", "")), "h", 1
            ),
        )
    )
    expect_value_error(lambda: netcfg.encode_apply(-1))
    expect_value_error(lambda: netcfg.encode_query(0x1_0000_0000))

    for bad in ("=", "A", "!!!", "Zh"):
        expect_value_error(lambda bad=bad: netcfg.b64url_decode(bad, 32, "x"))
    encoded_safe = netcfg.b64url_encode("safe", 32, "x")
    assert netcfg.b64url_decode(encoded_safe, 32, "x") == "safe"
    expect_value_error(
        lambda: netcfg.b64url_decode("AA", 32, "control character")
    )
    expect_value_error(
        lambda: netcfg.decode_line("NETCFG:RESULT:9|APPLY|7|0")
    )
    expect_value_error(
        lambda: netcfg.decode_line("NETCFG:STATUS:9|1|0|3|||||0")
    )


if __name__ == "__main__":
    test_wtft_image_only()
    test_serial_codec_round_trip()
    test_serial_boundaries_and_rejections()
    print("WTFT image and serial NETCFG tests passed")
