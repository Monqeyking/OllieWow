#include "openwow/data/formats/dbc/dbc_loader.h"

#include "openwow/core/storm_error.h"
#include "openwow/data/loading/dbc_locale_adapter.h"
#include "openwow/foundation/diagnostics/logging.h"
#include "openwow/vfs/virtual_file_system.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace openwow::data::dbc {

namespace {

constexpr std::uint32_t kDbcVersionMismatchError = 0x85100079u;

#include "dbc_retail_catalog.inc"

bool ReadHeaderWord(const std::vector<std::uint8_t> &bytes, std::size_t *const cursor,
                    std::uint32_t *const out) {
  if (*cursor > bytes.size() || bytes.size() - *cursor < sizeof(*out)) {
    return false;
  }

  const auto *const value = bytes.data() + *cursor;
  *out = static_cast<std::uint32_t>(value[0]) |
         (static_cast<std::uint32_t>(value[1]) << 8u) |
         (static_cast<std::uint32_t>(value[2]) << 16u) |
         (static_cast<std::uint32_t>(value[3]) << 24u);
  *cursor += sizeof(*out);
  return true;
}

std::string BuildVfsPath(const std::string &root, const RetailDbcDescriptor &descriptor) {
  std::string path = root;
  if (!path.empty() && path.back() != '/' && path.back() != '\\') {
    path.push_back('/');
  }
  path.append(descriptor.filename());
  return path;
}

bool IsClassicSpellbookDbc(const std::string_view filename) {
  static constexpr std::array<std::string_view, 3> kSpellbookTables = {
      "DBFilesClient\\SkillLine.dbc",
      "DBFilesClient\\SkillLineAbility.dbc",
      "DBFilesClient\\SkillRaceClassInfo.dbc",
  };
  return std::find(kSpellbookTables.begin(), kSpellbookTables.end(), filename) !=
         kSpellbookTables.end();
}

bool IsClassicMvpDbc(const std::string_view filename) {
  // Keep this list tied to the current Classic world-entry MVP. The full
  // catalog remains available for later Classic feature work, but loading it
  // eagerly retains a large amount of data that the MVP never reads.
  static constexpr std::array<std::string_view, 78> kClassicMvpTables = {
      "DBFilesClient\\AreaTable.dbc",
      "DBFilesClient\\AnimationData.dbc",
      "DBFilesClient\\CharBaseInfo.dbc",
      "DBFilesClient\\CharHairGeosets.dbc",
      "DBFilesClient\\CharSections.dbc",
      "DBFilesClient\\CharStartOutfit.dbc",
      "DBFilesClient\\CharacterFacialHairStyles.dbc",
      "DBFilesClient\\ChrClasses.dbc",
      "DBFilesClient\\ChrRaces.dbc",
      "DBFilesClient\\CreatureDisplayInfo.dbc",
      "DBFilesClient\\CreatureFamily.dbc",
      "DBFilesClient\\CreatureModelData.dbc",
      "DBFilesClient\\CreatureMovementInfo.dbc",
      "DBFilesClient\\CreatureSoundData.dbc",
      "DBFilesClient\\CreatureType.dbc",
      "DBFilesClient\\DanceMoves.dbc",
      "DBFilesClient\\Emotes.dbc",
      "DBFilesClient\\EmotesText.dbc",
      "DBFilesClient\\EmotesTextData.dbc",
      "DBFilesClient\\EmotesTextSound.dbc",
      "DBFilesClient\\Exhaustion.dbc",
      "DBFilesClient\\FactionGroup.dbc",
      "DBFilesClient\\FactionTemplate.dbc",
      "DBFilesClient\\GameObjectArtKit.dbc",
      "DBFilesClient\\GameObjectDisplayInfo.dbc",
      "DBFilesClient\\GameTables.dbc",
      "DBFilesClient\\GameTips.dbc",
      "DBFilesClient\\GroundEffectDoodad.dbc",
      "DBFilesClient\\GroundEffectTexture.dbc",
      "DBFilesClient\\HelmetGeosetVisData.dbc",
      "DBFilesClient\\ItemClass.dbc",
      "DBFilesClient\\ItemDisplayInfo.dbc",
      "DBFilesClient\\ItemSubClass.dbc",
      "DBFilesClient\\Light.dbc",
      "DBFilesClient\\LightFloatBand.dbc",
      "DBFilesClient\\LightIntBand.dbc",
      "DBFilesClient\\LightParams.dbc",
      "DBFilesClient\\LightSkybox.dbc",
      "DBFilesClient\\LiquidMaterial.dbc",
      "DBFilesClient\\LiquidType.dbc",
      "DBFilesClient\\LoadingScreens.dbc",
      "DBFilesClient\\Map.dbc",
      "DBFilesClient\\Material.dbc",
      "DBFilesClient\\NPCSounds.dbc",
      "DBFilesClient\\NameGen.dbc",
      "DBFilesClient\\ObjectEffect.dbc",
      "DBFilesClient\\ObjectEffectGroup.dbc",
      "DBFilesClient\\ObjectEffectModifier.dbc",
      "DBFilesClient\\ObjectEffectPackage.dbc",
      "DBFilesClient\\ObjectEffectPackageElem.dbc",
      "DBFilesClient\\PaperDollItemFrame.dbc",
      "DBFilesClient\\Resistances.dbc",
      "DBFilesClient\\SkillLine.dbc",
      "DBFilesClient\\SkillLineAbility.dbc",
      "DBFilesClient\\SkillRaceClassInfo.dbc",
      "DBFilesClient\\SoundAmbience.dbc",
      "DBFilesClient\\SoundEntries.dbc",
      "DBFilesClient\\SoundEntriesAdvanced.dbc",
      "DBFilesClient\\Spell.dbc",
      "DBFilesClient\\SpellIcon.dbc",
      "DBFilesClient\\SpellVisual.dbc",
      "DBFilesClient\\SpellVisualEffectName.dbc",
      "DBFilesClient\\SpellVisualKit.dbc",
      "DBFilesClient\\TerrainType.dbc",
      "DBFilesClient\\TerrainTypeSounds.dbc",
      "DBFilesClient\\TransportAnimation.dbc",
      "DBFilesClient\\TransportPhysics.dbc",
      "DBFilesClient\\TransportRotation.dbc",
      "DBFilesClient\\WorldChunkSounds.dbc",
      "DBFilesClient\\WorldMapArea.dbc",
      "DBFilesClient\\WorldMapContinent.dbc",
      "DBFilesClient\\WorldMapOverlay.dbc",
      "DBFilesClient\\WorldMapTransforms.dbc",
      "DBFilesClient\\WorldStateUI.dbc",
      "DBFilesClient\\WorldStateZoneSounds.dbc",
      "DBFilesClient\\WMOAreaTable.dbc",
      "DBFilesClient\\ZoneIntroMusicTable.dbc",
      "DBFilesClient\\ZoneMusic.dbc",
  };
  return std::find(kClassicMvpTables.begin(), kClassicMvpTables.end(), filename) !=
         kClassicMvpTables.end();
}

template <typename T>
bool LoadRetailStrictDbcStore(DbcStore<T> &store, const openwow::vfs::VirtualFileSystem &vfs,
                              const std::string &path,
                              const DbcTableSchema<T> &schema) {
  if (!store.empty()) {
    return true;
  }

  const char *const retail_path = schema.retail_path.data();
  auto bytes = vfs.ReadFileBytes(path);
  if (!bytes.has_value()) {
    openwow::core::SErrFatalError_VArgs(kDbcVersionMismatchError, "Unable to open %s",
                                        retail_path);
  }

  std::size_t cursor = 0;
  std::uint32_t signature = 0;
  if (!ReadHeaderWord(*bytes, &cursor, &signature)) {
    openwow::core::SErrFatalError_VArgs(kDbcVersionMismatchError,
                                        "Unable to read signature from %s", retail_path);
  }
  if (signature != DbcHeader::kWdbcSignature) {
    openwow::core::SErrFatalError_VArgs(kDbcVersionMismatchError,
                                        "Invalid signature 0x%x from %s", signature,
                                        retail_path);
  }

  std::uint32_t record_count = 0;
  if (!ReadHeaderWord(*bytes, &cursor, &record_count)) {
    openwow::core::SErrFatalError_VArgs(kDbcVersionMismatchError,
                                        "Unable to read record count from %s", retail_path);
  }

  if (record_count == 0u) {
    return true;
  }

  std::uint32_t field_count = 0;
  if (!ReadHeaderWord(*bytes, &cursor, &field_count)) {
    openwow::core::SErrFatalError_VArgs(kDbcVersionMismatchError,
                                        "Unable to read column count from %s", retail_path);
  }
  if (field_count != schema.field_count) {
    openwow::core::SErrFatalError_VArgs(
        kDbcVersionMismatchError,
        "%s has wrong number of columns (found %i, expected %i)", retail_path,
        static_cast<int>(field_count), static_cast<int>(schema.field_count));
  }

  std::uint32_t record_size = 0;
  if (!ReadHeaderWord(*bytes, &cursor, &record_size)) {
    openwow::core::SErrFatalError_VArgs(kDbcVersionMismatchError,
                                        "Unable to read row size from %s", retail_path);
  }
  if (record_size != schema.record_size) {
    openwow::core::SErrFatalError_VArgs(
        kDbcVersionMismatchError, "%s has wrong row size (found %i, expected %i)", retail_path,
        static_cast<int>(record_size), static_cast<int>(schema.record_size));
  }

  std::uint32_t string_block_size = 0;
  if (!ReadHeaderWord(*bytes, &cursor, &string_block_size)) {
    openwow::core::SErrFatalError_VArgs(kDbcVersionMismatchError,
                                        "Unable to read string size from %s", retail_path);
  }

  const auto expected_size =
      static_cast<std::uint64_t>(DbcHeader::kEncodedSize) +
      static_cast<std::uint64_t>(record_count) * record_size +
      string_block_size;
  if (expected_size > static_cast<std::uint64_t>(bytes->size())) {
    openwow::core::SErrFatalCondition("%s: Cannot read string table", retail_path);
  }

  DbcFile file{openwow::data::loading::CurrentDbcLocale()};
  if (file.LoadFromBytes(std::move(*bytes)) != DbcError::kOk ||
      !store.LoadFromFile(std::move(file), schema, path)) {
    openwow::core::SErrFatalCondition("%s: Cannot read string table", retail_path);
  }

  return true;
}

// The Classic/Turtle client does not ship the complete WotLK DBC catalog.
// Probe optional tables before entering the strict loader so an absent table
// or a known Classic layout difference does not abort client startup.
template <typename T>
bool LoadRetailOptionalDbcStore(DbcStore<T> &store,
                                const openwow::vfs::VirtualFileSystem &vfs,
                                const std::string &path,
                                const DbcTableSchema<T> &schema,
                                const bool require_declared_schema) {
  if (!store.empty()) {
    return true;
  }

  auto bytes = vfs.ReadFileBytes(path);
  if (!bytes.has_value()) {
    return false;
  }

  DbcFile probe{openwow::data::loading::CurrentDbcLocale()};
  if (probe.LoadFromBytes(*bytes) != DbcError::kOk) {
    return false;
  }

  if (require_declared_schema &&
      (probe.field_count() != schema.field_count ||
       probe.record_size() != schema.record_size)) {
    using openwow::diagnostics::Log;
    using openwow::diagnostics::LogLevel;
    Log(LogLevel::kWarn,
        "DBC: Skipping " + std::string(schema.retail_path) +
            " because its Classic header is " + std::to_string(probe.field_count()) +
            " fields/" + std::to_string(probe.record_size()) +
            " bytes; expected " + std::to_string(schema.field_count) +
            " fields/" + std::to_string(schema.record_size) + " bytes.");
    return false;
  }

  // The filename identifies the logical table; the actual Classic/Turtle
  // header identifies its layout. This lets the same loader accept tables
  // whose WotLK-era column count changed while retaining strict bounds
  // validation for the layout that is actually being decoded.
  DbcTableSchema<T> actual_schema = schema;
  actual_schema.field_count = probe.field_count();
  actual_schema.record_size = probe.record_size();
  return LoadRetailStrictDbcStore(store, vfs, path, actual_schema);
}

}

