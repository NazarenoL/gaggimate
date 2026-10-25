#include "BarcodeScannerPlugin.h"
#include <display/config.h>
#include <display/core/Controller.h>
#include <display/core/PluginManager.h>
#include <display/plugins/ShotHistoryPlugin.h>

#if !defined(GAGGIMATE_SIM) && defined(BARCODE_SCANNER_RX_PIN)
#include <driver/uart.h>
#ifndef BARCODE_SCANNER_BAUD
// Waveshare Barcode Scanner Module (D): factory UART mode is 9600 baud, 8N1.
#define BARCODE_SCANNER_BAUD 9600
#endif
#endif

void BarcodeScannerPlugin::setup(Controller *owner, PluginManager *manager) {
    controller = owner;
    plugins = manager;
    for (const char *event : {"controller:mode:change", "controller:brew:start", "ota:update:start", "controller:error",
                              "controller:bluetooth:connect", "controller:bluetooth:disconnect"}) {
        plugins->on(event, [this](Event const &) {
            currentSession = 0;
            resetPending = true;
        });
    }
    plugins->on("controller:barcode", [this](Event const &event) {
        uint32_t session = currentSession.load();
        if (session && session == static_cast<uint32_t>(event.getInt("session")))
            dispatchCode(event.getString("code"));
    });
#if !defined(GAGGIMATE_SIM) && defined(BARCODE_SCANNER_RX_PIN)
    // Receive-only: the scanner is configured using its printed setup barcodes.
    uart_config_t config{};
    config.baud_rate = BARCODE_SCANNER_BAUD;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_APB;
    initialized =
        uart_param_config(UART_NUM_1, &config) == ESP_OK &&
        uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, BARCODE_SCANNER_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) == ESP_OK &&
        uart_driver_install(UART_NUM_1, 2048, 0, 0, nullptr, 0) == ESP_OK;
    if (!initialized)
        ESP_LOGE("BarcodeScanner", "Could not initialize scanner UART");
#endif
}

void BarcodeScannerPlugin::loop() {
    bool enabled = controller->getMode() != MODE_STANDBY && !controller->isActive() && !controller->isUpdating() &&
                   !controller->isAutotuning() && !controller->isErrorState();
    bool reset = resetPending.exchange(false) || enabled != wasEnabled;
    if (reset) {
        uint32_t session = 0;
        if (enabled) {
            if (++nextSession == 0)
                ++nextSession;
            session = nextSession;
        }
        currentSession = session;
        controller->setBarcodeSession(session);
        wasEnabled = enabled;
    }
#if !defined(GAGGIMATE_SIM) && defined(BARCODE_SCANNER_RX_PIN)
    if (!initialized)
        return;
    if (reset || !enabled) {
        // Never replay bytes received in standby or during a brew after waking up.
        bool partial = reader.hasPartialFrame();
        uint8_t byte;
        while (uart_read_bytes(UART_NUM_1, &byte, 1, 0) == 1)
            partial = byte != '\r' && byte != '\n';
        reader.reset();
        scanFilter.reset();
        if (partial)
            reader.discardFrame();
        return;
    }
    std::string code;
    // Bound work on the controller loop, without blocking for a whole frame.
    for (int i = 0; i < 1024; ++i) {
        if (resetPending.load())
            break;
        uint8_t byte;
        if (uart_read_bytes(UART_NUM_1, &byte, 1, 0) != 1)
            break;
        if (reader.push(static_cast<char>(byte), millis(), code)) {
            if (resetPending.load() || !scanFilter.accept(code, millis()))
                continue;
            dispatchCode(String(code.c_str()));
        }
    }
#endif
}

void BarcodeScannerPlugin::dispatchCode(const String &code) {
    if (!currentSession.load() || controller->getMode() == MODE_STANDBY || controller->isActive() || controller->isUpdating() ||
        controller->isAutotuning() || controller->isErrorState())
        return;
#ifdef GAGGIMATE_HEADLESS
    JsonDocument request, response;
    request["tp"] = "req:beans:scan";
    request["code"] = code;
    ShotHistory.handleBeansRequest(request, response);
    if (response["error"].isNull())
        controller->updateLastAction();
#else
    plugins->trigger("beans:barcode", "code", code);
#endif
}
