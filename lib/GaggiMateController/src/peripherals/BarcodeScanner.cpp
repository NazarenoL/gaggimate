#include "BarcodeScanner.h"
#include <driver/gpio.h>
#include <driver/uart.h>

void BarcodeScanner::setup(int rxPin) {
    uart_config_t config{};
    config.baud_rate = 9600;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_APB;
    initialized = uart_param_config(UART_NUM_1, &config) == ESP_OK &&
                  uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) == ESP_OK &&
                  uart_driver_install(UART_NUM_1, 2048, 0, 0, nullptr, 0) == ESP_OK;
    if (initialized) {
        gpio_pullup_en(static_cast<gpio_num_t>(rxPin));
        ESP_LOGI("BarcodeScanner", "Module D input on expansion EXT4 (GPIO%d), 9600 8N1", rxPin);
    } else
        ESP_LOGE("BarcodeScanner", "Could not initialize scanner UART");
}

void BarcodeScanner::loop(bool healthy) {
    if (!initialized)
        return;
    uint32_t session = healthy && !comms.isUpdating() && comms.isConnected() ? comms.barcodeSession() : 0;
    if (session == 0 || session != previousSession) {
        bool partial = reader.hasPartialFrame();
        uint8_t byte;
        for (int i = 0; i < 1024 && uart_read_bytes(UART_NUM_1, &byte, 1, 0) == 1; ++i)
            partial = byte != '\r' && byte != '\n';
        reader.reset();
        filter.reset();
        if (partial)
            reader.discardFrame();
        previousSession = session;
        return;
    }
    std::string code;
    for (int i = 0; i < 1024; ++i) {
        uint8_t byte;
        if (uart_read_bytes(UART_NUM_1, &byte, 1, 0) != 1)
            break;
        if (reader.push(static_cast<char>(byte), millis(), code) && filter.accept(code, millis()))
            comms.sendBarcode(code, session);
    }
}
