
#include "openwow/platform/window/window_manager.h"

#include <SDL2/SDL.h>
#include "openwow/platform/window/sdl_syswm.h"

#include "openwow/foundation/diagnostics/logging.h"

#include <algorithm>
#include <cmath>

#if defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace openwow::platform {

namespace {

void ApplyMinimumClientSize(SDL_Window* window,
                            const uint32_t min_width,
                            const uint32_t min_height) {
    if (window == nullptr) {
        return;
    }

    SDL_SetWindowMinimumSize(window,
                             static_cast<int>(min_width),
                             static_cast<int>(min_height));
}

const char* WindowModeName(const WindowMode mode) {
    switch (mode) {
        case WindowMode::Fullscreen: return "fullscreen";
        case WindowMode::WindowedFullscreen: return "fullscreen-desktop";
        case WindowMode::Windowed: return "windowed";
    }
    return "unknown";
}

void LogWindowState(const char* stage, SDL_Window* window, const WindowMode requested,
                    const int bordered_requested) {
    if (window == nullptr) {
        return;
    }

    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;
    SDL_GetWindowSize(window, &width, &height);
    SDL_GetWindowPosition(window, &x, &y);
    const Uint32 flags = SDL_GetWindowFlags(window);
    openwow::diagnostics::Log(
        openwow::diagnostics::LogLevel::kWarn,
        std::string("WindowManager: ") + stage +
            " requested=" + WindowModeName(requested) +
            " flags=0x" + std::to_string(flags) +
            " fullscreen=" + ((flags & SDL_WINDOW_FULLSCREEN) != 0 ? "1" : "0") +
            " fullscreen_desktop=" +
                ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0 ? "1" : "0") +
            " borderless=" + ((flags & SDL_WINDOW_BORDERLESS) != 0 ? "1" : "0") +
            " maximized=" + ((flags & SDL_WINDOW_MAXIMIZED) != 0 ? "1" : "0") +
            " bordered_requested=" + std::to_string(bordered_requested) +
            " size=" + std::to_string(width) + "x" + std::to_string(height) +
            " pos=" + std::to_string(x) + "," + std::to_string(y));
}

}

WindowManager& WindowManager::Get() {
    static WindowManager instance;
    return instance;
}

bool WindowManager::Initialize(const WindowConfig& config) {
    if (initialized_) {
        return true;
    }

    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            openwow::diagnostics::Log(openwow::diagnostics::LogLevel::kError,
                               std::string("WindowManager: SDL_Init(VIDEO) failed: ") + SDL_GetError());
            return false;
        }
    }

    uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;

    if (config.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    switch (config.mode) {
        case WindowMode::Fullscreen:
            flags |= SDL_WINDOW_FULLSCREEN;
            break;
        case WindowMode::WindowedFullscreen:
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            break;
        case WindowMode::Windowed:
        default:
            break;
    }

    int pos_x = (config.x < 0) ? SDL_WINDOWPOS_CENTERED : config.x;
    int pos_y = (config.y < 0) ? SDL_WINDOWPOS_CENTERED : config.y;

    SDL_Window* sdl_window = SDL_CreateWindow(
        config.title.c_str(),
        pos_x, pos_y,
        static_cast<int>(config.width),
        static_cast<int>(config.height),
        flags);

    if (!sdl_window) {
        openwow::diagnostics::Log(openwow::diagnostics::LogLevel::kError,
                           std::string("WindowManager: SDL_CreateWindow failed: ") + SDL_GetError());
        return false;
    }

    min_width_ = config.min_width;
    min_height_ = config.min_height;
    ApplyMinimumClientSize(sdl_window, min_width_, min_height_);

    window_      = sdl_window;
    width_       = config.width;
    height_      = config.height;
    mode_        = config.mode;
    should_close_= false;
    minimized_   = false;
    focused_     = true;
    gamma_       = 1.0f;
    title_       = config.title;
    initialized_ = true;
    owns_window_ = true;
    title_was_set_explicitly_ = true;
    mouse_button_capture_mask_ = 0;
    EndRelativeCursorMode();
    pending_windowed_size_.reset();
    pending_windowed_retried_ = false;

    return true;
}

