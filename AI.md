# AI

Everything about AI: the Purpose (utility AI) system's design and behavior, its performance, networking, and rendering AI at scale.

**Source:** the Purpose system currently lives in the Lyra Warzone plugin: `C:\Dev\Epic Games\UnrealEngine\Samples\Games\Lyra\Plugins\GameFeatures\Warzone\Source\WarzoneRuntime\Public\Purpose\`.

## Goal

DMZ-style density: many AI that stay interesting, in player-hosted sessions where the host's hardware is the limit. Every section below is weighed against that.

## Current direction

- Land the Purpose system fixes under "Performance" first. The load-balancer bug and the fixed-sleep throughput cap change how the system behaves under real load, not just its efficiency.
- Separate perception from decision (see "Behavior design") before scaling up AI density.
- Render distant AI cheaply: kit as attached static meshes, and baked-animation static meshes for distant bodies (see "Rendering AI at scale").

## Design: how the Purpose system works

1. Every action (an ability activating: a player turning, an NPC pathfinding, a door opening) raises an **`Occurrence`**: a set of candidates, a `SubjectMap` (Instigator, EventTarget, Candidate, Context → objects holding DataChunks), and context data.
2. The occurrence is queued to one of several background **`FPurposeEvaluationThread`**s, chosen by queue depth.
3. The thread scores every cached **`FPurpose`** against every candidate using weighted **`UCondition`**s that read DataChunks from the subjects.
4. The winning purpose is sent back to the game thread via a `TGraphTask`. `ProvidePurposeToOwner` ends the current behavior and performs the purpose's **`UBehavior_AI`** gameplay ability.
5. Activating that ability raises a new `Occurrence`, so the system perpetuates itself.

The system runs wherever the AI is authoritative. Clients see the results through ordinary GAS replication (ability activation, montages, cues, attributes); the event stream itself is never replicated (see "Networking").

## Behavior design

### Perception vs. decision separation

Originally proposed: cascade nearby-AI reactions off the target AI's own reaction (target gets hit → target reacts → nearby AI react to the target's new state). Problem: if the target's current purpose already outscores reacting to the new threat (e.g. mid-execution of something more important), it doesn't react. Under that design nothing propagates to nearby AI either, even though "an ally got shot at" should be perceptible regardless of what the target personally chooses to do about it.

**Direction:** separate *perceiving/announcing* an event from *acting* on it. Fire both off the disturbance itself, in parallel, not sequentially off the target's decision. Two implementation options, not yet chosen between:
- A dedicated lightweight "announcement" action/layer that broadcasts to nearby candidates without interrupting their current purpose.
- Push a context DataChunk update (e.g. "PerceivedThreat") onto nearby candidates and let the *existing* utility scoring decide whether it now outweighs their current purpose. More consistent with the system's utility-AI philosophy (let scoring decide, don't hard-force), with fewer new concepts to maintain.

### World events as occurrences

Destruction and other world changes should raise occurrences too: a building breaking creates new sightlines, cover and paths that AI should react to (relevant to the project's EQS cover/flank queries). These must be keyed off the **server-side** discrete event, never off client-local cosmetic state. See [Physics.md](Physics.md).

## Performance

### Purpose system fixes

Concrete fixes from reviewing the Purpose/Occurrence code. Ordered by impact.

#### 1. Background-thread load-balancer drops occurrences under load (bug, not just perf)

`PurposeSystem::Occurrence()` (both overloads, `PurposeEvaluationThread.h`) selects the least-loaded `FPurposeEvaluationThread` using:

```cpp
int leastNumQueuedPurposes = 0;
...
if (thread->GetNumQueuedPurposes() <= leastNumQueuedPurposes)
```

Because the threshold starts at `0` and never rises, a thread is only ever selected while it has zero items queued. Once every background thread has ≥1 queued occurrence, `selectedThread` stays `nullptr` and the occurrence is silently discarded instead of load-balanced. This is likely the cause of AI appearing to "ignore" occurrences under any real load.

**Fix:** use an explicit "not yet initialized" sentinel instead of reusing `0`:

```cpp
int leastNumQueuedPurposes = -1; // queue counts are never negative, so -1 is unambiguous
for (FPurposeEvaluationThread* thread : backgroundThreads)
{
    ...
    if (leastNumQueuedPurposes == -1 || thread->GetNumQueuedPurposes() < leastNumQueuedPurposes)
    {
        leastNumQueuedPurposes = thread->GetNumQueuedPurposes();
        selectedThread = thread;
    }
}
```

#### 2. Fixed-interval sleep caps per-thread throughput regardless of backlog

`FPurposeEvaluationThread::Run()` (`PurposeEvaluationThread.cpp`) dequeues exactly one `FOccurrence`, evaluates it against every candidate, then always sleeps `tickTimer` (50 ms) before looping, even when the queue is backed up. Combined with #1, this caps each thread at ~20 occurrences/sec, and the cap bites exactly when the self-perpetuating event system generates more load. (There was a specific reason for this timer originally; annotate it in code if it's recovered.)

**Fix:** replace the poll-sleep with an `FEvent`-driven wake, and drain the whole backlog per wake:

```cpp
// Member: FEvent* WakeEvent = nullptr;

