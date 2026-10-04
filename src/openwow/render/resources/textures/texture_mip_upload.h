#pragma once

#include "openwow/data/blp/blp_texture_loader.h"

#include <bgfx/bgfx.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

namespace openwow::render {

enum class BlpUploadFormat : std::uint8_t {
  kRgba8,
  kBc1,
  kBc2,
  kBc3,
};

struct BlockCompressionSupport {
  bool bc1{false};
  bool bc2{false};
  bool bc3{false};

  [[nodiscard]] constexpr bool Supports(
      const BlpUploadFormat format) const noexcept {
    switch (format) {
      case BlpUploadFormat::kBc1:
        return bc1;
      case BlpUploadFormat::kBc2:
        return bc2;
      case BlpUploadFormat::kBc3:
        return bc3;
      case BlpUploadFormat::kRgba8:
        break;
    }
    return true;
  }
};

[[nodiscard]] constexpr std::uint32_t BlpUploadFormatBytesPerBlock(
    const BlpUploadFormat format) noexcept {
  switch (format) {
    case BlpUploadFormat::kBc1:
      return 8u;
    case BlpUploadFormat::kBc2:
    case BlpUploadFormat::kBc3:
      return 16u;
    case BlpUploadFormat::kRgba8:
      break;
  }
  return 64u;
}

[[nodiscard]] bgfx::TextureFormat::Enum ToBgfxTextureFormat(
    BlpUploadFormat format) noexcept;

[[nodiscard]] std::optional<std::uint32_t> BlpUploadMipSize(
    std::uint32_t width, std::uint32_t height, std::uint8_t level,
    BlpUploadFormat format) noexcept;

[[nodiscard]] std::optional<std::uint32_t> BlpUploadMipRowPitch(
    std::uint32_t width, BlpUploadFormat format) noexcept;

[[nodiscard]] BlockCompressionSupport QueryBlockCompressionSupport();

BlockCompressionSupport RefreshBlockCompressionSupport();

[[nodiscard]] BlockCompressionSupport CurrentBlockCompressionSupport() noexcept;

struct BlpRgbaMipUpload {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint8_t retail_mip_count = 0;
  std::uint8_t decoded_mip_count = 0;
  bool complete_mip_chain = false;

  BlpUploadFormat format = BlpUploadFormat::kRgba8;
  std::vector<std::uint32_t> mip_offsets;
  std::vector<std::uint32_t> mip_sizes;
  std::vector<std::uint8_t> bytes;
};

BlpRgbaMipUpload BuildBlpRgbaMipUpload(const data::BLPTextureData& blp,
                                       BlockCompressionSupport gpu_support = {});

// Vanilla "Texture Detail" (CVar `baseMip`): het eerste mip-niveau dat de GPU krijgt.
// 0 = volledige resolutie, 1 = de bovenste mip overslaan (halve resolutie). Wordt elk
// frame uit de CVar gezet; geldt voor textures die daarna worden geladen.
[[nodiscard]] inline std::atomic<std::uint8_t>& WorldTextureBaseMip() noexcept {
  static std::atomic<std::uint8_t> value{0};
  return value;
}

// Laat de eerste `count` mips van een upload vallen, zodat het daaropvolgende niveau het
// basisniveau wordt. Geeft false (en wijzigt niets) als er te weinig mips zijn.
bool DropLeadingMips(BlpRgbaMipUpload& upload, std::uint8_t count);

}
