#include <BarcodeTransfer.h>
#include <display/core/BarcodeReader.h>
#include <unity.h>
#include <vector>

static std::vector<std::string> readCodes(const std::string &bytes) {
    BarcodeReader reader;
    std::vector<std::string> codes;
    std::string code;
    uint32_t now = 0;
    for (char byte : bytes)
        if (reader.push(byte, now++, code))
            codes.push_back(code);
    return codes;
}

void setUp() {}
void tearDown() {}

void test_payloads_and_framing() {
    auto codes = readCodes("\r\n0012345678905\r\nhttps://roaster.example/bag/42?lot=abc\n coffee:é \rGS1\x1d"
                           "123\r");
    TEST_ASSERT_EQUAL(4, codes.size());
    TEST_ASSERT_EQUAL_STRING("0012345678905", codes[0].c_str());
    TEST_ASSERT_EQUAL_STRING("https://roaster.example/bag/42?lot=abc", codes[1].c_str());
    TEST_ASSERT_EQUAL_STRING(" coffee:é ", codes[2].c_str());
    TEST_ASSERT_EQUAL_STRING("GS1\x1d"
                             "123",
                             codes[3].c_str());
}

void test_invalid_frames_cannot_select_their_suffix() {
    auto codes = readCodes(std::string(513, 'a') + "KNOWN\rOK\r" + std::string("bad\0KNOWN\n", 10) + "NEXT\n");
    TEST_ASSERT_EQUAL(2, codes.size());
    TEST_ASSERT_EQUAL_STRING("OK", codes[0].c_str());
    TEST_ASSERT_EQUAL_STRING("NEXT", codes[1].c_str());
    codes = readCodes(std::string(512, 'x') + "\r");
    TEST_ASSERT_EQUAL(1, codes.size());
    TEST_ASSERT_EQUAL(512, codes[0].size());
}

void test_partial_and_timed_out_frames() {
    BarcodeReader reader;
    std::string code;
    TEST_ASSERT_FALSE(reader.push('a', 1, code));
    TEST_ASSERT_FALSE(reader.push('b', 2, code));
    TEST_ASSERT_FALSE(reader.push('c', 503, code));
    TEST_ASSERT_FALSE(reader.push('\r', 504, code));
    TEST_ASSERT_FALSE(reader.push('d', 505, code));
    TEST_ASSERT_TRUE(reader.push('\n', 506, code));
    TEST_ASSERT_EQUAL_STRING("d", code.c_str());
    TEST_ASSERT_FALSE(reader.push('a', UINT32_MAX - 20, code));
    TEST_ASSERT_TRUE(reader.push('\r', 5, code));
    TEST_ASSERT_EQUAL_STRING("a", code.c_str());
}

void test_payload_validation() {
    TEST_ASSERT_TRUE(validBeanCode("000123"));
    TEST_ASSERT_TRUE(validBeanCode(std::string(512, 'a')));
    TEST_ASSERT_FALSE(validBeanCode(""));
    TEST_ASSERT_FALSE(validBeanCode(std::string(513, 'a')));
    TEST_ASSERT_FALSE(validBeanCode("a\nb"));
    TEST_ASSERT_FALSE(validBeanCode("a\rb"));
    TEST_ASSERT_FALSE(validBeanCode(std::string("a\0b", 3)));
}

void test_discarding_a_frame_across_mode_changes() {
    BarcodeReader reader;
    std::string code;
    reader.push('a', 1, code);
    TEST_ASSERT_TRUE(reader.hasPartialFrame());
    reader.discardFrame();
    TEST_ASSERT_FALSE(reader.push('b', 2, code));
    TEST_ASSERT_FALSE(reader.push('\r', 3, code));
    TEST_ASSERT_FALSE(reader.hasPartialFrame());
    reader.push('c', 4, code);
    TEST_ASSERT_TRUE(reader.push('\n', 5, code));
    TEST_ASSERT_EQUAL_STRING("c", code.c_str());
}

