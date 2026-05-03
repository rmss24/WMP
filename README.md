# Addiction to White Monster is a problem

A Windows desktop overlay with a configurable crosshair, screen border, text HUD, and animated GIF character. The project also includes an Electron/React settings app so the overlay can be configured without editing JSON by hand.

## Features

- Always-on-top transparent Windows overlay.
- Click-through mode so mouse input passes to the app underneath.
- Configurable crosshair: color, length, gap, and thickness.
- Configurable screen border: color and thickness.
- Optional HUD with quick shortcut reminders.
- Animated GIF character with configurable screen anchoring and fine-tuned offsets.
- Per-character walking GIFs, with automatic horizontal mirroring when walking left.
- Optional walking animation loop with configurable frequency, direction, and speed.
- Settings app with character, display, animation, and system controls.
- Shared configuration through `config.json`.
- Optional user-level Windows autostart through the registry.
- NSIS installer build through Electron Builder.

## Project Structure

```text
.
|-- config.json
|-- config.installer.json
|-- launch-settings.bat
|-- characters/
|   `-- frieren/
|       `-- frieren.gif
|-- overlay/
|   |-- main.cpp
|   |-- CMakeLists.txt
|   |-- build_msvc.bat
|   |-- build_mingw.bat
|   |-- replace.bat
|   |-- json.hpp
|   |-- overlay.exe
|   `-- overlay_static.exe
`-- settings-ui/
    |-- package.json
    |-- electron.vite.config.ts
    |-- src/main/
    |-- src/preload/
    `-- src/renderer/
```

## Requirements

- Windows.
- Node.js and npm for the settings app.
- Electron dependencies installed through npm.
- One C++17 toolchain for the overlay:
  - Visual Studio Build Tools / Visual Studio Developer Command Prompt,
  - MinGW with `g++` available in `PATH`, or
  - CMake 3.15+ with a C++17 compiler.

## Setup

Install the settings app dependencies:

```bat
cd settings-ui
npm install
```

If `node_modules` already exists, this step may already be done.

## Quick Start

From the repository root, open the settings app with:

```bat
launch-settings.bat
```

Start the overlay directly without automatically opening the settings app:

```bat
overlay\overlay.exe --no-ui
```

In development, the settings app looks for the overlay executable in this order:

```text
overlay\overlay_new.exe
overlay\overlay_static.exe
overlay\overlay.exe
```

## Settings App Development

Run the Electron/React app in development mode:

```bat
cd settings-ui
npm run dev
```

Build the app:

```bat
npm run build
```

Preview the built app:

```bat
npm run start
```

The settings app uses Electron, React, and TypeScript. The Electron main process reads and writes `config.json`, exposes IPC APIs through the preload script, lists GIF characters, manages autostart, and can launch the overlay.

## Overlay Build

### MSVC

Open a Visual Studio Developer Command Prompt in the `overlay` folder and run:

```bat
build_msvc.bat
```

### MinGW

With `g++` available in `PATH`:

```bat
cd overlay
build_mingw.bat
```

### CMake

```bat
cd overlay
cmake -S . -B build
cmake --build build --config Release
```

The CMake build writes the executable to the CMake build directory.

## Installer Build

The installer is built from the `settings-ui` package:

```bat
cd settings-ui
npm run dist
```

Electron Builder writes the installer output to:

```text
dist-installer/
```

The packaged app includes:

- `overlay\overlay_static.exe` as `overlay.exe`.
- The `characters/` folder.
- `config.installer.json` copied as `config.json`.

Before building the installer, make sure `overlay\overlay_static.exe` exists and is up to date.

## Configuration

Runtime settings are stored in `config.json` during development. The installer uses `config.installer.json` as the packaged default configuration.

Example:

```json
{
  "version": 1,
  "autoStart": false,
  "debugMode": false,
  "launchSettingsOnStart": true,
  "character": {
    "enabled": true,
    "path": "characters\\frieren\\frieren.gif",
    "maxWidth": 260,
    "maxHeight": 260,
    "margin": 28,
    "anchorX": "right",
    "anchorY": "bottom",
    "offsetX": 0,
    "offsetY": 0
  },
  "walkPaths": {
    "characters\\frieren\\frieren.gif": "characters\\frieren\\frieren-walk.gif"
  },
  "animation": {
    "enabled": false,
    "walkFrequency": 30,
    "walkDirection": "both",
    "walkSpeed": 150
  },
  "crosshair": {
    "enabled": true,
    "length": 18,
    "gap": 5,
    "thickness": 2,
    "color": "#00FF50"
  },
  "border": {
    "enabled": true,
    "thickness": 3,
    "color": "#FF3232"
  },
  "hud": {
    "enabled": true,
    "fontSize": 16
  },
  "overlay": {
    "visible": true,
    "clickThrough": true
  }
}
```

Main fields:

- `autoStart`: reflects whether Windows autostart is enabled.
- `debugMode`: shows extra debug information in the HUD.
- `launchSettingsOnStart`: opens the settings app when `overlay.exe` starts.
- `character.enabled`: shows or hides the GIF character.
- `character.path`: absolute path or path relative to the project/resources root.
- `character.maxWidth` / `character.maxHeight`: maximum GIF size in pixels.
- `character.margin`: distance from the bottom and right screen edges.
- `character.anchorX`: horizontal anchor, one of `left`, `center`, or `right`.
- `character.anchorY`: vertical anchor, one of `top`, `center`, or `bottom`.
- `character.offsetX` / `character.offsetY`: pixel offsets from the selected anchor.
- `walkPaths`: maps each idle character GIF path to its optional walking GIF path.
- `animation.enabled`: enables or disables automatic walking cycles.
- `animation.walkFrequency`: seconds between walking cycles.
- `animation.walkDirection`: walking direction, one of `left`, `right`, or `both`.
- `animation.walkSpeed`: movement speed in pixels per second.
- `crosshair`: crosshair visibility, geometry, and color.
- `border`: screen border visibility, thickness, and color.
- `hud.enabled`: shows or hides the HUD text.
- `overlay.visible`: initial overlay visibility.
- `overlay.clickThrough`: initial click-through state.

## GIF Characters

Characters are discovered inside `characters/`, with one folder per character:

```text
characters/
`-- character-name/
    `-- animation.gif
