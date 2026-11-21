[CmdletBinding()]
param([string]$InstallerPath)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$version = (Get-Content -LiteralPath (Join-Path $projectRoot 'VERSION') -Raw).Trim()
if (-not $InstallerPath) { $InstallerPath = Join-Path $projectRoot "dist/AULA-Control-$version-Setup-x64.exe" }
$InstallerPath = (Resolve-Path -LiteralPath $InstallerPath).Path
$testRoot = Join-Path $projectRoot 'build/installer-smoke'
$installDirectory = Join-Path $testRoot 'AULA Control'
$dataDirectory = Join-Path $env:LOCALAPPDATA 'AulaControl'
$startupKeyPath = 'Software\Microsoft\Windows\CurrentVersion\Run'
$uninstallKeyPath = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{B67295C2-0F10-4A2C-B113-225FC0C0075A}_is1'
$shortcut = Join-Path $env:APPDATA 'Microsoft/Windows/Start Menu/Programs/AULA Control/AULA Control.lnk'
$registry = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser,[Microsoft.Win32.RegistryView]::Registry64)
$alreadyInstalled = $registry.OpenSubKey($uninstallKeyPath)
if ($alreadyInstalled) { $alreadyInstalled.Dispose(); throw 'An installed AULA copy exists. Run installer smoke tests in a separate Windows user profile.' }
try { $runningMutex = [Threading.Mutex]::OpenExisting('Local\AulaControl.Desktop.1') } catch [Threading.WaitHandleCannotBeOpenedException] { $runningMutex = $null }
if ($runningMutex) { $runningMutex.Dispose(); throw 'Exit AULA Control from the tray before testing its installer.' }
if ((Test-Path -LiteralPath $installDirectory) -or (Test-Path -LiteralPath $shortcut)) { throw 'Smoke-test target already exists; inspect it before retrying.' }
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null

function Assert-True([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message }; Write-Output "PASS: $Message" }
function Read-Startup {
    $key = $registry.OpenSubKey($startupKeyPath)
    if (-not $key) { return $null }
    try { return $key.GetValue('AULA Control',$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames) } finally { $key.Dispose() }
}
function Write-Startup($Value, $Kind = [Microsoft.Win32.RegistryValueKind]::String) {
    $key = $registry.CreateSubKey($startupKeyPath)
    try { if ($null -eq $Value) { $key.DeleteValue('AULA Control',$false) } else { $key.SetValue('AULA Control',$Value,$Kind) } }
    finally { $key.Dispose() }
}
function Data-Snapshot {
    $result = @{}
    if (Test-Path -LiteralPath $dataDirectory) {
        Get-ChildItem -LiteralPath $dataDirectory -File -Recurse | ForEach-Object {
            $result[$_.FullName.Substring($dataDirectory.Length)] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    }
    return $result
}
function Assert-Data {
    $after = Data-Snapshot
    Assert-True ($before.Count -eq $after.Count) 'User-data file count is preserved'
    foreach ($path in $before.Keys) { Assert-True ($before[$path] -eq $after[$path]) "User data is unchanged: $path" }
}
function Run-Setup([string]$Language,[string]$LogName) {
    $arguments = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LANG=$Language", "/DIR=`"$installDirectory`"", "/LOG=`"$(Join-Path $testRoot $LogName)`"")
    $process = Start-Process -FilePath $InstallerPath -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    Assert-True ($process.ExitCode -eq 0) "Setup completed: $Language"
}
function Run-Uninstall([string]$LogName) {
    $uninstaller = Join-Path $installDirectory 'unins000.exe'
    $arguments = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=`"$(Join-Path $testRoot $LogName)`"")
    $process = Start-Process -FilePath $uninstaller -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    Assert-True ($process.ExitCode -eq 0) 'Uninstall completed'
}
function Assert-Payload {
    $manifestPath = Join-Path $installDirectory 'release-manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    Assert-True ($manifest.version -eq $version) 'Installed manifest has the expected version'
    foreach ($file in $manifest.files) {
        $path = Join-Path $installDirectory $file.path
        Assert-True ((Test-Path -LiteralPath $path) -and ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -eq $file.sha256)) "Installed payload verified: $($file.path)"
    }
    $exe = Join-Path $installDirectory 'aula_control.exe'
    Assert-True ((Get-Item -LiteralPath $exe).VersionInfo.ProductVersion -eq $version) 'Windows product version is correct'
    $check = Start-Process -FilePath $exe -ArgumentList '--test' -WindowStyle Hidden -Wait -PassThru
    Assert-True ($check.ExitCode -eq 0) 'Installed GUI passes its isolated self-test'
    $key = $registry.OpenSubKey($uninstallKeyPath)
    Assert-True ($null -ne $key) 'Per-user uninstall registration exists'
    try { Assert-True ($key.GetValue('DisplayVersion') -eq $version) 'Installed-app version is correct' } finally { $key.Dispose() }
    Assert-True (Test-Path -LiteralPath $shortcut) 'Start-menu shortcut exists'
    $shell = New-Object -ComObject WScript.Shell
    $link = $shell.CreateShortcut($shortcut)
    Assert-True ($link.TargetPath -eq $exe -and $link.WorkingDirectory -eq $installDirectory) 'Shortcut points to the installed executable'
    Assert-Data
}

