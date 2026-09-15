# GrainLayer

GrainLayer is a small native Windows utility written in C++/Win32 that places a subtle paper-grain layer over the entire desktop.

It is designed to stay out of the way: the overlay is click-through, while configuration is available from the system tray.

## Features

- Subtle paper-grain overlay across the virtual desktop
- Click-through layered window using per-pixel alpha
- F6: toggle overlay
- F7/F8: increase/decrease intensity
- Live owner-drawn intensity slider in the tray menu
- Click-to-position and click-and-drag slider interaction
- Warm/tinted and paper presets
- Custom tray/application icon
- Optional Windows startup
- Automatic persistence of overlay state, intensity, and tint
- Exit: Ctrl+Alt+Shift+Q

## Building

Requirements:

- Windows
- Visual Studio 2022 with Desktop C++ workload
- Windows SDK
- x64 build tools

Open `GrainLayer.sln` and build the **Release | x64** configuration.

## Project history and guide

The `docs/` directory contains a companion guide following the project from its first working version to v1.4, with special attention to the bugs encountered during development and the reasoning behind their fixes.

## Version history

- v0.1 — initial project
- v0.2 — desktop overlay
- v0.3 — per-pixel alpha layered rendering
- v0.6 — tray integration and Win32 shell headers
- v0.8 — feature milestone
- v0.9 — stabilization
- v1.0 — first stable project structure
- v1.1 — intensity control
- v1.2 — owner-drawn slider integrated into the tray menu
- v1.2.4 — fixed menu-item position lookup for slider interaction
- v1.2.5 — live click-and-drag interaction
- v1.2.6 — natural mouse behaviour: hover does not capture or change the slider
- v1.3 — custom application/tray icon
- v1.4 — Windows startup and persistent settings; “Toggle overlay” menu wording

## License

MIT. See [LICENSE](LICENSE).
