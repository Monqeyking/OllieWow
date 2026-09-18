# Benilla-afwijkingen: audit + werkvoorraad

Status: levende checklist.
Laatste update: 2026-09-15 (NPC-interactionflags, tooltip-ownership).

Dit document is de werkvoorraad voor het gelijktrekken van onze client met de
Vanilla/Turtle-referentie. Het is opgesteld met acht parallelle read-only audits
die per domein `D:\OllieWoW\benilla` (referentie) naast deze worktree legden.

## Bronprioriteit en werkwijze

1. `D:\OllieWoW\Client` — lokale client-XML/Lua: leidend voor UI-flow, widgets,
   eventnamen, enumeraties, texturen.
2. `D:\OllieWoW\Source` — lokale server: leidend voor packetbodies,
   update-fieldindices en opcodewaarden.
3. `D:\OllieWoW\benilla` — referentie voor ontbrekende clientlogica
   (binary-geverifieerd waar het dat zegt).
4. Deze worktree — implementatiebasis; bestaande 3.3.5-aannames zijn géén
   contract.

Praktisch: elke bevinding hieronder is door een subagent aan beide kanten
gelezen. Vóór het bouwen wordt de bevinding zelf nagerekend (minstens één is al
als onjuist afgevoerd, zie "Vervallen bevindingen").

Bouwen en testen:

- `build\release\gen-rebuild.ps1 -Only '<pad-regex>$',...` — de regex wordt op de
  cl.exe-regel gematcht, dus gebruik padankers (`game[\\/]chat_manager\.cpp$`);
  een los woord als `chat` matcht niets en dan gebeurt er stil niets.
- `build\release\rebuild-all.cmd` — daarna; faalt met `LNK1104` als de client nog
  draait.
- Testrun: `build\release\apps\client\Start-OllieWoW-Log.cmd` (`--log-level info`;
  het standaardniveau `kError` verbergt alles behalve fouten).
- Voor wire-bewijs: `PacketLog::Get().SetEnabled(true)` is als enige plek in
  `apps/client/composition/main.cpp` te zetten (de CVar `packetLog` wordt gelezen
  vóór de config geladen is) en schrijft `D:\OllieWoW\logs\PacketLog.txt`.

Centrale registratie- en materialisatieplekken (uit de eerdere UI-audit, nog
steeds geldig): `src/openwow/ui/production_lua_surface.cpp`,
`src/openwow/ui/game/api/framexml_*_native_bindings.cpp`,
`src/openwow/ui/game/runtime/world_lua_runtime.cpp`,
`src/openwow/ui/game/framescript/core/frame_method_registry.cpp`,
`src/openwow/ui/game/framescript/xml/frame_xml_loader.cpp`,
`src/openwow/ui/glue/interleaved_toc_processor.cpp`.

Een onderdeel mag pas weg wanneer de lokale Lua/XML het niet gebruikt, geen
actieve addon het gebruikt, geen gameplay-/protocolcode ervan afhangt, de client
zonder dat onderdeel bouwt en de smoke-test schoon blijft. Bestandsnamen met
`wotlk`, `retail` of `3.3.5` zijn op zichzelf geen bewijs dat iets weg kan.

## Batches

### Batch 1 — protocolbodies en tabelindices (bezig)

Dezelfde soort fout: wij lezen/schrijven 3.3.5-vormen waar de lokale server 1.12
is. Klein werk, groot bereik.

- [x] **SMSG_MESSAGECHAT 1.12-body + chattype-mapping** — `flags`-veld en
      receiver-guid weg, koppen per type zoals `Source Chat.cpp:2290-2340`;
      wire↔enum-tabel in `chat_types.h` (`kChatMsgWireTable`), 54 1.12-chatevents
      toegevoegd aan `game_events.h` en `script_event_catalog.cpp`; verzenden
      gebruikt nu ook het wire-nummer. Gebouwd 22:42 (LINK_EXIT=0).
- [x] **Chat-tag 1.12** — de server stuurt de tag als enkelvoudige waarde
      (`Source Chat.h:83-89`: NONE=0, AFK=1, DND=2, **GM=3**), wij lazen de
      3.3.5-bitmask, waardoor elk bericht van een GM-account "DND" op de kop
      kreeg. `ChatTagFromLegacyWire()` vertaalt nu aan de parsegrens. Gebouwd
      22:53.
- [x] **Kleurcodes in de tekstballon** — de server wikkelt GM-chat in
      `|c1049e6ff…|r` (`Source Chat.cpp:2283`); `NormalizeSpeechBubbleText()`
      stript die escapes nu voor alle chattypen (de ballon tekent platte tekst).
- [x] **SMSG_GOSSIP_MESSAGE / CMSG_GOSSIP_SELECT_OPTION** — 1.12-body zonder
      menuId/box-velden. Al gedaan in `f74c613` (2026-09-14 17:24), dus vóór de
      laatste docupdate; hier alsnog nagerekend tegen de server.
      `GossipManager::HandleGossipMessage` leest guid, textId, gossipCount,
      {index, icon, coded, cstring} , questCount, {questID, icon, level, cstring}
      — gelijk aan `Source GossipDef.cpp:156-221`. `BuildGossipSelectOption`
      stuurt guid + gossipListId (+ code), gelijk aan
      `Source Handlers/NPCHandler.cpp:475-481`.
- [x] **SMSG_LIST_INVENTORY** — 7 velden per item, niet 8. Al gedaan in `f74c613`.
      Bewijs: `Source Handlers/ItemHandler.cpp:920` reserveert
      `8 + 1 + numitems * 7 * 4`, en de veldorde in het voorbeeldblok op
      `:931-937` is index, itemId, displayInfoID, aantal, prijs, maxDurability,
      buyCount — precies wat `HandleListInventory` leest. Het lege geval
      (count 0 + reden-byte, `:907-911`) wordt ook gedekt.
