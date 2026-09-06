# Classic/Turtle UI runtime audit

Status: eerste read-only inventarisatie  
Datum: 2026-09-04  
Doel: de lokale Vanilla/TurtleWoW Blizzard- en addon-UI laten werken op de OpenWow-runtime zonder de lokale Lua/XML naar WotLK-semantiek om te schrijven.

## Uitgangspunten

- De lokale clientdata en de lokale Classic/Turtle-Lua bepalen het gewenste gedrag.
- DBC's en clientbestanden worden via de MPQ/VFS-patchketen gelezen. De hoogste patch waarin een virtueel bestand bestaat wint; een ontbrekend bestand in een hogere patch valt terug naar een lagere patch.
- De actieve FrameXML-bron komt uit de effectieve MPQ/VFS-keten. De losse filesystem-map bevat daarnaast de addons.
- De lokale Lua/XML en MPQ's worden niet aangepast als workaround.
- OpenWow-native code mag alleen de ontbrekende runtime/API-contracten leveren.
- `D:\OllieWoW\benilla` is een gedragsreferentie voor Vanilla/Turtle-glue, geen vervanging voor de lokale clientbron.

## Bronketen

De MPQ/VFS-mounting staat in:

- `src/openwow/data/startup_archive_mount.h`
- `src/openwow/data/startup_archive_mount.cpp`

De FrameXML-index bevestigt dat de actieve UI-bestanden onder andere bevatten:

- `/Interface/FrameXML/FrameXML.toc`
- `/Interface/FrameXML/OptionsFrame.lua`
- `/Interface/FrameXML/FloatingChatFrame.lua`
- `/Interface/FrameXML/ChatFrame.lua`
- `/Interface/FrameXML/LFT/LFT.lua`
- `/Interface/FrameXML/EverlookBroadcastingCo/EverlookBroadcastingCo.lua`

De beschikbare index staat momenteel in:

- `build/release/scenario-artifacts/char-create-contract-4/mpq-index.txt`

De losse addonlaag bevat momenteel onder andere:

- `D:\OllieWoW\Client\Interface\AddOns\pfUI\pfUI.toc` — interface `11200`
- `D:\OllieWoW\Client\Interface\AddOns\pfUI-turtle\pfUI-turtle.toc` — interface `11200`
- `D:\OllieWoW\Client\Interface\AddOns\TortoiseGMManager\TortoiseGMManager.toc` — interface `11200`
- `D:\OllieWoW\Client\Interface\AddOns\OllieUIFixes\OllieUIFixes.toc` — interface `11200`

`pfUI-tbc.toc` bestaat als alternatieve TOC, maar is niet de leidende `pfUI.toc` voor de Classic/Turtle-addonload.

## Eerste runtimebevindingen

Logbron:

- `D:\OllieWoW\Client\Logs\openwow-client.log`

### A. Functie bestaat in C++, maar registratie moet worden gecontroleerd

De volgende Vanilla-functies hebben al een C++-implementatie, maar moeten aantoonbaar in de actieve Lua-bindingcatalogus geregistreerd zijn:

- `GetPlayerBuff()` — `src/openwow/ui/game/api/game_lua_api_unit.cpp`
- `GetPlayerBuffTimeLeft()` — `src/openwow/ui/game/api/game_lua_api_unit.cpp`
- `GetNumLanguages()` — `src/openwow/ui/game/api/game_lua_api_chatmsg.cpp`
- `GetPVPRankInfo()` — `src/openwow/ui/game/api/game_lua_api_misc_ui.cpp`
- `GetPVPThisWeekStats()` — `src/openwow/ui/game/api/game_lua_api_pvp.cpp`
- `CheckReadyCheckTime()` — `src/openwow/ui/game/api/game_lua_api_social.cpp`

De log bevat hiervoor onder andere meldingen als:

- `BuffFrame.lua:126: attempt to call global 'GetPlayerBuffTimeLeft' (a nil value)`
- `BuffFrame.lua:55: attempt to call global 'GetPlayerBuff' (a nil value)`
- `ChatFrame.lua:2471: attempt to call global 'GetNumLanguages' (a nil value)`
- `HonorFrame.lua:74: attempt to call global 'GetPVPThisWeekStats' (a nil value)`
- `CheckReadyCheckTime` ontbreekt in een ready-check callback.

Dit is een centrale binding-/registratiekwestie, geen reden om de Vanilla-Lua te herschrijven.

### B. Functie ontbreekt echt

`GetPlayerBuffApplications()` wordt gebruikt door:

- `D:\OllieWoW\Client\Interface\AddOns\pfUI\api\unitframes.lua`
- `D:\OllieWoW\Client\Interface\AddOns\pfUI\modules\buff.lua`
- `D:\OllieWoW\Client\Interface\AddOns\pfUI\modules\buffwatch.lua`

Deze functie is momenteel niet als werkende native Vanilla-binding aanwezig. De implementatie moet aansluiten op het bestaande aura-/buffmodel en het Vanilla-returncontract behouden.

### C. Frame-methodes bestaan, maar zijn niet overal beschikbaar

De runtime bevat implementaties voor onder andere:

