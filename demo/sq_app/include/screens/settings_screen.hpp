// demo — settings.
//
// This file is YOURS. The generator will not overwrite it.

#ifndef demo_SCREENS_SETTINGS_SCREEN_HPP
#define demo_SCREENS_SETTINGS_SCREEN_HPP

#include "screen.hpp"

#include <string_view>

namespace demo {

class SettingsScreen final : public Screen {
public:
    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "settings";
    }

    void build(sq::gui::Ui& ui, Services& services) override;
};

}  // namespace demo

#endif  // demo_SCREENS_SETTINGS_SCREEN_HPP
