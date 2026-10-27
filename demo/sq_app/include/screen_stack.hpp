// demo — navigation.
//
// This file is YOURS. The generator will not overwrite it.

#ifndef demo_SCREEN_STACK_HPP
#define demo_SCREEN_STACK_HPP

#include <squared/gui/src/detail/ui.hpp>  // internal since 7.7: see screen.hpp
#include <squared/gui/src/insets.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace sq::app {
struct Event;
}  // namespace sq::app

namespace sq::graphics {
class Context;
}  // namespace sq::graphics

namespace demo {

class Screen;
class Services;

/// The screens, and which one is visible.
///
/// One Ui, whose content the stack swaps. A Ui per screen would be simpler to
/// reason about and would rebuild the whole widget tree on every navigation
/// anyway, since a screen must be able to rebuild itself regardless.
class ScreenStack final {
public:
    /// Build the stack and its Ui.
    ///
    /// Takes the skin from `services`, so construct it after services.load()
    /// has succeeded. A Ui without the skin lays out and paints nothing: every
    /// label has no font and every button no drawable.
    ScreenStack(Services& services, float width, float height);
    ~ScreenStack();

    ScreenStack(const ScreenStack&) = delete;
    ScreenStack& operator=(const ScreenStack&) = delete;

    /// Make `screen` the visible one, keeping what is below it.
    ///
    /// Called while an event is being delivered - which is what a button's
    /// callback is - the change waits until delivery has finished. Building
    /// the next screen replaces the widgets, and the widget that is running
    /// this very call is one of them.
    void push(std::unique_ptr<Screen> screen);

    /// Replace the visible screen. Used for the root, and for navigation that
    /// should not be undone by back. Deferred during an event, like push().
    void replace(std::unique_ptr<Screen> screen);

    /// Return to the screen below. Deferred during an event, like push().
    ///
    /// @return false when this is the only screen, which is how the
    /// application knows back should quit instead.
    [[nodiscard]] bool pop();

    /// Keep the interface clear of the bars over the screen's edges.
    ///
    /// @param insets distances in logical units, as from the application.
    void set_safe_area(sq::gui::Insets insets);

    /// Offer an event to the visible screen, then to the Ui.
    ///
    /// Back is the interesting case: the screen sees it first, and if the
    /// screen does not handle it the stack pops. Only when the stack cannot
    /// pop does this return false, meaning "nothing left to go back to".
    [[nodiscard]] bool handle_event(const sq::app::Event& event);

    void update(std::chrono::nanoseconds delta);

    /// What a frame did, so a device with no log can show it as a colour.
    enum class PaintResult {
        painted,        ///< the frame was submitted
        no_screen,      ///< nothing to paint
        begin_failed,   ///< the batch refused to start
        fill_skipped    ///< the probe drew nothing: no usable fill source
    };

    PaintResult paint();

    /// Re-lay out for a new size. Rebuilds the visible screen's widgets.
    void resize(float width, float height);

    /// Rebuild the visible screen's widgets, after a lost context.
    void rebuild();

    [[nodiscard]] std::size_t depth() const noexcept { return screens_.size(); }
    [[nodiscard]] std::string_view top_name() const noexcept;
    [[nodiscard]] sq::gui::Ui& ui() noexcept { return ui_; }

private:
    /// A change asked for while an event was being delivered.
    struct Pending {
        enum class Kind : unsigned char { push, replace, pop };
        Kind kind;
        std::unique_ptr<Screen> screen;
    };

    void show_top();
    void do_push(std::unique_ptr<Screen> screen);
    void do_replace(std::unique_ptr<Screen> screen);
    void do_pop();
    void flush_pending();

    /// How many screens there will be once everything pending has happened.
    [[nodiscard]] std::size_t depth_after_pending() const noexcept;

    Services& services_;
    sq::gui::Ui ui_;
    std::vector<std::unique_ptr<Screen>> screens_;
    std::vector<Pending> pending_;
    bool dispatching_{false};
};

}  // namespace demo

#endif  // demo_SCREEN_STACK_HPP
