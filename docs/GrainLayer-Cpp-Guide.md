# GrainLayer: a C++/Win32 case study from v0.1 to v1.4

## Why this guide exists

This is not a tutorial that pretends a program appears fully formed. GrainLayer was built incrementally, and the most useful lessons often came from the failures between one version and the next.

The guide therefore follows the actual evolution of the program and treats bugs as part of the learning process: symptom → hypothesis → diagnosis → fix → lesson.

## 1. Starting point: a native Windows program

GrainLayer is a native C++ Win32 application. The program does not need a conventional visible main window: its persistent user interface is the system-tray icon and its popup menu, while the desktop effect is provided by a transparent layered window.

Important concepts introduced at the beginning:

- Win32 window classes and message loops
- `HWND`, `HDC`, `HBITMAP` and other Windows handles
- `WndProc` and Windows messages
- Unicode APIs (`W` suffix)
- compile-time constants for command identifiers
- resource files for application icons

## 2. v0.1 → v0.2: creating the overlay

The first milestone was simply getting a native application running and then turning it into a desktop overlay.

The key architectural decision was to make the overlay window topmost and click-through. The window is not meant to receive user interaction; it is a visual layer placed above other windows.

This introduces a useful Win32 distinction: a window can be visible without being an interactive surface.

## 3. v0.3: the black-overlay bug

### Symptom

The overlay appeared as a solid black layer rather than a subtle paper texture.

### Diagnosis

The first rendering approach did not provide the per-pixel alpha information required for a translucent layered window. A normal window background and a layered window using `UpdateLayeredWindow` are not interchangeable rendering models.

### Fix

GrainLayer switched to:

1. a 32-bit DIB section;
2. a memory device context;
3. per-pixel RGBA data;
4. premultiplied RGB values;
5. `UpdateLayeredWindow(..., ULW_ALPHA)`.

The source pixels are explicitly premultiplied before presentation. This is essential because the blend operation uses `AC_SRC_ALPHA`.

### Lesson

When debugging graphics, distinguish between:

- the bitmap format;
- the alpha representation;
- the window composition mechanism.

A bitmap that *contains* an alpha byte is not automatically being composited as a translucent window.

## 4. v0.5/v0.6: the missing header bug

### Symptom

Compilation failed around Shell/tray functionality because the required declarations were not available.

### Fix

`<shellapi.h>` was added and the corresponding Shell API dependency was linked.

### Lesson

In Win32, API families are exposed through specific headers and libraries. When a symbol is unknown, first check whether the declaration's header and library dependency are both present.

## 5. v1.0: project metadata and Visual Studio

At one point Visual Studio reported that one or more projects in the solution could not be loaded correctly.

The project files were rebuilt using a conventional `.sln`/`.vcxproj` structure, explicit x64 configurations, the v143 toolset, Windows SDK settings, standard C++ property imports, and a normal resource compilation step.

### Lesson

A C++ program and a Visual Studio project are separate layers. Source code can be perfectly valid while the build system metadata is broken.

## 6. v1.1: from keyboard control to a real slider

Intensity initially had keyboard controls. The next step was a graphical control.

A first attempt used a separate popup window with a standard trackbar. It worked technically but was rejected because it was cumbersome for a tiny tray utility.

The design was therefore changed to an owner-drawn item embedded directly inside the popup menu.

This is a useful example of an engineering decision driven by interaction design rather than API convenience.

## 7. v1.2: owner-drawn menu controls

The slider became an `MFT_OWNERDRAW` menu item.

Two message types are central:

- `WM_MEASUREITEM` determines the item's size;
- `WM_DRAWITEM` renders the slider.

The slider is therefore not a standard Windows trackbar. It is a visual control drawn inside a standard popup menu.

## 8. The `GetMenuItemRect` bug

### Symptom

The slider was visible and responded visually to hover, but clicking and dragging did nothing.

### Diagnosis

The critical detail was the `uItem` parameter of `GetMenuItemRect`. It expects the **zero-based menu position**, not the command ID.

The slider's command ID was `1006`. Passing `1006` as the menu position therefore failed.

### Fix

GrainLayer now walks the menu positions with `GetMenuItemCount` and `GetMenuItemInfoW(..., MIIM_ID)` until it finds the item whose `wID` is `IDM_INTENSITY_SLIDER`. Only then is `GetMenuItemRect` called with the actual position.

### Lesson

Win32 APIs frequently distinguish between an item's identity and its position. Similar-looking parameters are not interchangeable merely because both are integers.

This was one of the most instructive bugs in the project.

