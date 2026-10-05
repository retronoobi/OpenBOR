param(
    [string]$MsysRoot = 'C:\msys64',
    [ValidateSet('Release','RelWithDebInfo')][string]$Configuration = 'Release',
    [string]$BuildDirectory = (Join-Path $PSScriptRoot 'build-win64')
)
$ErrorActionPreference = 'Stop'
$MsysRoot = [IO.Path]::GetFullPath($MsysRoot).Replace('\','/')
$env:PATH = "$MsysRoot\ucrt64\bin;$env:PATH"
$build = [IO.Path]::GetFullPath($BuildDirectory)
& cmake -S $PSScriptRoot -B $build -G Ninja "-DCMAKE_C_COMPILER=$MsysRoot/ucrt64/bin/gcc.exe" "-DCMAKE_RC_COMPILER=$MsysRoot/ucrt64/bin/windres.exe" "-DCMAKE_BUILD_TYPE=$Configuration" -DBUILD_TESTING=ON
if($LASTEXITCODE) { throw 'CMake configuration failed.' }
& cmake --build $build --parallel
if($LASTEXITCODE) { throw 'Core build failed.' }
& ctest --test-dir $build --output-on-failure
if($LASTEXITCODE) { throw 'Regression tests failed.' }
$dist = Join-Path $PSScriptRoot '../../dist/openbor-libretro-win64'
New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item -LiteralPath "$build/openbor_libretro.dll" -Destination $dist
Copy-Item -LiteralPath "$PSScriptRoot/openbor_libretro.info","$PSScriptRoot/README.md","$PSScriptRoot/TESTING.md" -Destination $dist
Copy-Item -LiteralPath "$PSScriptRoot/../../LICENSE" -Destination "$dist/LICENSE-OpenBOR.txt"
if(Test-Path -LiteralPath "$PSScriptRoot/licenses") { Copy-Item -LiteralPath "$PSScriptRoot/licenses" -Destination $dist -Recurse -Force }
$hash = (Get-FileHash -Algorithm SHA256 "$dist/openbor_libretro.dll").Hash
"$hash  openbor_libretro.dll" | Set-Content -Encoding ascii "$dist/SHA256SUMS.txt"
Compress-Archive -Path "$dist/*" -DestinationPath "$dist.zip" -Force
Write-Output "Core built: $dist/openbor_libretro.dll"
