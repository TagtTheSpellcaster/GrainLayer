# Changelog

## v1.4
- Added optional Windows startup via the current-user Run key.
- Added automatic persistence of overlay state, intensity, and tint.
- Renamed `Disable overlay` to `Toggle overlay (F6)`.

## v1.3
- Added the custom GrainLayer application/tray icon.

## v1.2.6
- Hover no longer captures or changes the slider.
- Click and click-drag are the explicit slider interactions.

## v1.2.5
- Added live slider updates during click-and-drag.
- Added mouse capture during an active drag.

## v1.2.4
- Fixed slider hit-testing by resolving the menu item's positional index before calling `GetMenuItemRect`.

## v1.2
- Replaced the separate slider popup with an owner-drawn slider embedded in the tray menu.

## v1.1
- Introduced graphical intensity control.

## v1.0
- Stabilized the Visual Studio project structure and release build configuration.

## v0.9 / v0.8
- Feature and stabilization milestones leading to the first 1.x release.

## v0.6
- Added Shell/tray support and fixed the missing Shell API header/dependency.

## v0.3
- Switched to a per-pixel alpha layered-window renderer using a 32-bit DIB and `UpdateLayeredWindow`.

## v0.1
- Initial GrainLayer project.
