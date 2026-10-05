#pragma once

// Globale uniforms van de moderne fog (cvar fogModel); zie shaders/world_fog.sh.
//
// bgfx koppelt een user-uniform alleen aan shaders die NA createUniform worden aangemaakt ("User
// defined uniform is not found, it won't be set"). De uniforms worden daarom direct na bgfx::init
// gemaakt, vóór alle shaderprogramma's, en bij een shutdown weer losgelaten.

#include <bgfx/bgfx.h>

namespace openwow::render {

struct ModernFogUniformHandles {
  bgfx::UniformHandle modern = BGFX_INVALID_HANDLE;     // x = fogeinde van de scene (0 = classic), yzw = scene-fogkleur
  bgfx::UniformHandle end_color = BGFX_INVALID_HANDLE;  // rgb = eindfogkleur, w = einddistance
  bgfx::UniformHandle sun_dir = BGFX_INVALID_HANDLE;    // xyz = richting naar de zon, w = cosinus-drempel
  bgfx::UniformHandle sun_color = BGFX_INVALID_HANDLE;  // rgb = zon-fogkleur, w = sterkte (0 = uit)

  [[nodiscard]] bool Valid() const noexcept {
    return bgfx::isValid(modern) && bgfx::isValid(end_color) && bgfx::isValid(sun_dir) &&
           bgfx::isValid(sun_color);
  }
};

[[nodiscard]] inline ModernFogUniformHandles &ModernFogUniforms() noexcept {
  static ModernFogUniformHandles handles;
  return handles;
}

inline void CreateModernFogUniforms() {
  ModernFogUniformHandles &handles = ModernFogUniforms();
  if (!bgfx::isValid(handles.modern)) {
    handles.modern = bgfx::createUniform("u_fogModern", bgfx::UniformType::Vec4);
  }
  if (!bgfx::isValid(handles.end_color)) {
    handles.end_color = bgfx::createUniform("u_fogEndColor", bgfx::UniformType::Vec4);
  }
  if (!bgfx::isValid(handles.sun_dir)) {
    handles.sun_dir = bgfx::createUniform("u_fogSunDir", bgfx::UniformType::Vec4);
  }
  if (!bgfx::isValid(handles.sun_color)) {
    handles.sun_color = bgfx::createUniform("u_fogSunColor", bgfx::UniformType::Vec4);
  }
}

// bgfx::shutdown vernietigt de uniforms zelf; hier laten we alleen de verouderde handles los.
inline void ResetModernFogUniforms() noexcept {
  ModernFogUniforms() = ModernFogUniformHandles{};
}

}  // namespace openwow::render