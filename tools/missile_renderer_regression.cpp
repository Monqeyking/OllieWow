// Real production SpellVisualRenderer lifecycle fixture, no assets/GPU/server.
// Parent builds from existing build directory with production renderer/M2/game
// libraries and their normal transitive dependencies; include ../src, ../include.
// No test-only production seams. Short sleeps advance GameClock's raw steady
// millisecond clock; fake huge Update(dt) is intentionally NOT a clock advance.
#include "openwow/render/effects/spell_visuals/spell_visual_renderer.h"
#include "openwow/runtime/time/game_clock.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace openwow;
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
struct Harness {
  render::m2::M2System m2;
  game::ObjectPresentationSnapshot objects;
  render::SpellVisualRenderer renderer;
  std::vector<render::SpellVisualDeferredImpactCommand> impacts;
  game::ObjectHandle caster{game::ObjectGuid{1}, 11};
  game::ObjectHandle target{game::ObjectGuid{2}, 22};
  Harness() {
    game::ObjectPresentationRecord source;
    source.handle = caster; source.health = 100; source.x = 4.0f;
    game::ObjectPresentationRecord victim;
    victim.handle = target; victim.health = 100; victim.x = 10.0f;
    objects.active = {source, victim};
    Require(renderer.Initialize(&m2, &objects), "real renderer init failed");
    renderer.BindDeferredImpactSink([this](const auto& impact) { impacts.push_back(impact); });
  }
  game::SpellVisualPresentationEvent Event(bool expired, unsigned sequence) {
    const auto now = core::GameClock::GetTickCount32();
    game::SpellVisualPresentationEvent event;
    event.owner = caster;
    event.owner_position = {4.0f, 0.0f, 0.0f};
    event.spell_id = 3110; event.spell_visual_id = 1;
    event.missile.emplace(); // Empty path deliberately exercises invisible flight.
    event.missile->target_attachment_id = -1;
    event.missile_caster_guid = caster.guid.GetRawValue();
    event.missile_cast_count = static_cast<std::uint8_t>(sequence);
    event.missile_target_guid = target.guid.GetRawValue();
    event.missile_target_handle = target;
    event.missile_source_position = {4.0f, 0.0f, 0.0f}; // live release, not GO origin
    event.missile_target_position = {10.0f, 0.0f, 1.0f};
    event.missile_target_fallback_offset = {0.0f, 0.0f, 1.0f};
    event.missile_speed = 10.0f;
    event.missile_go_tick = now - 100;
    event.missile_queue_tick = now - 100;
    event.missile_release_tick = now;
    event.missile_deadline_tick = expired ? now - 1 : now + 30;
    event.missile_has_deadline = true;
    event.deferred_impact_kit_id = 77;
    return event;
  }
  void PastDeadline() {
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    renderer.Update(0.0f); // real elapsed clock, independent of forwarded dt
  }
};
void CheckExpiredAndReplay() {
  Harness h;
  auto event = h.Event(true, 1);
  h.objects.active[1].x = 25.0f; // moved while parked: GO aim must not freeze it
  h.renderer.ConsumePresentationEvent(event);
  Require(h.impacts.size() == 1 && h.impacts.back().target == h.target &&
              h.impacts.back().world_position[0] == 25.0f &&
              h.impacts.back().world_position[2] == 1.0f,
          "expired release did not arrive at live target + retained GO offset");
  h.renderer.ConsumePresentationEvent(event);
  h.renderer.Update(100.0f);
  Require(h.impacts.size() == 1, "duplicate consume emitted duplicate impact");
  h.renderer.Clear();
  h.renderer.ConsumePresentationEvent(event);
  Require(h.impacts.size() == 2, "reset did not clear consumed receipt lifecycle");
}
void CheckInvisibleDeadlineAndGenerations() {
  Harness h;
  const auto event = h.Event(false, 2);
  h.renderer.ConsumePresentationEvent(event);
  Require(h.impacts.empty(), "missing visual model impacted before GO deadline");
  h.renderer.DestroyEffectsForObject({h.caster.guid, h.caster.generation + 1});
  h.objects.active[1].x = 35.0f;
  h.PastDeadline();
  Require(h.impacts.size() == 1 && h.impacts.back().world_position[0] == 35.0f,
          "wrong owner generation cancelled flight or homing ignored target motion");
  h.renderer.ConsumePresentationEvent(h.Event(false, 3));
  h.renderer.DestroyEffectsForObject(h.caster);
  h.PastDeadline();
  Require(h.impacts.size() == 1, "invisible flight survived caster unload");
  h.renderer.ConsumePresentationEvent(h.Event(false, 4));
  h.renderer.DestroyEffectsForObject(h.target);
  h.objects.active[1].handle.generation += 1;
  h.PastDeadline();
  Require(h.impacts.size() == 1, "removed target impacted a reincarnated GUID");
}
void CheckExpiredReflection() {
  Harness h;
  auto event = h.Event(true, 5);
  event.missile_impact_result = 11; // Vanilla REFLECT
  event.missile_reflect_result = 0; // return lands on caster
  h.renderer.ConsumePresentationEvent(event);
  Require(h.impacts.empty(), "expired reflection impacted reflector immediately");
  h.renderer.Update(0.0f); // outbound expiry -> existing reflection transition
  Require(h.impacts.empty(), "reflection dispatched caster impact kit on outbound leg");
  h.renderer.Update(0.0f); // return first frame follows existing first-update convention
  h.renderer.Update(2.0f); // return leg has existing independent speed-based clock
  Require(h.impacts.size() == 1 && h.impacts.back().target == h.caster &&
              h.impacts.back().kit_id == 77,
          "expired outbound lost return leg or wrong impact owner");
  h.renderer.Update(2.0f);
  Require(h.impacts.size() == 1, "reflected arrival duplicated impact");
  Harness miss;
  event = miss.Event(true, 6);
  event.missile_impact_result = 11;
  event.missile_reflect_result = 1; // miss on return suppresses impact kit
  miss.renderer.ConsumePresentationEvent(event);
  miss.renderer.Update(0.0f); miss.renderer.Update(0.0f); miss.renderer.Update(2.0f);
  Require(miss.impacts.empty(), "reflect-result miss ignored by expired return");
}
}
int main() {
  try {
    CheckExpiredAndReplay();
    CheckInvisibleDeadlineAndGenerations();
    CheckExpiredReflection();
    std::cout << "missile renderer lifecycle regression passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
