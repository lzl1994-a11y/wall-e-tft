#!/usr/bin/env python3
"""WALL-E TFT camera stream server.

The ESP32 is a persistent TCP client. Press Enter to send a three-second
240x240 JPEG preview; the firmware keeps the final frame for three seconds.
The send_preview() function can also be imported by the upper-computer app.
"""

from __future__ import annotations

import argparse
import queue
import socket
import struct
import threading
import time
from dataclasses import dataclass

MAGIC = b"WTFT"
VERSION = 1
HEADER = struct.Struct("!4sBBHII")
STREAM_START_PAYLOAD = struct.Struct("!IIHH")

MSG_HELLO = 0x01
MSG_PING = 0x02
MSG_PONG = 0x03
MSG_STREAM_START = 0x10
MSG_JPEG_FRAME = 0x11
MSG_STREAM_END = 0x12
MSG_NETWORK_CONFIG_SET = 0x20
MSG_NETWORK_CONFIG_RESULT = 0x21
MSG_NETWORK_CONFIG_APPLY = 0x22
MSG_NETWORK_CONFIG_QUERY = 0x23
MSG_NETWORK_CONFIG_STATUS = 0x24

NETWORK_PAYLOAD_VERSION = 1
NETWORK_CONFIG_RESULT = struct.Struct("!BBBB")
RESULT_STAGED = 0
RESULT_APPLY_ACCEPTED = 1
RESULT_TRIAL_CONNECTED_SAVED = 2
RESULT_VALIDATION_ERROR = 3
RESULT_NO_CANDIDATE = 4
RESULT_STORE_ERROR = 5
RESULT_TRIAL_FAILED_RESTORED = 6
TERMINAL_APPLY_RESULTS = {
    RESULT_TRIAL_CONNECTED_SAVED,
    RESULT_STORE_ERROR,
    RESULT_TRIAL_FAILED_RESTORED,
}


@dataclass(frozen=True)
class NetworkConfig:
    """Complete device network configuration used by SET (three Wi-Fi slots)."""

    wifi: tuple[tuple[str, str], tuple[str, str], tuple[str, str]]
    host: str
    port: int = 9000


@dataclass(frozen=True)
class NetworkConfigResult:
    operation: int
    result: int
    detail: int


@dataclass(frozen=True)
class NetworkConfigStatus:
    active_from_nvs: bool
    candidate_staged: bool
    trial_in_progress: bool
    selected_wifi_index: int | None
    ssids: tuple[str, str, str]
    host: str
    port: int


def _encoded_text(value: str, maximum: int, label: str, *, required: bool = False) -> bytes:
    encoded = value.encode("utf-8")
    if (required and not encoded) or len(encoded) > maximum:
        raise ValueError(f"{label} must be {'1..' if required else '0..'}{maximum} bytes")
    if any(byte < 0x20 or byte == 0x7f for byte in encoded):
        raise ValueError(f"{label} contains a control character")
    return encoded


def encode_network_config(config: NetworkConfig) -> bytes:
    """Encode NETWORK_CONFIG_SET payload version 1; credentials never log."""
    if len(config.wifi) != 3:
        raise ValueError("exactly three Wi-Fi entries are required")
    if not 1 <= config.port <= 65535:
        raise ValueError("TCP port must be 1..65535")
    payload = bytearray((NETWORK_PAYLOAD_VERSION, 3))
    for index, (ssid, password) in enumerate(config.wifi, 1):
        ssid_bytes = _encoded_text(ssid, 32, f"wifi {index} SSID")
        password_bytes = _encoded_text(password, 64, f"wifi {index} password")
        payload.append(len(ssid_bytes))
        payload.extend(ssid_bytes)
        payload.append(len(password_bytes))
        payload.extend(password_bytes)
    host = _encoded_text(config.host, 64, "image server host", required=True)
    payload.append(len(host))
    payload.extend(host)
    payload.extend(struct.pack("!H", config.port))
    return bytes(payload)


