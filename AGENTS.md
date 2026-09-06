# OpenWow Classic/Turtle glue-workflow

Deze instructies gelden voor werk binnen deze OpenWow-worktree en vullen de
bovenliggende `D:\OllieWoW\AGENTS.md` aan. De bovenliggende instructies blijven
volledig van kracht.

## Doel

Breng de OpenWow-client minimaal en controleerbaar naar Classic/Turtle-gedrag,
zodat login, CharacterSelect, CharacterCreate en daarna world-entry werken met
de lokale Turtle-data.

## Doelruntime versus bestaande implementatie

- De doelclient en doelserver zijn Vanilla/Classic/Turtle. De lokale Client-
  XML/Lua beschrijft daarom het gewenste gedrag; OpenWow's oorspronkelijke
  3.3.5/WotLK-gedrag is technische erfenis en geen nieuw contract.
- Lokale Vanilla/Turtle-Lua mag niet inhoudelijk worden omgezet naar 3.3.5-
  of WotLK-semantiek. Een compatibiliteitslaag is alleen toegestaan als
  tijdelijke interne runtimebrug wanneer de ingebouwde Lua-engine een andere
  Lua-versie of ABI heeft. Die brug moet de Vanilla-semantiek behouden en mag
  geen lokale API-, enumeratie- of UI-keuzes vervangen.
- Geef bij zo'n verschil de voorkeur aan een kleine runtime-/VM-correctie of
  een aantoonbaar semantiekbehoudende loader-aanpassing. Een bronrewrite die
  expliciete Vanilla-constructies verandert, is fout en moet met de lokale
  Client-Lua en Benilla worden gevalideerd.
- Behandel iedere runtimebridge als technische schuld richting een echte
  Vanilla-compatibele runtime; een succesvolle build bewijst niet dat de
  Vanilla-Lua-flow correct is.

## Bronprioriteit

Gebruik bij twijfel deze volgorde:

1. Lokale client-XML en Lua onder `D:\OllieWoW\Client` voor UI-flow,
   widgetnamen, callbacks, enumeratievolgorde, labels, texturebestanden en
   texcoords.
2. Lokale serverbron onder `D:\OllieWoW\Source` voor DBC-schema’s,
   Classic-velden, packetstructuren, objecttypes en servergedrag.
3. `D:\OllieWoW\benilla` als Vanilla/Turtle-referentie voor ontbrekende
   client-render- en glue-logica.
4. Bestaande OpenWow-code als implementatiebasis, alleen waar die de lokale
   clientcontracten nog niet correct ondersteunt.

Gebruik geen generieke AzerothCore-, cMaNGOS- of WotLK-definities als de lokale
bron of client daarvan afwijkt.

## Data als vaststaand uitgangspunt

- Behandel de aanwezige lokale DBC’s, MPQ-bestanden, XML, Lua en overige
  clientdata als correcte en gewenste Classic/Turtle-brondata, tenzij een
  concrete bronvergelijking het tegendeel bewijst.
- Wanneer content ontbreekt, verkeerd wordt getoond of niet werkt, onderzoek
  eerst de OpenWow-code: VFS/MPQ-uitlezing, DBC-loader, Lua-ABI, native
  bindings, widget-runtime, packetvertaling en gameplay-flow.
- Pas geen DBC, MPQ, XML of Lua aan om een fout in de uitlees- of runtime-laag
  te maskeren. Een wijziging aan brondata is alleen toegestaan wanneer de
  lokale Client, Source of Benilla aantoonbaar bewijst dat die data zelf niet
  het gewenste contract bevat.
- Een succesvolle build of geladen bestand bewijst niet dat de runtime het
  bestand correct uitleest of gebruikt; valideer de volledige data -> loader
  -> native API -> Lua/XML- of gameplay-flow.

## Herkomst van bevindingen en aanbevelingen

Elke diagnose en aanbeveling moet herleidbaar zijn tot een concrete bron. Maak
in rapportages onderscheid tussen het gewenste contract, de implementatie en
het waargenomen runtimegedrag:

1. `D:\OllieWoW\Client` is de primaire bron voor het echte Classic/Turtle
   glue-contract: XML-widgetnamen en -structuur, Lua-callers en
   returnposities, enumeratievolgorde, labels, zichtbaarheid,
   texturebestanden en texcoords. Wanneer bestanden in MPQ zitten, moet de
   lokale VFS/MPQ-resolver en het exacte archive-pad worden vermeld. Een
   screenshot of een OpenWow-aanname vervangt dit contract niet.