- `GetFrameType()` — `src/openwow/ui/game/framescript/core/frame_base_methods.cpp`
- `SetTextFontObject()` — `src/openwow/ui/game/framescript/widgets/font_string_state_methods.cpp`
- `SetHighlightTextColor()` — `src/openwow/ui/game/framescript/widgets/button_methods.cpp`
- `SetDisabledTextColor()` — `src/openwow/ui/game/framescript/widgets/button_methods.cpp`

Toch meldt de log:

- `OptionsFrame.lua:48: attempt to call method 'GetFrameType' (a nil value)`
- `UIDropDownMenu.lua:189: attempt to call method 'SetDisabledTextColor' (a nil value)`
- `UIDropDownMenu.lua:232: attempt to call method 'SetHighlightTextColor' (a nil value)`
- `OptionsFrame.lua:421: attempt to call method 'SetTextFontObject' (a nil value)`

Dit wijst op een probleem in centrale frame-methoderegistratie/materialisatie of op een verkeerd widgettype, niet op losse fouten in ieder Lua-bestand.

### D. Verkeerd Lua-type of ontbrekende returnwaarde

De log bevat meerdere meldingen van `attempt to call a table value`, onder andere in:

- `OptionsFrame.lua`
- `ChatFrame.lua`
- `UIParent.lua`
- `QuestLogFrame.lua`
- `DurabilityFrame.lua`

Deze groep moet worden onderzocht als ABI-/returntypeprobleem. Een functie die als tabel in plaats van als callable functie zichtbaar wordt, moet centraal in de Lua-compositie of bindingregistratie worden gecorrigeerd.

### E. XML-widgetcontract

De melding `GuildControlPopupFrameCheckbox14 does not exist!` is een widgetmismatch. De lokale pfUI-skin verwerkt in:

- `D:\OllieWoW\Client\Interface\AddOns\pfUI\skins\blizzard\friends.lua:386`

alleen checkboxen `1` tot en met `13`. We voegen geen kunstmatige checkbox 14 toe voordat is bewezen dat de effectieve Classic/Turtle-XML die widget werkelijk definieert.

### F. Losse addonfouten

Deze fouten mogen niet worden verward met de centrale FrameXML-runtimeproblemen:

- `EverlookBroadcastingCo.lua:163` — nil arithmetic.
- `LFT.lua:495` — font niet ingesteld.

Ze worden pas na de centrale runtimefixes afzonderlijk beoordeeld.

## Binding- en laadlocaties

De belangrijkste centrale samenstelling staat in:

- `src/openwow/ui/production_lua_surface.cpp`
- `src/openwow/ui/game/api/framexml_core_native_bindings.cpp`
- `src/openwow/ui/game/api/framexml_client_native_bindings.cpp`
- `src/openwow/ui/game/api/framexml_gameplay_native_bindings.cpp`
- `src/openwow/ui/game/runtime/world_lua_runtime.cpp`
- `src/openwow/ui/game/framescript/core/frame_method_registry.cpp`
- `src/openwow/ui/game/framescript/xml/frame_xml_loader.cpp`
- `src/openwow/ui/glue/interleaved_toc_processor.cpp`

De addoninterface-validatie staat los van `GetBuildInfo()`:

- `src/openwow/ui/addon_manager.h` gebruikt `11200` voor addonvalidatie.
- `src/openwow/ui/retail_client_build.h` levert de waarden van `GetBuildInfo()`.

Deze twee contracten mogen niet opnieuw door één globale wijziging aan elkaar worden gekoppeld.

## Geplande werkvolgorde

1. Controleer de effectieve FrameXML-bron en loadvolgorde via de bestaande VFS-index.
2. Maak een lijst van Lua-globals, frame-methodes, templates en returnwaarden die de lokale UI werkelijk gebruikt.
3. Vergelijk die lijst met de actieve native bindingcatalogi en frame-methodetabellen.
4. Registreer eerst functies die al correct in C++ bestaan maar niet zichtbaar zijn voor Lua.
5. Implementeer daarna ontbrekende Vanilla-functies zoals `GetPlayerBuffApplications()`.
6. Repareer vervolgens frame-methodematerialisatie en foutieve Lua-types/returnwaarden.
7. Los daarna specifieke XML-widgetmismatches op.
8. Test na elke categorie login, world-entry, actionbars, spellbook, bags, options, chat, guild en tooltips.
9. Verwijder pas na een schone test ongebruikte WotLK/3.3.5-families, één familie per keer.

## Verwijderregels

Een onderdeel mag pas verwijderd worden wanneer:

- de lokale effectieve Lua/XML het niet gebruikt;
- geen actieve addon het gebruikt;
- geen interne gameplay- of protocolcode ervan afhankelijk is;
- de client zonder het onderdeel bouwt;
- de volledige smoke-test en logvergelijking schoon blijven.

Bestandsnamen met `wotlk`, `retail` of `3.3.5` zijn op zichzelf geen bewijs dat iets verwijderd kan worden. Sommige daarvan zijn generieke runtimecode of adapters voor de huidige Classic/Turtle-packetflow.

