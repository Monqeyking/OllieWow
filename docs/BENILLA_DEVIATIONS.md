# Benilla-afwijkingen: audit + werkvoorraad

Status: levende checklist.
Laatste update: 2026-09-13 (batch 1 deels afgerond: chat).

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
- [ ] **SMSG_GOSSIP_MESSAGE / CMSG_GOSSIP_SELECT_OPTION** — 1.12-body zonder
      menuId/box-velden (`Source GossipDef.cpp:176-217`, benilla `gossip.rs:75-95`).
      Symptoom: bank/vendor/trainer/innkeeper parsen niet of kiezen de verkeerde
      optie. Ons: `gossip_manager.cpp:24-58`, `:167-176`.
- [ ] **SMSG_LIST_INVENTORY** — 7 velden per item, niet 8 (`Source
      ItemHandler.cpp:974-980`). Symptoom: vendor "malformed", venster leeg. Ons:
      `gossip_manager.cpp:145-153`.
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
- [ ] **Unit-field → event-indices** — retail-offsets gebruikt
      (`player_unit_field_event_callbacks.cpp:56,62`: UNIT_TARGET op 12,
      UNIT_HEALTH op 18) waar 1.12 andere indices heeft. Symptoom: spook-events
      (mana→UNIT_DISPLAYPOWER, rage→UNIT_HEALTH) en echte UNIT_HEALTH/LEVEL/TARGET
      vuren nooit.
- [ ] **SpellVisualEffectName** — WotLK-7-koloms schema op de 5-koloms
      Classic-tabel (`dbc_structures.cpp:374-382` leest schaal op veld 5-6 die 0
      blijft) → `ceffect_c.cpp:126-132` klemt elke effect-schaal op 0 en valt
      terug op 1.0x. Alle attach-VFX staan op 1x.

### Batch 2 — visueel en ruimtelijk

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

### Batch 4 — beweging, persistentie en rest

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

## Vervallen bevindingen

- **`__benilla_now` (cooldown-klok)**: een subagent meldde dat
  `game_lua_api_action.cpp` tegen een Lua-global `__benilla_now` vergelijkt. Die
  string komt in onze hele source niet voor (0 treffers); het citaat was
  Benilla's Rust-code. Niet overgenomen. Actie-cooldowns blijven dus een open
  vraag, maar zonder deze onderbouwing.
