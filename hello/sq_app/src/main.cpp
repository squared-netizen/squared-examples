// hello: the app.
//
// This file is YOURS (ownership class: seeded). The generator wrote it once and
// will never overwrite it.
//
// squared_main runs once, when the first window is up: push the first screen
// here, and start your model with app.run<YourModel>(). A screen is a layout
// file in sq_app/layouts/, compiled by the build into layouts::<its name>, or a
// type written in C++, like About below. A pane is a layout of kind pane, over
// the screen that opens it, like quit.json. Buttons' taps are lines: `push
// settings`, `back`, `open quit`, `close`, `quit`, or a telegram such as `set
// settings.sound false`, which goes to your model.

#include <layouts.hpp>
#include <squared/squared.hpp>

#include <string_view>

namespace {

/// A screen written in C++, built each time it is shown. landing.json reaches
/// it by its name (`push about`); names.json lists it, and squared_main adds it.
struct About {
    static constexpr std::string_view name = "about";

    static void build(sq::gui::LayoutWriter& root)
    {
        root.add<sq::gui::Label>("hello, a squared app");
        root.row();
        root.add<sq::gui::Label>("this screen is written in C++");
        root.row();
        root.add<sq::gui::Button>("Back").tap(sq::gui::Navigate::back);
    }
};

}  // namespace

void squared_main(sq::App& app)
{
    app.stage.add<About>();
    app.stage.push(layouts::landing);
}
