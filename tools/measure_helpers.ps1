param([string]$Executable = "$PSScriptRoot/../build/Release/bateria_f75.exe", [string]$OutputDirectory = "$PSScriptRoot/../build/resource-check/helpers")
$ErrorActionPreference = 'Stop'
[IO.Directory]::CreateDirectory([IO.Path]::GetFullPath($OutputDirectory)) | Out-Null
$rows = @()
foreach ($scenario in @(@{ argument='--detectar'; count=20 }, @{ argument='--estado'; count=3 })) {
    for ($i=0; $i -lt $scenario.count; $i++) {
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $helper = Start-Process -FilePath ([IO.Path]::GetFullPath($Executable)) -ArgumentList $scenario.argument -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $OutputDirectory 'reply.txt')
        # Open the handle before exit so GetProcessTimes remains available afterward.
        $helperHandle = $helper.Handle
        $helper.WaitForExit()
        $rows += [pscustomobject]@{ argument=$scenario.argument; iteration=$i+1; exitCode=$helper.ExitCode; cpu_ms=$helper.TotalProcessorTime.TotalMilliseconds; wall_ms=$watch.Elapsed.TotalMilliseconds; reply=(Get-Content -LiteralPath (Join-Path $OutputDirectory 'reply.txt') -Raw).Trim() }
        $helper.Dispose()
    }
}
$rows | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'samples.json') -Encoding UTF8
$rows | Group-Object argument | ForEach-Object { [pscustomobject]@{ argument=$_.Name; runs=$_.Count; mean_cpu_ms=($_.Group | Measure-Object cpu_ms -Average).Average; max_cpu_ms=($_.Group | Measure-Object cpu_ms -Maximum).Maximum; mean_wall_ms=($_.Group | Measure-Object wall_ms -Average).Average; failures=@($_.Group | Where-Object exitCode -ne 0).Count } } | ConvertTo-Json
