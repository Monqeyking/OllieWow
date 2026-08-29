# Volgende task: Classic CharacterSelect/CharacterCreate via lokale XML/Lua

Werk uitsluitend in:

`D:\OllieWoW\Experiments\OpenWow-snapshot`

## Doel

Maak CharacterSelect en CharacterCreate opnieuw compatibel met de echte lokale
Classic/Turtle XML- en Lua-flow. Nieuwe races/classes die correct in de lokale
DBC en clientdata staan moeten via dezelfde dynamische enumeratie zichtbaar en
selecteerbaar worden als in de vanilla client.

## Eerst read-only lezen

- `D:\OllieWoW\AGENTS.md`
- `D:\OllieWoW\Experiments\OpenWow-snapshot\AGENTS.md`
- relevante OpenWow glue-, model-, DBC- en widget-runtimebestanden;
- de lokale CharacterSelect/CharacterCreate XML en Lua onder
  `D:\OllieWoW\Client` of, wanneer die in MPQ zitten, de lokale VFS/MPQ-
  resolver en beschikbare bestandslijsten;
- relevante serverbron onder `D:\OllieWoW\Source`;
- overeenkomstige Benilla-code onder `D:\OllieWoW\benilla`;
- bestaande offline scenario’s en de huidige worktree-diff.

## Bekende startpunten

1. Controleer eerst de huidige wijziging in
   `src/openwow/ui/glue/glue_lua_api_game.cpp`.
   `GetCharacterInfo()` had oorspronkelijk 10 returnwaarden. De tijdelijke
   11e race-token is verdacht en mag niet blijven zonder bewijs uit de lokale
   CharacterSelect-Lua. Beoordeel en herstel dit contract indien nodig.
2. Controleer daarna de tijdelijke native overrides in
   `glue_background_controller.cpp`. Ze mogen de XML/Lua-keuzes niet per frame
   overschrijven en mogen geen vaste race/classmapping afdwingen.
3. Controleer de modelpadkeuze tegen `ChrRaces.model_client_prefix`, lokale
   VFS-resolutie en Benilla. Een DBC-token is niet automatisch een geldig
   scene-modelpad; bewijs de asset voordat je een fallback gebruikt.
4. Controleer `GetAvailableRaces()`, `GetNameForRace()`,
   `GetAvailableClasses()`, `GetClassesForRace()` en alle returnposities tegen
   de echte Lua-callers.
5. Controleer dat `SetTexture`, `SetTexCoord`, `SetText`, `Show` en `Hide` de
   door XML aangemaakte widgets bedienen zonder onbekende widgetnamen of
   texture-reloads per frame.

## Functionele eisen

- XML blijft eigenaar van layout en widgetstructuur.
- Lua blijft eigenaar van race/class-enumeratie, volgorde, labels,
  zichtbaarheid, selectie, atlaskeuze en texcoords.
- Native API’s leveren lokale DBC-data zonder returncontracten te breken.
- CharacterSelect toont namen, model, equipment/geosets en raceafhankelijke
  achtergrond correct.
- CharacterCreate toont de door Lua gekozen race/class-iconen en labels, zonder
  volledige atlas-mosaïeken.
- Een extra correct geconfigureerde race hoeft geen nieuwe C++-race-ID-case te
  krijgen.
- Onbekende of ontbrekende data faalt veilig en veroorzaakt geen under-read,
  over-read, out-of-range access, crash of eindeloze reloadlus.

## Werkwijze

1. Rapporteer concrete afwijkingen, kleinste wijziging, exacte bestanden,
   impact/risico, terugdraaibaarheid en benodigde fixtures.
2. Wacht op expliciete toestemming vóór edits als die nog niet voor deze task
   gegeven is.
3. Implementeer de kleinste contractgerichte wijziging; behoud bestaande
   geldige dirty-worktreewijzigingen.
4. Voeg kleine offline regressiechecks toe voor:
   - exact Lua-returnaantal en posities;
   - DBC/Lua-race- en class-enumeratie;
   - custom race zonder nieuwe C++-case;
   - XML-gedreven texture/texcoord-toepassing;
   - modelpad- en ontbrekende-asset-fallbacks.
5. Vraag apart toestemming vóór build/test. Gebruik alleen de bestaande
   `build`-map; deploy niets naar de echte client of server.

## Gereed wanneer

- de lokale CharacterSelect- en CharacterCreate-Lua zonder contractbreuk werkt;
- stock Classic/Turtle races zichtbaar en selecteerbaar zijn;
- een correct aangeleverde nieuwe DBC-race door dezelfde dynamische flow komt;
- geen native hardcoded lijst de XML/Lua overschrijft;
- offline checks slagen;
- resterende asset- of rendererbeperkingen expliciet zijn vastgelegd.
