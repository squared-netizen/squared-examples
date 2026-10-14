// demo — one screen of the application.
//
// This file is YOURS. The generator will not overwrite it.

#ifndef demo_SCREEN_HPP
#define demo_SCREEN_HPP

#include <squared/gui/ui.hpp>

#include <chrono>
#include <memory>
#include <string_view>

namespace sq::app {
struct Event;
}  // namespace sq::app

namespace demo {

class Services;
class ScreenStack;

/// A screen owns its widgets and nothing else.
///
/// The stack calls build() once when the screen is pushed, and again after a
/// resize or a lost context, so a screen must be able to build its widgets
/// more than once from the state it holds.
class Screen {
public:
    virtual ~Screen() = default;

    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

    /// A name, for logs and for tests.
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /// Attach the widgets for this screen to the given Ui.
    ///
    /// Called with an empty Ui. Anything the screen keeps a pointer to must be
    /// re-acquired here, because the previous widgets are gone.
    virtual void build(sq::gui::Ui& ui, Services& services) = 0;

    /// One simulation step. Most screens need nothing here.
    virtual void update(std::chrono::nanoseconds delta) { (void)delta; }

    /// An event the Ui did not consume.
    ///
    /// @return true when the screen handled it. Back navigation asks the
    /// screen first; a screen that returns false lets the stack pop.
    [[nodiscard]] virtual bool handle_event(const sq::app::Event& event)
    {
        (void)event;
        return false;
    }

    /// Called when this screen becomes, or stops being, the visible one.
    virtual void enter() {}
    virtual void exit() {}

protected:
    Screen() = default;

    /// Set by the stack before build(), so a screen can navigate.
    ScreenStack* stack_{nullptr};
    friend class ScreenStack;
};

}  // namespace demo

#endif  // demo_SCREEN_HPP
