# Protocol-audit: 3.3.5/WotLK-client vs lokale 1.12.1-server

Read-only inventarisatie in `D:\OllieWoW\Experiments\OpenWow-snapshot`.
Doel: alle opcodes waarbij de packet-BODY of het veldcontract in de OpenWow-client nog
de 3.3.5/WotLK-vorm heeft terwijl de lokale 1.12-server iets anders stuurt of verwacht.

**Bronnen**
- Client-opcodetabel: `include/openwow/network/protocol/wotlk/opcodes.h` (1307 entries).
- Server-opcodetabel (autoritatief): `D:\OllieWoW\Source\src\game\Protocol\Opcodes_1_12_1.h` (827 entries).
- Body-vergelijking: client `src/openwow/**` tegen server `D:\OllieWoW\Source\src\game\**`.

**Methode**: de twee opcodetabellen zijn volledig machinematig op naam én nummer
vergeleken (resultaat in sectie c); daarna zijn de client-handlers veld voor veld tegen
de server-`SendPacket`/`recvPacket >>`-code gelegd. Elke bevinding is aan
beide zijden gelezen; het merendeel van de hoog-impactgevallen is daarna nogmaals
handmatig line-voor-line gecontroleerd. Onzekerheid staat expliciet als **vermoeden**.

> De opdracht noemde enkele items "al gefixt". Die zijn gecontroleerd; waar de code dat
> tegenspreekt staat dat er expliciet bij (CMSG_JOIN_CHANNEL/CMSG_LEAVE_CHANNEL,
> SMSG_DEFENSE_MESSAGE).

---

## (a) Bevindingen per opcode

### A. Movement / splines

| # | opcode | onze file:regel | server file:regel | afwijking | symptoom | zekerheid |
|---|---|---|---|---|---|---|
| A1 | SMSG_FORCE_RUN_SPEED_CHANGE | `src/openwow/game/world_session_movement.cpp:319-331` | `Movement/MovementPacketSender.cpp:62-64` | Client slaat na de u32-counter nog 1 byte over (WotLK-layout); 1.12 stuurt packguid + u32 counter + float, zonder extra byte. Payload = 9+n, client leest op off+1 → `off + 4 > size` is altijd waar. | Elke door de server geforceerde runsnelheidswijziging (sprint, slow, charge) wordt stil gedropt. | zeker |
| A2 | SMSG_MONSTER_MOVE / _TRANSPORT | `src/openwow/game/monster_move.cpp:78` (`monster_move.h:35` kAnimation=0x00200000) | `Movement/spline/packet_builder.cpp:72` (`MoveSplineFlag.h:60` Enter_Cycle=0x00200000) | 1.12 kent geen Animation-bit; 0x00200000 is Enter_Cycle en wordt door de packet_builder op elke cyclische spline gezet. Client leest daardoor u8 animation_id + u32 anim_start_time (5 bytes) die nooit geschreven zijn. | Cyclische splines (PointMovementGenerator/SetCyclic) parsen verminkt: verkeerde duur/waypoints, unit warpt of packet wordt gedropt. | zeker |
| A3 | SMSG_MONSTER_MOVE (cyclische CatmullRom) | `src/openwow/game/monster_move.cpp:106-114` | `Movement/spline/packet_builder.cpp:125-126` | Server schrijft `count + 1` punten met een fake eerste vertex; client leest exact `count` punten en heeft geen "erase first"-logica → 12 bytes te veel/laatste waypoint weg. | Laatste waypoint van cyclische catmull-splines ontbreekt. | vermoeden |

### B. Spells / auras / combat

| # | opcode | onze file:regel | server file:regel | afwijking | symptoom | zekerheid |
|---|---|---|---|---|---|---|
| B1 | MSG_CHANNEL_START | `src/openwow/net/wotlk/spell_packets.cpp:332-336` | `Spells/Spell.cpp:5132-5134` | 1.12 = u32 spellId + u32 duration. Client leest eerst een packed guid, dan u32 spell_id + u32 duration → te weinig bytes. | Channeled cast wordt niet als channeling gezet; castbar/animatie ontbreekt. | zeker |
| B2 | MSG_CHANNEL_UPDATE | `src/openwow/net/wotlk/spell_packets.cpp:349-351` | `Spells/Spell.cpp:5086-5087` | 1.12 = alleen u32 time. Client leest packed guid + u32 remaining → 4-byte packet faalt. | Channelbar loopt niet af; stop-update genegeerd. | zeker |
| B3 | SMSG_SPELL_DELAYED | `src/openwow/net/wotlk/spell_packets.cpp:317` | `Spells/Spell.cpp:7715` | Server schrijft guid als 8 rauwe bytes; client leest een packed guid, waarna de delay op de verkeerde offset staat. | Cast-pushback vertraagt de castbar niet; casttijd onjuist. | zeker |
| B4 | SMSG_SPELLHEALLOG | `src/openwow/game/combat_log.cpp:1253` | `Objects/Object.cpp:4345-4350` | 1.12 = packguid victim + packguid healer + u32 spell + u32 Damage + u8 crit. Client leest daarna nog u32 overheal + u32 absorb + u8 crit + u8 unused; na Damage rest 1 byte → ReadU32 faalt. | Geen enkele heal in de combat log / geen floating heal. | zeker |
| B5 | SMSG_PERIODICAURALOG | `src/openwow/game/combat_log.cpp:1729` | `Objects/Unit.cpp:4863` | 1.12 stuurt per periodic-type 1–4 waarden (u32 damage [, school, absorb, resist]); client leest amount, overkill, school, absorb, resist, u8 crit. | DoT's en periodieke heals verdwijnen uit combat log/floating text. | zeker |
| B6 | SMSG_SUPERCEDED_SPELL | `src/openwow/game/world_session_combat.cpp:914` | `Objects/Player.cpp:4479-4480` | Server = u16 oud + u16 nieuw. De tweede client-helper `ReadSupercededSpellIds` eist 2× u32 en faalt (de spell_book-parser leest wél correct). | Bij rank-up wordt het oude spell-id niet in de actiebalk vervangen. | zeker |
| B7 | SMSG_LEARNED_SPELL | `src/openwow/game/world_session_combat.cpp:897` | `Objects/Player.cpp:4697` | Server stuurt alleen u32 spellId (4 B); client eist u32 + u16 learn_flags (≥6 B). | Leren van een spreuk triggert geen actiebalk-notificatie via deze route. | zeker (bytes); impact vermoeden |
| B8 | CMSG_PET_CAST_SPELL | `src/openwow/net/wotlk/protocol/packet_sender.cpp:545-551` | `Handlers/PetHandler.cpp:601` | Client schrijft u64 guid + u8 cast_count + u32 spellId + …; server leest direct u64 guid + u32 spellId (+u16 targetmask). De cast_count-byte schuift spellId op. | Pet-abilities komen als onbekend spellId binnen en worden gedropt; pet-balk werkt niet. | zeker |
| B9 | SMSG_UPDATE_AURA_DURATION (0x137) | `include/.../opcodes.h:316` (0x137 = SMSG_EQUIPMENT_SET_SAVED), route `world_session.cpp:2639-2640` | `Spells/SpellAuras.cpp:7603-7605` | 1.12 stuurt op 0x137 u8 auraSlot + u32 duration. Client heeft geen SMSG_UPDATE_AURA_DURATION; 0x137 wordt als equipment-set-saved gedecodeerd. | Aura-duur/vervaltijd komt nooit binnen (buff/debuff-timers blijven leeg); comment `unit_descriptor_callbacks.cpp:106` verwacht juist 0x137. | zeker |
| B10 | CMSG_CAST_SPELL / CMSG_CANCEL_CAST (dormante builders) | `src/openwow/game/spell_book.cpp:939`, `aura_manager.cpp:432` | `Handlers/SpellHandler.cpp:328` | WotLK-builders schrijven u8 cast_count + u32 spellId resp. u32(0)+u32 spellId; 1.12 leest u32 spellId direct. De actieve paden gebruiken de correcte PacketSender-varianten; deze hebben geen aanroeper. | Momenteel geen; bij herbedrading verschuift elk veld. | vermoeden |

