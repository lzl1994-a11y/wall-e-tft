#!/usr/bin/env python3
"""WALL-E TFT camera/video stream server.

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
MAX_JPEG_BYTES = 256 * 1024


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
                if magic != MAGIC or version != VERSION or _flags != 0:
                    raise ConnectionError("invalid WTFT protocol header")
                if length > MAX_JPEG_BYTES:
                    raise ConnectionError("WTFT payload exceeds JPEG limit")
                payload = recv_exact(self.sock, length) if length else b""
                if message_type == MSG_HELLO:
                    self.device_id = payload.decode("ascii", errors="replace")
                    print(f"ESP32 identified as {self.device_id}")
                elif message_type == MSG_PING:
                    self.send_message(MSG_PONG, sequence)
                elif message_type != MSG_PONG:
                    raise ConnectionError(f"unsupported WTFT message 0x{message_type:02X}")
        except (ConnectionError, OSError, ValueError) as exc:
            if self.alive:
                print(f"Connection closed: {exc}")
        finally:
            self.alive = False
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



def fit_within_square(frame, size: int = 240):
    """Scale a frame to fit a square while preserving its original aspect ratio."""
    import cv2

    height, width = frame.shape[:2]
    if height <= 0 or width <= 0:
        raise ValueError("camera frame has invalid dimensions")
    scale = min(size / width, size / height)
    scaled_width = max(1, round(width * scale))
    scaled_height = max(1, round(height * scale))
    resized = cv2.resize(frame, (scaled_width, scaled_height),
                         interpolation=cv2.INTER_AREA)
    return resized


def send_preview(connection: DeviceConnection, capture, *, fps: int = 10,
                 duration_ms: int = 3000, hold_ms: int = 3000,
                 jpeg_quality: int = 70, stream_sequence: int = 1,
                 loop_media: bool = False) -> int:
    """Capture and send one timed preview. Returns the number of JPEG frames."""
    import cv2

    fps = max(1, min(fps, 30))
    duration_ms = max(250, min(duration_ms, 10_000))
    hold_ms = max(0, min(hold_ms, 10_000))
    jpeg_quality = max(30, min(jpeg_quality, 90))

    start_payload = STREAM_START_PAYLOAD.pack(
        duration_ms, hold_ms, fps, 0
    )
    connection.send_message(MSG_STREAM_START, stream_sequence, start_payload)
    started = True
    frame_count = 0
    try:
        interval = 1.0 / fps
        deadline = time.monotonic() + duration_ms / 1000.0
        next_frame_at = time.monotonic()
        while connection.alive and time.monotonic() < deadline:
            ok, frame = capture.read()
            if not ok and loop_media:
                capture.set(cv2.CAP_PROP_POS_FRAMES, 0)
                ok, frame = capture.read()
            if not ok:
                raise RuntimeError("media capture failed")
            frame = fit_within_square(frame)
            encoded, jpeg = cv2.imencode(
                ".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, jpeg_quality]
            )
            if not encoded:
                raise RuntimeError("JPEG encoding failed")
            jpeg_bytes = jpeg.tobytes()
            if (len(jpeg_bytes) > MAX_JPEG_BYTES or jpeg_bytes[:2] != b"\xff\xd8" or
                    jpeg_bytes[-2:] != b"\xff\xd9"):
                raise RuntimeError("invalid or oversized JPEG frame")

            frame_count += 1
            frame_sequence = (stream_sequence << 16) | frame_count
            connection.send_message(MSG_JPEG_FRAME, frame_sequence, jpeg_bytes)

            next_frame_at += interval
            remaining = next_frame_at - time.monotonic()
            if remaining > 0:
                time.sleep(remaining)
        return frame_count
    finally:
        if started and connection.alive:
            connection.send_message(MSG_STREAM_END, stream_sequence)


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


def open_video(path: str):
    try:
        import cv2
    except ImportError as exc:
        raise SystemExit(
            "OpenCV is required: python -m pip install opencv-python"
        ) from exc

    capture = cv2.VideoCapture(path)
    if not capture.isOpened():
        raise SystemExit(f"Could not open video file: {path}")
    return capture


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=9000)
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument(
        "--video", metavar="PATH",
        help="read a local video file instead of the camera; playback loops",
    )
    parser.add_argument("--fps", type=int, default=10)
    parser.add_argument("--duration-ms", type=int, default=3000)
    parser.add_argument("--hold-ms", type=int, default=3000)
    parser.add_argument("--jpeg-quality", type=int, default=70)
    parser.add_argument(
        "--auto-on-connect", action="store_true",
        help="send one preview immediately after every ESP32 connection",
    )
    parser.add_argument(
        "--continuous", action="store_true",
        help="continuously send previews while the ESP32 remains connected",
    )
    return parser.parse_args()


def stdin_trigger_loop(triggers: "queue.Queue[bool]") -> None:
    while True:
        try:
            input("Press Enter to trigger the 3s camera preview... ")
        except EOFError:
            return
        triggers.put(True)


def main() -> None:
    args = parse_args()
    if args.video:
        capture = open_video(args.video)
    else:
        capture = open_camera(args.camera)
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.bind, args.port))
    server.listen(1)
    print(f"Waiting for ESP32 on {args.bind}:{args.port}")
    triggers: "queue.Queue[bool]" = queue.Queue()
    if not args.auto_on_connect and not args.continuous:
        threading.Thread(
            target=stdin_trigger_loop, args=(triggers,), daemon=True
        ).start()

    stream_sequence = 0
    try:
        while True:
            server.settimeout(None)
            try:
                sock, address = server.accept()
            except socket.timeout:
                print("Timed out waiting for the ESP32 to reconnect")
                return
            connection = DeviceConnection(sock, address)
            print(f"ESP32 connected from {address[0]}:{address[1]}")
            try:
                first = True
                while connection.alive:
                    if args.continuous or (args.auto_on_connect and first):
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
                        loop_media=bool(args.video),
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
