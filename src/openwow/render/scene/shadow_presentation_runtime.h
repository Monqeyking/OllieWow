#pragma once

#include "openwow/render/scene/shadow_data.h"
#include "openwow/render/m2/m2_public_types.h"
#include "openwow/render/api/math/render_math_types.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace openwow::world {
struct WorldPresentationSnapshot;
}
namespace openwow::core {
class FrameJobSystem;
}

namespace openwow::render {

class DoodadRenderer;
class TerrainRenderer;
namespace m2 {
class M2System;
}

class ShadowPresentationRuntime final {
public:
  explicit ShadowPresentationRuntime(m2::M2System &m2_system);
  ~ShadowPresentationRuntime();

  ShadowPresentationRuntime(const ShadowPresentationRuntime &) = delete;
  ShadowPresentationRuntime &operator=(const ShadowPresentationRuntime &) = delete;

  [[nodiscard]] bool Initialize();
  void Shutdown();
  void ResetMap();

  void Render(const world::WorldPresentationSnapshot &snapshot, std::uint8_t shadow_view,
              DoodadRenderer &doodads, TerrainRenderer &terrain);

  // Extra schaduwwerpers naast de doodads (units en spelers): vult de M2-instance-id's van
  // alles dat binnen `radius` van (x, y, z) schaduw moet werpen.
  using ExtraCasterProvider = std::function<void(float x, float y, float z, float radius,
                                                 std::vector<std::uint32_t> &out)>;
  void SetExtraCasterProvider(ExtraCasterProvider provider) {
    extra_caster_provider_ = std::move(provider);
  }

private:
  void ApplySettings(const world::WorldPresentationSnapshot &snapshot);

  void InvalidateShadowReuse() noexcept;

  struct ShadowFrameKey {

    std::uint64_t content_hash{0u};

    std::uint64_t previous_content_hash{0u};

    std::uint32_t caster_count{0u};

    bool reusable{false};

    [[nodiscard]] bool operator==(const ShadowFrameKey &) const noexcept = default;
  };

  m2::M2System &m2_system_;
  std::unique_ptr<ShadowRenderData> data_;
  std::vector<ShadowCasterEntry> casters_;
  std::vector<std::uint32_t> instance_ids_;
  std::vector<std::uint32_t> extra_caster_ids_;
  ExtraCasterProvider extra_caster_provider_;

  struct InstancedShadowGroup {
    std::uint32_t exemplar_instance_id{0};

    std::vector<m2::M2InstancedDrawRecord> records;
    std::vector<std::uint32_t> member_ids;
  };
  std::unordered_map<std::uint32_t, InstancedShadowGroup> instanced_groups_;

  std::vector<m2::M2RenderInstanceResult> render_results_scratch_;

  ShadowFrameKey rendered_key_{};
  bool has_rendered_key_{false};

  std::uint64_t previous_content_hash_{0u};
  bool has_previous_content_hash_{false};
  std::uint16_t resolution_{0};
  bool initialized_{false};
};

}
