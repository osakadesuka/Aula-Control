[CmdletBinding()]
param(
    [string]$BuildDirectory,
    [string]$IsccPath,
    [string]$RuntimeDirectory,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$version = (Get-Content -LiteralPath (Join-Path $projectRoot 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid VERSION file.' }
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $projectRoot 'build/release-1.0' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$packageRoot = Join-Path $projectRoot 'build/installer'
$stage = Join-Path $packageRoot "payload-$version"
$distribution = Join-Path $projectRoot 'dist'
New-Item -ItemType Directory -Force -Path $packageRoot,$distribution | Out-Null

if (-not $SkipBuild) {
    $configure = @('-S', $projectRoot, '-B', $BuildDirectory, '-A', 'x64', '-DBUILD_TESTING=ON')
    # Reuse downloaded sources, with a separate build tree for every target.
    foreach ($dependency in @('hidapi','raven_vulkan_headers')) {
        $source = Join-Path $projectRoot "build/_deps/$dependency-src"
        if (Test-Path -LiteralPath $source) {
            $configure += "-DFETCHCONTENT_SOURCE_DIR_$($dependency.ToUpperInvariant())=$source"
        }
    }
    & cmake @configure
    if ($LASTEXITCODE) { throw 'CMake configure failed.' }
    & cmake --build $BuildDirectory --config Release --parallel
    if ($LASTEXITCODE) { throw 'Release build failed.' }
    & ctest --test-dir $BuildDirectory -C Release --output-on-failure
    if ($LASTEXITCODE) { throw 'Release checks failed; setup was not generated.' }
}

$binaryDirectory = Join-Path $BuildDirectory 'Release'
$mainBinary = Join-Path $binaryDirectory 'aula_control.exe'
if ((Get-Item -LiteralPath $mainBinary).VersionInfo.ProductVersion -ne $version) {
    throw 'The executable version does not match VERSION. Rebuild before packaging.'
}

if (-not $RuntimeDirectory) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Visual C++ redist directory not found. Supply -RuntimeDirectory.' }
    $redist = Join-Path $visualStudio 'VC/Redist/MSVC'
    $versions = Get-ChildItem -LiteralPath $redist -Directory |
        Where-Object Name -Match '^\d+\.\d+\.\d+$' | Sort-Object { [version]$_.Name } -Descending
    foreach ($candidate in $versions) {
        $crt = Get-ChildItem -LiteralPath (Join-Path $candidate.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($crt) { $RuntimeDirectory = $crt.FullName; break }
    }
}
foreach ($required in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $RuntimeDirectory $required))) { throw "Missing runtime: $required" }
}