bool FPurposeEvaluationThread::Init()
{
    WakeEvent = FPlatformProcess::GetSynchEventFromPool(/*bIsManualReset=*/false);
    return true;
}

void FPurposeEvaluationThread::Exit()
{
    if (WakeEvent) { FPlatformProcess::ReturnSynchEventToPool(WakeEvent); WakeEvent = nullptr; }
}

void FPurposeEvaluationThread::Stop()
{
    stopThread = true;
    if (WakeEvent) { WakeEvent->Trigger(); } // wake immediately instead of waiting out the timeout
}

bool FPurposeEvaluationThread::QueuePurpose(FOccurrence potentialPurposesToQueue)
{
    if (potentialPurposeQueue.Enqueue(MoveTemp(potentialPurposesToQueue)))
    {
        numQueueItems++;
        if (WakeEvent) { WakeEvent->Trigger(); }
        return true;
    }
    return false;
}

uint32 FPurposeEvaluationThread::Run()
{
    while (!stopThread)
    {
        WakeEvent->Wait(FTimespan::FromMilliseconds(250)); // timeout is just a safety poll for stopThread

        FOccurrence purposeToEvaluate;
        while (!stopThread && DequeuePurpose(purposeToEvaluate))
        {
            SelectPurposeIfPossible(purposeToEvaluate);
        }
    }
    return 0;
}
```

#### 3. No relevance/spatial filtering before scoring: split into targeted + perception broadcast

Every `Occurrence` evaluates against the entire candidate cache the thread knows about (`SelectPurposeIfPossible`, `PurposeEvaluationThread.cpp:77`). Cost per occurrence is O(candidates × purposes × conditions), and since every action can spawn a new occurrence, this product is the main driver of runaway growth.

**Fix:** split dispatch into two independent, parallel paths, both fired off the disturbance itself (not off whether the target's purpose changes). This is the implementation side of "Perception vs. decision separation" above.
- **Targeted:** if the occurrence names a specific target (e.g. "player shot AI X"), route directly to X's `UPurposeAbilityComponent`. O(1), no candidate-cache scan.
- **Perception/disturbance broadcast:** unconditionally push a context update (e.g. a "PerceivedThreat"/"AllyUnderFire" DataChunk) to candidates within a radius via a spatial query (overlap sphere, or a coarse grid/cell lookup), and let utility scoring decide. Don't gate this on X's `ProvidePurposeToOwner` outcome.

This also bounds the "every event spawns an event" recursion spatially instead of fanning out to every AI in the level.

#### 4. Ability duplication on every purpose selection

`GiveAbilityDuplicate` (`PurposeAbilityComponent.cpp:260`) calls `DuplicateObject` to create a fresh `UGameplayAbility` every time a purpose is selected, because each `FPurpose` asset owns an `Instanced` template of `behaviorAbility` with purpose-specific property values. That's UObject construction + property copy on the game thread per AI decision, plus GC pressure.

These abilities run entirely server-side; GAS client prediction/rollback only applies to player-input-driven activation, so reuse can't interfere with it. **Fix:** cache the duplicated instance keyed by `(actor, source FPurpose)` the first time that purpose is performed by that actor, and reuse it on later selections. Only duplicate when that actor has never performed that purpose before.

#### 5. Linear array scans for every DataChunk lookup

`Condition::RetrieveConditionDataChunks` (`Condition.h`) does `TArray::Contains`/`FindByKey` against each subject's `FDataMapEntry` array, per condition, per purpose, per candidate, per occurrence, and scans the same array twice per lookup.

**Fix:** keep `TArray<FDataMapEntry>` as-is. It's required for the `Instanced` editor workflow (DataChunks created on demand per data asset) and for replication. Build a disposable `TMap<UClass*, UDataChunk*>` lookup index once per candidate, where `subjectMapForCondition` is already assembled (`PurposeEvaluationThread.cpp:147`):

```cpp
// built ONCE per candidate
TMap<ESubject, TMap<UClass*, UDataChunk*>> lookupIndex;
for (const auto& [subject, dataArray] : subjectMapForCondition)
{
    TMap<UClass*, UDataChunk*> chunkLookup;
    for (const FDataMapEntry& entry : dataArray)
    {
        chunkLookup.Add(entry.Chunk->GetClass(), entry.Chunk);
    }
    lookupIndex.Add(subject, chunkLookup);
}

