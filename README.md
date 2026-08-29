# OllieWow

OllieWow is a private, experimental client project based on the
[OpenWoW](https://github.com/rkabachenko/OpenWow-snapshot) codebase.

The purpose of this fork is to investigate and prototype a port of the
OpenWoW 3.3.5 client toward a local Classic/TurtleWoW environment. The work
focuses on restoring the data-driven Vanilla/Classic client flow, including
XML, Lua, DBC, VFS, model and rendering compatibility.

This is an experimental work-in-progress, not an official OpenWoW release and
not a complete Classic client. The current implementation is intended for
local development and testing.

## Project principles

- The client reads local game data; this repository does not distribute game
  client files, MPQ archives, DBC files, models, textures or server data.
- XML owns layout and widget structure.
- Lua owns glue flow, selection, labels, visibility and presentation choices.
- Native code provides the required runtime and DBC data without replacing
  the data-driven flow with race- or class-specific hardcoding.
- Classic/Turtle behaviour is checked against the local client data and the
  corresponding Vanilla/Turtle reference implementation where available.

The current development focus is CharacterSelect and CharacterCreate,
including dynamic race/class enumeration, character models, backgrounds,
equipment, geosets and race/class icons. Correctly configured additional DBC
entries should use the same flow without requiring a new C++ race case, as
long as the corresponding client assets and XML/Lua data exist.

## Credit and relationship to OpenWoW

The original OpenWoW project was created by
[rkabachenko](https://github.com/rkabachenko). OllieWow is an independent
experimental fork and is not affiliated with or endorsed by the upstream
project. OpenWoW remains the reference for the original 3.3.5 client
implementation.

This repository retains the applicable upstream notices and is released under
the [GNU Affero General Public License v3](LICENSE), unless a file or
third-party component states otherwise. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for third-party licensing
information.

## Requirements

You need:

- a C++20 compiler;
- CMake 3.24 or newer;
- vcpkg and the dependencies described by `vcpkg.json`;
- your own compatible local game data, including the required `Data/` files.

No game data is included in this repository.

## Building

Configure and build from the repository root:

```sh
cmake --preset release
cmake --build build/release --target openwow-client
```

The generated client is written to the existing build directory. Do not copy
game data into the repository or commit generated build output.

## Repository status

OllieWow is maintained as a local research and development fork. Features may
be incomplete, and compatibility claims require offline checks and runtime
verification against the intended local Classic/TurtleWoW data.

Bug reports and implementation notes can be filed through the
[OllieWow issue tracker](https://github.com/Monqeyking/OllieWow/issues).

## Legal

This project is not affiliated with, endorsed by, or associated with Blizzard
Entertainment, Inc. World of Warcraft and Blizzard Entertainment are
trademarks or registered trademarks of Blizzard Entertainment, Inc.

No original game assets, data files or server data are distributed here. You
must provide and use your own legally obtained game data.
