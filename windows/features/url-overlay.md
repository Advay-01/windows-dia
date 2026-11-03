# Dia URL/search overlay prototype

This is a WinUI 3 popup inside the running Dia process, hosted by the existing
DiaTweaks add-on. It is a Dia component, not a desktop window switcher or a
separate Electron app. It offers URL entry, web search, Dia tabs and Top sites.

## Use

1. Install Dia and run the Windows DiaTweaks launcher.
2. Press **Alt+Space**. If that combination is occupied, the prototype tries
   **Ctrl+Alt+Space**. Diagnostics report which shortcut registered.
3. Type `example.com` for **Open in Dia** plus a search alternative, or type
   ordinary words for **Search in Dia**.
4. Use Up/Down or Tab/Shift+Tab to select; Enter or clicking a row activates it.
5. Escape or the hotkey closes it with a short fade. Clicking outside dismisses it.

The global shortcut brings Dia forward and opens the popup over its client area.
The input clears and receives focus on opening. Navigation uses Windows protocol
activation with Dia's package family explicitly selected, so it targets Dia rather
than the Windows default browser. Search text is URI-encoded. Direct navigation rejects credentials
and non-HTTP(S) schemes; those inputs become search text instead.

With no query, the popup shows just the search field. Typing shows matching
Top sites and matches from up to three recent open tabs. Tabs come from Dia's
current native tab list, including its dock when the controls expose names.
Selection uses native selection/invoke providers or the tab model's selection
command. A tab that no longer exposes an action reports an error instead of
opening a duplicate. Switching tabs was verified live in the sidebar layout;
virtualized off-screen tabs and other Dia windows are not yet covered.
Recent order follows observed native tab selection and switches made through
the popup. Before those observations exist, tabs fall back to Dia's list order;
pre-session tab focus history is not available from this adapter.

Top sites reads the last-used Dia profile's History database on a worker thread,
groups HTTP(S) pages by domain and adds their visit counts. It displays the six
most visited domains, using the most visited page from each as its target. Hidden
pages and URLs containing credentials are excluded. Cached icons come from the
same profile's Favicons database; no favicon service is contacted. The original
databases are never opened for writing. Short-lived snapshots are removed after
reading; the result cache stays in memory for one minute. Diagnostics contain
counts, not page titles, URLs or search text. Typing also matches the collected
tabs and Top sites locally, while keeping URL/search actions first.

It does not enumerate desktop windows for search results. It registers a normal
Windows hotkey and does not hook or replace Alt+Tab. Closing the add-on unregisters
the shortcut and removes the popup.

## Appearance

The add-on creates `%LOCALAPPDATA%\DiaTweaks\url-overlay.css` and checks it for
changes every half second. Inside the packaged Dia process Windows normally
redirects this to `%LOCALAPPDATA%\Packages\TheBrowserCompany.Dia_ttt1ap7aakyb4\LocalCache\Local\DiaTweaks\url-overlay.css`.
Edit the existing file; saving applies supported variables without a restart.
This is a small variable parser, not a web rendering engine.

```css
:root {
  --width: 680px;
  --radius: 16px;
  --blur: system; /* system, acrylic, in-app or none; reopen the popup after changing modes */
  --tint: #92979d;
  --tint-opacity: 0.60;
  --luminosity-opacity: 0.10;
  --border-opacity: 0.10;
  --selection-opacity: 0.09;
  --secondary-opacity: 0.50;
  --accent: #ffffff;
}
```

The search panel uses WinUI Desktop Acrylic in a windowed popup, with a light
tint and a faint outline. WinUI clips its acrylic to the rounded panel bounds.
The `system` and `acrylic` modes use this backdrop. The `in-app` mode uses the
XAML compositor blur and can fall back to WinUI's acrylic brush.
The contrast check adapts only text to the rendered panel.