- [ ] **CMSG_JOIN_CHANNEL / CMSG_LEAVE_CHANNEL** — 1.12-kop is alleen
      naam+wachtwoord (`Source ChannelHandler.cpp:30-40`, `:74`). Symptoom:
      kanaalnaam verschoven met 4-7 bytes. Ons: `chat_manager.cpp:41-68`.
- [ ] **SMSG_DEFENSE_MESSAGE** — hoort op `0x33B`, niet `0x33A`
      (`Source Opcodes_1_12_1.h:828`). Symptoom: BG-berichten komen nooit aan en
      `0x33B` gaat naar de instance-difficulty-handler. Ons: `opcodes.h:830-831`,
      `decomposed_packet_routing.cpp:179`.
- [ ] **Snelheidstabel** — het create-blok stuurt 6 `SpeedInfo`-waarden
      (walk, run, run_back, swim, swim_back, turn_rate) en die worden 1-op-1 in
      onze 9-slots tabel gezet, dus turn rate belandt in de flight-slot en
      `kSpeedTurnRate` blijft 0. Symptoom: A/D draaien doet lokaal niets terwijl de
      wire "ik draai" zegt. Ons: `update_object_parser.cpp:227-238`,
      `unit_movement_runtime.cpp:847-862`, `object_types.h:125-134`; server:
      `Source Object.cpp:455-460`, `UnitDefines.h:22-30` (MOVE_TURN_RATE = 5).
- [x] **Unit-field → event-indices** — OPGELOST 2026-09-15 (staat in de werkmap,
      nog niet gecommit). De stale tabel is verwijderd; `MapChangedFieldsToEvents`
      is nu de enige bron. Before/after meting met dezelfde dertien events en een
      echte targetwissel: de set die vuurt is **identiek** (`UNIT_HEALTH`,
      `UNIT_MAXHEALTH`, `UNIT_MANA`, `UNIT_STATS`, `UNIT_FLAGS`, `UNIT_FACTION`,
      `UNIT_TARGET`). Dus geen regressie, en die drie verdachte events kwamen
      aantoonbaar uit de mapper, niet uit de verwijderde tabel.
      **Meetvalkuil die dit bijna verkeerd liet concluideren**: een losse
      `runfile`-aanroep kan stil niet aankomen. `ClearTarget()` leek daardoor
      kapot (target bleef bestaan), terwijl dezelfde aanroep in één chunk met de
      observatie erbij wél werkt. Doe actie en observatie in dezelfde Lua-chunk
      voordat je "geen effect" als bevinding opschrijft.
      Achtergrond en de oorspronkelijke analyse:
      hieronder beschreven. `s_unit_field_event_names` is de **3.3.5-layout**
      (incl. `UNIT_RUNIC_POWER`, dat 1.12 niet heeft) en `field_index` is de
      **absolute** veldindex (`byte_offset >> 2`), dus vergeleken met ons eigen
      `update_fields.h` staat elk nuttig item te laag: UNIT_TARGET 12 i.p.v. 16
      (`OBJECT_END + 0x0A`), UNIT_HEALTH 18 i.p.v. 22 (`+ 0x10`), UNIT_LEVEL 48
      i.p.v. 34 (`+ 0x1C`). Gevolg: absolute 18 is in 1.12
      `UNIT_FIELD_PERSUADED`, dus een persuade-wijziging vuurt UNIT_HEALTH; en de
      echte health-index 22 is hier `UNIT_ENERGY`.
      **Twee live paden naast elkaar**: `update_field_event_mapper.cpp`
      (`MapChangedFieldsToEvents`, aangeroepen op elke object-update in
      `world_session_object.cpp:2403`) doet het **wel** goed, met benoemde
      constanten; `player_unit_field_event_callbacks.cpp:250-267` registreert
      daarnaast per tabelitem een descriptor-callback. Het stale pad voegt dus
      spook-events toe bovenop de correcte.
      Extra: `kUnitFieldEventSlotCount = 142` terwijl 1.12's unit-blok tot
      `UNIT_END = 188` loopt, dus alles vanaf absolute index 142 is via dit pad
      **onbereikbaar** — o.a. UNIT_DYNAMIC_FLAGS (143), STAT0..4 (150-154) en
      RESISTANCES (155-161).
      **Fix-richting (kies bij oppakken)**: ofwel de tabel regenereren uit
      `update_fields.h` met absolute 1.12-indices en
      `kUnitFieldEventSlotCount` op `UNIT_END` zetten, ofwel de dubbele
      registratie in `player_unit_field_event_callbacks.cpp` laten vervallen nu
      de mapper alles dekt. Het eerste raakt ook
      `script_event_helpers.cpp:781-901`, dat dezelfde tabel gebruikt.
- [ ] **SpellVisualEffectName** — WotLK-7-koloms schema op de 5-koloms
      Classic-tabel (`dbc_structures.cpp:374-382` leest schaal op veld 5-6 die 0
      blijft) → `ceffect_c.cpp:126-132` klemt elke effect-schaal op 0 en valt
      terug op 1.0x. Alle attach-VFX staan op 1x.
- [x] **NPC-interactionflags naar 1.12** - de cursor- en dispatchtabellen gebruikten
      3.3.5-bits (VENDOR `0x80`, REPAIR `0x1000`, FLIGHTMASTER `0x2000`,
      INNKEEPER `0x10000`, BANKER `0x20000`, AUCTIONEER `0x200000`) terwijl 1.12
      `0x04`/`0x4000`/`0x08`/`0x80`/`0x100`/`0x1000` gebruikt (`Source
      UnitDefines.h:445-460`, benilla `cursor_mode.rs:150-163`). Symptoom:
      rechtsklik op een vendor deed niets, want de VENDOR-tak vuurde nooit.
      Gewijzigd in `cursor_surface.h`, `game_loop.cpp`,
      `unit_interaction_runtime.cpp`, `taxi_session.cpp`,
      `world_session_object.cpp`, `game_lua_api_pvp.cpp`,
      `petition_session_handlers.cpp`, `npc_interaction_controller.cpp`.
      **Runtime-bevestiging van vendor/questgiver staat nog open.**