### C. Group / raid / guild / channel

| # | opcode | onze file:regel | server file:regel | afwijking | symptoom | zekerheid |
|---|---|---|---|---|---|---|
| C1 | SMSG_GROUP_LIST | `src/openwow/game/group_manager.cpp:15-43, 66-77` | `Group/Group.cpp:1422-1436` | Client leest 4 header-bytes + u64 groupGuid + u32 counter vóór de count; server schrijft 2 bytes (groupType, group|assistant) en direct de u32 count. Per lid: client 4 bytes, server 2. | Party/raid-lijst volledig misgeparsed; leden, subgroepen, loot settings en roster onbruikbaar. | zeker |
| C2 | SMSG_PARTY_MEMBER_STATS / _FULL | `src/openwow/game/party_stats.cpp:187-197, 159-171, 294` | `Handlers/GroupHandler.cpp:670-677, 700-704, 841` | status u8 (client u16), hp u16 (client u32), auramask u32+u16 (client u64+u32+u8); masker verschoven vanaf bit10 (`party_stats.h:29` kPetGuid=0x400 vs `Group/Group.h:119` AURAS_NEGATIVE=0x400). _FULL: client leest eerst u8 marker, server begint met guid. | Party-health/mana/auras fout of parser faalt; pet/vehicle-velden op verkeerde plek. | zeker |
| C3 | SMSG_GROUP_INVITE | `src/openwow/game/group_manager.cpp:108-125` | `Handlers/GroupHandler.cpp:170-171` | Client leest u8 flag + cstring + u32 counter + realm-list + u32 related_counter; server stuurt alleen de naam-cstring. Eerste letter wordt als flag gelezen. | Inkomende invite verschijnt niet (pending=false); invite-venster blijft weg. | zeker |
| C4 | SMSG_PARTY_COMMAND_RESULT | `src/openwow/game/group_manager.cpp:153-163` | `Handlers/GroupHandler.cpp:49-52` | Client eist een 4e veld (i32 value) dat 1.12 niet stuurt; PacketReader faalt bij EOF. | Alle party-foutmeldingen (already in group, full, not found) worden nooit getoond. | zeker |
| C5 | CMSG_REQUEST_PARTY_MEMBER_STATS | `src/openwow/net/wotlk/protocol/packet_sender.cpp:952-955` | `Handlers/GroupHandler.cpp:834` | Client stuurt een packed guid; server leest een rauwe u64. | Stats-aanvraag leest buiten het pakket → verkeerde/geen guid, stats-update faalt. | zeker |
| C6 | SMSG_FRIEND_LIST (0x67) / SMSG_IGNORE_LIST (0x6B) | `src/openwow/game/social_manager.cpp:11-77`; `opcodes.h:110,114` | `SocialMgr.cpp:93-114` (friend), `125-133` (ignore) | 1.12: u8 count + per vriend u64 guid + u8 status (+3× u32); ignores = u8 count + guids. Client verwacht 3.3.5: u32 flags + u32 count + per contact u64 + u32 contact_flags + note-cstring. 0x6B is bij de client een CMSG, dus ignores worden nooit gedispatcht. | Vriendenlijst leeg/verkeerd (count-byte als flags); ignore-lijst komt nooit aan. | zeker |
| C7 | SMSG_GUILD_ROSTER | `src/openwow/game/guild_manager.cpp:290-299, 314-316` | `Guild/Guild.cpp:1018-1019, 1173-1176` | Per rank verwacht de client 52 bytes (flags + gold limit + 6×2 banktab); server stuurt alleen u32 Rights. Per lid leest de client een extra u8 gender tussen Class en ZoneId. | Guildroster/rankrechten leeg of misgeparsed. | zeker |
| C8 | SMSG_GUILD_QUERY_RESPONSE (0x55) | `src/openwow/game/guild_manager.cpp:370` | `Guild/Guild.cpp:1280-1286` (ook `World.cpp:4415-4426`) | Client verwacht na de 5 emblem-u32's nog u32 rank_count (WotLK); 1.12 stuurt dat niet. | Guildnaam/tabard/ranknamen worden nooit gecached; guildinfo blijft leeg. | zeker |
| C9 | SMSG_CHANNEL_LIST (0x9B) | `src/openwow/game/world_session_chat.cpp:1482-1491` | `Chat/Channel.cpp:530-534` | Client leest een leidende u8 (channel-type) vóór de naam-cstring; server begint direct met de naam. | Kanaalnaam 1 byte verschoven → kanaalroster/watch-selectie leeg. | zeker |
| C10 | SMSG_CHANNEL_NOTIFY YOU_JOINED/YOU_LEFT | `src/openwow/game/chat_manager.cpp:174-183` | `Chat/Channel.cpp:866-867, 870-875` | YOU_JOINED: server u32 flags + u32 0 (8 B), client u8 flags + u32 channelId + u32 memberCount (9 B). YOU_LEFT: client eist u32 + u8, server stuurt geen payload. | `/join` en `/leave` registreren het kanaal niet; kanaal-UI blijft leeg. | zeker |
| C11 | CMSG_JOIN_CHANNEL / CMSG_LEAVE_CHANNEL | `src/openwow/game/chat_manager.cpp:41-57` en `:59-68`; `packet_sender.cpp:610-628` | `Handlers/ChannelHandler.cpp:34+48` (join), `:81` (leave) | **Niet gefixt**, ondanks de opdrachtnotitie. Client schrijft u32 channel_id + u8 join_flag + u8 has_voice + naam + wachtwoord; 1.12 leest naam (+wachtwoord). Leave stuurt u32 id + naam; 1.12 leest alleen naam. Aanroepers: `interaction_sender.cpp:2337,2342`, `game_lua_api_chatmsg.cpp:366`. | Kanalen joinen/verlaten werkt niet of leest een channelnaam uit de id-bytes. | zeker |
| C12 | SMSG_PETITION_QUERY_RESPONSE (0x1C7) | `src/openwow/game/petition_handler.cpp:182-192` | `Handlers/PetitionsHandler.cpp:219-233` | Client eist 10 extra cstrings + 2 u32 (petition_type) die 1.12 nooit stuurt; packet-simulatie faalt met EOF op de 9e string. | Guild-charter/petition-query wordt nooit gecached; charter-UI blijft leeg. | zeker |
| C13 | SMSG_PETITION_SHOWLIST (0x1BC) | `src/openwow/game/petition_handler.cpp:394-399` | `Handlers/PetitionsHandler.cpp:597-602` | Client leest 6 u32 per entry; server schrijft er 5 (`required signs` is uitgecommentarieerd). | Charter-vendorlijst faalt; koopvenster opent niet. | zeker |
| C14 | CMSG_ADD_FRIEND / CMSG_CONTACT_LIST / CMSG_GUILD_RANK (extra velden) | `social_manager.cpp:317-319`, `interaction_sender.cpp:2231-2232`, `packet_sender.cpp:1313-1320` | `MiscHandler.cpp:574-588`, `GuildHandler.cpp:645-647` | Client stuurt een extra note-string, een extra u32 flags bij 0x66 en 48 bytes banktab/money_per_day bij CMSG_GUILD_RANK; server leest die niet. 0x66 levert bovendien nooit de ignore-lijst. | Friend-note en banktab-rechten genegeerd; ignore-lijst niet ververst. | zeker (extra veld) |

