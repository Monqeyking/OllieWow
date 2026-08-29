# OllieWoW / Turtle 1.18.1 portplan voor OpenWoW

## Doel

OpenWoW geschikt maken als vervangende client voor de lokale OllieWoW-server en
de Turtle WoW-clientdata onder `D:\OllieWoW\Client`.

De concrete doelomgeving is:

- clientversie: Turtle WoW 1.18.1, build 7272;
- serverbron: `D:\OllieWoW\Source`;
- serverruntime: `D:\OllieWoW\Server`;
- clientdata: `D:\OllieWoW\Client`;
- te wijzigen project: `D:\OllieWoW\Experiments\OpenWow-snapshot`.

De serverbron is een Turtle/Tortoise-WoW-fork op MaNGOS Zero/vMaNGOS-lijn.
Gebruik daarom de lokale serverbron als primaire waarheid. Gebruik geen
generieke cMaNGOS-, vMaNGOS- of AzerothCore-documentatie als die afwijkt van
wat de lokale server daadwerkelijk leest of schrijft.

## Primaire MVP: een zelf testbare verticale doorsnede

De eerste prioriteit is nadrukkelijk niet volledige gameplay-parity. De eerste
bruikbare versie moet de eigenaar zelf interactief kunnen testen met deze flow:

```text
client starten
  -> account invoeren en authenticeren
  -> realm zien en selecteren
  -> character list zien
  -> bestaand character selecteren
  -> eventueel een character maken
  -> world enter voltooien
  -> terrein, gebouwen, doodads, characters, creatures en UI renderen
  -> lopen, draaien, springen, zwemmen en camera bewegen
  -> uitloggen en opnieuw verbinden zonder corrupte state
```

Voor deze MVP hoeft nog niet alles te werken. Auction house, mail, guilds,
battlegrounds, volledige combat, alle quests en alle Turtle-addonfuncties mogen
na de verticale doorsnede komen. Login-, character-, world-, rendering- en
movementproblemen hebben voorrang op iedere brede gameplayfeature.

### MVP-acceptatiecriteria

- De executable gebruikt read-only de bestaande Turtle-clientdata.
- Login tegen OllieWoW lukt zonder wijziging van serverprotocol of database.
- De realm list toont de lokale realm correct.
- Character enum toont naam, race, class, level, uiterlijk en equipment zonder
  packet under-read of over-read.
- Een bestaand character kan worden geselecteerd.
- Character creation werkt minimaal voor één stock race/class. Turtle-races
  volgen zodra hun DBC- en modelpad stabiel is.
- World enter bereikt een stabiele in-world state.
- De gekozen zone toont geen leeg scherm of alleen placeholdergeometrie.
- Terrein, WMO's, doodads, het eigen character en minstens één creature zijn
  zichtbaar en zinvol getextureerd.
- Idle-, walk-, run- en jumpanimaties werken minimaal.
- Vooruit/achteruit, strafing, draaien, springen en camera-input worden door de
  server geaccepteerd en door andere objectupdates niet direct overschreven.
- De client blijft minimaal tien minuten bruikbaar tijdens lopen door een
  representatieve stock zone.
- Uitloggen en opnieuw inloggen werkt zonder herstart van server of database.

Een feature die compileert maar deze zichtbare flow niet dichterbij brengt,
krijgt vóór de MVP geen prioriteit.

## Verplichte werkwijze voor iedere taak

1. Lees eerst `D:\OllieWoW\AGENTS.md` als dat bestand aanwezig is.
2. Inspecteer read-only en rapporteer de bevindingen vóór wijzigingen.
3. Beschrijf de kleinste wijziging, de exacte bestanden, impact en
   terugdraaibaarheid.
4. Vraag expliciete toestemming voordat broncode, tests, buildbestanden of
   gegenereerde bestanden worden gewijzigd.
5. Eén taak-ID per sessie, tenzij de gebruiker uitdrukkelijk meerdere taken
   goedkeurt.
6. Houd bestaande lokale wijzigingen intact. De OpenWoW-worktree is al dirty.
7. Wijzig nooit zonder afzonderlijke toestemming:
   - `D:\OllieWoW\Source`;
   - `D:\OllieWoW\Server`;
   - de database, accounts of characters;
   - `D:\OllieWoW\Client\Data` of MPQ-bestanden;
   - draaiende server- of clientprocessen.
8. Start geen GUI, client of server als diagnostische shortcut.
9. Een build of test die artifacts maakt vereist eveneens expliciete
   toestemming. Bouw uitsluitend in de bestaande OpenWoW-buildmap.
10. Rapporteer na iedere taak: gewijzigd, niet gewijzigd, verificatie en
    resterende risico's.

## Huidige nulmeting

- OpenWoW upstream richt zich op WoW 3.3.5a build 12340.
- Er zijn momenteel 34 lokaal gewijzigde OpenWoW-bestanden plus een nieuw
  icoonbestand.
- De huidige wijzigingen omvatten ongeveer 563 toevoegingen en 660
  verwijderingen.
- Een bestaande build produceerde `OllieWoW.exe`.
- Een bestaande run vond de Turtle MPQ-keten en meldde 149 geladen DBC-tabellen.
- Dat DBC-resultaat bewijst nog niet dat de veldindelingen correct zijn.
- Van 774 opcode-namen die server en client delen hebben 772 al dezelfde
  numerieke waarde.
- `update_fields.h` is momenteel een ongeldige mix van classic- en
  WotLK-offsets.