if (Test-Path -LiteralPath $stage) {
    # Recursive cleanup is limited to this exact generated payload directory.
    $resolvedStage = (Resolve-Path -LiteralPath $stage).Path
    $allowedParent = (Resolve-Path -LiteralPath $packageRoot).Path
    if ((Split-Path $resolvedStage -Parent) -ne $allowedParent -or (Split-Path $resolvedStage -Leaf) -ne "payload-$version") {
        throw 'Refusing to clean an unexpected package path.'
    }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stage,(Join-Path $stage 'assets'),(Join-Path $stage 'licenses') | Out-Null
foreach ($name in @('aula_control.exe','rgb_prueba.exe','bateria_f75.exe','hidapi.dll')) {
    Copy-Item -LiteralPath (Join-Path $binaryDirectory $name) -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $binaryDirectory 'assets/keyboard-actual-v1.png') -Destination (Join-Path $stage 'assets')
Get-ChildItem -LiteralPath $RuntimeDirectory -Filter '*.dll' -File | Copy-Item -Destination $stage
foreach ($name in @('README.txt','THIRD-PARTY-NOTICES.txt')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $stage
}
function Find-DependencySource([string]$Name) {
    $cacheEntry = Select-String -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt') -Pattern ("^FETCHCONTENT_SOURCE_DIR_"+$Name.ToUpperInvariant()+":[^=]+=.+$") | Select-Object -First 1
    if ($cacheEntry) { return ($cacheEntry.Line -split '=',2)[1] }
    return Join-Path $BuildDirectory "_deps/$Name-src"
}
$hidapiSource = Find-DependencySource 'hidapi'
$licenseFiles = @{
    'third_party/Raven-0.2.3/LICENSE' = 'Raven-Apache-2.0.txt'
    'third_party/Raven-0.2.3/NOTICE' = 'Raven-NOTICE.txt'
    'third_party/Raven-0.2.3/THIRD_PARTY.md' = 'Raven-THIRD_PARTY.md'
    'third_party/Raven-0.2.3/third_party/stb/LICENSE' = 'stb-LICENSE.txt'
    'third_party/Raven-0.2.3/third_party/nanosvg/LICENSE.txt' = 'NanoSVG-LICENSE.txt'
    'third_party/Raven-0.2.3/third_party/roboto/LICENSE' = 'Roboto-Apache-2.0.txt'
    'third_party/Raven-0.2.3/third_party/roboto-mono/LICENSE' = 'RobotoMono-OFL.txt'
    'packaging/licenses/Vulkan-Headers-Apache-2.0.txt' = 'Vulkan-Headers-Apache-2.0.txt'
    'packaging/licenses/Vulkan-Headers-MIT.txt' = 'Vulkan-Headers-MIT.txt'
}
foreach ($source in $licenseFiles.Keys) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $source) -Destination (Join-Path $stage "licenses/$($licenseFiles[$source])")
}
Copy-Item -LiteralPath (Join-Path $hidapiSource 'LICENSE-bsd.txt') -Destination (Join-Path $stage 'licenses/HIDAPI-BSD.txt')
$payloadFiles = @(Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName | ForEach-Object {
    [ordered]@{ path = $_.FullName.Substring($stage.Length + 1).Replace('\','/'); bytes = $_.Length;
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
$manifest = [ordered]@{ product = 'AULA Control'; version = $version; architecture = 'x64';
    runtimeVersion = (Get-Item -LiteralPath (Join-Path $RuntimeDirectory 'vcruntime140.dll')).VersionInfo.FileVersion;
    files = $payloadFiles }
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'release-manifest.json') -Encoding utf8
# This test uses the NULL graphics backend and isolated stores; it does not
# access the user's profiles, remap multimedia keys, or send HID commands.
foreach ($application in @($mainBinary, (Join-Path $stage 'aula_control.exe'))) {
    $check = Start-Process -FilePath $application -ArgumentList '--test' -WindowStyle Hidden -Wait -PassThru
    if ($check.ExitCode -ne 0) { throw "GUI self-test failed: $application" }
}
foreach ($helper in @(@('bateria_f75.exe','--self-test'), @('rgb_prueba.exe','255 0 0 --modo 11 --multicolor'))) {
    $check = Start-Process -FilePath (Join-Path $stage $helper[0]) -ArgumentList $helper[1] -WindowStyle Hidden -Wait -PassThru
    if ($check.ExitCode -ne 0) { throw "Staged helper check failed: $($helper[0])" }
}

if (-not $IsccPath) {
    $compilerDirectory = Join-Path $projectRoot 'build/tools/innosetup-7.1.1'
    $IsccPath = Join-Path $compilerDirectory 'ISCC.exe'
    if (-not (Test-Path -LiteralPath $IsccPath)) {
        $download = Join-Path $packageRoot 'innosetup-7.1.1-x64.exe'
        $url = 'https://github.com/jrsoftware/issrc/releases/download/is-7_1_1/innosetup-7.1.1-x64.exe'
        $expectedHash = '8a5a6711c3766a6e5fb74f280ba0e3551e719ce94538d901fdbd1f8f9920b3f0'
        if (-not (Test-Path -LiteralPath $download)) { Invoke-WebRequest -Uri $url -OutFile $download }
        if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash -ne $expectedHash) { throw 'Inno Setup download hash mismatch.' }
        $signature = Get-AuthenticodeSignature -LiteralPath $download
        if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Pyrsys B\.V\.') {
            throw 'Inno Setup publisher could not be verified.'
        }
        $arguments = @('/PORTABLE=1','/CURRENTUSER','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS',"/DIR=`"$compilerDirectory`"")
        $process = Start-Process -FilePath $download -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $IsccPath)) { throw 'Portable Inno Setup extraction failed.' }
    }
}
& $IsccPath "/DAppVersion=$version" "/DPayloadDir=$stage" "/DOutputDir=$distribution" (Join-Path $PSScriptRoot 'aula-control.iss')
if ($LASTEXITCODE) { throw 'Installer compiler failed.' }
$installer = Join-Path $distribution "AULA-Control-$version-Setup-x64.exe"
$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($installer))" | Set-Content -LiteralPath "$installer.sha256" -Encoding ascii
Copy-Item -LiteralPath (Join-Path $stage 'release-manifest.json') -Destination (Join-Path $distribution "AULA-Control-$version-manifest.json") -Force
$releaseNotes = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'release-notes.txt.in') -Raw
$releaseNotes.Replace('@VERSION@',$version).Replace('@INSTALLER@',[IO.Path]::GetFileName($installer)).Replace('@SHA256@',$hash) |
    Set-Content -LiteralPath (Join-Path $distribution "AULA-Control-$version-release-notes.txt") -Encoding utf8
Write-Output "Installer ready: $installer"
Write-Output "SHA256: $hash"