### D. NPC / quest / taxi / pet

| # | opcode | onze file:regel | server file:regel | afwijking | symptoom | zekerheid |
|---|---|---|---|---|---|---|
| D1 | SMSG_QUESTGIVER_QUEST_COMPLETE | `src/openwow/game/session/quest_session.cpp:637-642` | `Objects/Player.cpp:16575-16593` | Client leest 6 u32 (quest, xp, money, honor, talent, arena) en eist Remaining()==0. 1.12 stuurt questid + u32 0x03 + XP + money + itemCount + itemCount×(itemId,count). Client leest 0x03 als xp, itemCount als talent; de itemlijst blijft over → altijd verworpen. | Quest inleveren geeft nooit QUEST_COMPLETE-afhandeling: geen reward/xp/money-melding, questlog/dialoog niet bijgewerkt. | zeker |
| D2 | CMSG_ACTIVATETAXIEXPRESS | `src/openwow/game/interaction_sender.cpp:1843-1846` | `Handlers/TaxiHandler.cpp:178` | Server verwacht guid + u32 _totalcost + u32 node_count + nodes; client stuurt guid + u32 count + nodes (totalcost ontbreekt). | Express/multi-hop taxi start met verkeerde/afgekapte route of faalt. | zeker |
| D3 | SMSG_PET_SPELLS (header) | `src/openwow/game/pet_manager.cpp:290-294` | `Objects/Player.cpp:19554-19560` | Client leest na guid u16 + u32 + u32 (10 B); 1.12 stuurt u32(0) + 4× u8 (8 B). Header 2 bytes verschoven. | Pet-actiebalk/spellbook toont verkeerde of geen knoppen; react/command/mode fout. | zeker |
| D4 | SMSG_PET_SPELLS (cooldowns) | `src/openwow/game/pet_manager.cpp:322-324` | `Objects/Unit.cpp:12204-12207` | Client leest 14 B/entry (u32 spell, u16 cat, u32 cd, u32 catcd); 1.12 schrijft 12 B (u16 spell, u16 0, u32 cd, u32 catcd). | Pet-spell cooldowns verschoven zodra er cooldown-entries zijn. | zeker |
| D5 | MSG_LIST_STABLED_PETS | `src/openwow/game/pet_manager.cpp:550-553` | `Handlers/NPCHandler.cpp:671-673` | Server schrijft na de naam u32 loyalty + u8 slot; client leest slechts 1 byte en laat 4 bytes/pet ongelezen (geen Remaining-check). | Stable-lijst toont verkeerde slot/flag; bij >1 pet corrupte vermeldingen. | zeker |
| D6 | CMSG_QUESTGIVER_QUERY_QUEST | `src/openwow/game/interaction_sender.cpp:1005-1008` | `Handlers/QuestHandler.cpp:208` | Client stuurt guid + questId + u8; 1.12 leest alleen guid + questId. Extra byte op de wire. | Geen functioneel gevolg (server negeert trailing bytes), wel afwijkende wire-vorm. | zeker |
| D7 | SMSG_SHOWTAXINODES (taximask) | `src/openwow/game/taxi_handler.h:48`, `taxi_handler.cpp:61-63` | `Objects/Player.cpp:381-382` (`DBCStructure.h:943` TaxiMaskSize=8) | Client leest 14× u32 (56 B); 1.12 schrijft 8× u32 (32 B). Reader bounds-checkt, dus alleen de eerste 8 woorden worden gevuld. | Latent: 1.12 heeft ≤256 nodes, dus geen zichtbaar effect; custom nodes >256 blijven onbekend. | zeker |
| D8 | SMSG_PET_NAME_QUERY_RESPONSE | `src/openwow/game/pet_manager.cpp:478-488` | `Handlers/PetHandler.cpp:300-303` | Client verwacht optioneel u8 has_declined + 5 declined-name strings (3.3.5); 1.12 stuurt die nooit. | Geen (client tolereert afwezigheid); 3.3.5-only restveld. | zeker |

