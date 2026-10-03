-- Run only inside an authorized in-world native Lua runtime, not standalone Lua.
-- Checks the actual GetZonePVPInfo ABI used by local ZoneText.xml.
local function check(...)
  assert(arg.n == 3, "GetZonePVPInfo must return three slots")
  local pvpType, factionName, isArena = arg[1], arg[2], arg[3]
  assert(pvpType == nil or pvpType == "friendly" or pvpType == "hostile" or
         pvpType == "contested", "non-Vanilla territory type")
  assert(factionName == nil or type(factionName) == "string",
         "factionName must occupy return position 2")
  if pvpType == "friendly" or pvpType == "hostile" then
    assert(type(factionName) == "string" and factionName ~= "",
           "owned territory needs its DBC faction name")
  end
  assert(isArena == nil or isArena == false or isArena == true or isArena == 1,
         "isArena must be a Vanilla boolean, not factionName")
end
check(GetZonePVPInfo())
