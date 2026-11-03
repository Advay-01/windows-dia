# Sidebar and frame appearance

Right-click the browser sidebar and choose **Themes**. The compact picker has a
dotted color canvas, five pages of swatches, an opacity wave and a grain dial.
Choosing a color applies it to the sidebar and frame immediately.

- Drag the large dot to choose a color. The smaller dots follow a color harmony.
- Use **+** and **−** for up to three colors. Removing the last color restores Dia.
- The three-circle button cycles complementary, analogous or triadic arrangements.
- Double-click the large dot for exact RGB and hex entry. With keyboard focus on
  the dot, arrow keys move it and Enter or Space opens exact color entry.
- The sparkle, sun and moon choose Dia's original, light and dark appearance.
- The arrows page through light, dark, harmonized and grayscale swatches.
- Drag the opacity wave or the sixteen-position grain dial. Both accept arrow
  keys when focused. These controls are separate from the Patches page.

The controls are inspired by Zen's theme picker. This implementation uses native
WinUI controls and generates its own color and grain pixels. It does not ship
Zen's JavaScript, CSS, images or haptic feedback implementation.

The menu discovers Dia's native sidebar MenuFlyoutPresenter using five fixed
command labels. It inserts Themes after Auto-Hide Tabs, before the final
separator and Close all tabs. It never assigns a replacement sidebar ContextFlyout.
It retains the native item collection and removes only its own item on disconnect.
The editor remains alive independently when the native menu closes.

