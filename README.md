# Wall-E-TFT (ESP32-S3)

**A robust multi-display firmware hub for the Wall-E / Wali multimodal robot interaction system.**

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus)
![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Embedded-E7352C?style=for-the-badge&logo=espressif)
![FreeRTOS](https://img.shields.io/badge/FreeRTOS-Task%20Oriented-23A455?style=for-the-badge)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Ready-F5822A?style=for-the-badge&logo=platformio)

> 中文：Wall-E-TFT 是瓦力（Wali）机器人多模态交互系统中的硬件显示中枢，负责三屏显示、串口通信握手、电量/状态 UI 和 AI 对话文本流渲染。  
> English: Wall-E-TFT is the hardware display hub of the Wall-E / Wali multimodal robot system, responsible for triple-display rendering, serial handshake, power/status UI, and AI dialogue text streaming.

---

## Demo / 演示

<!-- 中文：请将下方路径替换为你的实物运行动图，例如 docs/assets/wall-e-tft-demo.gif -->
![Wall-E-TFT real hardware demo placeholder](docs/assets/wall-e-tft-demo.gif)

---

## Key Features / 核心特性

- 🖥️ **Triple Display Pipeline / 三屏显示管线**  
  Drives two circular eye displays plus one main ST7789 display with clearly separated rendering responsibilities.

- 👀 **Synchronized Eye Animation / 双眼同步动画**  
  Designed for low-latency mirrored eye playback, suitable for expressive robot feedback such as zoom, blink, or attention states.

- 🔌 **Serial Handshake Decoupling / 串口握手解耦**  
  Supports host-side discovery through `getname:WHO_ARE_YOU` -> `WALL_E_TFT`, making it easy for an upstream Linux board such as RDK X3 to identify the display controller.

- 💬 **AI Dialogue Stream Rendering / AI 对话文本流显示**  
  Uses explicit `you:` and `ai:` message roles to keep user text and assistant text visually separated on the main screen.

- 🔋 **Partial-Update Power UI / 局部刷新的电量界面**  
  The power screen updates only changed battery blocks instead of repainting the whole screen, reducing flicker and improving perceived responsiveness.

- 🧱 **Engineering-First Design / 工程实用优先**  
  The project prioritizes deterministic state transitions, stable multi-screen rendering, and high first-pass success rate over pure visual decoration.

---

## Hardware / 硬件清单

| Module / 模块 | Recommended / 推荐 | Notes / 说明 |
| --- | --- | --- |
| MCU / 主控 | ESP32-S3 `__________` | Main controller for display scheduling and serial protocol. |
| Main TFT / 主屏 | ST7789 / ST7789V 240x240 `__________` | Shows power, hardware status, and AI dialogue text. |
| Eye TFT Left / 左眼屏 | GC9A01 240x240 round TFT `__________` | Circular eye display for animation playback. |
| Eye TFT Right / 右眼屏 | GC9A01 240x240 round TFT `__________` | Can mirror the left eye or use a dedicated CS topology. |
| Font Flash / 字库 Flash | GB2312 font flash `__________` | Used for GBK/GB2312 bitmap text rendering. |
| Host / 上位机 | RDK X3 / Linux board `__________` | Sends serial commands, AI text, power data, and eye actions. |

> 中文：当前固件不进行 UTF-8 到 GB2312/GBK 的编码转换；中文显示依赖上位机发送 GBK/GB2312 字节流。  
> English: The firmware does not transcode UTF-8 to GB2312/GBK. Chinese rendering expects GBK/GB2312 bytes from the host.

---

## Architecture / 系统架构

The ESP32-S3 Arduino runtime is built on top of FreeRTOS. Wall-E-TFT keeps communication, UI rendering, and eye animation boundaries clean so the firmware can be scheduled as deterministic tasks.

ESP32-S3 Arduino 运行时基于 FreeRTOS。Wall-E-TFT 在模块边界上明确拆分通信、主屏 UI 和眼睛动画逻辑，便于以确定性的任务模型调度。

| Task / 任务 | Responsibility / 职责 | Timing Goal / 时序目标 |
| --- | --- | --- |
| `Comm_Task` | Parse serial packets, handle handshake, dispatch `power:`, `openchat:`, `you:`, `ai:`, `eyeaction:` commands. | Never block on display refresh. |
| `UI_Task` | Render ST7789 power screen, status UI, and dialogue text stream with partial updates. | Avoid full-screen repaint unless mode changes. |
| `Eye_Task` | Play GC9A01 eye animation frames and keep both eye panels visually synchronized. | Keep animation latency stable. |
| `State_Task` | Maintain screen mode, chat idle timeout, latest power value, and system readiness state. | Ensure one-shot command flow succeeds. |

```text
Host / RDK X3 / Linux Board
        |
        |  USB CDC / UART
        v
+--------------------------+
|        Comm_Task         |
|  handshake + parser      |
+-----------+--------------+
            |
            v
+--------------------------+        +--------------------------+
|        State_Task        | -----> |         UI_Task          |
|  mode / timeout / power  |        | ST7789 power + dialogue  |
+-----------+--------------+        +--------------------------+
            |
            v
+--------------------------+
|        Eye_Task          |
|  dual GC9A01 animation   |
+--------------------------+
```

### Serial Protocol / 串口协议

| Command / 命令 | Direction / 方向 | Description / 描述 |
| --- | --- | --- |
| `getname:WHO_ARE_YOU` | Host -> ESP32-S3 | Device discovery request. |
| `WALL_E_TFT` | ESP32-S3 -> Host | Handshake response. |
| `power:0` ~ `power:100` | Host -> ESP32-S3 | Update the main power screen. |
| `openchat:1` | Host -> ESP32-S3 | Enter dialogue screen. |
| `openchat:0` | Host -> ESP32-S3 | Return to power screen. |
| `you:<bytes>` | Host -> ESP32-S3 | Append user message. |
| `ai:<bytes>` | Host -> ESP32-S3 | Append assistant message. |
| `eyeaction:zoom` | Host -> ESP32-S3 | Play the eye GIF animation. |

---

## Directory Tree / 目录结构

```text
Wall-E-TFT/
├── platformio.ini
├── README.md
├── docs/
│   └── manual-test.md
├── include/
│   └── README.keep              # Optional public headers / 可选公共头文件
└── src/
    ├── main.cpp                 # Arduino entry point / Arduino 入口
    ├── app/
    │   ├── AppController.h
    │   └── AppController.cpp    # State machine + command dispatch / 状态机与命令分发
    ├── config/
    │   └── Config.h             # Pins, SPI speed, runtime constants / 引脚与运行参数
    ├── domain/
    │   ├── AppState.h
    │   ├── ChatMessage.h
    │   ├── ChatSession.h
    │   └── ChatSession.cpp
    ├── ports/
    │   ├── IDisplayPort.h
    │   ├── IEyeDisplayPort.h
    │   ├── IInputPort.h
    │   └── ILogger.h
    └── adapters/
        ├── input/
        │   ├── SerialInputPort.h
        │   └── SerialInputPort.cpp
        ├── log/
        │   └── SerialLogger.h
        └── display/
            ├── St7789DisplayPort.h
            ├── St7789DisplayPort.cpp
            ├── Gc9a01EyeDisplay.h
            ├── Gc9a01EyeDisplay.cpp
            ├── Gb2312FontFlash.h
            ├── Gb2312FontFlash.cpp
            ├── SharedSpiBus.h
            └── eyeimg.h         # Embedded eye GIF data / 内置眼睛 GIF 数据
```

---

## Roadmap / 开发计划

- [x] Remove Wi-Fi and direct LLM request logic; keep the firmware as a serial-driven display controller.
- [x] Add host handshake command: `getname:WHO_ARE_YOU` -> `WALL_E_TFT`.
- [x] Implement ST7789 boot self-test and power screen.
- [x] Implement `power:`, `openchat:`, `you:`, `ai:`, and `eyeaction:` command parsing.
- [x] Add GC9A01 GIF playback for eye animation.
- [ ] Split runtime into explicit FreeRTOS tasks: `Comm_Task`, `UI_Task`, `Eye_Task`, and `State_Task`.
- [ ] Add non-blocking GIF playback so serial commands can be processed during eye animation.
- [ ] Add dual-eye topology options: mirrored CS, independent CS, or independent software SPI.
- [ ] Add protocol checksum or framed packet mode for noisy UART environments.
- [ ] Add CI build validation for PlatformIO.

---

## Build / 编译

```bash
pio run
```

## Upload / 烧录

```bash
pio run --target upload
```

## Serial Monitor / 串口监视器

```bash
pio device monitor -b 115200
```

---

## Maintainer Notes / 维护者说明

- Keep display drivers isolated behind ports and adapters.
- Avoid mixing protocol parsing with low-level SPI drawing code.
- Prefer partial redraws for the ST7789 main screen.
- Keep eye animation timing independent from dialogue text rendering.
- Treat host text encoding as an upstream responsibility.

---

## License / 许可证

This project is currently maintained as part of the Wall-E / Wali robot hardware stack.  
License: `__________`
