// Standalone header-only offline fixture; no game/runtime dependencies.
// Build/test execution belongs to the parent, from the existing build directory.
#include "openwow/game/missile_release_clock.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using openwow::game::MissileHomingFraction;
using openwow::game::MissileReleaseClock;
using openwow::game::MissileReleaseGate;

void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
bool Near(float actual, float expected) {
  return std::fabs(actual - expected) < 0.0001f;
}

void CheckClockAndHoming() {
  const std::array<float, 3> src{0.0f, 0.0f, 0.0f};
  const auto clock = MissileReleaseClock::FromGo(1000, src, {10.0f, 0.0f, 0.0f}, 10.0f);
  Require(clock.go_tick == 1000 && clock.deadline_tick == 2000,
          "travel deadline must originate at GO");
  Require(Near(clock.RemainingSeconds(1250), 0.75f),
          "delayed release restarted travel duration");
  // Source ownership changes at release: a moving caster's live marker is not
  // the GO sample. Neither this motion nor target motion rebases the GO clock.
  const std::array<float, 3> live_marker{4.0f, 2.0f, 1.0f};
  MissileReleaseGate moving_caster{10, 53};
  auto launch_source = src;
  Require(moving_caster.TryRelease(10, 53), "moving caster release rejected");
  launch_source = live_marker;
  Require(launch_source != src && launch_source == live_marker &&
              clock.deadline_tick == 2000,
          "launch used frozen GO source or restarted GO deadline");
  // Render/loading stalls consume real clock time, not a fresh full flight.
  Require(MissileHomingFraction(0.9f, clock.RemainingSeconds(1250)) == 1.0f &&
              clock.Expired(2150), "stall must arrive at original deadline");

  // A release 250 ms after GO still has only 750 ms to catch moving targets.
  // Fractions use remaining time at each frame start, not original duration.
  float position = 0.0f;
  const auto step = [&](std::uint32_t now, float target) {
    const float fraction = MissileHomingFraction(0.25f, clock.RemainingSeconds(now));
    position += (target - position) * fraction;
    return fraction;
  };
  Require(Near(step(1250, 20.0f), 1.0f / 3.0f) && Near(position, 20.0f / 3.0f),
          "moving-target first frame used wrong remaining-time fraction");
  Require(Near(step(1500, 30.0f), 0.5f) && Near(position, 55.0f / 3.0f),
          "moving-target fraction must grow as deadline approaches");
  Require(Near(step(1750, 40.0f), 1.0f) && Near(position, 40.0f),
          "last frame must reach current moving target");
  Require(clock.deadline_tick == 2000 && !clock.Expired(1999) && clock.Expired(2000),
          "target motion or release changed fixed GO deadline");
  Require(clock.RemainingSeconds(2000) == 0.0f && clock.RemainingSeconds(2100) == 0.0f,
          "expired clock returned positive remaining time");

  const auto spatial = MissileReleaseClock::FromGo(7, src, {0.0f, 3.0f, 4.0f}, 5.0f);
  Require(spatial.deadline_tick == 1007, "travel distance must include all three axes");
  const auto rounded = MissileReleaseClock::FromGo(7, src, {1.0f, 0.0f, 0.0f}, 3.0f);
  Require(rounded.deadline_tick == 341, "fractional milliseconds must round up");
  const auto wrapped = MissileReleaseClock::FromGo(0xffffff00u, src, {1.0f, 0.0f, 0.0f}, 1.0f);
  Require(wrapped.deadline_tick == 744 && Near(wrapped.RemainingSeconds(0xffffff00u), 1.0f),
          "GO deadline did not wrap safely");
  Require(Near(wrapped.RemainingSeconds(244), 0.5f) && !wrapped.Expired(743) &&
              wrapped.Expired(744) && wrapped.Expired(745),
          "expiry failed across uint32 tick wrap");

  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  for (float speed : {0.0f, -1.0f, nan, infinity}) {
    const auto immediate = MissileReleaseClock::FromGo(123, src, {1.0f, 0.0f, 0.0f}, speed);
    Require(immediate.deadline_tick == 123 && immediate.Expired(123),
            "invalid speed must expire immediately");
  }
  Require(MissileReleaseClock::FromGo(123, src, src, 10.0f).Expired(123),
          "zero distance must expire immediately");
  Require(MissileReleaseClock::FromGo(123, src, {nan, 0.0f, 0.0f}, 10.0f).Expired(123),
          "invalid coordinates must expire immediately");
  const auto capped = MissileReleaseClock::FromGo(0, src, {1.0f, 0.0f, 0.0f},
                                                 std::numeric_limits<float>::min());
  Require(capped.deadline_tick == 0x7fffffffu && !capped.Expired(0),
          "extreme travel duration exceeded wrap-safe half range");

  Require(Near(MissileHomingFraction(0.1f, 1.0f), 0.1f) &&
              MissileHomingFraction(2.0f, 1.0f) == 1.0f &&
              MissileHomingFraction(0.1f, 0.0f) == 1.0f &&
              MissileHomingFraction(0.1f, -1.0f) == 1.0f &&
              MissileHomingFraction(0.0f, 1.0f) == 0.0f &&
              MissileHomingFraction(-1.0f, 1.0f) == 0.0f &&
              MissileHomingFraction(nan, 1.0f) == 0.0f &&
              MissileHomingFraction(0.1f, nan) == 0.0f &&
              MissileHomingFraction(infinity, 1.0f) == 1.0f &&
              MissileHomingFraction(0.1f, infinity) == 0.0f,
          "homing fraction must stay finite and clamped");
}

