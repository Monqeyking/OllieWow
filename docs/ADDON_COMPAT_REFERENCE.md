# Addon-compatibiliteit: benilla als gedragsreferentie

Status: read-only inventarisatie + voorstel
Datum: 2026-09-12
Bron-prioriteit volgens `AGENTS.md`: lokale `Client`/`Source` blijven het contract;
`D:\OllieWoW\benilla` en de online repo zijn **gedragsreferentie**, geen vervanging.

## 1. Bronnen en versieverschil

| Bron | Versie / pad | Opmerking |
|---|---|---|
| Lokale benilla-checkout | `D:\OllieWoW\benilla`, HEAD `73826ca` (2026-08-25) | loopt ~2,5 week achter |
| Online benilla | https://github.com/samwhosung/benilla, HEAD `3e92b4f` (2026-09-11) | hier staan de nieuwste contracten (issues 2163-2192) |
| Lua-VM benilla | `third_party/lua-src/BENILLA.md` (lokaal aanwezig) | onderbouwt de dialectaanpak met corpusmetingen |
| Onze VM | `third_party/wow_lua` = **Lua 5.1.1** (`lua.h`: `LUA_RELEASE "Lua 5.1.1"`) | let op: benilla gebruikt 5.1.5; hunk moet op 5.1.1 passen |
| Onze dialectlaag | `src/openwow/ui/lua_base_overrides.h` | committed (niet in de WIP-diff) |

## 2. De vier pilaren bij benilla

### 2.1 Lua-dialect op VM-niveau, niet via bronrewrite

De 1.12-corpus is Lua **5.0**-code. De iterator-loze generic-for

```lua
for k, v in someTable do ... end
```

komt bij **183 van 218 corpus-addons** voor (1.163 sites) en is de **eerste
session-start-error voor 60 van 218 addons**. Lua 5.1 verwijderde dit op
**opcode-niveau**, dus geen library-, global- of metamethod-laag kan het
bereiken. Benilla's antwoord is een patch van **drie hunks in drie bestanden**,
verder byte-identiek aan upstream:

| bestand | hunk | reden |
|---|---|---|
| `lvm.c` | 5.0's `OP_TFORPREP` table->`next`-substitutie, gevouwen in de top van `OP_TFORLOOP` | `for k,v in t do` gaf "attempt to call a table value" |
| `luaconf.h` | `LUA_COMPAT_LSTR` 1 -> 2 | twee addons stierven op "nesting of `[[...]]` is deprecated" |
| `lparser.c` | 5.0's compat-puntkomma-skip in de veldlus van `constructor()` | `;;` in een table-constructor (AtlasLoot) |

Semantiek uit een byte-level reverse-engineering van de echte client:
een **bare type-tag test die nooit een metatable raadpleegt**, de **globale
`next` die per loop-entry opnieuw rauw gelezen wordt** (zodat `next = myfn`
alle latere generic-fors verandert), en **userdata wordt niet gesubstitueerd**.

