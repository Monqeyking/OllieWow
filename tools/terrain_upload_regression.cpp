// Headless integration fixture for the REAL incremental terrain uploader (not a mock).
// Requires C++20, include/ + src/ + bgfx headers, and rebuilt openwow_render_bgfx
// with its existing transitive engine/third-party dependencies (bgfx/bx/bimg, SDL2, etc.).
// No window, SDL initialization, real client data/file loader, or filesystem output is used.
// Scene failure injection uses an owning synthetic-texture callback, no assets or file I/O.
// BGFX_EMBEDDED_SHADER includes Noop bytecode in the installed BGFX version.
// Noop validates API/resource lifecycle, not visible pixels or actual driver latency.
#include "openwow/render/world/terrain/terrain_renderer.h"
#include "openwow/render/world/presentation/world_presentation_scene.h"
#include "openwow/render/m2/m2_system.h"
#include <atomic>
#include <thread>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
using openwow::render::PendingTerrainUpload;
using openwow::render::PreparedTerrainMaterialTextures;
using openwow::render::PreparedTerrainTile;
using openwow::render::TerrainRenderer;
using openwow::render::TerrainUploadStatus;
using openwow::render::TextureManager;
using Clock = std::chrono::steady_clock;
constexpr auto kUnlimited = Clock::time_point::max();
constexpr int32_t kTileX = 7;
constexpr int32_t kTileY = 11;

