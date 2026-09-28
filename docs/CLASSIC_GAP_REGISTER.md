# Classic/Turtle gap-register

Doel: één plek waar staat **wat er nog gecorrigeerd moet worden** om de client van
3.3.5-erfenis naar Classic/Turtle-gedrag te brengen, met per item de bron en de
status. Dit vervangt het bug-voor-bug werken.

Bijgehouden: 2026-09-26. Alle items hieronder zijn in deze worktree geverifieerd
tegen de lokale Client, Source of Benilla; herkomst staat per item vermeld.

## Statuslegenda

| Status | Betekenis |
|---|---|
| `gereed` | Gewijzigd, gebouwd en (waar vermeld) in-game bevestigd |
| `open` | Afwijking bevestigd, nog niet gewijzigd |
| `latent` | Afwijking aanwezig maar raakt de huidige flow nog niet |
| `schoon` | Onderzocht en correct bevonden — niet opnieuw uitzoeken |
| `infra` | Build- of gereedschapsprobleem, geen clientgedrag |

---

## 1. Protocol-bodies — het grootste blok

Bron: `docs/PROTOCOL_AUDIT_335_VS_112.md` (~60 bevindingen, machinematige
opcodevergelijking plus veld-voor-veld bodyvergelijking tegen
`D:\OllieWoW\Source\src\game\**`).

Status: **deels gereed**. Steekproef 2026-09-26 op A1 en C1: die waren nog open.
Daarna is de beweging-slice opgepakt.

| # | Item | Status |
|---|---|---|
| A1 | `SMSG_FORCE_RUN_SPEED_CHANGE` slaat een extra byte over | **gereed** — de skip is weg; de server stuurt `packguid + u32 counter + float` (`MovementPacketSender.cpp:57-66`) voor *alle* `SMSG_FORCE_*_SPEED_CHANGE`, geen extra byte |
| A2 | `SMSG_MONSTER_MOVE` leest een Animation-bit dat 1.12 niet heeft | **gereed** — zie hieronder; het was groter dan dit item |
| A3 | Cyclische splines wissen hun fake eerste vertex niet | **gereed** — zelfde oorzaak als A2 |
| C1 | `SMSG_GROUP_LIST` leest 4 header-bytes i.p.v. 2 | **open** — `group_manager.cpp:15-21` |
| overige ~56 | zie `PROTOCOL_AUDIT_335_VS_112.md` | **open** |

### De beweging-slice: de hele spline-vlaggentabel was WotLK

Wat het audit-item A2 als één bit beschreef, bleek de **hele tabel**. OpenWow's
`SplineFlag` (`monster_move.h`) gebruikte post-Classic bitposities:

| 1.12 (`MoveSplineFlag.h:38-78`) | waarde | OpenWow's naam op die waarde |
|---|---|---|
| `Done` | 0x00000001 | — |
| `Falling` | 0x00000002 | — |
| `Runmode` | 0x00000100 | `kDone` |
| `Flying` | 0x00000200 | `kFalling` |
| `Cyclic` | 0x00100000 | `kEnterCycle` |
| `Enter_Cycle` | 0x00200000 | `kAnimation` |
| `Frozen` | 0x00400000 | `kFreeze` ✓ |

Live gevolgen:
- `IsCyclic()` testte 0x00080000 waar 1.12 `Cyclic` op 0x00100000 zet → **een
  cyclische spline werd nooit als cyclisch herkend**, dus `EnterCyclicLoop()`'s
  erase-first-vertex draaide nooit. Dat is audit-item A3, en dit is de oorzaak.
- `HasTriggeredAnimationTier()` testte 0x00200000, wat de server op **elke**
  cyclische spline zet (`packet_builder.cpp:71-72`, "add fake Enter_Cycle flag")
  → cyclische splines kregen een animatietier met id 0.
- De animatie-lees in `monster_move.cpp:78` las 5 bytes die nooit geschreven
  waren → de waypoint-count landde op de verkeerde offset.
- `Falling`/`Flying` waren verwisseld, dus vallende en vliegende splines werden
  verkeerd geclassificeerd voor de animatiekeuze.

