# Engine & Performance Setup

Project configuration, engine stripping, profiling methodology, and the VR viability test. Rendering-specific techniques are in [Rendering.md](Rendering.md); AI-specific costs are in [AI.md](AI.md).

**Project:** `C:\Dev\AO\AO` (`AO.uproject`, created from the VR template, now a C++ project).

**Engine:** the **source build at `C:\Dev\Epic Games\UnrealEngine` (UE 5.8)**, which the project's engine association points to. Not the installed `C:\Dev\Epic Games\UE_5.7`. Build tools, Unreal Insights and plugin checks must all come from the source engine.

## Current direction

- Performance is a design constraint from the start (see [Vision.md](Vision.md)).
- **First milestone: the VR viability test.** Before gameplay work, measure how many AI and static meshes a large map can hold in VR with the single-material, textureless pipeline.
- Fix renderer settings first, then strip plugins. Renderer settings are where frame time is; plugin stripping mostly buys memory, load time, package size, shader permutation count and fewer things that can go wrong.

## Frame budget

| Refresh | Frame budget | Practical target (headroom for spikes, reprojection) |
|---|---|---|
| 72 Hz | 13.9 ms | ~12 ms |
| 90 Hz | 11.1 ms | ~9 ms |
| 120 Hz | 8.3 ms | ~7 ms |

Game thread, render thread and GPU each have to fit inside the budget independently; the slowest one sets the frame rate.

## Renderer config audit (`Config/DefaultEngine.ini`)

The VR template's config already has good VR basics but also several settings that cost frame time or memory for features this project won't use.

**Already good:**
- `r.ForwardShading=True`, `r.AntiAliasingMethod=3` (MSAA)
- `vr.InstancedStereo=True`, `vr.MobileMultiView=True`
- Motion blur, auto exposure and SSAO defaults off
- `r.AllowStaticLighting=True` (needed for baked lighting)

**Recommended changes:**

| Setting | Current | Proposed | Why |
|---|---|---|---|
| `r.DynamicGlobalIlluminationMethod` | 1 (Lumen) | 0 (None) | Lighting is baked; Lumen doesn't run in forward shading anyway, so make it explicit |
| `r.ReflectionMethod` | 1 (Lumen) | 0 (None: reflection captures) | Same reason |
| `r.RayTracing` | True | False | Ray tracing adds acceleration-structure builds, memory and shader permutations |
| `r.RayTracing.RayTracingProxies.ProjectEnabled` | True | False | Only relevant with ray tracing |
| `r.Shadow.Virtual.Enable` | 1 | 0 | Virtual Shadow Maps are expensive and aimed at Nanite/Lumen content; use baked shadows + limited dynamic shadow maps |
| `r.GenerateMeshDistanceFields` | True | False | Memory and build time for distance-field features that won't be used |
| `r.Substrate` | True | **test both** | The new material system. Compare its base-pass cost against legacy materials with the color-region material before committing |
| `TargetPlatforms` (uproject) | 12 platforms | Windows (+ Android if Quest standalone is a target) | Fewer platforms = fewer cooks and shader formats |

**Status (2026-10-26, commit `3cf7d3e`):** Lumen GI/reflections, ray tracing and distance fields are off. **Renderer path: deferred + VSM + Nanite** (`r.ForwardShading=False`, `r.Shadow.Virtual.Enable=1`, `r.Nanite.Foliage=True`), which supersedes the forward / VSM-off proposals in the table above. Reason: **Nanite needs the deferred renderer in practice**, and at large-map foliage density deferred + VSM + Nanite held 120 Hz where forward without Nanite fell to 85 fps (see "Test log" and [Rendering.md](Rendering.md)). MSAA only works in forward; the ini now has `r.AntiAliasingMethod=0` (none), so anti-aliasing under deferred is an open item. Remaining leftovers from testing: `r.Mobile.ShadingPath=1` and `r.MobileHDR=True` only affect the mobile renderer (Quest standalone); harmless on PCVR, remove if the target is PCVR only. `r.Substrate=True` is still untested against legacy materials. For Nanite scenes: the directional light's **Cast Static Shadows off** (Nanite meshes don't receive baked shadows anyway) saved ~0.6 ms render thread.

