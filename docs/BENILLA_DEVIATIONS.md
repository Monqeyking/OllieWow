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
### Batch 6 — aura's, quest log, cursor, GM-ticket, crashhandler en WMO-licht (opgelost 2026-09-19/20)

**Aura's.** Descriptor-auras worden nu in AuraTracker én AuraManager gezet
(`unit_descriptor_callbacks.cpp`), `UNIT_AURA` vuurt bij een wijziging
(`update_field_event_mapper.cpp`), `PLAYER_AURAS_CHANGED` wordt voor de speler
gequeued, en `SMSG_UPDATE_AURA_DURATION` (0x137) wordt vóór de packet_dispatcher
afgehandeld (`world_session.cpp`). De duur wordt slot-geïndexeerd bewaard
(`aura_manager.cpp`), zodat een timer een relog overleeft én werkt als het
duur-pakket vóór de aura aankomt. In-game bevestigd (timers op alle buffs).

**Quest log.** `GetQuestLogTitle` geeft het 1.12-zeswaarden-contract
(isHeader/isCollapsed = nil voor quests), de selectie blijft bij header-kliks,
template-loze quests houden hun rij, en een onbekende titel is `""` in plaats van
`nil` (`QuestLogFrame.lua:164` concateneert onvoorwaardelijk).

**Wereldcursor.** `unit_cursor_policy.h` bevat de 1.12-service-ladder over
UNIT_NPC_FLAGS (laagste bit wint, REPAIR nooit geconsulteerd), de vaste
5,5556-yd-grijsgrens en de questgiver-gate. Quest givers gebruiken `Speak` in
plaats van de 3.3.5 `Quest*`-cursors die in 1.12 niet bestaan. In-game bevestigd.

**GM-ticket.** `UPDATE_TICKET` krijgt `arg1 = 0` zonder ticket, zodat het
indicator-icoon alleen verschijnt bij een ingediend ticket.

**Crashhandler.** `CrashExceptionFilter` gebruikte de throwing
`create_directories`-overload; een falende aanroep liet een `filesystem_error`
uit het filter ontsnappen → `std::terminate` (geen actieve exception) →
`0xC0000409` — 318 van de 333 WER-rapporten. Nu `error_code` + try/catch, en de
stack gaat ook naar stderr. Daarmee werd de echte fout zichtbaar: een
static-destruction-order access violation in `~CInputControl` →
`BindingProfiles::ClearOverrideBindings` op een al vernietigde global; de
input-control-singleton wordt nu bewust niet opgeruimd bij exit. In-game
bevestigd (schone exit, geen enkel crash-artifact).

**WMO-licht.** De WMO gebruikte de *derived* dag/nacht-kleuren, terwijl terrein
en M2 LightIntBand rij 0/1 gebruiken (`benilla-formats/src/light/atmosphere.rs:8-9`).
Nu dezelfde bron. Bonus: `kSunColor` las rij 8 (shadow-opacity-slot) in plaats van
rij 9 (de zon) — `light.rs:15-16`.

### Batch 7 — terrein-schaduw: dynamische shadow map uit (opgelost 2026-09-21)

De donkere plekken op het terrein ("alsof het niet gerenderd is") zijn gemeten
met het debugkanaal en pixel-diffs op twee screenshots vanaf dezelfde camera:

| A/B | verschil | richting |
|---|---|---|
| `showCull` (terrein-frustum-culling) aan/uit | 0,81% van de pixels, **symmetrisch** (3761 donkerder vs 3519 helderder) | culling is **niet** de oorzaak |
| `showShadow` (dynamische schaduwmap) aan/uit | **3,02%**, helderheid **30,5 → 41,2** (+35% = precies de 0,7-modulate) | **dit is de oorzaak** |

**Oorzaak.** Onze dynamische terrein-schaduwmap. Die wordt vanuit de **view**
opgebouwd, dus de donkere plekken worden meer of minder als je de camera draait.
De 1.12-client heeft **geen** real-time shadow map: zijn terrein-schaduw is de
statische **MCSH**-bake per chunk (1 bit, 1.0 belicht; diffuse in schaduw
wordt volgens de Benilla-combine met 0.7 gemoduleerd) —
`benilla-assets/src/materials.rs:171-175`, `benilla-adt/src/lib.rs:90-96`,
`benilla-assets/src/shaders/terrain.wgsl:281-300`.