#### Batch 1b — volledige protocol-audit 3.3.5 vs 1.12 (2026-09-17)

Volledige inventaris: **`docs/PROTOCOL_AUDIT_335_VS_112.md`** (198 regels, ~60
bevindingen, elk met client- en server-file:regel). Systematisch: client-opcodetabel
(`include/openwow/network/protocol/wotlk/opcodes.h`, 1307 entries) machinematig
vergeleken met `Source\src\game\Protocol\Opcodes_1_12_1.h` (827), daarna de
bodies veld voor veld tegen de server-`SendPacket`/`recvPacket`-code.

**Klassen van afwijking** (dit is het nuttigste stuk: dezelfde soort fout, veel
plekken):

1. **Verkeerd opcodenummer** — ~25 botsingen. Het pakket gaat naar de verkeerde
   handler, soms met een lege body. Voorbeelden: `0x137` is bij ons
   `SMSG_EQUIPMENT_SET_SAVED` maar 1.12 `SMSG_UPDATE_AURA_DURATION`; `0x33B` is bij
   ons `SMSG_INSTANCE_DIFFICULTY` maar 1.12 `SMSG_DEFENSE_MESSAGE`; het
   meeting-stone-blok `0x292-0x299`/`0x2BB` botst met WotLK-LFG, waardoor
   `SMSG_MEETINGSTONE_COMPLETE` de **mailbox** opent.
2. **Extra veld dat 1.12 niet stuurt** (of andersom) — meestal een paar regels;
   de parser faalt op `Remaining()==0` of schuift een veld op.
3. **Packed guid vs rauwe u64** — één helper lost een hele reeks op.
4. **3.3.5-only restvelden** — onschuldig zolang de client afwezigheid tolereert.

**Top-10 naar gameplay-impact** (detail + file:regels in het rapport):

1. `SMSG_FORCE_RUN_SPEED_CHANGE` — élke gedwongen snelheidswijziging (sprint, slow,
   charge) wordt stil gedropt (`world_session_movement.cpp:319-331`).
2. `SMSG_GROUP_LIST` — party/raid-lijst volledig misgeparsed.
3. `SMSG_PARTY_MEMBER_STATS`/`_FULL` — hp/mana/auras fout, masker vanaf bit 10
   verschoven.
4. `CMSG_PET_CAST_SPELL` + `SMSG_PET_SPELLS` — petbalk en pet-abilities stuk.
5. `SMSG_QUESTGIVER_QUEST_COMPLETE` — de COMPLETE-melding wordt altijd verworpen.
   Let op: de inlever-flow zelf loopt via `SMSG_QUESTGIVER_OFFER_REWARD` en werkt;
   dit gaat om de melding/log-verversing daarna.
6. `MSG_CHANNEL_START`/`MSG_CHANNEL_UPDATE` — channeled casts en castbar kapot.
7. `SMSG_TRADE_STATUS`(`_EXTENDED`) — trade-venster opent nooit.
8. `MSG_AUCTION_HELLO` + `SMSG_AUCTION_LIST_RESULT` — AH opent/vult niet.
9. `SMSG_MAIL_LIST_RESULT` + `CMSG_SEND_MAIL` — mailbox lezen én verzenden stuk.
10. `SMSG_LOOT_START_ROLL`/`_ROLL` + `SMSG_ITEM_PUSH_RESULT` — need/greed en
    item-feedback weg.

**Daarna**: `SMSG_SPELLHEALLOG`, `SMSG_INIT_WORLD_STATES`,
`SMSG_SET_FACTION_STANDING`, `CMSG_JOIN`/`LEAVE_CHANNEL`, invites, vendor
kopen/verkopen, `SMSG_BATTLEFIELD_STATUS`, taxi, pushback, vriendenlijst.

**Correctie op eerdere notities in deze batch**: de audit controleerde de items die
hier als gefixt stonden. `CMSG_JOIN_CHANNEL`/`CMSG_LEAVE_CHANNEL` en
`SMSG_DEFENSE_MESSAGE` staan **terecht nog open** (zie de twee `[ ]`-punten
hierboven); de rest (messagchat, chat-tag, quest-query-response,
questgiver-quest-list, SpeedInfo-tabel, cast-result) is veld voor veld bevestigd.

**Bevestigd goed** (geen bevinding): `SMSG_UPDATE_OBJECT`/`COMPRESSED_OBJECT`
bloklayout incl. Classic hasTransport-byte, `SMSG_CHAR_ENUM`,
`SMSG_ITEM_QUERY_SINGLE_RESPONSE`, `SMSG_GOSSIP_*`, `SMSG_TRAINER_*`,
`SMSG_TAXINODE_STATUS`, `SMSG_SPELL_START/GO`, `SMSG_INITIAL_SPELLS`,
`SMSG_ACTION_BUTTONS`, `LOGIN_VERIFY_WORLD`, `MSG_MOVE_*`-headers, `CMSG_PING`.

### Batch 2 - visueel en ruimtelijk

- [ ] **Unit-schaal dubbel toegepast** — `cgobject.h:231`
      (`native_scale_ * SCALE_X`) plus `object_renderer.cpp:397,2251`
      (`* GetCreatureModelScale`) tegen benilla `entities/attach/mod.rs:730-736`
      (render = `OBJECT_FIELD_SCALE_X`, DBC er bovenop = native²). Symptoom:
      creatures te groot/klein, Tauren ~2.46 i.p.v. 1.35, totems verkeerd.
