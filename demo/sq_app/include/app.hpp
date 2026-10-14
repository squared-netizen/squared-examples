// demo — application.
//
// This file is YOURS (ownership class: seeded). The generator wrote it once
// and will never overwrite it.
//
// Note what is NOT here: no Android header, no EGL, no GLES. This class is
// portable - the platform layer in sq_android/ calls into it, and the same
// class compiles under another template with a different platform layer.
//
// It is deliberately thin. It owns the services and the screen stack, and
// forwards. Everything with behaviour lives in a screen.

#ifndef demo_APP_HPP
#define demo_APP_HPP

#include <squared/app/application.hpp>

#include <chrono>
#include <memory>

namespace demo {

class App final : public sq::app::Application {
public:
    App();
    ~App() override;

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    [[nodiscard]] bool create(sq::app::Runtime& runtime) override;
    void handle_event(const sq::app::Event& event) override;
    void update(std::chrono::nanoseconds delta) override;
    void render(sq::graphics::Context& graphics) override;
    void pause() override;
    void resume() override;
    void resize(int width, int height) override;
    void safe_area_changed(const sq::app::SafeArea& area) override;
    void surface_created(sq::graphics::Context& graphics) override;
    void surface_destroyed() override;
    void dispose() override;
    [[nodiscard]] bool quit_requested() const noexcept override;

private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace demo

#endif  // demo_APP_HPP