$originalStartup = Read-Startup
$originalKind = [Microsoft.Win32.RegistryValueKind]::String
if ($null -ne $originalStartup) {
    $key = $registry.OpenSubKey($startupKeyPath)
    try { $originalKind = $key.GetValueKind('AULA Control') } finally { $key.Dispose() }
}
$before = Data-Snapshot
if (Test-Path -LiteralPath $dataDirectory) { Copy-Item -LiteralPath $dataDirectory -Destination (Join-Path $testRoot ('data-before-'+[Guid]::NewGuid().ToString('N'))) -Recurse }
$originalStartup | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $testRoot 'startup-before.json') -Encoding utf8
$completed = $false
try {
    Write-Startup $null
    Run-Setup 'english' 'install-en.log'
    Assert-True ($null -eq (Read-Startup)) 'First install leaves Windows startup disabled'
    Assert-Payload

    $legacyStartup = '"C:\AULA Development\aula_control.exe" --background'
    Write-Startup $legacyStartup
    Run-Setup 'spanish' 'upgrade-es.log'
    Assert-True ((Read-Startup) -eq ('"'+(Join-Path $installDirectory 'aula_control.exe')+'" --background')) 'Upgrade migrates an enabled development-path startup entry'
    Assert-Payload

    $laterStartup = '"C:\AULA Later Version\aula_control.exe" --background'
    Write-Startup $laterStartup
    $sentinel = Join-Path $installDirectory 'untracked-user-file.txt'
    'Installer must preserve files it did not install.' | Set-Content -LiteralPath $sentinel
    Run-Uninstall 'uninstall-preserve.log'
    Assert-True ((Read-Startup) -eq $laterStartup) 'Uninstall preserves a later startup registration'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $installDirectory 'aula_control.exe'))) 'Installed application is removed'
    Assert-True (Test-Path -LiteralPath $sentinel) 'Uninstall preserves untracked files'
    Assert-True (-not (Test-Path -LiteralPath $shortcut)) 'Start-menu shortcut is removed'
    Assert-Data
    Remove-Item -LiteralPath $sentinel
    if ((Get-ChildItem -LiteralPath $installDirectory -Force | Measure-Object).Count -eq 0) { Remove-Item -LiteralPath $installDirectory }

    Write-Startup $legacyStartup
    Run-Setup 'english' 'reinstall.log'
    Run-Uninstall 'uninstall-own-startup.log'
    Assert-True ($null -eq (Read-Startup)) 'Uninstall removes its own startup path'
    $key = $registry.OpenSubKey($uninstallKeyPath)
    Assert-True ($null -eq $key) 'Uninstall registration is removed'
    if ($key) { $key.Dispose() }
    Assert-Data
    $completed = $true
} finally {
    Write-Startup $originalStartup $originalKind
    $registry.Dispose()
}
if ($completed) { Write-Output 'Installer round trip passed: install, update, manifest, shortcut, runtime self-test, startup migration, uninstall, preserved user data.' }
