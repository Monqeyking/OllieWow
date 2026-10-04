-- OllieHDGraphics.lua
-- "HD Graphics": control panel for the OllieWoW graphics options. Every control only reads and
-- writes a CVar; the engine (OpenWow) does the rendering. Vanilla 1.12 Lua 5.0: no '#', no '...',
-- handlers use 'this'.

local OHD = {}
OHD.refreshing = false
OHD.controls = {}

local PRESETS = {
  { name = "Classic", values = { fogModel = "0", gfxClutter = "1", frillDensity = "16",
      gfxSaturation = "1.0", gfxContrast = "1.0", gfxDither = "0" } },
  { name = "HD", values = { fogModel = "1", gfxClutter = "1", frillDensity = "32",
      gfxSaturation = "1.1", gfxContrast = "1.05", gfxDither = "1" } },
  { name = "Ultra", values = { fogModel = "1", gfxClutter = "1", frillDensity = "64",
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
  f:SetHeight(470)
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
  OHD_MakeCheck(f, "OllieHDGraphicsClutter", "Ground clutter (grass, flowers)", "gfxClutter", -150)
  OHD_MakeSlider(f, "OllieHDGraphicsDensity", "Clutter density", 1, 128, 1, 0, "frillDensity", 16, -196)

  local h2 = f:CreateFontString(nil, "ARTWORK", "GameFontNormal")
  h2:SetPoint("TOPLEFT", f, "TOPLEFT", 24, -226)
  h2:SetText("Image")
  OHD_MakeSlider(f, "OllieHDGraphicsSaturation", "Saturation", 0.5, 1.5, 0.05, 2, "gfxSaturation", 1, -262)
  OHD_MakeSlider(f, "OllieHDGraphicsContrast", "Contrast", 0.8, 1.3, 0.01, 2, "gfxContrast", 1, -312)
  OHD_MakeSlider(f, "OllieHDGraphicsDither", "Dither (less banding)", 0, 2, 0.25, 2, "gfxDither", 0, -362)

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

-- A button on the stock Video Options window. It is parented to OptionsFrame, so it shows and
-- hides together with it.
local function OHD_AttachButton()
  if OHD.button or not OptionsFrame then return end
  local b = CreateFrame("Button", "OllieHDGraphicsButton", OptionsFrame, "UIPanelButtonTemplate")
  b:SetWidth(130)
  b:SetHeight(22)
  b:SetPoint("TOPRIGHT", OptionsFrame, "BOTTOMRIGHT", 0, 2)
  b:SetText("HD Graphics")
  b:SetScript("OnClick", function() OllieHDGraphics_Toggle() end)
  OHD.button = b
end

SLASH_OLLIEHDGRAPHICS1 = "/hd"
SLASH_OLLIEHDGRAPHICS2 = "/hdgraphics"
SlashCmdList["OLLIEHDGRAPHICS"] = function() OllieHDGraphics_Toggle() end

pcall(OHD_AttachButton)

local ev = CreateFrame("Frame")
ev:RegisterEvent("PLAYER_ENTERING_WORLD")
ev:SetScript("OnEvent", function() pcall(OHD_AttachButton) end)