- De M2-loader accepteert alleen WotLK v264.
- De BLP-loader accepteert alleen BLP2.
- Er is nog geen bewijs van een geslaagde OpenWoW-login, character list of
  world enter tegen OllieWoW.

## Ontwerpbesluit

Voeg een expliciet clientprofiel toe in plaats van steeds meer losse classic
voorwaarden in de WotLK-code te plaatsen:

```text
OpenWoW engine
|-- wotlk_3_3_5a
`-- turtle_1_18_1
    |-- auth en realm list
    |-- world protocol en crypto
    |-- opcodes en packet layouts
    |-- update fields en object layouts
    |-- DBC schemas
    |-- classic asset formats
    `-- classic/Turtle UI en addon API
```

Niet alle namespaces hoeven direct te worden hernoemd. Het belangrijkste is
dat versieafhankelijke tabellen, parsers en serializers niet langer stilzwijgend
dezelfde structs delen.

## Moeilijkheid en aanbevolen uitvoerder

| Label | Betekenis | Aanbevolen uitvoerder |
|---|---|---|
| L | Afgebakend, mechanisch, goed te controleren | Luna |
| M | Inhoudelijk werk met duidelijke lokale bron van waarheid | Luna, daarna review |
| H | Architectuur, binair formaat of protocolintegratie | Primair Codex/frontier |
| X | Reverse engineering of complexe cross-layer debugging | Codex/frontier, Luna alleen voor deelonderzoek |

## Fase 0 - Veiligstellen en meetbaar maken

### T00 - Bestaand werk veiligstellen

- Moeilijkheid: L
- Uitvoerder: Luna
- Afhankelijk van: niets
- Doel: de huidige dirty worktree herstelbaar vastleggen zonder bestaande
  wijzigingen te normaliseren of te herschrijven.
- Werk:
  - inventariseer alle gewijzigde en nieuwe bestanden;
  - maak na toestemming een gedateerde patch en een manifest met checksums;
  - leg commit, branch en buildartifact vast;
  - sla geen wachtwoorden, logs met accounts of andere secrets op.
- Gereed wanneer:
  - de patch met `git apply --check` controleerbaar is;
  - het manifest alle huidige lokale wijzigingen noemt;
  - er niets aan server, clientdata of database is veranderd.

### T01 - Compatibiliteitsmatrix aanmaken

- Moeilijkheid: L
- Uitvoerder: Luna
- Afhankelijk van: T00
- Doel: één tabel maken van alle subsystemen met kolommen `WotLK huidig`,
  `Turtle vereist`, `bron van waarheid`, `status`, `test` en `eigenaar`.
- Minimaal opnemen: auth, realm list, world framing, crypto, opcodes, character
  packets, object updates, movement, DBC, MPQ/VFS, BLP, M2, WMO, ADT, Lua,
  XML, addons, audio en video.
- Gereed wanneer ieder onderdeel een lokale bron en een concrete test heeft.

### T02 - Turtle-profiel ontwerpen

- Moeilijkheid: H
- Uitvoerder: Codex/frontier
- Afhankelijk van: T00 en T01
- Doel: bepalen waar compile-time en runtime profielgrenzen komen.
- Gereed wanneer er een klein ontwerpdocument ligt met bestandsgrenzen,
  naamgeving, selectie van het standaardprofiel en migratiepad voor bestaande
  wijzigingen.

### T03 - Turtle-profiel invoeren

- Moeilijkheid: M
- Uitvoerder: Luna na T02, daarna review door Codex/frontier
- Afhankelijk van: T02
- Doel: het goedgekeurde profielontwerp implementeren zonder gedrag te
  veranderen.
- Gereed wanneer beide profielen afzonderlijk selecteerbaar zijn en een smalle
  buildcontrole slaagt.

## Fase 1 - Protocolbron genereren

### T10 - Opcodegenerator uit serverheader

- Moeilijkheid: M
- Uitvoerder: Luna
- Afhankelijk van: T03
- Bron: `D:\OllieWoW\Source\src\game\Protocol\Opcodes_1_12_1.h`
- Doel: de Turtle-opcodetabel genereren, inclusief namen en maximale geldige
  waarde.
- Eisen:
  - gegenereerde output niet handmatig bewerken;
  - duplicate waarden en ontbrekende namen rapporteren;
  - WotLK-only opcodes niet in het Turtle-profiel registreren.
- Gereed wanneer een test alle serverwaarden één-op-één vergelijkt.

### T11 - Update-fieldgenerator uit serverheader

- Moeilijkheid: H
- Uitvoerder: Codex/frontier
- Afhankelijk van: T03
- Bron: `D:\OllieWoW\Source\src\game\Objects\UpdateFields.h`
- Doel: alle object-, item-, container-, unit-, player-, gameobject-, dynamic
  object- en corpsevelden exact overnemen.
- Eisen:
  - veldgroottes en eindwaarden behouden;
  - geen WotLK currencies, arena, rune of glyphvelden in Turtle;
  - compile-time assertions voor belangrijke offsets en `*_END`-waarden;
  - type masks en objecttypen tegelijk controleren.
- Gereed wanneer de gegenereerde waarden exact overeenkomen met een klein
  hulpprogramma dat tegen de serverheader compileert.

### T12 - Pakketlayout-inventarisatie

- Moeilijkheid: M
- Uitvoerder: Luna
- Afhankelijk van: T10
- Doel: voor alle door OpenWoW gebruikte CMSG/SMSG-pakketten de corresponderende
  server read/write-functie vinden.
