#include "app.hpp"

#include "screen_stack.hpp"
#include "screens/landing_screen.hpp"
#include "services.hpp"
#include "trace.hpp"

#include <squared/app/event.hpp>
#include <squared/app/runtime.hpp>
#include <squared/graphics/color.hpp>
#include <squared/graphics/context.hpp>
#include <squared/gui/src/insets.hpp>

#include <chrono>
#include <memory>

namespace demo {

namespace {

/// Where the bundled skin lives, relative to the asset root.
constexpr const char* k_skin_directory = "skins/default/skin";

/// The logical length of the drawable's shorter side.
///
/// Logical rather than pixels: a phone's pixel width varies by a factor of
/// three across devices, and laying out in pixels makes every widget a
/// different size on each. The longer side follows the drawable's aspect, so
/// one logical unit is square in either orientation - a fixed 720x1280 was
/// stretched on any screen of another shape, and squashed in landscape.
constexpr float k_logical_short_side = 720.0F;

struct LogicalSize {
    float width;
    float height;
};

/// The logical size for a drawable of `pixel_width` x `pixel_height`.
LogicalSize logical_size_for(int pixel_width, int pixel_height) noexcept
{
    if (pixel_width <= 0 || pixel_height <= 0) {
        return {k_logical_short_side, k_logical_short_side * 16.0F / 9.0F};
    }
    const float width = static_cast<float>(pixel_width);
    const float height = static_cast<float>(pixel_height);
    if (width <= height) {
        return {k_logical_short_side, k_logical_short_side * height / width};
    }
    return {k_logical_short_side * width / height, k_logical_short_side};
}

/// Each state gets a colour nobody could mistake for another, or for a blank
/// screen. A near-black "normal" background is indistinguishable from an app
/// that never rendered at all, which is exactly the question one wants
/// answered first on a device with no log.
///
/// red     the skin did not load; failure_reason() says why
/// blue    the skin loaded but there is no screen stack
/// grey    painting; anything drawn appears on top of this
constexpr sq::graphics::Color k_failure{
    .red = 0.55F, .green = 0.05F, .blue = 0.05F, .alpha = 1.0F};

constexpr sq::graphics::Color k_no_screens{
    .red = 0.05F, .green = 0.15F, .blue = 0.55F, .alpha = 1.0F};

constexpr sq::graphics::Color k_background{
    .red = 0.25F, .green = 0.25F, .blue = 0.28F, .alpha = 1.0F};

/// magenta  the batch refused to begin
/// yellow   the painter queued nothing: no usable fill source
///
/// Both are drawn over the finished frame, so they are unmistakable. A frame
/// that painted is left alone.
constexpr sq::graphics::Color k_begin_failed{
    .red = 0.80F, .green = 0.00F, .blue = 0.80F, .alpha = 1.0F};

constexpr sq::graphics::Color k_fill_skipped{
    .red = 0.90F, .green = 0.80F, .blue = 0.00F, .alpha = 1.0F};

}  // namespace

struct App::State {
    Services services;
    std::unique_ptr<ScreenStack> screens;
    int pixel_width{0};
    int pixel_height{0};
    LogicalSize logical{k_logical_short_side, k_logical_short_side};
    sq::app::SafeArea safe{};
    bool quit{false};

    /// The bars over the screen's edges, in logical units.
    ///
    /// The platform measures them in pixels; the interface lives in logical
    /// units, and the two scales differ per axis only by the aspect the
    /// logical size was derived from.
    [[nodiscard]] sq::gui::Insets logical_safe() const noexcept
    {
        if (pixel_width <= 0 || pixel_height <= 0) return {};
        const float x = logical.width / static_cast<float>(pixel_width);
        const float y = logical.height / static_cast<float>(pixel_height);
        return {static_cast<float>(safe.left) * x, static_cast<float>(safe.top) * y,
                static_cast<float>(safe.right) * x, static_cast<float>(safe.bottom) * y};
    }

