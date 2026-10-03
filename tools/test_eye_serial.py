#!/usr/bin/env python3
"""On-robot eye protocol regression using the host's existing SerialBridge.

Run only with the regular serial owner stopped. No motion/network commands are
sent. Eye settings are restored in finally; a timeout fails instead of retrying.
"""
import argparse
import json
import queue
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-project", default="/root/wall-e-bt")
    parser.add_argument("--count", type=int, default=300)
    parser.add_argument("--timeout", type=float, default=3.0)
    args = parser.parse_args()
    sys.path.insert(0, args.host_project)
    from services.hardware.serial_bridge import SerialBridge

    bridge = SerialBridge()
    replies = queue.Queue()
    bridge.add_line_listener(replies.put)
    times = []
    baseline = None

    def exchange(command, prefix="EYE:OK"):
        start = time.monotonic()
        if not bridge.send_raw(command + "\n", wake_screen=False):
            raise RuntimeError("write failed: " + command)
        while True:
            remaining = args.timeout - (time.monotonic() - start)
            if remaining <= 0:
                raise TimeoutError(command)
            try:
                line = replies.get(timeout=remaining)
            except queue.Empty:
                raise TimeoutError(command) from None
            if line.startswith(prefix):
                times.append(time.monotonic() - start)
                return line
            if line.startswith("EYE:"):
                raise RuntimeError("unexpected reply for " + command + ": " + line)

    def state():
        line = exchange("eyeconfig:query", "EYE:STATE:")
        return dict(item.split("=", 1) for item in line[10:].split(","))

    try:
        baseline = state()
        print("BASELINE " + json.dumps(baseline), flush=True)
        commands = [
            "eyeconfig:blinkMs=4600", "eyeconfig:blinkMs=4500",
            "eyeconfig:scale=0.4", "eyeconfig:scale=1.5",
            "eyeconfig:glow=8", "eyeconfig:glow=30",
            "eyeconfig:dots=0", "eyeconfig:dots=64",
            "eyeconfig:breathMs=500", "eyeconfig:breathMs=10000",
            "eyeaction:look:x=-26,y=26", "eyeaction:look:x=26,y=-26",
            "eyeconfig:ring=0", "eyeconfig:ring=1",
            "eyeconfig:mood=flame", "eyeconfig:mood=heart",
            "eyeconfig:mood=dot", "eyeconfig:blinkMs=0",
            "eyeconfig:blinkMs=1000", "eyeconfig:blinkMs=15000",
            "eyeconfig:brightness=0", "eyeconfig:brightness=1",
            "eyeconfig:ringBrightness=0", "eyeconfig:ringBrightness=1",
            "eyeconfig:dotBrightness=0", "eyeconfig:dotBrightness=1",
            "eyeconfig:color=00E5FF", "eyeconfig:ringColor=FF8800",
            "eyeconfig:dotColor=00FF91",
        ]
        # Do not widen the device's configured scale bounds for a test.
        commands[2] = "eyeconfig:scale=" + baseline["minScale"]
        commands[3] = "eyeconfig:scale=" + baseline["maxScale"]
        for index in range(args.count):
            exchange(commands[index % len(commands)])
            if index % 8 == 0:
                assert state()["ready"] == "1"
        for command in ("eyeconfig:dots=65", "eyeconfig:glow=31",
                        "eyeconfig:blinkMs=999", "eyeconfig:breathMs=499"):
            before = state()
            exchange(command, "EYE:ERR:invalid_config")
            assert state() == before, "invalid command changed state"
        exchange("eyeaction:blink")
        exchange("eyeaction:zoom")
        print("PASS " + json.dumps({"updates": args.count,
              "responses": len(times), "max_response_ms": round(max(times) * 1000, 2)}),
              flush=True)
    finally:
        try:
            if baseline is not None:
                fields = [key + "=" + value for key, value in baseline.items()
                          if key != "ready"]
                # Keep each config body below the firmware's 240-byte limit.
                batch = []
                for field in fields:
                    if len(",".join(batch + [field])) > 230:
                        exchange("eyeconfig:" + ",".join(batch))
                        batch = []
                    batch.append(field)
                if batch:
                    exchange("eyeconfig:" + ",".join(batch))
                assert state() == baseline, "restore mismatch"
                print("RESTORED", flush=True)
        finally:
            bridge.close()


if __name__ == "__main__":
    main()