- Output per packet:
  - opcode;
  - client serializer/parser;
  - server handler/builder met bestandsregel;
  - bekende layoutverschillen;
  - minimale fixture die nodig is.
- Geen protocolcode wijzigen in deze taak.

### T13 - Binaire protocolfixtures

- Moeilijkheid: H
- Uitvoerder: Codex/frontier
- Afhankelijk van: T10, T11 en T12
- Doel: testfixtures definiëren die vanuit de serverimplementatie worden
  afgeleid, zonder een live database of server nodig te hebben.
- Prioriteit: auth challenge, auth session, realm list, char enum, login verify
  world, update object en movement.

## Fase 2 - Login tot character list

### T20 - Realmd SRP6 en security flags

- Moeilijkheid: H
- Uitvoerder: Codex/frontier
- Afhankelijk van: T03 en T13
- Bronnen: lokale `AuthSocket.cpp`, `RealmList.cpp` en OpenWoW auth-code.
- Controleer:
  - 1.18.1/7272 challengevelden;
  - platform-, OS- en locale-bytevolgorde;
  - SRP6 proof en server proof;
  - PIN/security flags;
  - reconnectpad;
  - classic realm-list count en tail bytes.
- Gereed wanneer offline fixtures slagen en een live test afzonderlijk kan
  worden aangevraagd.

### T21 - World framing en classic AuthCrypt

- Moeilijkheid: H
- Uitvoerder: Codex/frontier
- Afhankelijk van: T13
- Doel:
  - server-naar-client header van 4 bytes;
  - client-naar-server header van 6 bytes;
  - juiste endianess;
  - classic rolling headercrypt met juiste send/receive state;
  - gefragmenteerde TCP-reads en meerdere frames per read testen.

### T22 - CMSG_AUTH_SESSION en addonblok

- Moeilijkheid: H
- Uitvoerder: Codex/frontier
- Afhankelijk van: T20 en T21
- Bron: lokale `WorldSocket.cpp` en `AddonHandler.cpp`.
- Doel: build, accountnaam, seeds, SHA1 proof en gecomprimeerde addoninformatie
  byte-exact maken.
- Gereed wanneer de server een fixture accepteert en de addonrespons correct
  parseerbaar is.

### T23 - Character enum/create/delete

- Moeilijkheid: M
- Uitvoerder: Luna na layoutreview door Codex/frontier
- Afhankelijk van: T22 en T12
- Controleer classic equipment-slotcount, petdata, first-login flags, appearance,
  Turtle-races en afwezigheid van WotLK customizationvelden.
- Gereed wanneer fixtures voor minimaal één normaal character, één Turtle-race,
  een dode character en een character met pet slagen.

## Fase 3 - DBC en statische clientdata

### T30 - DBC-catalogus vergelijken

- Moeilijkheid: M
- Uitvoerder: Luna
- Afhankelijk van: T01
- Bronnen:
  - Turtle MPQ-data, read-only;
  - lokale `DBCStores.cpp` en `DBCStructure.h`;
  - OpenWoW DBC-catalogus.
- Doel: per tabel records, fields, record size, string block, vereiste status
  en consumerende code vastleggen.
- Geen MPQ wijzigen of uitpakken naar de clientmap.

### T31 - Exacte Turtle DBC-schema's

- Moeilijkheid: X
- Uitvoerder: Codex/frontier
- Afhankelijk van: T30
- Doel: de huidige optionele WotLK-loader vervangen door strikte,
  tabelspecifieke Turtle-schema's.
- Hoge-risicotabellen: `Spell`, `ChrRaces`, `ChrClasses`, `CharSections`,
  `CharStartOutfit`, `Map`, `AreaTable`, `CreatureDisplayInfo`,
  `CreatureModelData`, `ItemDisplayInfo`, talents, skills, taxi en lights.
- Gereed wanneer iedere gelezen kolom binnen het schema valt en representative
  Turtle-records inhoudelijk worden gecontroleerd.

### T32 - DBC-consumers opschonen

- Moeilijkheid: M
- Uitvoerder: Luna per kleine tabelgroep
- Afhankelijk van: T31
- Doel: WotLK-only aannames uit character creation, spells, mapselectie,
  modellen en UI verwijderen of profielgebonden maken.

## Fase 4 - Assetformaten en renderer

### T40 - BLP1 inventarisatie en decoder

- Moeilijkheid: H
- Uitvoerder: Codex/frontier; Luna kan vooraf testbestanden catalogiseren
- Afhankelijk van: T01
- Doel: BLP1 JPEG/paletted varianten en mipmaps ondersteunen naast BLP2.
- Eisen:
  - boundschecks op offsets en lengtes;
  - alpha depths correct behandelen;
  - geen bestaande BLP2-regressie;
  - fixtures uit eigen Turtle-data, zonder assets in Git op te nemen.

### T41 - Classic M2 v256 formaatonderzoek

- Moeilijkheid: X
- Uitvoerder: Codex/frontier
- Afhankelijk van: T01 en T30
- Doel: byte-exact verschil vastleggen tussen classic/Turtle M2 v256 en de
  bestaande WotLK v264-loader.
- Onderzoek minimaal: header, arrays, animations, bones, vertices, textures,
  render flags, views/skins, particles, ribbons, cameras en collision.
- Dit is eerst een onderzoekstaak; nog geen brede rendererwijziging.

### T42 - Classic M2 v256 parser

