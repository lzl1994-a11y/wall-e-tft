# 上位机 Wi-Fi 配网屏幕

通过原 USB Serial/JTAG 串口接收新命令：

```text
hostnet:<uint32_seq>|1|<state>|<IPv4>|<web_port>|<AP_SSID>|<AP_password>
```

`state`：`setup` 显示热点和网页地址；`connecting` 保持配网屏；
`connected` 显示新地址；`syncing` 明确显示 ESP32 同步中；
`partial` 显示主机在线但同步未完成及重试地址；`done` 显示 `WIFI READY` 三秒后返回电量页。
SSID/密码允许空字段，长度分别不超过 32/63 个 ASCII 字节。
禁止字段分隔符、控制字符、超范围 IP、端口与序列号。

主循环完成屏幕处理后返回：

```text
NETCFG:RESULT:<seq>|HOSTSCREEN|0|0
```

参数错误的 result 为 3。ACK 不回传热点密码。
配网期间不允许聊天开关、视频流或 ESP32 网络事件覆盖屏幕；
眼睛、串口 NETCFG 和 PCA9685 控制继续工作。
本命令仅展示上位机配网入口，ESP32 自身联网继续沿用 NETCFG v2。

主机协议测试：

```bash
g++ -std=c++11 -I src tools/test_host_network_screen.cpp -o test_host_network_screen
./test_host_network_screen
python tools/test_camera_stream_protocol.py
pio run -e esp32
```
