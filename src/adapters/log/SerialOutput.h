#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
#include <hal/usb_serial_jtag_ll.h>
#endif

namespace WallE {

// One mutex is shared by logs and protocol replies, so no two tasks can leave
// interleaved fragments on USB serial. A mutex is deliberately used instead
// of a portMUX: Serial writes can block and must never run in a spinlock.
inline SemaphoreHandle_t serialOutputMutex() {
  static SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
  return mutex;
}

inline void serialWriteLine(const char* text, size_t length) {
  constexpr size_t kMaxLineBytes = 512;
  if (text == nullptr) {
    text = "";
    length = 0;
  }
  length = min(length, kMaxLineBytes);
  char line[kMaxLineBytes + 2] = {};
  if (length > 0) memcpy(line, text, length);
  line[length] = '\r';
  line[length + 1] = '\n';
  SemaphoreHandle_t mutex = serialOutputMutex();
  if (mutex != nullptr) xSemaphoreTake(mutex, portMAX_DELAY);
#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
  const int txCapacity = Serial.availableForWrite();
#endif
  Serial.write(reinterpret_cast<const uint8_t*>(line), length + 2);
  // HWCDC::write only enqueues bytes. Drain/kick TX before another task can
  // write: otherwise a queued reply can remain pending until the next output.
  Serial.flush();
#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
  // HWCDC drains its software ring, but the final USB packet can be full-sized
  // after a ring wrap. Terminate that transfer with a zero-length packet so
  // the host does not retain it until the next line's short packet arrives.
  const uint32_t drainAt = millis();
  constexpr uint32_t kUsbDrainTimeoutMs = 100;  // HWCDC's default TX timeout.
  while ((Serial.availableForWrite() < txCapacity ||
          !usb_serial_jtag_ll_txfifo_writable()) && Serial.isPlugged() &&
         millis() - drainAt < kUsbDrainTimeoutMs) {
    delay(1);
  }
  if (Serial.isPlugged() && Serial.availableForWrite() >= txCapacity &&
      usb_serial_jtag_ll_txfifo_writable()) {
    usb_serial_jtag_ll_txfifo_flush();
  }
#endif
  if (mutex != nullptr) xSemaphoreGive(mutex);
}

inline void serialPrintln(const char* text) {
  serialWriteLine(text, strnlen(text == nullptr ? "" : text, 512));
}

inline void serialLogLine(const char* level, const char* message) {
  char line[512] = {};
  const int written = snprintf(line, sizeof(line), "[%s] %s",
                               level == nullptr ? "" : level,
                               message == nullptr ? "" : message);
  const size_t length = written <= 0 ? 0 : min(
      static_cast<size_t>(written), sizeof(line) - 1);
  serialWriteLine(line, length);
}

}  // namespace WallE
