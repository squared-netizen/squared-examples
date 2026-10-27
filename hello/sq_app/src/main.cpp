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
//
// A node with a "bind" shows what the model states at that address, and a
// check box, slider or text field sends set to it when the user changes it
// (settings.json). The addresses are in names.json, which the build makes into
// the keys in names.hpp.

#include <layouts.hpp>
#include <names.hpp>
#include <squared/squared.hpp>

#include <cstdint>
#include <string_view>

namespace {

/// The model: it keeps the settings and counts the seconds, on its own thread.
/// Each value it states is shown by the widgets bound to it; each set the
/// widgets send comes to receive, and is stated back.
struct Hello {
    bool sound = true;
    double volume = 5.0;
    std::int64_t seconds = 0;
    double part = 0.0;  // of the next second

    void start(sq::messaging::Telegraph& t)
    {
        t.state(names::settings_sound, sound);
        t.state(names::settings_volume, volume);
        t.state(names::clock_seconds, seconds);
        t.tick_every(250);
    }

    void receive(sq::messaging::Telegraph& t, const sq::messaging::Received& m)
    {
        const sq::messaging::Name to = m.telegram.address().name;
        if (to == names::settings_sound.name()) {
            sound = m.telegram.as_boolean();
            t.state(names::settings_sound, sound);
        } else if (to == names::settings_volume.name()) {
            volume = m.telegram.as_number();
            t.state(names::settings_volume, volume);
        }
    }

    void tick(sq::messaging::Telegraph& t, double elapsed)
    {
        part += elapsed;
        if (part >= 1.0) {
            part -= 1.0;
            t.state(names::clock_seconds, ++seconds);
        }
    }
};

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
    app.run<Hello>();
    app.stage.add<About>();
    app.stage.push(layouts::landing);
}