Expliciet afgewezen door benilla: het herschrijven van chunk-broncode
("parsing Lua with a regex to tell `in t do` from `in pairs(t) do`, and getting
that wrong changes behaviour silently").

### 2.2 Loader, volgorde en AddOn-API

Commit `eebbcc0` (issues 2175/2178):

- addons laden in **NTFS-mapvolgorde**, case-insensitive, `_` vóór letters —
  de volgorde waarin de vanilla-corpus geschreven is en die bepaalt welke addon
  een gedeelde library levert;
- `GetNumAddOns` en de index-vorm van alle AddOn-verbs lopen een
  **titel-gesorteerde lijst** die de Blizzard-addons verbergt die de server
  verbergt; de character-select-lijst staat in titelorde.

Code om te lezen: `crates/benilla-ui/src/toc.rs`, `order.rs`, `framexml.rs`.

### 2.3 FrameXML-contract op naam

- `6d66329` (2177): het video-optionsvenster van de **reference** wordt geladen
  en blijft verborgen, zodat pfUI's video-skin en elke addon die sliders en
  checkbuttons **bij naam** zoekt echte frames vindt; benilla's eigen venster
  heet nu `BenillaOptionsFrame`.
- `eebbcc0`: `$parent`-expansie in `CreateFrame`/`CreateTexture`/
  `CreateFontString`; `ANCHOR_CURSOR`-tooltips; `SetPoint`/`SetAllPoints`-fouten
  zoals de reference.
- `3e92b4f` (2191): onbekend frame-type in **XML** = loggen en overslaan;
  **alleen een script dat een frame van een fout type maakt raise't**.
- `5af3a15` (2171): `select` en de string-metatable zijn weg, zoals in 1.12's
  Lua 5.0; addons die daarop leunen (`BuffCheck2`) gedragen zich daardoor goed.
- `0a3c069` (2163): `SetWorldDetail`/`GetWorldDetail` als echte verbs met de
  drie stops van de reference (pfUI's `hdgraphic`-module).

### 2.4 Een addon-harness als meetlat

`crates/benilla-app/src/addon_harness/` (+ `examples/addon_harness.rs`) laadt
echte addons tegen hun UI-runtime; het project werkt met een **corpus van 218
addons** en genummerde contract-issues (2163-2192 in deze reeks).

## 3. Mapping naar onze open fouten

| Onze observatie (bron) | benilla-bewijs | Onze seam | Richting |
|---|---|---|---|
| `pfUI/skins/blizzard/options-video.lua:39` `slider` nil (log 10-9 13:28:59) | `6d66329` (2177) | `ui/game/runtime/framexml_runtime_loader.cpp`, `frame_materializer.cpp`, `frame_store.cpp` | reference-`OptionsFrame` met zijn `OptionsFrameSlider1..9` als echte Lua-globals beschikbaar maken; eigen optiescherm onder een andere naam |
| `CreateFrame: Unknown frame type 'LootButton'` (`pfUI/modules/loot.lua:530`) | `3e92b4f` (2191) | `ui/widgets/script_object.cpp:79` (register), `frame_materializer.cpp:879` | `LootButton` als geldig type registreren -> Button-runtime (XML-pad doet dit al: `framexml_parser.cpp:783-809`) |
| `autoshift.lua:62: 'string.gfind' was renamed to 'string.gmatch'` | `5af3a15` (2171) + `BENILLA.md` | `ui/lua_base_overrides.h:560,674` | 5.0-`gfind` echt laten werken (alias op `gmatch`); de 5.1-deprecation-stub is 5.1-gedrag en hoort niet in een 1.12-doel |
| `attempt to call a table value` in `OptionsFrame.lua`, `ChatFrame.lua`, `UIParent.lua`, `QuestLogFrame.lua`, `DurabilityFrame.lua` (eerder de "ABI/returntype"-categorie van de UI-audit; zie `docs/BENILLA_DEVIATIONS.md`) | `BENILLA.md` (183/218 addons, 1.163 sites) | `ui/lua_base_overrides.h:372` `RewriteLua50TableIterators` + `third_party/wow_lua` | waarschijnlijk dezelfde opcode; onze regex-bronrewrite is de door benilla afgewezen aanpak |
| glue `GlueWidgetRuntime.SetEnabled: unknown widget: VideoOptionsEffectsPanel*` / `AudioOptionsSoundPanelUseHardware` | `6d66329` (2177) | `apps/client/glue_host/*`, glue-widget-runtime | reference-GlueXML-widgets bij naam leveren i.p.v. een eigen widgetlijst |
| Lokale workaround `Client\...\pfUI\modules\hdgraphic.lua` (`_G.OptionsFrameSliders[3]`-guard, backup aanwezig) | `0a3c069` (2163) | `SetWorldDetail`/`GetWorldDetail`-verbs | workaround hoort te verdwijnen zodra de verbs bestaan (wijzigen in `Client` blijft verboden) |

## 4. Beslispunt: welke Lua-dialectstrategie?

**Geen 5.0-VM, geen herschrijving, 5.1.1 blijft.** De vraag is alleen welke
5.0-gedragingen we in 5.1.1 terugzetten.

| Optie | Wat | Beoordeling |
|---|---|---|
| A. Kleine VM-compat (aanbevolen) | 5.0-gedrag herstellen voor de verwijderde opcode + de twee parser-/compat-punten, in `third_party/wow_lua`, achter een expliciete compat-macro | sluit aan op `AGENTS.md` ("kleine runtime-/VM-correctie"), maakt `RewriteLua50TableIterators` overbodig, geen ABI-wijziging |
| B. Volledige Lua 5.0 inbouwen | andere VM + andere stdlib-oppervlakte | grote ingreep, breekt bestaande 5.1-API's in de WIP; benilla koos dit expliciet **niet** |
| C. Bronrewrite houden | huidige `RewriteLua50TableIterators` | door benilla afgewezen (stille gedragsverandering bij verkeerde parse) en in strijd met onze eigen regel over bronrewrites |

Aandachtspunten bij A:

- Onze VM is **5.1.1**, benilla's hunk is geschreven op **5.1.5**; `lvm.c` moet
  daarvoor vergeleken worden.
- Elke hunk krijgt een eigen, genummerd register in de stijl van
  `third_party/lua-src/BENILLA.md` (bijv. `third_party/wow_lua/OPENWOW.md`) met
  een verificatiecommando dat aantoont dat er niets anders is gewijzigd.
- Niet blind `select`/string-metatable verwijderen: dat is een apart
  contractpunt per gedraging, te toetsen tegen de lokale client en onze addons.
- Regressiechecks die de reference-details dekken: iterator-loze generic-for
  werkt; `for k,v in pairs(t)` en `for k,v in next,t` blijven werken;
  `next = myfn` beinvloedt latere loops; userdata wordt niet gesubstitueerd.
- Pas daarna de bronrewrite uitfaseren (of inert maken), zodat dezelfde chunk
  niet twee keer wordt aangepast.

## 5. Voorgestelde werkwijze

1. **Meetlat eerst**: `apps/client/scenarios/scenario_runner.cpp` uitbreiden met
   een addon-harness-modus die `Client\Interface\AddOns` tegen de offline
   world-fixture laadt en per addon Lua-fouten telt (bestaande tellers:
   `world_ui_player_portrait_ready`, `world_ui_unit_frames_ready`,
   `world_ui_action_icon_ready`). Daarmee worden foutklassen reproduceerbaar
   zonder server.
2. **Per contract een kleine fix op de gedeelde seam**, nooit per addon.
3. **Rapporteren met (a) afwijking, (b) bronpad, (c) kleinste wijziging,
   (d) impact/terugdraaibaarheid, (e) benodigde controle** - zoals `AGENTS.md`
   vraagt.

## 6. Wat we expliciet niet doen

- Geen Lua 5.0-VM en geen herschrijving van de 5.1.1-bron als "oplossing".
- Geen wijzigingen in `Client`, `Source`, `benilla`, MPQ's of serverdata.
- Geen addon-specifieke uitzondering in native code (geen "pfUI-modus").
- Geen workaround die de echte oorzaak maskeert; de bestaande lokale
  `hdgraphic.lua`-patch in `Client` wordt alleen **gerapporteerd**, niet aangeraakt.

## 7. Openstaande metingen

- Huidige foutlijst met pfUI aan: één run met `OPENWOW_LOG_LEVEL=warn`
  (standaardniveau is `kError`, zie `apps/client/composition/main.cpp:494`;
  niveaus in `src/openwow/foundation/diagnostics/logging.cpp:190-215`).
- Welke `OptionsFrameSlider*` ontbreekt werkelijk, en of de WIP-fix
  (`ReconcileDefaultLuaGlobals`) dat al dekt.
- Of de framebuffer-fout van het 3D-target-portret (`ModelPortrait`,
  `ui/game/runtime/render/ui_compositor.cpp`) nog optreedt in de build van
  2026-09-12 13:20.
