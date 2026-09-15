# Changelog

## v1.4.3
- Added a visual indicator for the currently selected tint.
- Updated the "No tint" menu item to use the same selection indicator without a color swatch.

## v1.4.2
- Capped maximum intensity at 75%.
- F7/F8 intensity controls and the tray slider now use the same 0–75% range.
- Updated version information and credits.

## v1.4.1
- Reversed the intensity controls so F7 decreases intensity and F8 increases it, matching the left-to-right convention used by volume and brightness controls.
- The owner-drawn intensity slider now refreshes immediately when intensity is changed with F7/F8 while the tray menu is open.

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