- Moeilijkheid: X
- Uitvoerder: Codex/frontier
- Afhankelijk van: T41
- Doel: v256 naar een genormaliseerd intern model vertalen, zodat de renderer
  niet overal classic-branches krijgt.
- Gereed wanneer representative character-, creature-, item-, doodad- en
  spell-effectmodellen veilig parsen.

### T43 - Classic skin/views en render batches

- Moeilijkheid: X
- Uitvoerder: Codex/frontier
- Afhankelijk van: T42
- Doel: classic embedded views/skinindeling normaliseren naar de bestaande
  interne skin geometry en render batches.
- Test transparantie, geosets, character equipment en meerdere texture units.

### T44 - WMO/ADT/WDL/WDT compatibiliteitsaudit

- Moeilijkheid: M
- Uitvoerder: Luna voor inventarisatie, Codex/frontier voor parserwijzigingen
- Afhankelijk van: T30
- Doel: met echte Turtle-bestanden vaststellen welke bestaande parsers al
  compatibel zijn en waar chunklayouts afwijken.
- Speciale aandacht: liquids, alpha maps, doodad placements, area IDs,
  vertex colors en custom Turtle-maps.

### T45 - Wereldrender smoke fixtures

- Moeilijkheid: H
- Uitvoerder: Codex/frontier
- Afhankelijk van: T40, T42, T43 en T44
- Doel: offline een vaste camera in één stock zone en één Turtle-zone renderen,
  zonder serververbinding.

## Fase 5 - World enter, objects en movement

### T50 - Classic update-object parser

- Moeilijkheid: X
- Uitvoerder: Codex/frontier
- Afhankelijk van: T11, T13 en T22
- Doel: update types, block header, has-transport byte, packed GUIDs, masks,
  create/movement/value layouts en out-of-range blocks exact implementeren.
- Gereed wanneer fixtures voor player, unit, gameobject, item, corpse en dynamic
  object slagen zonder handmatige offsetcorrecties.
- Status: gereed (2026-08-27).
- Verificatie: offline Classic parserfixtures slagen voor player, unit, item,
  gameobject, corpse en dynamic object; daarnaast zijn movement, transport,
  packed GUID, values, out-of-range, truncated en trailing-input gevallen
  afgedekt. De world-regressietrace toont dat de twee Classic units veilig tot
  en met M2-submit komen.
- Beperking: de volledige offline world-regressie blijft rood door bestaande
  FrameXML/UI-contractfouten (`OptionsFrame.lua:426` en ontbrekende world-UI
  anchors). Dit blokkeert de T50-parseracceptatie niet en valt buiten T50.

### T51 - Classic movement en splines

- Moeilijkheid: X
- Uitvoerder: Codex/frontier
- Afhankelijk van: T13 en T50
- Bron: server `MovementHandler`, `MovementInfo` en MoveSpline packet builder.
- Controleer movement flags, transport GUID, timestamps, swimming pitch, jump
  volgorde, zes speeds, spline facing, nodes en destination.

### T52 - World-enter sequentie

- Moeilijkheid: H
- Uitvoerder: Codex/frontier
- Afhankelijk van: T23, T50 en T51
- Doel: player login tot een stabiele in-world state, inclusief initial spells,
  action buttons, account data, time/speed, tutorial flags en eerste object
  updates.

### T53 - Basale world handlers

- Moeilijkheid: M
- Uitvoerder: Luna, steeds één packetfamilie per taak
- Afhankelijk van: T52
- Volgorde: chat, target/select, movement heartbeat, name query, creature query,
  item query, inventory, loot, gossip, vendors, trainers, spells en quests.
- Iedere familie vereist eerst fixtures en daarna implementatie.

## Fase 6 - Classic/Turtle UI en addons

### T60 - GlueXML/FrameXML API-audit

- Moeilijkheid: M
- Uitvoerder: Luna
- Afhankelijk van: T30
- Doel: ontbrekende XML-elementen, Lua globals, widgetmethoden, events en
  argumentverschillen inventariseren door de werkelijke Turtle UI-files te
  vergelijken met OpenWoW-registraties.
- Output sorteren op eerste gebruik tijdens login, character select en world UI.

### T61 - Login en character-select UI

- Moeilijkheid: H
- Uitvoerder: Codex/frontier voor integratie; Luna voor afgebakende API-functies
- Afhankelijk van: T23 en T60
- Doel: Turtle GlueXML zonder fatale Lua/XML-fouten laten draaien.

### T62 - In-world FrameXML minimum

- Moeilijkheid: X
- Uitvoerder: Codex/frontier
- Afhankelijk van: T52 en T60
- Doel: action bars, unit frames, chat, spellbook, bags, quests en tooltips
  bruikbaar maken vóór overige UI.

### T63 - Turtle addon-handshake

- Moeilijkheid: H
- Uitvoerder: Codex/frontier
- Afhankelijk van: T22 en T62
- Doel: `Turtle_General` en `Turtle_GroupUI` correct aanmelden en de
  serverrespons/public-keyflow afhandelen.

### T64 - Turtle addonberichten

- Moeilijkheid: M
- Uitvoerder: Luna per addonprotocol, na review
- Afhankelijk van: T63
- Volgorde: LFT, guildbank, shop/custom merchant, transmog en overige
  `SendAddonMessage`-flows.

## Fase 7 - Gameplay parity en stabiliteit

### T70 - Gameplay packetfamilies

