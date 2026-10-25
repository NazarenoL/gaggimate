#pragma once

#include "BarcodeReader.h"

constexpr size_t BARCODE_FRAGMENT_BYTES = 48;

// Reliable frames arrive in order. A session changes whenever the display
// disables scanning, preventing an old partial/queued scan from waking it.
class BarcodeTransfer {
  public:
    bool append(uint32_t expectedSession, uint32_t session, uint32_t scanId, size_t offset, size_t total, const uint8_t *data,
                size_t size, std::string &code) {
        if (!expectedSession || session != expectedSession || !scanId || !size || size > BARCODE_FRAGMENT_BYTES || !total ||
            total > MAX_BEAN_CODE_BYTES || offset > total || size > total - offset) {
            reset();
            return false;
        }
        if (offset == 0) {
            reset();
            currentSession = session;
            currentScan = scanId;
            currentTotal = total;
        }
        if (session != currentSession || scanId != currentScan || total != currentTotal || offset != buffer.size()) {
            reset();
            return false;
        }
        buffer.append(reinterpret_cast<const char *>(data), size);
        if (buffer.size() != total)
            return false;
        bool valid = validBeanCode(buffer);
        if (valid)
            code = buffer;
        reset();
        return valid;
    }

    void reset() {
        buffer.clear();
        currentSession = currentScan = 0;
        currentTotal = 0;
    }

  private:
    std::string buffer;
    uint32_t currentSession = 0, currentScan = 0;
    size_t currentTotal = 0;
};