**Applied: `r.NumBufferedOcclusionQueries=2`** (found 2026-10-26, in `DefaultEngine.ini` as of `3cf7d3e`). With Nanite scenery, hardware occlusion queries only apply to non-Nanite meshes (characters), and waiting for their results from the GPU cost the render thread 1.4–2.6 ms per frame with 100 characters. Buffering them (use results from two frames ago) removed the wait entirely (render thread 4.44 → 1.92 ms, p99 6.71 → 2.36 ms) **while keeping occlusion culling**, which matters on maps where buildings hide many characters. Cost: an object revealed from behind an occluder can appear one frame (8 ms) late; frustum culling during head turns isn't delayed. Alternative: `r.AllowOcclusionQueries=0` gave the same render-thread result and no GPU cost in the open test map, but drops occlusion culling of non-Nanite meshes entirely.

**Decision needed: target hardware.** PCVR only, Quest standalone, or both. Standalone changes nearly everything: no Nanite, mobile shading path, much smaller budgets for draw calls, triangles and memory. The deferred + Nanite path above effectively commits the project to PCVR; Quest standalone would need a separate mobile-renderer content path.

## Plugin stripping

The engine enables well over 150 plugins by default (169 counted in the 5.7 install), on top of the template's own (OpenXR, OpenXREyeTracker, OpenXRHandTracking, PICOController). Disable them by adding `"Enabled": false` entries in `AO.uproject`. Editor-only plugins never ship in a packaged game, so they matter for editor speed and stability, not frame time.

**Applied 2026-10-23:** 38 plugins disabled in `AO.uproject`, checked against the source engine for dependency conflicts (none). Kept enabled because other plugins depend on them: GeometryCache and VariantManagerContent (Interchange, Dataflow), MetaHumanSDK (Bridge). Source control and IDE integration plugins were re-enabled after disabling PerforceSourceControl stopped the editor from opening. They're editor-only with no runtime cost, so they stay enabled. After changing the plugin list, rebuild the editor target (`Build.bat AOEditor Win64 DebugGame -Project=...`) before opening.

**Source-engine gotcha (shared binaries):** AO's editor target builds into the source engine's shared `Engine/Binaries`. Building it can give the engine a new build ID while only refreshing module manifests for the plugins AO enables. Opening the engine on its own, or another project (e.g. Lyra) whose plugin set differs, then fails with "module missing / built with a different engine version" for plugins outside AO's set (seen with CompositeCore). Fix: rebuild the engine's default editor target (`Build.bat UnrealEditor Win64 Development -WaitMutex`), then rebuild `AOEditor`, or accept the editor's rebuild prompt. The first open after plugin or renderer changes can freeze for a while while shaders recompile; that's expected.

**Standalone engine programs aren't rebuilt with the editor.** In the source engine, Unreal Insights and UnrealLightmass are separate programs, always built in Development whatever the project's configuration (DebugGame only affects the game module). Rebuild them after engine updates: `Build.bat UnrealLightmass Win64 Development` (fixes "Unreal Lightmass executable is outdated") and `Build.bat UnrealInsights Win64 Development`. Swarm Agent (`Engine\Binaries\DotNET\SwarmAgent.exe`) runs the lighting builds and already exists.

**Keep (in use or planned):**
- OpenXR (+ EyeTracker/HandTracking if the target headset supports them)
- EnhancedInput, Niagara, Metasound, Interchange (import)
- ControlRig, IKRig, RigVM (VR body IK)
- OnlineSubsystem, OnlineSubsystemNull, OnlineSubsystemUtils, OnlineBase, OodleNetwork (multiplayer)
- **Iris** (not enabled by default; must be added, see [Networking.md](Networking.md))
- AISupport, EnvironmentQueryEditor (AI/EQS)
- **AnimationSharing**: worth *using*, not just keeping. It shares animation evaluation across many characters, which directly helps AI density.
- ACLPlugin (animation compression), GameplayTagsEditor
- Profiling: TraceUtilities, RenderDocPlugin, PixWinPlugin, ChaosInsights, RenderGraphInsights, WorldMetrics
- Source control and IDE access for the tools actually used (Git, Visual Studio/Rider)

