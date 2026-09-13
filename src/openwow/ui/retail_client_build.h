#pragma once

#include <cstdint>

namespace openwow::ui {

inline constexpr char kRetailClientVersion[] = "3.3.5";
inline constexpr char kRetailClientBuildNumber[] = "12340";
inline constexpr char kRetailClientBuildDate[] = "Jun 25 2010";
inline constexpr std::uint32_t kRetailInterfaceVersion = 30300;

// The in-world Lua API targets the local Vanilla/Turtle client data. Keep the
// retail constants above for glue/protocol compatibility until those paths are
// migrated independently.
inline constexpr char kClassicClientVersion[] = "1.12.1";
inline constexpr char kClassicClientBuildNumber[] = "5875";
inline constexpr char kClassicClientBuildDate[] = "Sep 19 2006";
inline constexpr std::uint32_t kClassicInterfaceVersion = 11200;

}