Fix: tabel naar de 1.12-waarden, alle consumers gelijkgetrokken, en de
WotLK-only mechanismen verwijderd (spline-animatietier, backward-splinevlag).
`kOrientFixed` is behouden op 0x00004000: die bit bestaat in 1.12, alleen
naamloos (`Unknown15`).

**Nog niet in-game getest.**

Aanpak voor de rest: de audit per item doorlopen en status zetten. Dit is
mechanisch werk met een bestaande bron; geen nieuw onderzoek nodig.

---

## 2. WotLK-subsystemen die nog aanwezig zijn

Status: **open**. Grotendeels dood, maar ze vervuilen de codebase en de DBC-lijsten.

| Subsysteem | Omvang | Bewijs |
|---|---|---|
| achievements | ~950 refs | `src/openwow/game/achievements/` |
| vehicles | ~2400 refs | `Vehicle.dbc`/`VehicleSeat.dbc` bestaan niet in de client |
| glyphs | ~256 refs | `GlyphSlot.dbc`/`GlyphProperties.dbc` bestaan niet |
| barbershop | ~292 refs | `BarberShopStyle.dbc` bestaat niet |
| currency | ~72 refs | `CurrencyTypes.dbc` bestaat niet |
| Death Knight | ~26 refs | `CharCreate_GetDeathKnightModelName`, `creature_sound.cpp:135` |
| battlenet/comsat | ~450 refs | `battlenet_login.cpp`, `comsat_client.cpp` |

Bewijs dat de tabellen ontbreken: read-only extractie uit de client-MPQ's
(`artifacts/dbc-audit`), allemaal MISSING.

---

## 3. Dode mechanismen — "intentie die nooit is aangesloten"

Dit is het patroon dat het bugfixen eindeloos deed lijken: elke keer dat we een bug
aanraakten, bleek er een half-afgemaakte laag onder te zitten.

Status: **open**, en dit is de hoogste opbrengst per uur.

| Mechanisme | Afwijking | Bewijs |
|---|---|---|
| `BuildTextureRenderStateFromLuaFieldsInto` | Nooit aangeroepen; bevatte de vertexkleur-regel die daardoor nooit werkte | `lua_frame_projection.cpp:1075` — enige declaratie + definitie, nul callsites |
| `SetTimestampCleanup` | Had nul aanroepers; daardoor kon een `kPendingDestroy`-effect nooit aflopen | `ceffect_c.h:206` |
| `kEffectSoundSuppressed` / `kSuppressKitSound` (0x400) | Wordt gelezen, nooit gezet → suppressie is dood | `spell_visual_renderer.cpp:2017`, `ceffect_c.cpp:511` |
| `kSoundModeOne` (0x1) | Wordt gelezen, nooit gezet | `ceffect_c.cpp:519` |
| `RaceClassInfoManager::RegisterWotLKDefaults` | Nooit aangeroepen; bevat hardcoded WotLK-races/classes | `race_class_info.cpp:116` |
| `era/era_registry.cpp` | Nooit aangeroepen; alleen een `wotlk-3.3.5`-profiel, geen Turtle | `src/openwow/era/` |

---

## 4. Naamgebonden tabellen

Bron: strings uit `D:\OllieWoW\Client\WoW.exe` (4,68 MB, x86 PE, build
`0x4510B6DB`), read-only gelezen. De binary is niet gestript van deze namen.

### 4a. `HARDCODED`-effectnamen

Client heeft **14**, OpenWow's tabel (`object_effect_system.cpp:849-862`) heeft **12**.

Status: **open**.

| Richting | Namen |
|---|---|
| In de client, niet in OpenWow | `HARDCODED PetLoyalty Down Base`, `PetLoyalty Down Head`, `PetLoyalty Up Base`, `PetLoyalty Up Head` |
| In OpenWow, niet in de client | `HARDCODED Resist Spell`, `HARDCODED Achievement Base` (WotLK-erfenis, matchen niets) |

