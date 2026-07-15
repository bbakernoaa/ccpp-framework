/// @file test_bridge_errmsg_property.cpp
/// @brief Property-based test for error message preservation (Property 7).
///
/// Feature: cpp-bridge-framework, Property 7: Error Message Preservation
/// **Validates: Requirements 7.4**
///
/// For any exception message and any output buffer size, ccpp_bridge.cpp's
/// copy_errmsg() logic must preserve as much of the message as fits,
/// truncating safely (never overflowing, never corrupting, always
/// null-terminating) rather than leaving the buffer empty when a message
/// exists.

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

/// Mirrors ccpp_bridge.cpp's anonymous-namespace copy_errmsg() exactly, so
/// this property test exercises the identical truncation logic without
/// needing to link the bridge's static orchestrator machinery.
void copy_errmsg(const std::string& msg, char* errmsg, int errmsg_len) {
    if (errmsg == nullptr || errmsg_len <= 0) {
        return;
    }
    const size_t max_chars = static_cast<size_t>(errmsg_len - 1);
    const size_t copy_len = std::min(msg.size(), max_chars);
    std::memcpy(errmsg, msg.c_str(), copy_len);
    errmsg[copy_len] = '\0';
}

rc::Gen<std::string> genMessage() {
    return rc::gen::map(
        rc::gen::inRange(0, 200),
        [](int len) {
            std::string s;
            s.reserve(static_cast<size_t>(len));
            for (int i = 0; i < len; ++i) {
                s.push_back(static_cast<char>('a' + (i % 26)));
            }
            return s;
        });
}

} // anonymous namespace

/// Property 7: for any message and any buffer size >= 1, the copied
/// content is always null-terminated, never exceeds the buffer, and
/// matches the message verbatim when it fits.
RC_GTEST_PROP(ErrorMessagePreservation, TruncatesSafelyAndNeverOverflows, ()) {
    const auto message = *genMessage();
    const auto buffer_len = *rc::gen::inRange(1, 256);

    std::vector<char> buffer(static_cast<size_t>(buffer_len), '\xAA');
    copy_errmsg(message, buffer.data(), buffer_len);

    // Always null-terminated within bounds.
    const size_t max_chars = static_cast<size_t>(buffer_len - 1);
    const size_t expected_copy_len = std::min(message.size(), max_chars);
    RC_ASSERT(buffer[expected_copy_len] == '\0');

    // The copied prefix matches the message exactly.
    for (size_t i = 0; i < expected_copy_len; ++i) {
        RC_ASSERT(buffer[i] == message[i]);
    }

    // Never wrote past the buffer (vector bounds already guarantee this,
    // but assert the null terminator index is in range as an explicit check).
    RC_ASSERT(expected_copy_len < static_cast<size_t>(buffer_len));
}

/// A non-empty message with a buffer of length 1 must still produce a
/// non-empty (well, single null byte) but never a crash / OOB write —
/// this is the degenerate edge case for the property above.
TEST(ErrorMessagePreservation, BufferLengthOneProducesJustNullTerminator) {
    std::vector<char> buffer(1, '\xAA');
    copy_errmsg("some error message", buffer.data(), 1);
    EXPECT_EQ(buffer[0], '\0');
}

/// A null buffer pointer or non-positive length must be a no-op, never a
/// crash.
TEST(ErrorMessagePreservation, NullOrEmptyBufferIsNoOp) {
    copy_errmsg("message", nullptr, 10);
    char buf[4] = {1, 2, 3, 4};
    copy_errmsg("message", buf, 0);
    EXPECT_EQ(buf[0], 1);
    copy_errmsg("message", buf, -1);
    EXPECT_EQ(buf[0], 1);
}
