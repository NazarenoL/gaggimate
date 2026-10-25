#ifndef GAGGIMATE_BARCODE_READER_H
#define GAGGIMATE_BARCODE_READER_H

#include <cstdint>
#include <string>

constexpr size_t MAX_BEAN_CODES = 16;
constexpr size_t MAX_BEAN_CODE_BYTES = 512;

// Module D's factory continuous mode repeats a held code about once a second.
// Accept a different code immediately; require a quiet interval to accept the
// same code again. Refresh lastSeen even on suppressed reads.
class BarcodeScanFilter {
  public:
    bool accept(const std::string &code, uint32_t now) {
        bool repeated = code == lastCode && now - lastSeen < 1500;
        lastCode = code;
        lastSeen = now;
        return !repeated;
    }
    void reset() { lastCode.clear(); }

  private:
    std::string lastCode;
    uint32_t lastSeen = 0;
};

inline bool validBeanCode(const std::string &code) {
    return !code.empty() && code.size() <= MAX_BEAN_CODE_BYTES &&
           code.find_first_of(std::string("\r\n\0", 3)) == std::string::npos;
}

// The scanner decodes the symbology; UART carries its payload followed by CR/LF.
// Invalid, oversized and interrupted frames are discarded through the terminator.
class BarcodeReader {
  public:
    bool push(char byte, uint32_t now, std::string &code) {
        if (!buffer.empty() && now - lastByte > 500) {
            buffer.clear();
            discarding = true;
        }
        lastByte = now;
        if (byte == '\r' || byte == '\n') {
            bool complete = !discarding && !buffer.empty();
            if (complete)
                code = buffer;
            reset();
            return complete;
        }
        if (discarding)
            return false;
        if (byte == '\0' || buffer.size() >= MAX_BEAN_CODE_BYTES) {
            buffer.clear();
            discarding = true;
        } else
            buffer += byte;
        return false;
    }

    void reset() {
        buffer.clear();
        discarding = false;
    }
    void discardFrame() {
        buffer.clear();
        discarding = true;
    }
    bool hasPartialFrame() const { return discarding || !buffer.empty(); }

  private:
    std::string buffer;
    uint32_t lastByte = 0;
    bool discarding = false;
};

#endif
