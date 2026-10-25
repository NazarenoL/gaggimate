#pragma once

#include <BarcodeReader.h>
#include <GaggiMateServer.h>

// Module D receives 3.3 V power and sends decoded text to expansion EXT4.
class BarcodeScanner {
  public:
    explicit BarcodeScanner(GaggiMateServer &comms) : comms(comms) {}
    void setup(int rxPin);
    void loop(bool healthy);

  private:
    GaggiMateServer &comms;
    BarcodeReader reader;
    BarcodeScanFilter filter;
    uint32_t previousSession = 0;
    bool initialized = false;
};
