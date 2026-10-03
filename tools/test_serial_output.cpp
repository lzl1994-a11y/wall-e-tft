#include "adapters/log/SerialOutput.h"
#include <cassert>
#include <iostream>

bool serialMutexHeld = false;
uint32_t serialTestClock = 0;
uint32_t serialTestDrainAfter = 0;
bool serialTestFifoWritable = true;
bool serialTestPlugged = true;
int serialTestFreeTx = 256;
uint32_t serialTestRingDrainAfter = 0;
TestSerial Serial;

void expectLine(const std::string& expected) {
  assert(!serialMutexHeld);
#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
  assert(Serial.events.size() == 3);
  assert(Serial.events[2] == "ZLP");
#else
  assert(Serial.events.size() == 2);
#endif
  assert(Serial.events[0] == expected);
  assert(Serial.events[1] == "FLUSH");
  Serial.events.clear();
}

int main() {
  WallE::serialPrintln("EYE:OK");
  expectLine("EYE:OK\r\n");
  WallE::serialWriteLine(nullptr, 100);
  expectLine("\r\n");
  std::string oversized(600, 'x');
  WallE::serialWriteLine(oversized.data(), oversized.size());
  expectLine(std::string(512, 'x') + "\r\n");
  WallE::serialLogLine("ERROR", "test");
  expectLine("[ERROR] test\r\n");
#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
  serialTestFifoWritable = false;
  serialTestDrainAfter = serialTestClock + 3;
  WallE::serialPrintln("EYE:OK");
  expectLine("EYE:OK\r\n");
  assert(serialTestClock == 3);
  // An ISR can have taken the final ring item without publishing its FIFO yet.
  serialTestRingDrainAfter = serialTestClock + 3;
  WallE::serialPrintln("EYE:OK");
  expectLine("EYE:OK\r\n");
  assert(serialTestClock == 6);
  serialTestFifoWritable = false;
  serialTestDrainAfter = UINT32_MAX;
  const uint32_t start = serialTestClock;
  WallE::serialPrintln("EYE:OK");
  assert(serialTestClock - start == 100);
  assert(!serialMutexHeld && Serial.events.size() == 2);
  Serial.events.clear();
  serialTestPlugged = false;
  WallE::serialPrintln("EYE:OK");
  assert(serialTestClock - start == 100);
  assert(!serialMutexHeld && Serial.events.size() == 2);
#endif
  std::cout << "serial output tests passed\n";
}