### E. Commerce / inventory / loot

| # | opcode | onze file:regel | server file:regel | afwijking | symptoom | zekerheid |
|---|---|---|---|---|---|---|
| E1 | SMSG_ITEM_PUSH_RESULT | `src/openwow/game/inventory/adapters/protocol/inventory_messages.cpp:8,28` | `Objects/Player.cpp:14127-14138` | Client eist ≥45 B en leest een extra u32 total_count; 1.12 stuurt 41 B zonder total_count (regel 14139 is [-ZERO]). | Parse faalt altijd → geen item-push/ReceiveItem-feedback bij loot, vendor of quest. | zeker |
| E2 | SMSG_INVENTORY_CHANGE_FAILURE | `inventory_messages.cpp:50-52` | `Objects/Player.cpp:13752-13759` | Bij kCantEquipLevel staat requiredLevel server-side vóór de twee guids; client leest het erna. Gelijke lengte (22 B), dus parser slaagt met vervuilde guids. | "Level too low"-melding toont verkeerd item/level. | zeker |
| E3 | SMSG_LOOT_START_ROLL / SMSG_LOOT_ROLL | `src/openwow/game/inventory/loot/adapters/protocol/loot_roll_packet_codec.cpp:16-24,44` | `Group/Group.cpp:763-769`, `786-794` | Client leest 3.3.5 map_id + item_count + vote-mask die 1.12 niet stuurt; SMSG_LOOT_ROLL eist een extra u8 auto_pass. | Need/greed-rollvenster en rollresultaten verschijnen nooit. | zeker |
| E4 | SMSG_TRADE_STATUS (OPEN_WINDOW) | `src/openwow/game/commerce/trade/adapters/protocol/trade_packet_codec.cpp:55-58` | `Handlers/TradeHandler.cpp:50-52` | Client eist u32 status + u32 trade_session_id (8 B); 1.12 stuurt alleen u32 status (4 B). | Trade-venster opent nooit. | zeker |
| E5 | SMSG_TRADE_STATUS_EXTENDED | `trade_packet_codec.cpp:118-122` | `Handlers/TradeHandler.cpp:110-111` | Client leest 3× u32 socket_enchants vóór creator; 1.12 stuurt 0 sockets → creator/charges/suffix/random/durability schuiven. | Trade-venster toont geen/verkeerde itemdata. | zeker |
| E6 | MSG_AUCTION_HELLO | `src/openwow/game/commerce/auctions/adapters/protocol/auction_packet_codec.cpp:17-19` | `Handlers/AuctionHouseHandler.cpp:76-78` | Client eist na guid + houseId nog een u8 enabled (13 B); 1.12 stuurt 12 B. | Auction-house-venster opent niet. | zeker |
| E7 | SMSG_AUCTION_LIST_RESULT | `auction_packet_codec.cpp:41-52,66` | `AuctionHouse/AuctionHouseMgr.cpp:997-1005`, `AuctionHouseHandler.cpp:738` | Client leest 3×(id,duration,charges) + u32 unk_flags + trailing u32 search_delay; 1.12 stuurt 1× enchant-id en geen search_delay. | AH-zoek/bied/owner-lijsten parsen niet; AH-lijst blijft leeg. | zeker |
| E8 | SMSG_AUCTION_COMMAND_RESULT | `auction_packet_codec.cpp:89` | `Handlers/AuctionHouseHandler.cpp:99-102` | Server schrijft een rauwe u64-guid; client leest een packed guid. | "Outbid"-resultaat parseert fout/verschuift. | zeker |
| E9 | CMSG_AUCTION_SELL_ITEM | `auction_packet_codec.cpp:214-222` | `Handlers/AuctionHouseHandler.cpp:265-269` | Client stuurt count + per item (guid,count); 1.12 leest exact één itemGuid. | Item te koop zetten leest guid/prijzen verkeerd; veiling plaatsen mislukt. | zeker |
| E10 | CMSG_SEND_MAIL | `src/openwow/game/commerce/mail/adapters/protocol/mail_packet_codec.cpp:246-255` | `Handlers/MailHandler.cpp:173-183` | Client stuurt u8 attachment_count + per bijlage (u8 slot, u64 guid); 1.12 leest direct één itemGuid. Alles erna verschuift. | Mail met/zonder bijlage ontkoppelt ontvanger/bijlage/geld; verzenden faalt of verkeerde inhoud. | zeker |
| E11 | SMSG_MAIL_LIST_RESULT | `mail_packet_codec.cpp:57,64` | `Handlers/MailHandler.cpp:891,915-960` | Client leest 3.3.5 u32 real_count + u16 message_size + item-index/guid-low + 3 enchant-triples + body; 1.12 heeft u8 count, geen message_size, geen item-index, 1 enchant, geen body. | Mailbox opent/vult verkeerd. | zeker |
| E12 | CMSG_BUY_ITEM / CMSG_BUY_ITEM_IN_SLOT / CMSG_SELL_ITEM | `src/openwow/net/wotlk/protocol/packet_sender.cpp:862-864,873,836` | `Handlers/ItemHandler.cpp:779,742,552` | Client stuurt u32 vendor-slot + u32 count; 1.12 leest een u8 count (en BUY_ITEM_IN_SLOT geen vendor-slot-u32). | Kopen in hoeveelheid/bag-slot en sell-count worden verkeerd gelezen. | zeker |
| E13 | SMSG_READ_ITEM_OK / SMSG_READ_ITEM_FAILED | `src/openwow/game/inventory/items/item_protocol.cpp:146-160` | `Handlers/ItemHandler.cpp:511-523` | Server stuurt in beide takken nog een tweede guid (OK=16 B, FAILED=17 B); client eist decode_exact met 1 guid (8/12 B). | Boek/pagina-item lezen toont geen tekst. | zeker |
| E14 | SMSG_LOOT_ALL_PASSED / MSG_QUERY_NEXT_MAIL_TIME / SMSG_ITEM_TEXT_QUERY_RESPONSE / SMSG_ENCHANTMENTLOG | `loot_packet_codec.cpp:58-60`; `mail_packet_codec.cpp:162`; `item_protocol.cpp:168-175`; `item_protocol.cpp:25-28` | `Group/Group.cpp:836-837`; `MailHandler.cpp:1067-1069`, `991-992`; `ItemHandler.cpp:1238-1242` | randomPropId/randomSuffix omgewisseld; mail-time alleen float i.p.v. float+u32+senders; item-text u32+string i.p.v. u8+guid+string; enchant-log rauwe u64-guids + extra u8 i.p.v. packed guids. | Loot-all-passed, mailtimer, itemtekst en enchant-log tonen verkeerde/geen data. | zeker |