The compact panel uses a denser tint in the reading area in in-app mode,
a 16-unit corner radius,
a subtle border, a 56-unit input section, divider and 46-unit result
row with a moving selection highlight. Opening uses 150 ms opacity/scale animation; explicit closing uses 120 ms.
Windows' reduced-motion preference disables these animations. Popup light-dismiss
handles outside clicks. Its root selection is a single-window prototype;
multiple Dia windows, top tabs and minimized-window activation need live checks.

## Code structure

```text
features/url_overlay.h/.cpp   native popup, input/results, animations and hotkey
features/url_logic.h/.cpp     safe URL classification and encoded search actions
features/url_style.h/.cpp     visual tokens and live CSS-variable subset
features/url_tabs.h/.cpp      native Dia tab discovery and activation
features/url_sites.h/.cpp     read-only history snapshots and cached favicons
tap/session.h                UI-thread overlay lifetime
tap/features.cpp             discovery and cleanup in the existing host
tap/tree.cpp                 WM_HOTKEY dispatch and status diagnostics
tests/url_tests.cpp          URL and scheme/encoding checks
tests/url_data_tests.cpp     synthetic history ranking and live-style checks
```

## Build and run from source

Install Visual Studio C++ Build Tools with the Windows SDK and CMake. From the
repository root:

```powershell
./windows/build-settings.ps1 -Configuration Release
./windows/build/settings/Release/DiaTweaks.exe
```

The script discovers the installed Dia package and generates the WinUI projections
used by this repository. The packaged launcher needs no SDK or terminal. Native
DLL changes require fully quitting Dia before replacing the add-on.

For the existing development build:

```powershell
cmake --build windows/build/reviewer --config Release
ctest --test-dir windows/build/reviewer -C Release --output-on-failure
```

The `url-tests` check covers bare domains, explicit HTTP URLs, localhost ports,
query encoding, whitespace, empty input and unsafe-scheme/credential handling.
On Windows 11 with Dia 1.4.0.424 x64, the popup appeared inside Dia and registered
Alt+Space. The first executable-alias handoff failed; the revised build uses
Dia-targeted protocol activation and handles Enter on the input's preview event.
In fresh process 25188, search activation was confirmed working through both
Enter and clicking a result. All six automated checks passed before attachment.
The history/style tests pass, including visit aggregation, filtering and unchanged
source database bytes. A local history read returned six sites without logging
their browsing data. The refreshed build loaded in Dia process 2268 and all seven
checks passed. The populated sidebar reported nine tabs with native selection
actions and six Top sites. The user confirmed both empty-state sections show
entries, switching to another tab works, and opening a Top site works in Dia.
The compact revision loaded in Dia process 4368 with all seven checks passing.
The user confirmed the smaller layout and logos look correct. It uses a 680-unit
width, 56-unit search header, 46-unit rows and at most three recent tab results.
Recent-tab ordering across native tab switches, glass transparency and animation
still need focused live review.
The next visual revision hides row arrows until hover or explicit keyboard
selection, with a 140 ms fade/slide in and 100 ms exit. Arrow space stays reserved
to prevent text movement. Icons and the search field share a 24-unit left inset;
action labels use a fixed right-aligned column. That build loaded in Dia process
14500 with all seven automated checks passing. The follow-up removes persistent
labels and boxed arrow backgrounds: a short action label plus a plain vector
arrow reveals only for hover or explicit keyboard selection. The header uses a
vector document icon with vertically centered input content. These alignment,
action and compositor-backdrop changes loaded in Dia process 18200 with all seven
automated checks passing. Their visual/tab-to-tab checks are pending.
The user confirmed glass looks consistent on New Tab and a regular webpage.
Diagnostics at that check reported `material=disconnected`, so an active native
acrylic controller is not yet independently verified. The following revision
replaces the document glyph with a magnifying glass, centers TextBox's internal
ContentElement and placeholder template parts, and uses black primary text with
muted black secondary text. Header alignment and light-background readability
await live verification of this revision. It loaded in Dia process 27220 with all
seven automated checks passing.

## Automatic contrast

Automatic contrast follows the rendered glass brightness. Light glass uses black
text; dark glass uses white. It updates input, placeholders, section headings,
secondary text and hover-action labels through shared brushes. Icons keep a fixed
neutral gray. The material theme, tint, opacity, borders, selection highlight and
Dia page theme remain unchanged when the text color switches.

