#!/usr/bin/env python3
"""Serial NETCFG v1 codec and optional pyserial one-line client."""
from __future__ import annotations
import argparse
import base64
import time
from dataclasses import dataclass

RESULT_STAGED, RESULT_ACCEPTED, RESULT_SAVED = 0, 1, 2
RESULT_VALIDATION, RESULT_NO_CANDIDATE, RESULT_STORE_ERROR, RESULT_TRIAL_FAILED = 3, 4, 5, 6
TERMINAL_APPLY_RESULTS = frozenset((RESULT_SAVED, RESULT_STORE_ERROR, RESULT_TRIAL_FAILED))

def _text(value: str, maximum: int, label: str, required: bool = False) -> bytes:
    raw = value.encode("utf-8")
    if (required and not raw) or len(raw) > maximum or any(b < 0x20 or b == 0x7f for b in raw): raise ValueError(f"invalid {label}")
    return raw
def b64url_encode(value: str, maximum: int, label: str, required: bool = False) -> str:
    return base64.urlsafe_b64encode(_text(value, maximum, label, required)).decode().rstrip("=")
def b64url_decode(value: str, maximum: int, label: str, required: bool = False) -> str:
    if "=" in value or any(c not in "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_" for c in value): raise ValueError(f"invalid {label} base64")
    try:
        raw = base64.urlsafe_b64decode(value + "=" * (-len(value) % 4))
        if base64.urlsafe_b64encode(raw).decode().rstrip("=") != value:
            raise ValueError(f"invalid {label} base64")
        text = raw.decode("utf-8")
    except (ValueError, UnicodeDecodeError) as exc: raise ValueError(f"invalid {label} base64") from exc
    _text(text, maximum, label, required); return text

@dataclass(frozen=True, repr=False)
class NetworkConfig:
    wifi: tuple[tuple[str, str], tuple[str, str], tuple[str, str]]; host: str; port: int
    def __repr__(self) -> str: return f"NetworkConfig(ssids={tuple(s for s, _ in self.wifi)!r}, host={self.host!r}, port={self.port})"
@dataclass(frozen=True)
class Result:
    sequence: int; operation: str; result: int; detail: int
    @property
    def terminal(self) -> bool: return self.operation == "APPLY" and self.result in TERMINAL_APPLY_RESULTS
@dataclass(frozen=True)
class Status:
    sequence: int; flags: int; selected: int; ssids: tuple[str, str, str]; host: str; port: int
def _seq(value: int) -> str:
    if not 0 <= value <= 0xffffffff: raise ValueError("invalid sequence")
    return str(value)
def encode_set(sequence: int, config: NetworkConfig) -> str:
    if len(config.wifi) != 3 or not 1 <= config.port <= 65535 or not any(s for s, _ in config.wifi): raise ValueError("invalid configuration")
    fields = [_seq(sequence), "1"]
    for i, (ssid, password) in enumerate(config.wifi, 1): fields += [b64url_encode(ssid, 32, f"ssid {i}"), b64url_encode(password, 64, f"password {i}")]
    fields += [b64url_encode(config.host, 64, "host", True), str(config.port)]
    line = "netcfg:set:" + "|".join(fields)
    if len(line.encode()) > 512: raise ValueError("SET line exceeds 512 bytes")
    return line
def encode_apply(sequence: int) -> str: return f"netcfg:apply:{_seq(sequence)}|1"
def encode_query(sequence: int) -> str: return f"netcfg:query:{_seq(sequence)}|1"
def decode_line(line: str) -> Result | Status:
    fields = line.strip("\r\n").split("|"); head = fields.pop(0) if fields else ""
    if head.startswith("NETCFG:RESULT:") and len(fields) == 3:
        sequence, operation, result, detail = int(head[14:]), fields[0], int(fields[1]), int(fields[2])
        if not 0 <= sequence <= 0xffffffff or operation not in ("SET", "APPLY", "QUERY") or not 0 <= result <= 6 or not 0 <= detail <= 8: raise ValueError("invalid NETCFG result")
        return Result(sequence, operation, result, detail)
    if head.startswith("NETCFG:STATUS:") and len(fields) == 8 and fields[0] == "1":
        sequence, flags, selected, port = int(head[14:]), int(fields[1]), int(fields[2]), int(fields[7])
        if not 0 <= sequence <= 0xffffffff or not 0 <= flags <= 7 or selected not in (0, 1, 2, 255) or not 0 <= port <= 65535: raise ValueError("invalid NETCFG status")
        ssids = tuple(b64url_decode(x, 32, "ssid") for x in fields[3:6])
        host = b64url_decode(fields[6], 64, "host")
        if port == 0 and host: raise ValueError("invalid inactive NETCFG status")
        if port and not host: raise ValueError("invalid active NETCFG status")
        return Status(sequence, flags, selected, ssids, host, port)
    raise ValueError("invalid NETCFG line")
def _request_identity(command: str) -> tuple[int, str]:
    lowered = command.lower()
    for prefix, operation in (("netcfg:set:", "SET"),
                              ("netcfg:apply:", "APPLY"),
                              ("netcfg:query:", "QUERY")):
        if lowered.startswith(prefix):
            sequence_text = command[len(prefix):].split("|", 1)[0]
            sequence = int(sequence_text)
            _seq(sequence)
            return sequence, operation
    raise ValueError("command is not a NETCFG request")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("port")
    parser.add_argument("command")
    parser.add_argument("--timeout", type=float, default=65.0)
    args = parser.parse_args()
    sequence, operation = _request_identity(args.command)
    try: import serial
    except ImportError as exc: raise SystemExit("pip install pyserial is required") from exc
    with serial.Serial(args.port, 115200, timeout=0.5) as device:
        device.write((args.command + "\r\n").encode("ascii"))
        deadline = time.monotonic() + max(0.1, args.timeout)
        while time.monotonic() < deadline:
            raw = device.readline().decode("utf-8", "replace")
            if not raw.startswith("NETCFG:"):
                continue
            response = decode_line(raw)
            if response.sequence != sequence:
                continue
            print(response)
            if isinstance(response, Status):
                return
            if response.operation != operation:
                continue
            if operation != "APPLY" or response.result != RESULT_ACCEPTED:
                return
        raise SystemExit("timed out waiting for NETCFG response")
if __name__ == "__main__": main()
