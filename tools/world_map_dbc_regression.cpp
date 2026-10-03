// Standalone offline check: link dbc_file.cpp and dbc_entries_extended.cpp.
// Uses the actual decoder and the same predicate as WorldMapSystem, no GUI/VFS.
#include "openwow/data/formats/dbc/dbc_file.h"
#include "openwow/game/world_map_continent_filter.h"
#include "openwow/game/world_map_continent_rect.h"
#include "openwow/game/contested_area.h"
#include <cmath>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

using namespace openwow::data::dbc;
void Require(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
void Word(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}
void Row(std::vector<std::uint8_t>& bytes, std::uint32_t id,
         std::uint32_t map, std::uint32_t area, std::uint32_t name, bool rect) {
  for (auto v : {id, map, area, name}) Word(bytes, v);
  for (float v : {rect ? 100.f : 0.f, rect ? -100.f : 0.f,
                  rect ? 200.f : 0.f, rect ? -200.f : 0.f})
    Word(bytes, std::bit_cast<std::uint32_t>(v));
}
int main(int argc, char** argv) {
  try {
    // 46 AreaID-zero rows, as in the Turtle snapshot: 2 real overviews,
    // 43 empty instance placeholders, and one named zero-rect World sheet.
    std::vector<std::uint8_t> bytes;
    for (auto v : {DbcHeader::kWdbcSignature, 47u, 8u, 32u, 11u}) Word(bytes, v);
    Row(bytes, 13, 1, 0, 1, true);
    Row(bytes, 14, 0, 0, 1, true);
    for (std::uint32_t i = 0; i < 43; ++i) Row(bytes, 600+i, 800+i, 0, 0, false);
    Row(bytes, 694, 0, 0, 5, false);
    Row(bytes, 4, 1, 14, 1, true); // zone: row ID != AreaTable ID
    for (char c : std::string_view("\0Map\0World\0", 11)) bytes.push_back(c);
    DbcFile file;
    Require(file.LoadFromBytes(std::move(bytes)) == DbcError::kOk, "fixture load");
    std::vector<std::uint32_t> selected;
    for (std::uint32_t i = 0; i < file.record_count(); ++i) {
      const auto row = WorldMapAreaEntry::Load(file, i);
      Require(row.id == file.GetUInt32(i, 0), "row ID overwritten by AreaID");
      Require(row.display_map_id == -1 && row.default_dungeon_map_id == -1,
              "Classic missing override must not become map zero");
      if (openwow::game::IsWorldMapContinentOverview(row)) selected.push_back(row.id);
    }
    Require(selected == std::vector<std::uint32_t>({13,14}), "continent count/order");
    auto custom = WorldMapAreaEntry::Load(file, 0);
    custom.id = 9001; custom.map_id = 987; custom.name = "CustomContinent";
    Require(openwow::game::IsWorldMapContinentOverview(custom), "custom IDs rejected");
    custom.loc_right = custom.loc_left;
    Require(!openwow::game::IsWorldMapContinentOverview(custom), "degenerate rect accepted");
    // Real Vanilla tile bounds: world-map click rectangles must be disjoint.
    const auto kal = openwow::game::ProjectWorldMapContinentRect(
        23, 48, 9, 52, -19.f, -0.322498f, 0.75f);
    const auto ek = openwow::game::ProjectWorldMapContinentRect(
        23, 47, 15, 61, 14.5f, -7.f, 0.75f);
    Require(kal.left < 0.1f && kal.right > 0.39f && kal.right < ek.left,
            "world continent projection overlap/offset");
    Require(ek.left > 0.62f && ek.right > 0.92f, "EK world hitbox");
    Require(kal.top < 0.5f && kal.bottom > 0.5f, "Kalimdor centre click");

    // Actual Classic AreaPOI decoder; unrelated neighboring columns are nonzero.
    std::vector<std::uint8_t> poi_bytes;
    for (auto v : {DbcHeader::kWdbcSignature, 1u, 29u, 116u, 11u}) Word(poi_bytes, v);
    std::vector<std::uint32_t> fields(29, 0);
    fields[0]=9001; fields[1]=4; fields[2]=7; fields[3]=84;
    fields[4]=std::bit_cast<std::uint32_t>(-1234.f);
    fields[5]=std::bit_cast<std::uint32_t>(5678.f);
    fields[6]=std::bit_cast<std::uint32_t>(12.f);
    fields[7]=0; fields[8]=0x18; fields[9]=0xffffffffu;
    fields[10]=1; fields[18]=0xffffffffu; fields[19]=5;
    fields[27]=0xffffffffu; fields[28]=0;
    for (auto v : fields) Word(poi_bytes,v);
    for (char c : std::string_view("\0Map\0World\0",11)) poi_bytes.push_back(c);
    DbcFile poi_file;
    Require(poi_file.LoadFromBytes(poi_bytes)==DbcError::kOk,"POI fixture load");
    const auto poi=AreaPOIEntry::Load(poi_file,0);
    Require(poi.id==9001 && poi.importance==4 && poi.texture_indices[0]==7,
            "POI ID/importance/single icon");
    for (std::size_t i=1;i<poi.texture_indices.size();++i)
      Require(poi.texture_indices[i]==0,"Classic must not read nine icons");
    Require(poi.faction_id==84 && poi.x==-1234.f && poi.y==5678.f && poi.z==12.f,
            "POI faction and position offsets");
    Require(poi.map_id==0 && poi.flags==0x18 && poi.area_id==0xffffffffu,
            "POI EK map zero/continent-wide sentinel");
    Require(poi.name=="Map" && poi.description=="World" && poi.world_state_id==0 &&
            poi.map_link_id==0,"POI strings/state/absent map link");
    DbcFile unsupported_locale(DbcLocale::kRuRu);
    Require(unsupported_locale.LoadFromBytes(std::move(poi_bytes))==DbcError::kOk,
            "locale fixture load");
    Require(AreaPOIEntry::Load(unsupported_locale,0).name.empty(),
            "Classic flags column must not be treated as ninth locale");

    using openwow::game::ResolveClassicZonePvpType;
    using openwow::game::ZonePvPType;
    Require(ResolveClassicZonePvpType(2,2,4)==ZonePvPType::Friendly,"Alliance territory");
    Require(ResolveClassicZonePvpType(4,2,4)==ZonePvPType::Hostile,"Horde territory");
    Require(ResolveClassicZonePvpType(0,2,4)==ZonePvPType::Contested,"unowned territory");
    Require(ResolveClassicZonePvpType(2,2,2)==ZonePvPType::Friendly,"friend precedence");

    // Optional read-only check of the existing Turtle extract (not active MPQ proof).
    if (argc == 2) {
      std::ifstream input(argv[1], std::ios::binary);
      Require(input.is_open(), "extract open");
      std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), {});
      DbcFile local;
      Require(local.LoadFromBytes(std::move(data)) == DbcError::kOk, "extract load");
      selected.clear();
      for (std::uint32_t i = 0; i < local.record_count(); ++i) {
        const auto row = WorldMapAreaEntry::Load(local, i);
        Require(row.id == local.GetUInt32(i, 0), "extract row identity");
        if (openwow::game::IsWorldMapContinentOverview(row)) selected.push_back(row.id);
      }
      Require(selected == std::vector<std::uint32_t>({13,14}), "Turtle extract continent list");
    }
    std::cout << "world map DBC regression passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