### F. World / object / UI / overig

| # | opcode | onze file:regel | server file:regel | afwijking | symptoom | zekerheid |
|---|---|---|---|---|---|---|
| F1 | SMSG_INIT_WORLD_STATES | `src/openwow/game/world_state_manager.cpp:25-31` | `Objects/Player.cpp:10019-10023` (+10058-10059, 10085) | Client leest drie i32 (map, zone, area) vóór de u16 count; 1.12 schrijft alleen mapid + zoneid + count. Het 3.3.5 areaId bestaat niet op de 1.12-wire. | Wereldstates (BG-scores, Scourge Invasion, zone-UI) initialiseren nooit; `area_id` en count zijn garbage. | zeker |
| F2 | SMSG_SET_FACTION_STANDING | `src/openwow/game/reputation_info.cpp:497-521`, `faction_manager.cpp:39-67` | `ReputationMgr.cpp:186-208` (ook `Commands/Commands.cpp:15286-15294`) | Client leest 3.3.5 float bonus_rep + u8 increased + u32 count + paren; 1.12 schrijft alleen u32 count + paren. | Faction-standing-updates schuiven 5 bytes op; reputatiebalken en gain-meldingen kloppen niet. | zeker |
| F3 | SMSG_TRANSFER_ABORTED | `src/openwow/game/session/world_transition_controller.cpp:378` | `Objects/Player.cpp:21462-21463` | 1.12 = alleen u8 reason; client eist u32 map_id + u8 reason → ReadU32 faalt. | Transfer-abort-reden wordt nooit getoond. | zeker |
| F4 | SMSG_QUERY_TIME_RESPONSE | `world_transition_controller.cpp:404` | `Handlers/QueryHandler.cpp:448-449` | Server stuurt 1 u32; client eist server_time + daily_reset_secs (2× u32). | Lokale/server-tijdverschil en daily-reset-deadline worden nooit berekend (dag/nacht en reset-timers fout). | zeker |
| F5 | SMSG_DEFENSE_MESSAGE | `include/.../opcodes.h:830` (0x33A) en `:831` (0x33B = SMSG_INSTANCE_DIFFICULTY) | `Protocol/Opcodes_1_12_1.h:828` = 0x33B; body `Maps/Map.cpp:2568-2571` | **Niet gefixt**, ondanks de opdrachtnotitie. 1.12 stuurt defense op 0x33B (u32 zoneId + u32 strlen+1 + tekst); client bezet 0x33B met SMSG_INSTANCE_DIFFICULTY (`instance_session.cpp:314-319` leest twee u32). | Zone-defensebroadcast wordt als instance-difficulty gelezen: difficulty_index=zoneId, player_difficulty_index=strlen; tekst verschijnt nooit. | zeker |
| F6 | SMSG_INSPECT (0x115) | `opcodes.h:282` = SMSG_INSPECT_RESULTS_UPDATE; `src/openwow/game/inspect_handler.cpp:142-150` | `Handlers/MiscHandler.cpp:1074-1076` | 1.12 stuurt alleen een rauwe guid (8 B); client leest een packed guid en roept daarna ParseAndStoreInspectEquipment aan alsof er itemdata in de packet zit. | Inspect-venster toont geen/verouderde uitrusting. | zeker (body); symptoom vermoeden |
| F7 | Meeting-stone/LFG-blok | `opcodes.h:663-670,704`; handlers `world_session.cpp:2100,2697,2835,2954` | `Handlers/LFGHandler.cpp:136-146`; `LFG/LFGMgr.cpp:548-567`; `Group/Group.cpp:478,493,613` | 1.12 gebruikt 0x292-0x299 en 0x2BB voor meeting stones; client heeft daar WotLK-namen (CMSG_SET_SAVED_INSTANCE_EXTEND, SMSG_LFG_OFFER_CONTINUE, CMSG_TEST_DROP_RATE, SMSG_TEST_DROP_RATE_RESULT, CMSG_LFG_GET_STATUS, SMSG_SHOW_MAILBOX, SMSG_RESET_RANGED_COMBAT_TIMER, SMSG_CHAT_NOT_IN_PARTY, SMSG_LFG_ROLE_CHOSEN) en implementeert geen CMSG_MEETINGSTONE_*. | Meeting stones/queue onbruikbaar. SMSG_MEETINGSTONE_COMPLETE (leeg) opent de **mailbox** (`world_session.cpp:2100`); MEMBER_ADDED (u64) meldt "not in party" (`:2697`); JOINFAILED → LFG role-chosen (`:2954`). | zeker |
| F8 | SMSG_GAMEOBJECT_SPAWN_ANIM (0x214) | `opcodes.h:537` = SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT; `decomposed_packet_routing.cpp:199` | `Objects/Object.cpp:2028-2029` | 1.12 stuurt op 0x214 de GO-guid; client behandelt 0x214 als encounter-update. | GO-spawnanimatie voedt het instance-encounter-updatepad. | zeker (nummer); symptoom vermoeden |
| F9 | SMSG_SET_REST_START (0x21E) | `opcodes.h:547` = SMSG_QUEST_FORCE_REMOVE; `decomposed_packet_routing.cpp:271` | `Objects/Player.cpp:21366-21367` | 1.12 stuurt op 0x21E u32 restStateTime; client leest het als quest-id (u32) voor force-remove. | Rest/XP-status wordt niet toegepast; quest-remove-pad krijgt een verkeerde id. | zeker (nummer) |
| F10 | SMSG_ATTACKSWING_NOTSTANDING (0x147) | `opcodes.h:332` = SMSG_INSTANCE_LOCK_WARNING_QUERY; `decomposed_packet_routing.cpp:201` | `Objects/Player.cpp:19188` (lege body) | 1.12 stuurt 0x147 zonder body; client probeert er een instance-lock-warning uit te lezen. | Aanvalsfout raakt het instance-lock-warningpad. | zeker (nummer); symptoom vermoeden |
| F11 | SMSG_BATTLEFIELD_STATUS | `src/openwow/game/battlefield_info.cpp:419-485` | `Battlegrounds/BattleGroundMgr.cpp:1081-1092` | Client leest 3.3.5 (u64 queue-descriptor + u8 arena + u8 rated + u32 instance + u8 registered); 1.12 schrijft u32 slot + u32 mapId + u8 bracket + u32 instance + u32 status. Zelfs de STATUS_NONE-tak (u32 slot + u32 0) faalt omdat de client daarna 8 bytes eist. | BG-queue/status/uitnodiging/clear wordt niet of verkeerd getoond; status blijft hangen. | zeker |
| F12 | SMSG_GMTICKET_GETTICKET | `src/openwow/game/gm_ticket_handler.cpp:64-68` | `GMTicketMgr.cpp:122-135` | Client verwacht na status eerst u32 ticket-id, dán de tekst; 1.12 schrijft direct de tekst-string (geen id). | GM-tickettekst in het venster is verminkt/verschoven. | zeker |
| F13 | CMSG_GMTICKET_CREATE | `src/openwow/game/interaction_sender.cpp:842-849` | `Handlers/GMTicketHandler.cpp:115-117` | Client begint met u32 mapId; server leest eerst u8 ticketType en dán mapId/x/y/z. | Ticketcategorie/positie/tekst verkeerd; ticketcreatie faalt of bevat rommel. | zeker |
| F14 | CMSG_GMTICKET_UPDATETEXT | `interaction_sender.cpp:3477-3480` | `Handlers/GMTicketHandler.cpp:50-52` | Client stuurt alleen de string; server verwacht eerst u8 type. | Eerste tekstbyte wordt als type gelezen; update met verkeerd type/afgekapte tekst. | zeker |
| F15 | SMSG_ACCOUNT_DATA_TIMES | `src/openwow/game/session_handler.cpp:153-166` | `Handlers/CharacterHandler.cpp:781-784` | 1.12 stuurt 32× u32 (128 B platte array); client leest 3.3.5 u32 time + u8 unk + u32 mask + mask-afhankelijke timestamps. | Vorm klopt niet; nu gemaskeerd doordat de server alles 0 stuurt. Account-data (config/bindings/macros) wordt nooit als nieuwer gezien. | zeker |
| F16 | SMSG_CHAT_RESTRICTED | `src/openwow/game/chat_manager.cpp:386-388` | `Handlers/ChatHandler.cpp:1031` | Server stuurt een leeg pakket; client eist 1 byte. | Chat-restrictie/mute-melding wordt niet geregistreerd. | zeker |
| F17 | SMSG_ADDON_INFO | `src/openwow/net/wotlk/addon_handshake.cpp:982-985` | `Handlers/AddonHandler.cpp:139-171` | Per-addon-deel klopt, maar client verwacht na de lijst nog u32 catalog_count + entries; 1.12 eindigt direct. | ProcessServerInfo kan false teruggeven → addon-handshake als mislukt beschouwd. | vermoeden |