void Require(bool condition, const char *message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

struct NoopDevice {
  NoopDevice() {
    // Calling renderFrame before init selects single-threaded BGFX; no render thread/window.
    static_cast<void>(bgfx::renderFrame());
    bgfx::Init init;
    init.type = bgfx::RendererType::Noop;
    init.resolution.width = 1u;
    init.resolution.height = 1u;
    init.resolution.reset = BGFX_RESET_NONE;
    Require(bgfx::init(init), "headless Noop initialization failed");
  }
  NoopDevice(const NoopDevice &) = delete;
  NoopDevice &operator=(const NoopDevice &) = delete;
  ~NoopDevice() { bgfx::shutdown(); }
};

struct TextureShutdown {
  TextureManager &manager;
  ~TextureShutdown() { manager.Shutdown(); }
};

void FlushFrames() {
  // BGFX defers freeing handles. A fixed number of frames is not wall-clock polling.
  for (int frame = 0; frame < 3; ++frame) {
    static_cast<void>(bgfx::frame());
  }
}

struct Handles {
  std::uint16_t vertices;
  std::uint16_t indices;
  std::uint16_t textures;
  bool operator==(const Handles &) const = default;
};

Handles ReadHandles() {
  const auto *stats = bgfx::getStats();
  Require(stats != nullptr, "Noop resource statistics unavailable");
  return {stats->numVertexBuffers, stats->numIndexBuffers, stats->numTextures};
}

std::shared_ptr<const PreparedTerrainTile> MakeTile(bool replacement) {
  auto tile = std::make_shared<PreparedTerrainTile>();
  // A normal 145-vertex chunk exercises shared LOD0 indices; the second uses a hole-index
  // triangle subset. The changed subset distinguishes old and replacement tile publication.
  constexpr std::uint32_t kVerticesPerChunk = 145u;
  tile->vertices.resize(2u * kVerticesPerChunk);
  for (std::size_t chunk_index = 0u; chunk_index < 2u; ++chunk_index) {
    auto &chunk = tile->chunks[chunk_index];
    chunk.valid = true;
    chunk.vertex_start = static_cast<std::uint32_t>(chunk_index) * kVerticesPerChunk;
    chunk.vertex_count = kVerticesPerChunk;
    chunk.chunk_x = static_cast<std::uint32_t>(chunk_index);
    chunk.layer_count = 1; // Empty path uses existing checker fallback; no material/asset I/O.
    chunk.bounds_min[0] = static_cast<float>(chunk_index);
    chunk.bounds_max[0] = static_cast<float>(chunk_index + 1u);
    chunk.bounds_max[1] = 1.0f;
    chunk.bounds_max[2] = 1.0f;
    for (std::uint32_t local = 0u; local < kVerticesPerChunk; ++local) {
      auto &vertex = tile->vertices[chunk.vertex_start + local];
      vertex.position[0] = static_cast<float>(chunk_index) + static_cast<float>(local % 9u) / 8.0f;
      vertex.position[1] = static_cast<float>(local / 9u) / 16.0f;
      vertex.normal[2] = 1.0f;
      vertex.color = UINT32_MAX;
      vertex.alpha_slice[0] = static_cast<std::uint8_t>(chunk_index);
    }
  }
  tile->hole_indices = {0u, 1u, 9u, 1u, 10u, 9u};
  tile->chunks[1].hole_index_count = replacement ? 3u : 6u;
  tile->has_alpha_layers = true;
  constexpr std::size_t kSliceBytes =
      static_cast<std::size_t>(openwow::render::kAlphaMapSize) *
      openwow::render::kAlphaMapSize * 4u;
  tile->alpha_array_rgba.resize(TerrainRenderer::kChunksPerTile * kSliceBytes);
  for (std::size_t slice = 0; slice < TerrainRenderer::kChunksPerTile; ++slice) {
    for (std::size_t pixel = 0; pixel < kSliceBytes; pixel += 4u) {
      const auto offset = slice * kSliceBytes + pixel;
      tile->alpha_array_rgba[offset] = static_cast<std::uint8_t>(slice);
      // Alpha channel represents the prepared MCSH contribution, copied without conversion.
      tile->alpha_array_rgba[offset + 3u] = 127u;
    }
  }
  return tile;
}

void CheckVisible(const TerrainRenderer &renderer, std::size_t tiles, std::uint32_t triangles) {
  Require(renderer.loaded_tile_count() == tiles, "partial/duplicate tile became visible");
  Require(renderer.loaded_chunk_count() == tiles * 2u, "partial chunk set became visible");
  Require(renderer.total_triangles() == triangles, "old tile or hole-index semantics changed");
}

std::size_t Drain(TerrainRenderer &renderer, PendingTerrainUpload &pending,
                  std::size_t old_tiles, std::uint32_t old_triangles) {
  for (std::size_t units = 1u; units <= 1024u; ++units) {
    const auto status = renderer.PumpPreparedAdtUpload(pending, kUnlimited, 1u);
    static_cast<void>(bgfx::frame());
    if (status == TerrainUploadStatus::kComplete) {
      return units;
    }
    Require(status == TerrainUploadStatus::kPending, "real incremental upload failed");
    CheckVisible(renderer, old_tiles, old_triangles);
  }
  throw std::runtime_error("incremental upload did not complete within bounded fixture frames");
}

enum class CancelPoint { kBeforeVertexBuffer, kAfterVertexBuffer, kDuringAlpha };

void AdvanceTo(TerrainRenderer &renderer, PendingTerrainUpload &pending, CancelPoint point,
               Handles baseline, std::size_t old_tiles, std::uint32_t old_triangles) {
  for (std::size_t unit = 0u; unit < 1024u; ++unit) {
    Require(renderer.PumpPreparedAdtUpload(pending, kUnlimited, 1u) == TerrainUploadStatus::kPending,
            "upload published before cancellation checkpoint");
    static_cast<void>(bgfx::frame());
    CheckVisible(renderer, old_tiles, old_triangles);
    const auto handles = ReadHandles();
    if (point == CancelPoint::kBeforeVertexBuffer ||
        (point == CancelPoint::kAfterVertexBuffer && handles.vertices > baseline.vertices)) {
      return;
    }
    if (point == CancelPoint::kDuringAlpha && handles.textures > baseline.textures) {
      Require(handles.vertices > baseline.vertices && handles.indices > baseline.indices,
              "alpha allocation preceded complete geometry buffers");
      // Cancel after several actual self-contained alpha uploads, not merely allocation.
      for (int slice = 0; slice < 5; ++slice) {
        Require(renderer.PumpPreparedAdtUpload(pending, kUnlimited, 1u) ==
                    TerrainUploadStatus::kPending, "alpha slice upload published a partial tile");
        static_cast<void>(bgfx::frame());
        CheckVisible(renderer, old_tiles, old_triangles);
      }
      return;
    }
  }
  throw std::runtime_error("requested GPU cancellation checkpoint was never reached");
}

void Run(TerrainRenderer &renderer, std::shared_ptr<PendingTerrainUpload> &shutdown_survivor,
         std::shared_ptr<PendingTerrainUpload> &complete_survivor) {
  const auto old_tile = MakeTile(false);
  const auto new_tile = MakeTile(true);
  const auto materials = std::make_shared<const PreparedTerrainMaterialTextures>();
  const auto lod_triangles = openwow::render::TerrainLodManager::GetLodIndexData(0).count / 3u;
  const auto old_triangles = lod_triangles + 2u;
  const auto new_triangles = lod_triangles + 1u;
  FlushFrames();
  const auto empty_handles = ReadHandles();

  Require(!renderer.BeginPreparedAdtUpload({}, materials, kTileX, kTileY),
          "null input unexpectedly accepted");
  auto pending = renderer.BeginPreparedAdtUpload(old_tile, materials, kTileX, kTileY);
  Require(pending != nullptr, "Begin rejected synthetic tile");
  Require(renderer.PumpPreparedAdtUpload(*pending, kUnlimited, 0u) == TerrainUploadStatus::kPending,
          "zero-unit pump unexpectedly terminated");
  Require(renderer.PumpPreparedAdtUpload(*pending, Clock::time_point::min(), 16u) ==
              TerrainUploadStatus::kPending, "expired pump unexpectedly terminated");
  CheckVisible(renderer, 0u, 0u);
  Require(ReadHandles() == empty_handles, "zero/expired budget performed GPU work");
  Require(Drain(renderer, *pending, 0u, 0u) >= 2u * TerrainRenderer::kChunksPerTile,
          "per-chunk/per-alpha units were collapsed into monolithic publication");
  CheckVisible(renderer, 1u, old_triangles);
  Require(renderer.PumpPreparedAdtUpload(*pending, Clock::time_point::min(), 0u) ==
              TerrainUploadStatus::kComplete, "completion is not sticky");
  complete_survivor = std::move(pending);
  FlushFrames();
  const auto old_handles = ReadHandles();
  Require(old_handles.vertices == empty_handles.vertices + 1u &&
              old_handles.indices == empty_handles.indices + 1u &&
              old_handles.textures == empty_handles.textures + 1u,
          "complete tile does not own exactly one VB/IB/alpha array");

  for (auto point : {CancelPoint::kBeforeVertexBuffer, CancelPoint::kAfterVertexBuffer,
                     CancelPoint::kDuringAlpha}) {
    auto cancelled = renderer.BeginPreparedAdtUpload(new_tile, materials, kTileX, kTileY);
    Require(cancelled != nullptr, "replacement Begin failed");
    AdvanceTo(renderer, *cancelled, point, old_handles, 1u, old_triangles);
    cancelled.reset();
    FlushFrames();
    CheckVisible(renderer, 1u, old_triangles);
    Require(ReadHandles() == old_handles, "abandoned pending upload leaked/destroyed GPU handles");
  }

  auto invalid = std::make_shared<PreparedTerrainTile>(*new_tile);
  for (auto &chunk : invalid->chunks) { chunk.valid = false; }
  auto failed = renderer.BeginPreparedAdtUpload(invalid, materials, kTileX, kTileY);
  Require(failed != nullptr, "invalid-chunk fixture Begin failed too early");
  Require(renderer.PumpPreparedAdtUpload(*failed, kUnlimited, 1024u) == TerrainUploadStatus::kFailed,
          "empty batch indices did not fail");
  Require(renderer.PumpPreparedAdtUpload(*failed, kUnlimited, 0u) == TerrainUploadStatus::kFailed,
          "failure is not sticky");
  failed.reset();
  FlushFrames();
  CheckVisible(renderer, 1u, old_triangles);
  Require(ReadHandles() == old_handles, "failed index finalization leaked its vertex buffer");

  pending = renderer.BeginPreparedAdtUpload(new_tile, materials, kTileX, kTileY);
  Require(pending != nullptr, "replacement Begin failed");
  Drain(renderer, *pending, 1u, old_triangles);
  CheckVisible(renderer, 1u, new_triangles);
  pending.reset(); // transferred handles must not be destroyed here
  FlushFrames();
  Require(ReadHandles() == old_handles, "replacement transfer leaked/double-freed handles");

  // RemoveAdt removes published state, not caller-owned requests: parent eviction must
  // explicitly drop its pending request. ClearTerrain/Shutdown automatically cancel all.
  auto evicted = renderer.BeginPreparedAdtUpload(old_tile, materials, kTileX, kTileY);
  Require(evicted != nullptr, "eviction fixture Begin failed");
  AdvanceTo(renderer, *evicted, CancelPoint::kDuringAlpha, old_handles, 1u, new_triangles);
  evicted.reset();
  renderer.RemoveAdt(kTileX, kTileY);
  renderer.RemoveAdt(kTileX, kTileY); // idempotent removal
  FlushFrames();
  CheckVisible(renderer, 0u, 0u);
  Require(ReadHandles() == empty_handles, "eviction leaked tile or pending GPU handles");

  auto stale = renderer.BeginPreparedAdtUpload(old_tile, materials, kTileX, kTileY);
  auto queued = renderer.BeginPreparedAdtUpload(new_tile, materials, kTileX + 1, kTileY);
  Require(stale && queued, "ClearTerrain fixture Begin failed");
  AdvanceTo(renderer, *stale, CancelPoint::kAfterVertexBuffer, empty_handles, 0u, 0u);
  renderer.ClearTerrain();
  Require(renderer.PumpPreparedAdtUpload(*stale, kUnlimited, 1u) == TerrainUploadStatus::kFailed &&
              renderer.PumpPreparedAdtUpload(*queued, kUnlimited, 1u) == TerrainUploadStatus::kFailed,
          "ClearTerrain allowed stale requests to republish");
  stale.reset();
  queued.reset();
  FlushFrames();
  CheckVisible(renderer, 0u, 0u);
  Require(ReadHandles() == empty_handles, "ClearTerrain leaked pending GPU handles");

  shutdown_survivor = renderer.BeginPreparedAdtUpload(old_tile, materials, kTileX, kTileY);
  Require(shutdown_survivor != nullptr, "shutdown fixture Begin failed");
  AdvanceTo(renderer, *shutdown_survivor, CancelPoint::kDuringAlpha, empty_handles, 0u, 0u);
  renderer.Shutdown();
  Require(renderer.PumpPreparedAdtUpload(*shutdown_survivor, kUnlimited, 1u) ==
              TerrainUploadStatus::kFailed, "shutdown pending status is not terminal");
  Require(!renderer.BeginPreparedAdtUpload(old_tile, materials, kTileX, kTileY),
          "shutdown renderer accepted new upload");
  FlushFrames();
  Require(ReadHandles() == empty_handles, "shutdown leaked pending GPU handles");
}
void CheckSceneLifecycle(TextureManager& textures) {
  using namespace openwow;
  render::m2::M2System models;
  models.BindTextureManager(&textures);
  render::WorldPresentationScene scene(textures, models, [] { return render::SkyRenderSettings{}; });
  Require(scene.Initialize(), "headless presentation scene initialization failed");
  auto adt = std::make_shared<data::terrain::AdtFile>();
  auto liquids = std::make_shared<const std::vector<world::WaterHeightfield>>();
  std::uint64_t generation = 1u;
  const auto submit = [&](world::WorldPresentationCommand command, std::uint64_t next_generation = 0u) {
    if (next_generation != 0u) generation = next_generation;
    world::WorldPresentationCommandBatch batch;
    batch.generation = world::MapGeneration{generation};
    batch.commands.push_back(std::move(command));
    static_cast<void>(scene.Consume(std::move(batch)));
  };
  const auto publish = [&](std::shared_ptr<const data::terrain::AdtFile> data) {
    submit(world::PublishTerrainTileCommand{
        .owner = 42u, .tile_x = kTileX, .tile_y = kTileY, .adt = std::move(data),
        .liquids = liquids});
  };
  const auto tick = [&] {
    scene.Update(1.0f / 60.0f, {0.0f, 0.0f, 0.0f}, 1.0f, 0.0f, false, false);
    static_cast<void>(bgfx::frame());
    std::this_thread::yield();
  };
  const auto finish = [&] {
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    while (!scene.IsDoodadWorldEntryLoadDrained() && Clock::now() < deadline) tick();
    Require(scene.IsDoodadWorldEntryLoadDrained(), "scene terrain readiness never completed");
  };
  publish(adt);
  Require(!scene.IsDoodadWorldEntryLoadDrained(), "queued terrain released entry too early");
  // Unload while worker prep is in flight, then republish SAME coords.
  submit(world::RemoveTerrainTileCommand{.owner = 42u, .tile_x = kTileX, .tile_y = kTileY});
  Require(scene.IsDoodadWorldEntryLoadDrained(), "remove did not cancel queued terrain");
  publish(adt);
  finish();
  // A replacement/reset while GPU work is parked must discard the old generation.
  publish(adt);
  for (int frame = 0; frame < 3; ++frame) tick();
  submit(world::ResetPresentationCommand{}, 2u);
  Require(scene.IsDoodadWorldEntryLoadDrained(), "reset did not clear parked terrain");

  // First decode must succeed: the generic loader intentionally swallows exceptions.
  // Terrain's authored-mip reread is unwrapped, so fail on that SECOND fixture read.
  std::vector<std::uint8_t> tga(22u, 0u);
  tga[2] = 2u; tga[12] = 1u; tga[14] = 1u; tga[16] = 32u; tga[17] = 0x28u;
  tga[18] = tga[19] = tga[20] = tga[21] = 255u;
  Require(TextureManager::PrepareTextureUploadFromLoader(
      "fixture.blp", [tga](const std::string&) { return tga; }).valid,
      "synthetic texture preflight did not decode");
  auto loader_calls = std::make_shared<std::atomic<unsigned>>(0u);
  scene.SetFileLoader([loader_calls, tga](const std::string& path) -> std::vector<std::uint8_t> {
    if (path.find("fixture") == std::string::npos) return {};
    if (loader_calls->fetch_add(1u) == 0u) return tga;
    throw std::runtime_error("intentional terrain fixture preparation failure");
  });
  auto bad = std::make_shared<data::terrain::AdtFile>();
  bad->textures.push_back("fixture.blp");
  bad->chunks[0].layers.push_back({});
  publish(bad);
  const auto error_deadline = Clock::now() + std::chrono::seconds(10);
  while (loader_calls->load() < 2u && Clock::now() < error_deadline) tick();
  Require(loader_calls->load() >= 2u, "preparation exception fixture never reached worker");
  // Worker and task-record captures must be gone before asserting terminal readiness.
  // Only this local pointer and the scene's retained failed command should own the ADT.
  while (bad.use_count() > 2 && Clock::now() < error_deadline) tick();
  Require(bad.use_count() == 2, "failed tile lost ownership marker or task record was not forgotten");
  for (int frame = 0; frame < 100; ++frame) tick();
  Require(!scene.IsDoodadWorldEntryLoadDrained(), "failed preparation falsely released entry");
  Require(loader_calls->load() == 2u, "terminal preparation failure resubmitted every frame");
  submit(world::RemoveTerrainTileCommand{.owner = 42u, .tile_x = kTileX, .tile_y = kTileY});
  scene.SetFileLoader({});
  publish(adt);
  finish();

  // All256 fully-holed chunks have no drawable vertices, but are valid completed data.
  auto holes = std::make_shared<data::terrain::AdtFile>();
  for (auto& chunk : holes->chunks) chunk.holes = 0xffffu;
  publish(holes);
  finish();
  scene.Shutdown();
  FlushFrames();
}
}  // namespace

int main() {
  // These caller-held states intentionally outlive both renderer and BGFX shutdown.
  std::shared_ptr<PendingTerrainUpload> shutdown_survivor;
  std::shared_ptr<PendingTerrainUpload> complete_survivor;
  try {
    {
      NoopDevice device;
      Require(bgfx::getRendererType() == bgfx::RendererType::Noop, "backend fallback is forbidden");
      TextureManager textures;
      TextureShutdown texture_shutdown{textures};
      Require(textures.Initialize(), "headless TextureManager initialization failed");
      TerrainRenderer renderer(textures);
      Require(renderer.Initialize(), "embedded terrain Noop shaders failed to initialize");
      Run(renderer, shutdown_survivor, complete_survivor);
      CheckSceneLifecycle(textures);
    }
    shutdown_survivor.reset();
    complete_survivor.reset();
    std::cout << "terrain incremental upload Noop regression passed\n";
    return 0;
  } catch (const std::exception &error) {
    // Unwinding shuts the renderer down before these references are released.
    shutdown_survivor.reset();
    complete_survivor.reset();
    std::cerr << "terrain incremental upload Noop regression failed: " << error.what() << "\n";
    return 1;
  }
}