Gevolg: pet-loyalty op/af-effecten spelen nooit (`grep PetLoyalty` in
`src/openwow` = 0 treffers). Kleinste wijziging: 4 namen toevoegen, 2 fantomen
eruit, `kHardcodedEffectIdCount` van 12 naar 14.

**Open vraag:** de attachment-points van die vier staan niet in de strings en moeten
uit de DBC of de binary komen.

### 4b. `DONOTRENAME`-geluidskits

Client heeft **10**, OpenWow heeft er **3** als constante
(`combat_sounds.cpp:68-69`, `unit_sound_dispatch.cpp:64`).

Status: **open, te triëren**. Niet elk ontbrekend item is een gat — sommige lopen via
tabelopzoeking in plaats van op naam.

| In de client | In OpenWow? |
|---|---|
| `(DONOTRENAME)Combat Miss 1H` / `2H` | ja |
| `SpiritWolf (DONOTRENAME)` | ja |
| `(DONOTRENAME)AbsorbGetHit` | nee |
| `(DONOTRENAME)ShieldWoodImpact` | nee |
| `(DONOTRENAME)WrappedItemDrop` / `Pickup` | nee |
| `Ghost` / `Underwater` / `GhostMusic (DONOTRENAME)` | nee — waarschijnlijk via ZoneMusic/SoundAmbience, dus **geen gat** |

---

## 5. Animatie