2. `D:\OllieWoW\Source` is de autoriteit voor lokale serverdata en
   servergedrag: DBC-schema's en velden, Classic-velden, packetstructuren,
   objecttypes, character-enumeratie en server-side validatie. Gebruik deze
   bron niet als bewijs voor een UI-widgetcontract wanneer de lokale client
   iets anders vraagt.
3. `D:\OllieWoW\benilla` is de Vanilla/Turtle-referentie voor ontbrekende
   client-side glue-, model- en renderlogica. Benoem expliciet wanneer Benilla
   als referentie wordt gebruikt en controleer daarna of het lokale XML/Lua- en
   servercontract ermee overeenkomt.
4. `D:\OllieWoW\Experiments\OpenWow-snapshot` is de implementatiebasis. OpenWow
   is oorspronkelijk een 3.3.5-client; bestaande OpenWow-API's, limieten,
   modelpaden, shadergedrag en returncontracten zijn daarom niet automatisch
   Classic/Turtle-correct. Een aanbeveling die op OpenWow-code is gebaseerd
   moet als aanpassing worden behandeld en tegen Client, Source en Benilla
   worden gevalideerd.
5. Runtime-logs, offline scenario-uitvoer en screenshots zijn observaties van
   de huidige build. Zij bewijzen dat iets gebeurt of niet gebeurt, maar zijn
   geen normatief contract. Vermeld bij een runtimebevinding het logbestand,
   de relevante melding en, waar stabiel, het symbool of bestand dat de
   melding veroorzaakt.

Voor iedere aanbeveling rapporteer je daarom minimaal: (a) concrete afwijking,
(b) bron en exact pad/symbool waarop de verwachting is gebaseerd, (c) kleinste
wijzigingsdoel, (d) impact en terugdraaibaarheid, en (e) benodigde fixture of
runtimecontrole. Als bronnen elkaar tegenspreken, wint voor glue/UI eerst de
lokale Client-XML/Lua, voor server- en DBC-feiten de lokale Source, en voor
ontbrekende client-renderdetails Benilla. De lokale taakdoelen hebben voorrang
op generieke 3.3.5- of WotLK-gedragsaannames.

Voor CharacterSelect en CharacterCreate betekent dit concreet dat een limiet
zoals het huidige maximum van 10 CharacterSelect-slots alleen als lokaal
XML/Lua-contract of expliciete taakvereiste mag worden behandeld. Het is geen
algemene OpenWow-limiet. Een nieuwe race mag geen nieuwe C++-racecase vereisen,
maar moet wel een corresponderende lokale XML/Lua-slot, DBC/CharBaseInfo-data,
model- en texture-assets hebben voordat de dynamische flow haar kan tonen.

## Verplichte afwijkingsvergelijking

Wanneer OpenWow een concrete runtime-afwijking, foutmelding, ontbrekend
modelpad, verkeerde mapping of ander afwijkend gedrag toont, controleer dan
eerst de overeenkomstige Benilla-code en asset-/padconventies voordat je een
nieuwe verklaring, fallback of workaround introduceert.

Gebruik Benilla om vast te stellen:
- welk vanilla/Classic-gedrag verwacht wordt;
- welke race-, class- en modeltokenmapping geldt;
- welke modelpaden en dependencies verwacht worden;
- of de afwijking in OpenWow, de lokale clientdata of de custom Turtle-data zit.

Ga pas daarna over tot wijziging of fallback. Gebruik geen workaround die een
Benilla-afwijking maskeert zonder die afwijking expliciet te rapporteren.

## Vast analyseformat

Bij een niet-triviale glue-, model- of renderafwijking zet de analyse de drie
relevante implementaties kort naast elkaar:

| Aspect | Benilla/Vanilla | OpenWow 3.3.5 | Lokale Classic/Turtle-variant |
|---|---|---|---|
| Verwacht gedrag / afwijking | bronpad en symbool | bronpad en gedrag | bronpad, wijziging en runtime-observatie |

Gebruik alleen de relevante aspecten (bijvoorbeeld Lua-flow, modelpad, DBC-
veld, widget of asset); voeg geen tabel toe voor een triviale eenregelige
bevinding. Sluit af met de kleinste wijziging en de benodigde controle.

## Modulaire DBC-datastroom

- De client leest de actieve DBC-data voor CharacterSelect en CharacterCreate;
  deze glue-flow schrijft of wijzigt geen DBC-bestanden.
- Native glue-logica mag geen race-, class-, faction-, token-, model- of
  asset-specifieke hardcoding toevoegen als vervanging voor DBC/Lua/XML-data.
  Dus geen speciale `if (race_id == ...)`-tak, vaste standaardrace,
  geforceerde tokenalias of vaste modelnaam om een ontbrekende of verkeerd
  geladen data-entry te maskeren. Dit geldt ook voor toekomstige custom races:
  de generieke dataflow moet blijven werken zonder nieuwe C++-case.
