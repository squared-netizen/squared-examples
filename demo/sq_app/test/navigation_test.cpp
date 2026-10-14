// demo — navigation, against no graphics at all.
//
// The screen stack is pure logic: which screen is visible, what back does, and
// whether the root refuses to pop. None of that needs a GPU or a skin, so it
// is tested without either - including the case where the skin failed to load,
// which must still navigate rather than crash.

#include "screen.hpp"
#include "screen_stack.hpp"
#include "services.hpp"

#include <squared/app/event.hpp>

#include <cassert>
#include <cstdio>
#include <memory>
#include <string_view>

using namespace demo;

namespace {

int g_built = 0;

class FakeScreen final : public Screen {
public:
    explicit FakeScreen(std::string_view name, bool swallow_back = false)
        : name_(name), swallow_back_(swallow_back) {}

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return name_;
    }

    void build(sq::gui::Ui&, Services&) override { ++g_built; }

    [[nodiscard]] bool handle_event(const sq::app::Event& event) override
    {
        if (swallow_back_ &&
            event.type == sq::app::Event::Type::BackRequested) {
            ++swallowed_;
            return true;
        }
        return false;
    }

    void enter() override { ++entered_; }
    void exit() override { ++exited_; }

    int entered_{0};
    int exited_{0};
    int swallowed_{0};

private:
    std::string_view name_;
    bool swallow_back_{false};
};

/// A screen that navigates from inside event delivery, as a button's callback
/// does, and records what the stack looked like while it was doing so.
class NavigatingScreen final : public Screen {
public:
    enum class Action { push, pop, replace };

    explicit NavigatingScreen(Action action) : action_(action) {}

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "navigating";
    }

    void build(sq::gui::Ui&, Services&) override { ++g_built; }

    [[nodiscard]] bool handle_event(const sq::app::Event& event) override
    {
        if (event.type != sq::app::Event::Type::KeyDown) return false;
        depth_during_ = stack_->depth();
        switch (action_) {
        case Action::push:
            stack_->push(std::make_unique<FakeScreen>("pushed"));
            break;
        case Action::replace:
            stack_->replace(std::make_unique<FakeScreen>("replaced"));
            break;
        case Action::pop:
            pop_result_ = stack_->pop();
            break;
        }
        depth_after_call_ = stack_->depth();
        return true;
    }

    std::size_t depth_during_{0};
    std::size_t depth_after_call_{0};
    bool pop_result_{false};

private:
    Action action_;
};

sq::app::Event key_event()
{
    sq::app::Event event;
    event.type = sq::app::Event::Type::KeyDown;
    return event;
}

sq::app::Event back_event()
{
    sq::app::Event event;
    event.type = sq::app::Event::Type::BackRequested;
    return event;
}

sq::app::Event cancel_event()
{
    sq::app::Event event;
    event.type = sq::app::Event::Type::NavigationInput;
    event.navigation = sq::app::Event::Navigation::cancel;
    return event;
}

}  // namespace

int main()
{
    Services services;                   // deliberately never loaded
    assert(!services.loaded());

    ScreenStack stack{services, 720.0F, 1280.0F};
    assert(stack.depth() == 0);

    // --- the root ---------------------------------------------------------
    stack.replace(std::make_unique<FakeScreen>("landing"));
    assert(stack.depth() == 1);
    assert(stack.top_name() == "landing");

    // back at the root does not pop: the application quits instead
    assert(!stack.handle_event(back_event()));
    assert(stack.depth() == 1);

    // --- pushing and popping ----------------------------------------------
    stack.push(std::make_unique<FakeScreen>("settings"));
    assert(stack.depth() == 2);
    assert(stack.top_name() == "settings");

    assert(stack.handle_event(back_event()));
    assert(stack.depth() == 1);
    assert(stack.top_name() == "landing");

    // --- a screen may swallow back ----------------------------------------
    {
        auto dialog = std::make_unique<FakeScreen>("dialog", true);
        FakeScreen* raw = dialog.get();
        stack.push(std::move(dialog));
        assert(stack.depth() == 2);

        assert(stack.handle_event(back_event()));
        assert(raw->swallowed_ == 1);
        assert(stack.depth() == 2);      // swallowed, so no pop
    }

    // --- the cancel that accompanies back must not pop a second time ------
    {
        const std::size_t before = stack.depth();
        assert(!stack.handle_event(cancel_event()));
        assert(stack.depth() == before);
    }

    // --- replace does not grow the stack ----------------------------------
    {
        const std::size_t before = stack.depth();
        stack.replace(std::make_unique<FakeScreen>("workspace"));
        assert(stack.depth() == before);
        assert(stack.top_name() == "workspace");
    }

    // --- navigation from inside an event waits for the event to finish -----
    //
    // A button's callback runs while the Ui is still delivering the event to
    // that button. Replacing the widgets right then frees the button under the
    // Ui's feet.
    {
        stack.replace(std::make_unique<FakeScreen>("base"));
        const std::size_t base = stack.depth();

        auto pusher = std::make_unique<NavigatingScreen>(NavigatingScreen::Action::push);
        NavigatingScreen* raw = pusher.get();
        stack.push(std::move(pusher));
        assert(stack.depth() == base + 1);

        assert(stack.handle_event(key_event()));
        assert(raw->depth_during_ == base + 1);
        assert(raw->depth_after_call_ == base + 1);   // not yet: the call only queued it
        assert(stack.depth() == base + 2);            // done once delivery ended
        assert(stack.top_name() == "pushed");

        // back returns through the pushed screen to the one that navigated
        assert(stack.handle_event(back_event()));
        assert(stack.top_name() == "navigating");
    }
    {
        auto popper = std::make_unique<NavigatingScreen>(NavigatingScreen::Action::pop);
        NavigatingScreen* raw = popper.get();
        stack.push(std::move(popper));
        const std::size_t before = stack.depth();

        assert(stack.handle_event(key_event()));
        assert(raw->pop_result_);                     // there was something to pop to
        assert(raw->depth_after_call_ == before);     // deferred
        assert(stack.depth() == before - 1);
    }
    {
        auto replacer = std::make_unique<NavigatingScreen>(NavigatingScreen::Action::replace);
        stack.push(std::move(replacer));
        const std::size_t before = stack.depth();
        assert(stack.handle_event(key_event()));
        assert(stack.depth() == before);
        assert(stack.top_name() == "replaced");
    }
    {
        // Popping the only screen is refused even when asked from an event.
        ScreenStack lone{services, 720.0F, 1280.0F};
        auto popper = std::make_unique<NavigatingScreen>(NavigatingScreen::Action::pop);
        NavigatingScreen* raw = popper.get();
        lone.replace(std::move(popper));
        assert(lone.handle_event(key_event()));
        assert(!raw->pop_result_);
        assert(lone.depth() == 1);
    }

    // --- the bars over the screen's edges ----------------------------------
    stack.set_safe_area({.left = 0.0F, .top = 48.0F, .right = 0.0F, .bottom = 32.0F});
    assert(stack.ui().safe_area().top == 48.0F);
    assert(stack.ui().safe_area().bottom == 32.0F);
    stack.resize(1280.0F, 720.0F);                    // rotation keeps them
    assert(stack.ui().safe_area().top == 48.0F);

    // --- painting without a skin is a no-op, not a crash -------------------
    static_cast<void>(stack.paint());
    stack.update(std::chrono::milliseconds(16));
    stack.resize(1080.0F, 1920.0F);

    std::printf("navigation: all assertions passed, %d screen build(s)\n",
                g_built);
    return 0;
}