## 9. Mouse input inside `TrackPopupMenu`

A standard popup menu is modal while `TrackPopupMenu` is running. Ordinary application-level mouse handling is therefore not enough for a custom interactive menu item.

GrainLayer uses a thread `WH_MSGFILTER` hook while the tray menu is active. The hook receives menu messages and can inspect `WM_LBUTTONDOWN`, `WM_MOUSEMOVE`, and `WM_LBUTTONUP`.

The relevant state is deliberately small:

- active menu handle;
- hook handle;
- slider-dragging flag.

## 10. v1.2.5: live drag

### Symptom

Click-and-drag could start, but the visual position did not update naturally while dragging.

### Fix

During a drag, the menu window captures the mouse. Each movement updates the intensity, redraws the layered desktop window, invalidates the popup menu, and saves the new value.

### Lesson

Interactive custom controls need both input state and visual invalidation. Changing the underlying value is not enough: the owner-drawn surface must also be repainted.

## 11. v1.2.6: hover versus drag

The first working drag implementation made the slider too eager: mere mouse movement could effectively take control of the interaction.

The final behaviour is intentionally Windows-like:

- hover does nothing;
- left click positions the slider;
- holding the left button starts a drag;
- movement updates the slider live;
- release ends the drag.

This small change illustrates a broader UI lesson: technically possible input behaviour is not necessarily natural input behaviour.

## 12. v1.3: resources and the application icon

The generic Windows application icon was replaced with a dedicated GrainLayer icon.

The icon is compiled as a Windows resource through `GrainLayer.rc` and loaded with `LoadIconW` using the application resource identifier.

This is the first point where the project visibly becomes its own application rather than a generic Win32 executable.

## 13. v1.4: persistent settings

GrainLayer now saves settings automatically in:

`%APPDATA%\GrainLayer\settings.ini`

The persisted values are:

- intensity;
- overlay enabled/disabled state;
- selected tint.

The settings are loaded during startup and saved whenever one of these values changes.

### Lesson

For a small utility, persistence does not require a large configuration subsystem. A simple INI file is sufficient when the data model is small and human-readable storage is useful.

## 14. v1.4: Open at startup

The startup option uses the current-user Windows Run key:

`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`

When enabled, GrainLayer stores its executable path under the `GrainLayer` value. When disabled, that value is removed.

Using `HKCU` means the feature does not require administrator privileges.

## 15. v1.4: final menu terminology

The former menu label `Disable overlay` became `Toggle overlay (F6)`.

This is deliberately more accurate: the command is a toggle, so its name describes the operation rather than only one of its two possible outcomes.

## 16. What the final program teaches

The most useful C++ lessons in GrainLayer are not isolated syntax rules. They are the relationships between layers of a real application:

- C++ types interact with Win32 types (`int` versus `LONG` was enough to break `std::clamp` in one iteration).
- Window composition depends on the exact alpha model.
- Menus have both command IDs and positional indexes.
- Modal Win32 UI changes how input messages must be intercepted.
- Owner-drawn controls require explicit measuring and repainting.
- Windows resources are part of the executable's identity.
- Small applications still benefit from explicit state management and persistence.

## 17. Bug diary

| Version | Symptom | Root cause | Fix |
|---|---|---|---|
| v0.3 | Overlay was black/opaque | Incorrect layered-window alpha model | 32-bit DIB + premultiplied alpha + `UpdateLayeredWindow` |
| v0.5/v0.6 | Shell/tray compilation failure | Missing Shell API declarations/dependency | Add `shellapi.h` and Shell library |
| v1.0 | Solution/project failed to load | Broken/non-standard project metadata | Rebuild `.sln`/`.vcxproj` structure |
| v1.2.1 | `std::clamp` compilation error | `LONG`/`int` type mismatch | Explicit conversion/bounds handling |
| v1.2.2 | Cascading C++ syntax errors | Accidental duplicated `{` | Remove extra brace |
| v1.2.3/v1.2.4 | Slider did not respond | Menu command ID passed as menu position | Resolve actual menu position first |
| v1.2.5 | Drag did not repaint naturally | Mouse capture/repaint state incomplete | Capture during drag + invalidate/update menu |
| v1.2.6 | Hover felt like mouse capture | Input handled outside explicit drag state | Only capture/update after left-button press |

## 18. A final principle

GrainLayer is small enough to understand as a whole, but large enough to expose the realities of native Windows programming.

That makes it a useful learning project: the code is the result of a sequence of concrete decisions, and the bugs are not embarrassing exceptions to the process. They are part of the process.