- Moeilijkheid: M/H
- Uitvoerder: Luna voor geïsoleerde families, Codex/frontier voor integratiebugs
- Afhankelijk van: T53 en T62
- Families: combat log, aura's, cooldowns, groups, guilds, mail, auction house,
  trade, pets, talents, skills, taxi, battlegrounds, corpse/resurrection en
  instances.

### T71 - Turtle content edge cases

- Moeilijkheid: H
- Uitvoerder: Codex/frontier
- Afhankelijk van: T31, T43 en T70
- Controleer custom races, maps, display IDs, spell IDs, talent trees, items,
  sounds, cinematics en transports.

### T72 - Fuzz/bounds en corrupte packettests

- Moeilijkheid: M
- Uitvoerder: Luna
- Afhankelijk van: stabiele parsers uit eerdere fasen
- Doel: parsers mogen op afgeknotte, te grote of inconsistente input nooit
  buiten bounds lezen of onbeperkt alloceren.

### T73 - Regressiematrix

- Moeilijkheid: M
- Uitvoerder: Luna
- Afhankelijk van: alle vereiste fasen
- Minimaal testen:
  - schone startup;
  - auth en reconnect;
  - realm en character flows;
  - stock en Turtle-race;
  - stock en Turtle-zone;
  - movement/transport/swim/fall;
  - combat, loot, quest, vendor en trainer;
  - addons aan/uit;
  - windowed/fullscreen en meerdere resoluties;
  - herhaald connect/disconnect zonder statelek.

## Mijlpalen

| Mijlpaal | Definitie |
|---|---|
| M0 | Huidige worktree veiliggesteld en profielen gescheiden |
| M1 | Offline auth-, opcode- en update-fieldfixtures slagen |
| M2 | Login en character list tegen OllieWoW |
| M3 | Turtle DBC, BLP1 en representative M2-assets laden |
| M4 | Character enter world en staat stil in een correct gerenderde zone |
| M5 | Bewegen, chatten, combat, loot, inventory en quests werken |
| M6 | Turtle UI en kernaddons werken |
| M7 | Brede gameplay- en stabiliteitsmatrix slaagt |

Een mijlpaal telt alleen als logs en tests het resultaat aantonen. “Compileert”
of “parser retourneert true” is niet voldoende.

## MVP-uitvoervolgorde met vroege testpoorten

De genummerde fasen hierboven beschrijven het volledige project. Voor de eerste
speelbare verticale doorsnede wordt onderstaande volgorde gebruikt. Deze
volgorde heeft voorrang wanneer een latere volledige fase anders zou suggereren.

### V0 - Veilige basis en zichtbare offline assets

- T00, T01, T02 en T03;
- MPQ/VFS-smoketest;
- BLP1-minimum voor login- en worldtextures;
- classic M2-minimum voor één character, één creature en één doodad;
- één offline stock-zone met terrein/WMO/doodad renderen.

Testpoort: zonder serververbinding moeten logintextures en representative world
assets renderbaar zijn. Hierdoor wordt voorkomen dat een geslaagde login eindigt
in een zwart of model-loos scherm.

### V1 - Login en realm list

- T10, relevante delen van T13, T20 en T21;
- SRP6, security flags en classic realm-list;
- nog geen world connection nodig.

Testpoort: de gebruiker kan zijn accountgegevens lokaal invoeren en de realm
zien. Credentials worden nooit in logs, fixtures of chat geplaatst.

### V2 - Character select en create

- T22 en T23;
- minimale DBC-schema's voor races, classes, appearance en equipment;
- GlueXML/CharacterSelect minimum;
- character models en character equipment minimum.

Testpoort: een bestaand character is zichtbaar, selecteerbaar en draait correct
in character select. Daarna wordt character creation voor één stockcombinatie
geactiveerd en getest.

### V3 - World enter en eerste frame

- T11, T31-minimum, T50 en T52;
- alleen de packets en DBC-tabellen die de loginsequentie werkelijk gebruikt;
- terrein, WMO, doodad, eigen player en één unit renderen.

Testpoort: na characterselectie verschijnt een stabiel worldframe met UI en
zichtbare objecten. Nog niet bewegen totdat update fields en initial movement
deterministisch zijn.

### V4 - Bewegen en camera

- T51 plus de minimale movement handlers uit T53;
- walk, run, strafe, turn, jump, swim en heartbeat;
- camera-input, selection ray en eigen animatiestate.

Testpoort: tien minuten door een stock zone lopen zonder disconnect, teleport
naar nulcoördinaten, verkeerde speed of structurele packet parse errors.

### V5 - MVP verharden

- reconnect, logout/login, malformed packettests en logging;
- één stock race en zo snel mogelijk één Turtle-race;
- één stock zone en één Turtle-zone;
- regressiesuite voor V0 tot en met V4.

Pas na V5 krijgen brede gameplayfamilies structureel voorrang.

## Verder opgesplitste Luna-werkpakketten

Onderstaande pakketten maken ook zwaar werk geschikt voor Luna. Iedere code- of
buildactie blijft onder de verplichte goedkeuringsworkflow vallen. Geef Luna
altijd precies één pakket-ID.

### Basis, tooling en bronmapping