While visible, a worker samples the glass's empty side insets every 350 ms.
It measures the input header separately so the placeholder and typed text stay
readable when the top and bottom of the panel have different brightness. It
avoids content and text, rejects points owned by another window, and stops
reading when Dia loses foreground.
Only median relative luminance is passed back to the UI. No images are made,
written or transmitted, and no raw pixels or page content enter diagnostics.
The initial reading chooses the higher-contrast text color. Hysteresis and two
consistent subsequent readings prevent rapid flips on mixed backgrounds.

If fewer than three points can be read, it keeps the last valid color; before a
valid reading it uses Dia's current light/dark appearance. The actual glass is
sampled so a white webpage cannot cause black text on an opaque dark fallback.
This is a surface brightness estimate, not a DOM/CSS theme detector. HDR and
unusual DPI arrangements need live checks.
`url-contrast-tests` covers luminance, mixed-background medians, unavailable data,
light/dark changes and hysteresis. Live page-to-page verification is pending.
The first combined build loaded in Dia process 24628 with all eight checks passing.
The user initially confirmed both colors switch, then reported that the background
also changed. The correction removes RequestedTheme and background/highlight
updates from contrast handling. The correction loaded in Dia process 29860 with
all eight checks passing; foreground-only visual verification is pending.
The user reported lost glass and supplied a solid-black popup screenshot. The
next revision restores the in-app brush and constrained popup, while retaining
foreground-only contrast. It loaded in Dia process 22712 with all eight automated
checks passing. Diagnostics report the in-app brush is configured; actual glass
appearance and foreground-only adaptation remain pending user verification.
The user's three screenshots then confirmed the in-app material still differed
between New Tab and web surfaces, and black text could land on dark glass.
The unified native Flyout host and rendered-glass contrast revision loaded in
process 15144 with all eight checks passing. Its controller connection, placement
and three-background visual check are pending; packaging remains on hold.
Opening failed in that live build with HRESULT `0x800f0902` before a host was
retained. The next build makes optional flyout settings nonfatal, records the
failing operation and native error message, and tries the built-in desktop acrylic
if creating/attaching the custom backdrop is rejected. A built-in fallback is
reported separately and does not imply the custom tint settings were applied.
Opening and glass appearance remain unverified until that build is loaded.

For native opening diagnostics, the developer controller accepts `overlay ID`,
where ID is a SharedSurface node from `tree SharedSurface`. It toggles this
component on its owning UI thread and returns its status. Other node roles are
rejected. This lets a developer check opening failures without requiring the
tester to press the shortcut for every diagnostic attempt.
In process 22924, `overlay 17` reproduced `0x800f0902` at creation of the Flyout
presenter style. The next revision removes that XamlReader style path and applies
optional presenter properties directly after its native presenter exists.
In process 25756 the same HRESULT occurred later with the last constructor-stage
label, so that label did not isolate the actual failing operation. The follow-up
removes runtime XAML parsing from header/action glyphs and result-button templates,
uses native vector primitives and Button styling, and labels each construction
phase separately. That follow-up is compiled but not yet verified in the live host.
In process 20592 the diagnostic isolated the remaining failure to search-input
initialization. The next compiled revision moves optional TextBox resource styling
into a guarded Loaded handler and distinguishes construction, configuration,
event hookup and attachment. Opening remains unverified.
This refinement has not been packaged; packaging is on hold for user approval.
The global shortcut from other apps, multi-window selection, minimized-window
activation, top tabs and exhaustive visual matching remain to be checked.

The unrelated standalone UI experiment is archived under ignored
`windows/build/switcher-ui-reference`; it is not shipped.

API references: [WinUI Popup](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.controls.primitives.popup),
[targeted URI activation](https://learn.microsoft.com/en-us/uwp/api/windows.system.launcheroptions.targetapplicationpackagefamilyname),
[RegisterHotKey](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey).
