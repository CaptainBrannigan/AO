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