| ID | Luna-opdracht | Output / gereed wanneer |
|---|---|---|
| L001 | Dirty worktree en artifacts inventariseren | Manifest van bestanden, branch, commit, timestamps en checksums; geen wijziging |
| L002 | Bestaande OpenWoW-startuplog rubriceren | Tijdlijn MPQ, DBC, UI, auth en stopmoment; geen client starten |
| L003 | Alle WotLK-versieaannames zoeken | Bestand/regel, subsystem en gewenste profielgrens |
| L004 | Profiel-callsites groeperen | Kleine implementatiebatches voor config, data, protocol en UI |
| L005 | MVP-testmatrix bijhouden | Eén rij per zichtbare acceptatietest met logbewijs en status |
| L006 | Packetdiagnostiek ontwerpen | Veilige opcode/size/consumed logging zonder payloads of secrets |

### Gegenereerde protocoldata

| ID | Luna-opdracht | Output / gereed wanneer |
|---|---|---|
| L010 | Server-opcodeheader parsen | Deterministische naam/waarde-output en duplicate-rapport |
| L011 | OpenWoW/server opcodeverschillen testen | Automatische vergelijking; verwachte uitzonderingen expliciet |
| L012 | Server-updatefield-enum evalueren | Exacte numerieke waarden, sizes en eindwaarden als machineleesbare output |
| L013 | Updatefield compile-time checks genereren | Assertions voor alle `*_END`-waarden en kritieke velden |
| L014 | Objecttype- en maskvergelijking | Tabel voor TypeID, TypeMask, HighGuid en field count |
| L015 | Server packet read/write callsites indexeren | Per MVP-opcode de exacte lokale bronfunctie en regel |

Luna mag L012 en L013 voorbereiden en implementeren. De definitieve integratie
van de gegenereerde fields in de objectruntime blijft een reviewpunt voor
Codex/frontier.

### Auth, realm en TCP

| ID | Luna-opdracht | Output / gereed wanneer |
|---|---|---|
| L020 | Logon challenge bytes vergelijken | Veld-voor-veld tabel inclusief endianess en packetlengte |
| L021 | SRP6 bekende-vector-tests maken | Offline client/server proofvectors zonder echte credentials |
| L022 | Security/PIN varianten inventariseren | Layouts en verwachte state transitions uit lokale serverbron |
| L023 | Classic realm-list fixtures maken | Nul, één en meerdere realms plus offline/version flags |
| L024 | Realm-list parser implementeren of aanscherpen | Alle fixtures exact geconsumeerd, geen trailing bytes |
| L025 | Classic AuthCrypt vector-tests | Send 6-byte en receive 4-byte header state over meerdere packets |
| L026 | TCP-fragmentatietests | Headers/payloads byte voor byte en meerdere frames per read |
| L027 | CMSG_AUTH_SESSION layoutdocument | Exacte velden, proofinput en addonblockpositie |
| L028 | Addonblock compressiefixture | Round-trip zlibfixture passend bij lokale `AddonHandler.cpp` |

Na goedkeuring kan Luna L024, L025, L026 en het mechanische deel van L028
volledig implementeren. Codex/frontier behandelt alleen afwijkende crypt- of
live-handshakestates.

### Character select en creation

| ID | Luna-opdracht | Output / gereed wanneer |
|---|---|---|
| L030 | SMSG_CHAR_ENUM serverlayout uitschrijven | Volledige classic veldvolgorde en equipment count |
| L031 | Character enum fixtures maken | Normaal, dood, pet, first login en Turtle-race records |
| L032 | Character enum parser implementeren | Alle fixtures exact gelezen; WotLK-only tail verwijderd/geprofileerd |
| L033 | Create/delete requestlayouts vergelijken | Byte-exact stock race/class fixtures |
| L034 | Create/delete serializers implementeren | Offline servercompatibele packets en resultcode-tests |
| L035 | Character appearance DBC-consumers mappen | Race/sex/skin/face/hair/facial hair naar modeltextures |
| L036 | Character-select render checklist | Model, geosets, equipment, lighting, camera en animation evidence |

Luna kan L030 tot en met L036 uitvoeren. Codex/frontier hoeft alleen de eerste
live afwijking en model/renderintegratie te reviewen.

### DBC in kleine batches

| ID | Luna-opdracht | Output / gereed wanneer |
|---|---|---|
| L040 | MPQ DBC-headercensus | Naam, records, fields, record size en string size, read-only |
| L041 | Server DBC-formatstrings extraheren | Machineleesbare tabel uit `DBCStores.cpp`/structures |
| L042 | MVP-DBC-afhankelijkheden bepalen | Alleen tabellen voor login, char select, world enter en eerste render |
| L043 | Race/class/appearance schemas | Exacte decoders plus representative inhoudstests |
| L044 | Map/Area/Light schemas | Exacte decoders plus stock en Turtle IDs |
| L045 | Creature/model/display schemas | Exacte decoders plus één representative unit |
| L046 | Item/item-display schemas | Character equipment en inventory-minimum |
| L047 | Spell/talent/skill schemaonderzoek | Kolommap en consumers; nog geen brede implementatie |
| L048 | DBC bounds- en malformed tests | Te weinig velden, slechte stringoffsets en truncated files weigeren |

Luna kan L040 tot en met L048 doen. Laat iedere schemafamilie apart goedkeuren;
`Spell.dbc` krijgt een verplichte Codex/frontier-review vanwege de omvang en de
huidige gemengde offsets.

### BLP1 en texturepad

