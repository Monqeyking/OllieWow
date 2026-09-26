
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "openwow/game/object_guid.h"
#include "openwow/game/packet_reader.h"
#include "openwow/game/vec3.h"

namespace openwow::game {

enum class MonsterMoveType : std::uint8_t {
  kNormal       = 0,
  kStop         = 1,
  kFacingSpot   = 2,
  kFacingTarget = 3,
  kFacingAngle  = 4,
};

namespace SplineFlag {
  // 1.12.1 wire values. Authority:
  // D:\OllieWoW\Source\src\game\Movement\spline\MoveSplineFlag.h:38-78.
  //
  // The previous table carried post-Classic (WotLK) bit positions. Two
  // consequences were live: IsCyclic() tested 0x00080000 where 1.12 puts Cyclic
  // at 0x00100000, so a cyclic spline was never recognised as cyclic and never
  // ran EnterCyclicLoop()'s erase-first-vertex step; and HasTriggeredAnimationTier()
  // tested 0x00200000, which the 1.12 server sets as Enter_Cycle on every cyclic
  // spline (packet_builder.cpp:71-72), so cyclic splines were treated as carrying
  // an animation tier.
  inline constexpr std::uint32_t kDone        = 0x00000001;
  inline constexpr std::uint32_t kFalling     = 0x00000002;
  inline constexpr std::uint32_t kRunmode     = 0x00000100;
  inline constexpr std::uint32_t kFlying      = 0x00000200;
  inline constexpr std::uint32_t kNoSpline    = 0x00000400;
  inline constexpr std::uint32_t kFinalPoint  = 0x00010000;
  inline constexpr std::uint32_t kFinalTarget = 0x00020000;
  inline constexpr std::uint32_t kFinalAngle  = 0x00040000;
  inline constexpr std::uint32_t kCyclic      = 0x00100000;
  inline constexpr std::uint32_t kEnterCycle  = 0x00200000;
  inline constexpr std::uint32_t kFrozen      = 0x00400000;

  inline constexpr std::uint32_t kMaskFinalFacing =
      kFinalPoint | kFinalTarget | kFinalAngle;
  // 1.12's Mask_CatmullRom IS the Flying bit (MoveSplineFlag.h:77).
  inline constexpr std::uint32_t kMaskCatmullRom = kFlying;
  inline constexpr std::uint32_t kCatmullRom     = kMaskCatmullRom;
  inline constexpr std::uint32_t kFreeze         = kFrozen;

  // 1.12 has no parabolic, walk-mode, orient-fixed, transport-enter/exit or
  // backward spline flag. kParabolic keeps its old bit only so the parabolic read
  // stays inert (the 1.12 server never sets 0x00000800); backward movement is a
  // MovementInfo flag on this wire, not a spline flag.
  inline constexpr std::uint32_t kParabolic = 0x00000800;

  // 0x00004000 is a real 1.12 bit that the server enum leaves unnamed
  // (MoveSplineFlag.h:53, Unknown15); the client reads it as "orientation is
  // fixed", which is how it uses it in the no-auto-rotate mask.
  inline constexpr std::uint32_t kOrientFixed = 0x00004000;

  // Legacy name for the No_Spline gate that keeps spline-derived state out of
  // speed and flag queries.
  inline constexpr std::uint32_t kStateQueryExempt = kNoSpline;

  [[nodiscard]] inline bool HasNonExemptFlag(const std::uint32_t spline_flags,
                                             const std::uint32_t queried_flag) {
    return (spline_flags & kStateQueryExempt) == 0 &&
           (spline_flags & queried_flag) != 0;
  }
}

struct MonsterMoveInfo {
  ObjectGuid mover{ObjectGuid(0)};
  bool has_transport = false;
  ObjectGuid transport{ObjectGuid(0)};
  std::int8_t transport_seat = 0;

  std::uint8_t unk_byte = 0;

  std::size_t transition_payload_offset = 0;
  Vec3 position{};
  std::uint32_t spline_id = 0;

  MonsterMoveType move_type = MonsterMoveType::kNormal;

  Vec3 facing_spot{};
  std::uint64_t facing_target_guid = 0;
  float facing_angle = 0.0f;

  std::uint32_t spline_flags = 0;

  std::uint32_t duration = 0;

  float vertical_acceleration = 0.0f;
  std::uint32_t parabolic_start_time = 0;

  std::vector<Vec3> waypoints;

  bool catmull_rom = false;
};

class MonsterMoveManager {
 public:

  bool HandleMonsterMove(const std::uint8_t* data, std::size_t len);
  bool HandleMonsterMoveTransport(const std::uint8_t* data, std::size_t len);

  [[nodiscard]] const MonsterMoveInfo& last_move() const { return last_; }
  [[nodiscard]] std::size_t total_move_count() const { return total_count_; }

  void Clear();

 private:
  bool ParseMonsterMove(PacketReader& r, MonsterMoveInfo& out, bool transport);
  bool ParseWaypoints(PacketReader& r, MonsterMoveInfo& out);

  MonsterMoveInfo last_{};
  std::size_t total_count_ = 0;
};

}
