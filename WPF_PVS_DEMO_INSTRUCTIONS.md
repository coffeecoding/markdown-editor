# Minimal WPF PVS demo

Build a separate prototype in `wpf-pvs-demo/`, preserving the existing C++ demo.

## Stack and scope

- Use C#, WPF, and the .NET 10 SDK (`net10.0-windows`).
- Create one window: patient search/list on the left; selected patient's name, date of birth, and a short mock note on the right.
- Seed 5 fictional patients in memory. Search filters by name; selecting a patient updates the details.
- No backend, database, network requests, persistence, authentication, or additional screens.
- Keep code and dependencies minimal: built-in WPF/.NET only; simple bindings and code-behind are sufficient. Do not build a general framework.

## Appearance and behavior

- Use a clean flat layout, readable Segoe UI text, consistent spacing, and shared styles/resources.
- Include a light/dark switch: make it static at first: it should write to local config file the new value (app should have a config file at same directory as executable for any preferences we wanna persist across app sessions) and show a dialog saying to restart the app to apply change
- Give buttons clean 2d hover, pressed, disabled, and keyboard-focus states
- Support keyboard navigation, window resizing, and sharp text at 100%, 150%, and 200% display scaling. Configure Per-Monitor V2 DPI awareness and use WPF layout rather than fixed pixel positions.
- Keep theme resources organized so a reusable component library could be extracted later. Custom fonts and runtime localization are future extensions, outside this minimal demo.

## Create, run, and publish

Run from the repository root on a development machine with the .NET 10 SDK:

```powershell
dotnet new wpf -n WpfPvsDemo -o wpf-pvs-demo -f net10.0
dotnet run --project .\wpf-pvs-demo\WpfPvsDemo.csproj -c Release
dotnet publish .\wpf-pvs-demo\WpfPvsDemo.csproj -c Release -r win-x64 --self-contained true -p:PublishReadyToRun=true -o .\artifacts\wpf-pvs-demo\win-x64
```

- Publish **self-contained and ReadyToRun together**. The runtime is bundled; clients need no separate .NET installation. ReadyToRun reduces startup JIT work but is not Native AOT.
- Do not enable `PublishAot`, trimming, or single-file publishing. Distribute the entire publish folder, including its executable and supporting files.
- Publish a separate `win-arm64` build if needed, using a separate output folder. Target Windows versions supported by .NET 10.
- Redistribute updated builds when the bundled runtime needs security updates.

## Verify

- Run the published executable directly, outside Visual Studio, on a compatible Windows machine without .NET installed.
- Check search, selection, theme switching, resizing, keyboard use, and display scaling.
- Measure cold and repeated startup to the usable patient screen. Aim for under one second on the target hardware; record actual results rather than assuming ReadyToRun guarantees it.

Reference: [deployment options](https://learn.microsoft.com/en-us/dotnet/core/deploying/) and [ReadyToRun](https://learn.microsoft.com/en-us/dotnet/core/deploying/ready-to-run).
