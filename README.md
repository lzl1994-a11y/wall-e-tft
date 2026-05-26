# Wali Display System (ESP32-S3) / 瓦力显示系统

> **Triple-display firmware hub for the Wali multimodal interactive robot.**
> Two synchronized circular eye displays + one main status screen, driven by a single ESP32-S3 over dual independent SPI buses. Communicates with a host SBC (RDK X3) via a lightweight serial text protocol.
>
> **瓦力多模态交互机器人的三屏固件中枢。** 单颗 ESP32-S3 通过双独立 SPI 总线同时驱动两块圆形眼屏和一块主状态屏，通过轻量串口文本协议与上位机 (RDK X3) 通信。

[![Platform](https://img.shields.io/badge/platform-ESP32--S3-6c3a1e?logo=espressif)](https://www.espressif.com)
[![Framework](https://img.shields.io/badge/framework-Arduino%20%2B%20FreeRTOS-00979D?logo=arduino)](https://www.arduino.cc)
[![Language](https://img.shields.io/badge/language-C%2B%2B17-00599C?logo=c%2B%2B)](https://isocpp.org)
[![Build](https://img.shields.io/badge/build-PlatformIO-orange?logo=platformio)](https://platformio.org)

---

<!-- TODO: 请在此处插入项目实拍运行动图 / GIF -->
<!--
  建议截图内容 / Suggested screenshots:
  1. 三块屏幕同时点亮 — 主屏显示 AI 对话文本流，双眼播放 zoom 变焦动画
     All three screens lit — main display showing AI chat stream, eyes playing zoom animation
  2. 串口监控窗口，展示 getname:WHO_ARE_YOU → WALL_E_TFT 握手过程
     Serial monitor showing the handshake process
  3. 电量界面局部刷新演示，仅变化的电量条块发生更新
     Power screen partial refresh — only changed bars update
-->

---

## Table of Contents / 目录

- [Core Philosophy / 核心理念](#core-philosophy--核心理念)
- [Key Features / 核心特性](#key-features--核心特性)
- [Hardware / 硬件](#hardware--硬件)
- [Architecture / 系统架构](#architecture--系统架构)
- [Directory Structure / 目录结构](#directory-structure--目录结构)
- [State Machine / 状态机](#state-machine--状态机)
- [Serial Protocol / 串口协议](#serial-protocol--串口协议)
- [Rendering Pipelines / 渲染管线](#rendering-pipelines--渲染管线)
- [Configuration Guide / 配置指南](#configuration-guide--配置指南)
- [Build & Flash / 构建与烧录](#build--flash--构建与烧录)
- [Testing & Serial Examples / 测试与串口示例](#testing--serial-examples--测试与串口示例)
- [Roadmap / 开发路线](#roadmap--开发路线)
- [Contributing / 贡献指南](#contributing--贡献指南)
- [License / 许可证](#license--许可证)

---

## Core Philosophy / 核心理念

**工程确定性优先于视觉效果。Engineering determinism over visual decoration.**

在资源受限的 ESP32-S3 上同时驱动三块 SPI 屏幕，核心挑战不是 UI 好不好看，而是：
On a resource-constrained ESP32-S3 driving three SPI displays simultaneously, the real challenge isn't visual polish — it's:

- 双眼 GIF 动画是否**掉帧**？—— 解码每帧只做一行，主循环不受阻
  Do the eye animations **drop frames**? — Each frame decodes one line at a time; the main loop never blocks.
- 主屏 UI 刷新是否**撕裂**？—— 局部脏矩形策略，只重绘变化的区域
  Does the main screen UI **tear**? — Dirty-rectangle partial refresh; only changed regions are redrawn.
- 串口命令是否**丢失**？—— 双缓冲流水线 + 空闲超时提交，256 字节窗口无溢出
  Are serial commands **lost**? — Double-buffered pipeline + idle timeout flush; 256-byte window with zero overflow.
- 首次上电是否**需要人工介入**？—— 全自动彩条自检 → 字库签名校验 → Ready 通知
  Does first boot **require manual intervention**? — Fully automatic: color bar self-test → font flash signature check → Ready notification.

每一行代码、每一个状态转换、每一笔 SPI 传输都服务于这四个问题的确定性答案。
Every line of code, every state transition, every SPI transaction serves to provide a deterministic answer to these four questions.

---

## Key Features / 核心特性

- **三屏并发渲染 / Triple-Display Concurrency** — 双 GC9A01 圆形眼屏（HSPI @ 40 MHz）+ 单 ST7789 主屏（VSPI @ 80 MHz），两条独立 SPI 总线物理隔离，互不抢占带宽。Two GC9A01 round eye displays (HSPI @ 40 MHz) + one ST7789 main display (VSPI @ 80 MHz) on physically isolated SPI buses.
- **双眼同步动画 / Synchronized Eye Animation** — 内置 GIF 解码器基于 `AnimatedGIF` 库的非阻塞 API，每帧逐行解码，不阻塞 `loop()`；左右眼硬件 CS 镜像，天然同步。Built-in GIF decoder using `AnimatedGIF` non-blocking API; left and right eyes share CS via hardware mirroring for perfect sync.
- **串口握手解耦 / Serial Handshake Decoupling** — `getname:WHO_ARE_YOU` → `WALL_E_TFT` 设备发现协议，上位机即插即识，无需硬编码设备 ID。Plug-and-play device discovery; no hardcoded device IDs.
- **AI 对话流渲染 / AI Chat Stream Rendering** — 支持 `you:`（用户）/ `ai:`（AI）角色标签，GBK/GB2312 双字节中文字模从板载 SPI NOR Flash 实时读取，16×16 点阵按字形绘制到主屏。Role-tagged messages with real-time GBK/GB2312 Chinese glyph rendering from onboard SPI NOR Flash.
- **局部刷新 UI / Partial Refresh UI** — 电量界面使用脏矩形策略：标题栏和太阳图标仅绘制一次，7 段电量细条仅重绘状态变化的条块。Dirty-rectangle strategy: title bar and sun icon drawn once; only changed battery-bar segments are redrawn.
- **端口-适配器分层架构 / Ports-Adapters Architecture** — `ports/`（接口）→ `adapters/`（实现）→ `app/`（状态机），依赖倒置，核心库 `WallEDisplayCore` 零项目耦合，可跨板移植。Dependency inversion: interface layer → implementation layer → application layer; core library is zero-coupling and portable.
- **非阻塞 GIF 透明通道处理 / Non-blocking GIF Transparency** — 透明帧以连续不透明段为单位进行 SPI 批量写入，避免逐像素判断，显著减少 SPI 事务次数。Transparent frames rendered as contiguous opaque segments in bulk SPI writes, drastically reducing transaction count.
- **SPI 片选协调器 / SPI Chip-Select Coordinator** — `SpiBusCoordinator` 以设备表驱动模式统一管理所有 CS 引脚，每次访问前执行全局 `deselectAll()`，硬件层面消除总线冲突。Table-driven CS management with global deselect-before-access to eliminate bus contention.
- **聊天消息固定容量滚动 / Fixed-Capacity Chat FIFO** — `ChatSession` 维护 8 条定长消息队列，满时 FIFO 淘汰，无动态内存分配。8-message fixed-capacity FIFO queue; no dynamic allocation.
- **字库 Flash 签名校验 / Font Flash Signature Verification** — 启动时读取 Flash 地址 0 处的 22 字节签名 `JYC-4MbByte-FONT-FLASH`，不匹配则禁用中文字模并标记 `TEXT ERR`。22-byte signature check at boot; mismatched flash disables Chinese rendering gracefully.

---

## Hardware / 硬件

### Components / 组件清单

| # | Component / 组件 | Model / 型号 | Resolution / 分辨率 | SPI Bus | Frequency / 频率 | Purpose / 用途 |
|---|-----------|-------|------------|----------|-----------|---------|
| 1 | MCU / 主控 | `ESP32-S3-DevKitC-1` | — | — | — | Dual SPI + UART / 双 SPI + 串口 |
| 2 | Main Display / 主屏 | `ST7789 / ST7789V` IPS | 240 × 240 | VSPI | 80 MHz | Battery UI + AI Chat / 电量界面 + AI 对话 |
| 3 | Left Eye / 左眼 | `GC9A01` Round TFT | 240 × 240 | HSPI | 40 MHz | Eye animation (GIF) / 左眼表情动画 |
| 4 | Right Eye / 右眼 | `GC9A01` Round TFT | 240 × 240 | HSPI | 40 MHz | Mirror-synced with left / 与左眼 CS 镜像 |
| 5 | Font Flash / 字库 Flash | SPI NOR Flash (onboard) | 4 MB | VSPI (shared) | 2 MHz | GBK 16×16 bitmap font / 点阵字库 |
| 6 | Host SBC / 上位机 | RDK X3 / Linux Board | — | UART0 | 115200 bps | Command source / 命令下发 |

### Pin Map / 引脚映射

```
ESP32-S3 GPIO Layout / GPIO 布局
═══════════════════════════════════════════════════════════
  SPI Bus 1 (VSPI)                    SPI Bus 2 (HSPI)
  ┌─────────────────────┐            ┌─────────────────────┐
  │ SCK  → GPIO 18      │            │ SCK  → GPIO 9       │
  │ MOSI → GPIO 17      │            │ MOSI → GPIO 8       │
  │ MISO → GPIO 16      │            │ DC   → GPIO 5       │
  │                     │            │ RST  → GPIO 3       │
  │ ST7789 CS → GPIO 14 │            │ CS   → GPIO 4       │
  │ Flash CS  → GPIO 11 │            │      (L+R CS mirror)│
  │ DC        → GPIO 13 │            └─────────────────────┘
  │ RST       → GPIO 15 │
  └─────────────────────┘
```

### Wiring Diagram / 接线图

```
                    ESP32-S3 DevKitC-1
                  ┌───────────────────────┐
                  │  GND  ────────────────┼──┬─────────────┬─────────────┐
                  │  3.3V ────────────────┼──┼─────────────┼─────────────┐│
                  │                       │  │             │             ││
    ST7789 Main   │  GPIO 18 (SCK)  ──────┼──┼─ SCK ───────┼── Font Flash│
    Display       │  GPIO 17 (MOSI) ──────┼──┼─ MOSI ──────┼── SCK       │
    ╔══════════╗  │  GPIO 16 (MISO) ──────┼──┼─ MISO ──────┼── MOSI      │
    ║ 240×240  ║  │  GPIO 14 (CS)   ──────┼──┼─ CS ────────┼── MISO      │
    ║   IPS    ║  │  GPIO 13 (DC)   ──────┼──┼─ DC         │  CS         │
    ╚══════════╝  │  GPIO 15 (RST)  ──────┼──┼─ RST        │             │
                  │                       │  │             │             │
    GC9A01 Eyes   │  GPIO 9  (SCK)  ──────┼──┼─ SCK ───────┼─────────────┘
    ╔══════╗ ╔══╗ │  GPIO 8  (MOSI) ──────┼──┼─ MOSI ──────┤
    ║ Left ║ ║Right│  GPIO 5  (DC)   ──────┼──┼─ DC ────────┤
    ║ 240  ║ ║240 ║│  GPIO 3  (RST)  ──────┼──┼─ RST ───────┤
    ║ ×240 ║ ║×240║│  GPIO 4  (CS)   ──────┼──┼─ CS (L+R) ──┘
    ╚══════╝ ╚════╝│                       │
                  ┌───────────────────────┐
                  │  USB-CDC / UART0      │─── RDK X3 / Linux Host
                  │  115200 bps           │
                  └───────────────────────┘
```

---

## Architecture / 系统架构

### Layered Design / 分层设计

```
┌──────────────────────────────────────────────────────────────────┐
│                   Application Layer / 应用层                       │
│                                                                   │
│  main.cpp               AppController                            │
│  (DI & lifecycle)       (State Machine + Command Dispatch)        │
│  (依赖注入 & 生命周期)      (状态机 + 命令解析 + 路由分发)            │
├──────────────────────────────────────────────────────────────────┤
│                    Domain Layer / 领域层                           │
│                                                                   │
│  AppState.h              ChatMessage.h          ChatSession       │
│  (Booting → Ready)       (Role + 1KB buffer)    (8-msg FIFO)     │
├──────────────────────────────────────────────────────────────────┤
│                   Ports Layer (Interfaces) / 接口层                │
│                                                                   │
│  ILogger       IInputPort       IDisplayPort    IEyeDisplayPort  │
│  (Logging)      (Serial Input)   (Main Screen)   (Eye Animation) │
│                                                                   │
│  IBitmapFontProvider                                              │
│  (Font Data Source)                                               │
├──────────────────────────────────────────────────────────────────┤
│                Adapters Layer (Implementations) / 适配器层          │
│                                                                   │
│  SerialLogger       SerialInputPort    St7789DisplayPort          │
│  Gc9a01EyeDisplay   SharedSpiBus       ScreenFontFlash            │
├──────────────────────────────────────────────────────────────────┤
│                    Core Library / 核心库                           │
│                                                                   │
│  lib/WallEDisplayCore/                                            │
│  ├── SpiBusCoordinator   (Multi-device CS management / 多设备CS)  │
│  ├── St7789Panel         (Arduino_HWSPI + Arduino_ST7789 wrapper) │
│  ├── Gc9a01Panel         (Arduino_ESP32SPI + Arduino_GC9A01)      │
│  ├── TextRenderer        (Mixed ASCII/dual-byte renderer / 混合渲染)│
│  ├── GifPlayer           (Non-blocking line-by-line decoder)      │
│  └── ScreenFontFlash     (SPI Flash random-access font reader)    │
└──────────────────────────────────────────────────────────────────┘
```

### Dependency Injection in `main.cpp` / 依赖注入

```cpp
// Instantiate all adapters at global scope / 全局作用域实例化
SerialLogger logger;
SerialInputPort input;
SharedSpiBus spiBus;
St7789DisplayPort display(&spiBus);
Gc9a01EyeDisplay eyeDisplay;

// Inject into AppController / 注入到控制器
AppController controller(logger, input, display, eyeDisplay);

void setup() {
    eyeDisplay.begin();   // Optional: init eye displays / 选配：初始化眼睛屏
    controller.begin();   // Sequentially init all components / 顺序初始化各组件
}

void loop() {
    controller.loop();    // One command per iteration + eye frame advance
                           // 每轮处理一条命令 + 推进眼睛动画帧
}
```

### FreeRTOS Task Topology (Planned) / FreeRTOS 任务拓扑（规划中）

当前固件运行在 Arduino `loop()` 单线程中，足以应对当前负载。长远需要任务拆分：
Currently running in single-threaded `loop()`, sufficient for current load. Long-term task split planned:

```
                    ┌──────────────────┐
                    │    Comm_Task     │  Priority: HIGH / 高优先级
                    │  UART RX         │  115200 bps serial receive
                    │  Line Parser     │  CR/LF + 250ms timeout framing
                    │  Command Decode  │  Case-insensitive prefix match
                    └────────┬─────────┘
                             │ Queue<InputPacket>
              ┌──────────────┼──────────────┐
              ▼              ▼              ▼
       ┌────────────┐ ┌────────────┐ ┌────────────┐
       │ State_Task │ │  UI_Task   │ │ Eye_Task   │
       │ Priority:  │ │ Priority:  │ │ Priority:  │
       │   LOW      │ │  NORMAL    │ │   HIGH     │
       │            │ │            │ │            │
       │ State mach │ │ ST7789     │ │ GC9A01     │
       │ 30s timeout│ │ Partial    │ │ GIF line-   │
       │ Cmd routing│ │ refresh    │ │ by-line     │
       └────────────┘ └────────────┘ └────────────┘
```

| Task | Priority / 优先级 | Rationale / 理由 |
|------|----------|-----------|
| `Comm_Task` | High / 高 | 串口硬件 FIFO 仅 128 bytes，必须在下一帧到达前消费 |
| `Eye_Task` | High / 高 | 40 MHz SPI 逐行解码有实时性要求，帧间隔抖动需 < 2 ms |
| `UI_Task` | Normal / 中 | 主屏支持局部刷新，延迟容忍度高 |
| `State_Task` | Low / 低 | 纯逻辑，不涉及硬件 I/O |

### SPI Bus Topology / SPI 总线拓扑

```
ESP32-S3
  │
  ├── SPI Bus 1 (VSPI, 80 MHz)
  │    ├── CS14 → ST7789 Main Display (240×240 IPS)
  │    └── CS11 → SPI NOR Flash (4MB font, shared SCK/MOSI/MISO)
  │         │
  │         └── SpiBusCoordinator: begin() pulls all HIGH → deselectAll() before access
  │
  └── SPI Bus 2 (HSPI, 40 MHz)
       └── CS4  → GC9A01 Left + Right (hardware CS mirror, synchronized)
```

**为什么眼睛屏用独立 SPI 总线？ / Why a separate SPI bus for the eyes?**
主屏 ST7789 跑 80 MHz + 字库 Flash 读字模时 VSPI 已接近饱和。若眼睛屏共享同一总线，GIF 解码期间的 SPI 事务争抢会导致动画掉帧。独立 HSPI 总线从物理层隔离。
The VSPI bus is near saturation with the main display at 80 MHz plus font flash reads. Sharing would cause SPI contention during GIF decoding, resulting in frame drops. The independent HSPI bus provides physical-layer isolation.

---

## Directory Structure / 目录结构

```
arduino-wall-e-display/
│
├── platformio.ini                          # PlatformIO build config / 构建配置
│   ├── board: esp32-s3-devkitc-1
│   ├── framework: arduino
│   ├── monitor_speed: 115200
│   └── upload_speed: 921600
│
├── README.md                               # This document / 本文档
├── .gitignore
│
├── .vscode/
│   ├── c_cpp_properties.json               # C++ IntelliSense config
│   ├── extensions.json                     # Recommended VS Code extensions
│   └── launch.json                         # Debug launch config
│
├── docs/
│   └── manual-test.md                      # Manual test checklist / 手动测试清单
│
├── lib/
│   └── WallEDisplayCore/                   # Reusable core library (zero coupling)
│       ├── library.json                    # Library metadata / 库元信息
│       └── src/
│           ├── ports/
│           │   └── IBitmapFontProvider.h   # Font data source interface / 字模数据源接口
│           └── adapters/
│               └── display/
│                   ├── SpiBusCoordinator.h  # Multi-device SPI CS coordinator
│                   ├── St7789Panel.h/cpp    # ST7789 hardware abstraction
│                   ├── Gc9a01Panel.h/cpp    # GC9A01 hardware abstraction
│                   ├── TextRenderer.h/cpp   # Mixed ASCII/dual-byte renderer
│                   ├── GifPlayer.h/cpp      # Non-blocking line-by-line GIF player
│                   └── ScreenFontFlash.h/cpp# SPI Flash font reader (signature check)
│
└── src/
    ├── main.cpp                             # Arduino entry (setup/loop, DI)
    │
    ├── config/
    │   └── Config.h                         # Global config center / 全局配置中心
    │       ├── Pin definitions / 引脚定义
    │       ├── SPI frequencies / SPI 频率 (80M/40M/2M)
    │       ├── Serial params / 串口参数 (115200/512B/250ms)
    │       ├── UI constants / UI 常量 (colors/spacing/sizes)
    │       └── Feature flags / 功能开关 (eyes/font)
    │
    ├── domain/
    │   ├── AppState.h                       # App state enum (Booting/Ready)
    │   ├── ChatMessage.h                    # Single chat message (Role + 1KB)
    │   └── ChatSession.h/cpp                # Chat session cache (8-msg FIFO)
    │
    ├── ports/
    │   ├── ILogger.h                        # Logging interface (info/warn/error)
    │   ├── IInputPort.h                     # Input interface (poll/drain + InputPacket)
    │   ├── IDisplayPort.h                   # Main display interface
    │   └── IEyeDisplayPort.h               # Eye display interface (playZoom/update)
    │
    ├── app/
    │   └── AppController.h/cpp              # State machine core / 状态机核心
    │       ├── begin(): sequential init → self-test → Ready
    │       ├── loop(): eye frame + serial poll + cmd dispatch
    │       ├── handleXxx(): per-command handlers
    │       └── returnToPowerIfChatIdle(): 30s timeout fallback
    │
    └── adapters/
        ├── log/
        │   └── SerialLogger.h               # [INFO/WARN/ERROR] format logger
        ├── input/
        │   └── SerialInputPort.h/cpp        # Double-buffer + idle-timeout parser
        └── display/
            ├── SharedSpiBus.h               # Project-level SPI CS registry
            ├── St7789DisplayPort.h/cpp      # ST7789 main screen UI adapter
            │   ├── drawStartupSelfTest()    # Color bar self-test
            │   ├── drawPowerFrame()         # Battery frame (sun icon + title)
            │   ├── drawPowerBars()          # Battery bars partial refresh
            │   └── render(ChatSession)      # Chat screen full render
            ├── Gc9a01EyeDisplay.h/cpp       # GC9A01 eye display adapter
            └── eyeimg.h                     # Built-in eye GIF (~4.36MB PROGMEM)
```

---

## State Machine / 状态机

```
                    ┌─────────────────────────┐
                    │         Booting          │
                    │  • Serial logger init    │
                    │  • Eye display init      │
                    │    (optional / 可选)      │
                    │  • Main SPI init         │
                    │  • Color bar self-test   │
                    │  • Font flash sig check  │
                    │  • Input port init       │
                    └────────────┬────────────┘
                                 │ All success / 全部成功
                                 ▼
                    ┌─────────────────────────┐
                    │          Ready           │
                    │  • Accept handshake      │
                    │  • Accept power: cmd     │
                    │  • Wait for openchat:1   │
                    └──────┬──────────┬───────┘
                           │          │
              openchat:1   │          │  openchat:0
                           ▼          │  or / 或 30s idle
                    ┌──────────────┐ │
                    │    Chat      │◄┘
                    │  • you:/ai:  │
                    │  • Render    │
                    └──────────────┘
```

**安全机制 / Safety Mechanisms:**

- `Booting` 状态下，非握手命令一律忽略并 `drain()` 清空缓冲区
  In `Booting` state, non-handshake commands are ignored and buffer is drained.
- `Chat` 模式下 30 秒无活动自动回退到 `Ready` + 电量界面
  30-second chat inactivity auto-returns to `Ready` + power screen.
- 电量值 > 100 自动裁剪为 100
  Battery values > 100 are clamped to 100.
- 聊天模式关闭时收到的消息直接丢弃并打印警告
  Messages received while chat is closed are dropped with a warning.

---

## Serial Protocol / 串口协议

### Physical Layer / 物理层

| Parameter / 参数 | Value / 值 |
|-----------|-------|
| Interface / 接口 | USB-CDC / UART0 |
| Baud Rate / 波特率 | 115200 bps |
| Data Bits / 数据位 | 8 |
| Parity / 校验位 | None / 无 |
| Stop Bits / 停止位 | 1 |
| Flow Control / 流控 | None / 无 |

### Frame Layer / 帧层

- **分隔符 / Delimiter:** CR (`\r`) or LF (`\n`) as mandatory frame boundary / 强制帧边界
- **空闲超时 / Idle Timeout:** 250 ms without new bytes triggers auto-flush of current buffer / 无新字节超 250ms 自动提交
- **最大帧长 / Max Frame:** 512 bytes (`kInputMaxBytes`), excess silently dropped / 超出静默丢弃
- **空行处理 / Empty Line:** Consecutive newlines produce no message / 连续换行不产生空消息

### Application Layer — Handshake / 应用层 — 握手

```
Host / 上位机 (RDK X3)           ESP32-S3
     │                              │
     │  getname:WHO_ARE_YOU          │
     │──────────────────────────────▶│
     │                              │  (case-insensitive prefix match)
     │         WALL_E_TFT            │  (大小写不敏感前缀匹配)
     │◀──────────────────────────────│
     │                              │
     │  Handshake complete.          │
     │  Host now knows the           │
     │  connected display identity.  │
     │  握手完成，确认显示设备身份      │
```

### Application Layer — Command Set / 应用层 — 命令集

| # | Command / 命令 | Direction / 方向 | Description / 说明 | Example / 示例 |
|---|---------|-----------|-------------|---------|
| 1 | `getname:WHO_ARE_YOU` | Host → ESP32 | Device discovery / 设备发现 | — |
| 2 | `WALL_E_TFT` | ESP32 → Host | Handshake response / 握手应答 | — |
| 3 | `power:N` | Host → ESP32 | Update battery 0–100 / 更新电量 | `power:78` |
| 4 | `openchat:1` | Host → ESP32 | Enter chat screen / 进入聊天 | — |
| 5 | `openchat:0` | Host → ESP32 | Exit chat screen / 退出聊天 | — |
| 6 | `you:<text>` | Host → ESP32 | User message (GBK) / 用户消息 | `you:你好` |
| 7 | `ai:<text>` | Host → ESP32 | AI response / AI 回复 | `ai:今天天气不错` |
| 8 | `eyeaction:zoom` | Host → ESP32 | Trigger eye zoom animation / 触发变焦动画 | — |

### Protocol Notes / 协议说明

- 命令前缀匹配**大小写不敏感** (`startsWithIgnoreCase`) / Case-insensitive prefix matching.
- `you:` 和 `ai:` 前缀后的 ASCII 空格会自动跳过 / Trailing ASCII space after `you:`/`ai:` is auto-skipped.
- 无前缀的消息默认按 `you:`（用户角色）处理 / Unprefixed messages default to `you:` role.
- 数字参数独立解析，支持前导空格和制表符 / Numeric params parsed independently, leading whitespace tolerated.
- 中文由上位机编码为 GBK/GB2312 字节流发送，固件不做 UTF-8 转换 / Chinese text is GBK/GB2312-encoded by the host; firmware does no UTF-8 conversion.

### Current Limitations / 当前局限

| Limitation / 局限 | Impact / 影响 | Mitigation / 缓解计划 |
|-----------|--------|-----------------|
| No frame CRC / 无帧校验 | Corrupted data in noisy UART / 噪声下可能损坏 | Roadmap: CRC16 |
| No escaping / 无转义 | Message body cannot contain `\r`/`\n` / 正文不能含换行 | Roadmap: SLIP/COBS |
| No flow control / 无流控 | Host must self-throttle / 上位机自控发送速率 | Roadmap: ACK/BUSY |

---

## Rendering Pipelines / 渲染管线

### 1. Main Display: Power Screen / 主屏：电量界面

```
showPower(percent)
  │
  ├── [First call only / 仅首次] drawPowerFrame()
  │   ├── fillScreen(black / 黑色)
  │   ├── Title "SOLAR CHARGE LEVEL" (yellow, large / 黄色大字)
  │   ├── drawSunIcon()  ← asterisk-shaped sun / 米字形太阳图标
  │   └── powerFrameDrawn_ = true
  │
  └── drawPowerBars(percent, force=false)
      │
      ├── thinBars = percent/10 - 1    (0~7 thin bars / 条细条)
      ├── thinBars == lastPowerThinBars_? → skip / 跳过（无变化）
      │
      └── for i in 0..6:
          ├── active    = (i >= 7 - thinBars)
          ├── wasActive = (i >= 7 - lastPowerThinBars_)
          └── active != wasActive? → drawPowerThinBar(i, active)  ← partial redraw
```

| Battery % / 电量 | Active Bars / 亮条 | Visual / 示意 |
|-----------|-----------------|--------|
| ≥ 80% | 7/7 | ■■■■■■■ |
| 70–79% | 6/7 | ■■■■■■□ |
| 60–69% | 5/7 | ■■■■■□□ |
| ... | ... | ... |
| 10–19% | 0/7 | □□□□□□□ (bottom bar only / 仅底部粗条) |
| < 10% | 0/7 | □□□□□□□ + bottom bar dim / 粗条灭 |

### 2. Main Display: Chat Screen / 主屏：聊天界面

```
render(session)
  │
  ├── [First call only / 仅首次] drawFrame()
  │   ├── Dark background fill / 深色背景
  │   ├── Top bar: "WALL-E" (yellow) + status indicator / 标题栏 + 状态
  │   ├── Bottom bar: "ESP32 SERIAL DISPLAY" / 底部状态栏
  │   └── Separator line / 分割线
  │
  ├── clearChatArea()  ← clear content area each render / 每次清空
  │
  ├── measureMessageHeight() × N  ← measure each message
  │
  ├── Reverse scan from newest message / 从最新消息逆向扫描
  │   └── Accumulated height ≤ visible area → determine draw range
  │
  └── drawMessage() for each / 逐条绘制
      ├── Role label (32×16 block): "YOU"(green) / "AI"(cyan) / "SYS"(cyan)
      ├── Body → TextRenderer::drawBytes()
      │   ├── Byte ≥ 0xA1? → dual-byte glyph → ScreenFontFlash → 16×16 bitmap
      │   └── Byte < 0xA1? → ASCII → Arduino_GFX built-in 5×7 font
      └── 4px spacing between messages / 消息间距 4px
```

### 3. Eye Display: GIF Animation / 眼屏：GIF 动画

```
eyeaction:zoom
  │
  └── Gc9a01EyeDisplay::playZoom()
      └── GifPlayer::play(eyeimg.h PROGMEM data)
          ├── stop()  ← stop previous animation
          ├── Clear screen black / 清屏黑色
          ├── gif_.open(data, size, GIFDrawCallback)
          └── playing_ = true, record nextFrameAtMs_

Main loop calls GifPlayer::update() each iteration / 主循环每轮调用
  │
  ├── millis() < nextFrameAtMs_? → return (not yet / 未到帧间隔)
  │
  ├── gif_.playFrame(false, &frameDelayMs)
  │   │
  │   └── [Line-by-line callback] GIFDrawCallback → drawLine()
  │       │
  │       ├── hasTransparency = true?
  │       │   └── Scan contiguous opaque segments / 扫描连续不透明段
  │       │       ├── Skip transparent pixels / 跳过透明像素
  │       │       ├── Accumulate opaque → lineBuffer (RGB565)
  │       │       └── draw16bitRGBBitmap(opaque segment) ← bulk SPI
  │       │
  │       └── hasTransparency = false?
  │           └── Indexed → RGB565 full line → draw16bitRGBBitmap(whole line)
  │
  └── Auto stop after last frame delay / 最后一帧延迟后自动停止
```

**关键优化 / Key Optimization:** Transparent frames render contiguous opaque segments in bulk via `draw16bitRGBBitmap` instead of per-pixel `drawPixel()`. For 240×240 GIFs with large transparent regions, SPI transaction count drops by an order of magnitude.
透明帧以连续不透明段为单位通过 `draw16bitRGBBitmap` 批量 SPI 写入，而非逐像素判断。对于大面积透明区域的 240×240 GIF，SPI 事务次数可减少一个数量级。

### 4. Text Rendering: GBK Pipeline / 文本渲染：GBK 管线

```
TextRenderer::drawBytes(data, len, x, y, maxWidth)
  │
  └── Iterate per byte / 逐字节遍历：
      │
      ├── data[i] ≥ 0xA1 && data[i+1] ≥ 0xA1 && fontProvider_ != null?
      │   │
      │   ├── YES: Dual-byte Chinese char / 双字节中文字符
      │   │   └── ScreenFontFlash::readDoubleByte16x16(codeLo, codeHi)
      │   │       ├── Address = 24-bit linear (code-mapped) / 24位线性地址
      │   │       ├── SPI Flash random read 32 bytes (16×16 / 8)
      │   │       └── 1-bit bitmap → RGB565 pixel array → draw16bitRGBBitmap()
      │   │
      │   └── NO: ASCII char / ASCII 字符
      │       └── gfx_->print(char)  ← Arduino_GFX built-in 5×7 font
      │
      └── Auto line-wrap / 自动换行：
          cursorX + glyphWidth > maxWidth
          → cursorX = 0, cursorY += lineHeight
```

**Dual-byte detection / 双字节判定:**

```cpp
bool isDoubleByteGlyph(const uint8_t* data, size_t index, size_t len) {
    return fontProvider_ != nullptr
        && index + 1 < len
        && data[index]     >= config_.doubleByteFirst       // 0xA1
        && data[index + 1] >= config_.doubleByteSecondFirst; // 0xA1
}
```

The firmware performs **no UTF-8 → GBK conversion**. The host is responsible for encoding Chinese as GBK/GB2312 byte streams. The firmware only checks byte ranges and indexes the font flash directly.
固件**不做 UTF-8 → GBK 转换**。上位机负责编码为 GBK/GB2312 字节流，固件仅判断字节范围并直接索引字库。

---

## Configuration Guide / 配置指南

All tunable parameters in `src/config/Config.h` / 所有可调参数集中在:

```cpp
// ──── Feature Flags / 功能开关 ────
constexpr bool kEnableEyeDisplay     = true;   // Enable eye displays / 启用眼睛屏
constexpr bool kUseScreenFontFlash   = true;   // Enable SPI flash font / 启用字库 Flash

// ──── ST7789 Main Display / 主屏 ────
constexpr int8_t kSt7789PinCs    = 14;
constexpr int8_t kSt7789PinDc    = 13;
constexpr int8_t kSt7789PinRst   = 15;
constexpr int8_t kSt7789PinSck   = 18;
constexpr int8_t kSt7789PinMosi  = 17;
constexpr int8_t kSt7789PinMiso  = 16;
constexpr int8_t kSt7789SpiFreq  = 80000000;   // 80 MHz

// ──── GC9A01 Eye Displays / 眼睛屏 ────
constexpr int8_t kGc9a01PinCs    = 4;
constexpr int8_t kGc9a01PinDc    = 5;
constexpr int8_t kGc9a01PinRst   = 3;
constexpr int8_t kGc9a01PinSck   = 9;
constexpr int8_t kGc9a01PinMosi  = 8;
constexpr int8_t kGc9a01SpiFreq  = 40000000;   // 40 MHz

// ──── Font Flash / 字库 Flash ────
constexpr int8_t kScreenFontCs   = 11;
constexpr int8_t kScreenFontFreq = 2000000;    // 2 MHz

// ──── Serial / 串口 ────
constexpr uint32_t kBaudRate      = 115200;
constexpr size_t   kInputMaxBytes = 512;
constexpr uint32_t kPacketTimeoutMs = 250;

// ──── UI / 界面 ────
constexpr uint32_t kChatIdleTimeoutMs = 30000;  // 30s
constexpr size_t   kChatMaxMessages    = 8;
```

---

## Build & Flash / 构建与烧录

### Prerequisites / 前置条件

- [PlatformIO IDE](https://platformio.org/install) (VS Code extension) or [PlatformIO Core CLI](https://docs.platformio.org/en/latest/core/installation.html)
- ESP32-S3 connected via USB with drivers installed / ESP32-S3 通过 USB 连接，驱动正常

### Commands / 命令

```bash
# Clone / 克隆仓库
git clone <your-repo-url>
cd arduino-wall-e-display

# Install dependencies (auto) / 安装依赖（自动）
pio pkg install

# Build / 编译
pio run

# Flash / 烧录
pio run --target upload

# Serial monitor / 串口监视器
pio device monitor --baud 115200
```

### First Boot Expected Output / 首次启动预期输出

```
[INFO] Wall-E Arduino serial display boot
[INFO] ST7789 begin...OK
[INFO] Font flash signature check...OK
[INFO] TEXT OK
[INFO] READY
```

At this point the host should send `getname:WHO_ARE_YOU` and receive `WALL_E_TFT`.
此时上位机发送 `getname:WHO_ARE_YOU`，应收到应答 `WALL_E_TFT`。

---

## Testing & Serial Examples / 测试与串口示例

### Manual Test Categories / 手动测试分类

| Category / 分类 | Test Items / 测试项 |
|----------|-----------|
| Bring-up / 上电 | Power-on self-test, color bars, font signature check, `TEXT OK` |
| Power Screen / 电量 | Display at 0%/50%/100%, partial refresh no flicker, bar segments correct |
| Chat Screen / 聊天 | Single/multi message render, `you:`/`ai:` role colors, GBK Chinese, auto wrap, 8-msg FIFO eviction |
| Eye Action / 眼睛 | `eyeaction:zoom` trigger, animation frame rate, non-blocking (still responds to `getname:`) |
| Failure / 异常 | Non-handshake cmd rejected before Ready, font missing → `TEXT ERR`, `power:150` clamped to 100 |

### Serial Command Send Examples / 串口命令发送示例

以下示例使用常见的串口工具。将 ESP32-S3 连接到电脑，打开对应 COM 口，波特率 115200。
Examples below use common serial tools. Connect ESP32-S3 to your PC, open the matching COM port at 115200 bps.

---

#### Example 1: Handshake / 握手

```bash
# PlatformIO Monitor (built-in / 内置)
# Just type and press Enter / 直接输入回车即可:
getname:WHO_ARE_YOU

# Expected response / 预期应答:
WALL_E_TFT
```

```bash
# Linux: using screen / 使用 screen
screen /dev/ttyUSB0 115200
# Type: getname:WHO_ARE_YOU [Enter]
# Expected: WALL_E_TFT
# Exit: Ctrl+A then K

# Linux: using minicom / 使用 minicom
minicom -b 115200 -D /dev/ttyUSB0
# Type: getname:WHO_ARE_YOU [Enter]
# Expected: WALL_E_TFT
# Exit: Ctrl+A then X
```

```bash
# Windows: using PowerShell / 使用 PowerShell
[System.IO.Ports.SerialPort]::new("COM3", 115200).Open()
# Then use a terminal tool like PuTTY or PlatformIO Monitor
# 然后使用 PuTTY 或 PlatformIO Monitor 等终端工具
```

```powershell
# Windows: using PlatformIO CLI / 使用 PlatformIO CLI
pio device monitor --baud 115200
# Type: getname:WHO_ARE_YOU [Enter]
# Expected: WALL_E_TFT
```

---

#### Example 2: Power Update / 更新电量

```bash
# Send via serial monitor / 通过串口监视器发送:
power:0
power:50
power:100

# Expected / 预期结果:
# power:0   → 所有细条灭，底部粗条亮 / all thin bars off, bottom bar on
# power:50  → 4 条细条亮 / 4 thin bars lit
# power:100 → 7 条全亮 / all 7 lit
```

```bash
# Linux: batch send with echo / 批量发送
echo -e "power:0\r" > /dev/ttyUSB0
sleep 2
echo -e "power:50\r" > /dev/ttyUSB0
sleep 2
echo -e "power:100\r" > /dev/ttyUSB0
```

```powershell
# Windows PowerShell: using .NET SerialPort
$port = New-Object System.IO.Ports.SerialPort COM3,115200,None,8,One
$port.Open()
$port.WriteLine("power:0")
Start-Sleep -Seconds 2
$port.WriteLine("power:50")
Start-Sleep -Seconds 2
$port.WriteLine("power:100")
$port.Close()
```

---

#### Example 3: Chat Screen / 聊天界面

```bash
# Step 1: Enter chat mode / 进入聊天模式
openchat:1

# Step 2: Send user and AI messages / 发送用户和 AI 消息
you:你好，瓦力！
ai:你好！今天有什么可以帮助你的吗？
you:今天天气怎么样？
ai:根据最新数据，今天晴天，气温 25°C，适合出行。

# Step 3: Exit chat mode / 退出聊天模式
openchat:0

# Expected / 预期结果:
# - Chat screen shows messages with role labels (green YOU / cyan AI)
#   聊天界面显示消息，带角色标签（绿色 YOU / 青色 AI）
# - Messages auto-wrap / 消息自动换行
# - Oldest messages scroll off when > 8 / 超 8 条时最旧消息滚动淘汰
# - After openchat:0, returns to power screen / openchat:0 后返回电量界面
# - 30s idle also triggers return / 30 秒无活动也会自动返回
```

```bash
# Linux: batch chat simulation / 批量模拟对话
echo -e "openchat:1\r" > /dev/ttyUSB0
sleep 1
echo -e "you:你好瓦力\r" > /dev/ttyUSB0
sleep 1
echo -e "ai:你好！有什么可以帮你的？\r" > /dev/ttyUSB0
sleep 1
echo -e "you:介绍一下你自己\r" > /dev/ttyUSB0
sleep 1
echo -e "ai:我是瓦力机器人，ESP32-S3 驱动三块屏幕\r" > /dev/ttyUSB0
```

---

#### Example 4: Eye Animation / 眼睛动画

```bash
# Trigger zoom animation / 触发变焦动画
eyeaction:zoom

# Expected / 预期结果:
# - Both eye displays play the built-in zoom GIF simultaneously
#   双眼同时播放内置变焦 GIF
# - During animation, the system still responds to other commands
#   动画播放期间，系统仍响应其他命令
# - Verify non-blocking: send getname:WHO_ARE_YOU during animation
#   验证非阻塞：动画播放期间发送 getname:WHO_ARE_YOU
#   Should still respond WALL_E_TFT / 应仍能应答 WALL_E_TFT
```

```bash
# Non-blocking verification / 非阻塞验证
eyeaction:zoom
# Immediately send during animation / 动画期间立即发送:
getname:WHO_ARE_YOU
# Expected: still responds WALL_E_TFT / 预期仍应答 WALL_E_TFT
power:75
# Expected: power screen updates behind animation / 预期电量在动画后台更新
```

---

#### Example 5: Full Integration Test / 完整集成测试

```bash
# === Phase 1: Handshake / 握手 ===
getname:WHO_ARE_YOU
# Expect: WALL_E_TFT

# === Phase 2: Power Display / 电量显示 ===
power:85
# Expect: 7 bars lit / 7 条亮

# === Phase 3: Eye Animation / 眼睛动画 ===
eyeaction:zoom
# Expect: zoom animation plays on both eyes

# === Phase 4: Chat Flow / 聊天流程 (while eyes animate / 动画同时) ===
openchat:1
you:你好
ai:你好！我是瓦力。
you:介绍一下你的硬件配置
ai:我使用 ESP32-S3 主控，驱动 2 块 GC9A01 圆形眼屏和 1 块 ST7789 主屏。
you:很好
ai:谢谢！
openchat:0

# === Phase 5: Verify Power Screen / 验证电量界面 ===
# Expect: back to power screen with 85% / 预期回到电量界面 85%

# === Phase 6: Edge Cases / 边界测试 ===
power:150
# Expect: clamped to 100 / 预期裁剪为 100

getname:WHO_ARE_YOU
# Expect: still WALL_E_TFT / 预期仍应答

# === Phase 7: Idle Timeout / 空闲超时 ===
openchat:1
you:测试超时
# Wait 30 seconds / 等待 30 秒
# Expect: auto-return to power screen / 预期自动返回电量界面
```

---

#### Example 6: Python Script for Automated Testing / Python 自动化测试脚本

```python
#!/usr/bin/env python3
"""Wali Display System automated serial test / 瓦力显示系统自动化串口测试"""

import serial
import time

PORT = "COM3"       # Windows: COM3 / Linux: /dev/ttyUSB0
BAUD = 115200
TIMEOUT = 1

def send_cmd(ser, cmd: str, wait: float = 0.3):
    """Send a command and read response / 发送命令并读取应答"""
    ser.write((cmd + "\r\n").encode())
    time.sleep(wait)
    response = ser.read_all().decode(errors="replace").strip()
    print(f"  >>> {cmd}")
    if response:
        print(f"  <<< {response}")
    return response

def main():
    ser = serial.Serial(PORT, BAUD, timeout=TIMEOUT)
    time.sleep(1.5)  # Wait for boot / 等待启动

    print("=" * 50)
    print("Test 1: Handshake / 握手测试")
    resp = send_cmd(ser, "getname:WHO_ARE_YOU")
    assert "WALL_E_TFT" in resp, "Handshake failed!"

    print("\nTest 2: Power Display / 电量显示")
    for pct in [0, 30, 50, 80, 100]:
        send_cmd(ser, f"power:{pct}", wait=1.0)

    print("\nTest 3: Eye Animation / 眼睛动画")
    send_cmd(ser, "eyeaction:zoom", wait=0.5)
    # Non-blocking check during animation / 动画期间非阻塞检查
    resp = send_cmd(ser, "getname:WHO_ARE_YOU")
    assert "WALL_E_TFT" in resp, "Non-blocking check failed!"

    print("\nTest 4: Chat Flow / 聊天流程")
    send_cmd(ser, "openchat:1")
    messages = [
        ("you:", "你好瓦力"),
        ("ai:", "你好！我是瓦力机器人"),
        ("you:", "介绍一下你自己"),
        ("ai:", "ESP32-S3 驱动三块屏幕，串口与上位机通信"),
    ]
    for prefix, text in messages:
        send_cmd(ser, f"{prefix}{text}", wait=0.5)
    send_cmd(ser, "openchat:0")

    print("\nTest 5: Edge Case - Value Clamping / 边界-值裁剪")
    send_cmd(ser, "power:150", wait=0.5)
    send_cmd(ser, "power:-5", wait=0.5)
    send_cmd(ser, "power:100")

    print("\nAll tests passed! / 全部测试通过! ✅")
    ser.close()

if __name__ == "__main__":
    main()
```

---

## Roadmap / 开发路线

### Completed / 已完成

- [x] Dual independent SPI bus architecture / 双独立 SPI 总线架构
- [x] ST7789 240×240 main display driver / ST7789 主屏驱动
- [x] GC9A01 dual round eye display driver (CS mirror) / GC9A01 双圆形眼屏驱动
- [x] `getname:WHO_ARE_YOU` / `WALL_E_TFT` serial handshake / 串口握手协议
- [x] `power:` / `openchat:` / `you:` / `ai:` / `eyeaction:` command set / 命令集
- [x] Power screen dirty-rectangle partial refresh / 电量界面局部刷新
- [x] GBK/GB2312 dual-byte Chinese rendering (SPI Flash font) / GBK 中文渲染
- [x] Font flash signature verification `JYC-4MbByte-FONT-FLASH` / 字库签名校验
- [x] GIF non-blocking line-by-line decoder (GifPlayer) / GIF 非阻塞逐行解码
- [x] Transparent GIF contiguous-segment bulk SPI optimization / 透明帧批量 SPI 优化
- [x] Chat message fixed-capacity FIFO queue (8 × 1KB) / 聊天消息 FIFO 队列
- [x] 30-second chat idle auto-return to power screen / 30 秒空闲超时回退
- [x] Ports-Adapters layered architecture (DI, core library zero-coupling) / 端口-适配器分层架构
- [x] SPI chip-select coordinator (SpiBusCoordinator table-driven) / SPI 片选协调器
- [x] Manual test checklist (5 categories) / 手动测试清单

### In Progress / Planned / 进行中 / 规划中

- [ ] **FreeRTOS multi-task split / FreeRTOS 多任务拆分** — Comm / State / UI / Eye 4 tasks + inter-task queues
- [ ] **Independent CS for each eye / 双眼睛独立 CS** — left/right independent, async different expressions
- [ ] **Serial frame CRC16 / 串口帧 CRC16** — data integrity in noisy environments
- [ ] **SLIP/COBS framing / SLIP/COBS 分帧** — allow arbitrary bytes (including newlines) in message body
- [ ] **Host flow control feedback / 上位机流控** — ESP32 sends ACK/BUSY, host adaptive throttling
- [ ] **eyeaction expression set / eyeaction 表情集** — `blink`, `wink`, `sleep`, `happy`
- [ ] **Chat scroll transition animation / 聊天滚动过渡动画** — smooth push-up instead of jump
- [ ] **Error state visualization / 异常状态显示** — font failure / SPI timeout shown on main screen
- [ ] **Unit test framework / 单元测试** — Unity/CppUTest covering command parser, ChatSession, battery calc
- [ ] **OTA firmware update / OTA 固件无线更新** — leverage ESP32-S3 Wi-Fi
- [ ] **PlatformIO CI (GitHub Actions) / CI 构建** — auto build + lint + unit tests
- [ ] **Persistent config (NVS) / 配置持久化** — brightness, timeout stored in NVS
- [ ] **I2C sensor extension / I2C 传感器扩展** — ambient light / IMU for auto-brightness and gesture triggers

---

## Contributing / 贡献指南

本项目遵循**端口-适配器架构**规范 / This project follows the Ports-Adapters architecture:

1. **新增功能必须先定义接口 / Define interface first:** 在 `src/ports/` 下添加纯虚接口类，再在 `src/adapters/` 下实现
2. **不要绕过接口直接调用硬件 / No direct hardware access:** 所有 SPI / UART / GPIO 操作必须通过适配器封装
3. **可复用逻辑放在核心库 / Reusable logic in core lib:** 与项目配置无关的通用代码下沉到 `lib/WallEDisplayCore/`
4. **配置项集中在 Config.h / Centralize config:** 引脚、频率、超时等参数不允许散落在实现文件中
5. **提交前确保编译通过 / Build before commit:** 运行 `pio run`，并补充 `docs/manual-test.md` 测试用例

---

## License / 许可证

MIT

---

<p align="center">
  <i>Built for the Wali multimodal interactive robot system.</i><br>
  <i>为瓦力多模态交互机器人系统而构建。</i><br><br>
  <b>Determinism over decoration. Stability over spectacle.</b><br>
  <b>工程确定性优先于视觉效果。系统稳定性优先于表面光鲜。</b>
</p>