void WindowManager::AdoptExternalWindow(void* sdl_window) {
    window_ = sdl_window;
    owns_window_ = false;
    should_close_ = false;
    minimized_ = false;
    focused_ = true;
    initialized_ = sdl_window != nullptr;
    mouse_button_capture_mask_ = 0;
    cursor_anchor_.reset();
    EndRelativeCursorMode();
    pending_windowed_size_.reset();
    pending_windowed_retried_ = false;

    if (!initialized_) {
        width_ = 0;
        height_ = 0;
        mode_ = WindowMode::Windowed;
        return;
    }

    auto* sdl_window_ptr = static_cast<SDL_Window*>(window_);
    ApplyMinimumClientSize(sdl_window_ptr, min_width_, min_height_);
    if (title_was_set_explicitly_) {
        SDL_SetWindowTitle(sdl_window_ptr, title_.c_str());
    } else if (const char* title = SDL_GetWindowTitle(sdl_window_ptr);
               title != nullptr && title[0] != '\0') {
        title_ = title;
    } else {
        title_ = "World of Warcraft";
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(sdl_window_ptr, &width, &height);
    width_ = static_cast<uint32_t>(std::max(width, 0));
    height_ = static_cast<uint32_t>(std::max(height, 0));

    const Uint32 flags = SDL_GetWindowFlags(sdl_window_ptr);
    minimized_ = (flags & SDL_WINDOW_MINIMIZED) != 0;
    focused_ = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
    if ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0) {
        mode_ = WindowMode::WindowedFullscreen;
    } else if ((flags & SDL_WINDOW_FULLSCREEN) != 0) {
        mode_ = WindowMode::Fullscreen;
    } else {
        mode_ = WindowMode::Windowed;
    }
}

void WindowManager::Shutdown() {
    ResetMouseButtonCapture();

    EndRelativeCursorMode();

    if (window_) {
        if (owns_window_) {
            SDL_DestroyWindow(static_cast<SDL_Window*>(window_));
        }
        window_ = nullptr;
    }
    width_       = 0;
    height_      = 0;
    min_width_   = kRetailMinimumClientWidth;
    min_height_  = kRetailMinimumClientHeight;
    initialized_ = false;
    owns_window_ = false;
    should_close_= false;
    minimized_   = false;
    focused_     = true;
    gamma_       = 1.0f;
    title_       = "World of Warcraft";
    title_was_set_explicitly_ = false;
    resize_cb_   = nullptr;
    focus_cb_    = nullptr;
    close_cb_    = nullptr;
    mouse_button_capture_mask_ = 0;
    cursor_anchor_.reset();
    pending_windowed_size_.reset();
    pending_windowed_retried_ = false;
}

bool WindowManager::IsInitialized() const {
    return initialized_;
}

uint32_t WindowManager::GetWidth() const  { return width_; }
uint32_t WindowManager::GetHeight() const { return height_; }

float WindowManager::GetAspectRatio() const {
    if (height_ == 0) return 0.0f;
    return static_cast<float>(width_) / static_cast<float>(height_);
}

bool WindowManager::IsFullscreen() const {
    return mode_ == WindowMode::Fullscreen || mode_ == WindowMode::WindowedFullscreen;
}

bool WindowManager::IsMinimized() const { return minimized_; }
bool WindowManager::IsFocused() const   { return focused_; }

void WindowManager::SetTitle(const std::string& title) {
    title_ = title;
    title_was_set_explicitly_ = true;
    if (initialized_ && window_ != nullptr) {
        SDL_SetWindowTitle(static_cast<SDL_Window*>(window_), title_.c_str());
    }
}

std::string WindowManager::GetTitle() const {
    return title_;
}

void WindowManager::SetWindowMode(WindowMode mode) {
    if (!initialized_ || mode == mode_) return;

    auto* sdl_win = static_cast<SDL_Window*>(window_);
    uint32_t flag = 0;

    switch (mode) {
        case WindowMode::Fullscreen:
            flag = SDL_WINDOW_FULLSCREEN;
            break;
        case WindowMode::WindowedFullscreen:
            flag = SDL_WINDOW_FULLSCREEN_DESKTOP;
            break;
        case WindowMode::Windowed:
        default:
            flag = 0;
            break;
    }

    SDL_SetWindowFullscreen(sdl_win, flag);
    mode_ = mode;

    int w = 0, h = 0;
    SDL_GetWindowSize(sdl_win, &w, &h);
    if (w > 0 && h > 0) {
        width_  = static_cast<uint32_t>(w);
        height_ = static_cast<uint32_t>(h);
    }
}

