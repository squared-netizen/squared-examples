#include "screens/landing_screen.hpp"

#include "screen_stack.hpp"
#include "screens/settings_screen.hpp"
#include "services.hpp"

#include <squared/gui/button.hpp>
#include <squared/gui/label.hpp>
#include <squared/gui/linear_layout.hpp>

#include <memory>

namespace demo {

void LandingScreen::build(sq::gui::Ui& ui, Services& services)
{
    static_cast<void>(services);

    auto column = std::make_unique<sq::gui::LinearLayout>();
    auto* layout = column.get();

    // No MarginContainer here: the spike proved Ui -> LinearLayout -> widgets
    // draws, and this keeps the first screen on exactly that path.
    ui.set_content(std::move(column));

    layout->add(std::make_unique<sq::gui::Label>("demo"));
    layout->add(std::make_unique<sq::gui::Label>("no projects yet"));

    // The stack pointer is set before build(), so navigation is available
    // here. Capturing it is safe: the stack outlives every screen in it.
    ScreenStack* stack = stack_;
    layout->add(std::make_unique<sq::gui::Button>(
        "settings",
        [stack]() {
            if (stack != nullptr) {
                stack->push(std::make_unique<SettingsScreen>());
            }
        }));
}

}  // namespace demo
