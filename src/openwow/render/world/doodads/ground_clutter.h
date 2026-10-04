#pragma once

// Vanilla ground clutter (detail doodads): het gras, de kruiden, bloemen en steentjes die per
// terreinchunk over het landschap worden gestrooid.
//
// Dit is een port van het algoritme van de originele client (CMapChunk::CreateDetailDoodads),
// zoals het is vastgelegd in Benilla (crates/benilla-formats/src/ground_effects.rs, MIT/Apache-2.0)
// en de ruistabel uit Noggit Red (ChunkAddDetailDoodads.cpp). Eén RNG-stroom per chunk, geseed met
// de globale chunkcoordinaten; pass 1 kiest `frillDensity` cellen (met herhaling), pass 2 plaatst
// per cel `density` doodads uit de vier slots van het effect van de dominante laag.
//
// Bewust zonder afhankelijkheden van de renderer, zodat dit los te testen is.

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace openwow::render::clutter {

inline constexpr float kChunkSize = 33.333332f;
inline constexpr float kUnitSize = kChunkSize / 8.0f;

// De detail-doodad-horizon van de originele client: een chunk bouwt zijn clutter alleen binnen
// 70 yd van de camera.
inline constexpr float kHorizonYards = 70.0f;

// frillDensity wordt door de originele client begrensd op 1..256, maar de renderer verzadigt op
// 128 (frillDensity << 6 instances, maximaal 0x2000).
inline constexpr std::uint32_t kMaxFrillDensity = 128u;

inline constexpr std::uint32_t kEmptySlot = 0xFFFFFFFFu;
inline constexpr std::size_t kVerticesPerChunk = 145u;

struct Placement {
  std::uint16_t model{0};
  std::array<float, 3> position{};
  float yaw{0.0f};
  float scale{1.0f};
};

// De gegevens van één terreinchunk die het strooien nodig heeft, los van de ADT-structuren.
struct ChunkSource {
  std::int32_t tile_x{0};
  std::int32_t tile_y{0};
  std::uint32_t index_x{0};
  std::uint32_t index_y{0};
  float base_x{0.0f};
  float base_y{0.0f};
  float base_z{0.0f};
  float min_height{0.0f};
  float max_height{0.0f};
  std::uint16_t holes{0};
  std::uint8_t layer_count{0};
  std::array<std::uint32_t, 4> layer_effect_ids{kEmptySlot, kEmptySlot, kEmptySlot, kEmptySlot};
  std::array<std::uint8_t, 64> pred_tex{};
  std::array<bool, 64> no_effect_doodad{};
  std::array<float, kVerticesPerChunk> heights{};
};

struct EffectEntry {
  // Index in Catalog::models per slot; -1 laat de plek leeg. Volgorde en dubbelen blijven
  // behouden: het slotpatroon is de weging.
  std::array<std::int32_t, 4> model_slot{-1, -1, -1, -1};
  std::uint32_t density{0};
};

struct Catalog {
  std::vector<std::string> models;
  std::unordered_map<std::uint32_t, EffectEntry> effects;

  [[nodiscard]] bool Empty() const noexcept { return effects.empty() || models.empty(); }
};

namespace detail {

inline constexpr std::array<std::uint8_t, 256> kNoise = {
    0x8e, 0x14, 0x27, 0x99, 0xfd, 0xaa, 0xc7, 0x08, 0xd5, 0xe6, 0x3e, 0x1f, 0xf6, 0xbb, 0x55, 0xda,
    0x75, 0xa0, 0x4a, 0x6a, 0xe8, 0xbd, 0x97, 0xff, 0xde, 0x9b, 0xbc, 0x9f, 0x81, 0x8a, 0xa1, 0x46,
    0x6e, 0x0b, 0xe3, 0x63, 0x76, 0x7a, 0x6c, 0x5d, 0x88, 0xd3, 0x69, 0xca, 0xc3, 0x47, 0xb9, 0x25,
    0x83, 0xab, 0xa2, 0x3f, 0xa6, 0x41, 0x7c, 0xba, 0xe5, 0xac, 0x95, 0x01, 0x7e, 0xcf, 0x09, 0xc1,
    0xd9, 0x62, 0x70, 0x71, 0x8d, 0xdb, 0x05, 0x02, 0x24, 0x87, 0xef, 0x54, 0xc6, 0xd4, 0x37, 0x30,
    0xd0, 0x1b, 0xcb, 0x7b, 0xb8, 0xe4, 0xd8, 0xec, 0x49, 0xce, 0xad, 0xdc, 0x13, 0xa9, 0x94, 0xc4,
    0x8f, 0x39, 0xae, 0x0d, 0x18, 0x52, 0xdd, 0x0e, 0x78, 0xfa, 0xf5, 0x85, 0x58, 0xd2, 0xaf, 0x6d,
    0xa4, 0xb2, 0x53, 0x3b, 0x51, 0xa5, 0x50, 0xbe, 0xfc, 0x2d, 0xf4, 0x11, 0x48, 0x98, 0x16, 0xf1,
    0x86, 0xdf, 0x3d, 0x66, 0x5e, 0x44, 0x2e, 0x2f, 0x36, 0x07, 0x6b, 0x17, 0x8b, 0x29, 0x4c, 0xb6,
    0xe2, 0x89, 0x5f, 0xe7, 0xcd, 0xa7, 0x21, 0xe1, 0x4d, 0xc9, 0x65, 0xed, 0xfe, 0xee, 0x9c, 0x23,
    0x33, 0x7d, 0xb7, 0x04, 0x9e, 0x9a, 0x2a, 0x40, 0xb3, 0x10, 0x5b, 0xf3, 0x82, 0x77, 0x1c, 0x92,
    0x20, 0x4e, 0x1e, 0x57, 0x22, 0x72, 0x06, 0x8c, 0x67, 0x2c, 0x73, 0xfb, 0x59, 0xc2, 0x0a, 0xbf,
    0x79, 0x5c, 0xf9, 0x0c, 0x28, 0x1a, 0x12, 0x68, 0x74, 0x34, 0x19, 0x42, 0xb1, 0xc0, 0x84, 0xf8,
    0x38, 0xf0, 0x15, 0x9d, 0x60, 0xf2, 0x3a, 0x6f, 0xb4, 0x90, 0xeb, 0x91, 0x1d, 0x7f, 0x35, 0x61,
    0x5a, 0x32, 0x03, 0x56, 0xa3, 0xc5, 0x2b, 0x93, 0x80, 0x0f, 0x4b, 0x43, 0xf7, 0xa8, 0xe0, 0x3c,
    0x96, 0xd1, 0x64, 0x26, 0xd7, 0x45, 0xcc, 0x4f, 0xc8, 0xb0, 0xe9, 0xb5, 0x00, 0xd6, 0x31, 0xea,
};

// De PRNG van de originele client (0x4531e0, zaad uitgebreid door 0x44ea80).
class BlizzardRandomizer {
 public:
  explicit BlizzardRandomizer(const std::uint32_t source) noexcept
      : source_(source),
        seed_(((source % 0x2Fu) << 26) | ((source % 0x35u) << 18) | ((source % 0x3Bu) << 10) |
              (4u * (source % 0x3Du))) {}

  std::uint32_t Shuffle() noexcept {
    // Een negatieve laneindex telt de eigen constante van de lane op, geen mod-256 wrap.
    const auto lane = [](const std::uint32_t value, const int sub, const int wrap) -> std::uint8_t {
      int index = static_cast<int>(value & 0xFFu) - sub;
      if (index < 0) {
        index += wrap;
      }
      return static_cast<std::uint8_t>(index);
    };
    const std::uint8_t a = lane(seed_, 0x1C, 0xF4);
    const std::uint8_t b = lane(seed_ >> 8, 0x18, 0xEC);
    const std::uint8_t c = lane(seed_ >> 16, 0x0C, 0xD4);
    const std::uint8_t d = lane(seed_ >> 24, 0x04, 0xBC);
    seed_ = static_cast<std::uint32_t>(a) | (static_cast<std::uint32_t>(b) << 8) |
            (static_cast<std::uint32_t>(c) << 16) | (static_cast<std::uint32_t>(d) << 24);
    source_ += Noise32(a) ^ std::rotl(Noise32(d), 1) ^ std::rotl(Noise32(c), 2) ^
               std::rotl(Noise32(b), 3);
    return source_;
  }

  // genCoord: een mantissa-float in [1, 2), door het tekenbit van de worp afgebeeld op (0, 1]
  // of [-1, 0).
  float SignedUnit() noexcept {
    const std::uint32_t u = Shuffle();
    const float f = std::bit_cast<float>((u & 0x007FFFFFu) | 0x3F800000u);
    return (u & 0x80000000u) != 0u ? 2.0f - f : f - 2.0f;
  }

 private:
  // Little-endian u32 op byte i van de tabel; de lanes houden i hoogstens 251.
  static std::uint32_t Noise32(const std::uint8_t i) noexcept {
    const auto at = [i](const unsigned k) -> std::uint32_t {
      return kNoise[static_cast<std::uint8_t>(i + k)];
    };
    return at(0) | (at(1) << 8) | (at(2) << 16) | (at(3) << 24);
  }

  std::uint32_t source_;
  std::uint32_t seed_;
};

// Het punt (fx, fy) in [0,1]^2 (oost, zuid) van een MCNK-cel, op de getekende 4-driehoeks
// middenwaaier; een hoekbilineaire zou clutter onder een verhoogde middenvertex laten zakken.
inline std::array<float, 3> FanPoint(const float fx, const float fy, const std::array<float, 3>& tl,
                                     const std::array<float, 3>& tr,
                                     const std::array<float, 3>& bl,
                                     const std::array<float, 3>& br,
                                     const std::array<float, 3>& ctr) noexcept {
  float u;
  float v;
  float w;
  const std::array<float, 3>* va;
  const std::array<float, 3>* vb;
  if (fx + fy <= 1.0f) {
    if (fy <= fx) {
      u = 2.0f * fy; v = 1.0f - fx - fy; w = fx - fy; va = &tl; vb = &tr;
    } else {
      u = 2.0f * fx; v = fy - fx; w = 1.0f - fx - fy; va = &bl; vb = &tl;
    }
  } else if (fy >= fx) {
    u = 2.0f * (1.0f - fy); v = fx + fy - 1.0f; w = fy - fx; va = &br; vb = &bl;
  } else {
    u = 2.0f * (1.0f - fx); v = fx - fy; w = fx + fy - 1.0f; va = &tr; vb = &br;
  }
  return {u * ctr[0] + v * (*va)[0] + w * (*vb)[0], u * ctr[1] + v * (*va)[1] + w * (*vb)[1],
          u * ctr[2] + v * (*va)[2] + w * (*vb)[2]};
}

struct DbcView {
  const std::uint8_t* records{nullptr};
  const std::uint8_t* strings{nullptr};
  std::uint32_t record_count{0};
  std::uint32_t field_count{0};
  std::uint32_t record_size{0};
  std::uint32_t string_size{0};
  bool valid{false};

  [[nodiscard]] std::uint32_t Field(const std::uint32_t row, const std::uint32_t field) const noexcept {
    std::uint32_t value = 0;
    std::memcpy(&value, records + static_cast<std::size_t>(row) * record_size + field * 4u, 4u);
    return value;
  }

  [[nodiscard]] std::string String(const std::uint32_t offset) const {
    if (offset >= string_size) {
      return {};
    }
    const char* begin = reinterpret_cast<const char*>(strings) + offset;
    const std::size_t max_len = string_size - offset;
    const void* terminator = std::memchr(begin, 0, max_len);
    const std::size_t length =
        terminator != nullptr ? static_cast<std::size_t>(static_cast<const char*>(terminator) - begin)
                              : max_len;
    return std::string(begin, length);
  }
};

inline std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, const std::size_t at) noexcept {
  std::uint32_t value = 0;
  std::memcpy(&value, bytes.data() + at, 4u);
  return value;
}

inline DbcView OpenDbc(const std::vector<std::uint8_t>& bytes) noexcept {
  DbcView view;
  if (bytes.size() < 20u || std::memcmp(bytes.data(), "WDBC", 4u) != 0) {
    return view;
  }
  view.record_count = ReadU32(bytes, 4);
  view.field_count = ReadU32(bytes, 8);
  view.record_size = ReadU32(bytes, 12);
  view.string_size = ReadU32(bytes, 16);
  const std::uint64_t record_bytes =
      static_cast<std::uint64_t>(view.record_count) * view.record_size;
  if (20u + record_bytes + view.string_size > bytes.size() ||
      static_cast<std::uint64_t>(view.field_count) * 4u > view.record_size) {
    return view;
  }
  view.records = bytes.data() + 20u;
  view.strings = view.records + record_bytes;
  view.valid = true;
  return view;
}

// "ElwGra01.mdl" -> "World\NoDXT\Detail\ElwGra01.m2": de DBC noemt een kale .mdl, het bestand is
// <stem>.m2.
inline std::string ResolveModelPath(const std::string& dbc_name) {
  const std::size_t slash = dbc_name.find_last_of("\\/");
  const std::string file = slash == std::string::npos ? dbc_name : dbc_name.substr(slash + 1);
  const std::size_t dot = file.find_last_of('.');
  const std::string stem = dot == std::string::npos ? file : file.substr(0, dot);
  return "World\\NoDXT\\Detail\\" + stem + ".m2";
}

}  // namespace detail

