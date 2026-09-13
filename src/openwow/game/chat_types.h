#pragma once

#include "openwow/core/storm_string.h"

#include <cstdint>

namespace openwow::game {

enum class ChatMsg : std::uint8_t {
  kSystem       = 0x00,
  kSay          = 0x01,
  kParty        = 0x02,
  kRaid         = 0x03,
  kGuild        = 0x04,
  kOfficer      = 0x05,
  kYell         = 0x06,
  kWhisper      = 0x07,
  kWhisperForeign = 0x08,
  kWhisperInform = 0x09,
  kEmote        = 0x0A,
  kTextEmote    = 0x0B,
  kMonsterSay   = 0x0C,
  kMonsterParty = 0x0D,
  kMonsterYell  = 0x0E,
  kMonsterWhisper = 0x0F,
  kMonsterEmote = 0x10,
  kChannel      = 0x11,
  kChannelJoin  = 0x12,
  kChannelLeave = 0x13,
  kChannelList  = 0x14,
  kChannelNotice = 0x15,
  kChannelNoticeUser = 0x16,
  kAfk          = 0x17,
  kDnd          = 0x18,
  kIgnored      = 0x19,
  kSkill        = 0x1A,
  kLoot         = 0x1B,
  kMoney        = 0x1C,
  kOpening      = 0x1D,
  kTradeskills  = 0x1E,
  kPetInfo      = 0x1F,
  kCombatMiscInfo = 0x20,
  kCombatXpGain = 0x21,
  kCombatHonorGain = 0x22,
  kCombatFactionChange = 0x23,
  kBgSystemNeutral = 0x24,
  kBgSystemAlliance = 0x25,
  kBgSystemHorde = 0x26,
  kRaidLeader   = 0x27,
  kRaidWarning  = 0x28,
  kRaidBossEmote = 0x29,
  kRaidBossWhisper = 0x2A,
  kFiltered     = 0x2B,
  kBattleground = 0x2C,
  kBattlegroundLeader = 0x2D,
  kRestricted   = 0x2E,
  kBattlenet    = 0x2F,
  kAchievement  = 0x30,
  kGuildAchievement = 0x31,
  kArenaPoints  = 0x32,
  kPartyLeader  = 0x33,
  kTargetIcons  = 0x34,

  kBnWhisper        = 0x35,
  kBnWhisperInform  = 0x36,
  kBnConversation   = 0x37,
  kBnConversationNotice = 0x38,
  kBnConversationList = 0x39,
  kBnInlineToastAlert = 0x3A,
  kBnInlineToastBroadcast = 0x3B,
  kBnInlineToastBroadcastInform = 0x3C,
  kBnInlineToastConversation = 0x3D,

