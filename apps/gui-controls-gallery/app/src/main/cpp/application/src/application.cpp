#include <SDL.h>
#include <SDL_ttf.h>

#include <squared/application/application.hpp>
#include <squared/application/event.hpp>
#include <squared/graphics/color.hpp>
#include <squared/graphics/context.hpp>
#include <squared/graphics2d/orthographic_camera.hpp>
#include <squared/graphics2d/sprite_batch.hpp>
#include <squared/graphics2d/texture.hpp>
#include <squared/graphics2d/texture_atlas.hpp>
#include <squared/graphics2d/texture_region.hpp>
#include <squared/gui/gui.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr float kWidth = 960.0F;
constexpr float kHeight = 540.0F;
constexpr std::string_view kFont = "fonts/DejaVuSansMono.ttf";
constexpr std::string_view kSkin = "gui/kenney-test/skin.atlas";

using squared::gui::Rectangle;
using squared::gui::Size;

Rectangle intersect(Rectangle left, Rectangle right) noexcept
{
    const float x = std::max(left.x, right.x);
    const float y = std::max(left.y, right.y);
    const float far_x = std::min(left.x + left.width, right.x + right.width);
    const float far_y = std::min(left.y + left.height, right.y + right.height);
    return {x, y, std::max(0.0F, far_x - x), std::max(0.0F, far_y - y)};
}

class GalleryPainter final : public squared::gui::Painter {
public:
    GalleryPainter(
        squared::graphics2d::SpriteBatch& batch,
        const squared::graphics2d::TextureRegion& white
    ) noexcept
        : batch_(batch), white_(white)
    {
    }

    [[nodiscard]] bool initialize() noexcept
    {
        font_ = TTF_OpenFont(std::string(kFont).c_str(), 24);
        if (!font_) {
            SDL_Log("Controls Gallery font failed: %s", TTF_GetError());
            return false;
        }
        return true;
    }

    void dispose() noexcept
    {
        text_.clear();
        if (font_) TTF_CloseFont(font_);
        font_ = nullptr;
        clips_.clear();
    }

    [[nodiscard]] Size measure_text(std::string_view text) override
    {
        if (!font_ || text.empty()) return {};
        const std::string owned(text);
        int width = 0;
        int height = 0;
        if (TTF_SizeUTF8(font_, owned.c_str(), &width, &height) != 0) return {};
        return {static_cast<float>(width), static_cast<float>(height)};
    }

    void fill_rectangle(
        const Rectangle& rectangle,
        squared::graphics::Color color
    ) override
    {
        const Rectangle visible = clipped(rectangle);
        if (visible.width <= 0.0F || visible.height <= 0.0F) return;
        batch_.draw(
            white_, visible.x, visible.y, visible.width, visible.height, color
        );
    }

    void stroke_rectangle(
        const Rectangle& rectangle,
        squared::graphics::Color color,
        float thickness
    ) override
    {
        thickness = std::max(1.0F, thickness);
        fill_rectangle(
            {rectangle.x, rectangle.y, rectangle.width, thickness}, color
        );
        fill_rectangle(
            {rectangle.x, rectangle.y + rectangle.height - thickness,
             rectangle.width, thickness},
            color
        );
        fill_rectangle(
            {rectangle.x, rectangle.y, thickness, rectangle.height}, color
        );
        fill_rectangle(
            {rectangle.x + rectangle.width - thickness, rectangle.y,
             thickness, rectangle.height},
            color
        );
    }

