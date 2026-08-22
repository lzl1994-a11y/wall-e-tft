#!/usr/bin/env python3
"""No-hardware checks for the WTFT network-configuration codec."""

from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import camera_stream_server as wtft  # noqa: E402


def test_set_encoding() -> None:
    config = wtft.NetworkConfig(
        (("robot", "secret-1"), ("workshop", "secret-2"), ("", "")),
        "192.168.4.1", 9000,
    )
    payload = wtft.encode_network_config(config)
    assert payload[:2] == b"\x01\x03"
    assert b"robot" in payload and b"192.168.4.1" in payload
    assert struct.unpack("!H", payload[-2:])[0] == 9000
    try:
        wtft.encode_network_config(
            wtft.NetworkConfig((("x" * 33, ""), ("", ""), ("", "")), "host", 1)
        )
    except ValueError:
        pass
    else:
        raise AssertionError("SSID length limit was not enforced")


def test_result_and_status() -> None:
    result = wtft.decode_network_config_result(bytes((1, 0x20, 0, 0)))
    assert result.operation == 0x20 and result.result == 0

    # version, flags, selected Wi-Fi, three SSID fields, host, port.
    payload = b"\x01\x03\x01\x05robot\x08workshop\x00\x0b192.168.4.1" + struct.pack("!H", 9000)
    status = wtft.decode_network_config_status(payload)
    assert status.active_from_nvs and status.candidate_staged
    assert status.selected_wifi_index == 1
    assert status.ssids == ("robot", "workshop", "")
    assert status.host == "192.168.4.1" and status.port == 9000
    # The status dataclass and raw wire format expose no password field/value.
    assert "password" not in status.__dict__
    assert b"secret" not in payload


def test_message_order() -> None:
    sequence = 17
    config = wtft.NetworkConfig((("a", "b"), ("", ""), ("", "")), "host", 9000)
    set_header = wtft.HEADER.pack(wtft.MAGIC, wtft.VERSION, wtft.MSG_NETWORK_CONFIG_SET,
                                  0, sequence, len(wtft.encode_network_config(config)))
    apply_header = wtft.HEADER.pack(wtft.MAGIC, wtft.VERSION, wtft.MSG_NETWORK_CONFIG_APPLY,
                                    0, sequence + 1, 1)
    assert wtft.HEADER.unpack(set_header)[2] == wtft.MSG_NETWORK_CONFIG_SET
    assert wtft.HEADER.unpack(apply_header)[2] == wtft.MSG_NETWORK_CONFIG_APPLY
    # The ESP32 contract is SET -> RESULT(STAGED) -> APPLY -> RESULT(ACCEPTED).
    assert [wtft.MSG_NETWORK_CONFIG_SET, wtft.MSG_NETWORK_CONFIG_RESULT,
            wtft.MSG_NETWORK_CONFIG_APPLY, wtft.MSG_NETWORK_CONFIG_RESULT] == [
                0x20, 0x21, 0x22, 0x21
            ]


class FakeConnection:
    def __init__(self, responses: list[bytes]) -> None:
        self.responses = responses
        self.messages: list[tuple[int, int, bytes]] = []
        self.sequence = 100

    def next_control_sequence(self) -> int:
        sequence = self.sequence
        self.sequence += 1
        return sequence

    def send_message(self, message_type: int, sequence: int,
                     payload: bytes = b"") -> None:
        self.messages.append((message_type, sequence, payload))

    def wait_for_response(self, message_type: int, sequence: int,
                          timeout: float = 5.0) -> bytes:
        assert message_type == wtft.MSG_NETWORK_CONFIG_RESULT
        assert sequence == 123
        assert timeout > 0
        return self.responses.pop(0)


def test_apply_ack_and_terminal_helpers() -> None:
    connection = FakeConnection([
        bytes((1, wtft.MSG_NETWORK_CONFIG_APPLY,
               wtft.RESULT_APPLY_ACCEPTED, 0)),
        bytes((1, wtft.MSG_NETWORK_CONFIG_APPLY,
               wtft.RESULT_TRIAL_CONNECTED_SAVED, 0)),
    ])
    ack = wtft.apply_network_config(connection, sequence=123)
    assert ack.result == wtft.RESULT_APPLY_ACCEPTED
    assert connection.messages == [
        (wtft.MSG_NETWORK_CONFIG_APPLY, 123, b"\x01")
    ]
    terminal = wtft.wait_for_network_config_terminal(
        connection, 123, timeout=65.0
    )
    assert terminal.result == wtft.RESULT_TRIAL_CONNECTED_SAVED

    invalid = FakeConnection([
        bytes((1, wtft.MSG_NETWORK_CONFIG_APPLY,
               wtft.RESULT_APPLY_ACCEPTED, 0)),
    ])
    try:
        wtft.wait_for_network_config_terminal(invalid, 123)
    except ValueError as exc:
        assert "non-terminal" in str(exc)
    else:
        raise AssertionError("accepted ACK was mistaken for a terminal result")


if __name__ == "__main__":
    test_set_encoding()
    test_result_and_status()
    test_message_order()
    test_apply_ack_and_terminal_helpers()
    print("WTFT network configuration protocol tests passed")