### Reeds als "gefixt" gemarkeerd — gecontroleerde status

| item | status | bewijs |
|---|---|---|
| SMSG_MESSAGECHAT flags + receiver-guid | gefixt; body klopt | client `chat_manager.cpp:85-144` vs server `Chat/Chat.cpp:2290-2340` |
| Chat-tag enkelvoudige waarde (NONE/AFK/DND/GM) | gefixt | client `chat_manager.cpp:142-144` leest één u8 |
| SMSG_QUEST_QUERY_RESPONSE 3.3.5-velden | gefixt | client `quest_manager.cpp:817-821` slaat xp/honor/faction-arrays over |
| SMSG_QUESTGIVER_QUEST_LIST | gefixt; veld voor veld gelijk | client `session/quest_session.cpp:655-694` vs server `GossipDef.cpp:432-491` |
| SMSG_UPDATE_AURAS / SMSG_SET_EXTRA_AURA_INFO | terecht afwezig; client heeft alleen WotLK `..._OBSOLETE` (0x3A4/0x3A5) | `opcodes.h:936-937`, `world_session.cpp:3130-3131` |
| Create-blok: 6 SpeedInfo-waarden | gefixt | client `update_object_parser.cpp:233-238` leest 6 floats in een 9-slots tabel |
| SMSG_CAST_RESULT | gefixt; 1.12-vorm | client `spell_packets.cpp:274-296` (u32 spell, u8 status, u8 reason) |
| CMSG_SETSHEATHED | body klopt (u32) | client `packet_sender.cpp:1843-1846` vs server `CombatHandler.cpp:85-86` |
| CMSG_TOGGLE_HELM/CLOAK | server leest de body niet | server `CharacterHandler.cpp:1230,1236` |
| CMSG_JOIN_CHANNEL / CMSG_LEAVE_CHANNEL | **niet gefixt** — zie C11 | client `chat_manager.cpp:41-68` |
| SMSG_DEFENSE_MESSAGE op 0x33B | **niet gefixt** — zie F5 | client `opcodes.h:830-831` |
| SpellVisualEffectName 5-koloms Classic-schema | buiten packet-scope (DBC-audit; hier niet onderzocht) | n.v.t. |