void test_module_d_continuous_scan_filter() {
    BarcodeScanFilter filter;
    TEST_ASSERT_TRUE(filter.accept("0012345678905", 0));
    // The quiet interval starts at the most recent read, not the first one.
    TEST_ASSERT_FALSE(filter.accept("0012345678905", 1000));
    TEST_ASSERT_FALSE(filter.accept("0012345678905", 2000));
    TEST_ASSERT_FALSE(filter.accept("0012345678905", 3000));
    TEST_ASSERT_TRUE(filter.accept("0012345678905", 4500));
    TEST_ASSERT_TRUE(filter.accept("https://roaster.example/bag/42", 4501));
    TEST_ASSERT_TRUE(filter.accept("0012345678905", 4502));
    filter.reset();
    TEST_ASSERT_TRUE(filter.accept("0012345678905", 4503));
}

void test_module_d_scan_filter_timer_wraparound() {
    BarcodeScanFilter filter;
    TEST_ASSERT_TRUE(filter.accept("bag", UINT32_MAX - 500));
    TEST_ASSERT_FALSE(filter.accept("bag", 499));
    TEST_ASSERT_TRUE(filter.accept("bag", 1999));
}

void test_controller_transfer_preserves_maximum_payload() {
    BarcodeTransfer transfer;
    std::string expected = "https://roaster.example/bag/";
    expected.resize(512, 'a');
    TEST_ASSERT_EQUAL(512, expected.size());
    std::string result;
    for (size_t offset = 0; offset < expected.size(); offset += BARCODE_FRAGMENT_BYTES) {
        size_t size = std::min(BARCODE_FRAGMENT_BYTES, expected.size() - offset);
        bool complete = transfer.append(7, 7, 12, offset, expected.size(),
                                        reinterpret_cast<const uint8_t *>(expected.data() + offset), size, result);
        TEST_ASSERT_EQUAL(offset + size == expected.size(), complete);
    }
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

void test_controller_transfer_rejects_standby_and_old_sessions() {
    BarcodeTransfer transfer;
    std::string result;
    const uint8_t data[] = {'a', 'b', 'c'};
    TEST_ASSERT_FALSE(transfer.append(0, 1, 1, 0, 3, data, 3, result));
    TEST_ASSERT_FALSE(transfer.append(2, 1, 1, 0, 3, data, 3, result));
    TEST_ASSERT_FALSE(transfer.append(2, 2, 1, 0, 6, data, 3, result));
    TEST_ASSERT_FALSE(transfer.append(3, 2, 1, 3, 6, data, 3, result));
    TEST_ASSERT_FALSE(transfer.append(3, 3, 1, 3, 6, data, 3, result));
    TEST_ASSERT_TRUE(transfer.append(3, 3, 2, 0, 3, data, 3, result));
    TEST_ASSERT_EQUAL_STRING("abc", result.c_str());
}

void test_controller_transfer_rejects_invalid_fragments() {
    BarcodeTransfer transfer;
    std::string result;
    const uint8_t data[] = {'a', 'b', 'c'};
    TEST_ASSERT_FALSE(transfer.append(1, 1, 0, 0, 3, data, 3, result));
    TEST_ASSERT_FALSE(transfer.append(1, 1, 1, 0, 513, data, 3, result));
    TEST_ASSERT_FALSE(transfer.append(1, 1, 1, 0, 2, data, 3, result));
    TEST_ASSERT_FALSE(transfer.append(1, 1, 1, 0, 6, data, 3, result));
    TEST_ASSERT_FALSE(transfer.append(1, 1, 2, 3, 6, data, 3, result));
    const uint8_t nul[] = {'a', 0, 'b'};
    TEST_ASSERT_FALSE(transfer.append(1, 1, 3, 0, 3, nul, 3, result));
    TEST_ASSERT_TRUE(transfer.append(1, 1, 4, 0, 3, data, 3, result));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_payloads_and_framing);
    RUN_TEST(test_invalid_frames_cannot_select_their_suffix);
    RUN_TEST(test_partial_and_timed_out_frames);
    RUN_TEST(test_payload_validation);
    RUN_TEST(test_discarding_a_frame_across_mode_changes);
    RUN_TEST(test_module_d_continuous_scan_filter);
    RUN_TEST(test_module_d_scan_filter_timer_wraparound);
    RUN_TEST(test_controller_transfer_preserves_maximum_payload);
    RUN_TEST(test_controller_transfer_rejects_standby_and_old_sessions);
    RUN_TEST(test_controller_transfer_rejects_invalid_fragments);
    return UNITY_END();
}
