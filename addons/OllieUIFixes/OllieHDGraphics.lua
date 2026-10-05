-- OllieHDGraphics.lua
-- "HD Graphics": control panel for the OllieWoW graphics options. Every control only reads and
-- writes a CVar; the engine (OpenWow) does the rendering. Vanilla 1.12 Lua 5.0: no '#', no '...',
-- handlers use 'this'.

local OHD = {}
OHD.refreshing = false
OHD.controls = {}

local PRESETS = {
  { name = "Classic", values = { fogModel = "0", fogSunGlow = "1", gfxClutter = "1", frillDensity = "16",
      gfxSaturation = "1.0", gfxContrast = "1.0", gfxDither = "0" } },
  { name = "HD", values = { fogModel = "1", fogSunGlow = "1", gfxClutter = "1", frillDensity = "32",
      gfxSaturation = "1.1", gfxContrast = "1.05", gfxDither = "1" } },
  { name = "Ultra", values = { fogModel = "1", fogSunGlow = "1.2", gfxClutter = "1", frillDensity = "64",
      gfxSaturation = "1.15", gfxContrast = "1.08", gfxDither = "1" } },
}

-- Engine builds that lack these CVars make the panel show a notice instead of failing.
local function OHD_Has(cvar)
  return GetCVar(cvar) ~= nil
end

local function OHD_Num(cvar, fallback)
  local v = tonumber(GetCVar(cvar))
  if v == nil then return fallback end
  return v
end

local function OHD_Set(cvar, value)
  if OHD_Has(cvar) then SetCVar(cvar, value) end
end

local function OHD_Fmt(value, decimals)
  return string.format("%." .. decimals .. "f", value)
end

local function OHD_SliderText(slider, value)
  getglobal(slider:GetName() .. "Text"):SetText(slider.label .. ": " .. OHD_Fmt(value, slider.decimals))
end

local function OHD_Refresh()
  OHD.refreshing = true
  for i = 1, table.getn(OHD.controls) do
    local c = OHD.controls[i]
    local has = OHD_Has(c.cvar)
    if c.kind == "check" then
      local on = nil
      if has and OHD_Num(c.cvar, 0) >= 0.5 then on = 1 end
      c.frame:SetChecked(on)
      if has then c.frame:Enable() else c.frame:Disable() end
    else
      local v = OHD_Num(c.cvar, c.default)
      c.frame:SetValue(v)
      OHD_SliderText(c.frame, v)
      c.frame:EnableMouse(has)
    end
  end
  if OHD.notice then
    if OHD_Has("fogModel") then OHD.notice:Hide() else OHD.notice:Show() end
  end
  OHD.refreshing = false
end

local function OHD_MakeCheck(parent, name, label, cvar, y)
  local cb = CreateFrame("CheckButton", name, parent, "OptionsCheckButtonTemplate")
  cb:SetPoint("TOPLEFT", parent, "TOPLEFT", 24, y)
  getglobal(name .. "Text"):SetText(label)
  cb.cvar = cvar
  cb:SetScript("OnClick", function()
    if OHD.refreshing then return end
    local value = "0"
    if this:GetChecked() then value = "1" end
    OHD_Set(this.cvar, value)
  end)
  table.insert(OHD.controls, { kind = "check", cvar = cvar, frame = cb })
  return cb
end

local function OHD_MakeSlider(parent, name, label, minv, maxv, step, decimals, cvar, default, y)
  local s = CreateFrame("Slider", name, parent, "OptionsSliderTemplate")
  s:SetWidth(240)
  s:SetHeight(16)
  s:SetPoint("TOP", parent, "TOP", 0, y)
  s:SetMinMaxValues(minv, maxv)
  s:SetValueStep(step)
  getglobal(name .. "Low"):SetText(OHD_Fmt(minv, decimals))
  getglobal(name .. "High"):SetText(OHD_Fmt(maxv, decimals))
  s.cvar = cvar
  s.label = label
  s.decimals = decimals
  s:SetScript("OnValueChanged", function()
    local v = this:GetValue()
    OHD_SliderText(this, v)
    if OHD.refreshing then return end
    OHD_Set(this.cvar, OHD_Fmt(v, this.decimals))
  end)
  table.insert(OHD.controls, { kind = "slider", cvar = cvar, frame = s, default = default })
  return s