---

## (b) Top-10 naar gameplay-impact

1. **SMSG_FORCE_RUN_SPEED_CHANGE** — élke door de server geforceerde snelheidswijziging (sprint, slow, charge) wordt stil gedropt. `world_session_movement.cpp:319-331` vs `Movement/MovementPacketSender.cpp:62-64`.
2. **SMSG_GROUP_LIST** — party/raid-lijst volledig misgeparsed. `group_manager.cpp:15-43` vs `Group/Group.cpp:1422-1436`.
3. **SMSG_PARTY_MEMBER_STATS / _FULL** — party-health/mana/auras fout. `party_stats.cpp:187-197` vs `GroupHandler.cpp:670-677`.
4. **CMSG_PET_CAST_SPELL + SMSG_PET_SPELLS** — pet-actiebalk en pet-abilities (hunter/warlock) werken niet. `packet_sender.cpp:545-551` vs `PetHandler.cpp:601`; `pet_manager.cpp:290-294` vs `Player.cpp:19554-19560`.
5. **SMSG_QUESTGIVER_QUEST_COMPLETE** — quest inleveren verwerkt nooit. `quest_session.cpp:637-642` vs `Player.cpp:16575-16593`.
6. **MSG_CHANNEL_START / MSG_CHANNEL_UPDATE** — channeled spells (castbar/animatie) breken. `spell_packets.cpp:332-351` vs `Spell.cpp:5086-5087,5132-5134`.
7. **SMSG_TRADE_STATUS / SMSG_TRADE_STATUS_EXTENDED** — trade-venster opent nooit en itemdata schuift. `trade_packet_codec.cpp:55-58,118-122` vs `TradeHandler.cpp:50-52,110-111`.
8. **MSG_AUCTION_HELLO + SMSG_AUCTION_LIST_RESULT** — auction house opent/vult niet. `auction_packet_codec.cpp:17-19,41-52` vs `AuctionHouseHandler.cpp:76-78,738` + `AuctionHouseMgr.cpp:997-1005`.
9. **SMSG_MAIL_LIST_RESULT + CMSG_SEND_MAIL** — mailbox lezen én verzenden breken. `mail_packet_codec.cpp:57,64,246-255` vs `MailHandler.cpp:891,915-960,173-183`.
10. **SMSG_LOOT_START_ROLL / SMSG_LOOT_ROLL + SMSG_ITEM_PUSH_RESULT** — need/greed en item-feedback verdwijnen. `loot_roll_packet_codec.cpp:16-24,44` vs `Group/Group.cpp:763-769,786-794`; `inventory_messages.cpp:8,28` vs `Player.cpp:14127-14138`.

**Direct daarna**: SMSG_SPELLHEALLOG (geen heal-feedback), SMSG_INIT_WORLD_STATES (BG/zone-states), SMSG_SET_FACTION_STANDING (reputatie), CMSG_JOIN/LEAVE_CHANNEL (kanalen), SMSG_GROUP_INVITE (invites), CMSG_BUY_ITEM/SELL_ITEM (vendor), SMSG_BATTLEFIELD_STATUS (BG-queue), CMSG_ACTIVATETAXIEXPRESS (taxi), SMSG_SPELL_DELAYED (pushback), MSG_AUCTION/AH en SMSG_FRIEND_LIST (sociaal).

---

## (c) Opcodes die in 1.12 ontbreken of een ander nummer hebben

De client-enum is voor alle **gedeelde namen** al op de 1.12-nummers gezet: 0 van de
gedeelde namen heeft een afwijkend nummer. De afwijkingen zitten in (1) namen die in
1.12 een ánder contract op hetzelfde nummer hebben en (2) 1.12-namen die in de client
ontbreken.