  // 1.12-only chattypen. WotLK schrapte de losse combat-/spellregels (die werden
  // COMBAT_LOG_EVENT), maar de lokale 1.12-server stuurt ze nog en de lokale
  // FrameXML/addons (CombatText, pfUI, SCT) luisteren erop. Deze waarden zijn
  // ONZE interne nummers; de wire-nummers staan in kChatMsgWireTable hieronder.
  kCombatSelfHits = 0x3E,
  kCombatSelfMisses = 0x3F,
  kCombatPetHits = 0x40,
  kCombatPetMisses = 0x41,
  kCombatPartyHits = 0x42,
  kCombatPartyMisses = 0x43,
  kCombatFriendlyPlayerHits = 0x44,
  kCombatFriendlyPlayerMisses = 0x45,
  kCombatHostilePlayerHits = 0x46,
  kCombatHostilePlayerMisses = 0x47,
  kCombatCreatureVsSelfHits = 0x48,
  kCombatCreatureVsSelfMisses = 0x49,
  kCombatCreatureVsPartyHits = 0x4A,
  kCombatCreatureVsPartyMisses = 0x4B,
  kCombatCreatureVsCreatureHits = 0x4C,
  kCombatCreatureVsCreatureMisses = 0x4D,
  kCombatFriendlyDeath = 0x4E,
  kCombatHostileDeath = 0x4F,
  kSpellSelfDamage = 0x50,
  kSpellSelfBuff = 0x51,
  kSpellPetDamage = 0x52,
  kSpellPetBuff = 0x53,
  kSpellPartyDamage = 0x54,
  kSpellPartyBuff = 0x55,
  kSpellFriendlyPlayerDamage = 0x56,
  kSpellFriendlyPlayerBuff = 0x57,
  kSpellHostilePlayerDamage = 0x58,
  kSpellHostilePlayerBuff = 0x59,
  kSpellCreatureVsSelfDamage = 0x5A,
  kSpellCreatureVsSelfBuff = 0x5B,
  kSpellCreatureVsPartyDamage = 0x5C,
  kSpellCreatureVsPartyBuff = 0x5D,
  kSpellCreatureVsCreatureDamage = 0x5E,
  kSpellCreatureVsCreatureBuff = 0x5F,
  kSpellTradeskills = 0x60,
  kSpellDamageShieldsOnSelf = 0x61,
  kSpellDamageShieldsOnOthers = 0x62,
  kSpellAuraGoneSelf = 0x63,
  kSpellAuraGoneParty = 0x64,
  kSpellAuraGoneOther = 0x65,
  kSpellItemEnchantments = 0x66,
  kSpellBreakAura = 0x67,
  kSpellPeriodicSelfDamage = 0x68,
  kSpellPeriodicSelfBuffs = 0x69,
  kSpellPeriodicPartyDamage = 0x6A,
  kSpellPeriodicPartyBuffs = 0x6B,
  kSpellPeriodicFriendlyPlayerDamage = 0x6C,
  kSpellPeriodicFriendlyPlayerBuffs = 0x6D,
  kSpellPeriodicHostilePlayerDamage = 0x6E,
  kSpellPeriodicHostilePlayerBuffs = 0x6F,
  kSpellPeriodicCreatureDamage = 0x70,
  kSpellPeriodicCreatureBuffs = 0x71,
  kSpellFailedLocalPlayer = 0x72,
  kHardcore = 0x73,
};

constexpr std::uint8_t kMaxChatMsgType = 0x73;

// De lokale 1.12-server nummert de chattypen anders dan onze 3.3.5-enum: SAY is
// daar 0x00 (bij ons kSay = 0x01), SYSTEM 0x0A, LOOT 0x18 enzovoort. Deze tabel
// is de ENIGE plek waar dat verschil hoort te leven: bij het parsen van
// SMSG_MESSAGECHAT en het schrijven van CMSG_MESSAGECHAT.
//
// Bron: Source\src\game\SharedDefines.h:1365-1462 (enum ChatMsg) en
// Source\src\game\Chat\Chat.cpp:2290-2340 (BuildChatMessage); Benilla leest
// dezelfde nummering (benilla-protocol/src/messages/chat.rs:171-210).
struct ChatMsgWirePair {
  ChatMsg type;
  std::uint8_t legacy;
};

inline constexpr ChatMsgWirePair kChatMsgWireTable[] = {
    {ChatMsg::kSay, 0x00},
    {ChatMsg::kParty, 0x01},
    {ChatMsg::kRaid, 0x02},
    {ChatMsg::kGuild, 0x03},
    {ChatMsg::kOfficer, 0x04},
    {ChatMsg::kYell, 0x05},
    {ChatMsg::kWhisper, 0x06},
    {ChatMsg::kWhisperInform, 0x07},
    {ChatMsg::kEmote, 0x08},
    {ChatMsg::kTextEmote, 0x09},
    {ChatMsg::kSystem, 0x0A},
    {ChatMsg::kMonsterSay, 0x0B},
    {ChatMsg::kMonsterYell, 0x0C},
    {ChatMsg::kMonsterEmote, 0x0D},
    {ChatMsg::kChannel, 0x0E},
    {ChatMsg::kChannelJoin, 0x0F},
    {ChatMsg::kChannelLeave, 0x10},
    {ChatMsg::kChannelList, 0x11},
    {ChatMsg::kChannelNotice, 0x12},
    {ChatMsg::kChannelNoticeUser, 0x13},
    {ChatMsg::kAfk, 0x14},
    {ChatMsg::kDnd, 0x15},
    {ChatMsg::kIgnored, 0x16},
    {ChatMsg::kSkill, 0x17},
    {ChatMsg::kLoot, 0x18},
    {ChatMsg::kCombatMiscInfo, 0x19},
    {ChatMsg::kMonsterWhisper, 0x1A},
    {ChatMsg::kCombatSelfHits, 0x1B},
    {ChatMsg::kCombatSelfMisses, 0x1C},
    {ChatMsg::kCombatPetHits, 0x1D},
    {ChatMsg::kCombatPetMisses, 0x1E},
    {ChatMsg::kCombatPartyHits, 0x1F},
    {ChatMsg::kCombatPartyMisses, 0x20},
    {ChatMsg::kCombatFriendlyPlayerHits, 0x21},
    {ChatMsg::kCombatFriendlyPlayerMisses, 0x22},
    {ChatMsg::kCombatHostilePlayerHits, 0x23},
    {ChatMsg::kCombatHostilePlayerMisses, 0x24},
    {ChatMsg::kCombatCreatureVsSelfHits, 0x25},
    {ChatMsg::kCombatCreatureVsSelfMisses, 0x26},
    {ChatMsg::kCombatCreatureVsPartyHits, 0x27},
    {ChatMsg::kCombatCreatureVsPartyMisses, 0x28},
    {ChatMsg::kCombatCreatureVsCreatureHits, 0x29},
    {ChatMsg::kCombatCreatureVsCreatureMisses, 0x2A},
    {ChatMsg::kCombatFriendlyDeath, 0x2B},
    {ChatMsg::kCombatHostileDeath, 0x2C},
    {ChatMsg::kCombatXpGain, 0x2D},
    {ChatMsg::kSpellSelfDamage, 0x2E},
    {ChatMsg::kSpellSelfBuff, 0x2F},
    {ChatMsg::kSpellPetDamage, 0x30},
    {ChatMsg::kSpellPetBuff, 0x31},
    {ChatMsg::kSpellPartyDamage, 0x32},
    {ChatMsg::kSpellPartyBuff, 0x33},
    {ChatMsg::kSpellFriendlyPlayerDamage, 0x34},
    {ChatMsg::kSpellFriendlyPlayerBuff, 0x35},
    {ChatMsg::kSpellHostilePlayerDamage, 0x36},
    {ChatMsg::kSpellHostilePlayerBuff, 0x37},
    {ChatMsg::kSpellCreatureVsSelfDamage, 0x38},
    {ChatMsg::kSpellCreatureVsSelfBuff, 0x39},
    {ChatMsg::kSpellCreatureVsPartyDamage, 0x3A},
    {ChatMsg::kSpellCreatureVsPartyBuff, 0x3B},
    {ChatMsg::kSpellCreatureVsCreatureDamage, 0x3C},
    {ChatMsg::kSpellCreatureVsCreatureBuff, 0x3D},
    {ChatMsg::kSpellTradeskills, 0x3E},
    {ChatMsg::kSpellDamageShieldsOnSelf, 0x3F},
    {ChatMsg::kSpellDamageShieldsOnOthers, 0x40},
    {ChatMsg::kSpellAuraGoneSelf, 0x41},
    {ChatMsg::kSpellAuraGoneParty, 0x42},
    {ChatMsg::kSpellAuraGoneOther, 0x43},
    {ChatMsg::kSpellItemEnchantments, 0x44},
    {ChatMsg::kSpellBreakAura, 0x45},
    {ChatMsg::kSpellPeriodicSelfDamage, 0x46},
    {ChatMsg::kSpellPeriodicSelfBuffs, 0x47},
    {ChatMsg::kSpellPeriodicPartyDamage, 0x48},
    {ChatMsg::kSpellPeriodicPartyBuffs, 0x49},
    {ChatMsg::kSpellPeriodicFriendlyPlayerDamage, 0x4A},
    {ChatMsg::kSpellPeriodicFriendlyPlayerBuffs, 0x4B},
    {ChatMsg::kSpellPeriodicHostilePlayerDamage, 0x4C},
    {ChatMsg::kSpellPeriodicHostilePlayerBuffs, 0x4D},
    {ChatMsg::kSpellPeriodicCreatureDamage, 0x4E},
    {ChatMsg::kSpellPeriodicCreatureBuffs, 0x4F},
    {ChatMsg::kSpellFailedLocalPlayer, 0x50},
    {ChatMsg::kCombatHonorGain, 0x51},
    {ChatMsg::kBgSystemNeutral, 0x52},
    {ChatMsg::kBgSystemAlliance, 0x53},
    {ChatMsg::kBgSystemHorde, 0x54},
    {ChatMsg::kCombatFactionChange, 0x55},
    {ChatMsg::kMoney, 0x56},
    {ChatMsg::kRaidLeader, 0x57},
    {ChatMsg::kRaidWarning, 0x58},
    {ChatMsg::kRaidBossWhisper, 0x59},
    {ChatMsg::kRaidBossEmote, 0x5A},
    {ChatMsg::kFiltered, 0x5B},
    {ChatMsg::kBattleground, 0x5C},
    {ChatMsg::kBattlegroundLeader, 0x5D},
    {ChatMsg::kHardcore, 0x5E},
};

// 1.12-wirenummer -> onze enum. Onbekend nummer wordt SYSTEM (zoals de oude
// cast deed voor de waarden die wij niet kennen) zodat er altijd een event vuurt.
[[nodiscard]] constexpr ChatMsg ChatMsgFromLegacyWire(
    const std::uint8_t legacy) {
  for (const auto& entry : kChatMsgWireTable) {
    if (entry.legacy == legacy) {
      return entry.type;
    }
  }
  return ChatMsg::kSystem;
}

// Onze enum -> 1.12-wirenummer; 0xFF voor typen die 1.12 niet kent (de server
// weigert die met "Wrong message type" in plaats van ze in het verkeerde kanaal
// te zetten).
[[nodiscard]] constexpr std::uint8_t LegacyWireFromChatMsg(const ChatMsg type) {
  for (const auto& entry : kChatMsgWireTable) {
    if (entry.type == type) {
      return entry.legacy;
    }
  }
  return 0xFF;
}

enum class Language : std::uint32_t {
  kUniversal    = 0,
  kOrcish       = 1,
  kDarnassian   = 2,
  kTaurahe      = 3,
  kDwarvish     = 6,
  kCommon       = 7,
  kDemonic      = 8,
  kTitan        = 9,
  kThalassian   = 10,
  kDraconic     = 11,
  kKalimag      = 12,
  kGnomish      = 13,
  kTroll        = 14,
  kGutterspeak  = 33,
  kDraenei      = 35,
  kZombie       = 36,
  kGnomishBinary = 37,
  kGoblinBinary = 38,
  kAddon        = 0xFFFFFFFF,
};

enum class ChatTag : std::uint8_t {
  kNone = 0x00,
  kAfk  = 0x01,
  kDnd  = 0x02,
  kGm   = 0x04,
  kCom  = 0x08,
  kDev  = 0x10,
};

// De lokale 1.12-server stuurt de chat-tag als EENVOUDIGE waarde, niet als
// bitmask: Source\src\game\Chat\Chat.h:83-89 heeft NONE=0, AFK=1, DND=2, GM=3.
// Onze enum hierboven is de 3.3.5-bitmask (kGm = 0x04), dus zonder deze
// vertaling leest de client de GM-tag (3) als AFK|DND en zet hij "DND" op elk
// bericht van een GM-account (en op de afzender zelf).
[[nodiscard]] constexpr ChatTag ChatTagFromLegacyWire(const std::uint8_t legacy) {
  switch (legacy) {
    case 1:
      return ChatTag::kAfk;
    case 2:
      return ChatTag::kDnd;
    case 3:
      return ChatTag::kGm;
    default:
      return ChatTag::kNone;
  }
}

enum class ChannelNotify : std::uint8_t {
  kJoined       = 0x00,
  kLeft         = 0x01,
  kYouJoined    = 0x02,
  kYouLeft      = 0x03,
  kWrongPassword = 0x04,
  kNotMember    = 0x05,
  kNotModerator = 0x06,
  kPasswordChanged = 0x07,
  kOwnerChanged = 0x08,
  kPlayerNotFound = 0x09,
  kNotOwner     = 0x0A,
  kChannelOwner = 0x0B,
  kModeChange   = 0x0C,
  kAnnouncementsOn = 0x0D,
  kAnnouncementsOff = 0x0E,
  kModerationOn = 0x0F,
  kModerationOff = 0x10,
  kMuted        = 0x11,
  kPlayerKicked = 0x12,
  kBanned       = 0x13,
  kPlayerBanned = 0x14,
  kPlayerUnbanned = 0x15,
  kPlayerNotBanned = 0x16,
  kPlayerAlreadyMember = 0x17,
  kInvite       = 0x18,
  kInviteWrongFaction = 0x19,
  kWrongFaction = 0x1A,
  kInvalidName  = 0x1B,
  kNotModerated = 0x1C,
  kPlayerInvited = 0x1D,
  kPlayerInviteBanned = 0x1E,
  kThrottled    = 0x1F,
  kNotInArea    = 0x20,
  kNotInLfg     = 0x21,
  kVoiceOn      = 0x22,
  kVoiceOff     = 0x23,
  kVoiceOnSilent = 0x24,

};

constexpr bool IsMonsterChatType(ChatMsg type) {
  switch (type) {
    case ChatMsg::kMonsterSay:
    case ChatMsg::kMonsterParty:
    case ChatMsg::kMonsterYell:
    case ChatMsg::kMonsterWhisper:
    case ChatMsg::kMonsterEmote:
    case ChatMsg::kRaidBossEmote:
    case ChatMsg::kRaidBossWhisper:
    case ChatMsg::kBattlenet:
      return true;
    default:
      return false;
  }
}

constexpr bool IsBgSystemMessage(ChatMsg type) {
  return type == ChatMsg::kBgSystemNeutral ||
         type == ChatMsg::kBgSystemAlliance ||
         type == ChatMsg::kBgSystemHorde;
}

constexpr bool IsAchievementMessage(ChatMsg type) {
  return type == ChatMsg::kAchievement ||
         type == ChatMsg::kGuildAchievement;
}

constexpr bool ChatTypeNeedsTarget(ChatMsg type) {
  return type == ChatMsg::kWhisper;
}

constexpr bool ChatTypeNeedsChannel(ChatMsg type) {
  return type == ChatMsg::kChannel;
}

inline const char* GetLanguageName(Language lang) {
  switch (lang) {
    case Language::kUniversal:     return "";
    case Language::kOrcish:        return "Orcish";
    case Language::kDarnassian:    return "Darnassian";
    case Language::kTaurahe:       return "Taurahe";
    case Language::kDwarvish:      return "Dwarvish";
    case Language::kCommon:        return "Common";
    case Language::kDemonic:       return "Demonic";
    case Language::kTitan:         return "Titan";
    case Language::kThalassian:    return "Thalassian";
    case Language::kDraconic:      return "Draconic";
    case Language::kKalimag:       return "Kalimag";
    case Language::kGnomish:       return "Gnomish";
    case Language::kTroll:         return "Troll";
    case Language::kGutterspeak:   return "Gutterspeak";
    case Language::kDraenei:       return "Draenei";
    case Language::kZombie:        return "Zombie";
    case Language::kGnomishBinary: return "Gnomish Binary";
    case Language::kGoblinBinary:  return "Goblin Binary";
    case Language::kAddon:         return "";
    default:                       return "";
  }
}

inline bool ChatTypeStringToID(const char* str, ChatMsg* outType) {
  struct Entry { const char* name; ChatMsg id; };
  static constexpr Entry kTable[] = {
    {"SAY",              ChatMsg::kSay},
    {"PARTY",            ChatMsg::kParty},
    {"RAID",             ChatMsg::kRaid},
    {"GUILD",            ChatMsg::kGuild},
    {"OFFICER",          ChatMsg::kOfficer},
    {"YELL",             ChatMsg::kYell},
    {"WHISPER",          ChatMsg::kWhisper},
    {"EMOTE",            ChatMsg::kEmote},
    {"CHANNEL",          ChatMsg::kChannel},
    {"AFK",              ChatMsg::kAfk},
    {"DND",              ChatMsg::kDnd},
    {"RAID_WARNING",     ChatMsg::kRaidWarning},
    {"BATTLEGROUND",     ChatMsg::kBattleground},
    {"BN",               ChatMsg::kBattlenet},
    {"BN_WHISPER",       ChatMsg::kBnWhisper},
    {"BN_WHISPER_INFORM",ChatMsg::kBnWhisperInform},
    {"BN_CONVERSATION",  ChatMsg::kBnConversation},
  };
  for (const auto& e : kTable) {
    if (openwow::core::SStrCmpNoCase(e.name, str, 0x7FFFFFFFu) == 0) {
      *outType = e.id;
      return true;
    }
  }
  return false;
}

}