| ID | Luna-opdracht | Output / gereed wanneer |
|---|---|---|
| L050 | BLP1-corpus inventariseren | Representatieve UI, terrain, character, creature en doodadtextures |
| L051 | BLP1-header/variantparser | Bounds-safe metadata voor paletted en JPEG varianten |
| L052 | Paletted BLP1 decoder | Mipmaps en alpha 0/1/4/8-bit met pixeltests |
| L053 | JPEG BLP1 decoder | Gedeelde JPEG-header en mipdata met pixeltests |
| L054 | BLP1 naar bestaande textureloader koppelen | BLP1 en BLP2 fixtures zonder regressie |
| L055 | Logintexture smokecheck voorbereiden | Vaste lijst textures en verwachte dimensies/formats |
| L056 | Worldtexture smokecheck voorbereiden | Terrain, character en doodad texture evidence |

BLP1 is geschikt om bijna volledig aan Luna uit te besteden zolang ieder
decoderdeel eigen fixtures en bounds-tests krijgt.

### Classic M2 in gecontroleerde delen

| ID | Luna-opdracht | Output / gereed wanneer |
|---|---|---|
| L060 | Representative v256 M2-corpus kiezen | Character, creature, item, doodad en spell-effect; alleen paden/metadata |
| L061 | v256/v264 headerdiff | Veld-voor-veld offsets, sizes en arrays |
| L062 | Classic animation trackdiff | Ranges, times, keys, interpolation en global sequences |
| L063 | Classic views/skins onderzoek | Waar indices, triangles, submeshes en texture units staan |
| L064 | Bounds-safe v256 headerparser | Alleen header en arraydescriptors, met truncated tests |
| L065 | Vertices/bones/textures parser | Genormaliseerde data en count/offsetvalidatie |
| L066 | Animations/tracks parser | Representative idle/walk/run/jump tracks uitleesbaar |
| L067 | Particles/ribbons/cameras parser | Losse optionele secties, geen rendererwijziging |
| L068 | Embedded view/skin parser | Indices, triangles, submeshes en texture units als neutrale structs |
| L069 | v256 parser regressiesuite | Alle corpusbestanden parsen of met specifieke veilige reden weigeren |

Luna kan L060 tot en met L069 uitvoeren. Codex/frontier bepaalt vóór L064 het
genormaliseerde interne datamodel en behandelt daarna de omzetting naar de
renderer. Zo wordt het reverse-engineeringwerk grotendeels uitbesteed zonder de
architectuur te laten versnipperen.

### WMO, ADT en offline world render

| ID | Luna-opdracht | Output / gereed wanneer |
|---|---|---|
| L070 | WMO stock/Turtle corpus en chunkdiff | Ondersteunde/ontbrekende chunks en struct sizes |
| L071 | ADT stock/Turtle corpus en chunkdiff | Versie, MCNK, alpha, liquid en placementverschillen |
| L072 | WDT/WDL padcontrole | Mapselectie en tilebeschikbaarheid voor twee zones |
| L073 | Classic alpha/liquid fixtures | Kleine deterministische decoderfixtures |
| L074 | Placement tests | M2/WMO position, rotation, scale en filename resolution |
| L075 | Offline stock-zone scenario | Vaste camera en lijst verwachte terrain/WMO/doodad resources |
| L076 | Offline Turtle-zone scenario | Zelfde controle op custom map/content |

### GlueXML, FrameXML en Lua

| ID | Luna-opdracht | Output / gereed wanneer |
|---|---|---|
| L080 | Login GlueXML ontbrekende API's | Gesorteerd op eerste runtimegebruik |
| L081 | Character-select ontbrekende API's | Events, globals, widgetmethods en XML templates |
| L082 | In-world minimale UI API's | Actionbar, unit frames, chat, bags, tooltip en minimap |
| L083 | XML parsercompatibiliteit batches | Eén element/templatefamilie per wijziging |
| L084 | Lua globals batches | Maximaal tien gerelateerde functies per taak |
| L085 | Widgetmethods batches | Eén widgettype per taak met Lua-tests |
| L086 | Event argumenttests | Classic eventnamen en exacte argumentvolgorde |
| L087 | UI-foutenclassificatie | Fatal, blokkeert flow, visueel, of post-MVP |

Luna kan het merendeel van de UI API implementeren. Codex/frontier behandelt
alleen eventdispatch, secure-state of lifetimeproblemen die meerdere widgets en
flows tegelijk raken.

### Update objects, world enter en movement

| ID | Luna-opdracht | Output / gereed wanneer |
|---|---|---|
| L090 | Server update-object writers mappen | Per update type exacte writevolgorde en flags |
| L091 | Update mask fixtures | Kleine/meerbloksmasks en veldgrenzen per objecttype |
| L092 | Create/value/out-of-range fixtures | Player, unit, gameobject, item, corpse en dynamic object |
| L093 | Losse values-block parser | Exact consumption en field-countvalidatie |
| L094 | Losse create-block parser | GUID/type/movement/values zonder runtime-mutatie |
| L095 | Initial world packetvolgorde | Tijdlijn vanaf CMSG_PLAYER_LOGIN tot eerste stabiele frame |
| L096 | MovementInfo server/clientdiff | Flags, optionele blokken en bytevolgorde |
| L097 | Movement serializers per opcode | Heartbeat, start/stop, strafe, turn, jump en swim fixtures |
| L098 | Spline fixtures | Facing angle/target/point, nodes, duration en destination |
| L099 | Movement malformed tests | Truncation, extreme nodecounts en ongeldige flags veilig weigeren |

Luna kan L090 tot en met L099 uitvoeren en losse parsers implementeren.
Codex/frontier integreert create blocks met de objectruntime en behandelt live
spline/transportbugs.

### MVP-testondersteuning

