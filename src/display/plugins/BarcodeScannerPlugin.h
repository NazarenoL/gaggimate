#ifndef BARCODE_SCANNER_PLUGIN_H
#define BARCODE_SCANNER_PLUGIN_H

#include <Arduino.h>
#include <atomic>
#include <display/core/BarcodeReader.h>
#include <display/core/Plugin.h>

class BarcodeScannerPlugin : public Plugin {
  public:
    void setup(Controller *controller, PluginManager *plugins) override;
    void loop() override;

  private:
    Controller *controller = nullptr;
    PluginManager *plugins = nullptr;
    BarcodeReader reader;
    BarcodeScanFilter scanFilter;
    std::atomic<bool> resetPending{true};
    bool wasEnabled = false;
    bool initialized = false;
    std::atomic<uint32_t> currentSession{0};
    uint32_t nextSession = 0;
    void dispatchCode(const String &code);
};

#endif
