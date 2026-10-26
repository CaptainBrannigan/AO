param(
    [Parameter(Mandatory)][string]$Trace,
    [string]$OutDir = (Join-Path $env:TEMP ("ao_trace_" + [IO.Path]::GetFileNameWithoutExtension($Trace))),
    [double]$SkipSeconds = 5.0,
    [double]$FrameBudgetMs = 8.333,
    [switch]$SkipExport,
    [switch]$IncludeWorkers
)

# One-command summary of an Unreal Insights trace from a packaged VR build:
# frame pacing, real thread work (XR waits removed), render-thread hot spots,
# traditional vs. Nanite GPU draws, and the GPU pass breakdown.
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $SkipExport) { & "$here\export_trace.ps1" -Trace $Trace -OutDir $OutDir -IncludeWorkers:$IncludeWorkers }
Add-Type -Path "$here\TraceAnalyzer.cs"

$frames = 0
"== $Trace"
[TraceAnalyzer]::Frames("$OutDir\ev_game.csv", $SkipSeconds, $FrameBudgetMs, [ref]$frames)
"== Render thread"
[TraceAnalyzer]::TimerStats("$OutDir\ev_render.csv", $SkipSeconds, [string[]]@(
    'FDeferredShadingSceneRenderer_Render', 'ProcessVisibilityTasks', 'WaitForVisibilityTasks',
    'InitNaniteRaster', 'FSceneRenderer::RenderShadowDepthMaps'))
"== RHI thread"
[TraceAnalyzer]::TimerStats("$OutDir\ev_rhi.csv", $SkipSeconds, [string[]]@('End_Rendering', 'Submission_Wait', 'EndFrame'))
"== GPU traditional draws (ParallelDraw) by pass; near zero when everything is Nanite"
[TraceAnalyzer]::Paths("$OutDir\ev_gpu.csv", $SkipSeconds, $frames, 'ParallelDraw', 4)
"== GPU pass breakdown (ms per frame)"
[TraceAnalyzer]::Breakdown("$OutDir\ev_gpu.csv", $SkipSeconds, $frames, 4, 5, 12)
if (Test-Path "$OutDir\ev_workers.csv") {
    "== Worker threads: total work per frame, summed across all workers (top-level tasks and their first level)"
    [TraceAnalyzer]::Breakdown("$OutDir\ev_workers.csv", $SkipSeconds, $frames, 0, 1, 15)
}