template <typename T>
bool DbcLoader::LoadOne(DbcStore<T> &store, const openwow::vfs::VirtualFileSystem &vfs,
                        const std::string &path, const RetailDbcDescriptor &descriptor) {
  const DbcTableSchema<T> schema{
      .retail_path = descriptor.retail_path,
      .field_count = descriptor.field_count,
      .record_size = descriptor.record_size,
      .decode = &T::Load,
  };
  return LoadRetailStrictDbcStore(store, vfs, path, schema);
}

template <typename T>
bool DbcLoader::LoadOneOptional(DbcStore<T> &store,
                                const openwow::vfs::VirtualFileSystem &vfs,
                                const std::string &path,
                                const RetailDbcDescriptor &descriptor,
                                const bool require_declared_schema) {
  const DbcTableSchema<T> schema{
      .retail_path = descriptor.retail_path,
      .field_count = descriptor.field_count,
      .record_size = descriptor.record_size,
      .decode = &T::Load,
  };
  return LoadRetailOptionalDbcStore(store, vfs, path, schema, require_declared_schema);
}

int DbcLoader::LoadAll(const openwow::vfs::VirtualFileSystem &vfs,
                       const std::string &dbc_root_path,
                       const DbcLoadProfile profile) {
  using openwow::diagnostics::Log;
  using openwow::diagnostics::LogLevel;

  vfs_ = &vfs;
  int loaded = 0;

  const char *const profile_name = profile == DbcLoadProfile::kClassicMvp ?
                                       "Classic MVP" : "full";
  Log(LogLevel::kInfo, "DBC: Loading " + std::string(profile_name) +
                       " client data files from '" + dbc_root_path + "' ...");

  const auto load = [&](auto &store, const RetailDbcDescriptor &descriptor) {
    if (profile == DbcLoadProfile::kClassicMvp && !IsClassicMvpDbc(descriptor.retail_path)) {
      return;
    }
    if (LoadOneOptional(store, vfs, BuildVfsPath(dbc_root_path, descriptor), descriptor,
                        IsClassicSpellbookDbc(descriptor.retail_path))) {
      ++loaded;
    }
  };

#define OPENWOW_LOAD_DBC(type, member, path, fields, size)                                          \
  load(static_cast<DbcStore<type> &>(member),                                  \
       RetailDbcDescriptor{path, fields, size});
  OPENWOW_RETAIL_DBC_CATALOG(OPENWOW_LOAD_DBC)
#undef OPENWOW_LOAD_DBC

  Log(LogLevel::kInfo,
      "DBC: Finished. " + std::to_string(loaded) + " loaded, 0 failed.");
  return loaded;
}

bool DbcLoader::LoadAreaTableRetailStrict(const openwow::vfs::VirtualFileSystem &vfs,
                                          const std::string &path) {
  vfs_ = &vfs;
  return LoadOne(core_.area_table_, vfs, path,
                 FindRetailDbcDescriptor("AreaTable.dbc"));
}

#undef OPENWOW_RETAIL_DBC_CATALOG

}
