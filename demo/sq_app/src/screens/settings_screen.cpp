#include "screens/settings_screen.hpp"

#include "screen_stack.hpp"
#include "services.hpp"

#include <squared/gui/button.hpp>
#include <squared/gui/check_box.hpp>
#include <squared/gui/label.hpp>
#include <squared/gui/linear_layout.hpp>
#include <squared/gui/margin_container.hpp>

#include <memory>

namespace demo {

void SettingsScreen::build(sq::gui::Ui& ui, Services& services)
{
    static_cast<void>(services);

    auto column = std::make_unique<sq::gui::LinearLayout>();
    auto* layout = column.get();

    auto margin = std::make_unique<sq::gui::MarginContainer>();
    margin->set_content(std::move(column));
    ui.set_content(std::move(margin));

    layout->add(std::make_unique<sq::gui::Label>("settings"));
    layout->add(std::make_unique<sq::gui::CheckBox>("something optional"));

    // Back does this too. The button exists because a phone without a back
    // gesture, or a desktop, still needs a way out.
    ScreenStack* stack = stack_;
    layout->add(std::make_unique<sq::gui::Button>(
        "back",
        [stack]() {
            if (stack != nullptr) static_cast<void>(stack->pop());
        }));
}

}  // namespace demo