def decode_network_config_result(payload: bytes) -> NetworkConfigResult:
    if len(payload) != NETWORK_CONFIG_RESULT.size:
        raise ValueError("NETWORK_CONFIG_RESULT must be exactly 4 bytes")
    version, operation, result, detail = NETWORK_CONFIG_RESULT.unpack(payload)
    if version != NETWORK_PAYLOAD_VERSION:
        raise ValueError(f"unsupported network config result version {version}")
    return NetworkConfigResult(operation, result, detail)


def decode_network_config_status(payload: bytes) -> NetworkConfigStatus:
    """Decode STATUS. The wire format has no password fields by design."""
    if len(payload) < 6 or payload[0] != NETWORK_PAYLOAD_VERSION:
        raise ValueError("invalid NETWORK_CONFIG_STATUS payload")
    flags, selected, offset = payload[1], payload[2], 3
    ssids: list[str] = []
    for _ in range(3):
        if offset >= len(payload):
            raise ValueError("truncated STATUS SSID")
        length = payload[offset]
        offset += 1
        if length > 32 or offset + length > len(payload):
            raise ValueError("invalid STATUS SSID length")
        ssids.append(payload[offset:offset + length].decode("utf-8"))
        offset += length
    if offset >= len(payload):
        raise ValueError("truncated STATUS host")
    host_length = payload[offset]
    offset += 1
    if not 1 <= host_length <= 64 or offset + host_length + 2 != len(payload):
        raise ValueError("invalid STATUS host length")
    host = payload[offset:offset + host_length].decode("utf-8")
    offset += host_length
    port = struct.unpack("!H", payload[offset:offset + 2])[0]
    if port == 0:
        raise ValueError("invalid STATUS port")
    return NetworkConfigStatus(
        active_from_nvs=bool(flags & 0x01),
        candidate_staged=bool(flags & 0x02),
        trial_in_progress=bool(flags & 0x04),
        selected_wifi_index=None if selected == 0xFF else selected,
        ssids=(ssids[0], ssids[1], ssids[2]),
        host=host,
        port=port,
    )