void WindowManager::SetResolution(uint32_t width, uint32_t height) {
    if (!initialized_) return;

    auto* sdl_win = static_cast<SDL_Window*>(window_);
    if (mode_ == WindowMode::Fullscreen) {
        const int display_index = SDL_GetWindowDisplayIndex(sdl_win);
        SDL_DisplayMode requested{};
        requested.w = static_cast<int>(width);
        requested.h = static_cast<int>(height);
        SDL_DisplayMode closest{};
        if (display_index >= 0 &&
            SDL_GetClosestDisplayMode(display_index, &requested, &closest) != nullptr) {
            (void)SDL_SetWindowDisplayMode(sdl_win, &closest);
            (void)SDL_SetWindowFullscreen(sdl_win, SDL_WINDOW_FULLSCREEN);
        }
        return;
    }
    if (mode_ == WindowMode::WindowedFullscreen) return;

    int logical_width = 0;
    int logical_height = 0;
    int drawable_width = 0;
    int drawable_height = 0;
    SDL_GetWindowSize(sdl_win, &logical_width, &logical_height);
    SDL_GL_GetDrawableSize(sdl_win, &drawable_width, &drawable_height);
    const double scale_x = logical_width > 0 && drawable_width > 0
                               ? static_cast<double>(drawable_width) / logical_width
                               : 1.0;
    const double scale_y = logical_height > 0 && drawable_height > 0
                               ? static_cast<double>(drawable_height) / logical_height
                               : 1.0;
    const auto requested_logical_width = static_cast<uint32_t>(std::max(
        1.0, std::round(static_cast<double>(width) / scale_x)));
    const auto requested_logical_height = static_cast<uint32_t>(std::max(
        1.0, std::round(static_cast<double>(height) / scale_y)));
    if (requested_logical_width == width_ && requested_logical_height == height_) return;

    SDL_SetWindowSize(sdl_win,
                      static_cast<int>(requested_logical_width),
                      static_cast<int>(requested_logical_height));
    int actual_width = 0;
    int actual_height = 0;
    SDL_GetWindowSize(sdl_win, &actual_width, &actual_height);
    if (actual_width == static_cast<int>(requested_logical_width) &&
        actual_height == static_cast<int>(requested_logical_height)) {
        pending_windowed_size_.reset();
        pending_windowed_retried_ = false;
        SetClientSize(requested_logical_width, requested_logical_height);
    } else {

        pending_windowed_size_ =
            std::pair{requested_logical_width, requested_logical_height};
        pending_windowed_retried_ = false;
    }
}