    void draw_region(
        const squared::graphics2d::TextureRegion& region,
        const Rectangle& rectangle,
        squared::graphics::Color color
    ) override
    {
        if (!region.valid() || rectangle.width <= 0.0F ||
            rectangle.height <= 0.0F) {
            return;
        }
        const Rectangle visible = clipped(rectangle);
        if (visible.width <= 0.0F || visible.height <= 0.0F) return;

        const bool complete =
            visible.x == rectangle.x && visible.y == rectangle.y &&
            visible.width == rectangle.width &&
            visible.height == rectangle.height;
        if (complete || region.rotated_clockwise()) {
            batch_.draw(
                region,
                visible.x,
                visible.y,
                visible.width,
                visible.height,
                color
            );
            return;
        }

        const float left = (visible.x - rectangle.x) / rectangle.width;
        const float top = (visible.y - rectangle.y) / rectangle.height;
        const float right =
            (visible.x + visible.width - rectangle.x) / rectangle.width;
        const float bottom =
            (visible.y + visible.height - rectangle.y) / rectangle.height;
        const auto& texture = region.texture();
        const float source_left = region.u1() + (region.u2() - region.u1()) * left;
        const float source_top = region.v1() + (region.v2() - region.v1()) * top;
        const float source_right = region.u1() + (region.u2() - region.u1()) * right;
        const float source_bottom = region.v1() + (region.v2() - region.v1()) * bottom;
        const int x = static_cast<int>(std::lround(
            source_left * static_cast<float>(texture.width())
        ));
        const int y = static_cast<int>(std::lround(
            source_top * static_cast<float>(texture.height())
        ));
        const int width = std::max(1, static_cast<int>(std::lround(
            (source_right - source_left) * static_cast<float>(texture.width())
        )));
        const int height = std::max(1, static_cast<int>(std::lround(
            (source_bottom - source_top) * static_cast<float>(texture.height())
        )));
        const squared::graphics2d::TextureRegion cropped(
            texture, x, y, width, height
        );
        batch_.draw(
            cropped,
            visible.x,
            visible.y,
            visible.width,
            visible.height,
            color
        );
    }

    void draw_text(
        std::string_view text,
        float x,
        float y,
        squared::graphics::Color color
    ) override
    {
        CachedText* cached = cache(text);
        if (!cached) return;
        draw_region(
            cached->region,
            {x, y, cached->size.width, cached->size.height},
            color
        );
    }

    void push_clip(const Rectangle& rectangle) override
    {
        clips_.push_back(
            clips_.empty() ? rectangle : intersect(clips_.back(), rectangle)
        );
    }

    void pop_clip() override
    {
        if (!clips_.empty()) clips_.pop_back();
    }

private:
    struct CachedText {
        squared::graphics2d::Texture texture;
        squared::graphics2d::TextureRegion region;
        Size size;
    };

    [[nodiscard]] Rectangle clipped(const Rectangle& rectangle) const noexcept
    {
        return clips_.empty() ? rectangle : intersect(rectangle, clips_.back());
    }

    CachedText* cache(std::string_view text)
    {
        if (!font_ || text.empty()) return nullptr;
        const std::string key(text);
        const auto found = text_.find(key);
        if (found != text_.end()) return found->second.get();

        SDL_Surface* rendered = TTF_RenderUTF8_Blended(
            font_, key.c_str(), SDL_Color{255, 255, 255, 255}
        );
        if (!rendered) return nullptr;
        SDL_Surface* rgba = SDL_ConvertSurfaceFormat(
            rendered, SDL_PIXELFORMAT_RGBA32, 0
        );
        SDL_FreeSurface(rendered);
        if (!rgba || rgba->w <= 0 || rgba->h <= 0) {
            if (rgba) SDL_FreeSurface(rgba);
            return nullptr;
        }

        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(rgba->w) *
            static_cast<std::size_t>(rgba->h) * 4U
        );
        const auto* source = static_cast<const std::uint8_t*>(rgba->pixels);
        const std::size_t row_bytes = static_cast<std::size_t>(rgba->w) * 4U;
        for (int row = 0; row < rgba->h; ++row) {
            std::memcpy(
                pixels.data() + static_cast<std::size_t>(row) * row_bytes,
                source + static_cast<std::size_t>(row) *
                    static_cast<std::size_t>(rgba->pitch),
                row_bytes
            );
        }

        auto entry = std::make_unique<CachedText>();
        entry->size = {
            static_cast<float>(rgba->w), static_cast<float>(rgba->h)
        };
        const bool created = entry->texture.create_rgba(
            rgba->w, rgba->h, pixels.data()
        );
        SDL_FreeSurface(rgba);
        if (!created) return nullptr;
        entry->texture.set_filter(
            squared::graphics2d::TextureFilter::Linear,
            squared::graphics2d::TextureFilter::Linear
        );
        entry->region = squared::graphics2d::TextureRegion(entry->texture);
        CachedText* result = entry.get();
        text_.emplace(key, std::move(entry));
        return result;
    }

    squared::graphics2d::SpriteBatch& batch_;
    const squared::graphics2d::TextureRegion& white_;
    TTF_Font* font_{nullptr};
    std::map<std::string, std::unique_ptr<CachedText>> text_;
    std::vector<Rectangle> clips_;
};

}  // namespace

