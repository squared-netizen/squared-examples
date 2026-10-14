#include "services.hpp"

#include <squared/app/runtime.hpp>
#include <squared/graphics/context.hpp>
#include <squared/files/file_handle.hpp>
#include <squared/files/file_system.hpp>
#include <squared/files/src/file_text_result.hpp>
#include <squared/graphics/atlas_region.hpp>
#include <squared/graphics/bitmap_font.hpp>
#include <squared/graphics/src/bitmap_font_error.hpp>
#include <squared/graphics/src/bitmap_font_page.hpp>
#include <squared/graphics/src/coordinate_origin.hpp>
#include <squared/graphics/texture_region.hpp>
#include <squared/gui/src/font_resource.hpp>
#include <squared/gui/src/skin_load_issue.hpp>
#include <squared/gui/src/skin_load_report.hpp>
#include <squared/gui/src/skin_load_severity.hpp>
#include <squared/gui/skin_loader.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace demo {

namespace {

/// "default.png" -> "default", which is how an atlas names a packed page.
std::string region_name_of(std::string_view file)
{
    const std::size_t dot = file.rfind('.');
    return std::string{dot == std::string_view::npos ? file
                                                     : file.substr(0, dot)};
}

}  // namespace

Services::Services() noexcept = default;
Services::~Services() = default;

bool Services::load_font(
    sq::app::Runtime& runtime,
    const std::string& skin_directory
)
{
    sq::files::FileHandle descriptor =
        runtime.files.internal(skin_directory + "/default.fnt");
    sq::files::FileTextResult text = descriptor.read_string();
    if (!text) {
        failure_reason_ = "default.fnt could not be read";
        return false;
    }

    sq::graphics::BitmapFont font;
    sq::graphics::BitmapFontError error;
    if (!font.load(text.text, error)) {
        failure_reason_ = "default.fnt: " + error.message;
        return false;
    }

    // A .fnt names its page as a file, but a skin packs that page into the
    // atlas under the file's stem and ships no such file. Look in the atlas
    // first; fall back to a file for a font that does ship one.
    std::vector<sq::graphics::TextureRegion> pages;
    for (const sq::graphics::BitmapFontPage& page : font.pages()) {
        const sq::graphics::AtlasRegion* packed =
            atlas_.find_region(region_name_of(page.file));
        if (packed != nullptr) {
            pages.push_back(packed->region());
            continue;
        }
        failure_reason_ = "font page not in the atlas: " + page.file;
        return false;
    }

    font_ = std::make_shared<sq::gui::FontResource>(
        "default.fnt", std::move(font), std::move(pages));
    return font_->resolved();
}

bool Services::load(
    sq::app::Runtime& runtime,
    const std::string& skin_directory
)
{
    loaded_ = false;
    failure_reason_.clear();
    skin_directory_ = skin_directory;

    if (!atlas_.load(runtime.files.internal(skin_directory + "/uiskin.atlas"))) {
        failure_reason_ = "uiskin.atlas could not be loaded";
        return false;
    }
    if (!load_font(runtime, skin_directory)) return false;

    sq::files::FileTextResult json =
        runtime.files.internal(skin_directory + "/uiskin.json").read_string();
    if (!json) {
        failure_reason_ = "uiskin.json could not be read";
        return false;
    }

    sq::gui::SkinLoadReport report;
    const sq::gui::SkinDrawableResolver drawables =
        [this](std::string_view name) {
            return sq::gui::resolve_atlas_drawable(atlas_, name);
        };
    const sq::gui::SkinFontResolver fonts =
        [this](std::string_view, std::string_view) { return font_; };

    if (!sq::gui::load_libgdx_skin(skin_, json.text, drawables, fonts, report)) {
        failure_reason_ = "uiskin.json: ";
        for (const sq::gui::SkinLoadIssue& issue : report.issues) {
            if (issue.severity == sq::gui::SkinLoadSeverity::error) {
                failure_reason_ += issue.message;
                break;
            }
        }
        return false;
    }

    if (!batch_.initialize()) {
        failure_reason_ = "the sprite batch could not be created";
        return false;
    }

    // The size here is provisional: resize() supplies the logical one before
    // the first frame. Built from the drawable only so the object exists.
    camera_ = std::make_unique<sq::graphics::OrthographicCamera>(
        static_cast<float>(runtime.graphics.pixel_width()),
        static_cast<float>(runtime.graphics.pixel_height()));

    painter_ = std::make_unique<sq::gui::BatchPainter>(batch_, font_.get());

    // Fills come from the skin's white region so they share the atlas page
    // with every other widget graphic. Without it each fill would be a texture
    // switch, and therefore a draw call.
    const sq::graphics::AtlasRegion* white = atlas_.find_region("white");
    if (!painter_->set_fill_source(white != nullptr ? &white->region()
                                                    : nullptr)) {
        failure_reason_ = "no fill source for the painter";
        return false;
    }

    loaded_ = true;
    return true;
}

void Services::resize(
    float logical_width,
    float logical_height,
    int pixel_width,
    int pixel_height
) noexcept
{
    if (camera_) {
        camera_->set_viewport(logical_width, logical_height);

        // set_viewport changes the extents and deliberately leaves the
        // position alone, so a camera built for one size keeps the centre of
        // another. Centring it here is what puts (0,0) at the top-left of the
        // screen; without it the visible window is offset by the difference
        // between the two sizes, and the whole interface sits outside it.
        camera_->set_position(logical_width * 0.5F, logical_height * 0.5F);
        camera_->update();
    }
    if (painter_) {
        painter_->set_viewport(logical_width, logical_height,
                               pixel_width, pixel_height);
    }
}

bool Services::restore(const sq::graphics::Context& graphics) noexcept
{
    // The atlas comes back first: it holds the textures the skin's drawables
    // point at. Nothing needs re-acquiring afterwards - the painter keeps a
    // pointer to the atlas region, so its fill source follows the atlas.
    if (!atlas_.restore(graphics)) return false;
    return batch_.restore(graphics);
}

}  // namespace demo