bool WindowManager::ApplyDisplayMode(const DisplayModeRequest& request) {
    const auto mode = request.mode;
    const auto pixel_width = request.pixel_width;
    const auto pixel_height = request.pixel_height;
    if (!initialized_ || window_ == nullptr || pixel_width == 0 || pixel_height == 0) {
        return false;
    }

    auto* sdl_window = static_cast<SDL_Window*>(window_);
    if (mode == WindowMode::Fullscreen) {
        const int display_index = SDL_GetWindowDisplayIndex(sdl_window);
        SDL_DisplayMode requested{};
        requested.w = static_cast<int>(pixel_width);
        requested.h = static_cast<int>(pixel_height);
        requested.refresh_rate = static_cast<int>(request.refresh_rate);
        SDL_DisplayMode closest{};
        if (display_index < 0 ||
            SDL_GetClosestDisplayMode(display_index, &requested, &closest) == nullptr ||
            SDL_SetWindowDisplayMode(sdl_window, &closest) != 0 ||
            SDL_SetWindowFullscreen(sdl_window, SDL_WINDOW_FULLSCREEN) != 0) {
            return false;
        }
    } else if (mode != mode_) {
        const uint32_t fullscreen_flag =
            mode == WindowMode::WindowedFullscreen
                ? SDL_WINDOW_FULLSCREEN_DESKTOP
                : 0;
        if (SDL_SetWindowFullscreen(sdl_window, fullscreen_flag) != 0) {
            return false;
        }
    }

    mode_ = mode;
    int bordered_requested = 0;
    if (mode == WindowMode::Windowed) {
        SDL_SetWindowBordered(sdl_window, SDL_TRUE);
        bordered_requested = 1;
        if (request.maximize) {
            SDL_MaximizeWindow(sdl_window);
        } else {
            SDL_RestoreWindow(sdl_window);

            uint32_t windowed_width = pixel_width;
            uint32_t windowed_height = pixel_height;
            const int display_index = SDL_GetWindowDisplayIndex(sdl_window);
            SDL_Rect usable_bounds{};
            int border_left = 0;
            int border_right = 0;
            int border_top = 0;
            int border_bottom = 0;
            if (display_index >= 0 &&
                SDL_GetDisplayUsableBounds(display_index, &usable_bounds) == 0 &&
                SDL_GetWindowBordersSize(sdl_window, &border_top, &border_left,
                                         &border_bottom, &border_right) == 0) {
                const int max_client_width = usable_bounds.w - border_left - border_right;
                const int max_client_height = usable_bounds.h - border_top - border_bottom;
                if (max_client_width > 0) {
                    windowed_width = std::min(windowed_width,
                                              static_cast<uint32_t>(max_client_width));
                }
                if (max_client_height > 0) {
                    windowed_height = std::min(windowed_height,
                                               static_cast<uint32_t>(max_client_height));
                }
                SDL_SetWindowPosition(sdl_window, usable_bounds.x + border_left,
                                       usable_bounds.y + border_top);
            }
            SetResolution(windowed_width, windowed_height);
        }
    } else {
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(sdl_window, &width, &height);
        if (width > 0 && height > 0) {
            SetClientSize(static_cast<uint32_t>(width),
                          static_cast<uint32_t>(height));
        }
    }
    LogWindowState("ApplyDisplayMode", sdl_window, mode, bordered_requested);
    return true;
}

void WindowManager::SetClientSize(const uint32_t width, const uint32_t height) {
    if (mode_ == WindowMode::Windowed && pending_windowed_size_.has_value()) {
        const auto requested = *pending_windowed_size_;
        if (width != requested.first || height != requested.second) {

            if (pending_windowed_retried_) {
                pending_windowed_size_.reset();
                pending_windowed_retried_ = false;
            } else {
                auto* sdl_window = static_cast<SDL_Window*>(window_);
                pending_windowed_retried_ = true;
                SDL_SetWindowSize(sdl_window,
                                  static_cast<int>(requested.first),
                                  static_cast<int>(requested.second));
                int actual_width = 0;
                int actual_height = 0;
                SDL_GetWindowSize(sdl_window, &actual_width, &actual_height);
                if (actual_width == static_cast<int>(requested.first) &&
                    actual_height == static_cast<int>(requested.second)) {
                    pending_windowed_size_.reset();
                    pending_windowed_retried_ = false;
                    SetClientSize(requested.first, requested.second);
                }
                return;
            }
        } else {
            pending_windowed_size_.reset();
            pending_windowed_retried_ = false;
        }
    }

    width_ = width;
    height_ = height;

    if (resize_cb_) {
        resize_cb_(width_, height_);
    }
}

void WindowManager::ToggleFullscreen() {
    if (IsFullscreen()) {
        SetWindowMode(WindowMode::Windowed);
    } else {
        SetWindowMode(WindowMode::WindowedFullscreen);
    }
}

void WindowManager::SetGamma(float gamma) {
    gamma_ = gamma;
    if (initialized_) {
        SDL_SetWindowBrightness(static_cast<SDL_Window*>(window_), gamma);
    }
}

float WindowManager::GetGamma() const { return gamma_; }

void WindowManager::SaveCursorAnchorIfUnlocked(const int x, const int y) {
    if (relative_cursor_mode_active_) {
        return;
    }

    cursor_anchor_ = std::pair<int, int>{x, y};
}

void WindowManager::SetCursorPosition(int x, int y) {
    SaveCursorAnchorIfUnlocked(x, y);
    if (cursor_position_override_.has_value()) {
        cursor_position_override_ = std::pair<int, int>{x, y};
    }
    if (window_) {
        SDL_WarpMouseInWindow(static_cast<SDL_Window*>(window_), x, y);
    }
}

