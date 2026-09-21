# Benilla/OpenWow terrain seam audit

Date: 2026-09-19

## Scope and revisions

- OpenWow worktree: D:/OllieWoW/Experiments/OpenWow-snapshot/.
- Local Benilla checkout: 73826ca3d751a7d74792d4ca463b2d1d1d9c2bd4 (2026-08-25).
- Upstream Benilla main inspected: 43b282a07c88748d5e693a1a49e771e937601d01 (2026-09-19), 164 commits ahead of local.

## Source comparison

| Aspect | Benilla reference | OpenWow before this correction | Assessment |
|---|---|---|---|
| MCNK mesh | benilla-formats/src/terrain.rs: 145 vertices, authored MCNR normals, 4-triangle center fan, MCNK holes | src/openwow/render/world/terrain/terrain_mesh.cpp: same topology, normal order, hole mapping and center fan | No concrete topology deviation found. |
| Shared outer positions | Benilla snaps outer vertices to a global WoW lattice | OpenWow now does the same in terrain_mesh.cpp | Source-backed geometry correction already present; runtime effect not yet confirmed. |
| Alpha maps | benilla-assets/src/shaders/terrain.wgsl:252-258: one local 64x64 map per chunk, array-indexed and clamped to half a texel before linear filtering | OpenWow now uploads one 64x64 RGBA array slice per MCNK and samples it with a per-vertex slice index | The shared-atlas deviation is removed; MCSH remains in the fourth channel as a temporary OpenWow compatibility detail. |
| WDL boundary/mesh | wdl.wgsl: overlap at farclip-33 plus fragment depth push/clamp; benilla-formats/src/wdl.rs builds tile coordinates from a shared double-precision lattice | OpenWow used f32 tile origins plus local subtraction in distant_terrain.cpp, omitted the farclip-33 fragment discard, and used a vertex depth push near 1.0 | Concrete lattice deviation corrected; the new shader build adds the source-backed farclip-33 discard as the next depth diagnostic. |
| Terrain residency | Current Benilla terrain_stream.rs derives StreamWindow::at(view.farclip, ...) | OpenWow TerrainStreamer uses fixed tile radii (5/7) and is not derived from farclip | Architectural difference, but over-residency alone does not explain a stationary camera-rotation seam. |
| Ground layer textures | benilla-assets/src/terrain.rs: 256² array layers, eight authored/nearest-filled mips, non-sRGB RGBA8, Repeat + Linear/mipmap + anisotropy 8 | OpenWow previously kept native BLP dimensions/mips, bucketed arrays by native shape and fell back to separate textures | New source-backed normalization implemented in TextureManager for terrain uploads; runtime effect pending. |

## Implemented minimal correction

The terrain alpha path now follows Benilla's resource contract in the affected layer route. `src/openwow/render/world/terrain/terrain_mesh.cpp` packs 256 local 64x64 RGBA slices per ADT, emits local 0..1 alpha UVs and a per-vertex MCNK slice, and retains unconditional last-row/last-column repair. `src/openwow/render/world/terrain/terrain_renderer.cpp` creates a 64x64, 256-layer BGFX texture array and uploads each chunk slice. `src/openwow/render/shaders/fs_terrain_splat_body.sh` clamps local alpha UVs to [0.5/64, 1-0.5/64] and samples `texture2DArray`, matching Benilla's `terrain.wgsl:252-258`; the alpha array's fourth channel still carries OpenWow's existing MCSH value for now. The diffuse layer array/fallback paths remain otherwise unchanged, and WoWee's extra edge feather is intentionally not included yet.

In src/openwow/render/world/terrain/distant_terrain.cpp, WDL outer and inner vertices now derive their coordinates from global double-precision lattice indices, matching Benilla's wdl.rs tile_mesh construction instead of subtracting f32 offsets from a rounded tile origin. This is a reversible diagnostic/fix for one-pixel tile-edge gaps and z-fighting that move with camera rotation.

After the user reported that the supplied 1350x762 runtime capture still contains the black polygons, fs_distant_terrain.sc added Benilla's exact planar near-side condition: discard WDL fragments with eye-Z < farclip - 33.0. A temporary green/red debug pass then identified WDL versus detailed-terrain ownership; those debug outputs and artifacts were later removed. All fifteen detailed shader variants plus the WDL variants compiled successfully.

No MPQ, Client, Benilla, server, database, or active process was modified.

## Verification

