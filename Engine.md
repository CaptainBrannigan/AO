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

**Decision needed first: target hardware.** PCVR only, Quest standalone, or both. Standalone changes nearly everything: no Nanite, mobile shading path, much smaller budgets for draw calls, triangles and memory.

## Plugin stripping

The engine enables well over 150 plugins by default (169 counted in the 5.7 install), on top of the template's own (OpenXR, OpenXREyeTracker, OpenXRHandTracking, PICOController). Disable them by adding `"Enabled": false` entries in `AO.uproject`. Editor-only plugins never ship in a packaged game, so they matter for editor speed and stability, not frame time.

**Applied 2026-10-23:** 38 plugins disabled in `AO.uproject`, checked against the source engine for dependency conflicts (none). Kept enabled because other plugins depend on them: GeometryCache and VariantManagerContent (Interchange, Dataflow), MetaHumanSDK (Bridge). Source control and IDE integration plugins were re-enabled after disabling PerforceSourceControl stopped the editor from opening. They're editor-only with no runtime cost, so they stay enabled. After changing the plugin list, rebuild the editor target (`Build.bat AOEditor Win64 DebugGame -Project=...`) before opening.

**Source-engine gotcha (shared binaries):** AO's editor target builds into the source engine's shared `Engine/Binaries`. Building it can give the engine a new build ID while only refreshing module manifests for the plugins AO enables. Opening the engine on its own, or another project (e.g. Lyra) whose plugin set differs, then fails with "module missing / built with a different engine version" for plugins outside AO's set (seen with CompositeCore). Fix: rebuild the engine's default editor target (`Build.bat UnrealEditor Win64 Development -WaitMutex`), then rebuild `AOEditor`, or accept the editor's rebuild prompt. The first open after plugin or renderer changes can freeze for a while while shaders recompile; that's expected.

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

**Record with every capture:** date, build config, hardware, headset, refresh rate, and scene/test name. Keep the trace files.

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
- Substrate vs. legacy materials (test).
- Actual target AI and prop counts.
