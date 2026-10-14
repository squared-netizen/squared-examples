// demo — the screen the application starts on.
//
// This file is YOURS. The generator will not overwrite it.

#ifndef demo_SCREENS_LANDING_SCREEN_HPP
#define demo_SCREENS_LANDING_SCREEN_HPP

#include "screen.hpp"

#include <string_view>

namespace demo {

/// Projects, and the way in to everything else.
class LandingScreen final : public Screen {
public:
    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "landing";
    }

    void build(sq::gui::Ui& ui, Services& services) override;
};

}  // namespace demo

#endif  // demo_SCREENS_LANDING_SCREEN_HPP