**Fix.** `kCWorldInitializeRenderFlagDefaults`: `0x07104B73` → `0x07104B33`
(het `kTerrainShadows`-bit 0x40 uit). `showShadow` blijft als opt-in verbetering.

**Meetrecept (herbruikbaar).** `devctl.ps1 shot` → `artifacts\tgafast.ps1` →
pixel-diff; render-flags live omschakelen met
`devctl.ps1 lua 'ConsoleExec("<flag>")'` (`showCull`, `showShadow`,
`showLowDetail`, `maxLOD`).

**WDL (zelfde sessie).** De far band is nu een backdrop zoals de referentie:
altijd getekend, diepte gecomprimeerd naar `[0.99999, 0.999999]` (achter alles
wat wij tekenen, vóór de lege dieptebuffer) en een vlakke fog-hull. Let op: het
bereik `[0.955, 0.96]` uit de referentie werkte hier **niet** — bij onze
projectie reikt het verre terrein dieper, dus dan occludeert de backdrop de
verte (het hele beeld werd paars).

### Batch 8 — reputation-tab leeg: Faction.dbc werd nooit geladen, plus WotLK-kolomoffsets (opgelost 2026-09-22)

De reputation-tab toonde **helemaal geen rijen**. Live gemeten in de draaiende
client via het debug-kanaal: `GetNumFactions() == 0`, `GetFactionInfo(1)` gaf
alleen de synthetische `Inactive`-header, en `GetFactionInfoByID(id)` gaf voor
**0 van de 200** getestte ids een naam — de Faction-store was dus leeg.

**Oorzaak 1 (de lege lijst).** `IsClassicMvpDbc` (`dbc_loader.cpp:59-153`) is de
whitelist waarop `DbcLoader::LoadAll` met `DbcLoadProfile::kClassicMvp` alle
tabellen filtert (`dbc_table_registry.cpp:163`, `dbc_loader.cpp:329`).
`FactionGroup.dbc` en `FactionTemplate.dbc` stonden erin, **`Faction.dbc` niet**.
Zonder die tabel is `dbc_->faction()` leeg, geeft `LookupEntry()` altijd
`nullptr` en voegt `InitFromPlayerData` nul entries toe. Fix: `Faction.dbc`
toegevoegd (81 → 82). Exact dezelfde faalwijze die in dat bestand al bij
`CreatureDisplayInfoExtra.dbc` staat gedocumenteerd.

**Oorzaak 2 (rijen zonder naam), gevonden tijdens dezelfde audit.**
`FactionEntry::Load` las de WotLK-offsets (naam veld 23, beschrijving veld 40) op
het lokale **37-velds vanilla-record**. Gemeten op de echte clientarchieven
(`artifacts/mpq_extract.exe`, uit `patch-9.mpq`; zie `artifacts/dbc-check/`):
204 records, `field_count` 37, `record_size` 148. Veld 19 bevat de naam
("Ironforge", "Booty Bay", "Thorium Brotherhood"), veld 28 de beschrijving (46 van
de 54 reputatiefacties hebben er een). Veld 23 landde op een andere locale-kolom
(`??????(??????)`) en veld 40 is ≥ `field_count`, dus `DbcFile::FieldPtr`
weigerde het en `GetLocalizedString` gaf altijd leeg terug. Fix: veld 19/28.