| ID | Luna-opdracht | Output / gereed wanneer |
|---|---|---|
| L100 | MVP-logdashboard/document | Laat per run precies zien welke V-poort faalde |
| L101 | Login/realm handmatige testkaart | Stappen, verwacht beeld, relevante veilige logs |
| L102 | Character-select testkaart | Enum, appearance, create/delete en modelcheck |
| L103 | World-enter testkaart | Verwachte packetfasen en zichtbare renderingchecks |
| L104 | Movement testkaart | Input, serveracceptatie, animatie en positiecorrectheid |
| L105 | Tien-minuten soak rubric | Memory, parse errors, disconnects, missing assets en visual defects |
| L106 | Reconnect/logout regressietests | Herhaalde sessies zonder herstart of stale crypt/object state |

## Nieuwe taakverdeling

Met bovenstaande opsplitsing kan Luna ongeveer 70-80% van onderzoek,
fixtures, geïsoleerde implementatie en regressietests uitvoeren.
Codex/frontier blijft primair verantwoordelijk voor slechts deze
integratiepunten:

1. profielarchitectuur en versiegrenzen;
2. genormaliseerd classic M2/skinmodel en rendererintegratie;
3. definitieve update-field/objectruntime-integratie;
4. live auth/cryptproblemen die offline vectors niet verklaren;
5. world-enter en movementbugs die meerdere lagen tegelijk raken;
6. finale review vóór iedere V0-V5 testpoort.

## Regels voor interactieve MVP-tests

- Een handmatige test tegen de draaiende server vereist vooraf expliciete
  toestemming, omdat die een live server/account/character kan raken.
- Gebruik bij voorkeur een apart testcharacter. Character creation, deletion of
  appearancewijzigingen worden nooit automatisch uitgevoerd.
- Credentials worden alleen door de gebruiker in de client ingevoerd en nooit
  door een agent gevraagd, opgeslagen of gelogd.
- Een agent mag testinstructies en veilige logging voorbereiden, maar start of
  stopt server/client niet zonder afzonderlijke toestemming.
- Clientdata en MPQ's blijven read-only. De OpenWoW executable wordt niet
  automatisch naar `D:\OllieWoW\Client` gekopieerd.
- Na iedere zichtbare test wordt eerst het bewijs geanalyseerd voordat de
  volgende codebatch wordt voorgesteld.

## Wat Luna goed zelfstandig kan doen

- read-only inventarisaties en compatibiliteitsmatrices;
- generators voor opcodes nadat het gewenste formaat is vastgesteld;
- bestands- en callsite-mapping;
- packetfixtures voor reeds vastgelegde layouts;
- kleine, geïsoleerde packetfamilies;
- tabelgroepen of UI API-functies met duidelijke voorbeelden;
- bounds-, malformed-input- en regressietests;
- documentatie en statusupdates per taak.

Laat Luna nooit meerdere protocolfamilies tegelijk “globaal converteren”. Geef
één taak-ID, laat eerst bevindingen rapporteren en beoordeel de voorgestelde
bestanden voordat je toestemming geeft.

## Wat waarschijnlijk echt moeilijk wordt

Deze onderdelen zijn het meest geschikt voor Codex/frontier:

1. **Classic M2 v256 en skins** - andere binaire layout, animatietracks,
   embedded views/skins en render batches raken parser, animation en renderer
   tegelijk.
2. **Update-object en movement/splines** - één fout veld verschuift de rest van
   het packet en levert misleidende fouten op in objecten of rendering.
3. **Exacte Turtle DBC-schema's** - de huidige loader maskeert verschillen;
   inhoudelijke validatie is nodig om plausibel ogende maar verkeerde waarden
   te vinden.
4. **World auth plus addon-handshake** - SRP6, SHA1, compressie, crypt state en
   packet framing moeten byte-exact en in de juiste volgorde werken.
5. **FrameXML/Lua-integratie** - veel kleine API-verschillen veroorzaken
   kettingreacties; prioritering op basis van de echte startupflow is nodig.
6. **Cross-layer world-enter debugging** - protocol, DBC, object fields,
   modellen en UI komen hier voor het eerst tegelijk samen.

## Kopieerbaar taakprompt voor Luna

```text
Werk uitsluitend aan taak <TAAK-ID> uit:
D:\OllieWoW\Experiments\OpenWow-snapshot\docs\OLLIEWOW_TURTLE_PORT_PLAN.md

Volg D:\OllieWoW\AGENTS.md. Inspecteer eerst read-only. Rapporteer daarna:
1. je concrete bevindingen;
2. de kleinste voorgestelde wijziging;
3. exact welke bestanden zouden veranderen;
4. impact, risico en terugdraaibaarheid;
5. wat je ter verificatie wilt bouwen of testen.

Wacht vervolgens op expliciete toestemming voordat je iets wijzigt, bouwt,
test, start of genereert. Wijzig niets in Source, Server, Client, MPQ's of de
database. Houd alle bestaande lokale OpenWoW-wijzigingen intact.
```

## Voortgang bijhouden

Gebruik bij voorkeur een afzonderlijk statusdocument met per taak:

```text
Taak:
Status: niet gestart | analyse | wacht op toestemming | bezig | geblokkeerd | gereed
Commit/patch:
Gewijzigde bestanden:
Verificatie:
Openstaande risico's:
Volgende toegestane taak:
```

Werk de status pas bij nadat de inhoudelijke wijziging en verificatie zijn
goedgekeurd. Een taak mag geen stilzwijgende vervolgtaak starten.
