#include "openwow/data/formats/dbc/dbc_structures.h"
#include "dbc_schema_decode.h"

namespace openwow::data::dbc {

OPENWOW_DBC_SCHEMA(AreaTableEntry,
    e.id = f.GetUInt32(row, 0);
    e.map_id = f.GetUInt32(row, 1);
    e.parent_area = f.GetUInt32(row, 2);
    e.area_bit = f.GetUInt32(row, 3);
    e.flags = f.GetUInt32(row, 4);
    e.sound_provider_pref = f.GetUInt32(row, 5);
    e.sound_provider_pref_uw = f.GetUInt32(row, 6);
    e.sound_ambience_id = f.GetUInt32(row, 7);
    e.zone_music_id = f.GetUInt32(row, 8);
    e.zone_intro_music_id = f.GetUInt32(row, 9);
    e.exploration_level = f.GetUInt32(row, 10);
    e.name = f.GetLocalizedString(row, 11);
    e.faction_group_mask = f.GetUInt32(row, 20);
)

CharStartOutfitEntry CharStartOutfitEntry::Load(const DbcFile &f, std::uint32_t row) {

  constexpr std::uint32_t kIdOffset = 0u;
  constexpr std::uint32_t kRaceOffset = 4u;
  constexpr std::uint32_t kClassOffset = 5u;
  constexpr std::uint32_t kGenderOffset = 6u;
  constexpr std::uint32_t kOutfitIdOffset = 7u;
  constexpr std::uint32_t kItemIdsOffset = 8u;
  constexpr std::uint32_t kDisplayIdsOffset =
      kItemIdsOffset + kItemSlotCount * DbcHeader::kWordSize;
  constexpr std::uint32_t kInventoryTypesOffset =
      kDisplayIdsOffset + kItemSlotCount * DbcHeader::kWordSize;
  constexpr std::uint32_t kItemSlotStride = DbcHeader::kWordSize;

  CharStartOutfitEntry e{};
  e.id = f.GetUInt32AtOffset(row, kIdOffset);
  e.race = f.GetByte(row, kRaceOffset);
  e.class_id = f.GetByte(row, kClassOffset);
  e.gender = f.GetByte(row, kGenderOffset);
  e.outfit_id = f.GetByte(row, kOutfitIdOffset);

  for (std::size_t i = 0; i < kItemSlotCount; ++i) {
    const auto offset =
        static_cast<std::uint32_t>(i) * kItemSlotStride;
    e.item_id[i] = f.GetInt32AtOffset(row, kItemIdsOffset + offset);
    e.display_id[i] = f.GetInt32AtOffset(row, kDisplayIdsOffset + offset);
    e.inv_type[i] = f.GetInt32AtOffset(row, kInventoryTypesOffset + offset);
  }
  return e;
}

OPENWOW_DBC_SCHEMA(ChrClassesEntry,
  DBC_U32(id, 0)
  DBC_U32(power_type, 3)
  DBC_LOCALIZED(name, 5)
  e.name_female = e.name;
  e.name_male = e.name;
  DBC_STRING(client_file_string, 14)
  DBC_U32(spell_family, 15)
)

std::string_view ChrClassesEntry::DisplayNameForSex(const std::uint32_t sex_id) const {
  if (sex_id == 0) {
    if (!name_male.empty()) {
      return name_male;
    }
    if (!name_female.empty()) {
      return name_female;
    }
    return name;
  }

  if (sex_id == 1) {
    if (!name_female.empty()) {
      return name_female;
    }
    if (!name_male.empty()) {
      return name_male;
    }
    return name;
  }

  return name;
}

std::uint32_t ChrClassesEntry::ResolveDisplaySex(const std::uint32_t sex_id) const {
  if (sex_id == 0) {
    if (!name_male.empty()) {
      return 0;
    }
    if (!name_female.empty()) {
      return 1;
    }
  } else if (sex_id == 1) {
    if (!name_female.empty()) {
      return 1;
    }
    if (!name_male.empty()) {
      return 0;
    }
  }

  return sex_id;
}

OPENWOW_DBC_SCHEMA(ChrRacesEntry,
  DBC_U32(id, 0)
  DBC_U32(flags, 1)
  DBC_U32(faction_id, 2)
  DBC_U32(exploration_sound_id, 3)
  DBC_U32(model_male, 4)
  DBC_U32(model_female, 5)
  // Classic/Turtle ChrRaces.dbc keeps field 6 unused. Reading it as a string
  // turns a numeric value (for example 2) into the glue scene token "2" and
  // makes CharacterSelect request UI_2/UI_2. The client file string used by
  // the glue flow is field 15; mirror it for existing model-path consumers.
  DBC_U32(default_language_id, 8)
  DBC_U32(creature_type, 9)
  DBC_U32(resurrection_sickness_spell_id, 12)
  DBC_U32(splash_sound_id, 13)
  DBC_STRING(client_file_string, 15)
  e.model_client_prefix = e.client_file_string;
  DBC_STRING(facial_hair_male, 26)
  DBC_STRING(facial_hair_female, 27)
  DBC_U32(cinematic_sequence_id, 16)
  DBC_LOCALIZED(name, 17)
  e.name_female = e.name;
  e.name_male = e.name;
  DBC_STRING(hair_customization, 28)
)

std::string_view ChrRacesEntry::DisplayNameForSex(const std::uint32_t sex_id) const {
  if (sex_id == 0) {
    if (!name_male.empty()) {
      return name_male;
    }
    if (!name_female.empty()) {
      return name_female;
    }
    return name;
  }

  if (sex_id == 1) {
    if (!name_female.empty()) {
      return name_female;
    }
    if (!name_male.empty()) {
      return name_male;
    }
    return name;
  }

  return name;
}

std::uint32_t ChrRacesEntry::ResolveDisplaySex(const std::uint32_t sex_id) const {
  if (sex_id == 0) {
    if (!name_male.empty()) {
      return 0;
    }
    if (!name_female.empty()) {
      return 1;
    }
  } else if (sex_id == 1) {
    if (!name_female.empty()) {
      return 1;
    }
    if (!name_male.empty()) {
      return 0;
    }
  }

  return sex_id;
}

OPENWOW_DBC_SCHEMA(CinematicSequencesEntry,
    e.id = f.GetUInt32(row, 0);
    e.sound_id = f.GetUInt32(row, 1);
    for (std::size_t index = 0; index < e.camera_ids.size(); ++index) {
      e.camera_ids[index] = f.GetUInt32(row, static_cast<std::uint32_t>(index + 2));
    }
)

OPENWOW_DBC_SCHEMA(CinematicCameraEntry,
  DBC_U32(id, 0)
  DBC_STRING(model, 1)
  DBC_U32(sound_id, 2)
  DBC_F32(origin_x, 3)
  DBC_F32(origin_y, 4)
  DBC_F32(origin_z, 5)
  DBC_F32(origin_facing, 6)
)

OPENWOW_DBC_SCHEMA(CreatureDisplayInfoEntry,
    e.id = f.GetUInt32(row, 0);
    e.model_id = f.GetUInt32(row, 1);
    e.extra_info = f.GetUInt32(row, 3);
    e.scale = f.GetFloat(row, 4);
    e.model_alpha = f.GetUInt32(row, 5);
    for (std::size_t index = 0; index < e.texture_variation.size(); ++index) {
      e.texture_variation[index] = f.GetString(row, 6u + static_cast<std::uint32_t>(index));
    }
)

OPENWOW_DBC_SCHEMA(CreatureModelDataEntry,
    e.id = f.GetUInt32(row, 0);
    e.flags = f.GetUInt32(row, 1);
    e.model_name = f.GetString(row, 2);
    e.size_class = f.GetInt32(row, 3);
    e.scale = f.GetFloat(row, 4);
    e.collision_height = f.GetFloat(row, 15);
)

OPENWOW_DBC_SCHEMA(GameObjectDisplayInfoEntry,
  DBC_U32(id, 0)
  DBC_STRING(filename, 1)
)

OPENWOW_DBC_SCHEMA(ItemDisplayInfoEntry,
  DBC_U32(id, 0)
  DBC_STRING(model_name_left, 1)
  DBC_STRING(model_name_right, 2)
  DBC_STRING(texture_name_left, 3)
  DBC_STRING(texture_name_right, 4)
  DBC_STRING(inventory_icon, 5)
  DBC_U32(geoset_control_1, 6)
  DBC_U32(geoset_control_2, 7)
  DBC_U32(geoset_control_3, 8)
  DBC_U32(flags, 9)
  DBC_U32(spell_visual_id, 10)
  DBC_U32(group_sound_index, 11)
  DBC_U32(helmet_geoset_vis_male, 12)
  DBC_U32(helmet_geoset_vis_female, 13)
  DBC_STRING_ARRAY(component_texture_name, 14)
  DBC_U32(item_visuals_id, 22)
)

OPENWOW_DBC_SCHEMA(LightEntry,
    e.id = f.GetUInt32(row, 0);
    e.map_id = f.GetUInt32(row, 1);
    e.x = f.GetFloat(row, 2);
    e.y = f.GetFloat(row, 3);
    e.z = f.GetFloat(row, 4);
    e.falloff_start = f.GetFloat(row, 5);
    e.falloff_end = f.GetFloat(row, 6);
    DBC_U32_ARRAY(light_params, 7)
    NormalizeLightEntryRetailCoordinates(e);
)

void NormalizeLightEntryRetailCoordinates(LightEntry &entry,
                                          const bool normalize_coordinates,
                                          const bool special_zero_origin_mode) {
  if (!normalize_coordinates ||
      (entry.x == 0.0f && entry.y == 0.0f && entry.z == 0.0f)) {
    return;
  }

  constexpr float kDatabaseUnitsToWorld = 1.0f / 36.0f;
  constexpr float kWorldMapOrigin = 17066.666f;
  const float origin = special_zero_origin_mode ? 0.0f : kWorldMapOrigin;
  const float raw_x = entry.x;
  const float raw_y = entry.y;
  const float raw_z = entry.z;

  entry.x = origin - raw_z * kDatabaseUnitsToWorld;
  entry.y = origin - raw_x * kDatabaseUnitsToWorld;
  entry.z = raw_y * kDatabaseUnitsToWorld;
  entry.falloff_start *= kDatabaseUnitsToWorld;
  entry.falloff_end *= kDatabaseUnitsToWorld;
}

OPENWOW_DBC_SCHEMA(LightIntBandEntry,
  DBC_U32(id, 0)
  DBC_U32(num_entries, 1)
  DBC_U32_ARRAY(times, 2)
  DBC_U32_ARRAY(values, 18)
)

OPENWOW_DBC_SCHEMA(LightFloatBandEntry,
  DBC_U32(id, 0)
  DBC_U32(num_entries, 1)
  DBC_U32_ARRAY(times, 2)
  DBC_F32_ARRAY(values, 18)
)

OPENWOW_DBC_SCHEMA(LoadingScreensEntry,
  DBC_U32(id, 0)
  DBC_STRING(name, 1)
  DBC_STRING(path, 2)
  DBC_U32(has_wide_screen, 3)
)

OPENWOW_DBC_SCHEMA(MapEntry,
  DBC_U32(id, 0)
  DBC_STRING(internal_name, 1)
  DBC_U32(map_type, 2)
  DBC_U32(flags, 3)
  DBC_LOCALIZED(name, 4)
  DBC_U32(linked_zone, 19)
  DBC_LOCALIZED(description_horde, 20)
  DBC_LOCALIZED(description_alliance, 29)
  DBC_U32(loading_screen_id, 38)
)

OPENWOW_DBC_SCHEMA(SoundEntriesEntry,
  DBC_U32(id, 0)
  DBC_U32(sound_type, 1)
  DBC_STRING(name, 2)
  DBC_STRING_ARRAY(file, 3)
  DBC_U32_ARRAY(freq, 13)
  DBC_STRING(directory_base, 23)
  DBC_F32(volume, 24)
  DBC_U32(flags, 25)
  DBC_F32(min_distance, 26)
  DBC_F32(distance_cutoff, 27)
  DBC_F32(eax_definition, 28)
  DBC_U32(sound_entries_advanced_id, 29)
)

OPENWOW_DBC_SCHEMA(SpellIconEntry,
  DBC_U32(id, 0)
  DBC_STRING(icon_path, 1)
)

OPENWOW_DBC_SCHEMA(SpellVisualEntry,
  // Classic/Turtle layout: 16 u32 columns, no state_done_kit and none of the
  // later WotLK missile/area columns. Verified against DBFilesClient telle quelle
  // (row 173 = 173,124,72,283,0,0,1,157,0,1,3087,0,0,0,0,0) and Benilla's
  // VisualStages mapping (gate=6, model=7, dest_attach=9, flight_sound=10,
  // strike_sound=14). Unmapped members stay 0 via value-initialization.
  DBC_U32(id, 0)
  DBC_U32(precast_kit, 1)
  DBC_U32(cast_kit, 2)
  DBC_U32(impact_kit, 3)
  DBC_U32(state_kit, 4)
  DBC_U32(channel_kit, 5)
  DBC_U32(has_missile, 6)
  DBC_I32(missile_model, 7)
  DBC_U32(missile_path_type, 8)
  DBC_I32(missile_destination_attachment, 9)
  DBC_U32(missile_sound_id, 10)
  DBC_U32(anim_event_sound_id, 14)
)

OPENWOW_DBC_SCHEMA(SpellVisualKitEntry,
  // Classic/Turtle layout: 35 columns; nine emitter slots (3-11), world
  // effect (12), sound (13), then four CharProc slots transposed over five
  // parallel arrays (types 15-18, params 19-22/23-26/27-30/31-34). Verified
  // against raw rows (kit 72: anim=53 @2, hands=293 @6/7, sound=2561 @13)
  // and Benilla's VisualKit mapping. No special2/3, shake, or flags columns
  // exist in this table version; those members stay 0.
  DBC_U32(id, 0)
  DBC_I32(start_anim_id, 1)
  DBC_I32(anim_id, 2)
  DBC_U32(head_effect, 3)
  DBC_U32(chest_effect, 4)
  DBC_U32(base_effect, 5)
  DBC_U32(left_hand_effect, 6)
  DBC_U32(right_hand_effect, 7)
  DBC_U32(breath_effect, 8)
  DBC_U32(left_weapon_effect, 9)
  DBC_U32(right_weapon_effect, 10)
  DBC_U32(special1_effect, 11)
  DBC_U32(world_effect, 12)
  DBC_U32(sound_id, 13)
  DBC_U32_ARRAY(proc_type, 15)
  DBC_F32_ARRAY(proc_param_zero, 19)
  DBC_F32_ARRAY(proc_param_one, 23)
  DBC_F32_ARRAY(proc_param_two, 27)
  DBC_F32_ARRAY(proc_param_three, 31)
)

OPENWOW_DBC_SCHEMA(SpellVisualEffectNameEntry,
  DBC_U32(id, 0)
  DBC_STRING(name, 1)
  DBC_STRING(file_path, 2)
  DBC_F32(area_effect_size, 3)
  DBC_F32(scale, 4)
  DBC_F32(min_allowed_scale, 5)
  DBC_F32(max_allowed_scale, 6)
)

OPENWOW_DBC_SCHEMA(AnimationDataEntry,
  DBC_U32(id, 0)
  DBC_STRING(name, 1)
  DBC_U32(weapon_flags, 2)
  DBC_U32(body_flags, 3)
  DBC_U32(flags, 4)
  DBC_U32(fallback, 5)
  DBC_U32(behavior_id, 6)
  DBC_U32(behavior_tier, 7)
)

OPENWOW_DBC_SCHEMA(SkillLineEntry,
  DBC_U32(id, 0)
  DBC_I32(category_id, 1)
  DBC_U32(skill_cost_id, 2)
  DBC_LOCALIZED(name, 3)
  DBC_U32(spell_icon_id, 21)
)

OPENWOW_DBC_SCHEMA(SkillLineAbilityEntry,
  DBC_U32(id, 0)
  DBC_U32(skill_id, 1)
  DBC_U32(spell_id, 2)
  DBC_U32(race_mask, 3)
  DBC_U32(class_mask, 4)
  DBC_U32(min_skill_rank, 7)
  DBC_U32(superseded_by_spell, 8)
  DBC_U32(acquire_method, 9)
  DBC_U32(trivial_skill_hi, 10)
  DBC_U32(trivial_skill_lo, 11)
  DBC_U32(num_skill_ups, 12)
  DBC_U32(unique_bit, 14)
)

OPENWOW_DBC_SCHEMA(GtCombatRatingsEntry,
  DBC_ROW_ID()
  DBC_F32(value, 0)
)

OPENWOW_DBC_SCHEMA(SpellCastTimesEntry,
  DBC_U32(id, 0)
  DBC_I32(base_cast_time, 1)
  DBC_I32(per_level, 2)
  DBC_I32(minimum, 3)
)

OPENWOW_DBC_SCHEMA(SpellRangeEntry,
  DBC_U32(id, 0)
  DBC_F32(range_min, 1)
  // Classic stores only minRange and maxRange after the ID.
  DBC_F32(range_max, 2)
  DBC_LOCALIZED(display_name, 4)
  DBC_LOCALIZED(display_name_short, 13)
)

OPENWOW_DBC_SCHEMA(SpellEntry,
    e.id = f.GetUInt32(row, 0);
    e.school_mask = f.GetUInt32(row, 1);
    e.category = f.GetUInt32(row, 2);
    e.cast_ui = f.GetUInt32(row, 3);
    e.dispel = f.GetUInt32(row, 4);
    e.mechanic = f.GetUInt32(row, 5);

    e.attributes = f.GetUInt32(row, 6);
    e.attributes_ex = f.GetUInt32(row, 7);
    e.attributes_ex2 = f.GetUInt32(row, 8);
    e.attributes_ex3 = f.GetUInt32(row, 9);
    e.attributes_ex4 = f.GetUInt32(row, 10);

    e.targets = f.GetUInt32(row, 13);
    e.target_aura_state = f.GetUInt32(row, 17);
    e.casting_time_index = f.GetUInt32(row, 18);
    e.recovery_time = f.GetUInt32(row, 19);
    e.category_recovery_time = f.GetUInt32(row, 20);
    e.interrupt_flags = f.GetUInt32(row, 21);
    e.aura_interrupt_flags = f.GetUInt32(row, 22);
    e.channel_interrupt_flags = f.GetUInt32(row, 23);
    e.proc_flags = f.GetUInt32(row, 24);
    e.proc_chance = f.GetUInt32(row, 25);
    e.proc_charges = f.GetUInt32(row, 26);
    e.max_level = f.GetUInt32(row, 27);
    e.base_level = f.GetUInt32(row, 28);
    e.spell_level = f.GetUInt32(row, 29);
    e.duration_index = f.GetUInt32(row, 30);
    e.power_type = f.GetUInt32(row, 31);
    e.mana_cost = f.GetUInt32(row, 32);
    e.mana_cost_per_level = f.GetUInt32(row, 33);
    e.mana_per_second = f.GetUInt32(row, 34);
    e.mana_per_second_per_level = f.GetUInt32(row, 35);
    e.range_index = f.GetUInt32(row, 36);
    e.speed = f.GetFloat(row, 37);
    e.modal_next_spell = f.GetUInt32(row, 38);
    e.stack_amount = f.GetUInt32(row, 39);

    DBC_U32_ARRAY(totem, 40)

    for (int i = 0; i < kMaxSpellReagents; ++i) {
      e.reagent[i] = f.GetInt32(row, 42 + i);
      e.reagent_count[i] = f.GetUInt32(row, 50 + i);
    }

    e.equipped_item_class = f.GetInt32(row, 58);
    e.equipped_item_sub_class_mask = f.GetInt32(row, 59);
    e.equipped_item_inv_type_mask = f.GetInt32(row, 60);

    for (int i = 0; i < kMaxSpellEffects; ++i) {
      e.effect[i] = f.GetUInt32(row, 61 + i);
      e.effect_die_sides[i] = f.GetInt32(row, 64 + i);
      e.effect_real_points_per_lvl[i] = f.GetFloat(row, 73 + i);
      e.effect_base_points[i] = f.GetInt32(row, 76 + i);
      e.effect_mechanic[i] = f.GetUInt32(row, 79 + i);
      e.effect_implicit_target_a[i] = f.GetUInt32(row, 82 + i);
      e.effect_implicit_target_b[i] = f.GetUInt32(row, 85 + i);
      e.effect_radius_index[i] = f.GetUInt32(row, 88 + i);
      e.effect_apply_aura[i] = f.GetUInt32(row, 91 + i);
      e.effect_amplitude[i] = f.GetUInt32(row, 94 + i);
      e.effect_value_multiplier[i] = f.GetFloat(row, 97 + i);
      e.effect_chain_target[i] = f.GetUInt32(row, 100 + i);
      e.effect_item_type[i] = f.GetUInt32(row, 103 + i);
      e.effect_misc_value[i] = f.GetInt32(row, 106 + i);
      e.effect_trigger_spell[i] = f.GetUInt32(row, 109 + i);
      e.effect_points_per_combo[i] = f.GetFloat(row, 112 + i);
    }

    e.mana_cost_percentage = f.GetUInt32(row, 156);
    e.start_recovery_category = f.GetUInt32(row, 157);
    e.start_recovery_time = f.GetUInt32(row, 158);
    e.max_target_level = f.GetUInt32(row, 159);
    e.spell_family_name = f.GetUInt32(row, 160);
    e.spell_family_flags[0] = f.GetUInt32(row, 161);
    e.spell_family_flags[1] = f.GetUInt32(row, 162);
    e.max_affected_targets = f.GetUInt32(row, 163);
    e.dmg_class = f.GetUInt32(row, 164);
    e.prevention_type = f.GetUInt32(row, 165);
    e.stance_bar_order = f.GetUInt32(row, 166);

    e.spell_visual[0] = f.GetUInt32(row, 115);
    e.spell_icon_id = f.GetUInt32(row, 117);
    e.spell_name = f.GetLocalizedString(row, 120);
    e.rank = f.GetLocalizedString(row, 129);
    e.description = f.GetLocalizedString(row, 138);
    e.tooltip = f.GetLocalizedString(row, 147);
)

OPENWOW_DBC_SCHEMA(TalentEntry,
  DBC_U32(id, 0)
  DBC_U32(tab_id, 1)
  DBC_U32(tier_id, 2)
  DBC_U32(column_index, 3)
  DBC_U32_ARRAY(spell_rank, 4)
  DBC_U32_ARRAY(prereq_talent, 13)
  DBC_U32_ARRAY(prereq_rank, 16)
  DBC_U32(flags, 19)
  DBC_U32(required_spell_id, 20)
  DBC_U32_ARRAY(pet_talent_mask, 21)
)

OPENWOW_DBC_SCHEMA(TalentTabEntry,
  DBC_U32(id, 0)
  DBC_LOCALIZED(name, 1)
  DBC_U32(spell_icon_id, 18)
  DBC_U32(race_mask, 19)
  DBC_U32(class_mask, 20)
  DBC_U32(pet_talent_mask, 21)
  DBC_U32(order_index, 22)
  DBC_STRING(background_file, 23)
)

OPENWOW_DBC_SCHEMA(TaxiNodesEntry,
    e.id = f.GetUInt32(row, 0);
    e.map_id = f.GetUInt32(row, 1);
    e.x = f.GetFloat(row, 2);
    e.y = f.GetFloat(row, 3);
    e.z = f.GetFloat(row, 4);
    e.name = f.GetLocalizedString(row, 5);
    e.mount_creature_id[0] = f.GetUInt32(row, 14);
    e.mount_creature_id[1] = f.GetUInt32(row, 15);
)

OPENWOW_DBC_SCHEMA(TaxiPathEntry,
  DBC_U32(id, 0)
  DBC_U32(from_node_id, 1)
  DBC_U32(to_node_id, 2)
  DBC_U32(cost, 3)
)

OPENWOW_DBC_SCHEMA(TaxiPathNodeEntry,
  DBC_U32(id, 0)
  DBC_U32(path_id, 1)
  DBC_U32(node_index, 2)
  DBC_U32(map_id, 3)
  DBC_F32(x, 4)
  DBC_F32(y, 5)
  DBC_F32(z, 6)
  DBC_U32(flags, 7)
  DBC_U32(delay, 8)
  DBC_U32(arrival_event_id, 9)
  DBC_U32(departure_event_id, 10)
)

}
