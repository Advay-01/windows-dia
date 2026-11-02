DiaTweaks for Windows - review build

Requires Windows 11 x64 and Dia installed normally. Tested with Dia 1.4.0.424.

1. Extract the entire ZIP to a folder.
2. Double-click DiaTweaks.exe. It opens Dia and stays in the notification area.
3. In Dia Settings, select Patches below Advanced.

Themes: right-click an empty area of Dia's sidebar and choose Themes. Drag the
large color dot or choose a swatch. Use + and - to add/remove linked colors;
the three-circle button changes their harmony. Double-click the large dot for
exact RGB/hex entry. Adjust the opacity wave and grain dial below the swatches.
Remove every color with - to restore Dia. The controls are separate from Patches.

Switches apply immediately and are remembered for the next launch. There is no
terminal to leave open, no developer tools to enable, and no administrator step.
Keep DiaTweaks running while using patches. Right-click its notification icon
to turn all patches off or quit. Quitting restores the original live UI.

For repeated testing, Edit patch preferences opens the saved preferences file.
Save a switch as 1/0 (or on/off); a running DiaTweaks applies the edit within
about a second. The same file reflects changes made through Dia Settings.
File: %LOCALAPPDATA%\DiaTweaks\preferences.ini
Diagnostics: %LOCALAPPDATA%\DiaTweaks\launcher.log

Replacing native code builds: fully Quit/Exit Dia and quit DiaTweaks, replace
the extracted files, then double-click DiaTweaks.exe. Closing only a Dia window
can leave its process alive. The notification tooltip reports if a restart is
needed. This is required because the diagnostics DLL stays loaded until Dia exits.

Verified in the live development session: Patches navigation/selection, wand,
compact sidebar, edge-to-edge, square sidebar corners, hidden toolbar, hover
reveal, and hiding/restoring the sidebar and Chat buttons. Caption-color support
is experimental. Windows controls its own window shadow and backdrop.
Chats/Recent tabs controls still need a matching Windows selector. The domain
grouping command is not included. Both tab layouts need broader verification.

No Dia binary, profile or installed application file is replaced. This ZIP
contains only DiaTweaks binaries, third-party license notices and these instructions. No Microsoft
SDK or runtime files are distributed. diatweaks-probe.exe is optional diagnostics;
reviewers can ignore it. New native code requires a restart; switch edits do not.