- [ ] **Mount-schaal** — berijder-`SCALE_X` ontbreekt en `CMD.modelScale` telt
      onterecht mee (`mount_renderer.cpp:219-222,412` vs benilla `mount.rs:170-175`).
- [x] **Helm-modelpad** — racetoken hoort `Ta`/`Hu`/`Or`/… te zijn
      (benilla `equipment/mod.rs:309-316`), wij gebruikten
      `chr_races.model_client_prefix` ("Tauren"). Bewijs: log
      `Head/Helm_Leather_D_01_TaurenM.mdx ... reason=4` terwijl het bestand
      `Helm_Leather_D_01_TaM.m2` heet. Nieuw:
      `EquipmentVisualSystem::HelmRaceFilePrefix()`, gebruikt in
      `equipment_renderer.cpp` (wereld) en `glue_charselect_scene.cpp`
      (characterselect). Gebouwd 23:01 — **test ook of karakter-model NPC's
      (disguises/illusion-NPC's) hiermee zichtbaar worden**.
- [ ] **Paint-order per (strata,level)-laagbucket** — de client emit laag voor
      laag over álle frames (`benilla-ui order.rs:160-176`), wij groeperen per
      frame (`ui_paint_order.h:97-209`). Symptoom: zelfde-level parent/kind en
      pfUI-backdrops tekenen in de verkeerde volgorde.
- [ ] **Zwevende combat text**: heals horen niet te zweven (1.12), lettergrootte
      volgt de 4:3-wet i.p.v. de diagonaal (`world_overlay_metrics.h:38-43`),
      Absorb/Resist/Miss-woorden ontbreken (`combat_log_messages.cpp:89-91`), cap
      is 64 globaal i.p.v. 4 per unit (`floating_text.cpp:296-302`), anker is
      voet+2.5y i.p.v. model-overhead (`game_loop.cpp:3046-3051`).

### Batch 3 — UI- en API-contracten

- [x] **Chatweergave 1.12** — chattag als enkelvoudige waarde (`ChatTagFromLegacyWire`),
      kleurcodes uit de tekstballon, alpha-byte van tekstkleuren genegeerd
      (`bgfx_text_cache.cpp:98` + `ui/widgets/simple_html.cpp`), en de GM-kleurwrap
      van de server (`|c1049e6ff…|r`, `Source Chat.cpp:2283`) er af zodat say wit
      en yell rood blijft.
- [x] **Chatregel valt buiten de box** — opgelost met `TextRenderer::WrapRichText`
      (woordwrap + karakter-fallback) in `render/ui/chat_frame_presenter.cpp`; de
      presenter tekende elke regel eerst in één keer zonder breedtelimiet.
- [x] **Tekst-emote toont de afzender dubbel** — `GetPrefix` (0x0B) geeft nu geen
      prefix meer: de tekst-emote bevat de afzender al in de zin.
- [x] **Rode invoerkleur bij `/yell`** — de invoerregel neemt nu de kleur van het
      gekozen kanaal (`GetColorForType` per `ChatMode`).
- [x] **Tekstballon te groot** — de tekst schaalde met de schermdiagonaal
      (`diagonal / 1280`), de rest van de ballon met de UI-schaal; nu volgt de
      tekst `ui_scale` (zelfde factor als randen/tegels/staart).
- [x] **Humanoïde NPC's onzichtbaar (en zonder uitrusting)** — de echte oorzaak zat
      in het **laadprofiel**, niet in de data of de schema's:
      `DbcLoader::IsClassicMvpDbc()` (`data/formats/dbc/dbc_loader.cpp:63-146`)
      laadt alleen een hardgecodeerde allowlist van 80 tabellen, en
      `CreatureDisplayInfoExtra.dbc` stond daar niet op. Die store bleef dus leeg
      ("65 geladen, 0 failed" maar deze zat er niet bij), `LookupEntry(extra_info)`
      gaf altijd nullptr, en daarmee kreeg een karakter-model NPC géén look — geen
      vervangbare huid (een karakter-M2 levert die niet zelf) én geen
      `Equipment0..9`-uitrusting. Nu staat de tabel erop (81). Verificatie met
      Benilla's eigen 1.12-schema: rij 3152 = Race 2, Sex 0, Skin 5, Face 8,
      Equipment[2..] = 13349/9536/6299/… en een bake-atlas.
      Twee hulplijnen erbij, zodat dit niet opnieuw onzichtbaarheid oplevert:
      (a) karakterlichaam wordt óók aan het modelpad herkend (`Character\…`, zoals
      Benilla `entities/display.rs:292-295`), en (b) zonder bruikbare extra-rij
      worden ras/geslacht uit het modelpad afgeleid met een default-look.
      Ook: een mislukte cosmetische textuur ontkoppelt het model niet meer, en een
      nog niet klare compositie houdt het tekenen niet tegen
      (`render/scene/object_renderer.cpp`).

- [ ] **`ChangeActionBarPage()` bare aanroep** — 1.12-FrameXML roept hem zonder
      argument (`Client-artifacts/framexml/ActionBarFrame.xml:183-211`); onze
      binding eist een argument en doet dan niets (`game_lua_api_action.cpp:2975-2996`).
      Symptoom: engine blijft op pagina 1 (keybinds, `[actionbar:N]`-macro's).
- [ ] **Aura-API** — `GetPlayerBuff*` is een lege stub
      (`game_lua_api_unit.cpp:2036-2063`) → pfUI-buffbalk leeg, CancelPlayerBuff
      dood; `UnitBuff/UnitDebuff` geven de Era-11-tuple terwijl de lokale 1.12-Lua
      `(icon, stacks)` op positie 1-2 leest.
- [ ] **Actieslots van vorm/pet** — wij schrijven ze in 121-132
      (`game_lua_api_action.cpp:180-199`, `generated_action_bar.h:74-94`), de
      balkformule adresseert 73-108 (`benilla ActionBar.xml:402-414`). Symptoom:
      vorm-/stancebalk blijft leeg.
- [ ] **Keybind-dispatch** — `ACTIONBUTTON`-prefix stuurt `Click` in plaats van
      `ActionButtonDown/Up` en `BONUSACTIONBUTTON` bestaat niet
      (`game_loop.cpp:253-262,392-427`); petbalk-toetsen (CTRL-1..0) doen niets.
- [ ] **`SetPoint(nil, …)` / `SetAllPoints(nil)`** ankeren aan de parent, niet aan
      UIParent (benilla `layout_methods.rs:344-412`; ons
      `frame_anchor_methods.cpp:490-522,634`).
- [ ] **`SetAlpha`** zet in de client de hele subtree op de absolute waarde
      (benilla `propagation.rs:226-242`); wij vermenigvuldigen pas bij het tekenen
      (`frame_base_methods.cpp:247-278`, `ui_compositor.cpp:98-130`).
- [ ] **`$parent` zonder benoemde voorouder** hoort `"Top"` te zijn (benilla
      `framexml.rs:436-469`; ons `frame_runtime_shared.cpp:646-688`).
- [ ] **`SetFrameLevel`** kapt de delta af op 128 (`frame_method_helpers.cpp:104-118`),
      waardoor expliciete levels 128/255 samenvallen.
- [ ] **Eventcatalogus** — `RegisterEvent` negeert onbekende namen stil
      (`frame_event_methods.cpp:195`) en de catalogus mist nog events die lokale
      addons registreren (o.a. `UNIT_CASTEVENT`, `CRAFT_SHOW`,
      `MINIMAP_ZONE_CHANGED`, `UI_SCALE_CHANGED`).
- [x] **`GameTooltip:IsOwned()` bleef waar na `Hide()`** - vanilla's
      `GameTooltip_Hide()` zet `this.owner = nil`; wij lieten het Lua-veld
      `__ow_tooltip_owner_frame` staan. 1.12 `ContainerFrame.lua:693`
      (`ContainerFrameItemButton_OnUpdate`) hertoont de tooltip zolang
      `IsOwned(this)` waar is, dus een backpack-item-tooltip kwam elke
      `TOOLTIP_UPDATE_TIME` terug (~230 ms gemeten) en was niet weg te krijgen.
      `IsOwned` eist nu ook `TooltipSystem::IsShown()`
      (`frame_tooltip_methods.cpp`). Bewijs: de echte FrameXML is met
      `--dump-vfs-file` uit de client-MPQ gehaald.
      **Dit is een familie, geen losse bug**: elke Lua-zichtbare methode waarvan
      het contract afwijkt laat vanilla-FrameXML stil in een lus of verkeerde tak
      lopen. Zie ook `SetPoint(nil)`, `SetAlpha` en `SetFrameLevel` hierboven.

### Batch 4 - beweging, persistentie en rest

- [ ] **`FALLING` ontbreekt in `kActiveMoverMotionMask`** (`player_move_event.h:954`)
      → van een rand lopen stuurt geen enkel pakket meer, dus geen
      `MOVEFLAG_FALLINGFAR` airborne en geen valschade (`Source Player.cpp:23016-23046`).
- [ ] **Zwemsprong/uit-water-breken** — `TryStartJump` blokkeert op SWIMMING
      voordat de swim-takeoff (9.0967 yd/s) kan lopen (`movement_direction.cpp:771-808`;
      benilla `swim.rs:231-266`).
- [ ] **Swim-latch** negeert unit-flag-poort, levitate en waterwalking
      (`unit_movement_runtime.cpp:918-976`; benilla `swim.rs:84-134,203-212`).
- [ ] **Knockback op transport** — richting moet in transportruimte
      (`movement_direction.cpp:813-876` doet dat wel, de stap zelf niet:
      `player_move_event.cpp:1639-1644`).
- [ ] **FALL_LAND-drempel** — aankondiging hangt aan valafstand i.p.v. de
      accumulatie van valtijd (`unit_movement_runtime.cpp:2062-2078`).
- [ ] **SavedVariables zonder TOC-declaratie** — wij laden/schrijven elk bestand
      (`addon_runtime_loader.cpp:696`) terwijl benilla `addon.rs:508` de declaratie
      respecteert; per-personage data belandt ook in het accountbestand (`:482-498`).
- [ ] **Addon-laadorde** sorteert niet (`addon_manager.cpp:507-604`) terwijl
      benilla dat wel doet (`addons.rs:346-351`); `LoadOnDemand`-vlag wordt
      permanent gezet (`addon_runtime_loader.cpp:558-577`).
- [ ] **Castbalk** — `SPELLCAST_START` krijgt de wire-casttijd mee
      (`world_session_combat.cpp:141-193`), de referentie seint de door de client
      berekende tijd.

### Batch 5 — Quest log-detail: plooi-icoon, klik en de driedubbele textuurrepresentatie

Status: **opgelost** (2026-09-17), op het REWARDS/clip-punt na. Drie oorzaken
gevonden, gefixt en door de eigenaar in-game bevestigd. Gemeten met het
debug-controlkanaal (`devctl.ps1`) op een draaiende client en met
`D:\OllieWoW\Client\Logs\openwow-client.log`.

**Oorzaak A -- `isHeader`/`isCollapsed` als getal i.p.v. nil.**
`LuaGetQuestLogTitle` (`src/openwow/ui/game/api/game_lua_api_quest.cpp`) pushte in
de quest-tak `lua_pushnumber(L, 0)` op positie 4 (`isHeader`) en 5 (`isCollapsed`),
en in beide foutpaden op positie 4. In Lua 5.0 is het getal `0` **truthy** -- alleen
`nil` en `false` zijn falsy; het contract (`QuestLogFrame.lua:138`) doet
`if ( isHeader )`. De client zag dus ELKE quest als zone-header. Bewijs:

```
GetQuestLogTitle index=2 is_header=0
questlog expand header index=2
```

Fix: `lua_pushwowbool(L, false)` (= `nil`) op die vier plekken. De quest-tak loopt
weer, zet `SetText("  "..titel)` en `SetNormalTexture("")`, en daarmee verdween het
plus/min-icoon van de questregels. Punt 1 hieronder was dus geen zelfstandige
textuurbug maar een gevolg van A.

**Oorzaak B -- in-/uitklappen werd binnen 1 ms teruggedraaid.**
`LuaGetQuestLogSelection` herleidde de index uit de **zichtbare** regels en gaf `0`
zodra de geselecteerde quest door een inklap-actie buiten die lijst viel. De geladen
(vanilla) Lua handelt daarop:

```lua
-- QuestLogFrame.lua:297-298
if ( GetQuestLogSelection() == 0 ) then
    QuestLog_SetFirstValidSelection();
end
```

Die komt via `QuestLog_GetFirstSelectableQuest` (`:568-580`) op de ENIGE zichtbare
regel uit -- de zone-header -- en `QuestLog_SetSelection` (`:325-332`) ziet die als
ingeklapt en roept `ExpandQuestHeader` aan. Daarnaast wiste
`LuaSelectQuestLogEntry` de selectie **onvoorwaardelijk**, ook voor een
header-index, terwijl de Lua die aanroept voor ELKE regel vóór de header-check
(`:321` vs `:325`); een header is geen selecteerbare entry. Bewijs:

```
questlog collapse header index=1
questlog expand header index=1
```

Fix: `LuaGetQuestLogSelection` geeft de opgeslagen index uit de **volledige**
interleaved lijst (`FindInterleavedQuestIndexById`), zonder clamp; en
`LuaSelectQuestLogEntry` laat de selectie staan als de index een header is (alleen
`index < 1` of geen sessie wist). Gevolg: een header-klik klapt alleen in/uit, de
header wordt nooit de selectie, en het detailpaneel blijft via
`QuestLog_OnEvent:64-65` de gekozen quest tonen.

**Correctie op eerdere metingen in deze batch.** `vfsdump2/framexml_all`
(2026-09-16 15:38) is van vóór de questlog25-patch en is dus NIET de geladen Lua:
daar stond `MAX_QUESTS = 20` en een re-couple `selection == 0 or selection >
numEntries` (OctoWoW-variant). De echt geladen `QuestLogFrame.lua` (26.062 bytes,
uit `patch-A.mpq`) heeft `MAX_QUESTS = 25`, `MAX_QUESTLOG_QUESTS = 25`,
`QUESTS_DISPLAYED = 24` en de **vanilla** `== 0`-regel. `artifacts/mpq_extract.exe`
leest `patch-A.mpq` eerst, dus na een clientdata-wijziging opnieuw extraheren
(`vfsdump3/`, `vfsdump4/`) -- anders meet je een oude revisie.

**`QuestTimerFrame.lua:23` was geen engine-bug maar clientdata.** Runtime-probe in
de draaiende client: `MQ=25 MQL=25`, `missing=21,...,30` -> `QuestTimer1..20`
bestonden wel. `QuestTimerFrame.xml` had 20 timer-buttons terwijl `MAX_QUESTS` 25
was, dus de lus `for i=arg.n + 1, MAX_QUESTS` in `QuestTimerFrame.lua:22` liep tot
25 en `_G["QuestTimer21"]:Hide()` was nil. Opgelost in `patch-A.mpq` (25 buttons);
her-extractie bevestigt 25 buttons in `QuestTimerFrame.xml`.

**Opgelost en geverifieerd** — `GetQuestLogTitle` (`game_lua_api_quest.cpp`)
gaf de returnwaarden in de verkeerde volgorde. Het 1.12-contract
(`QuestLogFrame.lua:137` en `:562-565`) is
`title, level, questTag, isHeader, isCollapsed, isComplete`; wij stuurden in de
header-tak `0, 1.0, collapsed` op positie 4-6 en in de quest-tak
`suggested_players` op positie 4 met `isComplete` op 7. Nu staan `isHeader`,
`isCollapsed` en `isComplete` op 4/5/6 en is `suggested_players` naar 7
geschoven (9 returns blijven). Meting na de fix:

```
hdr 1000      isHeader van entry 1..4  -> header herkend, quests niet
gsel 4        GetQuestLogSelection()   -> volgt de klik
```

Zichtbaar gevolg (screenshot eigenaar): de gekozen quest krijgt de
selectiebalk, het detailpaneel volgt, en de zone-header klapt in/uit. Wat
**niet** meebeweegt is `QuestLogFrame.selectedButtonID`: die blijft op de
default 2 staan terwijl `GetQuestLogSelection()` 4 is. Dat raakt alleen
`QuestLogTitleButton_OnLeave` (`:62-64`, tag-kleur/tooltip), niet de selectie.

Symptomen (screenshots van de eigenaar):

1. In het quest log staat op **gewone questregels** een rood plus/min-blokje; dat
   hoort alleen op zone-headers te staan.
2. In het quest log lijkt het of je alleen het plooi-icoon kunt raken en geen
   quest kunt selecteren; er is ook geen selectiebalk zichtbaar.
   **Meting (log 2026-09-16 16:48-16:50) nuanceert dit**: de klikken komen wél
   aan en worden door de UI afgehandeld --
   `InputTrace: router-down button=1 position=54,327 hit=QuestLogTitle1` +
   `left-down … ui-handled=1`, idem op `QuestLogTitle4` (255,390) en op
   `QuestLogFrameCloseButton`. Er zijn in die sessie **geen Lua-fouten**. Twee van
   de drie klikken landden op rij 1, de zone-header, en daar is inklappen correct
   gedrag. De klik op rij 4 ("Burning Blade Medallion") selecteerde die quest ook
   echt: de latere screenshot toont precies dat detail. Blijvende defecten zijn dus
   het verkeerde icoon op questregels, het ontbreken van de selectie-highlight, en
   een inconsistente fold-state (screenshot toonde een `+` op de header terwijl de
   quests eronder zichtbaar bleven).
3. In het detailpaneel valt het REWARDS-blok samen met de reward-itemknoppen.

Contract (lokaal, leidend):

- `Interface\FrameXML\QuestLogFrame.lua:138-155` — header: plus/min per
  fold-state; **gewone quest**: `SetText("  "..titel)` én
  `SetNormalTexture("")`; de highlight wordt daar ook gewist.
- `Interface\FrameXML\QuestLogFrame.xml:68` — `QuestLogTitleButtonTemplate`
  declareert `<NormalTexture file="Interface\Buttons\UI-MinusButton-UP">`,
  dus elke rij begint met dat plaatje.
- Detailpaneel is puur ankerwerk: `QuestLogFrame.xml:925-959` hangt
  DESCRIPTION aan `QuestLogObjective10`, de beschrijving aan die kop, en
  REWARDS aan de **gemeten onderkant van de beschrijvings-FontString**.
  `QuestLogItem1..10` (`:986-1035`) hebben alleen een bare `TOPLEFT`-anker; er is
  **geen Lua** die ze positioneert, dus de engine hoort dat te doen.
- Benilla: `benilla-ui/src/loader/widgets.rs:210-213` zet de XML-textuur
  eenmalig via dezelfde setter als Lua, en `widget/kinds/mod.rs:401-426` houdt
  hem als widget-state (`ButtonState.normal`, clientveld `+0x4bc`). Eén object.

Gemeten feiten:

- `GetQuestLogTitle(2)` geeft `hdr=0`, tekst "Lazy Peons" → de Lua neemt de
  **quest**-tak en roept dus `SetNormalTexture("")` aan.
- `QuestLogTitle2.__ow_btn_normal_tex` is **niet nil**, maar
  `QuestLogTitle2NormalTexture` bestaat **niet als Lua-global**:
  `Lua error: attempt to index global 'QuestLogTitle2NormalTexture' (a nil value)`.
  De `ui`-inspectie matcht die naam alleen als **frame-key**, niet als global.
- De tekstuur van die key blijft `Interface\Buttons\UI-MinusButton-UP` — de
  XML-templatewaarde — ook nadat de Lua hem gewist heeft.
- Diagnostiek in deze build (`frame_materializer.cpp`, kDebug) logt tweede
  instanties: bij één startup **477** regels `rematerialize frame …`, grotendeels
  `$anonymous_frame_N` met lege naam. Dat zijn de regio's die `TrackRuntimeRegion`
  → `AdoptRuntimeFrame` aanmaakt voor Lua-gemaakte texturen, niet de XML-frames.

Voorlopige oorzaak:

De XML `<NormalTexture>` van een template-knop wordt wel als regio
gematerialiseerd (en getekend), maar **niet in `__ow_btn_normal_tex` gebonden**.
De eerste `SetNormalTexture(...)` van de Lua vindt die slot dus leeg en maakt via
`CreateTextureTable` + `TrackRuntimeRegion` een **tweede regio** op dezelfde plek.
Gevolg: (a) het wissen van de Lua raakt de tweede regio terwijl de eerste het
minteken blijft tekenen, en (b) die extra regio ligt over de rij, wat het
klikgedrag verklaart. Dit is exact het drielaagsprobleem: Lua-tabel,
`__ow_btn_normal_tex` en het frame-store-record worden niet als één object
behandeld.

Wat geprobeerd is en **niet** werkte:

- Marker in `ApplyTextureTemplate` (`frame_xml_region_materializer.cpp`) zodat een
  XML-`file=` niet opnieuw wordt toegepast: dit pad loopt niet voor
  XML-gedeclareerde texturen (alleen voor Lua-`CreateTexture(inherits=)`), dus een
  no-op voor dit geval.
- Adoptie in `LuaSetNativeTextureSlot` (`button_methods.cpp`) via de conventienaam:
  eerst via `lua_getglobal` (bestaat niet, dus nooit geraakt), daarna via een scan
  van `__ow_regions` op `__ow_name == <naam><Rol>`. Laatste versie bouwt en draait,
  maar het beeld verandert niet — vermoedelijk heeft de XML-regio **geen**
  `__ow_name`, dus de naam-match faalt. Dit moet gemeten worden, niet geraden.

Kern gevonden (meting 2026-09-16 19:02, zelfde sessie):

```
ids 1 2 3        QuestLogTitle1/2/3:GetID()           -> ids kloppen
off 0            FauxScrollFrame_GetOffset(...)       -> offset klopt
t1 Valley of Trials   GetQuestLogTitle(1)            -> zone-header als entry
t2 Lazy Peons         GetQuestLogTitle(2)
n 4              GetNumQuestLogEntries()             -> 1 header + 3 quests
hdr 0000         isHeader van entry 1..4             -> OVERAL 0
gsel 3 sel 2     SelectQuestLogEntry(3) -> GetQuestLogSelection()=3,
                 maar QuestLogFrame.selectedButtonID blijft 2
```

De engine levert de zone-header dus **wel** als entry (naam klopt), maar zet
`isHeader` niet. Alle andere symptomen volgen daaruit:

- `QuestLog_Update` (`QuestLogFrame.lua:136-169`) neemt voor élke rij de
  **quest**-tak, zet dus nooit het plus/min-icoon en wist de template-textuur
  alleen op de Lua-kant (die de getekende regio niet raakt, zie hierboven).
  Het minteken op alle rijen is dus een **gevolg**, geen zelfstandige textuurbug.
- `QuestLog_SetSelection` (`:311-342`) kan de header-tak nooit nemen, dus
  in-/uitklappen en de selectie-highlight werken niet zoals bedoeld.
- `selectedButtonID` loopt niet mee met `GetQuestLogSelection()`; de
  verzoening in `QuestLog_Update` (`:281-292`) zet hem terug op de default 2.

Kleinste wijziging: in de quest-log-Lua-API (`GetQuestLogTitle`,
`game_lua_api_quest.cpp`) de zone-header-entries als header flaggen (`isHeader`,
`isCollapsed` uit de tracking-/expand-state) en de selectie-verzoening laten
volgen. Daarmee verdwijnen icoon, highlight en fold-gedrag in één keer -- de
textuur-/regiovondsten hierboven blijven geldig als *tweede* laag, maar zijn niet
de oorzaak van wat de speler ziet.

Meting 2026-09-16 19:2x (client via `devctl`), per restpunt:

**Punt 3 -- REWARDS/items: de ankers zijn GOED.** Live rects:

```
QuestLogQuestDescription   x=613 y=463 468x389   -> onderkant 852
QuestLogDescriptionTitle   x=613 y=423 494x31
QuestLogRewardTitleText    x=613 y=878 520x32    -> 26 px onder de beschrijving
QuestLogItemChooseText     x=613 y=918 512x14
QuestLogItem1              x=607 y=941 256x71
QuestLogItem2              x=864 y=941 256x71
```

Dus REWARDS en de items staan netjes gestapeld. De echte oorzaak is dat onze UI
**niet clipt**: een ScrollFrame-kind tekent buiten het venster door, waardoor het
REWARDS-blok en de items over de onderrand van het detailpaneel heen vallen. Het
`clip_rect`-veld in `ui/game/stateful_widget_render.h:147` wordt door de UI-laag
nergens gevuld (de enige treffers in de tree zijn WMO/terrain). Fix = scissor/clip
voor scroll-kinderen in de compositor; dat is een renderer-feature, geen
eenregelige aanpassing, en het profiteert elke scrollframe in de UI.

**Inklappen van de zone-header (nieuw, gemeld door de eigenaar).** Native werkt
het: `CollapseQuestHeader(1)` maakt rij 3 onzichtbaar en `ExpandQuestHeader(1)`
weer zichtbaar (gemeten via `ui QuestLogTitle3`). Een **klik** op de headerregel
klapt echter niet in, terwijl de klik wel aankomt. Volgende stap: de
`collapsed`-semantiek natrekken in `BuildInterleavedQuestLog` versus
`SetQuestLogHeaderCollapsed` (`game_lua_api_quest.cpp:698-716`); vermoeden is dat
de vlag die de Lua leest (positie 5) niet dezelfde state is die de natives
schrijven, waardoor de klik-tak altijd `ExpandQuestHeader` kiest.

**Punt 1 -- plus/min-icoon op questregels.** De regiolijst van rij 2 is gemeten:
`QuestLogTitle2NormalTexture` is **wel** een regio van de knop (plus
`...Highlight`, `...Check`, `...GroupMates`, `...Tag`, en de tekstregio). De
binding bestaat dus; het plusje moet uit de store/render-laag komen die de
knop-state-tekstuur zelf schildert (`FrameStore::SetTextureRole` + de
button-state). Volgende stap: nagaan wie die state-tekstuur in de renderlaag zet
en waarom de `SetNormalTexture("")` van de Lua daar niet aankomt.

**Punt 2 -- `selectedButtonID`.** **Vervallen** (2026-09-17): de bewering dat de
lokale Lua een OctoWoW-re-couple op `:276-299` heeft, kwam uit de stale
`vfsdump2`-extractie. De geladen Lua heeft de vanilla `== 0`-regel (`:297-298`) en
geen re-couple-tak; zie de correctie bovenaan deze batch.

Volgende stappen (bijgewerkt 2026-09-17):

1. **Vervallen.** Het icoon bleek een gevolg van oorzaak A; er is geen tweede regio
   nodig gebleken -- `SetNormalTexture("")` komt aan op de quest-tak en de
   questregels zijn schoon (screenshot eigenaar).
2. **Vervallen.** Idem: `BindNativeTextureRegion` hoeft de XML-regio niet te
   adopteren zolang de Lua de juiste tak neemt.
3. **Vervallen.** Klik/selectie werkt en is in-game bevestigd.
4. **Open:** het detailpaneel. De ankers zijn goed (meting hierboven); de echte
   oorzaak is dat onze UI niet clipt -- `clip_rect` in
   `ui/game/stateful_widget_render.h:147` wordt nergens gevuld. Fix = scissor/clip
   voor scroll-kinderen in de compositor (renderer-feature, geen eenregelige
   aanpassing).

Verificatie (herhaalbaar): `ui QuestLogTitle2NormalTexture` → leeg; screenshot van
de lijst → geen blokje op questregels; `devctl click` op een rij →
`QuestLogFrame.selectedButtonID` verandert; daarna `HideUIPanel` + `ToggleQuestLog`
→ alles blijft.
## Vervallen bevindingen

- **`__benilla_now` (cooldown-klok)**: een subagent meldde dat
  `game_lua_api_action.cpp` tegen een Lua-global `__benilla_now` vergelijkt. Die
  string komt in onze hele source niet voor (0 treffers); het citaat was
  Benilla's Rust-code. Niet overgenomen. Actie-cooldowns blijven dus een open
  vraag, maar zonder deze onderbouwing.
