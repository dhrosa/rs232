#pragma once

#include <tusb.h>

class CdcDevice {
 public:
  CdcDevice(uint8_t id) : id_(id) {}

  int ReadAvailable() { return tud_cdc_n_available(id_); }

  char ReadChar() { return tud_cdc_n_read_char(id_); }

  void WriteChar(char c) { tud_cdc_n_write_char(id_, c); }

  void Flush() { tud_cdc_n_write_flush(id_); }

 private:
  const uint8_t id_;
};