```

The settings app automatically lists GIFs found there. You can also select an external GIF from the character page.

Each character card has a menu button for walking animations:

- `Set walk GIF`: assign a separate GIF used while the character walks.
- `Change walk GIF`: replace an existing walking GIF.
- `Remove walk GIF`: clear the walking GIF for that character.

Cards with a configured walking GIF show a running badge.

## Character Positioning

The character position is controlled by an anchor plus optional offsets. The anchor can be any of the 9 screen positions:

```text
top-left      top-center      top-right
center-left   center          center-right
bottom-left   bottom-center   bottom-right
```

`anchorX` selects `left`, `center`, or `right`; `anchorY` selects `top`, `center`, or `bottom`. `offsetX` and `offsetY` then move the character from that anchor, which is useful for small layout adjustments without changing the anchor itself.

In the settings app, the Animations page exposes this as a 3x3 anchor grid plus Offset X/Y sliders.

## Walking Animation

When walking animation is enabled, the overlay periodically starts a full walk cycle:

```text
IDLE -> WALK_OUT -> WALK_BACK -> IDLE
```

During `WALK_OUT`, the character moves from its anchor toward the selected screen edge. During `WALK_BACK`, it returns to the configured anchor. Movement is updated by the overlay timer at roughly 60 FPS for smooth motion.

Walking behavior is controlled from the Animations page:

- Enable or disable walking.
- Set the frequency from 5 to 300 seconds.
- Set movement speed in pixels per second.
- Pick left, right, or alternating left/right direction.

If a character has a configured walk GIF, that GIF is used during the walking phases. When the character walks left, the overlay mirrors the GIF horizontally, so a separate left-facing GIF is not required.

## Shortcuts

When the overlay is running:

- `Ctrl+Shift+H`: show or hide the overlay.
- `Ctrl+Shift+T`: toggle click-through mode.
- `Ctrl+Shift+S`: open the settings app.
- `Ctrl+Shift+Q`: quit the overlay.

When visibility or click-through is changed with a shortcut, the overlay saves the new state to `config.json` so the settings app stays in sync.

## Autostart

The System page can enable or disable user-level autostart through:

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Run
```

The registry value is named `WMP` and points to the overlay executable found by the settings app. In a packaged build, that is the `overlay.exe` file inside the app resources directory.

## Technical Notes

- The overlay uses GDI+ to load and draw animated GIFs.
- Transparency is implemented with a near-black color key: `RGB(1, 1, 1)`.
- `launch-settings.bat` sets `OVERLAY_ROOT` so the settings app can find `config.json`, `characters/`, and `overlay/` from the repository root.
- `json.hpp` is the header-only nlohmann/json library used by the C++ overlay.

## Troubleshooting

### The settings app does not find characters

Start it through `launch-settings.bat`, or set `OVERLAY_ROOT` to the repository root.

### The "Launch Overlay" button does nothing

Make sure one of these files exists:

```text
overlay\overlay_new.exe
overlay\overlay_static.exe
overlay\overlay.exe
```

Then try launching the overlay again from the settings app.

### The installer build fails because the overlay is missing

Build or copy the static overlay executable to:

```text
overlay\overlay_static.exe
```

Then run `npm run dist` again from `settings-ui`.

### The overlay blocks clicks

Press `Ctrl+Shift+T` to toggle click-through mode, or enable click-through from the System page.

### Changes do not appear immediately

The overlay watches `config.json` and applies changes automatically. If something gets stuck, quit the overlay with `Ctrl+Shift+Q` and start it again.