namespace squared_gui_controls_gallery {
namespace {

class ControlsGallery final : public squared::application::Application {
public:
    ControlsGallery() noexcept
        : camera_(
              kWidth,
              kHeight,
              squared::graphics2d::CoordinateOrigin::TopLeft
          )
    {
    }

    [[nodiscard]] bool create(squared::graphics::Context&) override
    {
        if (!batch_.initialize() ||
            !white_texture_.create_solid(squared::graphics::Color::white())) {
            SDL_Log("Controls Gallery graphics initialization failed");
            dispose();
            return false;
        }
        white_region_ = squared::graphics2d::TextureRegion(white_texture_);
        painter_ = std::make_unique<GalleryPainter>(batch_, white_region_);
        if (!painter_->initialize() || !skin_atlas_.load(kSkin.data()) ||
            !build_ui()) {
            dispose();
            return false;
        }
        SDL_Log("Squared GUI Controls Gallery: READY");
        return true;
    }

    void handle_event(const squared::application::Event& event) override
    {
        if (event.type == squared::application::Event::Type::PointerDown ||
            event.type == squared::application::Event::Type::PointerMove ||
            event.type == squared::application::Event::Type::PointerUp) {
            if (ui_) static_cast<void>(ui_->event(event));
        }
        if (event.type == squared::application::Event::Type::QuitRequested ||
            event.type == squared::application::Event::Type::BackRequested) {
            quit_ = true;
        }
    }

    void update(std::chrono::nanoseconds delta) override
    {
        if (ui_) {
            ui_->update(std::chrono::duration<double>(delta).count());
        }
    }

    void render(squared::graphics::Context& graphics) override
    {
        graphics.clear(squared::graphics::Color::from_rgba8(14, 19, 29));
        if (!ui_ || !painter_ || !batch_.begin(camera_)) return;
        ui_->layout(*painter_);
        ui_->paint(*painter_);
        batch_.end();
    }

    void dispose() override
    {
        if (disposed_) return;
        disposed_ = true;
        ui_.reset();
        if (painter_) painter_->dispose();
        painter_.reset();
        skin_atlas_.destroy();
        white_texture_.destroy();
        batch_.destroy();
    }

    [[nodiscard]] bool quit_requested() const noexcept override
    {
        return quit_;
    }

private:
    [[nodiscard]] const squared::graphics2d::AtlasRegion* region(
        const char* name
    ) const noexcept
    {
        return skin_atlas_.find_region(name);
    }

