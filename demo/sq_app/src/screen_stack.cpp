#include "screen_stack.hpp"

#include "screen.hpp"
#include "services.hpp"
#include "trace.hpp"

#include <squared/app/event.hpp>
#include <squared/graphics/sprite_batch.hpp>
#include <squared/graphics/color.hpp>
#include <squared/gui/batch_painter.hpp>
#include <squared/gui/src/rectangle.hpp>
#include <squared/gui/src/size.hpp>
#include <squared/gui/widget.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

namespace demo {

namespace {

/// Draw a white bar to separate "nothing draws" from "widgets do not draw".
constexpr bool k_draw_probe = false;

}  // namespace


ScreenStack::ScreenStack(Services& services, float width, float height)
    : services_(services), ui_(width, height, services.take_skin())
{
}

ScreenStack::~ScreenStack() = default;

void ScreenStack::show_top()
{
    if (screens_.empty()) return;
    Screen& top = *screens_.back();
    top.build(ui_, services_);

    // Laying out needs a painter, and there is none when the skin failed to
    // load. Navigation still has to work in that state: the application comes
    // up red and says so rather than crashing on its first screen.
    if (services_.loaded()) ui_.layout(services_.painter());

    const sq::gui::Widget* content = ui_.content();
    if (content != nullptr && services_.loaded()) {
        const sq::gui::Size text = services_.painter().measure_text("settings");
        trace("show %.*s: content x %.0f y %.0f w %.0f h %.0f; \"settings\" measures %.0fx%.0f",
              static_cast<int>(top.name().size()), top.name().data(),
              static_cast<double>(content->x()), static_cast<double>(content->y()),
              static_cast<double>(content->width()), static_cast<double>(content->height()),
              static_cast<double>(text.width), static_cast<double>(text.height));
    }
}

void ScreenStack::do_push(std::unique_ptr<Screen> screen)
{
    if (!screens_.empty()) screens_.back()->exit();
    screen->stack_ = this;
    screens_.push_back(std::move(screen));
    screens_.back()->enter();
    show_top();
}

void ScreenStack::do_replace(std::unique_ptr<Screen> screen)
{
    if (!screens_.empty()) {
        screens_.back()->exit();
        screens_.pop_back();
    }
    screen->stack_ = this;
    screens_.push_back(std::move(screen));
    screens_.back()->enter();
    show_top();
}

void ScreenStack::do_pop()
{
    screens_.back()->exit();
    screens_.pop_back();
    screens_.back()->enter();
    show_top();
}

std::size_t ScreenStack::depth_after_pending() const noexcept
{
    std::size_t depth = screens_.size();
    for (const Pending& change : pending_) {
        switch (change.kind) {
        case Pending::Kind::push: ++depth; break;
        case Pending::Kind::replace: if (depth == 0U) ++depth; break;
        case Pending::Kind::pop: if (depth > 0U) --depth; break;
        }
    }
    return depth;
}

void ScreenStack::flush_pending()
{
    // A change can ask for another (a screen that navigates as it appears), so
    // take the list first and keep going until nothing is left.
    while (!pending_.empty()) {
        std::vector<Pending> changes;
        changes.swap(pending_);
        for (Pending& change : changes) {
            switch (change.kind) {
            case Pending::Kind::push: do_push(std::move(change.screen)); break;
            case Pending::Kind::replace: do_replace(std::move(change.screen)); break;
            case Pending::Kind::pop:
                if (screens_.size() > 1U) do_pop();
                break;
            }
        }
    }
}

void ScreenStack::push(std::unique_ptr<Screen> screen)
{
    if (!screen) return;
    if (dispatching_) {
        pending_.push_back({Pending::Kind::push, std::move(screen)});
        return;
    }
    do_push(std::move(screen));
}

void ScreenStack::replace(std::unique_ptr<Screen> screen)
{
    if (!screen) return;
    if (dispatching_) {
        pending_.push_back({Pending::Kind::replace, std::move(screen)});
        return;
    }
    do_replace(std::move(screen));
}

bool ScreenStack::pop()
{
    // The root screen is not poppable: there is nothing behind it, and an
    // empty stack would leave the application with nothing to draw.
    if (depth_after_pending() <= 1U) return false;
    if (dispatching_) {
        pending_.push_back({Pending::Kind::pop, nullptr});
        return true;
    }
    do_pop();
    return true;
}

void ScreenStack::set_safe_area(sq::gui::Insets insets)
{
    ui_.set_safe_area(insets);
    if (services_.loaded() && ui_.content() != nullptr) {
        ui_.layout(services_.painter());
    }
    trace("safe area (logical): left %.0f top %.0f right %.0f bottom %.0f",
          static_cast<double>(ui_.safe_area().left), static_cast<double>(ui_.safe_area().top),
          static_cast<double>(ui_.safe_area().right), static_cast<double>(ui_.safe_area().bottom));
}

bool ScreenStack::handle_event(const sq::app::Event& event)
{
    using Type = sq::app::Event::Type;

    if (screens_.empty()) return false;

    // The screen sees everything first: a dialog wants to swallow back before
    // the stack acts on it. Then the widgets - without that a tap reached no
    // button at all.
    //
    // Navigation asked for in here (a button's callback) is held back until
    // delivery has finished: it replaces the widgets, and the one delivering
    // this event is among them. Freed while it was still running, the Ui went
    // on to touch it, which is how a button worked once and then did not.
    dispatching_ = true;
    const bool screen_handled = screens_.back()->handle_event(event);
    const bool ui_handled = !screen_handled && ui_.event(event);
    dispatching_ = false;
    flush_pending();

    if (event.type == Type::PointerDown || event.type == Type::PointerUp) {
        trace("%s at %.0f,%.0f: screen %d ui %d, now %.*s",
              event.type == Type::PointerDown ? "down" : "up",
              static_cast<double>(event.x), static_cast<double>(event.y),
              screen_handled ? 1 : 0, ui_handled ? 1 : 0,
              static_cast<int>(top_name().size()), top_name().data());
    }
    if (screen_handled || ui_handled) return true;

    if (event.type == Type::BackRequested) return pop();

    // Back also arrives as a navigation cancel. Consuming it here would pop
    // twice for one press, so the stack ignores cancel and acts only on
    // BackRequested.
    if (event.type == Type::NavigationInput &&
        event.navigation == sq::app::Event::Navigation::cancel) {
        return false;
    }
    return false;
}

void ScreenStack::update(std::chrono::nanoseconds delta)
{
    if (screens_.empty()) return;
    screens_.back()->update(delta);
    ui_.update(std::chrono::duration<double>(delta).count());
}

ScreenStack::PaintResult ScreenStack::paint()
{
    if (screens_.empty() || !services_.loaded()) return PaintResult::no_screen;
    if (!services_.batch().begin(services_.camera())) {
        return PaintResult::begin_failed;
    }

    // The probe, drawn directly rather than through a widget. Whether it
    // reaches the queue separates "the painter refused" from "it was
    // submitted and you cannot see it" - two problems that look identical on
    // a screen. Delete it once the interface is up.
    PaintResult result = PaintResult::painted;
    if (k_draw_probe) {
        const std::size_t before = services_.batch().queued_sprites();
        services_.painter().fill_rectangle(
            {16.0F, 16.0F, 240.0F, 80.0F},
            sq::graphics::Color{.red = 1.0F, .green = 1.0F, .blue = 1.0F,
                                .alpha = 1.0F});
        if (services_.batch().queued_sprites() == before) {
            result = PaintResult::fill_skipped;
        }
    }

    ui_.paint(services_.painter());
    services_.batch().end();
    return result;
}

void ScreenStack::resize(float width, float height)
{
    ui_.resize(width, height);
    show_top();
}

void ScreenStack::rebuild()
{
    show_top();
}

std::string_view ScreenStack::top_name() const noexcept
{
    return screens_.empty() ? std::string_view{} : screens_.back()->name();
}

}  // namespace demo
