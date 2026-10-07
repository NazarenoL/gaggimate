#pragma once

#include <LittleFS.h>
#include <atomic>
#include <mutex>
#include <freertos/task.h>

// Browser converts photos to 480 x 480 little-endian RGB565. No image decoder
// or original, potentially very large photo is needed on the ESP32.
namespace StandbyPhoto {
constexpr size_t WIDTH = 480;
constexpr size_t BYTES = WIDTH * WIDTH * 2;
constexpr const char *PATH = "/standby.rgb565";
inline std::mutex mutex;
inline std::atomic<uint32_t> revision{0};

inline bool save(const uint8_t *pixels) {
    std::lock_guard<std::mutex> lock(mutex);
    const char *temp = "/standby.tmp";
    File file = LittleFS.open(temp, FILE_WRITE);
    bool ok = static_cast<bool>(file);
    for (size_t offset = 0; ok && offset < BYTES; offset += 4096) {
        const size_t length = (BYTES - offset < 4096) ? BYTES - offset : 4096;
        ok = file.write(pixels + offset, length) == length;
        // Let idle/network tasks run between flash writes on the hardware.
        vTaskDelay(1);
    }
    file.close();
    // LittleFS rename replaces the destination atomically. A failed upload
    // leaves the previous photo intact; all readers use the same lock.
    ok = ok && LittleFS.rename(temp, PATH);
    if (!ok)
        LittleFS.remove(temp);
    else
        ++revision;
    return ok;
}

inline bool remove() {
    std::lock_guard<std::mutex> lock(mutex);
    if (LittleFS.exists(PATH) && !LittleFS.remove(PATH))
        return false;
    ++revision;
    return true;
}

inline bool load(uint8_t *pixels) {
    std::lock_guard<std::mutex> lock(mutex);
    File file = LittleFS.open(PATH, FILE_READ);
    return file && file.size() == BYTES && file.read(pixels, BYTES) == BYTES;
}
} // namespace StandbyPhoto
