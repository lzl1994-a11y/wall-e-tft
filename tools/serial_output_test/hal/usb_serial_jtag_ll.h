#pragma once
inline int usb_serial_jtag_ll_txfifo_writable() {
  return serialTestFifoWritable;
}
inline void usb_serial_jtag_ll_txfifo_flush() {
  if (!serialMutexHeld || !serialTestFifoWritable) std::abort();
  Serial.events.emplace_back("ZLP");
}
