#pragma once
#include <cstdlib>
using SemaphoreHandle_t = void*;
extern bool serialMutexHeld;
inline SemaphoreHandle_t xSemaphoreCreateMutex() {
  static int mutex;
  return &mutex;
}
inline int xSemaphoreTake(SemaphoreHandle_t, unsigned int) {
  if (serialMutexHeld) std::abort();
  serialMutexHeld = true;
  return 1;
}
inline int xSemaphoreGive(SemaphoreHandle_t) {
  if (!serialMutexHeld) std::abort();
  serialMutexHeld = false;
  return 1;
}