- Een uitzondering is alleen toegestaan wanneer de lokale Client, Source of
  Benilla aantoonbaar een bestaand Vanilla/Turtle-contract documenteert. Zo'n
  uitzondering moet klein, expliciet gemotiveerd, fixture-gedekt en beperkt
  blijven tot het bewezen contract; zij mag geen algemeen fallbackmechanisme
  voor toekomstige races worden.
- DBC-velden zoals race/class-ID, displaynaam, `client_file_string`,
  `model_client_prefix`, `CharBaseInfo` en `CharSections` moeten via de
  bestaande loaders en Lua-API's worden doorgegeven. Voeg geen C++-case toe
  alleen omdat een DBC-edit een nieuwe ID introduceert.
- Een DBC-edit is alleen volledig bruikbaar wanneer de lokale clientdata ook
  de benodigde model-, texture-, atlas- en XML/Lua-slotgegevens bevat. Als een
  vereiste asset of widget ontbreekt, moet de client veilig leeg/fallback
  gedrag tonen en de ontbrekende bron rapporteren; hij mag de DBC-edit niet
  vervangen door een hardcoded standaardrace.
- Projectconventie voor High Elves: High Elf is ingame een afzonderlijke
  Alliance-race en mag niet als Horde-race worden behandeld. Voor model-,
  texture- en race-icon-flow gebruikt deze race lokaal echter de Blood Elf-
  token/assetconventie wanneer de actieve DBC/Lua-data dat zo definiëren.
  Gebruik daarom niet automatisch een `HighElf`-token of een nieuwe C++-case;
  controleer faction, modeltoken en atlas-key afzonderlijk tegen de lokale
  DBC en Lua.
- Nieuwe DBC-velden of een afwijkend lokaal DBC-schema mogen alleen een
  generieke loader-aanpassing krijgen wanneer de lokale `Source` en het echte
  DBC-bestand dat bewijzen. Houd de richting altijd data -> loader -> native
  Lua-API -> lokale Lua/XML-flow.
- Offline checks moeten minimaal aantonen dat een gewijzigde race/class via
  dezelfde enumeratie- en validatiepaden loopt als bestaande entries en geen
  nieuwe ID-specifieke C++-branch nodig heeft.

## Belangrijke architectuurregels

- XML bepaalt welke UI-widgets bestaan en hoe ze heten.
- Lua bepaalt de CharacterSelect/CharacterCreate-flow, selectie, zichtbaarheid,
  labels, race/class-keuzes en atlas-coördinaten.
- Native code moet de Lua/API-contracten correct uitvoeren en DBC-data leveren;
  native code mag Lua-keuzes niet per frame overschrijven.
- Race- en classlijsten moeten uit de actieve DBC/Lua-enumeratie komen. Voeg
  geen vaste lijst toe om een ontbrekende clientfunctie te maskeren.
- Een extra Lua-returnwaarde is een ABI-wijziging. Behoud bestaande
  Classic/Turtle returnposities en returnaantallen tenzij de lokale Lua-bron
  aantoonbaar anders vereist.
- Een nieuwe race moet kunnen werken via DBC, CharBaseInfo/CharSections,
  modelbestanden en de bijbehorende XML/Lua/asset-data. Hardcode geen race-ID
  als algemene oplossing.
- Behoud bestaande geldige lokale wijzigingen; reset, overschrijf of herontwerp
  ze niet stilzwijgend.

## Wijzigings- en testregels

- Lees eerst de relevante bestanden read-only en rapporteer concrete afwijkingen.
- Wijzig alleen `D:\OllieWoW\Experiments\OpenWow-snapshot`.
- Wijzig niets in `Source`, `Server`, `Client`, `benilla`, MPQ’s, databases,
  accounts, characters of actieve processen.
- Vraag expliciete toestemming vóór wijzigingen als de scope niet al duidelijk
  is, en altijd vóór build, test, deployment of processtart.
- Build en test uitsluitend vanuit de bestaande
  `D:\OllieWoW\Experiments\OpenWow-snapshot\build`-map.
- Start geen GUI, client of server als onderdeel van een diagnose zonder aparte
  toestemming.
- Voeg waar mogelijk kleine offline regressietests toe voor API-contracten,
  dynamische race/class-enumeratie, widget-texcoords en modelpadkeuze.
- Rapporteer na afloop gewijzigde bestanden, build/testresultaat,
  onaangeraakte paden en resterende beperkingen.