std::optional<std::pair<int, int>> WindowManager::GetCursorPositionInWindow() {
    if (cursor_position_override_.has_value()) {
        SaveCursorAnchorIfUnlocked(cursor_position_override_->first,
                                   cursor_position_override_->second);
        return cursor_position_override_;
    }
    if (!window_) {
        return std::nullopt;
    }

    int x = 0;
    int y = 0;
    SDL_GetMouseState(&x, &y);
    SaveCursorAnchorIfUnlocked(x, y);
    return std::pair<int, int>{x, y};
}

std::optional<std::pair<int, int>> WindowManager::ResolveLogicalCursorPosition() {
    if (relative_cursor_mode_active_ && cursor_anchor_.has_value()) {
        return cursor_anchor_;
    }
    return GetCursorPositionInWindow();
}

std::optional<std::pair<int, int>>
WindowManager::ResolveLogicalCursorPositionInDrawablePixels() {
    const auto cursor = ResolveLogicalCursorPosition();
    if (!cursor.has_value()) {
        return std::nullopt;
    }
    auto* const sdl_win = static_cast<SDL_Window*>(window_);
    if (sdl_win == nullptr) {
        return cursor;
    }
    int logical_width = 0;
    int logical_height = 0;
    int drawable_width = 0;
    int drawable_height = 0;
    SDL_GetWindowSize(sdl_win, &logical_width, &logical_height);
    SDL_GL_GetDrawableSize(sdl_win, &drawable_width, &drawable_height);
    if (logical_width <= 0 || logical_height <= 0) {
        return cursor;
    }
    return std::pair<int, int>{cursor->first * drawable_width / logical_width,
                               cursor->second * drawable_height / logical_height};
}

std::pair<int, int> WindowManager::ResolveMouseButtonDispatchPosition(const int raw_x,
                                                                      const int raw_y,
                                                                      const bool relative_mode_active) const {
    if (relative_mode_active && cursor_anchor_.has_value()) {
        return *cursor_anchor_;
    }

    return {raw_x, raw_y};
}

bool WindowManager::BeginRelativeCursorMode() {
    if (relative_cursor_mode_active_) {
        return true;
    }

#if defined(__APPLE__)

    if (window_ != nullptr) {
        (void)CGAssociateMouseAndMouseCursorPosition(false);

        int32_t discard_x = 0;
        int32_t discard_y = 0;
        CGGetLastMouseDelta(&discard_x, &discard_y);
    }
#endif

    relative_cursor_mode_active_ = true;
    return true;
}

void WindowManager::EndRelativeCursorMode() {
    if (!relative_cursor_mode_active_) {
        return;
    }

#if defined(__APPLE__)

    (void)CGAssociateMouseAndMouseCursorPosition(true);
#endif

    relative_cursor_mode_active_ = false;
}

bool WindowManager::IsRelativeCursorModeActive() const {
    return relative_cursor_mode_active_;
}

RelativeCursorMotion WindowManager::HandleRelativeCursorMotion() {
    RelativeCursorMotion motion{};
    if (!relative_cursor_mode_active_) {
        return motion;
    }

#if defined(__APPLE__)
    if (window_ != nullptr) {

        int32_t dx = 0;
        int32_t dy = 0;
        CGGetLastMouseDelta(&dx, &dy);
        motion.delta_x = static_cast<int>(dx);
        motion.delta_y = static_cast<int>(dy);
        motion.has_delta = (dx != 0 || dy != 0);
    }
#endif

    return motion;
}

bool WindowManager::CaptureCursorAnchor() {
    const auto cursor = GetCursorPositionInWindow();
    if (!cursor.has_value()) {
        return false;
    }

    cursor_anchor_ = cursor;
    return true;
}

bool WindowManager::RestoreCursorAnchor() {
    if (!cursor_anchor_.has_value()) {
        return false;
    }

    SetCursorPosition(cursor_anchor_->first, cursor_anchor_->second);
    return true;
}

bool WindowManager::HasCursorAnchor() const {
    return cursor_anchor_.has_value();
}

void WindowManager::ClearCursorAnchor() {
    cursor_anchor_.reset();
}