**Disable (no planned use):**
- **Template:** PICOController (unless Pico is a target)
- **2D / legacy FX:** Paper2D, Cascade
- **Media/movie players:** AndroidMedia, AvfMedia, WmfMedia, MediaPlate, MediaCompositing, MediaPlayerEditor, AndroidMoviePlayer, AppleMoviePlayer, WebMMoviePlayer, WindowsMoviePlayer (keep one only if startup movies or in-game video are planned)
- **Mobile / store / platform services:** GooglePAD, GoogleCloudMessaging, OnlineSubsystemGooglePlay, LocationServicesBPLibrary, MobilePatchingUtils, ChunkDownloader, LauncherChunkInstaller, AppleImageUtils, IOSDeviceProfileSelector, MacGraphicsSwitching, LinuxDeviceProfileSelector (unless Linux is a target). Keep AndroidDeviceProfileSelector/AndroidPermission/AndroidFileServer only if Quest standalone is a target.
- **Enterprise/importers/content:** DatasmithContent, GLTFExporter, VariantManagerContent, AlembicImporter, GeometryCache, SpeedTreeImporter, MetaHumanSDK, ArchVisCharacter
- **Misc runtime components:** CableComponent, CustomMeshComponent (keep ProceduralMeshComponent only if used)
- **Path-tracing denoisers / ML:** NNEDenoiser, NFORDenoise
- **Audio extras:** ResonanceAudio, SoundFields, Synthesis (re-evaluate when audio design starts). AudioCapture: keep if voice chat is planned.
- **Experimental/telemetry:** CharacterAI, BackChannel, RuntimeTelemetry, EditorTelemetry, LowLevelNetTrace (keep if profiling networking), GlobalConfigurationData
- **Unused source control / IDE integrations:** Perforce, Plastic, Subversion, CLion, CodeLite, KDevelop, N10X, XCode, Null
- **Messaging:** UdpMessaging, TcpMessaging (editor↔device communication; keep only while deploying to headsets from the editor)

**Evaluate case by case:** ChaosCloth (cloth on characters?), XInputDevice (gamepad testing), GameplayCameras/EngineCameras, MsQuic (network transport), Bridge/Fab (content sources), the editor tooling plugins (UVEditor, MeshPainting, SkeletalMeshModelingTools, etc. are harmless at runtime).

**Process:** disable in batches, restart, confirm the project still opens and the test map runs, then commit. Some plugins are dependencies of others, and the editor reports these on startup.

**Modules:** a Blueprint-only project can't strip engine modules directly. Converting to a C++ project (needed anyway for the AI system) allows `Target.cs` settings for shipping builds (e.g. disabling unused engine features at compile time). This is a later, smaller win.

## Profiling a packaged build

**Baseline first: an empty level** (only the VR pawn, plus at most a single directional light and sky light). That measures the engine's fixed cost before any content.

