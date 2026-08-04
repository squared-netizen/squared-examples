# Squared Examples

Independent, complete applications built with the Squared framework and
Squared Project Generator. These projects are deliberately kept outside the
framework and generator repositories so experiments cannot change their
release code.

## Applications

### GUI Controls Gallery

`apps/gui-controls-gallery` is a handheld touch gallery for the optional
Squared GUI module. It demonstrates the CC0 Kenney skin, buttons, toggle
buttons, check boxes, sliders, labels, retained layout, application events,
and an application-owned Graphics2D painter. HoloDisk is not enabled.

Build it in Termux:

```sh
cd "$HOME/projects/squared-examples/apps/gui-controls-gallery" || exit 1
squared-pg project verify .
squared-pg project build .
```

The APK is written to:

```text
/sdcard/Download/squared_gui_controls_gallery-debug.apk
```

## Repository model

Each directory beneath `apps/` is a complete generated project with immutable
module versions recorded by `squared-pg`. Build products and the local SDL2 kit
remain untracked. Every application can evolve independently; framework and
generator changes happen only in their own repositories.
