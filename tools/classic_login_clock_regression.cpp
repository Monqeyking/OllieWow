// Offline regression for the actual SessionHandler; no client/server or assets.
// From an x64 MSVC developer shell, working directory build/ (not yet run):
// cl /nologo /std:c++20 /EHsc /O2 /Gy /I../src /I../include
//   ../tools/classic_login_clock_regression.cpp
//   ../src/openwow/game/session_handler.cpp
//   ../src/openwow/game/game_time_callback_registry.cpp
//   ../src/openwow/runtime/time/game_time.cpp
//   ../src/openwow/runtime/time/game_clock.cpp
//   ../src/openwow/net/wotlk/main_thread_packet_dispatcher.cpp
//   ../src/openwow/foundation/diagnostics/logging.cpp
//   /Fe:classic_login_clock_regression.exe /link /OPT:REF
// Then run .\classic_login_clock_regression.exe from the same build directory.
// /Gy + /OPT:REF discard unrelated session/cache code; dependencies are the
// listed production sources and the C++20 standard library, no assets or network.
// No CMake target is added. Checks remain enabled under NDEBUG.
#include "openwow/game/session_handler.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using openwow::game::SessionHandler;
using openwow::core::ida::GameTimeData;

void Check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

std::array<std::uint8_t, 16> Packet(std::uint32_t packed, float speed) {
  std::array<std::uint8_t, 16> bytes{};
  const auto bits = std::bit_cast<std::uint32_t>(speed);
  for (unsigned i = 0; i < 4; ++i) {
    bytes[i] = static_cast<std::uint8_t>(packed >> (8 * i));
    bytes[4 + i] = static_cast<std::uint8_t>(bits >> (8 * i));
  }
  // Explicitly nonzero former timezone field: 12 bytes must be rejected.
  bytes[8] = 7;
  return bytes;
}

bool CountMinute(const openwow::game::GameTimeCallbackMoment&, void* context) {
  ++*static_cast<unsigned*>(context);
  return true;
}

void RejectUnchanged(SessionHandler& session, GameTimeData& clock,
                     const std::uint8_t* bytes, std::size_t size,
                     unsigned& callbacks) {
  std::array<unsigned char, sizeof(GameTimeData)> before{};
  std::memcpy(before.data(), &clock, sizeof(clock));
  const auto callback_count = callbacks;
  const auto registrations = session.game_time_callbacks().RegisteredCount();
  Check(!session.HandleLoginSetTimeSpeed(bytes, size), "malformed packet accepted");
  Check(std::memcmp(before.data(), &clock, sizeof(clock)) == 0,
        "rejected packet mutated clock");
  Check(callbacks == callback_count, "rejected packet dispatched callback");
  Check(session.game_time_callbacks().RegisteredCount() == registrations,
        "rejected packet mutated callback registry");
}
}  // namespace

int main() {
  try {
    static_assert(std::endian::native == std::endian::little,
                  "PacketReader currently requires a little-endian host");
    GameTimeData clock{};
    SessionHandler session(&clock);
    // Packed Classic tuple: minute 37, hour 13, weekday 2, day 8,
    // month 4, year 25 (day/month are zero-based, year relative to 2000).
    constexpr std::uint32_t packed = 37u | (13u << 6) | (2u << 11) |
        (8u << 14) | (4u << 20) | (25u << 24);
    constexpr float speed = 0.01666667f;  // Exact local server literal.
    auto bytes = Packet(packed, speed);
    unsigned callbacks = 0;
    const auto handle = session.game_time_callbacks().Register(
        {.minute = 37, .hour = 13}, CountMinute, &callbacks);
    Check(handle != openwow::game::GameTimeCallbackRegistry::kInvalidHandle,
          "callback registration failed");

    // Seed nondefault timing state: rejects must preserve every clock byte.
    clock.fractional_minute = 0.25f;
    clock.deferred_minutes = 3;
    clock.time_speed = 2.0f;
    for (std::size_t length = 0; length <= bytes.size(); ++length) {
      if (length != 8) RejectUnchanged(session, clock, bytes.data(), length, callbacks);
    }
    RejectUnchanged(session, clock, nullptr, 0, callbacks);
    RejectUnchanged(session, clock, nullptr, 8, callbacks);
    for (float invalid : {0.0f, -0.0f, -1.0f,
                         std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::quiet_NaN()}) {
      const auto bad = Packet(packed, invalid);
      RejectUnchanged(session, clock, bad.data(), 8, callbacks);
    }

    Check(session.HandleLoginSetTimeSpeed(bytes.data(), 8), "Classic packet rejected");
    const auto decoded = session.game_time();
    Check(decoded.packed_time == packed, "packed tuple changed");
    Check(decoded.tz_hint == 0, "Classic packet supplied a timezone");
    Check(decoded.game_speed == speed, "minutes/second speed changed");
    Check(clock.minute == 37 && clock.hour == 13 && clock.weekday == 2 &&
          clock.day == 8 && clock.month == 4 && clock.year == 25,
          "date/time fields decoded incorrectly");
    Check(callbacks == 1, "login minute callback not dispatched exactly once");

    // Rejections must also be atomic after a successful clock initialization.
    bytes = Packet(packed, speed);
    RejectUnchanged(session, clock, bytes.data(), 12, callbacks);
    clock.fractional_minute = 0.0f;
    clock.deferred_minutes = 0;
    session.AdvanceGameTime(60.0f);
    Check(clock.minute == 38 && clock.hour == 13,
          "60 real seconds did not advance one game minute");

    // Preserve the existing setter's clamps for finite, positive speeds.
    for (float positive : {std::numeric_limits<float>::denorm_min(), 0.001f,
                           1.0f, 60.0f, std::numeric_limits<float>::max()}) {
      bytes = Packet(packed, positive);
      Check(session.HandleLoginSetTimeSpeed(bytes.data(), 8), "positive speed rejected");
      const float expected = positive < openwow::core::ida::kRetailMinimumGameSpeed
          ? openwow::core::ida::kRetailMinimumGameSpeed
          : positive > openwow::core::ida::kRetailMaximumGameSpeed
              ? openwow::core::ida::kRetailMaximumGameSpeed : positive;
      Check(clock.time_speed == expected, "existing speed clamp changed");
    }

    // Exercise existing notify-current-minute rollback at midnight. Use the
    // calendar-consistent weekday: 2025-05-09 is Friday (5), not Tuesday (2).
    // Midnight crosses AddDays twice, which recomputes weekday from the date.
    constexpr std::uint32_t midnight = (5u << 11) | (8u << 14) |
        (4u << 20) | (25u << 24);
    bytes = Packet(midnight, speed);
    Check(session.HandleLoginSetTimeSpeed(bytes.data(), 8), "midnight packet rejected");
    Check(session.game_time().packed_time == midnight, "midnight tuple changed");
    std::cout << "Classic login clock regression passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