| Item | Status | Bewijs |
|---|---|---|
| Skill-rij-trough (vol blauw i.p.v. rank) | **gereed** | `lua_frame_projection.cpp` — vertexkleur vermenigvuldigt nu met de authored `<Color>`; authored kleur wordt gediscard op textures met `file=`. In-game bevestigd door eigenaar. |
| Eenmalige effecten blijven eeuwig staan | **gereed** | `unit_spell_visual_runtime.cpp` + `ceffect_c.cpp` — deadline uit de file-order-eerste M2-sequentieduur (Benilla's `first_seq_span`). Nog niet in-game getest. |
| **Wond-reactie vervangt de swing** | **open** | `PlayWoundReaction` → `SubmitRawPlayback` (`unit_animation_runtime.cpp:2114`) overschrijft `playback_request_`. Benilla legt de flinch in een aparte SECONDARY-slot (`driver/wound.rs:85-92`) die de base/one-shot nooit raakt. |
| Geen derde kanaal | **open** | M2 heeft 35 slots met smoothstep-blend, maar zonder amplitude-veld; `PlaybackRequest` heeft alleen base + upper-body. |

---

## 6. Geluid

| Item | Status | Bewijs |
|---|---|---|
| `$SND` op area-modellen: 3 van 4 weggegooid | **gereed** | `spell_visual_system.cpp:1993` had `(counter & 3u) == 0` met een globale teller. Benilla rinkelt onvoorwaardelijk per event (`sound/anim_events.rs:83-87`, gate is alleen `data != 0`). Teller + dood lid verwijderd. Nog niet in-game getest. |
| **Kit-geluid speelt twee keer** | **open** | `BuildDestLocAreaEvents` (`spell_packet_visual_dispatch.cpp:51-95`) maakt twee events uit één kit, elk met `.sound_kit_id = kit->sound_id`. **752 van 1967 kits** vullen beide slots én hebben geluid. Benilla dedupliceert per frame op `(entity, kit_sound)` (`sound/spell.rs:48-60`). |
| `sound_entries_advanced_id` altijd 0 | **latent** | Schema leest veld 29, tabel heeft 29 velden (0-28) → buiten bereik → altijd 0. `SoundEntriesAdvanced.dbc` bestaat niet in 1.12, dus impact klein. |

---

## 7. Data en DBC

Status: **schoon** — hier is geen werk meer.

- **54 DBC-headers** read-only geëxtraheerd en vergeleken met
  `dbc_retail_catalog.inc`: alle `field_count`/`record_size` exact gelijk.
- **`Spell`** (173 velden) offset-voor-offset tegen `Spellfmt` uit de serverbron:
  klopt, inclusief de gelokaliseerde stringblokken.
- **`SoundEntries`** kolomsemantiek geverifieerd tegen echte data: id 1143 =
  `Mining Impact` / `MiningHitA.wav` / `Sound\Spells\Tradeskills`, volume 0,69.
- **`SpellVisualKit` veld 14** is géén geluid maar een visual-group fallback —
  Benilla leest hem ook niet. OpenWow slaat hem terecht over.
- **`SpellVisual` veld 10 en 14** correct gemapt en gebruikt.
- **`ChrRaces`** veld 6/7 zijn `ClientPrefix`/unused — correct overgeslagen.

---

## 8. Assets en renderer

Status: **schoon** voor wat onderzocht is.

| Item | Bevinding | Bewijs |
|---|---|---|
| BLP1-support | Niet nodig | Turtle levert BLP2 — 7 UI/icon/castbar-textures gecontroleerd, alle `BLP2` |
| M2 v256 | Ondersteund | `m2_model.cpp:748` accepteert v256 én v264 met aparte Classic-paden |
| ADT | Ondersteund | `adt_file.cpp:88` accepteert 17/18/19 |
| Update fields | Classic-correct | `update_fields.h`: `UNIT_END`=188, `PLAYER_END`=1282, WotLK-currencies op `PLAYER_UNSUPPORTED_FIELD` |

---

## 9. Infrastructuur

Status: **infra**.

| Item | Bevinding |
|---|---|
| CMake-reconfigure hangt | `ninja` blijft staan op `[0/2] Re-checking globbed directories...` — `dev.ps1` documenteert dit zelf. 9 minuten geprobeerd, niet doorgekomen. |
| 5 ontbrekende response-files | `storm.rsp`, `openwow_game.rsp`, `openwow_ui_runtime.rsp`, `openwow_render_bgfx.rsp`, `openwow-client.rsp` ontbraken; gereconstrueerd uit `build.ninja`. Elke build heeft ze nodig. |
| `openwow_render_bgfx.lib` ontbrak | Bestond niet; opnieuw gebouwd. |
| `dev.ps1` meldde succes bij een gefaalde link | Bij een `LNK1104` (de draaiende client hield `OllieWoW.exe` vast) stond er tóch `DEV_BUILD_OK` plus de **oude** exe-tijd. De oorzaak is niet vastgesteld: de gegenereerde `dev-link.bat` heeft per link wél `if errorlevel 1` plus een `findstr`-vangnet. Toegevoegd als postconditie die daar niet van afhangt: het script controleert nu of de exe daadwerkelijk opnieuw geschreven is. |
| Een teruggezette bron wordt niet hercompileerd | `dev.ps1` bepaalt zijn werkset uit `git status --porcelain`; een met `git checkout` teruggezette `.cpp` staat daar niet meer in en wordt dus overgeslagen, ook al is zijn object ouder dan de bron. Gebruik dan `dev.ps1 -All`; het script waarschuwt nu wanneer alle geraakte objecten al nieuwer waren. |

---

## 10. Geverifieerd schoon — niet opnieuw uitzoeken

- Opcode-**nummers**: 0 van de gedeelde namen wijkt af.
- Animatieroutes: alle 17 `UnitAnimationEventRoute`-waarden worden geproduceerd
  én afgehandeld; geen stubs.
- `$CST`/`$CSL`/`$CSR` routeren naar `kSpellContact`, **niet** naar geluid —
  correct volgens Benilla (`anim_events.rs:25-28`).
- `$SND` op **aangehechte** effectmodellen loopt via `ceffect_c.cpp:364` →
  `DispatchSpellVisualM2Event`, dat onvoorwaardelijk afspeelt. Was nooit stuk.
- FontString-kleur gebruikt een apart veld (`__ow_text_*`), niet het
  texture-vertexkleurpad.
- De locatie van het level-up-effect: `unit_descriptor_callbacks.cpp:222`.

### Glue-UI: half scherm terug na een wereld-bezoek — opgelost, niet opnieuw uitzoeken

Symptoom: na `Logout` naar het karakterselect (en daarna het loginscherm) waren het model en de
achtergrond er wel, maar de hele XML-UI (logo, panelen, rijen) niet.

Oorzaak (28-9-2026, `src/openwow/render/ui/ui_renderer.cpp`): bgfx' scissor is **plakkend per
view** en `bgfx::setViewRect` reset hem niet. `UiRenderer::Begin` zette framebuffer, clear,
view-mode, transform en rect, maar niet de scissor; `SubmitRun` zette hem alleen als een draw
geclipt was, en nooit terug. Eén geclipte widget liet zijn clip dus staan voor élke volgende
ongeclipte draw in die view (en over frames heen): de draws werden gesubmit en kwamen nergens
terecht. Herstel: volle view-scissor in `Begin` plus een `else` in `SubmitRun`.

Tweede reparatie (`apps/client/glue_host/glue_client.cpp` en `src/openwow/ui/glue/cgluemgr.cpp`):
de TOC-herlading op de terugweg bouwt alle 2751 widgets opnieuw, waardoor de rij-FontStrings leeg en
verborgen terugkomen. `CharacterSelect_OnShow` vraagt in de connected tak alleen een refresh aan
(`CharacterSelect.lua:69-73`) en vult zelf niets; de rijen worden uitsluitend door het
`CHARACTER_LIST_UPDATE`-event gevuld en getoond (`CharacterSelect.lua:177-182`, `225-254`).
Zolang het serverantwoord onderweg is blijft de lijst dus leeg. De roster blijft nu bewaard over het
wereld-bezoek (referentie: benilla overschrijft `roster.chars` in `char_select/mod.rs:328` en
`back_on_logout` wist alleen de pending pick) en op de terugweg wordt `CHARACTER_LIST_UPDATE`
opnieuw gevuurd. Meetbaar: `with_text` 8 → 25 op de terugkeer-frame zelf.

Met bewijs uitgesloten, dus niet opnieuw uitzoeken: FontFace-levensduur, tekst-/font-/atlas-pad,
scriptcache- en OnUpdate-staleness, transient-bufferbudget, stale framebuffer, diepte-only wipe door
de model-pass, globale alpha, de tekenorde vóór de model-pass (de segmentverdeling was `1/N` in
beide toestanden), de zes stille `return false`-paden in `UiRenderer`, en vernietigde GPU-handles.
Het ras is geen discriminator: in de goede referentie stond de Tauren óók geselecteerd.

---

## 11. Methodes om dit register aan te vullen

Deze sweeps zijn mechanisch en leverden al op:

| Sweep | Opbrengst | Les |
|---|---|---|
| **String-diff tegen `WoW.exe`** | Vond de PetLoyalty-gap en de 2 fantomen | Hoogste opbrengst per uur; de client is niet gestript van naamtabellen |
| **RTTI-diff** | 49 klassenamen | **Levert valse positieven.** `CSimpleHyperlinkButton` ontbreekt in OpenWow, maar de client-XML/Lua gebruikt hem **nul** keer — dus geen gat. Altijd verifiëren tegen echte XML/Lua. |
| **Route/flag-sweep** | Schoon (geen dode routes) | Nuttig als negatieve controle |
| **Dode-code-scan** | Vond de 6 mechanismen in sectie 3 | Nog niet geautomatiseerd; nu met de hand |

Nog te doen als methode: een geautomatiseerde scan voor "gedefinieerd maar nooit
aangeroepen" en "gelezen maar nooit gezet" over de hele boom.

---

## Bronnen

- `D:\OllieWoW\Client` — primair voor glue/UI en assets (read-only)
- `D:\OllieWoW\Source` — autoriteit voor serverdata en packetbodies
- `D:\OllieWoW\benilla` — Vanilla/Turtle-referentie voor clientgedrag
- `D:\OllieWoW\Client\WoW.exe` — originele client, voor naamtabellen en RTTI
- `docs/PROTOCOL_AUDIT_335_VS_112.md` — protocol-bodies (sectie 1)
- `docs/BENILLA_DEVIATIONS.md` — eerdere batches met onderbouwing
