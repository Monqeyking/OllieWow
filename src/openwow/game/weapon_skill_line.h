#pragma once

#include <array>
#include <cstdint>

namespace openwow::game {

// Classic/Turtle Item.cpp's item_weapon_skills table (class 2, subclasses 0..20).
inline constexpr std::uint32_t kSkillUnarmed = 162u;
inline constexpr std::array<std::uint32_t, 21> kWeaponSubclassSkillLines{{
    44u,  172u, 45u,  46u,  54u, 160u, 229u,
    43u,  55u,  0u,  136u, 0u,  0u, 162u,
    0u,  173u, 176u, 253u, 226u, 228u, 356u,
}};

[[nodiscard]] constexpr std::uint32_t WeaponSubclassSkillLineOrZero(
    const std::uint32_t subclass) noexcept {
  return subclass < kWeaponSubclassSkillLines.size()
             ? kWeaponSubclassSkillLines[subclass]
             : 0u;
}

}  // namespace openwow::game