void CheckUnitGoTiming() {
  const std::array<float, 3> center{0.0f, 0.0f, 0.0f};
  const std::array<float, 3> victim{10.0f, 0.0f, 0.0f};
  // Production takes a separately-computed unit clock; visual attachment
  // positions can be stale/poisoned without entering those timing inputs.
  const std::array<float, 3> stale_visual_source{-10000.0f, 5000.0f, 1000.0f};
  const std::array<float, 3> stale_visual_aim{20000.0f, 9000.0f, 1000.0f};
  const auto clock = MissileReleaseClock::FromUnitGo(1000, center, victim, 10.0f);
  const auto poisoned_visual_clock = MissileReleaseClock::FromGo(1000,
      stale_visual_source, stale_visual_aim, 10.0f);
  Require(clock.deadline_tick == 2000 && poisoned_visual_clock.deadline_tick != clock.deadline_tick,
          "unit GO timing used poisoned M2 attachment coordinates");
  Require(MissileReleaseClock::FromUnitGo(1000, center, {1.0f, 0.0f, 0.0f}, 10.0f).deadline_tick == 1500 &&
              MissileReleaseClock::FromUnitGo(1000, center, center, 10.0f).deadline_tick == 1500,
          "distinct close-range unit target did not retain server minimum 5 yards");
  Require(MissileReleaseClock::FromUnitGo(1000, center, center, 10.0f, true).Expired(1000),
          "self-target incorrectly used distinct-target travel minimum");
  Require(MissileReleaseClock::FromUnitGo(7, center, victim, 3.0f).deadline_tick == 3340 &&
              MissileReleaseClock::FromGo(7, center, victim, 3.0f).deadline_tick == 3341,
          "unit server floor policy was replaced by generic ceil policy");
  Require(Near(clock.RemainingSeconds(1610), 0.39f) && clock.Expired(2000),
          "610ms release restarted authoritative GO timing");
  Require(clock.deadline_tick == 2000 &&
              MissileReleaseClock::FromUnitGo(1000, {4.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}, 10.0f).deadline_tick != clock.deadline_tick,
          "moving release positions rebased fixed GO clock");
  const auto wrapped = MissileReleaseClock::FromUnitGo(0xffffff00u, center, victim, 10.0f);
  Require(wrapped.deadline_tick == 744 && !wrapped.Expired(743) && wrapped.Expired(744),
          "unit GO clock wrap ordering failed");
  Require(MissileReleaseClock::FromUnitGo(1000, center, victim, 0.0f).Expired(1000),
          "zero-speed unit did not arrive immediately");
  Require(MissileReleaseClock::FromGo(1000, center, {1.0f, 0.0f, 0.0f}, 10.0f).deadline_tick == 1100,
          "ground/generic timing accidentally gained unit minimum distance");
}

void CheckReleaseGate() {
  // IDs from render/models/animation/animation_state.h (no renderer dependency).
  constexpr std::uint16_t cast_animation = 53; // SpellCastDirected
  constexpr std::uint16_t ready_animation = 51; // ReadySpellDirected
  constexpr std::uint16_t wound_animation = 8; // StandWound
  MissileReleaseGate pending{42, cast_animation};
  unsigned dispatches = 0;
  const auto dispatch = [&](std::uint64_t serial, std::uint16_t animation) {
    if (pending.TryRelease(serial, animation)) ++dispatches;
  };
  Require(pending.Matches(42, cast_animation) && !pending.released && dispatches == 0,
          "GO must queue without dispatching");
  dispatch(41, cast_animation); // stale cast serial
  dispatch(42, ready_animation);
  dispatch(42, wound_animation);
  dispatch(0, cast_animation);
  Require(dispatches == 0 && !pending.released,
          "unrelated Ready/wound/serial event released pending missile");
  dispatch(42, cast_animation); // spell release marker
  dispatch(42, cast_animation); // duplicate marker or later completion
  Require(dispatches == 1 && pending.released && !pending.Matches(42, cast_animation),
          "release marker and completion must dispatch exactly once");

  pending = MissileReleaseGate{43, cast_animation};
  dispatch(43, cast_animation); // completion fallback when no marker fired
  dispatch(43, cast_animation);
  Require(dispatches == 2, "completion fallback failed or dispatched twice");
  pending = MissileReleaseGate{44, 0};
  dispatch(pending.serial, pending.animation); // explicit no-animation fallback
  dispatch(44, 0);
  Require(dispatches == 3, "no-animation path failed or dispatched twice");

  pending = MissileReleaseGate{45, cast_animation};
  pending.cancelled = true;
  dispatch(45, cast_animation);
  Require(dispatches == 3 && !pending.Matches(45, cast_animation),
          "cancelled entry accepted an animation event");
  pending.Reset();
  Require(pending.serial == 0 && pending.animation == 0 &&
              !pending.released && !pending.cancelled && !pending.TryRelease(0, 0),
          "reset must clear identity and reject zero-serial events");
  dispatch(45, cast_animation);
  Require(dispatches == 3, "reset retained stale pending identity");
  pending = MissileReleaseGate{46, cast_animation};
  dispatch(45, cast_animation);
  dispatch(46, cast_animation);
  Require(dispatches == 4, "new cast after reset accepted stale serial or failed release");
}
} // namespace

int main() {
  try {
    CheckClockAndHoming();
    CheckUnitGoTiming();
    CheckReleaseGate();
    std::cout << "Missile release fixtures passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