WinUI connects MenuFlyout.Items to the presenter's ItemsSource; see Microsoft's
[MenuFlyout implementation](https://github.com/microsoft/microsoft-ui-xaml/blob/main/dxaml/xcp/dxaml/lib/MenuFlyout_Partial.cpp).
Only a literal, typed collection with the expected command signature is eligible.
Tab, page and unrelated menus do not qualify. English labels are currently required.

The adapter replaces eligible literal Background brushes on the browser's
SidebarBorder, SidebarFloatingSurface and BackgroundSurface borders. It also
colors the frame grid directly under ClientAreaPresenter, which paints the thin
margins around web content. It stays
within browser XAML roots, preserves bindings and animations, regenerates the
brush on resize or appearance changes, and disables custom backgrounds in system
high contrast. This colors the browser chrome; it does not add behind-window blur
or change webpage colors. Opacity reveals the surface behind each border.
Light/dark mode overrides an eligible RequestedTheme property on the browser
root; choosing the sparkle restores Dia's original value. It leaves bindings
untouched if Dia controls that property itself.

Removing all colors, resetting patches or disconnecting restores the original
brushes and requested appearance, unless Dia has replaced them since the override.
The enabled state uses the launcher's existing preferences.ini synchronization.
Opacity (0–100), Texture (0–15), and Canvas are saved separately in
`%LOCALAPPDATA%\DiaTweaks\appearance.ini`, in the `[Theme]` section, and read when
the add-on first loads. With the Microsoft Store build of Dia, Windows redirects
this file to `%LOCALAPPDATA%\Packages\TheBrowserCompany.Dia_ttt1ap7aakyb4\LocalCache\Local\DiaTweaks\appearance.ini`.
Editing that file externally requires restarting Dia. Canvas stores the primary
dot coordinates, lightness, color count, harmony, mode, explicit-lightness flag,
retained hue and grayscale flag. The retained hue preserves fully saturated RGB
colors at the center of the canvas. A saved legacy Color value is migrated when
there is no Canvas value.
Dragging updates the in-memory theme and uses a lower-resolution background
preview. Releasing the pointer saves the theme and restores full-resolution
grain. Color swatches have room around their circular artwork so native button
layout does not clip the edges. Dot transitions use an explicit 400 ms duration
and respect Windows' animation setting.
Dot movement uses render transforms rather than changing layout positions;
grabbing a dot retains the pointer's offset within it. The panel hides its
vertical scrollbar while retaining wheel scrolling on smaller screens. Pointer
clicks do not show the bottom controls' focus outlines; keyboard focus does.
The panel artwork follows the selected light/dark appearance.
Action icons use a fixed 24-unit drawing box, including the horizontal minus;
palette arrows use matching vector chevrons. Button hover highlights fade over
120 ms without changing their layout. Unsupported background properties are
cached until reset rather than requesting their diagnostics on every UI tick.
Frame discovery also uses SharedSurface so it can survive a hidden sidebar.
The dotted canvas has stronger contrast against the outer panel in both light
and dark modes. Its 18-unit corner radius and 10-unit inset share corner centers
with the panel's 28-unit outer radius. The presenter uses the same outer radius
and has no border stroke. Grain uses decorrelated, zero-centered samples at the display's
physical pixel scale, with up to 4096 pixels per dimension. The temporary color
preview omits grain rather than stretching coarse noise. Noise tests check its
mean, variance, amplitude and correlation between adjacent pixels.
The opacity wave reuses its curve segments and keeps fixed layout bounds. Its
pill scales and translates through a render transform, with sub-percent visual
steps. The brush opacity changes immediately without regenerating its pixels.
Grain updates reuse cached gradient pixels; the dial tick rotates without
relayout. Both bottom controls defer preference writes until pointer release or
capture loss, while keyboard changes are saved immediately.

The portable tests cover dial detents, wraparound, the center dead zone, exact
RGB round-trips, all preset colors, color harmonies, invalid inputs and gradients.
On Dia 1.4.0.424 x64, the initial controls loaded in Settings and the native adapter read
back the applied brushes. The sidebar and frame changes, color/opacity/texture
controls, and restoration of both backgrounds with the switch Off were confirmed
in a live session. The separate Themes menu and RGB picker were also confirmed
working. Themes was then inserted into Dia's native menu below Auto-Hide Tabs.
The first click did not open the editor. Opening from the existing UI timer after
the native menu closes fixed that: in fresh process 27840 the probe reported
`Themes panel shown`, with a loaded 300x498.4 ColorPicker, 240x40 opacity control
and 88x88 grain dial. The editor survives removal of the closed menu's presenter.
Opening the editor from the same native sidebar menu was visually confirmed.
The compact canvas redesign was then checked in fresh processes 3860, 22880,
8884 and 19432. Swatch clipping, direct-dot dragging, movement smoothness,
scrollbar visibility, pointer outlines, light/dark panel appearance and gradient
color on the thin frame were corrected and visually confirmed.
The later updates also confirmed alignment, hover, contrast, fine grain, frame
color with the sidebar hidden, and smooth bottom controls. Restart persistence,
high contrast, broader appearance combinations, multiple windows and top tabs
still need visual checks.

The pending performance revision caches the exact RGB flyout and combines color
events into 33 ms previews. These previews use the existing low-resolution,
grain-free theme renderer and avoid preference writes. After 160 ms of idle time,
or when the flyout closes, the latest color is saved and rendered at full quality.
Native active/inactive caption-button backgrounds receive a separate neutral
surface while Themes is enabled, with their previous values restored on disable.
Hover/pressed caption-button colors remain native. These changes need live checks
for RGB interaction speed, final color persistence and caption appearance during
focus, resize and light/dark transitions. The combined build loaded in Dia process
14500 and all seven automated checks passed. The user confirmed the RGB picker
is smoother and the caption buttons stay neutral after the update. Focus, resize,
light/dark combinations and final-color persistence still need broader checks.

## Active-tab glass prototype

When Themes is enabled, the sidebar's selected tab receives a subtle translucent
fill in place of its opaque neutral fill. The adapter uses Dia's
native selected-tab state and only considers neutral, row-sized fills within
that tab. It preserves native tab geometry, labels, favicon, close button,
selection and input handlers. Switching tabs releases the old override, and
disabling Themes restores original literal values. Bound or unsupported fills
are left alone. The native selection border remains in place.

The selected shape uses a translucent white sheen in dark appearance, or a subtle
black tint in light appearance. This preserves the color flow without putting an
acrylic effect on Dia's vector path. Diagnostics report selected-tab and overridden
surface counts without tab titles or URLs. This revision is awaiting a live
check with tab switching and Themes on/off; top-tab layouts are not covered by
the current tab-list adapter.
In process 24628, diagnostics found one selected tab but zero matching surfaces.
The native tab canvas contains vector Paths with zero layout size. The follow-up
checks Path geometry bounds and Fill, in addition to ordinary panel backgrounds;
in process 29860 diagnostics report one selected tab, one overridden surface and
zero unsupported properties. Its appearance, switching behavior and restoration
still need user verification. All eight automated checks passed before attachment.
The user reported the highlight or controls looked wrong in that build. The next
revision replaces the path's AcrylicBrush with a plain translucent SolidColorBrush,
retaining its native geometry and input behavior; appearance and switching remain
pending live verification.
That correction loaded in Dia process 22712 with all eight checks passing and
one selected-tab fill overridden, with no unsupported properties reported.
