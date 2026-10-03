#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
using std::min;
extern bool serialMutexHeld;
extern uint32_t serialTestClock;
extern uint32_t serialTestDrainAfter;
extern bool serialTestFifoWritable;
extern bool serialTestPlugged;
extern int serialTestFreeTx;
extern uint32_t serialTestRingDrainAfter;
inline uint32_t millis() { return serialTestClock; }
inline void delay(unsigned int ms) {
  serialTestClock += ms;
  if (serialTestClock >= serialTestDrainAfter) serialTestFifoWritable = true;
  if (serialTestClock >= serialTestRingDrainAfter) serialTestFreeTx = 256;
}
struct TestSerial {
  std::vector<std::string> events;
  size_t write(const uint8_t* data, size_t size) {
    if (!serialMutexHeld) std::abort();
    events.emplace_back(reinterpret_cast<const char*>(data), size);
    if (serialTestClock < serialTestRingDrainAfter) serialTestFreeTx = 0;
    return size;
  }
  void flush() {
    if (!serialMutexHeld) std::abort();
    events.emplace_back("FLUSH");
  }
  bool isPlugged() const { return serialTestPlugged; }
  int availableForWrite() const { return serialTestFreeTx; }
};
extern TestSerial Serial;
