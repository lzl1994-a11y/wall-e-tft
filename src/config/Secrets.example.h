#pragma once

// Optional bootstrap defaults only. USB serial NETCFG is the sole runtime
// configuration/recovery channel and can replace these values without reboot.
// Secrets.h is ignored by Git.
// Up to three networks are tried in this order. Leave an SSID empty to skip it.
#define WALLE_WIFI_1_SSID "RDK_X3_HOTSPOT"
#define WALLE_WIFI_1_PASSWORD "change-me"
#define WALLE_WIFI_2_SSID ""
#define WALLE_WIFI_2_PASSWORD ""
#define WALLE_WIFI_3_SSID ""
#define WALLE_WIFI_3_PASSWORD ""
#define WALLE_IMAGE_SERVER_HOST "192.168.4.1"
#define WALLE_IMAGE_SERVER_PORT 9000
