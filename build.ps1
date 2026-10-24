param([ValidateSet('Release', 'Debug')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$projectDirectory = $PSScriptRoot

if (-not $env:VSCMD_VER) {
    $vswherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswherePath)) { throw 'Install Visual Studio with Desktop development with C++, or run from an x64 Native Tools prompt.' }
    $installation = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installation) { throw 'No MSVC x64 installation found.' }
    $devCommand = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
    $environmentLines = & $env:ComSpec /d /c "call `"$devCommand`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the MSVC environment.' }
    foreach ($line in $environmentLines) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
    }
}

$buildDirectory = Join-Path $projectDirectory "build\$Configuration"
$bundledNinja = Join-Path $env:VSINSTALLDIR 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
if (Test-Path -LiteralPath $bundledNinja) { $ninjaPath = $bundledNinja }
else {
    $ninjaFile = Get-Item -LiteralPath (Get-Command ninja -ErrorAction Stop).Source
    $ninjaPath = if ($ninjaFile.LinkType -eq 'SymbolicLink') { $ninjaFile.Target } else { $ninjaFile.FullName }
}
& cmake -S $projectDirectory -B $buildDirectory -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_MAKE_PROGRAM:FILEPATH=$ninjaPath"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& cmake --build $buildDirectory
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
if ($Configuration -eq 'Release') {
    $releaseDirectory = Join-Path $projectDirectory 'release'
    New-Item -ItemType Directory -Path $releaseDirectory -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildDirectory 'markdown-editor.exe') -Destination $releaseDirectory
    Copy-Item -LiteralPath (Join-Path $buildDirectory 'MD4C-LICENSE.txt') -Destination $releaseDirectory
    Get-ChildItem -LiteralPath $releaseDirectory -File | Select-Object Name, Length
}