// Bouwt de catalogus uit de twee Classic-DBC's (GroundEffectTexture: 7 velden/28 bytes,
// GroundEffectDoodad: 3 velden/12 bytes). Een andere indeling geeft een lege catalogus.
// De doodad van een slot wordt gezocht op GroundEffectDoodad.internalId (veld 1).
inline Catalog BuildCatalog(const std::vector<std::uint8_t>& texture_dbc,
                            const std::vector<std::uint8_t>& doodad_dbc) {
  Catalog catalog;
  const detail::DbcView textures = detail::OpenDbc(texture_dbc);
  const detail::DbcView doodads = detail::OpenDbc(doodad_dbc);
  if (!textures.valid || !doodads.valid || textures.field_count != 7u || textures.record_size != 28u ||
      doodads.field_count != 3u || doodads.record_size != 12u) {
    return catalog;
  }

  std::unordered_map<std::string, std::int32_t> model_index;
  std::unordered_map<std::uint32_t, std::int32_t> by_internal_id;
  for (std::uint32_t row = 0; row < doodads.record_count; ++row) {
    const std::string path = detail::ResolveModelPath(doodads.String(doodads.Field(row, 2)));
    auto [it, inserted] = model_index.emplace(path, static_cast<std::int32_t>(catalog.models.size()));
    if (inserted) {
      catalog.models.push_back(path);
    }
    by_internal_id[doodads.Field(row, 1)] = it->second;
  }

  for (std::uint32_t row = 0; row < textures.record_count; ++row) {
    EffectEntry entry;
    bool any = false;
    for (std::uint32_t slot = 0; slot < 4u; ++slot) {
      const std::uint32_t doodad_id = textures.Field(row, 1u + slot);
      if (doodad_id == kEmptySlot) {
        continue;
      }
      const auto found = by_internal_id.find(doodad_id);
      if (found != by_internal_id.end()) {
        entry.model_slot[slot] = found->second;
        any = true;
      }
    }
    if (!any) {
      continue;  // een terreintype-rij: voetstappen en geluid, geen doodads
    }
    entry.density = textures.Field(row, 5);
    catalog.effects[textures.Field(row, 0)] = entry;
  }
  return catalog;
}

