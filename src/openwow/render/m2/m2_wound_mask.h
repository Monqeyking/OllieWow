#pragma once

#include <cstddef>
#include <cstdint>

namespace openwow::render::m2 {

// Dependency-light actual production mask seam; Bones entries expose .parent.
// Wound inherits along its own keyed subtree, not along primary slot ownership.
template <typename Bones, typename KeyBoneLookup>
[[nodiscard]] inline bool M2WoundMaskAffectsBone(
    const Bones &bones, const KeyBoneLookup &key_bone_lookup,
    const std::uint32_t keybone_slot, const std::size_t bone_index) {
  if (bone_index >= bones.size()) {
    return false;
  }
  if (keybone_slot == 0xFFFFFFFFu) {
    return true;
  }
  if (keybone_slot >= key_bone_lookup.size()) {
    return false;
  }
  const auto root = key_bone_lookup[keybone_slot];
  if (root < 0 || static_cast<std::size_t>(root) >= bones.size()) {
    return false;
  }
  // Strictly decreasing indices match M2 hierarchy evaluation and bound cycles.
  for (std::size_t cursor = bone_index;;) {
    if (cursor == static_cast<std::size_t>(root)) {
      return true;
    }
    const auto parent = bones[cursor].parent;
    if (parent < 0 || static_cast<std::size_t>(parent) >= cursor) {
      return false;
    }
    cursor = static_cast<std::size_t>(parent);
  }
}

} // namespace openwow::render::m2
