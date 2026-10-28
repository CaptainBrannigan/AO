# AI

Everything about AI: the Purpose (utility AI) system's design and behavior, its performance, networking, and rendering AI at scale.

**Source:** the Purpose system now lives in the AO module: `C:\Dev\AO\AO\Source\AO\Purpose\` (plus `Data\`, `DataChunks\`, `Abilities\`). It was ported from the Lyra Warzone plugin (`C:\Dev\Epic Games\UnrealEngine\Samples\Games\Lyra\Plugins\GameFeatures\Warzone\`) and compiles in AO as of 2026-10-27.

## Goal

DMZ-style density: many AI that stay interesting, in player-hosted sessions where the host's hardware is the limit. Every section below is weighed against that.

- **Delegated AI decision-making:** the first player in an area runs the decision-making for that area's AI, to take load off the host. See "Networking" → "Delegated AI decision-making".

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

## Design: occurrence to reaction with GAS (proposed)

This section describes how an occurrence should travel from "something happened" to an AI reacting. GAS covers what it already does well: state, quantities, timed effects, gating, cooldowns and cosmetic replication. The Purpose system keeps what GAS has no answer to: scoring, "how long ago", and subjects. Proposed 2026-10-28; none of it is implemented yet.

### Principles

- **Behavior is defined by the conditions we can establish, and intents are the vocabulary for them.** An intent should be broad enough to be its own consideration (taking cover is a different decision from flanking) and narrow enough that designers aren't buried in variants. Conditions and references refine an intent; they don't multiply it. The code should make adding an intent cheap and adding a variant of one unnecessary.
- **Intent and reference are the primary pair; phase is secondary.** "Cover, from the Instigator" is the decision. Seeking, Moving and AtLocation only track progress on it.
- **Others read the past and the perceivable present, never the future.** An AI's intent and its upcoming tasks are private. Other AI learn them only by observing (it's moving, it's behind that wall) or by being told (an announcement). Nothing reads the upcoming tasks of a sequence.
- **Each kind of fact has one home.** Tags say what kind, attributes say how much, history says when, and subjects say who and where. `Contextual.Combat.Engaged.1–5` broke this by storing a count as a category.
- **Use GAS where it already solves the problem; keep the Purpose system where GAS has no equivalent.** GAS has no utility scoring, no "how long ago", and no subjects beyond source and target.

### Tag vocabulary

Top-level namespaces, split by how long a tag lasts and who can see it. These replace `Contextual.*`.

| Namespace | Meaning | Lifetime | Lives on | Visible to others |
|---|---|---|---|---|
| `State.*` | True right now | Removed when it stops being true | ASC owned tags: loose, granted by a duration GE, or `ActivationOwnedTags` | Yes, when perceived |
| `Event.*` | Something happened | An instant; stamped into history with a time | The occurrence's type, `FGameplayEventData::EventTag`, history entries | Only to those who perceived it |
| `Intent.*` | What a behavior is for | While the behavior runs, then an intent history entry | The behavior ability's asset tags (formerly `AbilityTags`); intent history | No, except through an announcement |
| `Action.*` | What a task is physically doing | While the task runs | The task's description (`contextualTagForTask`) | Only through its effects (moving, firing) |
| `Cooldown.*` | Can't do X yet | The cooldown GE's duration | ASC owned tags | No |
| `Data.*` | Chunk keys | n/a | `DataTags.h` | n/a |

Examples:
- **State:**
  - `State.Position.Seeking` / `Moving` / `AtLocation`. The only three phase tags.
  - `State.Combat.Engaged` / `UnderFire` / `Suppressed`.
  - `State.Health.Wounded` / `Downed`.
- **Event:**
  - `Event.Damage.Received`, `Event.Weapon.Fired`.
  - `Event.Perception.Sighted` / `Lost` / `Heard` / `Movement`.
  - `Event.Announce`, `Event.Behavior.Finished`, `Event.World.Destruction`.
- **Intent:** `Intent.Cover`, `Flank`, `Overwatch`, `Advance`, `Fallback`, `Regroup`, `Investigate`, `Patrol`, `Ambush`, `Suppress`.
- **Action:** `Action.Move`, `Action.Face`, `Action.Animate`, `Action.Query`.

**The namespace is the visibility rule.** When a subject other than the Candidate is snapshot, only its `State.*` tags are copied, through one filter container. `Intent.*` and `Cooldown.*` never leave their owner, so tags need no per-tag flags. Numbers stay private for the same reason. If others should perceive one, expose it as a banded State tag: the Health attribute stays private, and `State.Health.Wounded` is visible.

### Where each kind of data lives

| Kind | Example | Home | Why there |
|---|---|---|---|
| Current state | Moving, engaged | `State.*` owned tags | Ability gating, `showdebug abilitysystem`, ASC replication |
| Quantities | Engaged count, morale, alarm, suppression | Attributes, changed by GEs | Stacking, aggregation, durations, change delegates |
| Timed state | Engaged for 5 s | A duration GE that grants a `State.*` tag and modifies an attribute | Expires on its own, with no time math in conditions |
| When something happened | Last shot at, last saw an enemy | History entries `{Event tag, time}` | GAS has no "how long ago" |
| What I'm trying to do | Cover from the Instigator, currently moving | Intent history entries `{intent, reference, phase, time, outcome}` | Keeps intent, reference and phase paired, even with several intents in history |
| What I believe about others | Enemy last seen at X, 4 s ago | Observation chunks on the observer | Knowledge belongs to the observer |
| Places and references | Cover point, last known position | Subjects (see below) | Conditions address a place the same way as an actor |
| Don't repeat | No re-flank for 10 s | A cooldown GE, plus the behavior's blocked tags | A pre-filter that needs no condition |
| Cosmetic reaction | "Moving to cover!" voice line, hand signal | GameplayCue | Replicates to clients for free |

Attributes reach conditions through `FAttributeChunk`, which `InitializeDataForOwner` already binds to the ASC. They're copied at snapshot like any other chunk. EngagedCombatants, Morale and LevelOfAlarm become attribute chunks that read an `AttributeSet`.

AI decisions run on the host, so loose tags don't need to replicate for the Purpose system. State tags that clients need, for animation or UI, use the replicated loose-tag calls or come from GEs.

### Subjects for places and beliefs

Today anything that isn't an actor goes into Context as a chunk. Instead, let references be subjects. Then conditions such as RelativePosition or distance work the same whether the reference is an actor or a place, and a behavior can be designed around an intent plus a reference.

- **`ESubject::Reference`:** what the current intent is relative to. The behavior fills it when selected, with one of:
  - an actor (the Instigator),
  - a place (the Instigator's last known position),
  - an announcement's reference.
- **Virtual subjects:** a place has no UObject. The worker already reads `TMap<ESubject, FDataMap> subjectSnapshots`, so a virtual subject only needs a DataMap (Location, optionally ViewPoint and time seen). Only the game-thread `FSubjectMap` needs a way to carry one, as a DataMap beside the weak object pointers.
- **Beliefs over truth:** "last known position of EventTarget" is what the observer believes, not data the target holds. Store observations on the observer, keyed by whom they're about: last seen location and velocity, time seen, last announced intent.
  - **First version:** snapshot other subjects' filtered truth (State tags and position). It's cheap and shared across candidates.
  - **When stealth matters:** per-candidate belief subjects replace it, so the EventTarget a candidate scores against is what that candidate believes about it. The cost is that subject snapshots are no longer shared across an occurrence's candidates.

An intent is authored once and varied by its reference: cover from the Instigator, from its last known position, or from what an ally announced.

### The flow

#### 1. Something happens: an occurrence

Every occurrence carries an `Event.*` tag naming what happened. It can come from any of these:
- **Ability activation**, as today.
- **Gameplay effects.** Damage arrives as a GE, and its `FGameplayEffectContext` already holds:
  - Instigator → `ESubject::Instigator`.
  - EffectCauser and SourceObject → the weapon.
  - HitResult → Context (hit location and direction).

  Raise the occurrence from `OnGameplayEffectAppliedDelegateToSelf`, or from an attribute's `GetGameplayAttributeValueChangeDelegate` (Health dropped → `Event.Damage.Received`).
- **Tag changes.** `RegisterGameplayTagEvent` on a State tag: an ally gains `State.Combat.UnderFire` → an occurrence for its squad.
- **Perception.** `UAIPerceptionComponent::OnTargetPerceptionUpdated` → `Event.Perception.*`. This is the path for unannounced actions: an enemy who moves without a callout is still seen moving.
- **Announcements.** A behavior's task sends `Event.Announce` carrying an intent and a reference ("moving to cover, there"), plus a GameplayCue for the voice line.
  - Listeners get an occurrence with the announcer as Instigator and the announced place or actor as Reference.
  - It is the only way intent becomes visible to others, and it carries only what the announcer chose to say. That could be wrong, or a bluff.
- **World events:** destruction, keyed off the server-side event (see "World events as occurrences").

This settles the perception broadcast question in "Open questions": both. An announcement is an occurrence, and so is perceiving something unannounced. Neither interrupts the receiver directly; scoring decides.

#### 2. Who receives it

- **Targeted:** EventTarget receives it directly (Performance #3).
- **Broadcast:** candidates in range for that event tag. A table keyed by `Event.*` tag gives the radius and filters, for example:
  - `Event.Weapon.Fired`: audible at long range, any faction.
  - `Event.Announce`: short range, same faction.
- **Perceived:** for sight-gated events, the candidate must perceive the instigator. This is the same game-thread snapshot step as SubjectVisibility: perception first, a trace as fallback.
- **Recording on receipt:** the candidate adds a history entry `{Event tag, time}` and updates its observation of the instigator. This happens whether or not its behavior changes, which is the perception/decision separation above.

#### 3. Snapshot (game thread)

- **Candidate:** its DataMap (attributes resolve through `FAttributeChunk`), all owned tags (`GetOwnedGameplayTags()`) copied into a tags chunk, history, intent history and observations.
- **Other subjects:** position, view point and `State.*` tags only. No intent.
- **Context:** event tag, event location and direction, GE magnitude.
- **Virtual subjects:** Reference, last known positions.
- **Derived observations:** visibility between subjects, resolved now.

#### 4. Evaluation (worker)

1. **Trigger filter (new).** Each purpose declares which `Event.*` tags it reacts to, the way abilities declare `FAbilityTriggerData`. A damage occurrence never scores patrol purposes. This is the cheapest cut to the candidates × purposes × conditions cost.
2. **Tag gate.** Check the behavior ability's activation required, blocked and cooldown tags against the snapshot's copied tag container. They're protected on `UGameplayAbility`, so `UBehavior_AI` exposes them. Don't call `CanActivateAbility` here: it isn't thread-safe, but comparing tags against a copied container is.
3. **Score.** Conditions read tags (State, and the candidate's own intent history), attributes, history ages, observations and references.
4. **Compare to current.** The new score must beat the current behavior's score.

#### 5. Selection (game thread, `ProvidePurposeToOwner`)

1. End the current behavior; step 7 runs for it with the outcome Interrupted.
2. Set the new behavior and write its Reference subject.
3. **Apply OnSelected before performing the ability.** Today OnSelected runs after the first task has already started, so that task can't see Context-targeted modifiers.
4. Apply the OnSelected changes:
   - **Numeric and timed changes become GameplayEffects** applied to the subject's ASC. For example, `GE_Engaged`: 5 s, one stack per combatant, grants `State.Combat.Engaged`, +1 to the Engaged attribute.
   - **DataModifiers stay** for what GAS can't hold: history entries, references and observations.
5. Write the intent history entry `{Intent.Cover, Reference, Seeking, now}`.
6. Activate the behavior ability. Its asset tags include its `Intent.*` tag.

#### 6. Reaction (the behavior runs)

- **Tasks advance the phase.** `AT_RunEQSQuery` sets `State.Position.Seeking`; `AT_MoveTo` swaps it to `Moving`, then `AtLocation`.
  - Each swap replaces the loose State tag and updates the phase on the intent entry. No string building.
  - Loose tags must be removed when the ability ends, however it ends.
- **Tasks set their `Action.*` tag** while running.
- **An optional announce task** sends `Event.Announce` with a cue.
- **Others react only to observable consequences.** Movement and firing raise occurrences for others through perception, not through reading this AI's data.

#### 7. Finish

- Remove the loose State tags. Duration GEs expire on their own.
- Close the intent entry with an outcome. `EAbilityPurposeFeedback` already exists for this (succeeded, interrupted, failed). "Failed to reach cover 3 s ago" is something a condition can use.
- Apply the purpose's cooldown GE, if it has one (`Cooldown.Intent.Flank`).
- Raise `Event.Behavior.Finished` to self. This replaces the ad hoc occurrence in `SeekNewBehavior`.

### Worked example

A player shoots AI A. A's ally B is 20 m away. Enemy AI E is watching A from a distance.

1. **The shot.** The player's fire ability applies `GE_Damage` to A. A's Health change raises `Event.Damage.Received` (Instigator = the player, Context = hit direction). The shot also raises `Event.Weapon.Fired`, an audible broadcast.
2. **Receipt.** A receives the damage event. B hears the shot: it records `{Event.Weapon.Fired, t}` and an observation that the player was heard near X. E perceives nothing new yet.
3. **A evaluates.** Two purposes trigger on `Event.Damage.Received`: "cover from Instigator" (`Intent.Cover`, Reference = Instigator) and "return fire". Their conditions read A's Suppression and Morale attributes, and whether A is already AtLocation in cover. Cover wins.
4. **A selects.** `GE_UnderFire` applies (3 s, grants `State.Combat.UnderFire`, raises Suppression). A writes the intent entry `{Cover, player, Seeking}` and the behavior starts.
5. **A announces.** A's announce task sends `Event.Announce {Intent.Cover, Reference = the player's position as A believes it}`, and a cue plays "Taking fire, moving to cover!". B receives it with Instigator = A and Reference = that position.
6. **A moves.** A's EQS task sets Seeking; MoveTo sets Moving.
7. **B reacts to the announcement.** "Suppress the announced reference" (`Intent.Suppress`, Reference = the announced position) triggers on `Event.Announce`. It scores on B's own conditions: Morale, a line of sight to the reference, B not already engaged. B never read A's intent; A told it.
8. **E reacts to what it sees.** E sees A running, which raises `Event.Perception.Movement` with Instigator = A. E can read A's `State.Position.Moving` and current position, but not where A is going. "Push while they're moving" can score on that.

### What stays Purpose-only

- Scoring, selection, conditions and curves.
- History ages ("how long ago").
- Subjects, references and beliefs.
- Snapshots and the evaluation threads.

### Migration from the current tags

- **`Contextual.Combat.Engaged.1–5`:**
  - Replace with an Engaged attribute, `GE_Engaged` stacks and `State.Combat.Engaged`.
  - Retire the string parsing in `FContextualHistoryModifier_Engaged`.
  - Replace `UCondition_ContextualHistory_EngagedByCountForSubject` with an attribute-value condition.
- **`Contextual.Position.<Kind>.<Phase>`** becomes `Intent.<Kind>` plus `State.Position.<Phase>`. The NearInstigator tag-string bug and the AT_MoveTo finished-tag bug go away with the code they're in.
- **`Contextual.Position.Investigative.*`** becomes `Intent.Investigate` plus a Reference (the instigator, or the last known position). The search method stays on the EQS query asset.
- **The root-level `Moving` and `Seeking` tags** become `State.Position.*`.
- **EngagedCombatants, Morale and LevelOfAlarm** become attributes.

### Open questions (this flow)

- **Same intent, new reference** (cover from a second enemy): update the Reference in place, or restart the behavior?
- **Truth vs. belief snapshots:** at what point to pay for per-candidate subjects.
- **Event range and filter table:** a data table, or metadata on the tag itself?
- **Observable tags:** which State tags are perceivable at range, and which only up close.

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

### Delegated AI decision-making (goal)

**Goal:** in a large open world on a listen server, the first player in an area runs the decision-making for the AI in that area, so the host isn't paying for every AI's decisions.

**Constraint:** Unreal has no client authority over replicated actors. Every replicated actor has one authority, the server (the host on a listen server); a client can at most *own* an actor, which lets it send Server RPCs through it. GAS's `NetExecutionPolicy` only chooses where an ability runs within that model; for an AI with no owning player, "local" and "server" are both the host. So this is a custom layer above the engine's replication, whatever execution policy the abilities use.

**Design: a delegated brain.** The AI stays authoritative on the host, but its *decision-making* runs on an assigned client. That client runs the Purpose evaluation, EQS and perception for the AI near it and sends the chosen behavior and targets to the host through RPCs on an actor it owns (its PlayerController, or a per-area relay actor). The host executes the result: movement, pathing, abilities and replication.

- **What it saves:** the decision side, which is where the Purpose and EQS cost is.
- **What it doesn't save:**
  - The host still runs CharacterMovement, nav path following and animation for every AI.
  - The host still pays the replication cost for every AI to every client. On a listen server, the host's *upload bandwidth* is often the real limit, and delegating the brains doesn't reduce it, because all state still goes out through the host. Measure first (viability tests C and F in [Engine.md](Engine.md)): if bandwidth runs out before host CPU, delegation doesn't help and the effort belongs in Iris relevance and update rates.
- **Trust: accepted.** The assigned client chooses AI behavior, so a modified client could, for example, make the AI passive. On a listen server the host can already cheat freely, and no anti-cheat has solved cheating, so extending similar latitude to clients is accepted rather than defended against (see [Networking.md](Networking.md) → "Community moderation"). Server-side sanity checks on the decisions the host executes still apply.
- **Handoff:** when the assigned client dies, leaves or walks away, another client in the area or the host takes over. Needs hysteresis so assignment doesn't flip back and forth at area boundaries.
- **Latency:** decisions arrive one RPC round-trip late. Fine for AI decisions, but a constraint on anything reactive.
- **GAS:** GAS assumes AI abilities run on the authority. The client-side brain runs Purpose evaluation outside GAS and sends *decisions* (purpose/behavior identifier plus targets), not ability activations; the host then activates the ability as it does today. Keep Purpose abilities `ReplicateNo` / `ServerOnly` on the host; that doesn't block this design.

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
- Perception broadcast: dedicated announcement layer vs. a DataChunk update fed into existing scoring. Direction proposed: both. Announcements and unannounced perception are each occurrences, and scoring decides. See "Design: occurrence to reaction with GAS (proposed)".
- Baked-animation AI: at what distance to swap, and how to handle hit detection on distant AI.
- What exactly needs serializing from the Purpose system for a host migration snapshot.