1. **Package a Development build** (Platforms → Windows → build configuration: Development, then Package Project). Development has console access, `stat` commands and full Insights tracing. Use Test builds later for numbers closer to shipping, and confirm tracing still connects there.
2. **Start Unreal Insights first:** `C:\Dev\Epic Games\UnrealEngine\Engine\Binaries\Win64\UnrealInsights.exe` (built 2026-10-23; rebuild with `Engine\Build\BatchFiles\Build.bat UnrealInsights Win64 Development` after engine updates). It runs the trace server the game connects to. Use the Insights that matches the game's engine; trace formats differ between versions.
3. **Launch the packaged game with tracing:**
   `AO.exe -trace=default,gpu,frame,bookmark -tracehost=127.0.0.1`
   - Add `-statnamedevents` for more detailed CPU scopes (adds overhead, so don't use it for headline numbers).
   - Or start/stop from the console during play: `trace.start default,gpu` / `trace.stop`. `trace.bookmark <name>` marks a moment in the timeline.
   - `-tracefile=<path>` writes a file instead of streaming to Insights.
4. **Wear the headset while capturing.** An idle headset (proximity sensor uncovered) makes the runtime throttle or pause the app, which invalidates the numbers.
5. **Capture procedure:** let it settle ~10 s, then 60 s standing still facing one direction, then a slow 360° turn. Bookmark each phase.
6. **Read the trace in Timing Insights:** select the capture range and compare average and worst frames for **GameThread**, **RenderThread**, **RHIThread** and **GPU** against the frame budget.
   - ⚠ **In VR, frame pacing makes threads wait on the XR runtime** (OpenXR's wait-frame). That time is idle, not cost. Total frame time is pinned to the refresh rate regardless of load. Measure each thread's actual work, excluding the waits.
7. **Quick checks without Insights** (console in the desktop window): `stat unit`, `stat unitgraph`, `stat gpu`, `stat scenerendering` (draw calls, primitives), `stat rhi`.
8. **Repeatable comparisons:** the CSV profiler (`csvprofile start` / `csvprofile stop`, written to `Saved/Profiling/CSV`) produces per-frame stats that are easy to compare across builds.
9. **Compositor view:** the SteamVR frame timing graph (or the Meta/OpenXR runtime's equivalent) shows reprojection and dropped frames, which the engine's own numbers don't.

**Record with every capture:** date, build config, hardware, headset, refresh rate, and scene/test name. Keep the trace files. Use a **fixed capture route** (same start pose, same turn, bookmarks) so runs are comparable; otherwise differences of ~0.5 ms aren't meaningful.

### Analysis tooling (`Tools/Profiling`)

One command summarizes a trace: frame pacing, real thread work with XR waits removed, render-thread hot spots, traditional vs. Nanite GPU draws, and the GPU pass breakdown:

```powershell
& "C:\Dev\AO\Tools\Profiling\analyze_trace.ps1" -Trace "C:\Users\Jordan\AppData\Local\UnrealEngine\Common\UnrealTrace\Store\001\<name>.utrace"
```

- `export_trace.ps1` runs Unreal Insights headless (`-NoUI -AutoQuit -ExecOnAnalysisCompleteCmd`) to export per-thread timing events to CSV. Insights strips backslashes from export paths, so the script passes forward slashes.
- `TraceAnalyzer.cs` (compiled at runtime) does the statistics; PowerShell alone is too slow for traces of a few hundred MB.
- Options: `-FrameBudgetMs` (default 8.333 = 120 Hz), `-SkipSeconds` (default 5, skips startup), `-OutDir`, `-SkipExport` to re-analyze already-exported CSVs, **`-IncludeWorkers`** to also export the task-graph worker threads (Foreground/Background Workers) and summarize their work. Use it whenever a main thread shows a large wait (`WaitForVisibilityTasks`, `TickCompletionEvents`, `WaitForTasks`): the waits only show that the main thread was blocked, while the workers show on what. Example: the render thread's "visibility" time turned out to be workers waiting on GPU occlusion query results (`GPUBound_WaitingForGPUForOcclusionQueries`), not CPU culling work.
- How to read it: in VR, total frame time sits at the refresh rate. Look at "GameThread actual work", render-thread scene render, and GPU passes. "GPU traditional draws" should be near zero when all scenery is Nanite.

### Testing gotchas found so far

- **Verify what a packaged run actually used:** the packaged log records applied CVars (`Set CVar [[r.ForwardShading:...]]`) and material problems (`missing usage flag`). Check it before trusting a comparison. Two Nanite runs were invalid because the foliage material lacked a usage flag (see [MeshAuthoring.md](MeshAuthoring.md)).
- **Find non-Nanite geometry in a packaged build:** console `ShowFlag.NaniteMeshes 0` hides Nanite meshes; anything still visible is drawn traditionally.
- **`-nohmd`** runs the same build without VR (single view). Useful to rule VR stereo in or out; costs come out roughly half the VR numbers.
- **SteamVR must not run elevated:** if `steam.exe` / `vrmonitor.exe` are set to *Run as administrator*, a normal-privilege game or editor can't connect to SteamVR (`Access is denied` on the `SteamVR_Namespace` pipe), hangs for minutes at OpenXR startup, and may start a second vrserver. Fixed 2026-10-24 by removing those compatibility flags. Diagnose via `C:\Steam\logs\xrclient_<app>.txt`.
- **Steam Link encoder errors** (`NVENC: Invalid Level` in `C:\Steam\logs\driver_vrlink.txt`) can stall SteamVR. The PC-side `targetBandwidth` in `C:\Steam\config\steamvr.vrsettings` overrides the headset's "auto" setting.
- **Check which OpenXR runtime a packaged run used (found 2026-11-13):** the log line `Initialized OpenXR on <runtime>` names it. The active runtime had switched to Virtual Desktop's (`VirtualDesktopXR`) without anyone choosing it; with the headset not streaming through Virtual Desktop, every run logged `Failed to get an OpenXR system, please check that you have a VR headset connected.` and fell back to flat-screen: no `xrWaitFrame`, one view, uncapped (157 fps). Set SteamVR back as the OpenXR runtime (SteamVR → Settings → OpenXR). A trace without `xrWaitFrame` isn't a VR run.
- **Repackage after changing an asset:** a build cooked at 19:55 doesn't contain an asset saved at 19:57. Compare the paks' timestamps (`Packaged/<build>/AO/Content/Paks`) with the asset's before tracing; one Nanite run was invalid because of this.
- **PowerShell launch quoting:** commas in `-trace=default,gpu,...` need quotes: `& "...\AO.exe" "-trace=default,gpu,frame,bookmark" "-tracehost=127.0.0.1"`.

### Test log

Trace files live in `C:\Users\Jordan\AppData\Local\UnrealEngine\Common\UnrealTrace\Store\001`. All at 120 Hz on Quest 3 via Steam Link. GPU = summed render passes (both eyes).

| Trace | Scene / config | Result |
|---|---|---|
| `20261024_090704` | VR template level; package predated the config changes | 119.8 fps; game 1.20 ms, render 1.29 ms, GPU ~2.0–2.7 ms. Superseded |
| `Baseline_NoNanite` | Empty level, forward, new config | 119.7 fps; game 1.16, render 0.98, GPU ~1.4 ms (post-processing 0.58, base 0.33, dynamic shadows 0.27) |
| `Test_TreesOnly_NoNanite` | 1× trees, forward, VSM off | 119.8 fps, 0.12% over 12.5 ms; game 1.28, render 1.25, GPU ~2.9 ms |
| `Test_TreesOnly_Nanite` | 1× trees, deferred + VSM + Nanite | **Invalid**: foliage material missing usage flag (drawn traditionally). Redo |
| `Test_TreesOnly2_NoNanite` | 4× map + trees, forward, VSM off | **85.2 fps, 40.7% over 12.5 ms**; game 2.04, render 9.95 (visibility 8.95), GPU ~9.3 ms |
| `Test_TreesOnly2_Nanite` | 4×, deferred + VSM + Nanite | **Invalid** (same usage-flag bug): 110.7 fps |
| `Test_TreesOnly2_Nanite_nohmd` | Same build, `-nohmd` | Diagnostic: traditional draws present without VR too, ruling out stereo |
| `Test_TreesOnly2_Nanite_ISMMatTurnedOn` | 4×, deferred + VSM + Nanite, flag fixed | **119.9 fps, 0.05%**; game 1.40, render 3.40 (visibility 1.59), GPU ~3.5 ms, traditional draws 0.10 ms |
| `Test_TreesOnly2_Nanite_DynShadowsOn` | As above; light: static shadows off, dynamic on | **119.8 fps, 0.07%**; game 1.30, render 2.77 (visibility 0.93), GPU ~3.6 ms. Baseline for the character tests |
| `Test_TreesOnly2_Nan_SKM_*` (5 runs, 2026-10-26) | + 100 static SkeletalMeshActors, tick on, various shadow/tick settings | Game ~3.4 ms (skeletal mesh tick ~20 µs/character); GPU +1.2 ms (SKM base pass 0.6 + velocity 0.6). Shadow results unreliable (characters too far for shadows to render). See AI.md |
| `…SKM_ShadowsWorking_NoTickSpawnAtLevel_StaticRender` | 100 close Render Static characters, shadows on | Game 1.57, render 4.44 (p99 6.71; wait on GPU occlusion queries 2.6 ms), GPU ~4.9 ms |
| `…SKM_NoShadows_NoTick_SpawnAtLevel_StaticRender` | Same, shadows off | Game 1.43, render 3.26 (occlusion-query wait 1.4 ms), GPU ~4.8 ms |
| `…SKM_Shadows_OcclusionQueryOffBuffer2` | Same as shadows-on run + `r.AllowOcclusionQueries 0` | **120.0 fps, 0%**; game 1.43, **render 1.91 (p99 2.21)**, GPU ~4.8 ms |
| `…SKM_Shadows_OcclusionQueryBuffer2` | Shadows on, queries **on** + `r.NumBufferedOcclusionQueries 2` | **120.0 fps, 0%**; game 1.42, **render 1.92 (p99 2.36)**, wait 0, GPU ~4.8 ms. **Current baseline / recommended setting** |
| `…SKM_NoShadows_OcclusionQueryOffBuffer2` | Shadows off + queries off | 120.0 fps, 0%; game 1.47, render 1.94, GPU ~4.8 ms |
| `…SKM_Shadows_Animated` | 100 close characters, Animation Blueprint, shadows on, buffered queries (from ini) | 119.8 fps, 0.11%; **game 3.44** (~20 µs/character), render 1.99, workers ~2.9 ms animation, **GPU ~5.3 ms** (VSM shadows 0.35 → 0.74 ms). Current baseline for animation work |

## VR viability test

**Question:** how many AI and static meshes can one large map hold at the target refresh rate, using the color-region single-material pipeline?

**Test map:** a large open map (World Partition) with configurable spawn counts. Toggle counts at runtime (console variables or a debug menu) so one build covers every step. Separate *render* cost from *logic* cost by testing them independently first.

| Test | What varies | What it isolates |
|---|---|---|
| A. Static meshes | 1k / 5k / 10k / 50k visible props, color-region material; individual actors vs. instanced (ISM/HISM); shadows on/off | Draw calls, instancing benefit, shadow cost |
| B. AI rendering | 10 / 25 / 50 / 100 / 200 animated skeletal characters in view, no AI logic; shadows on/off; Update Rate Optimization and AnimationSharing on/off | Skinning, animation, per-character draw cost |
| C. AI logic | Same counts, offscreen or not rendered, with CharacterMovement + navigation + the Purpose system | Game-thread cost of movement and decisions |
| D. Combined | Realistic mix of A–C at the counts that passed | Whether they fit together |
| F. Replication | N replicated AI on a listen-server host with 1–3 clients, Iris vs. generic replication (runtime toggle) | Host game-thread and bandwidth cost of replication (see [Networking.md](Networking.md) → "Replication system: Iris") |
| E. Polycount | Character/prop triangle counts at 1× / 2× / 4× / 8× (see [Rendering.md](Rendering.md) → "Polycount scaling test") | How much geometry the textureless pipeline affords |

**Measure:**
- On the **target headset**, in a **packaged Development or Test build**. PIE and the editor distort performance badly.
- `stat unit` (Game / Draw / RHI / GPU), `stat gpu`, `stat scenerendering` (draw calls, primitives), `stat anim`, `stat game`.
- **Unreal Insights** traces for anything over budget, to see exactly what's slow.
- Record per test: the count at which each thread crosses the practical frame target.

**Initial target counts (placeholders until decided):** to be set from the vision. Suggested starting point to compare against: ~50 AI visible, ~150 AI active map-wide, 10k+ visible props.

## Open questions

- Target hardware: PCVR, Quest standalone, or both. Decides much of the above.
- Target refresh rate (72 / 90 / 120 Hz).
- Anti-aliasing under deferred (MSAA is forward-only): none, TAA or TSR.
- Substrate vs. legacy materials (test).
- Actual target AI and prop counts.
