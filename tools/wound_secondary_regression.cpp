// Parent builds/runs from the existing build directory; this session does neither.
// Link animation_state.cpp: the primary clocks and request are production types.
#include "openwow/game/objects/unit/unit_animation_runtime.h"
#include "openwow/render/models/animation/wound_secondary_state.h"
#include "openwow/render/m2/m2_wound_mask.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace openwow::render;
using Request = openwow::game::UnitAnimationRuntime::PlaybackRequest;
void Require(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
}
bool Near(float a, float b) { return std::fabs(a - b) < 0.0001f; }

void ReadyAndSwing() {
  AnimationState base, primary;
  base.Restart(26u, true);
  primary.Restart(17u, false);
  base.Update(0.125f, 1000u);
  primary.Update(0.25f, 1000u);
  Request request;
  request.animation_id = 17u;
  request.base_animation_id = 26u;
  request.upper_body_only = true;
  request.serial = 47u;
  const auto before = request;
  const auto base_clock = base.current_time_ms();
  const auto primary_clock = primary.current_time_ms();
  WoundSecondaryRequest wound;
  WoundSecondaryState state;
  Require(WoundFullBody(9u, 26u, false), "Ready must choose full-body secondary");
  Require(wound.Trigger(9u, 1000u, false, false), "Ready trigger failed");
  state.Synchronize(wound);
  Require(Near(state.weight(), 0.75f), "wound must start at 0.75, not replace");
  Require(Near(WoundBlendComponent(2.0f, 10.0f, state.weight()), 8.0f),
          "full-body secondary did not blend primary pose");
  Require(request == before && base.current_time_ms() == base_clock &&
          primary.current_time_ms() == primary_clock, "trigger mutated primary ownership");
  state.Update(0.5f);
  Require(Near(state.weight(), 0.375f), "decay must use smoothstep remaining span");
  // Full-body inherited swing is masked, rather than blocked mid-swing.
  Require(!WoundFullBody(9u, 17u, false), "inherited swing must remain under masked wound");
  Require(wound.Trigger(9u, 1000u, true, false), "swing wound failed");
  state.Synchronize(wound);
  Require(request == before && primary.current_anim() == 17u, "swing ID/serial replaced");
  base.Update(0.125f, 1000u);
  primary.Update(0.125f, 1000u);
  state.Update(0.125f);
  Require(base.current_time_ms() == base_clock + 125u &&
          primary.current_time_ms() == primary_clock + 125u, "primary clocks did not keep advancing");
  Require(!base.DidAnimationComplete() && !primary.DidAnimationComplete(),
          "secondary contaminated primary completion");
}

void RunMaskAndUnsplit() {
  struct Bone { std::int16_t parent; };
  // root, leg, spine, arm: only spine/arm are in the wound subtree.
  const std::array<Bone, 4> bones{{{-1}, {0}, {0}, {2}}};
  const std::array<std::int16_t, 1> keys{{2}};
  Require(!WoundFullBody(8u, 5u, false), "run wound must mask");
  Require(WoundFullBody(8u, 0u, true), "stationary StandWound full-body routing lost");
  Require(!WoundFullBody(9u, 0u, true), "combat kit Stand is not StandWound");
  const auto sample = [&](std::size_t bone, std::uint32_t slot) {
    const float lambda = m2::M2WoundMaskAffectsBone(bones, keys, slot, bone) ? 0.75f : 0.0f;
    return WoundBlendComponent(4.0f, 12.0f, lambda);
  };
  Require(Near(sample(1u, 0u), 4.0f) && Near(sample(3u, 0u), 10.0f),
          "actual M2 wound mask altered running legs or missed upper subtree");
  Require(Near(sample(1u, 0xFFFFFFFFu), 10.0f),
          "no-keybone fallback must full-body BLEND, not replacement");
  Require(!m2::M2WoundMaskAffectsBone(bones, keys, 7u, 1u), "invalid lookup must be bounded");
  const std::array<Bone, 2> malformed{{{-1}, {1}}};
  const std::array<std::int16_t, 1> root{{0}};
  Require(!m2::M2WoundMaskAffectsBone(malformed, root, 0u, 1u),
          "malformed hierarchy must not loop");
}

void RetriggerEvictionAndDeath() {
  WoundSecondaryRequest wound;
  WoundSecondaryState state;
  wound.Trigger(9u, 1000u, true, false);
  state.Synchronize(wound);
  state.Update(0.5f);
  const auto old_serial = wound.serial;
  wound.Trigger(10u, 1000u, true, false);
  Require(wound.serial != old_serial && state.Synchronize(wound), "retrigger must reseed secondary only");
  Require(state.time_ms() == 0u && Near(state.weight(), 0.75f), "retrigger did not reset own clock");
  Require(!wound.EvictForPrimaryRearm(false, false), "other-bone rearm evicted masked secondary");
  Require(!wound.EvictForPrimaryRearm(true, true), "zero-blend must not claim secondary");
  Require(wound.EvictForPrimaryRearm(true, false), "same-bone blended rearm must evict");
  state.Synchronize(wound);
  Require(!state.active && Near(state.weight(), 0.0f), "eviction left pose armed");
  wound.Trigger(8u, 1000u, false, false);
  Require(!wound.EvictForPrimaryRearm(true, false), "masked swing must not evict full-body wound");
  Require(wound.EvictForPrimaryRearm(false, false), "full-body rearm must evict full-body wound");
  wound.Trigger(9u, 500u, true, false);
  state.Synchronize(wound);
  AnimationState death;
  death.Restart(AnimId::kDeath, false);
  death.Update(0.125f, 2000u);
  const auto saved = wound;
  Require(!wound.Trigger(10u, 500u, true, true) && wound == saved, "dead unit must reject new wound");
  state.Update(0.5f);
  Require(!state.active && Near(state.weight(), 0.0f), "death must not stop secondary upkeep");
  Require(death.current_anim() == AnimId::kDeath && death.current_time_ms() == 125u,
          "secondary expiry stood corpse up or changed death clock");
  Require(!state.Synchronize(wound) && !state.active, "stable publication resurrected expired wound");
}

void KitAndZeroSpan() {
  Require(SelectMeleeWound(false, false) == 8u && SelectMeleeWound(false, true) == 9u &&
          SelectMeleeWound(true, false) == 10u, "melee selection contract");
  WoundSecondaryRequest wound;
  for (std::uint16_t kit = 8u; kit <= 10u; ++kit) {
    Require(wound.Trigger(kit, 500u, false, false) && wound.animation_id == kit,
            "kit ID must not be reselected by melee severity");
  }
  const auto saved = wound;
  Require(!wound.Trigger(8u, 0u, true, false) && wound == saved,
          "zero-span trigger must not disturb live secondary");
  Require(Near(WoundSecondaryWeight(0.0, 0u), 0.0f), "zero span division");
  WoundSecondaryState state;
  state.Synchronize(wound);
  state.Update(std::numeric_limits<float>::quiet_NaN());
  state.Update(-1.0f);
  Require(state.time_ms() == 0u, "invalid dt must not contaminate clock");
  state.Update(10.0f);
  Require(!state.active && state.time_ms() == 500u, "overshoot release must clamp to span");
}
}

int main() {
  try {
    ReadyAndSwing();
    RunMaskAndUnsplit();
    RetriggerEvictionAndDeath();
    KitAndZeroSpan();
    std::cout << "wound secondary regression: ready/swing/run/no-keybone/retrigger/eviction/death/kitID/zero-span PASS\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "wound secondary regression: " << error.what() << "\n";
    return 1;
  }
}