void WindowManager::CaptureMouse(bool capture) {
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
        SDL_CaptureMouse(capture ? SDL_TRUE : SDL_FALSE);
    }
}

void WindowManager::BeginMouseButtonCapture(const std::uint32_t button_flag) {
    if (button_flag == 0) {
        return;
    }

    if (mouse_button_capture_mask_ == 0) {
        CaptureMouse(true);
    }

    mouse_button_capture_mask_ |= button_flag;
}

void WindowManager::EndMouseButtonCapture(const std::uint32_t button_flag) {
    if (button_flag == 0) {
        return;
    }

    mouse_button_capture_mask_ &= ~button_flag;
    if (mouse_button_capture_mask_ == 0) {
        CaptureMouse(false);
    }
}

void WindowManager::ResetMouseButtonCapture() {
    mouse_button_capture_mask_ = 0;
    CaptureMouse(false);
}

std::uint32_t WindowManager::GetMouseButtonCaptureMask() const {
    return mouse_button_capture_mask_;
}

void* WindowManager::GetNativeHandle() const {
    if (!window_) return nullptr;

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (SDL_GetWindowWMInfo(static_cast<SDL_Window*>(window_), &wmi) != SDL_TRUE) {
        return nullptr;
    }

#if defined(SDL_VIDEO_DRIVER_X11)
    if (wmi.subsystem == SDL_SYSWM_X11) {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(wmi.info.x11.window));
    }
#endif
#if defined(SDL_VIDEO_DRIVER_WAYLAND)
    if (wmi.subsystem == SDL_SYSWM_WAYLAND) {
        return wmi.info.wl.surface;
    }
#endif
#if defined(_WIN32)
    if (wmi.subsystem == SDL_SYSWM_WINDOWS) {
        return wmi.info.win.window;
    }
#endif
#if defined(__APPLE__)
    if (wmi.subsystem == SDL_SYSWM_COCOA) {
        return wmi.info.cocoa.window;
    }
#endif

    return nullptr;
}

void* WindowManager::GetNativeDisplayHandle() const {
    if (!window_) return nullptr;

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (SDL_GetWindowWMInfo(static_cast<SDL_Window*>(window_), &wmi) != SDL_TRUE) {
        return nullptr;
    }

#if defined(SDL_VIDEO_DRIVER_X11)
    if (wmi.subsystem == SDL_SYSWM_X11) {
        return wmi.info.x11.display;
    }
#endif
#if defined(SDL_VIDEO_DRIVER_WAYLAND)
    if (wmi.subsystem == SDL_SYSWM_WAYLAND) {
        return wmi.info.wl.display;
    }
#endif

    return nullptr;
}

void WindowManager::PollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                should_close_ = true;
                if (close_cb_) close_cb_();
                break;

            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        SetClientSize(static_cast<uint32_t>(event.window.data1),
                                      static_cast<uint32_t>(event.window.data2));
                        break;

                    case SDL_WINDOWEVENT_MINIMIZED:
                        minimized_ = true;
                        break;

                    case SDL_WINDOWEVENT_RESTORED:
                        minimized_ = false;
                        break;

                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        focused_ = true;
                        if (focus_cb_) focus_cb_(true);
                        break;

                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        focused_ = false;
                        if (focus_cb_) focus_cb_(false);
                        break;

                    case SDL_WINDOWEVENT_CLOSE:
                        should_close_ = true;
                        if (close_cb_) close_cb_();
                        break;
                }
                break;

            default:
                break;
        }
    }
}

bool WindowManager::ShouldClose() const { return should_close_; }

void WindowManager::SetResizeCallback(ResizeCallback cb) { resize_cb_ = std::move(cb); }
void WindowManager::SetFocusCallback(FocusCallback cb)   { focus_cb_  = std::move(cb); }
void WindowManager::SetCloseCallback(CloseCallback cb)   { close_cb_  = std::move(cb); }

void WindowManager::SetCursorPositionOverrideForTests(const int x, const int y) {
    cursor_position_override_ = std::pair<int, int>{x, y};
}

void WindowManager::ClearCursorPositionOverrideForTests() {
    cursor_position_override_.reset();
}

void WindowManager::Reset() {
    Shutdown();
    cursor_position_override_.reset();
    cursor_anchor_.reset();
}

}