- Both fs_terrain_splat.sc and fs_terrain_splat_array.sc compiled for GLSL, ESSL, SPIR-V, Metal and DX11.
- An independent adjacent-edge arithmetic check over all 63 WDL tile joins reported 0 mismatches after the lattice change.
- All five WDL shader targets and all fifteen detailed-terrain shader targets compiled with shaderc.
- `dev.ps1 -NoLink` rebuilt the registry object successfully; the shader registry was touched after generated-header changes so the current red/green variants are embedded in the alternate link.
- An earlier `dev.cmd` invocation was blocked by the running `OllieWow.exe` lock; its wrapper incorrectly printed `DEV_BUILD_OK` after the LNK1104 failure. The later normal rebuild completed successfully after the lock was gone.
- Earlier pass-debug artifacts were linked while the static render library still held the previous shader registry object; they are not used for final interpretation. The render library was then rebuilt after the red terrain shaders and MAHO state change.
- Refreshed verified artifact: build/release/apps/client/OllieWow-pass-debug-full.exe, linked directly from the current response file after the library relink.
- Launcher created: build/release/apps/client/Start-OllieWoW-PassDebug-Full.cmd; it checks the executable and D:\OllieWoW\Client\Data, then starts the refreshed artifact.
- MAHO diagnostic: distant_terrain.cpp now submits MAHO indices without the extra CULL_CW state; this is embedded in the refreshed full artifact.
- Runtime full-red capture: the black regions disappeared under detailed-terrain red, so the affected pixels are in detailed-terrain coverage.
- Factor diagnostic: fs_terrain.sc, fs_terrain_splat.sc and fs_terrain_splat_array.sc were compiled for GLSL, ESSL, SPIR-V, Metal and DX11; the render library was relinked and build/release/apps/client/OllieWow-terrain-factor-debug.exe was linked directly. Launcher: Start-OllieWoW-TerrainFactorDebug.cmd. The user observed the seam signature in the blue channel, which is the MCSH shadow factor in this diagnostic.
- MCSH-off textured capture: black lines remained while static MCSH modulation was forced to 1.0, so MCSH is not sufficient to explain the artifact.
- Texture-only diagnostic: current textured shaders remove both vertex lighting and MCSH while retaining base/layer texture blending; build/release/apps/client/OllieWow-terrain-texture-only.exe and Start-OllieWoW-TerrainTextureOnly.cmd were compiled, embedded and linked successfully. The user captured the same black lines in this mode, ruling out vertex lighting and MCSH; the remaining detailed input is layer texture/alpha blending.
- Alpha-weight diagnostic: RGB now exposes `alphaBytes.rgb` directly and white represents solid terrain; build/release/apps/client/OllieWow-terrain-alpha-debug.exe and Start-OllieWoW-TerrainAlphaDebug.cmd were compiled and linked successfully.
- All temporary pass/factor/MCSH-off/texture-only/WDL/alpha debug executables, sidecars and launchers were removed after their runtime observations. The diagnostic shader source was restored to normal-color output.
- Minimal alpha repair build: all normal terrain/WDL shader variants compiled for GLSL, ESSL, SPIR-V, Metal and DX11 and the render libraries relinked. The first executable relink was blocked by LNK1104 while the client was running; after the user closed it, `dev.cmd` relinked `build/release/apps/client/OllieWow.exe` successfully. No process was killed automatically and no client was started.
- Benilla alpha-array migration: the four terrain splat shader pairs compiled directly with shaderc for GLSL, ESSL, SPIR-V, Metal and DX11; `dev.ps1` then compiled the changed C++ objects and relinked the normal `build/release/apps/client/OllieWow.exe` successfully. The client was not started automatically, so runtime seam validation remains pending.
- Benilla ground-layer migration: `dev.ps1` compiled 477 affected objects and relinked all required libraries plus `build/release/apps/client/OllieWow.exe` successfully. No client was started automatically. The user then ran the normal alpha-array build: the black lines remained. A temporary base-only shader (`blended = c0`, with all alpha/layer mixing removed) also left the black lines unchanged; that diagnostic was reverted and the normal layer blending was rebuilt. Together with the earlier MCSH-off observation, this moves the primary suspect away from MCAL/MCSH and toward the detailed terrain texture mip/upload path or geometry/depth overlap.

## Runtime interpretation

Existing artifacts wdl-baseline.png/wdl-nocull.png, sh-off.png/sh-on.png and chk-a.png/chk-b.png visibly retain thin black terrain lines across their A/B captures. They were created before the verified shader embedding and therefore do not validate the latest executable; they do show that the earlier culling/shadow experiments did not remove the phenomenon. The user subsequently supplied a 1350x762 capture and reported the black polygons are still visible after the lattice build, so the lattice correction is not sufficient by itself. The later red/green capture shows bright-green WDL geometry behind the scene while the offending polygons remain black; because the WDL debug shader is visibly active, those black pixels are not produced by the WDL fragment shader.

The red/green result rules out the WDL fragment shader as the black-pixel producer. The refreshed full build then rendered the affected ground coverage red and the black polygons disappeared; this proves the affected pixels are covered by the detailed-terrain draw, not only by WDL. The factor capture highlighted the blue channel, which represents the MCSH shadow factor. Benilla's terrain.wgsl:281-300 confirms this is a static per-chunk MCSH input, not the camera-following shadow map. The user then ran the MCSH-off and texture-only captures; the black lines remained with MCSH and vertex lighting removed. This localizes the remaining problem to the detailed layer/base texture route, but does not by itself distinguish alpha data from a diffuse sampler/texture handle.

The supplied original-WoW capture has the same terrain location without black seams. It also has materially different global lighting, fog, texture filtering/LOD and object presentation, so it is evidence that the MPQ terrain is not intentionally authored with those black lines, not a pixel-for-pixel shader baseline. The alpha-array migration now matches Benilla's per-chunk resource/sampling contract, but the user's rebuilt capture is unchanged. The temporary base-only test (`blended = c0`) was also unchanged and has been reverted; together with the earlier MCSH-off test this rules out alpha/layer mixing as the primary explanation.

The next Benilla delta was the ground-layer texture contract, not WoWee feathering: `benilla-assets/src/terrain.rs:1-8` requires authored mip fidelity, `:19-25` fixes the array at 256² with eight mips, `:117-140` uses Repeat + Linear mag/min/mipmap filtering with anisotropy 8, and `:143-152` keeps alpha and MCSH arrays separate. OpenWow now implements the terrain portion through `TextureManager::PrepareTerrainLayerTextureUploadFromLoader`: BLPs are decoded to RGBA8, authored matching mips are retained, missing/undersized levels are nearest-filled to a uniform 256²/8-level chain, and `PrepareMaterialTextures` routes terrain layers through it. The normal client relinked successfully. Runtime validation is now required; if the black lines remain, geometry/depth overlap becomes the next branch.
