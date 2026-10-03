// Standalone offline fixture: compile/link with src/openwow/net/wotlk/spell_packets.cpp.
// Run/build only from the existing build directory after approval. No GUI/server/data needed.
// Contract: local Source/src/game/Spells/Spell.cpp (5086-5087, 5132-5134, 7714-7716).
#include "openwow/net/wotlk/spell_packets.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
void Require(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
void Word(Bytes& bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}
template <typename Parser>
void CheckBounds(const Bytes& bytes, Parser parse) {
  Require(!parse(nullptr, bytes.size()), "null input accepted");
  for (std::size_t length = 0; length < bytes.size(); ++length)
    Require(!parse(bytes.data(), length), "truncated packet accepted");
  auto trailing = bytes;
  trailing.push_back(0);
  Require(!parse(trailing.data(), trailing.size()), "trailing byte accepted");
}
}

int main() {
  try {
    using namespace openwow::net::wotlk;
    Bytes start;
    Word(start, 10797); // Starshards: same representative fixture as Benilla.
    Word(start, 6000);
    const auto channel = ParseChannelStart(start.data(), start.size());
    Require(channel && channel->spell_id == 10797 && channel->duration == 6000,
            "Classic 8-byte channel start decoded incorrectly");
    CheckBounds(start, ParseChannelStart);

    for (const std::uint32_t remaining : {3000u, 0u}) {
      Bytes update;
      Word(update, remaining);
      const auto result = ParseChannelUpdate(update.data(), update.size());
      Require(result && result->remaining == remaining,
              "Classic channel update/stop decoded incorrectly");
      CheckBounds(update, ParseChannelUpdate);
    }
    // Both sparse and full GUIDs distinguish raw from packed decoding.
    for (const std::uint64_t guid : {0x0000000000000042ULL, 0x0807060504030201ULL}) {
      Bytes delayed;
      Word(delayed, static_cast<std::uint32_t>(guid));
      Word(delayed, static_cast<std::uint32_t>(guid >> 32));
      Word(delayed, 500);
      const auto result = ParseSpellDelayed(delayed.data(), delayed.size());
      Require(result && result->caster_guid.GetRawValue() == guid && result->delay_time == 500,
              "Classic raw-GUID pushback decoded incorrectly");
      CheckBounds(delayed, ParseSpellDelayed);
    }
    // Former WotLK-shaped channel bodies must not silently shift the fields.
    Bytes old_start(9, 0xff);
    old_start.insert(old_start.end(), start.begin(), start.end());
    Require(!ParseChannelStart(old_start.data(), old_start.size()),
            "GUID-prefixed channel start accepted");
    Bytes old_update(9, 0xff);
    Word(old_update, 3000);
    Require(!ParseChannelUpdate(old_update.data(), old_update.size()),
            "GUID-prefixed channel update accepted");
    std::cout << "Classic spell packet fixtures passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