end

local function OHD_ApplyPreset(preset)
  for cvar, value in pairs(preset.values) do
    OHD_Set(cvar, value)
  end
  OHD_Refresh()
end

local function OHD_Build()
  if OHD.frame then return OHD.frame end

  local f = CreateFrame("Frame", "OllieHDGraphicsFrame", UIParent)
  f:SetWidth(320)
  f:SetHeight(516)
  f:SetFrameStrata("DIALOG")
  f:SetToplevel(true)
  f:SetMovable(true)
  f:EnableMouse(true)
  f:SetClampedToScreen(true)
  f:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 32, edgeSize = 32,
    insets = { left = 11, right = 12, top = 12, bottom = 11 },
  })
  f:RegisterForDrag("LeftButton")
  f:SetScript("OnDragStart", function() this:StartMoving() end)
  f:SetScript("OnDragStop", function() this:StopMovingOrSizing() end)
  f:SetScript("OnShow", function() OHD_Refresh() end)
  f:Hide()
  tinsert(UISpecialFrames, "OllieHDGraphicsFrame")

  local title = f:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
  title:SetPoint("TOP", f, "TOP", 0, -18)
  title:SetText("HD Graphics")

  local sub = f:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
  sub:SetPoint("TOP", title, "BOTTOM", 0, -4)
  sub:SetText("Changes apply immediately and are saved.")

  local notice = f:CreateFontString(nil, "ARTWORK", "GameFontRedSmall")
  notice:SetPoint("TOP", sub, "BOTTOM", 0, -4)
  notice:SetWidth(280)
  notice:SetText("This client build does not have the HD graphics settings yet.")
  notice:Hide()
  OHD.notice = notice

  local px = 24
  for i = 1, table.getn(PRESETS) do
    local preset = PRESETS[i]
    local b = CreateFrame("Button", "OllieHDGraphicsPreset" .. i, f, "UIPanelButtonTemplate")
    b:SetWidth(88)
    b:SetHeight(22)
    b:SetPoint("TOPLEFT", f, "TOPLEFT", px, -68)
    b:SetText(preset.name)
    b:SetScript("OnClick", function() OHD_ApplyPreset(preset) end)
    px = px + 92
  end

  local h1 = f:CreateFontString(nil, "ARTWORK", "GameFontNormal")
  h1:SetPoint("TOPLEFT", f, "TOPLEFT", 24, -108)
  h1:SetText("World")
  OHD_MakeCheck(f, "OllieHDGraphicsFog", "Modern fog", "fogModel", -124)
  OHD_MakeSlider(f, "OllieHDGraphicsSunGlow", "Sun glow in fog", 0, 2, 0.1, 1, "fogSunGlow", 1, -170)
  OHD_MakeCheck(f, "OllieHDGraphicsClutter", "Ground clutter (grass, flowers)", "gfxClutter", -196)
  OHD_MakeSlider(f, "OllieHDGraphicsDensity", "Clutter density", 1, 128, 1, 0, "frillDensity", 16, -242)

  local h2 = f:CreateFontString(nil, "ARTWORK", "GameFontNormal")
  h2:SetPoint("TOPLEFT", f, "TOPLEFT", 24, -272)
  h2:SetText("Image")
  OHD_MakeSlider(f, "OllieHDGraphicsSaturation", "Saturation", 0.5, 1.5, 0.05, 2, "gfxSaturation", 1, -308)
  OHD_MakeSlider(f, "OllieHDGraphicsContrast", "Contrast", 0.8, 1.3, 0.01, 2, "gfxContrast", 1, -358)
  OHD_MakeSlider(f, "OllieHDGraphicsDither", "Dither (less banding)", 0, 2, 0.25, 2, "gfxDither", 0, -408)

  local reset = CreateFrame("Button", "OllieHDGraphicsReset", f, "UIPanelButtonTemplate")
  reset:SetWidth(130)
  reset:SetHeight(22)
  reset:SetPoint("BOTTOMLEFT", f, "BOTTOMLEFT", 24, 22)
  reset:SetText("Reset to Classic")
  reset:SetScript("OnClick", function() OHD_ApplyPreset(PRESETS[1]) end)

  local close = CreateFrame("Button", "OllieHDGraphicsClose", f, "UIPanelButtonTemplate")
  close:SetWidth(90)
  close:SetHeight(22)
  close:SetPoint("BOTTOMRIGHT", f, "BOTTOMRIGHT", -24, 22)
  close:SetText("Close")
  close:SetScript("OnClick", function() this:GetParent():Hide() end)

  OHD.frame = f
  return f
