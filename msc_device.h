#pragma once

#include <tusb.h>

#include <string>

#include "flash.h"

class MscDevice {
 public:
  MscDevice(uint8_t lun, FlashDisk& disk) : lun_(lun), disk_(disk) {}

  FlashDisk& Disk() { return disk_; }

  void SetReady(bool ready = true) { ready_ = ready; }
  bool Ready() { return ready_; }

  void SetVendorId(std::string_view str) { vendor_id_.assign(str); }
  void SetProductId(std::string_view str) { product_id_.assign(str); }
  void SetProductRev(std::string_view str) { product_rev_.assign(str); }

  std::string_view VendorId() { return vendor_id_; }
  std::string_view ProductId() { return product_id_; }
  std::string_view ProductRev() { return product_rev_; }

 private:
  uint8_t lun_;
  FlashDisk& disk_;
  bool ready_ = false;

  std::string vendor_id_;
  std::string product_id_;
  std::string product_rev_;
};