**Beide fixes zijn nodig:** oorzaak 1 maakt de lijst leeg, oorzaak 2 maakt de namen
leeg zodra de lijst wél gevuld is. Geen DBC-, MPQ-, server- of brongewijziging;
bouwt en linkt schoon (TU's/archieven/link exit 0).

**Volledige faction/reputation-ronde (zelfde sessie).** Na de twee oorzaken hierboven
is de hele surface tegen het lokale 1.12-contract gelegd — de lokale
`vfsdump2/framexml_all/Interface_FrameXML_ReputationFrame.{lua,xml}`, pfUI's
`xpbar.lua`, en Benilla's `benilla-ui/src/script/reputation.rs` +
`benilla-formats/src/factions.rs`. Daaruit zijn nog acht afwijkingen gerepareerd:

1. `GetFactionInfo` gaf **13** waarden met de 3.3.5-volgorde; het lokale contract
   is **11** met `isWatched` op positie 11 (`ReputationFrame.lua:35,51`). De twee
   3.3.5-only velden (`isPlayerFriendly`, `isChild`) en hun state zijn verwijderd.
2. Buiten het zichtbare bereik geeft `GetFactionInfo` nu de echte miss-tuple
   `nil,nil,1,0,0,0,nil,nil,nil,nil,nil` — die `1` in het standingID-slot is
   load-bearing, want de referentie indexeert `FACTION_BAR_COLORS[standingID]`
   ongeguard.
3. `GetWatchedFactionInfo` geeft bij "niets gevolgd" één `nil` i.p.v. vijf nullen.
4. `SetSelectedFaction` wist de selectie op een header- of buiten-bereik-rij.
5. `FactionToggleAtWar` respecteert `canToggleAtWar` (standing ≥ −3000 én geen
   peace-forced vlag 0x10).
6. `SendToggleAtWar`/`SendSetInactive` gebruikten `FindHeaderIndex(...) > 0`
   waardoor **header-index 0** erdoor glipte en een header als faction-bar werd
   behandeld; nu `>= 0`.
7. `SMSG_SET_FACTION_STANDING` werd als 3.3.5 geparsed (float bonus + u8
   "increased" vóór de count); de lokale `ReputationMgr::SendState` schrijft
   **count-first**. Nu count-first, `bonus_rep` vervalt (0.0f).
8. Een serverpush reset nu de folds (alles uitgeklapt, dan de synthetische
   "Inactive"-header dicht) — het gedrag dat maakt dat een fold een standing-tick
   niet overleeft.

Geverifieerd tegen de lokale Source: de opcodes (0x123/0x124/0x125/0x313/0x317/
0x318), de CMSG-bodies (ATWAR u32+u8, INACTIVE u32+u8, WATCHED i32) en de
SMSG-bodies (VISIBLE u32, ATWAR u32+u8) komen overeen; de rank-edges en de
Exalted-cap 43000 matchen `benilla-formats::reputation_rank`.

Bewust additief gelaten: `GetFactionInfoByID`, `CollapseAllFactionHeaders` en
`ExpandAllFactionHeaders` bestaan niet in 1.12/Benilla, maar zijn extra globals
zonder lokaal contract in de weg. De selectie wordt intern op faction-id gehouden
i.p.v. op rep-list-slot (waarneembaar gelijk). `DisplayRepChangeMessage`'s
`bonus_rep`-tak is nu onbereikbaar omdat 1.12 dat veld niet stuurt.

**Runtime-bevestiging (zelfde sessie, in de draaiende client).** Met het
debug-kanaal in-game gemeten op een Horde-character:

```
GetNumFactions()                      = 6            (was 0)
GetFactionInfo(1)                     = "Horde"      (was "Inactive")
GetFactionInfoByID(47)                = "Ironforge"  (was nil)
rijen 1..6                            = Horde | Darkspear Trolls | Durotar Labor
                                        Union | Orgrimmar | Thunder Bluff | Undercity
select('#', GetFactionInfo(1))        = 11           (was 13)
select('#', GetWatchedFactionInfo())  = 1            (was 5)
```

Niet runtime-bevestigd: de fold-reset bij een standing-tick (reparatie 8) — dat
vraagt een dichtgeklapte header plus een reputatiewijziging.

Nog open in dit domein (aparte beslissing nodig): de WotLK-only
`parentFactionMod[2]`/`parentFactionCap[2]` worden nog van veld 19-22 gelezen —
dat bereik IS hier `name[0..3]` — en `quest_manager.cpp:563-594` gebruikt ze voor
reputatie-spillover. Dat vraagt de vanilla-spillover-semantiek.

### Batch 9 — Skills-tab leeg (zelfde oorzaak) en rep-spillover opgeruimd (opgelost 2026-09-22)

**Skills-tab leeg.** `SkillInfoStore::UpdateFromPlayer` slaat elke skill over
waarvan de categorie niet in `SkillLineCategory.dbc` staat (`skill_info.cpp:273`),
en die tabel stond niet in `IsClassicMvpDbc` — net zomin als `SkillTiers.dbc` en
`SkillCostsData.dbc` (stepCost/skillMaxRank resp. de trainingskosten;
`game_lua_api_profession.cpp:397`, `skill_info.cpp:170`). Fix: alle drie
toegevoegd (82 → 85). Exact dezelfde faalwijze als `Faction.dbc` in Batch 8 — de
whitelist is handonderhouden en mist entries.

Daarnaast gaf `GetSkillLineInfo` buiten bereik dertien nullen i.p.v. één `nil`.
Het contract is 13 op een skill-rij, 12 op een header en 1 `nil` erbuiten
(`benilla-ui/src/script/skills.rs`). Nu één `nil`; een header zonder categorie
antwoordt in de header-vorm (12, lege naam), omdat een `nil` daar
`SkillFrame_SetStatusBar` in nil-rekenwerk zou sturen.

**Rep-spillover.** De WotLK-only `parentFactionMod/Cap`-kolommen bestaan niet in het
lokale 37-velds Faction.dbc — veld 19-22 is daar `name[0..3]`. De decode en de twee
spillover-blokken in `QuestManager::AccumulateRewardFactionPreview` zijn
verwijderd. Op de echte data evalueerde de oude code naar 0 (veld 20-22 zijn leeg en
veld 19 als float is denormaal), dus er verscheen nooit phantom-spillover; de code
suggereerde alleen steun die er niet is. De client hoeft het ook niet te weten: de
server past spillover toe (`reputation_spillover_template`) en pusht de
resulterende standing via `SMSG_SET_FACTION_STANDING`.

### Batch 10 — honor-pane verkeerd gecentreerd: GetWidth van een FontString (opgelost 2026-09-22)

De honor-tab stond scheef: de rank en het honor-getal hingen rechts, deels buiten het
paneel, terwijl de arena-tab (die `GetWidth` niet gebruikt) goed stond.

Gemeten in de draaiende client:

```
HonorFrameCurrentPVPTitle:GetWidth()        =  31.11   ("None")
HonorFrameCurrentPVPTitle:GetStringWidth()  =  31.11
HonorFrameCurrentPVPTitle:GetLeft()         =  62.80
HonorFrameCurrentPVPTitle:GetRight()        = 283.46   -> rect-breedte 220.66
HonorFrameCurrentPVPRank:GetLeft()          = 288.65   (= title.right + 5)
```

`GetWidth()` en de resolved rect spraken elkaar dus tegen: 31 versus 220,66.
`HonorFrameCurrentPVPRank` ankert op de **resolved** rechterrand (283,5), terwijl
`honorframe.lua:76` (`SetPoint("TOP","HonorFrame","TOP", -GetWidth()/2, -83)`) op
`GetWidth()/2 = 15,5` rekent. Het gevolg is een vaste puntverschuiving van ~190 px
naar rechts.

Dat is precies de afwijking die Benilla documenteert
(`benilla-ui/src/script/tests/measure.rs:285`): **`GetStringWidth` is de natuurlijke
tekstbreedte, `GetWidth` dekt de LAYOUT-extent** — dezelfde rect waar de anchor-engine
tegen werkt. OpenWow gaf in beide gevallen de gemeten tekstbreedte terug.

Fix: `ResolveLuaFontStringEffectiveSize` (`frame_region_geometry.cpp`) geeft nu de
resolved rect terug als die er is, en valt pas daarna terug op `SetSize` en de meting.
`GetStringWidth` blijft de gemeten natuurlijke breedte. Dit raakt alleen
FontStrings waar rect en tekstbreedte uiteenlopen (over-geconstraineerde of
parent-gevulde regio's); een gewone auto-sized FontString geeft dezelfde waarde als
voorheen.

**Addendum bij Batch 10 (oorzaak dieper).** Met de `GetWidth`-fix is `GetWidth` gelijk aan de rect (220,66 = `GetRight()-GetLeft()`), maar de rect zelf is fout — de honor-rij blijft scheef. Gemeten: `HonorFrameCurrentPVPTitle:GetNumPoints() == 2`. De titel heeft dus TWEE anchors: de `TOPLEFT (63,-84)` uit de lokale `HonorFrame.xml` én de `TOP` die `honorframe.lua:75-76` elke repaint zet.

De referentie bewaart anchors in negen `point`-slots en honoreert per as maar ÉÉN anchor (eerste aanwezige): Benilla `benilla-ui/src/layout.rs:196-198` ("if two share a point, the last wins", client `SetPoint 0x767c70`) en de scans `anchorScanX 0x7671a0` / `anchorScanY 0x7671f0`. OpenWow heeft dat slotmodel al (`framexml/layout_anchor_resolution.h:159 BuildAnchorSlots`), maar combineert in `layout_resolver.cpp:290-299` wél beide: `resolved_left = left_x` (63) én `resolved_right = SynthesizeHorizontalSide(center_x, left_x, width)` met een `width` van 220,66 in plaats van de gemeten tekstbreedte 31. Daardoor wordt de rect 220 breed en duwt `HonorFrameCurrentPVPRank` (geankerd op `title.right`) plus het honor-getal ~190 px naar rechts.

Kleinste wijziging: in die synthesepad voor een auto-sized FontString zonder opposing-paar (left+center, geen right) de **gemeten tekstbreedte** gebruiken — dezelfde waarde die `GetStringWidth` al teruggeeft. Dat is een layout-enginewijziging, geen datawijziging; de lokale XML blijft ongemoeid.

**Skillbalk.** `SkillRankFrameN` geeft via Lua de juiste waarden (`v=30 mn=0 mx=30 w=271`), dus de data klopt. In `SkillFrame.xml` is `$parentBackground` een Texture **zonder Size en zonder anchors** (vult dus het parent, volle breedte) die de lokale Lua zelf blauw zet; de voortgang is de StatusBar-BarTexture erbovenop. Dat er geen enkel verschil tussen 1/30 en 30/30 te zien is, betekent dat die fill-quad niet (zichtbaar) wordt gesubmit — vermoedelijk paint-orde van de BarTexture t.o.v. dezelfde-laag `$parentBackground`. Nog te toetsen in de render-/paint-orderroute.

### Batch 11 — skillbalk-fill gecropt; honor en skills visueel gelijk aan vanilla (geverifieerd 2026-09-22)

**Honor (Batch 10) is A/B-bevestigd.** Na de span-regel in `SynthesizeHorizontalSide`
staat "None (Rank 0)" met het honor-getal gecentreerd, gelijk aan de vanilla-client.

**Skillbalk.** De `[sbtrace]`-meting (env-gated, inmiddels weer verwijderd) liet zien
dat de keten klopte: `SkillRankFrame3Bar value=1/30 → fill=15,7 van 470 px`,
`SkillRankFrame5Bar value=32/33 → fill=455,8 van 470`, `visible=1`, `has_texture=1`,
en dezelfde textuur (`PaperDollInfoFrame\UI-Character-Skills-Bar`) wordt ook door de
reputatiebalken met `submitted=1` gerenderd. Geometrie, kleur, laagorde en textuur
waren dus alle correct — de fill werd alleen **geplet**: de volle UV-range (0→1) ging
in een 15-px quad, waardoor de 256-px balkkunst tot een egale lap werd gemiddeld.

Fix naar het patroon van WoWee's `drawStatusBar` (`Kelsidavis/WoWee`,
`src/ui/widget_renderer.cpp:876-881`: "the texture is cropped to the filled part
rather than squashed into it"): de fill-quad cropt nu op de fractie
(`u1 → u0 + (u1-u0)·fractie`, verticaal de y-twin). Bij fractie 1 is dat identiek aan
de oude waarde, dus volle balken (health/mana/reputatie) veranderen niet. Visueel
bevestigd gelijk aan vanilla.

**Open nieuw punt — onze wereld is donkerder dan vanilla.** A/B tegen de echte
vanilla-client laat zien dat het terrein bij ons duidelijk donkerder is. Twee
kandidaten, beide bron-toetsbaar:
1. `vs_terrain.sc:47` / `vs_terrain_splat_body.sh:49` clampen de lichtsom naar 1,0:
   `v_color0 = clamp(ambient + diffuse·max(N·L,0) + puntlichten, 0, 1)`. Dat is
   dezelfde wet die het comment zelf citeert
   (`benilla-assets/src/shaders/terrain.wgsl:5-8`), dus de clamp hoort — maar dan
   moeten de ambient/diffuse-waarden uit LightIntBand rij 0/1 ook kloppen.
   `vs_wmo.sc:33-34` clampt eveneens.
2. Het **dag/nacht-tijdstip**: als onze klok anders samplet dan de server, rendert de
   wereld op een donkerder uur. Dit is met één meting te scheiden
   (`GetGameTime()` in de client naast de servertijd) en dat is de eerstvolgende stap
   vóór er iets aan de lichtwaarden verandert.

## Werkvoorraad: pariteit met het origineel

Open, in volgorde van impact:

1. **Protocol-clusters uit de audit** (`docs/PROTOCOL_AUDIT_335_VS_112.md`, Batch 1b):
   de resterende 3.3.5→1.12-pakketpaden.
2. **`QuestGiverStatus`-enum**: wij gebruiken de 3.3.5-ordening terwijl de
   1.12-server de 1.12-waarden stuurt (`Source QuestDef.h:121-130`). In het
   cursorpad is dat omzeild door de ruwe wire-waarde te lezen; de enum zelf is nog
   fout en andere consumenten lezen hem ook.
3. **Cursor-follow-up**: de niet-NPC-potenen (attack/skin/loot). Onze attack-grens
   (10,45 yd) en skin-reach wijken af van de referentie
   (`target/cursor_mode.rs:174-182`).
4. **UI-clipping**: `clip_rect` wordt nergens gevuld; het quest-detailpaneel clipt
   daardoor niet (Batch 5, punt 4).
5. **Harness-gate**: `world_offline_play_regression` faalt op zijn eigen asserties
   (`ChatFrame1 message count did not advance`, render-ready-wait). Zolang die rood
   staan betekent "groen" alleen "mijn regel is groen".
6. **MCSH-terrein-schaduw** (Batch 7): we parsen de bake al
   (`adt_file.cpp:169-227`, `575`) maar voeden de shader met een globale
   `terrain_shadow_mod` (`world_render_pipeline.cpp:74`,
   `terrain_renderer.cpp:509-533`) in plaats van de per-chunk bake. Dat is de
   originele statische terrein-schaduw en hoort de volgende stap te zijn.
7. **Rep-spillover-velden** (Batch 8): de WotLK-only
   `parentFactionMod`/`parentFactionCap` worden van Faction.dbc-veld 19-22
   gelezen (dat bereik IS hier `name[0..3]`) en `quest_manager.cpp:563-594`
   gebruikt ze voor reputatie-spillover. Vraagt de vanilla-spillover-semantiek.

## Toekomst: opt-in graphics-upgrades (ná pariteit)

Alles achter één CVar (`gfx.enhanced`), standaard **uit**, zodat de 1.12-look het
contract blijft. Volgorde op impact:

1. **Schaduwen op WMO + M2.** De shadow-map-infrastructuur bestaat al
   (`shadow_presentation_runtime.cpp`, `ShadowMap`-pass), maar de Classic-
   terreinshaders sampelen hem niet meer: terrein gebruikt alleen de statische
   MCSH-bake. Gebouwen en modellen ontvangen nog geen dynamische schaduw.
   Grootste opt-in graphics-upgrade, additief.
2. **Per-pixel lighting** voor WMO/M2 (nu per-vertex in `vs_wmo.sc`) — voorwaarde
   voor normal maps.
3. **Post**: SSAO, HDR/tonemapping en AA-opties (de post-keten bestaat al).
4. **Optionele HD-asset-override** (alleen laden, geen MPQ-wijziging).

## Vervallen bevindingen

- **`__benilla_now` (cooldown-klok)**: een subagent meldde dat
  `game_lua_api_action.cpp` tegen een Lua-global `__benilla_now` vergelijkt. Die
  string komt in onze hele source niet voor (0 treffers); het citaat was
  Benilla's Rust-code. Niet overgenomen. Actie-cooldowns blijven dus een open
  vraag, maar zonder deze onderbouwing.