| 1.12-opcode | nummer | client-naam op dat nummer |
|---|---|---|
| SMSG_UPDATE_AURA_DURATION | 0x137 | SMSG_EQUIPMENT_SET_SAVED (`opcodes.h:316`) |
| SMSG_DEFENSE_MESSAGE | 0x33B | SMSG_INSTANCE_DIFFICULTY (`opcodes.h:831`) — client zet defense op 0x33A |
| SMSG_FRIEND_LIST | 0x67 | SMSG_CONTACT_LIST (`opcodes.h:110`) |
| SMSG_IGNORE_LIST | 0x6B | CMSG_SET_CONTACT_NOTES (`opcodes.h:114`) |
| SMSG_INSPECT | 0x115 | SMSG_INSPECT_RESULTS_UPDATE (`opcodes.h:282`) |
| SMSG_CAST_RESULT | 0x130 | SMSG_CAST_FAILED (`opcodes.h:309`) — body wél op 1.12 gezet |
| SMSG_SERVER_MESSAGE | 0x291 | SMSG_CHAT_SERVER_MESSAGE (`opcodes.h:662`) — body klopt (u32 + cstring) |
| SMSG_ENVIRONMENTALDAMAGELOG | 0x1FC | SMSG_ENVIRONMENTAL_DAMAGE_LOG (`opcodes.h:513`) |
| CMSG_SETSHEATHED | 0x1E0 | CMSG_SET_SHEATHED (`opcodes.h:485`) — body klopt (u32) |
| CMSG_TOGGLE_HELM / CMSG_TOGGLE_CLOAK | 0x2B9 / 0x2BA | CMSG_SHOWING_HELM / CMSG_SHOWING_CLOAK — server leest body niet |
| CMSG_FRIEND_LIST | 0x66 | CMSG_CONTACT_LIST — server leest niets (`MiscHandler.cpp:574-578`) |
| MSG_MOVE_SET_RAW_POSITION_ACK | 0xE0 | CMSG_MOVE_CHARM_PORT_CHEAT |
| SMSG_GAMEOBJECT_SPAWN_ANIM | 0x214 | SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT |
| SMSG_SET_REST_START | 0x21E | SMSG_QUEST_FORCE_REMOVE |
| SMSG_ATTACKSWING_NOTSTANDING | 0x147 | SMSG_INSTANCE_LOCK_WARNING_QUERY |
| SMSG_BATTLEFIELD_WIN / SMSG_BATTLEFIELD_LOSE | 0x23F / 0x240 | SMSG_FORCE_SET_VEHICLE_REC_ID / CMSG_SET_VEHICLE_REC_ID_ACK |
| SMSG_STANDSTATE_CHANGE_FAILURE | 0x261 | SMSG_COMBAT_EVENT_FAILED |
| SMSG_MEETINGSTONE_SETQUEUE / COMPLETE / IN_PROGRESS / MEMBER_ADDED / JOINFAILED | 0x295 / 0x297 / 0x298 / 0x299 / 0x2BB | SMSG_TEST_DROP_RATE_RESULT / SMSG_SHOW_MAILBOX / SMSG_RESET_RANGED_COMBAT_TIMER / SMSG_CHAT_NOT_IN_PARTY / SMSG_LFG_ROLE_CHOSEN |
| CMSG_MEETINGSTONE_JOIN / LEAVE / INFO | 0x292 / 0x293 / 0x296 | CMSG_SET_SAVED_INSTANCE_EXTEND / SMSG_LFG_OFFER_CONTINUE / CMSG_LFG_GET_STATUS |

**Server-only opcodes die géén praktisch probleem zijn** (gecontroleerd): `MSG_UPDATE_GROUP_MEMBERS` (0x80) en `MSG_UPDATE_GUILD` (0x94) zijn server-side `Handle_NULL` (`Opcodes.cpp:184,204`); `CMSG_DROP_ITEM` (0x110) is `Handle_NULL` (`Opcodes.cpp:328`). De WotLK-uitbreidingen in de client (achievements, calendar, voice, dance, vehicles, WotLK-LFG, guild bank) staan op hoge, in 1.12 ongebruikte nummers en leveren geen mis-dispatch op zolang de server ze niet stuurt.

**Ook gecontroleerd en gelijk** (geen bevinding): SMSG_UPDATE_OBJECT/COMPRESSED_OBJECT bloklayout incl. Classic hasTransport-byte (`update_object_parser.cpp:430` vs `UpdateData.cpp:104`), UPDATEFLAG-bits, SMSG_DESTROY_OBJECT, SMSG_LOGIN_VERIFY_WORLD/NEW_WORLD/TRANSFER_PENDING, MSG_MOVE_*-header en *_ACK-vorm, SMSG_CHAR_ENUM (`realm_connection_packets.cpp:72-171` vs `Player.cpp:2303-2390`), SMSG_ITEM_QUERY_SINGLE_RESPONSE (10 stats/5 damages), SMSG_GOSSIP_*, SMSG_NPC_TEXT_UPDATE, SMSG_TRAINER_*, SMSG_TAXINODE_STATUS, CMSG_GOSSIP_*, CMSG_TRAINER_*, SMSG_SPELL_START/GO, SMSG_INITIAL_SPELLS, SMSG_ACTION_BUTTONS, SMSG_ENVIRONMENTALDAMAGELOG-body, CMSG_PING/SMSG_PONG, SMSG_EMOTE/SMSG_TEXT_EMOTE, SMSG_ITEM_TEXT_QUERY_RESPONSE-vorm (vorm), Warden-opcode-enum.

---

## Bronbestanden

- Client-opcodetabel: `include/openwow/network/protocol/wotlk/opcodes.h`
- Client-dispatch: `src/openwow/game/world_session.cpp`, `src/openwow/game/session/decomposed_packet_routing.cpp`
- Client-handlers: `src/openwow/game/**`, `src/openwow/net/wotlk/**`
- Server-opcodetabel + bodies: `D:\OllieWoW\Source\src\game\Protocol\Opcodes_1_12_1.h` en de per-bevinding genoemde bestanden.
