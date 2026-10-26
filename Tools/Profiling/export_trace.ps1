param(
    [Parameter(Mandatory)][string]$Trace,
    [Parameter(Mandatory)][string]$OutDir,
    [string]$Insights = "C:\Dev\Epic Games\UnrealEngine\Engine\Binaries\Win64\UnrealInsights.exe",
    [switch]$IncludeGameThreadDetail,
    [switch]$IncludeWorkers
)

# Insights strips backslashes from export paths, so everything passed to it uses forward slashes.
New-Item -ItemType Directory -Force $OutDir | Out-Null
$out = (Resolve-Path $OutDir).Path.Replace('\', '/')
$cols = '-columns="ThreadName,TimerName,StartTime,EndTime,Duration,Depth"'
$cmds = @(
    "TimingInsights.ExportTimingEvents `"$out/ev_game.csv`" $cols -threads=`"GameThread`" -timers=`"FEngineLoop::Tick,xrWaitFrame`"",
    "TimingInsights.ExportTimingEvents `"$out/ev_render.csv`" $cols -threads=`"RenderThread 0`"",
    "TimingInsights.ExportTimingEvents `"$out/ev_rhi.csv`" $cols -threads=`"RHIThread`" -timers=`"End_Rendering,Submission_Wait,EndFrame`"",
    "TimingInsights.ExportTimingEvents `"$out/ev_gpu.csv`" $cols -threads=`"GPU0-Graphics0`""
)
if ($IncludeGameThreadDetail) {
    $cmds += "TimingInsights.ExportTimingEvents `"$out/ev_gameall.csv`" $cols -threads=`"GameThread`""
}
if ($IncludeWorkers) {
    # Task-graph workers run the parallel work the main threads wait on (visibility, shadow culling, animation).
    $cmds += "TimingInsights.ExportTimingEvents `"$out/ev_workers.csv`" $cols -threads=`"Foreground Worker*,Background Worker*`""
}
$rsp = "$out/cmds.rsp"
[System.IO.File]::WriteAllLines($rsp, $cmds)
$t = (Resolve-Path $Trace).Path.Replace('\', '/')
$p = Start-Process -FilePath $Insights -ArgumentList "-OpenTraceFile=`"$t`"", "-AutoQuit", "-NoUI", "-ExecOnAnalysisCompleteCmd=`"@=$rsp`"", "-ABSLOG=`"$out/insights.log`"", "-log" -PassThru -Wait
if ($p.ExitCode -ne 0) { Write-Warning "Unreal Insights exited with code $($p.ExitCode); see $out/insights.log" }