    [[nodiscard]] bool build_ui()
    {
        const auto* button_normal = region("button.normal");
        const auto* button_pressed = region("button.pressed");
        const auto* checkbox_unchecked = region("checkbox.unchecked");
        const auto* checkbox_checked = region("checkbox.checked");
        const auto* slider_track = region("slider.track");
        const auto* slider_knob = region("slider.knob");
        if (!button_normal || !button_pressed || !checkbox_unchecked ||
            !checkbox_checked || !slider_track || !slider_knob) {
            SDL_Log("Controls Gallery skin is incomplete");
            return false;
        }

        squared::gui::Skin skin;
        skin.padding = 10.0F;
        skin.spacing = 8.0F;
        skin.minimum_touch_size = 48.0F;
        skin.add_region_drawable(
            "kenney.button.normal",
            button_normal->region(),
            {18.0F, 10.0F, 18.0F, 10.0F}
        );
        skin.add_region_drawable(
            "kenney.button.pressed",
            button_pressed->region(),
            {18.0F, 10.0F, 18.0F, 10.0F}
        );
        skin.add_region_drawable(
            "kenney.checkbox.unchecked", checkbox_unchecked->region()
        );
        skin.add_region_drawable(
            "kenney.checkbox.checked", checkbox_checked->region()
        );
        skin.add_region_drawable("kenney.slider.track", slider_track->region());
        skin.add_region_drawable("kenney.slider.knob", slider_knob->region());

        auto button_style = skin.button_style("default");
        button_style.normal = skin.drawable("kenney.button.normal");
        button_style.hovered = button_style.normal;
        button_style.pressed = skin.drawable("kenney.button.pressed");
        button_style.disabled = button_style.normal;
        button_style.minimum_height = 52.0F;
        skin.add_button_style("kenney", std::move(button_style));

        auto checkbox_style = skin.check_box_style("default");
        checkbox_style.unchecked = skin.drawable("kenney.checkbox.unchecked");
        checkbox_style.checked = skin.drawable("kenney.checkbox.checked");
        checkbox_style.disabled = checkbox_style.unchecked;
        checkbox_style.minimum_touch_size = 48.0F;
        skin.add_check_box_style("kenney", std::move(checkbox_style));

        auto slider_style = skin.slider_style("default");
        slider_style.track = skin.drawable("kenney.slider.track");
        slider_style.filled_track = skin.drawable("kenney.button.pressed");
        slider_style.knob = skin.drawable("kenney.slider.knob");
        slider_style.minimum_touch_size = 48.0F;
        skin.add_slider_style("kenney", std::move(slider_style));

        ui_ = std::make_unique<squared::gui::Ui>(kWidth, kHeight, std::move(skin));
        auto stack = std::make_unique<squared::gui::Stack>();
        stack->add(std::make_unique<squared::gui::Panel>());

        auto column = std::make_unique<squared::gui::LinearLayout>(
            squared::gui::Direction::vertical
        );
        auto title = std::make_unique<squared::gui::Label>(
            "Squared GUI Controls Gallery"
        );
        column->add(std::move(title));
        auto instructions = std::make_unique<squared::gui::Label>(
            "Touch each control. Back exits."
        );
        instructions->set_muted(true);
        column->add(std::move(instructions));
        column->add(std::make_unique<squared::gui::Separator>());

        auto action = std::make_unique<squared::gui::Button>("Primary action");
        action->set_style("kenney");
        action->set_on_click([this] {
            ++clicks_;
            status_->set_text("Primary action count: " + std::to_string(clicks_));
        });
        column->add(std::move(action));

        auto toggle = std::make_unique<squared::gui::ToggleButton>(
            "Toggle button", false
        );
        toggle->set_style("kenney");
        toggle->set_on_change([this](bool enabled) {
            status_->set_text(enabled ? "Toggle: ON" : "Toggle: OFF");
        });
        column->add(std::move(toggle));

        auto checkbox = std::make_unique<squared::gui::CheckBox>(
            "Handheld mode", true
        );
        checkbox->set_check_style("kenney");
        checkbox->set_on_change([this](bool enabled) {
            status_->set_text(
                enabled ? "Handheld mode enabled" : "Handheld mode disabled"
            );
        });
        column->add(std::move(checkbox));

        auto slider = std::make_unique<squared::gui::Slider>(0.0F, 100.0F, 35.0F);
        slider->set_style("kenney");
        slider->set_step(1.0F);
        slider->set_on_change([this](float value) {
            status_->set_text(
                "Slider: " + std::to_string(static_cast<int>(value))
            );
        });
        column->add(std::move(slider));

        auto field = std::make_unique<squared::gui::TextField>("Sample text");
        field_ = field.get();
        column->add(std::move(field));

        auto replace_text = std::make_unique<squared::gui::Button>(
            "Change sample text"
        );
        replace_text->set_style("kenney");
        replace_text->set_on_click([this] {
            field_->set_text("Touch input confirmed");
            status_->set_text("Text field updated");
        });
        column->add(std::move(replace_text));

        auto status = std::make_unique<squared::gui::Label>("Gallery ready");
        status_ = status.get();
        status_->set_muted(true);
        column->add(std::move(status));

        auto margin = std::make_unique<squared::gui::MarginContainer>(
            squared::gui::Insets{36.0F, 24.0F, 36.0F, 24.0F}
        );
        margin->set_content(std::move(column));
        stack->add(std::move(margin));
        ui_->set_content(std::move(stack));
        return true;
    }

    squared::graphics2d::OrthographicCamera camera_;
    squared::graphics2d::SpriteBatch batch_;
    squared::graphics2d::Texture white_texture_;
    squared::graphics2d::TextureRegion white_region_;
    squared::graphics2d::TextureAtlas skin_atlas_;
    std::unique_ptr<GalleryPainter> painter_;
    std::unique_ptr<squared::gui::Ui> ui_;
    squared::gui::Label* status_{nullptr};
    squared::gui::TextField* field_{nullptr};
    int clicks_{0};
    bool quit_{false};
    bool disposed_{false};
};

}  // namespace

std::unique_ptr<squared::application::Application> create_application()
{
    return std::make_unique<ControlsGallery>();
}

}  // namespace squared_gui_controls_gallery