// then inside RetrieveConditionDataChunks:
if (UDataChunk** found = lookupIndex[primarySubjectType].Find(PrimaryData()))
{
    primary = *found;
}
else { return false; }
```

Every condition evaluation becomes an O(1) hash lookup instead of an O(n) scan, with no change to how data is authored or replicated.

#### 6. Full `FPurpose` struct copy on every score improvement

`SelectPurposeIfPossible` copies the whole `FPurpose` (`highScorePurpose = potentialPurpose;`) every time a new high score is found, rather than once at the end. An index variable for this already exists in the code. Restore its use: track the winning index during the loop and copy once after the winner is decided.

#### 7. Verify logging cost in the hot evaluation loop

Several `Global::Log` calls in `SelectPurposeIfPossible`'s innermost per-condition loop build `FString::Printf`/`EnumValueOnly` arguments. If `Global::Log` is a plain function rather than a macro that checks verbosity first, those strings are built on every condition evaluation even when that verbosity is disabled. Confirm `Global::Log`'s definition and convert it to a short-circuiting macro if needed.

### Rendering AI at scale

Rendering many AI is its own cost, separate from decision-making. General character composition rules are in [Rendering.md](Rendering.md) → "Character composition"; this covers the AI-specific parts.

**Kit as attached static meshes.** Gear (helmets, weapons, packs) as static meshes attached to sockets, colored via Custom Primitive Data. Many AI carrying the same mesh can share one instanced draw per pass, each with its own colors and animation state. Separate skeletal meshes never merge this way. Cost to watch: every attached part's transform is updated each frame to follow its socket, which becomes game-thread work with dozens of AI. Keep attached parts per AI few.

**Bodies as baked-animation static meshes (distant AI).** The standard large-crowd technique (Epic's AnimToTexture plugin, used in the City Sample crowds): animation is baked into data the vertex shader plays back, and hundreds of AI render as a handful of instanced draws with no skeletal skinning.
- It uses textures, but for animation data read per vertex, not surface detail read per pixel. It doesn't undo the textureless art style's pixel-cost savings.
- No skeleton at runtime: no IK, no ragdoll, no bone-attached kit, limited animation blending. Hit detection needs separate collision shapes.

**Distance-based swap:** AI near the player use full skeletal meshes (IK, ragdoll, precise hits); distant AI switch to baked-animation static meshes. Most AI on a large map are far away at any given moment, so this is where the density goal gets most of its headroom.

### Movement simulation LOD (off-screen / distant / unloaded AI)

**Idea (2026-10-26):** when an AI isn't visible, stop its real movement (CharacterMovement, per-tick path following) and instead track an *approximate* position cheaply: store its path, speed and start time, and compute where it would be from elapsed time (a timer, with the bookkeeping on a background thread). When it becomes relevant again, place it at that computed position and resume full simulation. The same mechanism covers distant AI and AI in map sections that are streamed out.

Notes for when this gets built:
- **The computation is cheap and analytic:** position = distance along the stored nav path at `speed × elapsed time`, so it only needs evaluating when queried (re-materializing, perception checks, map-wide events). A background thread is useful for batch-updating many agents (e.g. keeping a spatial grid current for occurrences/perception), not because each update is expensive.
- **Multiplayer: "visible" must mean visible to *any* player.** The authoritative machine can't use one player's view. Use distance/relevance to all players (with hysteresis so agents don't flip back and forth), not per-view visibility.
- **Re-materialize before entering view:** promote to full simulation at a larger radius than the view distance (or frustum plus a margin), so agents never visibly pop into place. Project the computed position onto the nav mesh first.
- **Promote on involvement:** an abstracted AI touched by an event (shot at, perceives a disturbance, joins a fight; see "World events as occurrences") switches to full simulation immediately.
- **Unloaded sections:** the AI has to exist as a lightweight record rather than an Actor while its section is streamed out, with the Actor spawned when relevant. The same "records, not Actors" model as the item pool in [Networking.md](Networking.md).
- **Decisions at lower frequency too:** the Purpose system can evaluate abstracted AI less often (or only on promotion), not just skip movement.
- **Replication:** abstracted AI don't need replicating until promoted (Iris relevance).
- **Precedent:** this is "simulation LOD", the same idea as Epic's Mass framework (MassAI runs cheap, lower-fidelity simulation for distant agents and spawns full Actors only near players) and long-standing open-world crowd systems. Mass is worth evaluating as the implementation instead of building it from scratch.
- Pairs with the rendering-side distance swap (baked-animation bodies for distant AI) and the shadow LOD below.

### Character shadows

Animated skeletal meshes are expensive shadow casters, especially with Virtual Shadow Maps (the current Nanite setup; see [Rendering.md](Rendering.md)). An animated mesh **invalidates its shadow pages every frame, even standing still**, because idle animation counts as change, and skeletal meshes take the non-Nanite shadow path (roughly 1.6× the Nanite path per page, per the VSM article in Rendering.md). With dense AI this multiplies. Options:

- **Hidden static shadow proxy:** the visible animated mesh has *Cast Shadow* off; a simple static blockout attached to the character (ideally a Nanite static mesh) has *Visible* off and **Cast Hidden Shadow** on. The same trick is used for wind-animated trees.
  - Saves: no skinning in the shadow pass (cheap static geometry instead), and **idle characters stop invalidating shadows**, since a static proxy only invalidates when the character actually moves. Big win for AI standing guard or idling in groups.
  - Doesn't save: a *moving* character still invalidates every frame (any moving caster does). For running AI the gain is only the cheaper geometry.
  - Cost: the shadow doesn't animate (legs/arms become a rigid shape), which can look wrong close to the player in VR.
- **Capsule shadows (built into UE):** assign a *Shadow Physics Asset* (a few capsules on the main bones) to the skeletal mesh and enable *Cast Capsule Direct Shadow*. The shadow follows the animation but is computed from capsules, far cheaper than rendering the skinned mesh into shadow maps. Designed for many characters. Verify how it looks with the project's lights and VSM before committing.
- **No character shadows** beyond some distance.

**Direction: shadow LOD by distance.** Near the player: real skinned shadows or capsule shadows (where shadows get noticed). Mid distance: capsule shadows or the static proxy. Far: none. This pairs with the baked-animation swap for distant AI, which are rigid meshes anyway.

### AI rendering test plan

Run on the 4× Nanite baseline scene (see [Engine.md](Engine.md) → "Test log"), stepping AI counts (e.g. 10 / 25 / 50 / 100), using the same capture route every time:

1. **Skeletal meshes, not animated:** per-character draw and visibility cost. Skeletal meshes aren't Nanite, so this shows the traditional path returning as counts grow.
2. **Animated skeletal meshes:** adds animation evaluation, skinning and VSM invalidation. At one fixed count, run four shadow variants:
   1. Full skinned shadows
   2. No character shadows (the upper bound on savings)
   3. Hidden static proxy shadows
   4. Capsule shadows
3. **Animated AI running behaviors:** adds game-thread cost (CharacterMovement, navigation, the Purpose system) on top of rendering.

Also worth an A/B once animation is in: Update Rate Optimization and AnimationSharing on/off.

**Results so far (2026-10-26, 100 non-animated SkeletalMeshActors, 4× Nanite scene):**

| Cost | Total (100 SKMs) | Per character |
|---|---|---|
| Game thread: `USkinnedMeshComponent::TickComponent` (0.95 ms) + waiting on parallel animation tasks | ~2.0–2.15 ms | ~20 µs |
| GPU: SKM base pass (traditional draws, stereo) | 0.60 ms | ~6 µs |
| GPU: velocity pass (`RenderVelocities`, new with skeletal meshes) | 0.58 ms | ~6 µs |
| Shadows on: render-thread visibility (`ProcessVisibilityTasks` 0.20 → 2.17 ms) | ~2.0 ms | ~20 µs |
| Shadows on: GPU shadow passes | ~0.15 ms | ~1.5 µs |

- **Skeletal meshes tick every frame even with no animation playing.** Static characters should have the SkeletalMeshComponent's tick disabled.
- **Render Static + spawning at runtime removes the tick (2026-10-26):** `USkinnedMeshComponent_TickComponent` dropped from ~102×/frame to 2 (the hands). Game thread: **1.68 ms** vs 3.41 ms ticking vs 1.30 ms baseline, so 100 static, non-ticking characters cost **~0.4 ms game thread (~4 µs each)**. GPU unchanged: base pass 0.66 ms, and the velocity pass (~0.6 ms) still draws even though nothing moves.
- **Render-thread `WaitForVisibilityTasks` is a wait, not a cost.** It measures the render thread waiting on visibility work running on worker threads, so it shrinks when the game thread is slow (the work finishes first) and grows when the game thread is fast (Render Static run: 1.36 ms avg, 1.23 median, vs ~0 median while ticking). This explains the inconsistent visibility numbers across runs. Measure the worker-thread visibility work directly before drawing conclusions from it.
- **Render-thread "visibility" time is a wait on GPU occlusion query results (worker-thread export, 2026-10-26, 100 close Render Static characters):** worker threads show `OcclusionCullPipe` → `GPUBound_WaitingForGPUForOcclusionQueries` at **1.52 ms/frame (shadows off) vs 2.68 ms (shadows on)**, while actual visibility computation (`ComputeViewRelevance`) is ~0.08 ms. Non-Nanite meshes (the skeletal meshes) are culled with hardware occlusion queries; the visibility task waits for the GPU to return them, and the render thread waits on that task (`WaitForVisibilityTasks` 1.44 / 2.61 ms; scene render 3.26 / 4.44 ms). Shadows lengthen the wait because the GPU is busier, not because of more CPU work; GPU shadow passes barely changed (~0.82 → 0.85 ms). Nanite does its own GPU occlusion culling, so Nanite-only scenes don't show this. **Next A/B:** `r.NumBufferedOcclusionQueries 2` (read results from 2 frames ago, so less waiting and one frame more latency for newly revealed meshes) and `r.AllowOcclusionQueries 0` (no queries, no wait; occluded meshes get drawn instead, costing GPU).
- **Occlusion queries off removes it (2026-10-26):** with `r.AllowOcclusionQueries 0` (plus `r.NumBufferedOcclusionQueries 2`, which does nothing once queries are off), render-thread scene render dropped to **1.91 ms with shadows on / 1.94 ms off** (p99 2.21 ms, from 6.71 ms), `WaitForVisibilityTasks` → 0, **0 frames over 12.5 ms**. GPU unchanged (~4.8 ms; HZB slightly cheaper), because little occludes the characters in this open terrain. **Follow-up, same day: `r.NumBufferedOcclusionQueries 2` with queries left on matched it** (render 1.92 ms, p99 2.36 ms, wait 0) while keeping occlusion culling, so that's the recommended setting (see Engine.md). **Static character shadows are effectively free** once the wait is gone (Render Static meshes' shadow pages stay cached). Scene totals at this point (4× Nanite foliage, dynamic shadows, 100 static characters): game 1.43 ms, render 1.91 ms, **GPU ~4.8 ms (now the largest)**. Biggest removable GPU item: skeletal mesh velocity pass ~0.6 ms.
- **Animated, shadows on (2026-10-26, 100 close characters with an Animation Blueprint, buffered occlusion queries):** game thread **3.44 ms** (vs 1.42 static): `CompleteParallelAnimationEvaluation` 0.79 ms + waiting on parallel animation tasks 0.64 ms + component tick ~0.3 ms, **~20 µs/character, about the same as the ticking non-animated run (3.41 ms)**, so the game-thread cost is mostly per-component tick/completion overhead, not animation math. Animation itself runs on **worker threads, ~2.9 ms total across cores** (Animation Blueprint `NewAnimBlueprint_C` 0.37 ms, render-state updates 0.47 ms). Render thread 1.99 ms (unchanged). **GPU ~5.3 ms (+0.4 ms): VSM shadow re-rendering 0.35 → 0.74 ms**, since animated meshes invalidate their shadow pages every frame. Frames over 12.5 ms 0.11%. Linear extrapolation to the ~7 ms target: GPU is the first limit (~100 more visible animated characters), game thread ~175 more, before AI logic. **Next levers:** Update Rate Optimizations + *Only Tick Pose when Rendered*; *Use Animation Asset* vs. Animation Blueprint; character shadow LOD; the velocity pass (~0.6 ms GPU).
- **Shadows were a distance issue, not broken:** character shadows render in game when the characters are close; the earlier "shadows not rendering" runs had characters spawned too far away. Shadow distance settings not yet tuned.
- **⚠ The far-spawned shadow on/off comparisons below are unreliable:** in those runs the characters were too far away for their shadows to render, so "shadows on" wasn't really on. Use the close-layout runs above instead.
- **Character shadows appeared cheap for static characters.** With all 100 kept in view and shadows on (2026-10-26, montage-only tick run), render-thread visibility averaged 0.27 ms vs 0.20 ms without shadows; the earlier 2.17 ms run was an outlier (visibility swings with how many characters are in view). Shadow cost for 100 static characters: ~0.07 ms render thread + ~0.15 ms GPU. Expect this to change with animation, since animated meshes invalidate VSM pages every frame.
- **`VisibilityBasedAnimTickOption = OnlyTickMontagesAndRefreshBonesWhenPlayingMontages` doesn't remove the tick:** `USkinnedMeshComponent_TickComponent` still ran ~102×/frame (100 characters + 2 hands) at ~10 µs each (~1 ms/frame). Game thread stayed at ~3.4 ms.
- **Disabling tick didn't take effect in the 2026-10-26 "NoTick" runs.** The spawner (`MeshArrayPlacer.PlaceArray`) calls `SetActorTickEnabled(false)` / `SetComponentTickEnabled(false)` right after spawning. If it runs in the editor, that only changes the live tick state, not the saved *Start with Tick Enabled* property, so the components tick again at game start. Options: **Render Static** (`bRenderStatic`, a saved property, so it persists when set from an editor-run Blueprint; `USkeletalMeshComponent` then disables its own tick, `SkeletalMeshComponent.cpp:1068`), a SkeletalMeshActor child Blueprint with *Start with Tick Enabled* off in its class defaults, or running the spawner at runtime. Verify in the packaged build with `DumpTicks`.
- **The velocity pass is only needed for temporal AA (TAA/TSR) or motion blur.** Check which AA the deferred builds actually use (MSAA, the configured method, is forward-only); if not temporal, velocity output is ~0.6 ms of wasted GPU.
- Frame pacing held 120 Hz in both runs.

**Next runs:** tick off + shadows off (fixed cost floor); then animated with *Use Animation Asset* (pure evaluation + skinning), then an Animation Blueprint (its overhead), with Update Rate Optimizations / *Visibility Based Anim Tick Option* A/B.

## Networking

### Purpose system across client-authoritative regions

If the project goes the P2P/client-authoritative-region route (see [Networking.md](Networking.md)), the question is what needs to cross the network for the Purpose system to work across peers.

**Current thinking:** don't replicate `Occurrence`/`FContextData` structs. That's an unbounded, self-perpetuating event graph and would be crushed by bandwidth. Instead:
- The condition-relevant state (`UPurposeAbilityComponent::data`) is already `DOREPLIFETIME`'d as ordinary actor state, for reasons unrelated to the Purpose system (visibility, GAS, animation).
- A remote or region-owning node can reconstruct an equivalent `Occurrence` **locally** from state it already has, if it receives a small trigger notification (who/what/when: Instigator handle, EventTarget handle, ability/purpose identifier).
- Anything created only at occurrence time with no persistent replicated home (e.g. `UTimeOfOccurrence`) either rides along in the trigger payload or is reconstructed deterministically on the receiver (e.g. stamped with the receiver's synced clock).

**Caveat:** replication is never instantaneous, so a remote reconstruction always works from a slightly stale snapshot. That's fine for ambient reactions. It's not fine once a reconstructed decision has stakes (target selection, damage), because a non-authoritative node would be making a trusted decision off state it controls: the client-hosting trust problem moved to the AI layer.

### Host migration

Host migration snapshots must include alive AI and their Purpose state (current behavior, relevant DataChunks) so the new host can resume them. See [Networking.md](Networking.md) → "Host migration".

## Open questions

- Where is the line between "ambient, can be regionally reconstructed" and "has stakes, must be arbitrated by a single authority"? Not yet defined.
- Perception broadcast: dedicated announcement layer vs. a DataChunk update fed into existing scoring.
- Baked-animation AI: at what distance to swap, and how to handle hit detection on distant AI.
- What exactly needs serializing from the Purpose system for a host migration snapshot.
