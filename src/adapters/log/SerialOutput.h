#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace WallE {

// One mutex is shared by logs and protocol replies, so no two tasks can leave
// interleaved fragments on USB serial. A mutex is deliberately used instead
// of a portMUX: Serial writes can block and must never run in a spinlock.
inline SemaphoreHandle_t serialOutputMutex() {
  static SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
  return mutex;
}

inline void serialPrintln(const char* text) {
  SemaphoreHandle_t mutex = serialOutputMutex();
  if (mutex != nullptr) xSemaphoreTake(mutex, portMAX_DELAY);
  Serial.println(text == nullptr ? "" : text);
  if (mutex != nullptr) xSemaphoreGive(mutex);
}

inline void serialLogLine(const char* level, const char* message) {
  SemaphoreHandle_t mutex = serialOutputMutex();
  if (mutex != nullptr) xSemaphoreTake(mutex, portMAX_DELAY);
  Serial.print('[');
  Serial.print(level == nullptr ? "" : level);
  Serial.print("] ");
  Serial.println(message == nullptr ? "" : message);
  if (mutex != nullptr) xSemaphoreGive(mutex);
}

}  // namespace WallE