def recv_exact(sock: socket.socket, length: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < length:
        chunk = sock.recv(length - len(chunks))
        if not chunk:
            raise ConnectionError("ESP32 disconnected")
        chunks.extend(chunk)
    return bytes(chunks)


@dataclass
class DeviceConnection:
    sock: socket.socket
    address: tuple[str, int]

    def __post_init__(self) -> None:
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.send_lock = threading.Lock()
        self.alive = True
        self.device_id = "unknown"
        self._responses: dict[tuple[int, int], bytes] = {}
        self._response_condition = threading.Condition()
        self._next_control_sequence = 0x80000000
        self.receiver = threading.Thread(target=self._receive_loop, daemon=True)
        self.receiver.start()

    def send_message(self, message_type: int, sequence: int,
                     payload: bytes = b"") -> None:
        header = HEADER.pack(
            MAGIC, VERSION, message_type, 0, sequence, len(payload)
        )
        with self.send_lock:
            self.sock.sendall(header)
            if payload:
                self.sock.sendall(payload)

    def _receive_loop(self) -> None:
        try:
            while self.alive:
                raw_header = recv_exact(self.sock, HEADER.size)
                magic, version, message_type, _flags, sequence, length = (
                    HEADER.unpack(raw_header)
                )
                if magic != MAGIC or version != VERSION:
                    raise ConnectionError("invalid WTFT protocol header")
                payload = recv_exact(self.sock, length) if length else b""
                if message_type == MSG_HELLO:
                    self.device_id = payload.decode("ascii", errors="replace")
                    print(f"ESP32 identified as {self.device_id}")
                elif message_type == MSG_PING:
                    self.send_message(MSG_PONG, sequence)
                elif message_type in (MSG_NETWORK_CONFIG_RESULT,
                                      MSG_NETWORK_CONFIG_STATUS):
                    with self._response_condition:
                        self._responses[(message_type, sequence)] = payload
                        self._response_condition.notify_all()
                    if message_type == MSG_NETWORK_CONFIG_RESULT:
                        result = decode_network_config_result(payload)
                        if (result.operation == MSG_NETWORK_CONFIG_APPLY and
                                result.result in TERMINAL_APPLY_RESULTS):
                            print(
                                "NETWORK_CONFIG_APPLY terminal "
                                f"sequence={sequence} result={result.result} "
                                f"detail={result.detail}"
                            )
        except (ConnectionError, OSError, ValueError) as exc:
            if self.alive:
                print(f"Connection closed: {exc}")
        finally:
            self.alive = False
            with self._response_condition:
                self._response_condition.notify_all()
            try:
                self.sock.close()
            except OSError:
                pass

    def close(self) -> None:
        self.alive = False
        try:
            self.sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self.sock.close()

    def next_control_sequence(self) -> int:
        with self._response_condition:
            sequence = self._next_control_sequence
            self._next_control_sequence = (sequence + 1) & 0xFFFFFFFF
            return sequence

    def wait_for_response(self, message_type: int, sequence: int,
                          timeout: float = 5.0) -> bytes:
        deadline = time.monotonic() + timeout
        with self._response_condition:
            while True:
                response = self._responses.pop((message_type, sequence), None)
                if response is not None:
                    return response
                if not self.alive:
                    break
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                self._response_condition.wait(remaining)
        raise TimeoutError(f"no WTFT response 0x{message_type:02X} for sequence {sequence}")


def set_network_config(connection: DeviceConnection, config: NetworkConfig, *,
                       sequence: int | None = None,
                       timeout: float = 5.0) -> NetworkConfigResult:
    """Stage a complete network candidate in ESP32 RAM; does not apply it."""
    sequence = connection.next_control_sequence() if sequence is None else sequence
    connection.send_message(MSG_NETWORK_CONFIG_SET, sequence,
                            encode_network_config(config))
    result = decode_network_config_result(
        connection.wait_for_response(MSG_NETWORK_CONFIG_RESULT, sequence, timeout)
    )
    if result.operation != MSG_NETWORK_CONFIG_SET:
        raise ValueError("unexpected RESULT operation for NETWORK_CONFIG_SET")
    return result


def apply_network_config(connection: DeviceConnection, *, sequence: int | None = None,
                          timeout: float = 5.0) -> NetworkConfigResult:
    """Request candidate application. Accepted ACK arrives before the 500 ms disconnect."""
    sequence = connection.next_control_sequence() if sequence is None else sequence
    connection.send_message(MSG_NETWORK_CONFIG_APPLY, sequence,
                            bytes((NETWORK_PAYLOAD_VERSION,)))
    result = decode_network_config_result(
        connection.wait_for_response(MSG_NETWORK_CONFIG_RESULT, sequence, timeout)
    )
    if result.operation != MSG_NETWORK_CONFIG_APPLY:
        raise ValueError("unexpected RESULT operation for NETWORK_CONFIG_APPLY")
    return result


def wait_for_network_config_terminal(
        connection: DeviceConnection, sequence: int, *,
        timeout: float = 65.0) -> NetworkConfigResult:
    """Wait on the post-switch connection for APPLY's terminal result.

    The caller must pass the APPLY sequence used on the old connection. On a
    successful trial, ``connection`` is the ESP32 session accepted by the new
    signal server. After rollback, it is the new session on the old server.
    """
    result = decode_network_config_result(
        connection.wait_for_response(MSG_NETWORK_CONFIG_RESULT, sequence, timeout)
    )
    if result.operation != MSG_NETWORK_CONFIG_APPLY:
        raise ValueError("unexpected terminal RESULT operation")
    if result.result not in TERMINAL_APPLY_RESULTS:
        raise ValueError(f"non-terminal APPLY result {result.result}")
    return result


def query_network_config(connection: DeviceConnection, *, sequence: int | None = None,
                         timeout: float = 5.0) -> NetworkConfigStatus:
    """Read visible active config only; returned data never contains passwords."""
    sequence = connection.next_control_sequence() if sequence is None else sequence
    connection.send_message(MSG_NETWORK_CONFIG_QUERY, sequence,
                            bytes((NETWORK_PAYLOAD_VERSION,)))
    return decode_network_config_status(
        connection.wait_for_response(MSG_NETWORK_CONFIG_STATUS, sequence, timeout)
    )


def square_crop(frame):
    height, width = frame.shape[:2]
    side = min(width, height)
    left = (width - side) // 2
    top = (height - side) // 2
    return frame[top:top + side, left:left + side]


def send_preview(connection: DeviceConnection, capture, *, fps: int = 10,
                 duration_ms: int = 3000, hold_ms: int = 3000,
                 jpeg_quality: int = 70, stream_sequence: int = 1) -> int:
    """Capture and send one timed preview. Returns the number of JPEG frames."""
    import cv2

    fps = max(1, min(fps, 20))
    duration_ms = max(250, min(duration_ms, 10_000))
    hold_ms = max(0, min(hold_ms, 10_000))
    jpeg_quality = max(30, min(jpeg_quality, 90))

    start_payload = STREAM_START_PAYLOAD.pack(
        duration_ms, hold_ms, fps, 0
    )
    connection.send_message(MSG_STREAM_START, stream_sequence, start_payload)

    interval = 1.0 / fps
    deadline = time.monotonic() + duration_ms / 1000.0
    next_frame_at = time.monotonic()
    frame_count = 0
    while connection.alive and time.monotonic() < deadline:
        ok, frame = capture.read()
        if not ok:
            raise RuntimeError("camera capture failed")
        frame = cv2.resize(square_crop(frame), (240, 240),
                           interpolation=cv2.INTER_AREA)
        encoded, jpeg = cv2.imencode(
            ".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, jpeg_quality]
        )
        if not encoded:
            raise RuntimeError("JPEG encoding failed")

        frame_count += 1
        frame_sequence = (stream_sequence << 16) | frame_count
        connection.send_message(MSG_JPEG_FRAME, frame_sequence, jpeg.tobytes())

        next_frame_at += interval
        remaining = next_frame_at - time.monotonic()
        if remaining > 0:
            time.sleep(remaining)

    connection.send_message(MSG_STREAM_END, stream_sequence)
    return frame_count


def open_camera(index: int):
    try:
        import cv2
    except ImportError as exc:
        raise SystemExit(
            "OpenCV is required: python -m pip install opencv-python"
        ) from exc

    capture = cv2.VideoCapture(index)
    if not capture.isOpened():
        raise SystemExit(f"Could not open camera index {index}")
    capture.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    return capture


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=9000)
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--fps", type=int, default=10)
    parser.add_argument("--duration-ms", type=int, default=3000)
    parser.add_argument("--hold-ms", type=int, default=3000)
    parser.add_argument("--jpeg-quality", type=int, default=70)
    parser.add_argument(
        "--auto-on-connect", action="store_true",
        help="send one preview immediately after every ESP32 connection",
    )
    parser.add_argument("--query-network-config", action="store_true",
                        help="print the active ESP32 Wi-Fi SSIDs and signal server")
    parser.add_argument("--set-wifi-1-ssid")
    parser.add_argument("--set-wifi-1-password")
    parser.add_argument("--set-wifi-2-ssid")
    parser.add_argument("--set-wifi-2-password")
    parser.add_argument("--set-wifi-3-ssid")
    parser.add_argument("--set-wifi-3-password")
    parser.add_argument("--set-image-host")
    parser.add_argument("--set-image-port", type=int, default=9000)
    parser.add_argument("--apply-network-config", action="store_true",
                        help="apply the staged --set-* config after its ACK")
    return parser.parse_args()


