param(
    [Parameter(Mandatory=$true)][int]$TargetProcessId,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [ValidateRange(10,86400)][int]$Seconds = 1200
)
$ErrorActionPreference = 'Stop'
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$csv = Join-Path $OutputDirectory 'samples.csv'
if (Test-Path -LiteralPath $csv) { throw 'Choose a new output directory; existing measurement samples are preserved.' }
$process = Get-Process -Id $TargetProcessId
$processorCount = [Environment]::ProcessorCount
$metadata = [ordered]@{
    startedUtc = [DateTime]::UtcNow.ToString('o'); processId = $TargetProcessId
    executable = $process.Path; sha256 = (Get-FileHash -LiteralPath $process.Path).Hash
    logicalProcessors = $processorCount; sampleIntervalSeconds = 1
    cpu = @(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors)
    ramBytes = (Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory
    gpu = @(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
    cpuDefinition = 'Process CPU-time delta / elapsed wall time / logical processors * 100'
    gpuDefinition = 'Busiest GPU engine attributed to target PID; GPU memory summed across its adapters'
    scope = 'GUI process only. Short-lived battery helpers and desktop compositor are excluded.'
}
$metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'metadata.json') -Encoding UTF8
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class AulaResourceProbe {
    [DllImport("user32.dll")] public static extern uint GetGuiResources(IntPtr process, uint flag);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
}
'@
$observedWindow = $process.MainWindowHandle
$phaseFile = Join-Path $OutputDirectory 'phase.txt'
$stopFile = Join-Path $OutputDirectory 'stop'
if (!(Test-Path -LiteralPath $phaseFile)) { 'startup' | Set-Content -LiteralPath $phaseFile -Encoding ASCII }
$watch = [Diagnostics.Stopwatch]::StartNew()
$lastTime = $watch.Elapsed.TotalSeconds
$lastCpu = $process.TotalProcessorTime.TotalSeconds
$lastPhase = ''
$counterErrors = @()
$samples = 0
try {
    Get-Counter -Counter '\GPU Engine(*)\Utilization Percentage','\GPU Process Memory(*)\Dedicated Usage','\GPU Process Memory(*)\Shared Usage' -SampleInterval 1 -MaxSamples $Seconds -ErrorAction SilentlyContinue -ErrorVariable +counterErrors | ForEach-Object {
        if ((Test-Path -LiteralPath $stopFile) -or $watch.Elapsed.TotalSeconds -ge $Seconds) { throw 'AULA_PROBE_COMPLETE' }
        $process.Refresh()
        if ($process.HasExited) { throw 'Target process exited during measurement' }
        $now = $watch.Elapsed.TotalSeconds
        $cpu = $process.TotalProcessorTime.TotalSeconds
        $phase = (Get-Content -LiteralPath $phaseFile -Raw).Trim()
        $own = @($_.CounterSamples | Where-Object { $_.InstanceName -like "pid_${TargetProcessId}_*" -and $_.Status -eq 0 })
        $engines = @($own | Where-Object Path -Like '*\utilization percentage')
        $dedicated = @($own | Where-Object Path -Like '*\dedicated usage')
        $shared = @($own | Where-Object Path -Like '*\shared usage')
        $row = [pscustomobject][ordered]@{
            utc = [DateTime]::UtcNow.ToString('o'); elapsed_s = [Math]::Round($now,3); phase = $phase
            cpu_pct = [Math]::Round(100 * ($cpu - $lastCpu) / ($now - $lastTime) / $processorCount,4)
            working_set_mib = [Math]::Round($process.WorkingSet64 / 1MB,3)
            private_mib = [Math]::Round($process.PrivateMemorySize64 / 1MB,3)
            gpu_pct = $(if ($engines.Count) { [Math]::Round(($engines | Measure-Object CookedValue -Maximum).Maximum,4) } else { $null })
            gpu_dedicated_mib = $(if ($dedicated.Count) { [Math]::Round(($dedicated | Measure-Object CookedValue -Sum).Sum / 1MB,3) } else { $null })
            gpu_shared_mib = $(if ($shared.Count) { [Math]::Round(($shared | Measure-Object CookedValue -Sum).Sum / 1MB,3) } else { $null })
            handles = $process.HandleCount; threads = $process.Threads.Count
            gdi_objects = [AulaResourceProbe]::GetGuiResources($process.Handle,0)
            user_objects = [AulaResourceProbe]::GetGuiResources($process.Handle,1)
            responding = $process.Responding
            window_visible = $(if ($observedWindow -ne [IntPtr]::Zero) { [AulaResourceProbe]::IsWindowVisible($observedWindow) } else { $null })
            window_minimized = $(if ($observedWindow -ne [IntPtr]::Zero) { [AulaResourceProbe]::IsIconic($observedWindow) } else { $null })
            window_foreground = $(if ($observedWindow -ne [IntPtr]::Zero) { [AulaResourceProbe]::GetForegroundWindow() -eq $observedWindow } else { $null })
        }
        $row | Export-Csv -LiteralPath $csv -NoTypeInformation -Append -Encoding UTF8
        if ($phase -ne $lastPhase) { Write-Output "Phase: $phase at $([Math]::Round($now)) s"; $lastPhase = $phase }
        $lastCpu = $cpu; $lastTime = $now; $samples++
    }
} catch {
    if ($_.Exception.Message -ne 'AULA_PROBE_COMPLETE') {
        $_ | Out-String | Set-Content -LiteralPath (Join-Path $OutputDirectory 'error.txt')
        throw
    }
} finally {
    [pscustomobject]@{ endedUtc = [DateTime]::UtcNow.ToString('o'); samples = $samples; elapsedSeconds = $watch.Elapsed.TotalSeconds; counterErrors = @($counterErrors | ForEach-Object { $_.ToString() } | Where-Object { $_ -notmatch 'AULA_PROBE_COMPLETE' }) } |
        ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'completion.json') -Encoding UTF8
    Write-Output "Saved $samples samples to $csv"
}