    /// Record a drawable size and derive the logical one from it.
    void set_pixel_size(int width, int height) noexcept
    {
        pixel_width = width;
        pixel_height = height;
        logical = logical_size_for(width, height);
    }
};

App::App() : state_(std::make_unique<State>()) {}
App::~App() = default;

bool App::create(sq::app::Runtime& runtime)
{
    state_->set_pixel_size(runtime.graphics.pixel_width(),
                           runtime.graphics.pixel_height());

    // A skin that will not load is not a reason to refuse to start: the
    // application comes up red and says so, which beats a process that
    // disappears before anything can report why.
    trace("create: pixels %dx%d, logical %.0fx%.0f", state_->pixel_width,
          state_->pixel_height, static_cast<double>(state_->logical.width),
          static_cast<double>(state_->logical.height));
    if (!state_->services.load(runtime, k_skin_directory)) {
        trace("skin failed: %s", state_->services.failure_reason().c_str());
        return true;
    }

    state_->services.resize(state_->logical.width, state_->logical.height,
                            state_->pixel_width, state_->pixel_height);

    state_->screens = std::make_unique<ScreenStack>(
        state_->services, state_->logical.width, state_->logical.height);
    state_->screens->set_safe_area(state_->logical_safe());
    state_->screens->replace(std::make_unique<LandingScreen>());
    return true;
}

void App::handle_event(const sq::app::Event& event)
{
    using Type = sq::app::Event::Type;

    // The platform reports in drawable pixels; the interface lives in logical
    // units. Pointers are scaled here, once, so no screen or widget ever sees
    // a pixel. A resize has already been applied through resize(), and would
    // otherwise resize the Ui to the pixel size.
    if (event.type == Type::Resize) return;

    sq::app::Event translated = event;
    if ((event.type == Type::PointerDown || event.type == Type::PointerMove ||
         event.type == Type::PointerUp) &&
        state_->pixel_width > 0 && state_->pixel_height > 0) {
        translated.x = event.x * state_->logical.width /
                       static_cast<float>(state_->pixel_width);
        translated.y = event.y * state_->logical.height /
                       static_cast<float>(state_->pixel_height);
    }

    if (translated.type == Type::PointerDown) {
        trace("pointer down: pixels %.0f,%.0f -> logical %.0f,%.0f",
              static_cast<double>(event.x), static_cast<double>(event.y),
              static_cast<double>(translated.x), static_cast<double>(translated.y));
    }
    if (state_->screens && state_->screens->handle_event(translated)) return;

    // Nothing wanted it. Back at the root means leave.
    if (event.type == Type::BackRequested ||
        event.type == Type::QuitRequested) {
        state_->quit = true;
    }
}

void App::update(std::chrono::nanoseconds delta)
{
    if (state_->screens) state_->screens->update(delta);
}

void App::render(sq::graphics::Context& graphics)
{
    if (!state_->services.loaded()) {
        graphics.clear(k_failure);
        return;
    }
    if (!state_->screens) {
        graphics.clear(k_no_screens);
        return;
    }
    graphics.clear(k_background);
    switch (state_->screens->paint()) {
    case ScreenStack::PaintResult::begin_failed:
        graphics.clear(k_begin_failed);
        break;
    case ScreenStack::PaintResult::fill_skipped:
        graphics.clear(k_fill_skipped);
        break;
    case ScreenStack::PaintResult::painted:
    case ScreenStack::PaintResult::no_screen:
        break;
    }
}

void App::pause() {}
void App::resume() {}

void App::resize(int width, int height)
{
    state_->set_pixel_size(width, height);
    state_->services.resize(state_->logical.width, state_->logical.height,
                            width, height);
    if (state_->screens) {
        state_->screens->resize(state_->logical.width, state_->logical.height);
        state_->screens->set_safe_area(state_->logical_safe());
    }
}

void App::safe_area_changed(const sq::app::SafeArea& area)
{
    state_->safe = area;
    if (state_->screens) {
        state_->screens->set_safe_area(state_->logical_safe());
    }
}

void App::surface_created(sq::graphics::Context& graphics)
{
    state_->set_pixel_size(graphics.pixel_width(), graphics.pixel_height());
    state_->services.resize(state_->logical.width, state_->logical.height,
                            state_->pixel_width, state_->pixel_height);

    // Unconditional: restore() asks the context whether anything was lost, so
    // a first context costs nothing here. Rebuilding the widgets is only
    // needed when it did lose something.
    if (!state_->services.restore(graphics)) return;
    if (!graphics.resources_preserved() && state_->screens) {
        state_->screens->rebuild();
    }
}

void App::surface_destroyed() {}
void App::dispose() {}

bool App::quit_requested() const noexcept { return state_->quit; }

}  // namespace demo
