// demo — the things every screen needs, built once.
//
// This file is YOURS. The generator will not overwrite it.

#ifndef demo_SERVICES_HPP
#define demo_SERVICES_HPP

#include <squared/graphics/orthographic_camera.hpp>
#include <squared/graphics/sprite_batch.hpp>
#include <squared/graphics/texture_atlas.hpp>
#include <squared/gui/batch_painter.hpp>
#include <squared/gui/src/font_ptr.hpp>
#include <squared/gui/skin.hpp>

#include <memory>
#include <string>
#include <utility>

namespace sq::app {
struct Runtime;
}  // namespace sq::app

namespace sq::graphics {
class Context;
}  // namespace sq::graphics

namespace demo {

/// Loaded once at startup and referred to by every screen.
///
/// Not a god object: a thing belongs here only if it is expensive to build and
/// shared by more than one screen. A screen's own widgets do not.
class Services final {
public:
    Services() noexcept;
    ~Services();

    Services(const Services&) = delete;
    Services& operator=(const Services&) = delete;

    /// Load the skin and prepare the batch and painter.
    ///
    /// @param runtime the platform services
    /// @param skin_directory where uiskin.atlas and its siblings live
    /// @return false when the skin could not be loaded; failure_reason() says
    /// why. The application should still start: a screen can say so on screen,
    /// which beats a process that vanishes.
    [[nodiscard]] bool load(
        sq::app::Runtime& runtime,
        const std::string& skin_directory
    );

    /// Tell the painter how logical units map to pixels. Call on every resize.
    void resize(float logical_width, float logical_height,
                int pixel_width, int pixel_height) noexcept;

    /// Rebuild whatever the GPU lost.
    ///
    /// Safe to call unconditionally: the context knows whether anything was
    /// lost, and on a first context nothing was, so this does nothing.
    [[nodiscard]] bool restore(const sq::graphics::Context& graphics) noexcept;

    /// Hand the loaded skin to whoever draws with it.
    ///
    /// Moved, not copied: the Ui is the only reader, and a second copy of the
    /// style tables would sit resident for nothing. After this call the
    /// services hold an empty skin. Call once, after load() succeeds.
    [[nodiscard]] sq::gui::Skin take_skin()
    {
        return std::move(skin_);
    }
    [[nodiscard]] sq::gui::BatchPainter& painter() noexcept { return *painter_; }
    [[nodiscard]] sq::graphics::SpriteBatch& batch() noexcept { return batch_; }
    [[nodiscard]] sq::graphics::OrthographicCamera& camera() noexcept
    {
        return *camera_;
    }
    [[nodiscard]] const sq::gui::FontPtr& font() const noexcept { return font_; }
    [[nodiscard]] bool loaded() const noexcept { return loaded_; }
    [[nodiscard]] const std::string& failure_reason() const noexcept
    {
        return failure_reason_;
    }

private:
    [[nodiscard]] bool load_font(
        sq::app::Runtime& runtime,
        const std::string& skin_directory
    );

    sq::graphics::TextureAtlas atlas_;
    sq::gui::Skin skin_;
    sq::gui::FontPtr font_;
    sq::graphics::SpriteBatch batch_;
    std::unique_ptr<sq::graphics::OrthographicCamera> camera_;
    std::unique_ptr<sq::gui::BatchPainter> painter_;
    std::string skin_directory_;
    std::string failure_reason_;
    bool loaded_{false};
};

}  // namespace demo

#endif  // demo_SERVICES_HPP
