#include "openwow/render/world/wmo/wmo_material_pipeline.h"
#include "openwow/world/wmo/wmo_vertex_color_query.h"
#include "openwow/render/world/wmo/wmo_mesh.h"
#include "openwow/render/world/wmo/wmo_renderer.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

// Source-only fixture. Build/run from build/ only after separate approval.
// Link the normal mesh/vertex-colour dependencies; no GPU/client is started.
namespace {
void Require(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
void Near(float actual, float expected, const char* message) {
  Require(std::abs(actual - expected) < 0.00001f, message);
}
}

int main() {
  using namespace openwow::render;
  using namespace openwow::data::wmo;
  try {
    WmoGroup group{};
    group.vertices.resize(3);
    group.vertexColors = {{10, 20, 30, 0}, {40, 50, 60, 0}, {70, 80, 90, 0}};
    // GPU color is authored/raw regardless of MOHD flags; CPU query bakes are separate.
    const auto full = GenerateWmoGroupMesh(group, kWmoFlagHasVertexColor);
    Require(full.vertices[0].color == 0x000A141Eu, "BGRA to RGBA packing");
    const auto first = full.vertices[0].color;
    const auto last = full.vertices[1].color;
    group.vertexColors.pop_back();
    for (auto flags : {unsigned(kWmoFlagHasVertexColor),
                      unsigned(kWmoFlagHasVertexColor | kWmoFlagUnifiedRender)}) {
      const auto short_tail = GenerateWmoGroupMesh(group, flags);
      Require(short_tail.vertices[0].color == first, "keep existing MOCV colours");
      Require(short_tail.vertices[1].color == last, "keep final complete MOCV");
      Require(short_tail.vertices[2].color == last, "repeat only one missing tail colour");
    }
    group.vertexColors.pop_back();
    const auto broken = GenerateWmoGroupMesh(group, kWmoFlagHasVertexColor);
    for (const auto& vertex : broken.vertices)
      Require(vertex.color == 0xFFFFFFFFu, "do not extrapolate a larger colour gap");
    group.vertexColors.resize(4);
    const auto excess = GenerateWmoGroupMesh(group, kWmoFlagHasVertexColor);
    for (const auto& vertex : excess.vertices)
      Require(vertex.color == 0xFFFFFFFFu, "excess colours are not a recoverable tail");
    group.vertexColors.clear();
    const auto absent = GenerateWmoGroupMesh(group, kWmoFlagHasVertexColor);
    for (const auto& vertex : absent.vertices)
      Require(vertex.color == 0xFFFFFFFFu, "absent MOCV stays neutral");

    // TRANS and padded INT keep identical authored RGB and alpha.
    // Their lighting difference is selected per draw, never baked into mesh bytes.
    group.vertexColors = {{10, 20, 30, 0}, {40, 50, 60, 64}};
    group.header.transBatchCount = 1;
    group.renderBatches.resize(1);
    group.renderBatches[0].lastVertex = 1;
    const auto prepared = GenerateWmoGroupMesh(group, 0);
    Require(prepared.vertices[1].color == 0x4028323Cu, "TRANS raw RGB and alpha preserved");
    Require(prepared.vertices[2].color == 0x4028323Cu, "padded INT preserves raw tail");
    Require(prepared.has_vertex_colors && !absent.has_vertex_colors &&
            !broken.has_vertex_colors && !excess.has_vertex_colors, "authored MOCV metadata");

    using Region = WmoBatchMesh::Region;
    for (bool unified : {false, true}) {
    for (auto material : {0u, unsigned(kMatUnlit), unsigned(kMatWindow),
                          unsigned(kMatUnlit | kMatWindow)}) {
      const auto interior_lit = (material & kMatWindow) != 0u
          ? WmoLightingMode::Window : WmoLightingMode::Outdoor;
      Require(ResolveRetailWmoLightingMode(0, Region::Interior, material, unified) ==
              WmoLightingMode::Unlit, "INT stays baked even with WINDOW");
      for (auto region : {Region::Transition, Region::Exterior})
        Require(ResolveRetailWmoLightingMode(0, region, material, unified) ==
                interior_lit, "TRANS/EXT lit lane ignores interior UNLIT");
      for (auto flags : {unsigned(kMogpExterior), unsigned(kMogpExteriorLit)})
        for (auto region : {Region::Interior, Region::Transition, Region::Exterior})
          Require(ResolveRetailWmoLightingMode(flags, region, material, unified) ==
                  ((material & kMatUnlit) != 0u ? WmoLightingMode::Unlit
                                              : WmoLightingMode::Outdoor),
                  "exterior drawer honors UNLIT and ignores WINDOW");
      Require(ResolveRetailWmoLightingMode(0, Region::Interior, material, false) ==
              WmoLightingMode::Unlit, "preserve non-unified interior bake");
    }

    }
    for (auto flags : {0u, unsigned(kMogpExterior), unsigned(kMogpExteriorLit),
                       unsigned(kMogpExterior | kMogpExteriorLit)})
      for (auto region : {Region::Interior, Region::Transition, Region::Exterior})
        Require(UsesClassicWmoTransitionBlend(flags, region) ==
                (flags == 0u && region == Region::Transition),
                "only interior TRANS receives lit/bake passes");

    for (bool unified : {false, true}) {
      WmoVertexLightingInput input{};
      input.unified_render_path = unified;
      input.vertex_color = {127.0f / 255.0f, 0.1f, 0.25f};
      input.ambient = {0.2f, 0.2f, 0.2f};
      input.diffuse = {};
      const auto lit = EvaluateRetailWmoVertexColor(input);
      Near(lit[0], input.vertex_color[0] * 0.2f, "no neutral fullbright addend");
      Near(lit[1], 0.02f, "baked colour modulates daylight");
      Near(lit[2], 0.05f, "baked colour modulates daylight");
      input.ambient = {};
      const auto dark = EvaluateRetailWmoVertexColor(input);
      for (float channel : dark) Near(channel, 0.0f, "lit lane has no light floor");
      input.lighting_enabled = false;
      const auto bake = EvaluateRetailWmoVertexColor(input);
      for (unsigned c = 0; c < 3; ++c)
        Near(bake[c], input.vertex_color[c], "INT/TRANS bake remains unlit");
      // Preserved two-pass contract: lit * alpha + bake * (1-alpha).
      for (float alpha : {0.0f, 0.5f, 1.0f})
        Near(lit[1] * alpha + bake[1] * (1.0f-alpha),
             input.vertex_color[1] * (0.2f * alpha + 1.0f-alpha),
             "TRANS endpoints and midpoint");
    }
    // The prior exterior amplification example: c=64,a=128 must stay authored.
    group.vertexColors.assign(3, WmoVertexColor{64, 64, 64, 128});
    group.header.flags = kMogpExterior;
    for (auto root_flags : {0u, unsigned(kWmoFlagHasVertexColor),
                            unsigned(kWmoFlagUnifiedRender)}) {
      const auto raw = GenerateWmoGroupMesh(group, root_flags);
      Require(raw.vertices[0].color == 0x80404040u, "all MOHD paths upload raw outside MOCV");
      WmoVertexLightingInput vertex{};
      vertex.vertex_color = {64.0f/255.0f, 64.0f/255.0f, 64.0f/255.0f};
      vertex.ambient = {0.5f, 0.5f, 0.5f}; vertex.diffuse = {};
      WmoPixelMaterialInput pixel{};
      pixel.lit_vertex_color = EvaluateRetailWmoVertexColor(vertex);
      pixel.diffuse_texture = {0.5f, 0.5f, 0.5f, 1.0f};
      pixel.vertex_alpha = 128.0f/255.0f;
      Near(EvaluateRetailWmoPixelMaterial(pixel)[0], 16.0f/255.0f,
           "outside has no alpha gain or universal x2");
      pixel.vertex_alpha = 0.0f;
      Near(EvaluateRetailWmoPixelMaterial(pixel)[3], 1.0f,
           "MOCV alpha zero does not hide opaque/cutout outside geometry");
      pixel.diffuse_texture[3] = 0.25f;
      Near(EvaluateRetailWmoPixelMaterial(pixel)[3], 0.25f, "texture cutout/blend alpha preserved");
      pixel.shader = kShaderOpaque;
      Near(EvaluateRetailWmoPixelMaterial(pixel)[3], 1.0f, "opaque material ignores texture alpha");
      pixel.shader = kShaderDiffuse;
      pixel.diffuse_texture[3] = 1.0f;
      pixel.interior_self_illumination = true;
      pixel.lit_vertex_color = vertex.vertex_color;
      pixel.vertex_alpha = 128.0f/255.0f;
      Near(EvaluateRetailWmoPixelMaterial(pixel)[0],
           (32.0f/255.0f)*(1.0f+4.0f*pixel.vertex_alpha), "INT-only authored alpha gain");
      pixel.interior_self_illumination = false;
      pixel.transition_blend = true;
      for (float alpha : {0.0f, 0.5f, 1.0f}) {
        pixel.vertex_alpha = alpha;
        Near(EvaluateRetailWmoPixelMaterial(pixel)[3], alpha, "TRANS draw keeps blend weight");
      }
      vertex.lighting_enabled = false;
      vertex.emissive = {0.9f, 0.8f, 0.7f};
      Near(EvaluateRetailWmoVertexColor(vertex)[0], 64.0f/255.0f, "UNLIT bypasses SIDN and keeps raw scale");
    }
    // Render doorway fade is independent of CPU signed/half-scale color queries.
    WmoRoot root{};
    root.groupInfos.resize(1); root.groupInfos[0].flags = kMogpExterior;
    root.portalVertices = {{0,-2,-2},{0,2,-2},{0,2,2},{0,-2,2}};
    root.portals.resize(1);
    root.portals[0].startVertex = 0; root.portals[0].nVertices = 4;
    root.portals[0].normal[0] = 1.0f;
    root.portalRefs.resize(2);
    for (auto& ref : root.portalRefs) { ref.groupIndex = 0; ref.portalIndex = 0; ref.side = 1; }
    group.header.portalStart = 0; group.header.portalCount = 2;
    const WmoVertexColor authored{64,64,64,128};
    const auto center = openwow::world::PrepareWmoRenderPortalVertexColor(root,group,{0.1f,0,0},authored);
    Require(center.r == 255 && center.a == 255, "portal outline plane epsilon goes white opaque");
    const auto retained = openwow::world::PrepareWmoRenderPortalVertexColor(root,group,{1,0,0},authored);
    Require(retained.r == 64 && retained.a == 128, "nonzero authored alpha stays unchanged away from door");
    const auto faded = openwow::world::PrepareWmoRenderPortalVertexColor(root,group,{1,0,0},{64,64,64,0});
    Require(faded.r > 64 && faded.r < 255 && faded.a > 0 && faded.a < 255, "alpha zero fades towards full white");
    group.header.portalCount = 1;
    const auto single = openwow::world::PrepareWmoRenderPortalVertexColor(root,group,{1,0,0},{64,64,64,0});
    Require(single.r == faded.r && single.a == faded.a, "nearest door is not summed twice");
    root.groupInfos[0].flags = 0;
    const auto no_door = openwow::world::PrepareWmoRenderPortalVertexColor(root,group,{0,0,0},authored);
    Require(no_door.r == 64 && no_door.a == 128, "interior-neighbor portals do not whiten");
    const auto no_emission = EvaluateRetailWmoMaterialEmissive(0xffffffffu, 0u, 1.0f, false);
    Near(no_emission[0], 0.0f, "ambient must not be emitted a second time");
    const auto sidn = EvaluateRetailWmoMaterialEmissive(0xffffffffu, 0x00804020u, 0.5f, true);
    Near(sidn[0], 64.0f/255.0f, "SIDN full-scale emission red");
    Near(sidn[1], 32.0f/255.0f, "SIDN full-scale emission green");
    Near(sidn[2], 16.0f/255.0f, "SIDN full-scale emission blue");
    std::cout << "WMO lighting regression passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