end

function OllieHDGraphics_Toggle()
  local f = OHD_Build()
  if f:IsShown() then
    f:Hide()
    return
  end
  f:ClearAllPoints()
  if OptionsFrame and OptionsFrame:IsShown() then
    f:SetPoint("TOPLEFT", OptionsFrame, "TOPRIGHT", 4, 0)
  else
    f:SetPoint("CENTER", UIParent, "CENTER", 0, 0)
  end
  f:Show()
end

-- Local Turtle OptionsFrame renders GameOptions rows with its own templates,
-- shared label/control anchors and OPTIONS_OPTION_OFFSET. Keep this page in
-- that layout instead of placing a separate button outside the window.
local function OHD_AttachCategory()
  if OHD.category or not GameOptions or not OptionsFrame then return end

  -- Insert at the end of Video, before the next non-selectable section heading.
  -- Do not rely on translated category names or fixed category indices.
  local insertAt = nil
  for i = 1, table.getn(GameOptions) do
    if not GameOptions[i].options then
      if insertAt then break end
      insertAt = i + 1
    elseif insertAt then
      insertAt = i + 1
    end
  end
  if not insertAt then return end

  OLLIE_HD_FOG = "Modern fog"
  OLLIE_HD_SUNGLOW = "Sun glow in fog"
  OLLIE_HD_CLUTTER = "Ground clutter (grass, flowers)"
  OLLIE_HD_DENSITY = "Clutter density"
  OLLIE_HD_SATURATION = "Saturation"
  OLLIE_HD_CONTRAST = "Contrast"
  OLLIE_HD_DITHER = "Dither (less banding)"

  local category = {
    name = "HD Graphics",
    options = {
      { name = "OLLIE_HD_FOG", type = "checkbutton", cvar = "fogModel" },
      { name = "OLLIE_HD_SUNGLOW", type = "slider", cvar = "fogSunGlow",
        minval = 0, maxval = 2, step = 0.1, numberLabels = true },
      { name = "OLLIE_HD_CLUTTER", type = "checkbutton", cvar = "gfxClutter" },
      { name = "OLLIE_HD_DENSITY", type = "slider", cvar = "frillDensity",
        minval = 1, maxval = 128, step = 1, numberLabels = true },
      { name = "OLLIE_HD_SATURATION", type = "slider", cvar = "gfxSaturation",
        minval = 0.5, maxval = 1.5, step = 0.05, numberLabels = true },
      { name = "OLLIE_HD_CONTRAST", type = "slider", cvar = "gfxContrast",
        minval = 0.8, maxval = 1.3, step = 0.01, numberLabels = true },
      { name = "OLLIE_HD_DITHER", type = "slider", cvar = "gfxDither",
        minval = 0, maxval = 2, step = 0.25, numberLabels = true },
    },
  }
  table.insert(GameOptions, insertAt, category)
  OHD.category = category
  if OptionsFrame_UpdateCategories then OptionsFrame_UpdateCategories() end
end

SLASH_OLLIEHDGRAPHICS1 = "/hd"
SLASH_OLLIEHDGRAPHICS2 = "/hdgraphics"
SlashCmdList["OLLIEHDGRAPHICS"] = function() OllieHDGraphics_Toggle() end

pcall(OHD_AttachCategory)

local ev = CreateFrame("Frame")
ev:RegisterEvent("VARIABLES_LOADED")
ev:RegisterEvent("PLAYER_ENTERING_WORLD")
ev:SetScript("OnEvent", function() pcall(OHD_AttachCategory) end)