def config_from_args(args: argparse.Namespace) -> NetworkConfig | None:
    values = (
        args.set_wifi_1_ssid, args.set_wifi_1_password,
        args.set_wifi_2_ssid, args.set_wifi_2_password,
        args.set_wifi_3_ssid, args.set_wifi_3_password,
    )
    if args.set_image_host is None and not any(value is not None for value in values):
        if args.apply_network_config:
            raise SystemExit("--apply-network-config requires a complete --set-* config")
        return None
    if args.set_image_host is None:
        raise SystemExit("--set-image-host is required when setting network config")
    pairs = (
        (args.set_wifi_1_ssid or "", args.set_wifi_1_password or ""),
        (args.set_wifi_2_ssid or "", args.set_wifi_2_password or ""),
        (args.set_wifi_3_ssid or "", args.set_wifi_3_password or ""),
    )
    return NetworkConfig(pairs, args.set_image_host, args.set_image_port)


def stdin_trigger_loop(triggers: "queue.Queue[bool]") -> None:
    while True:
        try:
            input("Press Enter to trigger the 3s camera preview... ")
        except EOFError:
            return
        triggers.put(True)


def main() -> None:
    args = parse_args()
    requested_config = config_from_args(args)
    needs_camera = not args.query_network_config and requested_config is None
    capture = open_camera(args.camera) if needs_camera else None
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.bind, args.port))
    server.listen(1)
    print(f"Waiting for ESP32 on {args.bind}:{args.port}")
    triggers: "queue.Queue[bool]" = queue.Queue()
    if not args.auto_on_connect:
        threading.Thread(
            target=stdin_trigger_loop, args=(triggers,), daemon=True
        ).start()

    stream_sequence = 0
    try:
        while True:
            sock, address = server.accept()
            connection = DeviceConnection(sock, address)
            print(f"ESP32 connected from {address[0]}:{address[1]}")
            try:
                if args.query_network_config:
                    status = query_network_config(connection)
                    print(f"Active signal server: {status.host}:{status.port}; "
                          f"Wi-Fi SSIDs: {status.ssids}")
                if requested_config is not None:
                    result = set_network_config(connection, requested_config)
                    print(f"NETWORK_CONFIG_SET result={result.result} detail={result.detail}")
                    if args.apply_network_config and result.result == RESULT_STAGED:
                        apply_sequence = connection.next_control_sequence()
                        apply_result = apply_network_config(
                            connection, sequence=apply_sequence
                        )
                        print("NETWORK_CONFIG_APPLY "
                              f"result={apply_result.result} detail={apply_result.detail}")
                        if apply_result.result == RESULT_APPLY_ACCEPTED:
                            print(
                                "Trial started; terminal RESULT uses sequence "
                                f"{apply_sequence} on the destination server connection"
                            )
                if args.query_network_config or requested_config is not None:
                    # A configuration CLI invocation handles one connected unit.
                    return
                first = True
                while connection.alive:
                    if args.auto_on_connect and first:
                        first = False
                    else:
                        try:
                            triggers.get(timeout=0.2)
                        except queue.Empty:
                            continue
                    stream_sequence += 1
                    count = send_preview(
                        connection,
                        capture,
                        fps=args.fps,
                        duration_ms=args.duration_ms,
                        hold_ms=args.hold_ms,
                        jpeg_quality=args.jpeg_quality,
                        stream_sequence=stream_sequence,
                    )
                    print(f"Sent {count} frames; TFT now holds the final frame")
            except (ConnectionError, OSError, RuntimeError) as exc:
                print(f"Preview stopped: {exc}")
            finally:
                connection.close()
    except KeyboardInterrupt:
        print("Stopping server")
    finally:
        if capture is not None:
            capture.release()
        server.close()


if __name__ == "__main__":
    main()