// Strooit de clutter van één chunk (CMapChunk::CreateDetailDoodads): `frill` cellen met
// herhaling, en per cel `density` doodads uit de laag die predominantTexture noemt.
inline void ScatterChunk(const ChunkSource& chunk, const Catalog& catalog, std::uint32_t frill,
                         std::vector<Placement>& out) {
  out.clear();
  frill = std::min(frill, kMaxFrillDensity);
  if (frill == 0u || catalog.Empty()) {
    return;
  }

  const auto outer = [&chunk](const int row, const int col) -> std::array<float, 3> {
    return {chunk.base_x - static_cast<float>(row) * kUnitSize,
            chunk.base_y - static_cast<float>(col) * kUnitSize,
            chunk.base_z + chunk.heights[static_cast<std::size_t>(row * 17 + col)]};
  };
  const auto inner = [&chunk](const int row, const int col) -> std::array<float, 3> {
    return {chunk.base_x - (static_cast<float>(row) + 0.5f) * kUnitSize,
            chunk.base_y - (static_cast<float>(col) + 0.5f) * kUnitSize,
            chunk.base_z + chunk.heights[static_cast<std::size_t>(row * 17 + 9 + col)]};
  };

  const std::uint32_t global_x =
      static_cast<std::uint32_t>(chunk.tile_x) * 16u + chunk.index_x;
  const std::uint32_t global_y =
      static_cast<std::uint32_t>(chunk.tile_y) * 16u + chunk.index_y;
  detail::BlizzardRandomizer rng((global_y << 16) | (global_x & 0xFFFFu));

  // Pass 1: `frill` cellen, twee worpen elk, met herhaling; nooit ontdubbelen.
  std::array<std::pair<int, int>, kMaxFrillDensity> cells{};
  for (std::uint32_t i = 0; i < frill; ++i) {
    const int col = static_cast<int>(rng.Shuffle() & 7u);
    const int row = static_cast<int>(rng.Shuffle() & 7u);
    cells[i] = {row, col};
  }

  // Pass 2: die cellen, op volgorde.
  for (std::uint32_t list_index = 0; list_index < frill; ++list_index) {
    const int row = cells[list_index].first;
    const int col = cells[list_index].second;
    const std::size_t k = static_cast<std::size_t>(row * 8 + col);
    if (chunk.no_effect_doodad[k]) {
      continue;  // het getekende clutterverbod (wegen, open plekken)
    }
    // Geen clutter boven een gat van 2x2 cellen, waar geen oppervlak getekend wordt.
    if ((chunk.holes & (1u << ((row >> 1) * 4 + (col >> 1)))) != 0u) {
      continue;
    }
    const std::size_t layer = chunk.pred_tex[k];
    if (layer >= chunk.layer_count || layer >= chunk.layer_effect_ids.size()) {
      continue;
    }
    const auto effect = catalog.effects.find(chunk.layer_effect_ids[layer]);
    if (effect == catalog.effects.end()) {
      continue;  // geen effect, of geen van de slots lost op: de cel blijft kaal
    }
    const EffectEntry& entry = effect->second;
    const std::uint32_t density = entry.density == 0u ? 8u : entry.density;

    const std::array<float, 3> tl = outer(row, col);
    const std::array<float, 3> tr = outer(row, col + 1);
    const std::array<float, 3> bl = outer(row + 1, col);
    const std::array<float, 3> br = outer(row + 1, col + 1);
    const std::array<float, 3> ctr = inner(row, col);

    for (std::uint32_t n = 0; n < density; ++n) {
      // De worpvolgorde van de originele client: rx en ry altijd, schaal en yaw alleen voor een
      // geplaatst model.
      const float rx = rng.SignedUnit();
      const float ry = rng.SignedUnit();
      const std::int32_t model = entry.model_slot[(n + list_index) & 3u];
      if (model < 0) {
        continue;
      }
      const float scale = rng.SignedUnit() * 0.1f + 1.0f;
      const float yaw = rng.SignedUnit() * 3.14159265358979323846f;
      Placement placement;
      placement.model = static_cast<std::uint16_t>(model);
      placement.position = detail::FanPoint((rx + 1.0f) * 0.5f, (ry + 1.0f) * 0.5f, tl, tr, bl, br, ctr);
      placement.yaw = yaw;
      placement.scale = scale;
      out.push_back(placement);
    }
  }
}

}  // namespace openwow::render::clutter