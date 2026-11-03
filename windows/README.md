# DiaTweaks Windows development

For a review build, extract **DiaTweaks-Windows-x64.zip** and double-click
**DiaTweaks.exe**. It opens Dia, runs in the notification area and installs
Patches automatically when Settings opens. No terminal or developer tools are
needed. See [tester instructions](TESTER-README.txt).

The launcher remembers switches in `%LOCALAPPDATA%\DiaTweaks\preferences.ini`.
Editing that file applies changes live; Settings toggles update the same file.
Native DLL code updates still require fully quitting Dia before replacing files.
The tray menu can turn everything off or quit and restore the original live UI.

The Windows preview includes a [native Patches page](settings/README.md),
a sidebar [Themes editor](features/appearance.md), and an
[Alt+Space search panel](features/url-overlay.md). Patches shares the macOS
feature catalog and enables switches with a Windows adapter for the current
Dia layout. See [feature adapters](features/README.md) for their scope.

The source also builds an x64 developer probe and a COM TAP DLL for WinUI XAML
diagnostics. The probe supports live attachment, native tree discovery, and timed
Visibility overrides on Dia 1.4.0.424.

## Build

Requires Windows 11 x64, Visual Studio 2022 or later C++ Build Tools, the Windows
SDK, and Visual Studio's CMake component. From the repository root:

```powershell
.\windows\build.ps1
```

The script builds Release and runs CTest. `-Configuration Debug` builds Debug.
It uses a single MSBuild worker with node reuse disabled. No NuGet packages or
other source downloads are required.

To build and package the full launcher without installing Dia on a build machine:

```powershell
.\windows\prepare-sdk.ps1
.\windows\build-settings.ps1 -MetadataPath "$PWD\windows\build\public-sdk"
.\windows\package.ps1
```

This uses pinned public SDK metadata at build time. The tester ZIP contains no
Dia, Microsoft SDK or Windows App Runtime files. Windows CI builds the same ZIP
and uploads it as an artifact for each pull request.

## Run

Open Dia normally, then run from a terminal at the repository root:

```powershell
.\windows\build\Release\diatweaks-probe.exe --check
.\windows\build\Release\diatweaks-probe.exe
```

`--check` validates the target without attaching. The controller requires the
installed `TheBrowserCompany.Dia_ttt1ap7aakyb4` package, an x64 `Dia.exe` with a
visible top-level window, and its
loaded `Microsoft.Internal.FrameworkUdk.dll`. If several candidates exist, use
`--pid PID` to select one. It rejects an already loaded `DiaTweaksTap.dll`.

After attachment, the following commands are available:

| Command | Behavior |
| --- | --- |
| `tree` | Print native XAML node IDs, owner thread, hierarchy, type and `x:Name` |
| `tree Button` | Filter the structural snapshot by text |
| `inspect 123` | Inspect node 123's Visibility and whether it has a local value |
| `layout 123` | Inspect structural layout geometry in a feature-enabled build |
| `features` | Report discovered, requested and applied feature states |
| `enable KEY` / `disable KEY` | Request a session patch by shared catalog key |
| `pulse 123 5` | Collapse that node for five seconds, then restore it |
| `restore` | Restore active overrides immediately |
| `quit` | Restore and end the controller session |

`123` is an example: use an actual node ID from the current `tree` output. IDs
are session-scoped and deliberately distinct from addresses. Inspect and visually
identify the intended native control before pulsing it. Pulse accepts 1–30
seconds and at most one active override per UI thread. It has no saved settings.

The probe does not patch, re-sign, replace or update the installed Dia package.
If Windows denies loading its diagnostics runtime from WindowsApps, the controller
copies that one DLL into a unique temporary directory and loads those exact bytes
locally. It attempts cleanup on exit; a file still in use may remain in the temp
directory. Do not distribute this runtime DLL. The TAP changes an in-memory
properties and native controls only. The TAP DLL remains resident but inert
after the session ends. **Restart Dia before attaching again or rebuilding a
loaded DLL.** Keep other XAML diagnostic tools detached during this experiment.

## What has been verified

The controller and DLL build with MSVC 19.51 and Windows SDK 10.0.26100.0.
Automated tests load the actual TAP DLL into a local fixture that implements the
diagnostics interfaces and owns a real Windows message loop. Tests cover COM
activation, shared-memory requests, UI-thread dispatch, readback, time-limited
restoration, disconnect restoration, local/default value restoration, stale IDs,
duplicate controllers, and refusal of bindings and animations.

The first live attachment also succeeded, returning controls including
`ChatButtonHost`, `SidebarToggleButton`, and navigation buttons. The initial
visibility eligibility checks rejected the inspected controls as unsupported.
Further inspection identified numeric enum serialization as the cause; the
adapter now accepts the observed `0`/`1` Visibility values. A live five-second
sidebar-toggle pulse passed, including readback and restoration of its local value.
See [live-probe.md](live-probe.md) for the results and remaining checks. Real
visual behavior, screenshots, both tab layouts, and multiple windows remain
unverified. Test shared-memory access requires running outside Codex's
restricted sandbox; normal same-user terminal execution is the intended mode.

The installed 1.4.0.424 diagnostics DLL was also checked separately: loading
directly from WindowsApps returns Access denied, while loading the exact staged
bytes succeeds and exposes `InitializeXamlDiagnosticsEx`. This check does not
attach to Dia. To repeat it:

```powershell
.\windows\build\Release\prototype-tests.exe --runtime 'C:\Program Files\WindowsApps\TheBrowserCompany.Dia_1.4.0.424_x64__ttt1ap7aakyb4\Microsoft.Internal.FrameworkUdk.dll'
```

Probe failures now name their stage. Only enter commands such as `tree Button`
after the probe prints `Attached` and its `>` prompt. At a PowerShell `PS ...>`
prompt, `tree` is Windows' folder-listing command.

## Limits

- The adapter depends on an internal WinUI diagnostics export. Dia or WinUI
  updates can break attachment. Missing capabilities return errors.
- Tree output is limited to 8,192 live nodes and a 256K-character response.
  Truncation is reported. It omits text content, URLs, accessible labels and
  arbitrary properties; structural names can still be app supplied.
- Only literal `Visibility` values from local, default, built-in style or style
  sources are eligible. Bindings, expressions, handles, animation and unfamiliar
  enum representations other than Visible/Collapsed or their `0`/`1` values are refused.
- Restoration runs on the owning UI thread. A hung UI delays it. A changed app
  value is preserved when detectable; an app writing the same `Collapsed` value
  cannot be distinguished from the probe's override.
- The prototype does not reapply a feature to newly created controls, reconnect
  after restart, implement preferences, or port Mac hooks. Those follow the live
  attachment experiment, as described in [architecture.md](architecture.md).

For `0x80070005`, use a normal terminal under the same user and integrity level
as Dia. Do not change the process security policy. For `0x800700b7`, close the
existing controller and restart Dia. On an unconfirmed restore, let the UI
resume or restart Dia to remove all in-memory overrides.
