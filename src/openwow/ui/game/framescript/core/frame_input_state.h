#pragma once

struct lua_State;

namespace openwow::ui::game::frame_api {

void NotifyFrameInputMutation(lua_State* lua, int self_index, bool reindex_only);
void NotifyFramePaintOrderMutation(lua_State* lua, int self_index);
// Relink without re-querying visibility.  The reference relinks a frame at its
// strata/level bucket on three events - becoming visible, a strata change and a
// changing level set - and the link stamp is the tie-break inside a bucket
// (later link draws on top).  Callers of those events already know the frame is
// visible (or that its bucket key changed), so a stale visibility snapshot must
// not be able to suppress the relink.
void ForceFramePaintTailMutation(lua_State* lua, int self_index);
[[nodiscard]] bool IsLuaTableEffectivelyVisible(lua_State* lua,
                                                int table_index);
bool GetLuaBooleanField(lua_State* lua, int table_index, const char* field);
int SetValidatedFrameShownState(lua_State* lua, bool shown);
int PushValidatedFrameShownState(lua_State* lua);
int SetFrameInputCategoryEnabled(lua_State* lua, const char* field_name);
int PushFrameInputCategoryEnabled(lua_State* lua, const char* field_name,
                                  const char* legacy_field_name = nullptr);

}
