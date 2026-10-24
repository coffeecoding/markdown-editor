param([switch]$Benchmark)
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path -Parent $PSScriptRoot

# Reuse the application's toolchain setup and Release build.
& (Join-Path $projectDirectory 'build.ps1')
$cache = Join-Path $projectDirectory 'build/Release/CMakeCache.txt'
$ninjaPath = (Select-String -LiteralPath $cache -Pattern '^CMAKE_MAKE_PROGRAM:FILEPATH=(.*)$').Matches.Groups[1].Value
$checkDirectory = Join-Path $projectDirectory 'build/mapping-tests'
& cmake -S $PSScriptRoot -B $checkDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_MAKE_PROGRAM:FILEPATH=$ninjaPath"
if ($LASTEXITCODE -ne 0) { throw 'Mapping check configure failed.' }
& cmake --build $checkDirectory
if ($LASTEXITCODE -ne 0) { throw 'Mapping check build failed.' }
& ctest --test-dir $checkDirectory --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Mapping checks failed.' }
if ($Benchmark) {
    & (Join-Path $checkDirectory 'mapping-checks.exe') --benchmark
    if ($LASTEXITCODE -ne 0) { throw 'Mapping benchmark failed.' }
}
