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

This section describes how an occurrence should travel from "something happened" to an AI reacting. GAS covers what it already does well: state, quantities, timed effects, gating, task resources, cooldowns and cosmetic replication. The Purpose system keeps what GAS has no answer to: scoring, "how long ago", relevance, and subjects. Proposed 2026-10-28 and revised the same day after review (revision 1), on 2026-10-29 (revision 2), on 2026-11-02 (revision 3) and on 2026-11-14 (revision 4); none of it is implemented yet. The sections below describe revision 4. Earlier revisions survive only in their worked examples.

**Revision 4 in short:**
- A manager subsystem owns the threads, the purpose library and registration.
- Occurrences are immutable and shared, built in one place, and point to the occurrence that caused them.
- There is no Purpose settings table. Data about a happening or an intent lives on its definition in the library, and signatures come from attributes on the equipment or actor that performed the action.
- Each occurrence definition has a weighted list of intent tags in place of the relevance matrix.
- Intent tags are structured by activity, and Action tags name the verb.
- Behavior changes only through new occurrences.

The worked example near the end is the litmus test for design changes. The baseline copy stays as first written. Each revision of the design adds its own copy of the example showing what changed.

### Principles

- **Behavior is defined by the conditions we can establish, and intents are the vocabulary for them.** An intent should be broad enough to be its own consideration (taking cover is a different decision from flanking) and narrow enough that designers aren't buried in variants. Conditions and references refine an intent; they don't multiply it. The code should make adding an intent cheap and adding a variant of one unnecessary.
- **Character comes from attributes, not archetype paths.** Every AI draws from the same purposes. Attributes such as Intelligence, Fear and Morale change which purposes win and how precisely conditions read the world. A careful veteran, a panicky recruit and a squad leader are the same purposes with different numbers (see "Character attributes").
- **Intent and reference are the primary pair; phase is secondary.** "Cover, from the Instigator" is the decision. Seeking, Moving and AtLocation only track progress on it.
- **Every action causes an occurrence, and occurrences never gate themselves.** Whether a candidate perceived something, cares about it, or can act on it is for conditions to answer. Anything that skips work before scoring must be unable to change the result (see "Evaluation").
- **No time constants in the flow.** Repeats are told apart by identity, not by a window: one occurrence per activation of the cause, and repeats of the same context merge while queued (see "Occurrence identity and repeats").
- **Others only know what they were present for.** History is shared, but an observer sees only the entries that happened while it perceived the subject (see "Relevance-gated history", a priority). Intent becomes known to others only by being communicated.
- **Each character records only itself.** Nobody writes another character's outcomes into their own history. To compare itself with someone, an AI reads that someone's history through relevance (see "One history per character").
- **Self-knowledge is read, never sent.** An AI's own intent and reference are read by its self-reaction conditions from its own snapshot. No occurrence carries them, and no condition can point at another character's current behavior (see "Self-reaction").
- **One definition per thing.** An occurrence's tag is the identity tag of what was performed (the task or the effect), not a second tag describing it. Phases are authored on the task that causes them, in the details panel, not built in code.
- **Data lives on the definition it describes (revision 4).** Everything about a happening (its relevance list, retention, outcome fields) lives on its definition in the purpose library, and everything about an intent on the intent's definition. No side table in settings duplicates or overrides them. Values that depend on the moment, such as how loud a shot was, come from attributes on whatever performed it, read when it happens.
- **Behavior changes only through occurrences (revision 4).** Nothing ticks "should I keep doing this", and no score decays on a timer. A running behavior is replaced only when a new occurrence produces a better purpose: an ad comes on, a bird lands nearby. Every change a player sees then has a cause they could have seen. Individuality comes from attributes and GEs: a guard who bores easily accumulates boredom on patrol and is pulled away by a smaller occurrence than a disciplined one, but it still takes an occurrence.
- **Each kind of fact has one home.** Tags say what kind, attributes say how much, history says when, and subjects say who and where. `Contextual.Combat.Engaged.1–5` broke this by storing a count as a category.
- **Use GAS where it already solves the problem; keep the Purpose system where GAS has no equivalent.** GAS has no utility scoring, no "how long ago", no relevance, and no subjects beyond source and target.

### Tag vocabulary

Top-level namespaces, split by what a tag identifies, how long it lasts, and who can read it. These replace `Contextual.*`.

There is no `Event.*` namespace. "Event tag" is the role a tag plays on an occurrence: it is the Action, Effect or Perception tag of whatever happened.

**Intent vs. Action (revision 4).** `Intent.*` names what a behavior seeks; it's what selection chooses. `Action.*`, `Effect.*`, `State.*` and `Perception.*` name what happened; they're what intents are scored against. An intent is never an action itself.

**How tags are structured (revision 4).** Each level answers one question, so a weight on any level reads unambiguously:
- **`Action.<Verb>[.<Detail>]`:** what the actor did, not what it did it with. `Action.Fire`, `Action.Throw`, `Action.Place`, `Action.Move`. A rifle and a pistol both perform `Action.Fire`. Their differences come from the equipment's attributes, carried by the occurrence (see "Equipment and latent actions").
- **`Intent.<Activity>.<Behavior>[.<Scope>[.<Subvariant>]]`:**
  - **Activity:** the broad kind of behavior. Starting set: `Investigate`, `Act`, and `React` (in this context a form of inaction: holding, freezing, staying down). More are added as behaviors show a better substructure. The set isn't fixed in code, but renames are costly once assets reference the tags; `GameplayTagRedirects` keeps renamed tags resolving.
  - **Behavior:** what the AI does, not why or what results. A result such as raised suspicion is a `State.*` tag the behavior applies, not part of the intent's name.
  - **Scope:** only where a behavior exists at more than one scope, as a variant. Scope isn't a separate axis, because a behavior that needs a squad can't exist for an individual: `Intent.Act.BattleDrill.Squad.BoundingOverwatch` is only ever offered to squad candidates.
  - At most four levels below `Intent`.
- **Activity comes first** because it's the axis occurrence definitions weight most often: gunfire weights `Intent.React` high and `Intent.Investigate` lower, whatever the scope.

| Namespace | Identifies | Lifetime | Lives on | Readable by others |
|---|---|---|---|---|
| `Action.*` | What a task physically does. It is the occurrence's tag when the task runs | An occurrence and a history entry | The task's identity tag (`contextualTagForTask`) | Yes, gated by relevance |
| `Effect.*` | A GameplayEffect landing on someone | An occurrence and a history entry. Duration effects may also grant State tags | The GE's asset tags | Yes, gated by relevance |
| `Perception.*` | The owner gaining or losing perception of someone | An occurrence and a history entry | Raised by the perception layer on the observer | No |
| `State.*` | True right now. Gaining or losing one raises a self occurrence | Removed when it stops being true | ASC owned tags: granted by a task's lifecycle entries, a duration GE, or `ActivationOwnedTags` | Only while perceiving the owner |
| `Intent.*` | What a behavior is for | While the behavior runs, then an intent history entry | The behavior ability's asset tags (formerly `AbilityTags`); intent history | No, unless communicated |
| `Cooldown.*` | Can't do X yet | The cooldown GE's duration | ASC owned tags | No |
| `Data.*` | Chunk keys | n/a | `DataTags.h` | n/a |

Examples:
- **Action:**
  - `Action.Move`, `Action.Face`, `Action.Fire`, `Action.Throw`, `Action.Place`. Revision 3 used `Action.Weapon.Fire`; earlier worked examples keep that name.
  - `Action.Query.<subject>`: asking the world, such as `Action.Query.Position` for the EQS task. Named for what the AI does, not the tool: a position query answered from the tactical grid instead of EQS keeps the same tag. A query is thought, not a visible act, so its signature has no reach and only its owner receives it, mainly for self-reactions.
  - `Action.Request.<nature>`: asking others, such as `Action.Request.Support`. It's communicated, so others can perceive it.
  - `Action.Communicate.Verbal.Loud` / `Normal` / `Quiet` / `Silent`, and `Action.Communicate.Gesture`. Each level has its own signature, so a loose squad shouting is heard from farther away than special forces speaking quietly (see "Squad coordination").
  - No `Action.Animate`: an animation is how an action looks, not a consideration.
- **Effect:** `Effect.Damage`, `Effect.Suppression`.
- **Perception:** `Perception.Gained`, `Perception.Lost`.
- **State:**
  - `State.Position.Seeking` / `Moving` / `AtLocation`. The only three phase tags.
  - `State.Combat.Engaged` / `UnderFire`.
  - `State.Suppressed`: granted by a GE when the Suppression attribute crosses its threshold (see "Thresholds as State tags").
  - `State.Downed`. Downed is a mechanic with its own rules, so it's a state. How hurt someone is is a number, read from the Health attribute by a condition.
- **Intent:** the behaviors so far are Cover, Flank, Overwatch, Advance, Fallback, Regroup, Investigate, Patrol, Ambush, Suppress, Inform and AwaitOrders. They still need placing under activities (`Intent.Investigate.*`, `Intent.Act.*`, `Intent.React.*`); until then the text below uses the short form `Intent.Cover`.

### Health and other observable numbers

Two paths, raised in review:
- **Visible wounds.** Perception reads what is presented. Decals don't break the textureless character material, since a decal brings its own material, which can itself be textureless. On moving, skinned characters, though, wounds drawn by the character material are the better fit (see Rendering.md → "Wounds and impact marks").
- **Attributes readable directly**, for ease and computation (Health, velocity).

**Recommendation:** for the AI, these cost the same. An AI doesn't need to look at a decal; it reads the Health attribute while it perceives the subject, which is what it would have seen.

**No per-attribute visibility (decided 2026-11-16).** Revision 3 marked each attribute observable or private. Revision 4 drops the mark and relies on the perception mechanic already in place:
- While B perceives A, B can read A's attributes.
- When B stops perceiving A, B keeps a snapshot of A's attributes from the end of that window, beside the last known position (see "Relevance-gated history"). Conditions read the snapshot until a new window opens.
- Trying to control which data is available beyond that adds more machinery than it's worth. Whether an AI should weigh an enemy's Fear or Intelligence is left to the conditions that are written.

Whether to draw wounds then becomes a fairness and art decision, not an AI one. If AI can read an enemy's health, players should be able to see it too, or the AI knows something players can't.

### Where each kind of data lives

| Kind | Example | Home | Why there |
|---|---|---|---|
| Current state | Moving, engaged, downed | `State.*` owned tags | Ability gating, `showdebug abilitysystem`, ASC replication |
| Quantities | Health, engaged count, morale, alarm, suppression, fear, intelligence | Attributes, changed by GEs | Stacking, aggregation, durations, change delegates |
| Timed state | Engaged for 5 s | A duration GE that grants a `State.*` tag and modifies an attribute | Expires on its own, with no time math in conditions |
| What happened, and what I was doing | Fired, was hit, started moving, intent Cover from the Instigator | One history per character (below) | GAS has no "how long ago" |
| Who I perceived, and when | Saw E from t1 to t2, last at X | Relevance records on the observer | Gates what history each observer can read |
| Places and references | Cover point, last known position | Subjects (below) | Conditions address a place the same way as an actor |
| Don't repeat | No re-flank for 10 s | A cooldown GE, plus the behavior's blocked tags | A pre-filter that needs no condition |
| Cosmetic reaction | Voice line, hand signal | GameplayCue | Replicates to clients for free |
| Data about a happening or an intent | Relevance list, retention, self weight, outcome fields | Its definition in the purpose library | One location; the library is published read-only to the workers |
| How it was performed | How loud the shot was, how visible | The performer's attributes, evaluated for the action's tag and copied into the occurrence when raised. Equipment changes them through its GEs | Equipping and unequipping is GE application and removal |
| The happening itself | Who, what, where, why (its cause) | The occurrence: immutable, shared, referenced by history | Built once, never copied |

Each attribute has one chunk: `FAttributeChunk`, keyed by a `Data.*` tag and bound to its `FGameplayAttribute` in `InitializeDataForOwner`. That's the tag pairing attributes don't have on their own. Attribute chunks are copied at snapshot like any other chunk. EngagedCombatants, Morale and LevelOfAlarm become attribute chunks; Fear and Intelligence are new ones.

AI decisions run on the host, so tags only the Purpose system reads don't need to replicate. State tags that clients need, for animation or UI, use the replicated loose-tag calls or come from GEs.

### One history per character

Action, Effect, Perception and Intent entries share a single history, ordered by time. There's no separate event history and intent history, and no third history pairing them: timestamps already order everything.

Timestamps alone stop being enough once behaviors run in layers (see "Behavior layers"). Two behaviors can be active at once, so the time window of an intent no longer tells you which actions belong to it. Each intent entry therefore gets a small id, and the action entries its behavior causes carry that id. Then a condition can ask "what has my current cover behavior done so far" directly.

An intent entry holds `{intent, reference, phase, time, outcome, id}`. Its phase and outcome are updated in place as the behavior progresses and ends.

**One entry per happening (decided 2026-11-08).** Every action, every hit and every perception change adds its own timestamped entry, whether or not it raised an occurrence. A full-auto burst that raises one occurrence still leaves one entry per hit. Start this way, watch how the data builds up (see "Monitoring history"), and then decide what to optimize and which data matters less than expected.

Each entry carries the context of what happened, not just its tag:
- **Context fields:**
  - **Tag:** the entry's own `Action.*`, `Effect.*` or `Perception.*` tag.
  - **Instigator:** the character id of whoever caused it. Effects received always record it, because the instigator is how the owner finds that character's history to read through relevance.
  - **Target:** the character id it happened to.
  - **Cause, for effects:** the instigating action's tag from the effect context, such as `Action.Fire` or `Action.Throw`. Gunfire and a grenade from the same enemy are then different entries and different occurrences.
  - **Reference, for actions:** what the action was relative to.
- **Entries with the same context fields form one context.** A context is what occurrences are identified by and merged on (see "Occurrence identity and repeats"); a different context always raises a new occurrence.
- **Notation used below:** `Effect.Damage ← 37 via Action.Fire` means damage instigated by character 37 through that action. The target is the entry's owner unless stated.
- **Entries reference the occurrence instead of copying it (revision 4).** The context fields, time and activation are already on the occurrence (see "Occurrences: one record, shared"), so an entry holds a shared reference to it plus what's the owner's own: behavior, reaction and outcome. Every character that recorded the same happening points to the same record.
- **Today** `FContextHistoryEntry` is `{contextualTag, timeOfContext}`, and `AddToValue` (`ContextualHistory.h`) refreshes the time of an existing tag instead of adding an entry, so hits from two shooters collapse into one.

| Field | Meaning |
|---|---|
| Context fields | Tag, instigator, target, cause, reference (above) |
| Time | When it happened |
| Activation | The cause's activation id (see "Occurrence identity and repeats") |
| Behavior | For actions: the id of the intent entry whose behavior performed it |
| Reaction | Set on the entry whose occurrence was evaluated: the id of the intent chosen in response, linking what happened to what was done about it |
| Outcome | What the owner observed of its own action. For fire, hit or miss, so the shooter always knows whether it's being effective. How a shot's result arrives depends on shot calculation (see "Open questions (revision 3)") |

**Everything else is derived by conditions** from the entries of one context:
- **Count, first time, latest time,** and how many fell in the last x seconds, which tells a 1 s burst 9 s ago from 10 s of steady fire.
- **Since the last reaction:** the entries newer than the latest one with a reaction. Merged occurrences and hits that never raised one are all counted.
- **Same activation:** compare against the activation of the latest entry in the context.
- **Hit ratio:** the outcomes of the owner's fire entries at a target.

Attributes still carry intensity: Fear and Suppression rise with each hit and decay, summed over every source. The entries carry who, what and when.

Example: P shoots A twice; A takes cover from P (intent #12), then fires at P six times and sees one hit land (intent #13). A's history holds eight entries:

| A's entries | Time | Outcome | Reaction |
|---|---|---|---|
| `Effect.Damage ← P via Action.Fire` | t0 | | #12 (cover from P) |
| `Effect.Damage ← P via Action.Fire` | t0 + 0.1 s | | |
| `Action.Fire`, at P, by #13, ×6 | t3 … t5 | 1 hit, 5 misses | #13 (suppress P) on the first |

**Comparing yourself with others uses relevance, not copies.** A's history never records P's numbers. If A perceives P, A reads P's history through the gated accessor (see "Relevance-gated history"):
- A's fire entries at P show 1 of 6 landed.
- P's fire entries at A, readable because A watched P fire, show 2 of 3 landed.
- A concludes it's losing the exchange, and that can drive a retreat with no special system.

If A didn't perceive P while P fired, A has only its own entries: hit twice by P.

**Entries stay small.** Outcome fields are listed on each occurrence definition, so only the outcomes someone uses are recorded. Entries are pruned by age and by a per-character cap, oldest first:
- **Age:** each entry's retention comes from the definition of what happened. Gunfire might be remembered for 60 s and a footstep for 5 s.
- **Cap:** a memory budget, not data about any happening, so it lives on the character's component and can differ by archetype.

#### Monitoring history

Per-happening entries are the simple starting point, not a settled format. Track these from the first playable fights, with stats or Insights trace scopes:
- **Growth:** entries added per character per second, at peak (full auto, grenades), and history length when pruning starts.
- **Snapshot cost:** bytes and time to copy a candidate's history into each occurrence's snapshot, and the shared copy of other subjects' histories.
- **Read cost:** time conditions spend scanning entries, per occurrence.
- **Use:** which fields and tags conditions actually read, so unused ones can be dropped.

**Optimizations to consider once there's data:**
- One record per context with a fixed-size time series (a ring of recent timestamps, or per-second counts) in place of per-happening entries.
- An index by context, so conditions don't scan the whole history.
- Shorter retention for high-rate tags such as fire.

### Relevance-gated history (priority)

**Rule:** observer B can read subject A's history only for the time windows in which B perceived A. If A leaves all of B's perception and later comes back, whatever A did in between is invisible to B. A's current State tags and observable attributes are readable only while B perceives A now.

- **Per direction.** A perceiving B and B perceiving A are separate records. Stealth depends on the asymmetry: A can watch B without B knowing.
- **Storage.** On the observer, keyed by subject: a list of windows `{start, end}`, plus what the observer saw at the end of the latest window: position, velocity, State tags, and a snapshot of the subject's attributes (revision 4). Pruned by age and count. Only pairs that ever perceived each other have one.
- **Occurrences.** Opening a window raises `Perception.Gained` for the observer; closing one raises `Perception.Lost`.
- **Last known position** is the position at the end of the latest window, exposed as a virtual subject (see "Subjects").
- **Intent stays private.** Even inside a window, B can't read A's intent entries. B can see A start moving, but not why or to where. A's intent reaches B only when A communicates it. B then records what it heard in its own history, so B never reads A's private entries.
- **Relevance records are private too.** "Have I been spotted?" is never answered by reading the enemy's record about me. Each AI estimates it from what it can perceive, more or less accurately depending on Intelligence (see "Character attributes").
- **Hearing and feeling.** Sight opens and closes windows. A sound or a hit is recorded as an entry in the listener's own history: what was heard and roughly where. A heard gunshot doesn't reveal the shooter's history.
- **Cause chains are gated the same way (revision 4).** Every occurrence points to the occurrence that caused it (see "Occurrences: one record, shared"). The chain is the objective truth, so an observer may follow a link only if that occurrence is in its own history (it perceived it) or was communicated to its group. Other links read as "caused by something I don't know". That's how F, who never knew A, can learn that A was shot first: someone who saw it has to tell F.
- **Cost.** A's history is copied into an occurrence's snapshot once and shared by every candidate. Each candidate brings its own windows. The filtering happens when a condition reads another subject's history, through a gated accessor on `FSubjectDataView` beside `Primary`/`Secondary`. If that accessor is the only way to read another subject's entries, conditions can't bypass the rule by accident, and shared snapshots stay shared.

### Subjects for places and references

This extends what `IDataMapInterface` was already for: the hand-placed POIs in the Warzone project were subjects that weren't characters. Any reference (a place, a last known position, a heard sound's location) becomes a DataMap that conditions read as a subject.

- **`ESubject::Reference`:** what the current intent is relative to. The behavior fills it when selected, with one of:
  - an actor (the Instigator),
  - a last known position from relevance,
  - a communicated reference.
- **Virtual subjects:** a place may have no UObject. The worker already reads `TMap<ESubject, FDataMap> subjectSnapshots`, so a virtual subject only needs a DataMap (Location, optionally ViewPoint and time). Only the game-thread `FSubjectMap` needs a way to carry one, as a DataMap beside the weak object pointers.
- **Group subjects (`ESubject::Squad`, `Platoon`, `Director`):** each level of the command hierarchy is a subject with its own data: leader, bounds, recent communications. Routing appends them to each candidate's context, so an AI never has to look up its own group (see "Command hierarchy and occurrence routing").

An intent is authored once and varied by its reference: cover from the Instigator, from its last known position, or from what an ally communicated.

### Behavior layers

**Problem:** when shot, an AI shouldn't have to choose between running to cover and warning its squad. Running uses the legs and facing; shouting uses the voice. Both should happen.

**Groundwork that exists:**
- Each task entry has a `requiredResource` (`FActionEntry`, `SequenceTaskInterface.h`). `UAI_SequentialActions` claims it on the task copy.
- Resource classes exist: Rotation, Logic, Pathing, EQS, Input, and animation FullBody / UpperBody / LowerBody / Head / Arms.
- `UGameplayTasksComponent` arbitrates tasks, but `UpdateTaskActivations` only blocks a task whose **required** resources overlap what's in use.

**Bug, blocks layers:** AI tasks only claim their resource and never require it, so they start even when another task holds it (`///Bug: AI Tasks Resources Behavior` in `AI_SequentialActions.cpp`). Each task must both require and claim. Two tasks in one action that share a resource would then block each other, so existing behavior assets need checking.

**Missing:** the Purpose side holds a single current behavior (`currentBehaviorForOwner`), and selection picks one purpose per occurrence.

**Proposal:**
- **A behavior's resources** are the union of its tasks' resources, computed once per purpose asset and cached.
- **The component holds one current behavior per set of resources it occupies**, each with its own score, instead of one behavior overall.
- **Selection takes as many purposes as fit:**
  1. Take the best-scoring purpose.
  2. Then take the next best whose resources don't overlap what's already taken.
  3. Repeat until resources run out or scores fall below a floor.

  Each purpose taken must beat every current behavior that holds a resource it needs, and ends only those.
- **Suggested layers:**
  - Locomotion: Pathing, Rotation, LowerBody.
  - Hands: Arms, UpperBody (the weapon).
  - Voice: a new voice resource.
  - Attention: Head (gaze).

  A hand signal uses the Hands layer, so it conflicts with firing, which is right.
- **Task priorities stay uniform.** The Purpose system is the arbiter, so task priority doesn't need to mirror scores. `UGameplayTasksComponent` pauses rather than ends lower-priority tasks, which isn't the behavior we want here.
- **The snapshot changes accordingly:** `FCandidateSnapshot::currentScore` and `currentBehaviorAbility` become per-layer lists.

### Communication

Communication is a behavior like any other, chosen by scoring, on the Voice layer or, for gestures, the Hands layer. It's chosen as a reaction to the AI's own decisions, so no other purpose has to include a communication task.

#### Self-reaction

Revision 2 raised a second, private occurrence when a behavior was selected. Revision 3 drops it: the AI already receives an occurrence for every action its behavior performs, so that is what it reacts to.

1. **The instigator is a candidate for its own actions.** When A's cover behavior starts moving, the `Action.Move` occurrence reaches A like anyone else. Today `PurposeEvaluationThread.cpp` skips the instigator as a candidate; that skip becomes the self weight.
2. **Only self-reaction purposes can score it.** Each purpose has a self weight on its own definition. It defaults to 0, unlike relevance-list weights, which default to 1.0; marking a purpose as a self-reaction sets it. Purposes at 0 are skipped by the same pruning as any other, so the skip can't change the result.
3. **Self-reaction conditions read the candidate's own current behavior.** Its intent and reference are in the Candidate snapshot already. These conditions have no subject setting, so they can't be pointed at the instigator or target. Nothing about the intent goes into the occurrence.
4. **Remove today's leaks.** These pass the current behavior to other AI:
   - `SnapshotSubject` appends the current behavior to the Instigator and EventTarget snapshots (`PurposeEvaluationThread.cpp`, `MakeOccurrence`).
   - `ActionPerformed` copies the behavior's subject map and context data into the occurrence (`PurposeAbilityComponent.cpp`).
5. **The self-reaction runs on a free layer.** It can't take a layer its behavior is using, or it would cancel the behavior that caused it. "Moving!" takes the Voice layer and leaves Locomotion to cover.
6. **Novelty stops repeats.** Every action in a sequence offers self-reactions again, so a condition on the AI's own history ("already said this about this reference") keeps it from repeating.
7. **Others hear only the communication.** The communicate task raises `Action.Communicate.Verbal.*`, which carries what A chose to say.

The same mechanism covers other "while I'm doing that" reactions, such as raising a weapon while moving to cover on the Hands layer. Single-action behaviors like "yell" are worth having for that reason: they cost almost nothing and fit many situations.

#### Thresholds as State tags

One occurrence per activation (see "Occurrence identity and repeats") means later hits in a burst update history without being evaluated. A reaction to the fifth hit pushing Suppression past the pinned level has to come from somewhere else:
- **A GE grants the State tag** when the attribute crosses its threshold, such as `State.Suppressed` or `State.Downed`.
- **Gaining or losing a State tag raises a self occurrence**, delivered only to the owner. The ASC's `RegisterGameplayTagEvent` reports the change. A State tag raises occurrences if it has a definition in the purpose library; there's no separate list. The manager binds tag events for those definitions when the library loads.
- **Others don't receive it.** They see the State tag only while they perceive the owner, through their own occurrences.

The freed-layer occurrence when a behavior ends (see "Finish") is the same kind of self occurrence.

#### The communicate task

- **One task, two channels.** A single `AT_Communicate` with a channel (Verbal or Gesture) and content: an intent and a reference, taken from the behavior's context. "Callout" and "Hand signal" are two purposes sharing that task with a different channel. Their conditions differ: a gesture wins when the AI believes it hasn't been spotted.
- **Verbal level.** The squad's grade sets the default level and the quietest one available (see "Squad coordination"). The situation picks within that, so special forces can still shout once compromised.
- **The occurrence comes from the task, not the cue.** The task's activation raises `Action.Communicate.Verbal.<level>` like any task, and the task also executes the GameplayCue for the voice line. The cue shouldn't be what raises the occurrence because cues are cosmetic and best-effort:
  - they're suppressed on dedicated servers,
  - they can be batched or culled,
  - they reach clients by unreliable multicast,
  - with delegated brains, the client running the brain isn't necessarily where the cue plays.

  Both tags are still authored in one place: the task holds its identity tag and its cue tag.
#### Squad tasks vs. self-reactions

Some reactions belong to the squad rather than to one member. **The test:** if two members doing it at once would be wrong, it's a squad task. If only the character itself can do it, it's a self-reaction. A self-reaction that would need a squad slot should be a squad task instead.

| Squad tasks | Self-reactions |
|---|---|
| Contact report | "Moving!" |
| Assigning targets | "Reloading!" |
| Bounding overwatch | "Frag out!" |
| Choosing who investigates | Looking at the threat or destination |
| One grenade at a time | Readying the weapon |
| "Man down" and assigning a medic | Muttering from Fear |
| Fall back or regroup | Quiet hand signal to a nearby ally |
| Radio support (a platoon task) | Acknowledging an order |

Two different limits, and each lives where it fits:

| Limit | Example | Mechanism |
|---|---|---|
| Capacity | One contact report at a time; one radio per platoon | Slots on the group's tasks component |
| Novelty, per reference | Don't repeat what the squad already heard about enemy A | A condition on the listener's history, keyed by the reference's id |

**The group owns its tasks.**
- Each group has a `UGameplayTasksComponent`, the base class of `UAbilitySystemComponent`, so groups need no ability system. It implements `IGameplayTaskOwnerInterface`, so it can own tasks directly, with no member ability in between. The question of whether a task routed through a member's ability ends with that ability no longer arises.
- `GetGameplayTaskAvatar(Task)` is virtual, so a subclass returns the member performing the task.
- **The squad holds the slot; a member performs the body action.** The squad's tasks can't see a member's Voice or Hands layer. So the squad decides and holds the slot, then has the chosen member perform the body action (speaking, signalling) as an ordinary ability. A member who is busy can be replaced by another.
- **The squad only knows what its members perceived.** It can act only on occurrences one of its members perceived, and the performer must be one of those members. Otherwise the squad is omniscient.
- **Losing the performer:** the task ends or switches to another member if the performer goes down.
- **Groups are candidates.** The squad evaluates its own purposes, such as a contact report or bounding overwatch. Candidates are typed to `UPurposeAbilityComponent` today, so this needs a small generalization.

**Slots are raw resource bits, not resource classes.**
- `FGameplayResourceSet` is a `uint16` (`GameplayTask.h`), so a component tracks 16 resource bits.
- Claims are per component, but the mapping from resource class to bit is process-wide. A global counter gives every non-abstract resource class one bit when its CDO is created (`GameplayTaskResource.cpp`). Body resources use that budget: the project has 10 (Rotation, Logic, Pathing, EQS, Input, five animation regions), plus AIModule's Movement and Logic.
- A group's component never shares its tasks with a character, so it can use bits directly through `AddRequiredResourceSet` / `AddClaimedResourceSet(FGameplayResourceSet)`, with no classes and no global IDs. Each group therefore has all 16 bits to itself. Debug output may show body resource names for them.
- **Several slots of one kind** allow several uses at once and cap how many run together: for example one lane for commands and two for reports.
- **There is no "any free slot" option.** A task pinned to slot 1 waits even when slot 2 is free. So pick a free slot from `GetCurrentlyUsedResources()` when the task starts. If none is free, keep the request on the group and retry from `OnClaimedResourcesChange`, rechecking novelty first. This covers five members reporting the same sighting on different threads at the same moment: the first takes a slot, and the rest find the news stale.

```cpp
FGameplayResourceSet slot;
slot.AddID(freeSlotIndex);	/// Picked from GetCurrentlyUsedResources()
task->AddRequiredResourceSet(slot);
task->AddClaimedResourceSet(slot);
```

- **Why not a resource per enemy:** resources answer "how many at once", not "about whom". Per-reference limits come from novelty. B calling out enemy A doesn't block C calling out enemy D.
- **Not every action, not on a timer.** A cooldown GE on the comms layer, plus the novelty condition.

#### Squad coordination

**Dynamic, with no standing roles.** Coordination comes from occurrences and scoring, not from assignments made before contact.
- **Every member receives every occurrence,** in every grade of squad. A member who isn't given the occurrence can't react to being shot, gating breaks "occurrences never gate themselves", and waiting on a manager delays every reaction. So no manager decides who is queued, and nothing overrides members with a higher priority.
- **Orders are heard occurrences.** A squad task speaks the order, and each member scores that `Action.Communicate.*` occurrence like any other. It replaces what a member is doing only if it wins, weighed by Discipline against Fear.
- **Reflex first, then the order.** Each member reacts to the gunshot on its own, and the order lands as a later occurrence. The lag is the time the order takes to deliver, and it gives the natural sequence.
- **Members need nothing extra to follow a squad behavior.** They received the same occurrence the squad reacted to, and they react to the order when it arrives.

**Squad grade is data, never separate code paths:**

| Grade | Default level | Squad purposes | Discipline |
|---|---|---|---|
| Loose squad | `Verbal.Loud` | Few or none, so everyone acts alone | Low |
| Experienced squad | `Verbal.Normal` | Common ones | Medium |
| Special forces | `Verbal.Quiet` | Most | High |
| Tier one | `Verbal.Silent` | All | Very high |

Grade is also a speed setting: quiet signals and short commands land sooner than a loose squad shouting.

**Orders and recipients.** An order can name a recipient. "Await orders" is an intent (`Intent.AwaitOrders`) whose conditions read the squad leader's presence and recent communications. The leader and Intelligence weight who tends to perform squad tasks.

**Platoons and the director** are candidates the same way, for purposes such as a platoon maneuver. That's the disabled Director work.

### Command hierarchy and occurrence routing

**One recursive class.** `UPurposeGroup` (a UObject implementing `IDataMapInterface`) holds:
- its members (weak pointers to AI components),
- its child groups,
- its DataMap: level (`Group.Squad`, `Group.Platoon`...), leader, bounds,
- its tasks component.

The root belongs to the manager (see "The manager"). The logic is written once, and each level is its own object.

This beats one manager holding tree data. Every node has to be addressable as a subject (a weak UObject in `FSubjectMap`) and own a tasks component, so the nodes would end up as objects anyway. Tasks components are actor components, so they all live on the director actor, one per group, created as groups form.

**Top-down routing:**
1. Every occurrence goes to the root. An AI raising one talks to the director subsystem and needs no reference to its own group.
2. Each level culls its children against the occurrence's reach, the maximum radius in its signature:
   - the director checks platoon bounds,
   - each platoon checks squad bounds,
   - each squad checks its members.
3. Each level appends itself as a subject for the candidates beneath it.
4. The root holds every faction, so the descent crosses hierarchies. An enemy's platoon is checked the same way as an ally's.

Bottom-up looks self-isolating, but anything crossing factions has to climb to the top anyway. It would also switch to top-down as soon as it reached a level with several platoons, because their squads need bounds checks.

**No back-references.** AI never store "my squad". A member list in the group and a group pointer in every AI would be two records of the same fact to keep in sync through reassignment, promotion and death. Instead:
- The occurrence brings the chain as subjects.
- Selection-time validation uses the group subject already in the selected context.
- Groups subscribe to a member's death or end-play when they register it.

**Other details:**
- **Bounds:** recomputed from members at a low rate, not every frame. The cull runs on the game thread when the occurrence is raised, and snapshots are built only for the candidates that survive it.
- **Players** belong to groups too, so their occurrences route the same way and they can be subjects. They're never candidates.
- **If squads scatter enough to cull poorly,** add the area of operation as a level between the director and the platoons.

This replaces Performance #3's spatial-filtering fix.

### Reference identity

**References need an id that outlives a pointer.** Novelty ("has my squad heard about enemy A"), communicated content and history keys all name who they're about.
- `TObjectKey` is stable only within one process. It means nothing to a delegated brain on a client or after host migration.
- **Recommendation:** each character gets a `uint32` id from the director when it registers. It's stored in the component's data and serialized in host-migration snapshots.
- Non-character references (a POI, a heard sound's location) use their provider's id, or the tactical grid cell they fall in.

### Occurrence identity and repeats

Revision 2 coalesced repeats inside a time window per tag. Revision 3 has no windows: repeats are recognized by what caused them.

**One occurrence per activation.** In revision 4 every happening builds an occurrence record (see "Occurrences: one record, shared"); these rules decide which records are queued for evaluation. The rest are only recorded in history.
- **The activation counter.** `UPurposeAbilityComponent` keeps a count that it increments whenever one of its abilities activates. Together with the character id, that count identifies one activation. It lives on the component because it has to work under every instancing policy:

  | Policy | What exists | So a counter on the ability… |
  |---|---|---|
  | NonInstanced (deprecated in 5.5) | The class default object, shared by every actor | Can't hold state at all |
  | InstancedPerActor | One object per actor, created when granted and reused | Works, but must be reset by hand |
  | InstancedPerExecution | A new object per activation | Always reads 1 |

- **Actions.** The action occurrence carries the activation id.
- **Continuous actions re-raise while active (decided 2026-11-16).** Full-auto fire, footsteps and other actions that last a while raise repeat occurrences for as long as they're active, all under one activation id.
  - **The interval lives on the action's definition, as a time (decided 2026-11-17).** `Action.Fire` has one re-raise interval for every weapon, so a high-rate SMG raises no more occurrences than a rifle.
  - **It's checked on physical events, not by a timer.** On each round fired, or each footstep notify, the action raises a repeat if the interval has passed since its last one. When the action stops, nothing is left running.
  - **The record carries the count.** Each repeat records how many rounds (or steps) happened since the previous one, so rounds are still counted for hit ratios without raising more occurrences. This answers late listeners: someone who comes into range mid-burst gets the next one.
  - **Every repeat reaches every candidate.** Routing doesn't skip candidates that already saw the activation, because that would be an occurrence gating itself.
  - **Merging absorbs backlog.** Repeats share a context key, so a repeat still queued when the next arrives is superseded (see "Repeats merge while queued").
  - **Novelty is a condition.** A heavily weighted condition scores a repeat low when the candidate's last entry in this context is only x seconds old. It can also read whether the candidate already reacted to this activation (the Reaction field). A late listener has no entry, so it scores normally.
  - **Cost:** each repeat is evaluated by every candidate in reach. The re-scored baseline (see "Evaluation") prunes most purposes for candidates already reacting. Measure under sustained fire.
- **Hits.** The shooter's activation id travels in the effect context (below). Every hit adds an entry. Then:
  - **Same activation as the latest entry** in that context: no occurrence.
  - **New activation** (a new burst, a semi-auto shot): raise an occurrence.
  - **New context** (another instigator, target or cause): raise an occurrence. A second shooter always gets through.
- **Mid-burst thresholds** come through State tags (see "Thresholds as State tags").
- **This replaces the 0.25 s timer** (`activatedAbilityTimeCache` with `occurenceTimerMin`, per character and ability class).

**The custom effect context.**
- `UGameplayAbility::MakeEffectContext()` records the ability in the hit's effect context, readable through `GetAbility()` / `GetAbilityInstance_NotReplicated()`. That instance pointer exists only on the host and depends on the instancing policy, so it isn't enough.
- **Add a context subclass** that carries the ids: the instigator's character id, its activation id, and the action tag that caused the hit. Allocate it by overriding `UAbilitySystemGlobals::AllocGameplayEffectContext` in a project subclass, and serialize the fields in its `NetSerialize`.
- The ids survive projectile travel time and reach a delegated brain on a client.
- **The fire ability must build the context**, or a projectile must carry the one the ability built. A projectile calling the component's own `MakeEffectContext` loses the ability fields.
- **Only ids go in it, not our context data.** Effect contexts are copied into every spec and replicated with effects and cues, so they should stay small. A DataMap holds UObject chunks, which don't serialize cheaply. The shooter's current behavior is private, which is the leak "Self-reaction" removes. The receiving side builds its occurrence from these ids and its own snapshots.

**Repeats merge while queued.** Distinct activations can still queue up close together, such as fast semi-auto taps. The newest occurrence of a context supersedes older ones still in the queue, and there is no follow-up check after evaluation.
- `TQueue` can't be searched or edited, and snapshots must be taken on the game thread. So a repeat raises a fresh occurrence with fresh snapshots, and the older one is skipped.
- **The evaluation thread decides, by timestamp.** Each occurrence carries the time it was raised and its context key (the history entry's context fields). On each wake the thread drains its whole queue, the same drain as Performance #2. It keeps only the newest occurrence per context key and evaluates those. The map is local to that drain, so there are no counters to reset and nothing shared with the game thread.
  1. **Drain:** dequeue until the queue is empty. Dequeuing removes items from the real queue, so nothing popped is evaluated twice.
  2. **Merge:** each popped occurrence goes into a local map keyed by context key. A newer one replaces the older one, which is released; it's freed if nothing else holds it.
  3. **Evaluate** each occurrence left in the map, oldest raise time first. The newest repeat is evaluated in place of the older ones. No count of merged repeats is needed, because the history entries recorded at raise time already hold it.
  4. **Arrivals during the drain or evaluation:** the queue is a lock-free single-consumer `TQueue`, so the game thread can keep enqueuing. Whatever arrives after the queue was found empty waits for the next drain, and enqueuing triggers the wake event. If that repeat is redundant with what was just evaluated, the novelty condition scores it low.
  5. **Batch size:** if a long drain delays the last items too much, cap it at N items. Merging still works within each batch.
- **Repeats of a context must reach the same thread.** The load balancer picks a thread by queue depth. It would also need to route by a hash of the context key, at some cost to balance (a noted concern; see "Open questions (revision 3)").
- The newest occurrence already has the latest history, because the game thread added the entry before raising it.
- **How many happened since the last reaction** comes from the entries: those in the context newer than the latest one with a reaction. It doesn't come from counting merged occurrences, because that would miss repeats that never raised one, such as the later hits of a burst. Shots fired at the owner that missed aren't in the owner's entries at all. Those are in the shooter's fire entries, readable only if the owner perceived the shooter.

**A yes for what's already running isn't a new behavior.** Evaluation only answers yes or no to a new behavior; a no keeps the current one running until some later occurrence produces a yes. There's no separate "continue" decision (revision 4). When the yes has the same intent and the same reference as a current behavior, the reaction is recorded and the behavior keeps running instead of restarting. A new reference starts a new behavior.
- Today's guard (`PurposeEvaluationThread.cpp`) skips a winner whose event target and behavior ability class match the current behavior. It ignores the instigator on purpose, because conditions couldn't yet tell "cover from P" from "cover from Q". With cover conditions they can, so the guard is replaced by the intent-and-reference comparison. A `/// Refactor: Identical Behaviors Time-Gated Occurrences` note in the code marks it.

### Where purposes live

`FPurpose` is already a struct, so it can live in more than one vessel:
- **Library:** `UEventAsset` (a DataAsset) for purposes every AI may consider. In revision 4 one asset class holds both kinds of definition: intents (conditions, behavior ability, scoring, self weight) and occurrences (relevance list, retention, outcome fields). One class rather than two keeps every input to selection in one system.
- **World providers:** a `UPurposeProviderComponent` on placed actors, for example a radio with purposes describing how to use it.
  - It holds `FPurpose` entries (instanced conditions per placed actor) or asset references plus overrides.
  - It implements `IDataMapInterface`, so the radio is the subject those purposes read.
  - It registers with the director when it streams in and unregisters when it streams out.
  - Providers in an occurrence's reach append their purposes for that occurrence's candidates, culled by the same routing. A "seek activity" occurrence is then just an occurrence.
- **Lifetime:** purposes passed with an occurrence must stay alive until in-flight evaluations finish, so unregistering waits for them. Evaluation threads currently cache purposes; dynamic ones travel with the occurrence instead.
- **Smart Objects** (`Engine/Plugins/Runtime/SmartObjects`, not enabled in AO) already provide placed definitions, per-slot claims and spatial queries with tag filters. A custom behavior definition could carry an `FPurpose`. Start with the provider component, which is small and in-house, and revisit Smart Objects if activities multiply. Its slot claims answer "how many can use the radio".

### Character attributes

Attributes give AI their own character without hardcoded archetype paths.

- **Fear** shifts selection. It raises the scores of retreat, panic and wild fire, and lowers holding a position. Conditions read it through curves.
- **Intelligence** works three ways:
  - **Breadth.** Intricate purposes (coordinated flanks, holding concealment, leading) carry an Intelligence condition, so they only win for capable AI.
  - **Precision.** The same question is answered better. Take "have I been spotted":
    - A low-Intelligence AI uses "the enemy is facing roughly my way", a wide cone. It panics and blows its cover.
    - A high-Intelligence AI uses the enemy's actual view direction plus its own concealment from the spatial field. It holds, or signals instead of shouting.

    Prefer one condition whose tolerance comes from an Intelligence curve over two purposes, so precision doesn't multiply purposes.
  - **Coordination.** Leading, awaiting orders and relaying information are purposes gated by Intelligence.
- **Discipline** weighs a heard order against Fear. A disciplined AI follows the order even under fire; an undisciplined one keeps reacting on its own. It's part of what sets a squad's grade (see "Squad coordination").
- **Focus, boredom and distraction (revision 4)** are attributes too, changed by GEs, never by a decaying score. A guard who bores easily gains boredom faster on patrol. When the next occurrence re-scores the patrol (see "Evaluation"), its conditions read Boredom and score lower, and distraction purposes read it too. So a smaller occurrence pulls that guard away than a disciplined one. The change still waits for an occurrence (see "Principles").
- **Equipment carries GEs, not attributes (decided 2026-11-16).** Each item can carry different GEs or magnitudes, so no two weapons need be the same. There's no attribute set and no ASC on equipment.
  - **Only what's in hand is equipped.** Holstered or slung items are world items that happen to be socketed to a character. They're treated no differently from an item on the ground and can be dropped at any time. Their GEs don't apply.
  - **The character's own loudness** (footsteps, voice) is preset in character data. It isn't the attribute's base value, because a base value can't be excluded by tag and would add to every action, gunfire included. Instead it's an innate infinite GE whose magnitudes come from character data, with each modifier requiring its action (`Action.Move`, `Action.Communicate.*`). The attribute's base stays 0.
  - **Action GEs are applied when the action happens.** Firing applies the weapon's fire GE, whose modifier requires `Action.Fire`, with a short duration so it falls away on its own.
    - **No stacking duration:** `StackingType` aggregated by source, `StackLimitCount` 1, and `StackDurationRefreshPolicy` refreshing on application. Sustained fire keeps one instance and refreshes it, instead of extending it.
    - A suppressor is an infinite GE while attached, with a multiply modifier that also requires `Action.Fire`.
    - So a shot's loudness is the weapon's GE times the suppressor, and the character's innate loudness doesn't apply.
  - **Throwables don't apply their GE to the owner.** A grenade's loudness GE stays on the grenade. When its detonation is raised, the magnitude is read from the GE itself: `FGameplayEffectModifierMagnitude::GetStaticMagnitudeIfPossible` returns a scalable-float magnitude with no ASC. Attribute-based and custom-calculation magnitudes would need a spec with captures, so throwables should use scalable floats or SetByCaller. The value is captured into the record on the game thread, and conditions read it from there, never from the GE object.
  - **The action tells a gunshot from a footstep.** A single Loudness attribute on the character serves every action. Each modifier carries tag requirements, which `FGameplayModifierInfo` already has (`SourceTags` / `TargetTags`). A rifle's or suppressor's modifier requires `Action.Fire`, and boots require `Action.Move`.
  - **Capturing it:** when an occurrence is raised, `RaiseOccurrence` reads the attribute with the action's tag through `UAbilitySystemComponent::GetFilteredAttributeValue`. That aggregates only the modifiers whose requirements the tag meets, and the result is copied into the record. It uses static tag containers internally, so it's game-thread only; that's where occurrences are raised anyway.
- **Archetypes are attribute profiles.** The character defaults DataAsset (TODO in `PurposeAbilityComponent.h`) carries attribute baselines, and optionally the set of purposes granted. An archetype is never a code path.

### The manager (revision 4)

Today nothing creates the evaluation threads, fills their caches or raises occurrences. The Lyra `ADirector_Level` that did wasn't ported, and both `PurposeSystem::Occurrence` calls in `PurposeAbilityComponent.cpp` are commented out. The manager replaces the director.

- **A world subsystem**, with a director actor it spawns to host the groups' `UGameplayTasksComponent`s, since those are actor components (see "Command hierarchy and occurrence routing").
- **Thread pool:** N identical evaluation threads, created on world begin play and stopped and joined on deinitialize. The empty `FEventThread` / `FActorThread` / `FCompanionThread` subclasses go. It includes Performance #1 (the load-balancer fix) and #2 (an `FEvent` wake that drains the whole queue).
- **Purpose library:** loads the `UEventAsset`s and publishes them as an immutable shared version. Workers hold the version they started with, so nothing they read changes under them. Threads no longer hold references to manager-owned arrays.
- **Registration:** `UPurposeAbilityComponent`s register on `InitAbilityActorInfo` and unregister on `EndPlay`, instead of being found by a scan. Registering assigns the character id (see "Reference identity"). Purpose providers register the same way.
- **One entry point, `RaiseOccurrence`.** It's the only place an occurrence is built (see "Occurrences: one record, shared"). It routes the occurrence through the hierarchy and queues it. AI never hold a reference to the manager's internals or to their own group.
- **State tag bindings:** when the library loads, the manager binds tag events for every `State.*` tag that has a definition (see "Thresholds as State tags").

### Occurrences: one record, shared (revision 4)

Today `FContextData` is the queued occurrence, the worker's result and the running behavior all at once, and it's copied whole at each step. Because a behavior and an occurrence are the same type, `ActionPerformed` builds the occurrence by copying the behavior, which is the intent leak.

**One context, and thin holders that point to it:**
- **`FOccurrenceEvent`:** the record of one happening. `RaiseOccurrence` builds it and nothing changes it afterwards. Held through `TSharedRef<const FOccurrenceEvent>`, so every worker and candidate reads the same one without locks.
- **The running behavior:** a reference to the occurrence that started it, a pointer to its purpose, and its score. The reference is also what lets the behavior be re-scored against its own subjects at each new occurrence. It copies none of the occurrence's fields, so there's nothing to keep in sync.
- **No selection struct.** The worker's result is a local that becomes the behavior on a yes.

**What the record holds:**
- The event tag, context key, time raised, and activation id.
- Subjects, as character ids plus weak object pointers.
- Values captured when raised: location and direction, the performing equipment's signature attributes (see "Equipment and latent actions"), and the effect magnitude.
- `Cause`: a weak pointer to the occurrence that started the behavior which raised this one.

**Cause chains.** B fires at A. C's fire points to B's, D taking cover from C points to C's, and B flanking D points to D's. Each link is an existing record, so nothing is copied, and no one can rewrite a link someone else is holding. Others see links only through relevance (see "Relevance-gated history").

**Links are readable for a set time (decided 2026-11-16).** A record can be read as a cause for a set time after it was raised. The link is pruned a margin later. So no condition depends on whether some unrelated AI still remembers the cause, and the time also bounds how long a chain can get. The weak pointer stays as the safety net: a worker pins the link before reading, so a record can't be destroyed mid-read even if a backlogged queue outlasts the margin.

**Lifetime: freed when nothing needs it.**
- A record isn't a UObject. It's reference-counted and freed the moment its last holder lets go, with no GC involved.
- **Holders:** workers while evaluating it, the behaviors it started, and the history entries that remember it. If an occurrence produced no yes, it's freed as soon as the last worker finishes. If hundreds of AI reacted, there's still one record and hundreds of pointers. Copying would cost one full copy per reaction.
- **`Cause` is weak**, so a chain doesn't keep its past alive. A link lives while someone's history or running behavior still refers to it and disappears when everyone has forgotten it.

**When a subject is garbage collected.** The record never depends on a live object:
- It holds the character ids and the values captured when it was raised, so a destroyed actor doesn't change who it was about or what happened. Only the weak pointer goes null.
- Who a behavior belongs to isn't stored in the occurrence: the component that holds the behavior is its owner.
- Copying wouldn't protect against this. A copy holds the same weak pointers, and strong ones would keep dead actors alive.
- **Rule:** anything a later reader needs is captured by value when the occurrence is raised. Object pointers are only for reaching live state on the game thread, and must be null-checked.
- A subject dying mid-behavior is itself an occurrence (death, or `State.Downed`), so a behavior aimed at it is replaced through the normal path.

**Every happening builds a record; only some are queued.** A hit from the same activation as the last one in its context builds a record and adds a history entry pointing to it, but isn't queued for evaluation (see "Occurrence identity and repeats").

### Occurrence definitions and relevance lists (revision 4)

**Gameplay tags carry no data.** A tag has a name, a developer comment and the source it was declared in. `meta=(Categories="...")` on a UPROPERTY only filters the tag picker, and the tag manager ignores extra fields on `FGameplayTagTableRow` subclasses. So data about a tag lives on that tag's definition in the purpose library.

An occurrence definition (`Action.Fire`, `Effect.Damage`, `State.Suppressed`…) holds:
- **A relevance list:** intent tags at any level, each with a weight. An intent that isn't covered defaults to 1.0. It replaces revision 3's single relevance matrix, which would have been cumbersome to author, while splitting it into several matrices would lose perspective. Example for `Action.Fire`:

  | Entry | Weight | Covers |
  |---|---|---|
  | `Intent.Act.Patrol` | 0.1 | Every patrol variant |
  | `Intent.Act.Contact` | 2.0 | Every contact behavior, at every scope |
  | `Intent.Investigate.Watch` | 1.5 | One behavior, overriding the activity's default |

  - **Most specific entry wins.** An intent walks up its own tag until it finds an entry. Lists are flattened into a lookup when the library loads, so evaluation does one map lookup per parent level.
  - **The list lives on the definition of the happening, never on the performer's intent.** Two identical shots must weigh the same to observers whatever the shooter's motive.
  - **The observer's role isn't in the list.** Being the target, hearing it from 60 m away, or being the target's squadmate are told apart by the purposes' own Subject conditions.
- **Retention:** how long history entries of this kind are kept.
- **Outcome fields** recorded for this kind of happening, such as hits landed for fire.

An intent definition holds its conditions, behavior ability, scoring and self weight (see "Self-reaction").

**Where an occurrence reaches:** loudness and visibility come from the performer's attributes evaluated for the action's tag, with equipment GEs included, captured in the record. The routing cull's maximum radius is derived from them. Whether a candidate heard it is a condition: loudness against distance and the candidate's hearing. How the `Action.Communicate.Verbal.*` levels get their loudness, with no equipment performing them, is open (see "Open questions (revision 4)").

**Settings.** What remains is system configuration with no owning definition: thread count and budgets. If nothing else turns up, there's no `UPurposeSettings` at all.

### Equipment and latent actions (revision 4)

**The action is the verb; the variance comes from what performed it.** A rifle shot and a pistol shot are both `Action.Fire`. Their loudness, visibility and anything else observers react to come from the GEs the weapon applies to its wielder, evaluated for `Action.Fire` and captured in the occurrence. A suppressor is another GE for as long as it's attached.

**Equipment as a subject (role name tentative: Equipment).** The object an action was performed with is a subject of its own, separate from the instigator. It isn't always a hazard: a ladder or a laser alarm is equipment too. Holstered and slung items are world items socketed to a character, the same as items on the ground, so they can be subjects and providers in their own right.

**Latent actions, such as a grenade:**
1. **`Action.Throw`:** the instigator is the thrower, the Equipment subject is the grenade, and the target is where it lands. Observers can react to the grenade as a position to get away from, while the thrower stays the one responsible.
2. **The detonation** is its own occurrence, raised later by the grenade. It carries the thrower's character id, the same way the custom effect context does, and its `Cause` points to the throw. Its loudness is read from the grenade's own GE, which never applies to the thrower (see "Character attributes"). The context key includes the tag, so it raises an occurrence despite sharing the throw's activation id.
3. **Lingering hazards** (a cooked grenade on the ground, fire, gas) can be purpose providers while they exist, so their purposes reach anyone in range without re-raising an occurrence.

**What kind of thing it is** (explosive, obscurant, how dangerous at this range) should come from the equipment's attributes rather than a fixed tag, since effectiveness depends on proximity and the observer's health. To be designed later.

### The flow

#### 1. Something happens: an occurrence

Every source calls the manager's `RaiseOccurrence`, which builds the immutable record (see "Occurrences: one record, shared"). It carries the identity tag of what happened, its context key, the time it was raised, the subjects' character ids, the values captured from the performing equipment and, for actions and hits, the cause's activation id. For an action performed by a behavior, `Cause` is the occurrence that started that behavior. Sources:
- **Tasks.** Each action a behavior starts already raises an occurrence (`ActionPerformed`, end of `AI_SequentialActions.cpp`'s action setup). The tag is the task's own `Action.*` tag. Communication is a task like any other. The occurrence no longer copies the behavior's subject map or context data.
- **Equipment acting later.** A thrown grenade raises its detonation itself, carrying the thrower's id, with `Cause` pointing to the throw (see "Equipment and latent actions").
- **Gameplay effects.** Damage arrives as a GE. Its `FGameplayEffectContext` already holds:
  - Instigator → `ESubject::Instigator`.
  - EffectCauser and SourceObject → the weapon.
  - HitResult → Context (hit location and direction).
  - The custom context subclass adds the instigator's character id, activation id and action tag (see "Occurrence identity and repeats").

  Handle it from `OnGameplayEffectAppliedDelegateToSelf`, tagged with the effect's `Effect.*` tag. Every hit updates the target's history entry and adds a landed hit to the outcomes of the shooter's fire entry; only a new activation or a new context raises an occurrence. A suppressed shot that lands is felt even when it isn't heard. The AI can then communicate it, and others react to that.
- **Perception changes.** `Perception.Gained` and `Perception.Lost`, raised by the perception layer when a relevance window opens or closes.
- **World events:** destruction, keyed off the server-side event (see "World events as occurrences").
- **State tag changes.** A self occurrence, delivered only to the owner (see "Thresholds as State tags").

There is no occurrence for selecting a behavior. The behavior's first action is what its owner's self-reactions answer.

#### 2. Candidates

- **Top-down routing.** The occurrence descends the command hierarchy, culled by bounds against its reach. Each level appends itself as a subject (see "Command hierarchy and occurrence routing"). Purpose providers within reach add their purposes. Nobody is excluded for not perceiving it.
- **The instigator is a candidate,** scored with the self weight (see "Self-reaction").
- **Groups are candidates** for squad purposes, if one of their members perceived the occurrence (see "Squad tasks vs. self-reactions").
- **Perception is data.** Per candidate, the snapshot records how it perceived the occurrence: seen, heard, felt or not at all, with a quality. Conditions decide what that means.

#### 3. Snapshot (game thread)

- **Candidate:**
  - its DataMap (attributes resolve through `FAttributeChunk`),
  - its owned tags (`GetOwnedGameplayTags()`) copied into a tags chunk,
  - its history and relevance records,
  - its current behaviors, one per layer, with intent and reference. Only self-reaction conditions read these,
  - how it perceived this occurrence.
- **Other subjects:** copied once and shared by every candidate. Conditions read them through the gated accessors, so each candidate sees only what its relevance allows. Their current behaviors are never included.
- **Context:** the occurrence record itself, shared rather than copied: its tag, location and direction, captured equipment values, GE magnitude, and its cause chain (read through relevance).
- **History** in the snapshot holds references to occurrence records, so copying a history copies pointers, not context fields.
- **Virtual subjects:** Reference, last known positions.
- **Group subjects:** each candidate's own chain, appended during routing.
- **The tactical grid:** a pointer to the current published version, so every condition in the occurrence reads the same one (see "Spatial perception").

#### 4. Evaluation (worker)

0. **Drain and merge.** The thread drains its queue and keeps only the newest occurrence per context key (see "Occurrence identity and repeats").
0b. **Re-score the current behaviors (decided 2026-11-16).** Before any new purpose is considered, each of the candidate's current behaviors is scored again with the fresh snapshot. That score, not the one it was selected with, is the baseline a new purpose has to beat on that layer.
   - **Attributes act here.** A guard's rising Boredom lowers the re-scored patrol through its conditions, so a smaller occurrence can win than it could an hour earlier. A disciplined guard's patrol re-scores high. Nothing decays on a timer, and the re-check runs only because an occurrence arrived.
   - **Pruning gets a real floor.** The baseline is usually well above 0, so fewer purposes survive the upper-bound check below, and the winner must beat what the AI is doing rather than just exceed 0.
   - **It's scored against its own occurrence.** The behavior holds the record that started it, so its conditions read their original subjects and reference, with fresh snapshots of those subjects. The relevance weight is the one from its original occurrence.
1. **Relevance list and upper-bound pruning.** This keeps the spirit of utility while still cutting cost.
   - The occurrence's definition gives a weight for each intent through its relevance list: the most specific entry above the intent's tag, or 1.0 (see "Occurrence definitions and relevance lists"). For example, `Action.Fire` weights `Intent.Act.Patrol` at 0.1.
   - The weight is a real scoring factor, not a gate.
   - Each purpose's highest possible score is its `FPurpose::Potential` times that weight. If even that can't beat the re-scored baseline on the layers it needs, the purpose is skipped without scoring its conditions. Skipping changes nothing, because scoring it couldn't have won.
   - A weight of 0 is an explicit authoring statement: this intent never reacts to this kind of occurrence.
   - A separate self weight, on the intent's own definition, applies when the candidate is the instigator. It's 0 except for purposes marked as self-reactions.
2. **Tag gate.** Check the behavior ability's activation required, blocked and cooldown tags against the snapshot's copied tag container.
   - They're protected on `UGameplayAbility`, so `UBehavior_AI` exposes them.
   - Don't call `CanActivateAbility` here: it isn't thread-safe, but comparing tags against a copied container is.
3. **Score.** Conditions read tags, attributes, history ages, relevance-gated history of others, references and the spatial field.
4. **Layered selection** (see "Behavior layers").

#### 5. Selection (game thread, `ProvidePurposeToOwner`)

0. **A no changes nothing.** If no purpose beat the current behaviors' re-scored baselines, they keep running. Nothing re-checks them until another occurrence arrives.
1. **Same behavior check.** If the winner has the same intent and reference as a current behavior, that behavior keeps running and only the reaction is recorded on the occurrence's history entry. Stop here.
2. **End the conflicting current behaviors.** Step 7 runs for each, with the outcome Interrupted. A new reference always starts a new behavior, even with the same intent.
3. **Set the new behavior:** a reference to the occurrence that started it, its purpose and its score. Write its Reference subject, and record its intent id as the reaction on the occurrence's history entry.
4. **Apply OnSelected before performing the ability.** Today OnSelected runs after the first task has already started, so that task can't see Context-targeted modifiers.
5. **Apply the OnSelected changes:**
   - Numeric and timed changes become GameplayEffects applied to the subject's ASC. For example, `GE_Engaged`: 5 s, one stack per combatant, grants `State.Combat.Engaged`, +1 to the Engaged attribute.
   - DataModifiers stay for what GAS can't hold: history entries, references.
6. **Write the intent entry** `{Intent.Cover, Reference, phase none, now, id}`.
7. **Activate the behavior ability.** Its asset tags include its `Intent.*` tag. Its first action raises an occurrence that its owner's self-reactions can answer.

**For a group candidate,** selection picks a free slot on the group's tasks component and a performer among the members who perceived the occurrence. If no slot is free, the request waits on the group (see "Squad tasks vs. self-reactions").

#### 6. Reaction (the behavior runs)

**Phases come from task lifecycle, authored in the details panel.** Tasks already have lifecycle points: activation, the time they're running, and finishing. Today `FContextAdjustmentFromAction` entries all run when an action starts (`AI_SequentialActions.cpp`, after the tasks are run). Give each entry a "when": OnActivate, OnSucceeded or OnFailed.

Each entry can:
- add or remove a State tag,
- apply a GE,
- run a DataModifier, including setting the phase on the behavior's intent entry.

The three phases are then authored on the behavior asset's tasks:

| Task | When | Effect |
|---|---|---|
| EQS task | OnActivate | Add `State.Position.Seeking` |
| MoveTo | OnActivate | Swap Seeking for `Moving` |
| MoveTo | OnSucceeded | Swap Moving for `AtLocation` |

No task code knows about phases. Reading the behavior asset shows everything that happens and when, which the old string-built tags in `AT_MoveTo` hid. State tags a behavior adds are tracked on the behavior and removed when it ends, however it ends.

**Other AI react only to what they can observe.** Each task's activation raises its occurrence. Others react to it through their own conditions and relevance, never by reading this AI's intent.

#### 7. Finish

- **Remove the behavior's State tags.** Duration GEs expire on their own.
- **Close the intent entry with an outcome.** `EAbilityPurposeFeedback` already exists for this (succeeded, interrupted, failed). "Failed to reach cover 3 s ago" is something a condition can use.
- **Apply the purpose's cooldown GE**, if it has one (`Cooldown.Intent.Flank`).
- **Raise an occurrence to self for the freed layer.** This replaces the ad hoc occurrence in `SeekNewBehavior`.

### Worked example (baseline, 2026-10-28)

As first written, before review. Kept unchanged for comparison.

A player shoots AI A. A's ally B is 20 m away. Enemy AI E is watching A from a distance.

1. **The shot.** The player's fire ability applies `GE_Damage` to A. A's Health change raises `Event.Damage.Received` (Instigator = the player, Context = hit direction). The shot also raises `Event.Weapon.Fired`, an audible broadcast.
2. **Receipt.** A receives the damage event. B hears the shot: it records `{Event.Weapon.Fired, t}` and an observation that the player was heard near X. E perceives nothing new yet.
3. **A evaluates.** Two purposes trigger on `Event.Damage.Received`: "cover from Instigator" (`Intent.Cover`, Reference = Instigator) and "return fire". Their conditions read A's Suppression and Morale attributes, and whether A is already AtLocation in cover. Cover wins.
4. **A selects.** `GE_UnderFire` applies (3 s, grants `State.Combat.UnderFire`, raises Suppression). A writes the intent entry `{Cover, player, Seeking}` and the behavior starts.
5. **A announces.** A's announce task sends `Event.Announce {Intent.Cover, Reference = the player's position as A believes it}`, and a cue plays "Taking fire, moving to cover!". B receives it with Instigator = A and Reference = that position.
6. **A moves.** A's EQS task sets Seeking; MoveTo sets Moving.
7. **B reacts to the announcement.** "Suppress the announced reference" (`Intent.Suppress`, Reference = the announced position) triggers on `Event.Announce`. It scores on B's own conditions: Morale, a line of sight to the reference, B not already engaged. B never read A's intent; A told it.
8. **E reacts to what it sees.** E sees A running, which raises `Event.Perception.Movement` with Instigator = A. E can read A's `State.Position.Moving` and current position, but not where A is going. "Push while they're moving" can score on that.

### Worked example (revision 1, 2026-10-28)

The same scene under the revised design. What changed:
- Occurrence tags are the action's or effect's own tag.
- Perception is data, never a gate.
- History is gated by relevance.
- Behaviors run in layers.
- Communication is a behavior, with claims.
- Phases come from task lifecycle entries.
- Health is observable.
- Purposes are skipped only when they provably can't win.

A player shoots AI A. A's ally B is 20 m away. Enemy AI E is watching A from a distance.

1. **The shot.** The player's fire task raises `Action.Weapon.Fire`, its own tag. Its signature in the Purpose settings says loud, long range. The hit applies `GE_Damage` to A, which raises `Effect.Damage` with Instigator = the player, taken from the effect context.
2. **Candidates.** A, B and E are inside the fire occurrence's maximum radius; A is also the target of the effect occurrence. Nobody is filtered out. Each snapshot records how that candidate perceived it:
   - A felt the hit and heard the shot.
   - B heard the shot. It records `Action.Weapon.Fire` and a rough location in its own history.
   - E heard the shot too.

   *Suppressed variant:* the fire signature is quiet. B's and E's conditions read "not heard", and nothing they have scores. A still feels the effect.
3. **Pruning.** The matrix weights `Action.Weapon` × `Intent.Patrol` low. B's patrol purposes can't beat B's current score even at their maximum, so they're skipped without being scored. Nothing that could have won was skipped.
4. **A evaluates, in layers.**
   - Best: "cover from Instigator" (`Intent.Cover`, Reference = Instigator) on Locomotion.
   - Next best that doesn't overlap: "inform squad" on Voice, with content `Intent.Cover` and the player's position as A believes it.

   Both come from the one occurrence. Their conditions read A's Suppression, Fear and Health.
5. **A selects.**
   - The game thread claims the player's position on the Squad subject for A's Inform behavior.
   - `GE_UnderFire` applies.
   - Intent entries `#12 {Cover, player}` and `#13 {Inform, player}` are written.
6. **A reacts.**
   - The Communicate task (Voice channel) raises `Action.Communicate.Voice` and plays the cue "Taking fire, moving to cover!".
   - The EQS task's OnActivate entry adds `State.Position.Seeking`, as authored on the behavior asset.
   - MoveTo's OnActivate swaps it to `Moving`.
   - Each action entry in A's history carries the id of the behavior that caused it.
7. **B hears the callout.** B records its content in its own history: A is taking cover from that position. B's "suppress the communicated reference" (`Intent.Suppress`) scores on B's own conditions: Morale, Fear, whether B can see the reference (via the spatial field), and whether B is already engaged.
   - B never read A's intent; it heard it.
   - Had B also wanted to call out the player, the squad claim would reject that at selection, and B would take its next best purpose.
8. **E watches A.** E has an open relevance window on A. E can read A's `Action.Move` entry, `State.Position.Moving`, velocity and Health, but not A's intent entries. E's "push while they're moving" scores on those. A high-Intelligence E can also ask whether A is heading for a spot that will be covered from E (see "Spatial perception").
9. **E loses contact.** A reaches cover out of E's sight. E's window closes, raising `Perception.Lost`, and the last known position is recorded. A's later entries (AtLocation, reloading) are invisible to E until a new window opens. E's "investigate last known position" uses that position as its Reference, as a virtual subject.

### Worked example (revision 2, 2026-10-29)

What changed since revision 1:
- Occurrences are routed top-down through the command hierarchy.
- Communication is a self-reaction to the AI's own selection, not a second pick from the damage occurrence.
- The squad's voice is a GAS task resource on the squad's tasks component.
- Novelty is keyed by character id.
- Repeated fire is coalesced.
- Sightlines come from the tactical grid on the worker.

A player shoots AI A. A and its ally B, 20 m away, are in squad S1 of platoon P1. Enemy AI E, watching A from a distance, is in squad S9 of platoon P9. The player has character id 37.

1. **The shot.** The player's fire task raises `Action.Weapon.Fire`; the hit applies `GE_Damage` to A, raising `Effect.Damage`. The player keeps firing. Shots inside the fire signature's window refresh the player's history entry and its count instead of raising new occurrences.
2. **Routing.** The director checks platoon bounds against the shot's reach: P1 and P9 overlap it, and other platoons are skipped with one bounds check each. Inside them, squad bounds, then members. A, B and E become candidates, each with its own chain appended as subjects: S1 and P1 for A and B, S9 and P9 for E. No AI holds a reference to its squad.
3. **Perception and pruning.** As in revision 1: A felt and heard it, B and E heard it. Patrol purposes are skipped by upper-bound pruning.
4. **A evaluates the damage occurrence.** "Cover from Instigator" (`Intent.Cover`, Reference = id 37) wins on Locomotion. Informing the squad is no longer picked here.
5. **A selects.**
   - `GE_UnderFire` applies, and intent entry `#12 {Cover, 37}` is written.
   - Selecting it raises a self occurrence tagged `Intent.Cover`, delivered only to A, with context `{Cover, Reference 37, the player's position as A believes it}`.
6. **A reacts to its own decision.** Only self-reaction purposes can score. "Inform squad" (Voice layer) scores: the novelty condition finds that S1 hasn't heard about id 37 recently. At selection, S1's `SquadVoice` resource is free, so A's claim task starts on S1's tasks component and holds it. Intent entry `#13 {Inform, 37}` is written.
7. **A acts.**
   - The Communicate task raises `Action.Communicate.Voice` and plays the cue "Taking fire, moving to cover!".
   - The EQS task's OnActivate entry adds `State.Position.Seeking`; MoveTo's swaps it to `Moving`.
8. **B hears.** B records the content in its own history: A is taking cover from id 37 at that position. B's "suppress the communicated reference" scores. Whether B can see that position comes from the tactical grid, on the worker, with no trace.
   - **If B also wanted to call out id 37:** novelty now fails, so it scores low.
   - **If B had picked it at the same instant on another thread:** its claim task waits behind A's on S1's voice. When it starts, it rechecks novelty and ends without speaking.
   - **If squadmate C spots a different enemy, id 52:** that callout is new, so C speaks as soon as S1's voice is free.
9. **E watches A.** As in revision 1, E reads A's observable entries, State tags, velocity and Health, but not A's intent. A high-Intelligence E asks the grid whether A's predicted position will be covered from E ("Will Be Covered").
10. **E loses contact.** As in revision 1: the window closes, and the last known position becomes the Reference for "investigate".

### Worked example (revision 3, 2026-11-02)

What changed since revision 2:
- No self occurrence on selection. A reacts to its own first action, reading its own intent.
- History has one timestamped entry per happening, with its context, activation, reaction and outcome. Counts and windows are derived.
- One occurrence per activation, keyed through a custom effect context, with no time windows. Repeats merge while queued.
- Mid-burst thresholds arrive as State tag self occurrences.
- The contact report is a squad task performed by a member. Orders are heard occurrences. There are no standing roles.
- Same intent and same reference continue; a new reference starts a new behavior.

A player (character id 37) fires a full-auto burst at AI A. A and its ally B, 20 m away, are in squad S1, an experienced squad (`Verbal.Normal`). Squadmate C is farther back. Enemy AI E, in squad S9, is watching A.

1. **The burst.** The player's fire ability activates once and keeps firing. The player's component counter is now at 812, so this is activation 812 of character 37. Its action occurrence `Action.Weapon.Fire` carries both ids. The first hit applies `GE_Damage` to A, and the custom effect context carries character 37, activation 812 and the cause `Action.Weapon.Fire`.
2. **A's history.** The first hit adds the entry `Effect.Damage ← 37 via Action.Weapon.Fire`, activation 812, and raises an occurrence. The next four hits carry 812 too, so each adds an entry and none raises an occurrence. A now has five entries in that context. Each landed round is recorded as a hit on the player's own fire entries.
3. **Routing.** As in revision 2. A, B, C and E are candidates for the fire occurrence, and A for the damage occurrence. S1 is a candidate too, since A and B perceived it.
4. **A evaluates.** "Cover from Instigator" (`Intent.Cover`, Reference = 37) wins on Locomotion. Intent entry `#12` is written and recorded as the reaction on the first damage entry, the one whose occurrence was evaluated.
5. **A reacts to its own action.** The cover behavior's EQS task finds a spot, then its MoveTo task raises `Action.Move`. A is the instigator, so only self-reaction purposes can score. "Moving!" reads A's current intent and reference from A's own Candidate snapshot, takes the free Voice layer, and passes novelty. It raises `Action.Communicate.Verbal.Normal`. The occurrence itself carries no intent.
6. **S1 reports contact.** S1 evaluates the damage occurrence as a group. "Contact report" wins, and S1 takes a free report slot on its tasks component. A's Voice layer is busy, so the performer is B, who heard the shot. B speaks the report as an ordinary ability, raising `Action.Communicate.Verbal.Normal` with content `{contact, 37, rough position}`.
7. **C hears the order.** C had already reacted to the gunshot on its own by readying its weapon. The report reaches it as a later occurrence. "Suppress the communicated reference" scores on C's Discipline against Fear, and on whether C can see that position on the tactical grid. Reflex first, then the order.
8. **Suppression crosses mid-burst.** The fifth hit pushes A's Suppression past its threshold. `GE_Suppressed` grants `State.Suppressed`, and gaining it raises a self occurrence for A only. "Stay down" beats continuing to move. It has a new intent, so A's cover behavior ends as Interrupted.
9. **A second shooter.** Player Q (id 41) hits A. `Effect.Damage ← 41 via Action.Weapon.Fire` is a new context, so its entry raises an occurrence. "Cover from Instigator" against 41 has a new reference, so it starts a new behavior if it wins. Today's identical-behavior guard would have skipped it.
10. **Semi-auto taps.** Later the player taps three single shots (activations 813, 814, 815) while A's thread is busy. All three hit, so three occurrences for `Effect.Damage ← 37 via Action.Weapon.Fire` queue up. The thread drains its queue, keeps the newest by timestamp and evaluates once. That snapshot holds eight entries in this context, and the latest one with a reaction is the first (from step 4). So A's conditions see seven hits since it last reacted to this shooter's gunfire: four within one second of the first hit and three just now. The step 8 reaction was recorded on the `State.Suppressed` entry, not in this context. If A is watching the player, the player's fire entries also show any taps that missed.
11. **A takes stock.** By now A has fired at the player six times and seen one land. A watched the player fire, so A can read the player's fire entries at A through relevance: 8 of 12 landed. "Fall back" scores higher. A never copied the player's numbers into its own history.
12. **E watches A.** As in revision 2. E reads A's `State.Suppressed`, position and observable attributes while it perceives A, but receives none of A's self occurrences.

### Worked example (revision 4, 2026-11-14)

What changed since revision 3:
- The manager builds every occurrence through `RaiseOccurrence`. Records are immutable and shared, and each points to its cause.
- Loudness and visibility come from the equipment's attributes, captured when the occurrence is raised. There's no settings table.
- Relevance comes from the occurrence definition's weighted list of intent tags, not a matrix.
- Action tags name the verb (`Action.Fire`, `Action.Throw`), and the equipment is its own subject.
- Behavior changes only on a new occurrence. Boredom and focus are attributes.
- History entries point to occurrence records instead of copying their fields.

A player (character id 37) carries a suppressed rifle. AI A and B are in squad S1. Guard G, in S1 too, is on a long patrol 40 m away and bores easily. F, in squad S2, is out of earshot.

1. **Before the fight.** G has patrolled for ten minutes, and a boredom GE has been raising G's Boredom attribute the whole time. Nothing re-evaluates G's patrol while that happens. Then a bird lands near G, a small world occurrence. G's patrol is re-scored first, and its Boredom condition drags it down; that's the baseline. "Watch the distraction" also scores on Boredom and beats it. A disciplined guard with low Boredom would have ignored the same bird. Either way, a player watching saw the bird.
2. **The shot.** The player's fire ability performs `Action.Fire`. `RaiseOccurrence` builds record R1: instigator 37, the rifle as the Equipment subject, activation 812. Its loudness is the player's Loudness evaluated for `Action.Fire`: the rifle's and the suppressor's GEs both apply, the boots' don't. The value is captured in R1. The routing reach is derived from that loudness.
3. **The hit.** `GE_Damage` lands on A. The custom effect context carries 37, 812 and `Action.Fire`. Record R2 (`Effect.Damage`) is built, with `Cause` pointing to R1. A's history entry points to R2.
4. **Relevance.** `Action.Fire`'s list weights `Intent.Act.Patrol` at 0.1, so patrol purposes are pruned without scoring. Nobody's list mentions G's distraction behavior, but G doesn't hear R1: the captured loudness against 40 m fails G's hearing condition. B, 10 m away, does hear it.
5. **A reacts.** "Cover from Instigator" wins. A's behavior holds a reference to R2, the purpose and its score, and nothing else. A's MoveTo performs `Action.Move` as record R3, with `Cause` = R2.
6. **B returns fire.** B's suppress behavior was started by R1, so B's own `Action.Fire` record R4 has `Cause` = R1. B's rifle has no suppressor, so R4's captured loudness is high.
7. **G hears B.** R4 reaches G. `Action.Fire`'s list weights `Intent.Investigate` above 1.0, and G's investigate beats watching the bird. G's behavior holds R4, which records B's position at the moment of firing.
8. **A grenade.** The player throws a grenade at B: record R5, `Action.Throw`, instigator 37, the grenade as the Equipment subject, target the landing point. B sees it land. "Get clear of the Equipment subject" reads the grenade's position from R5. Then the grenade raises its own detonation, R6, carrying 37, with `Cause` = R5. The tag differs from R5's, so R6 is queued despite sharing the throw's activation.
9. **B dies, and the actor is destroyed.** G's investigate still holds R4. Its weak pointer to B goes null, but R4 still has B's character id and the position captured when B fired, so G carries on to that position. B's death is its own occurrence, and G's yes or no to it is the normal path.
10. **F learns of it.** S1's contact report raises `Action.Communicate.Verbal.Normal` (R7) with content about 37 and A being hit. F's squad hears it over the radio, and F's history now points to R7. Through R7, F can see R2: that A was hit by 37. F can't follow R2 back to R1's details, such as where the player fired from, because nobody told F's group. That link reads as "caused by something I don't know".
11. **Lifetimes.** If R1 had produced no yes from anyone, it would have been freed as soon as the last worker finished. Instead A's, B's and G's history entries hold it until `Action.Fire`'s retention expires. R2 lives while A's cover behavior or any entry still refers to it. When the last holder lets go, the record is freed, and the weak `Cause` links pointing at it simply go null.

### What stays Purpose-only

- Scoring, selection, conditions and curves.
- History ages ("how long ago") and relevance.
- Subjects, references and the squad.
- Snapshots and the evaluation threads.

### Migration from the current tags

- **`Contextual.Combat.Engaged.1–5`:**
  - Replace with an Engaged attribute, `GE_Engaged` stacks and `State.Combat.Engaged`.
  - Retire the string parsing in `FContextualHistoryModifier_Engaged`.
  - Replace `UCondition_ContextualHistory_EngagedByCountForSubject` with an attribute-value condition.
- **`Contextual.Position.<Kind>.<Phase>`** becomes `Intent.<Kind>` plus `State.Position.<Phase>`, with phases authored as task lifecycle entries. The NearInstigator tag-string bug and the `AT_MoveTo` finished-tag bug go away with the code they're in.
- **`Contextual.Position.Investigative.*`** becomes `Intent.Investigate` plus a Reference (the instigator, or a last known position). The search method stays on the EQS query asset.
- **The root-level `Moving` and `Seeking` tags** become `Action.Move` and `Action.Query.Position` as the tasks' identity tags. The phases move to `State.Position.*`.
- **EngagedCombatants, Morale and LevelOfAlarm** become attributes. Fear and Intelligence are added.

### Open questions (this flow)

- **Design cornerstone, open: which State tags and observable attributes can be perceived at range, and which only up close.**
- **Resolved:** same intent with a new reference always starts a new behavior.
- **Resolved:** truth vs. belief is handled by relevance-gated history, with last known positions taken from relevance records.
- Should others ever read intent entries? The recommendation is no, unless it's communicated. Inferring intent from observed actions could later become an Intelligence-gated condition.
- Health: observable attribute (recommended) or visible wounds? See "Health and other observable numbers".
- **Superseded in revision 3:** behavior selection no longer raises a self occurrence. The owner reacts to its own first action instead.
- Should layer and resource sets be fixed by design, or derived freely from tasks?
- **Superseded in revision 3:** group tasks are owned by the group's tasks component, so routing them through a member's ability isn't needed.
- Resource ID budget: body resources share 16 class IDs game-wide (12 used). Group slots use raw bits and don't count against it.
- Character id: assigned by the director, but how is it kept stable through host migration and delegated brains?

### Open questions (revision 3)

Places where I'm unsure what you intended, or where a choice is still open.

**History and outcomes**
- **Decided: the shooter's own fire entry records whether its shots land,** because the shooter needs to know whether it's being effective. Reading the target's damage entry through relevance isn't enough, since it fails whenever the shooter loses sight of the target.
- **Open, depends on shot calculation: when and how a shot's result reaches the entry.** It follows from how shots are resolved, which isn't designed yet:
  - **Hitscan:** the result is known when the shot fires, so hit or miss is recorded at once.
  - **Projectiles:** a hit arrives on impact, through the ids in the effect context. A miss needs its own signal, such as the projectile expiring or striking something other than the target.
  - **Either way:** a miss has to be recorded explicitly, because only hits apply an effect.
- **What a fire entry stands for.** Revision 4: one entry per repeat occurrence, carrying the count of rounds since the previous one. Hits still need to reach the right entry. One entry per round, or one per activation (trigger pull)? A hit/miss ratio needs rounds, but `ActionPerformed` runs per activation, so a burst would have to add an entry for each round as it fires.
- **Own attribute changes.** Should an entry also record changes to the owner's own attributes ("my Health −40 from P"), or should conditions just read current attributes? Others never see either.
- **Decided (2026-11-08): one timestamped entry per happening,** then monitor how the data builds up and optimize from there (see "Monitoring history"). Considered and deferred: one record per context with a fixed-size time series, and one record per context with attributes alone carrying intensity. Both stay on the list of optimizations once there's data.
- **Name for the position-finding action.** The EQS task only finds a spot; moving there is the MoveTo task's `Action.Move`. `Action.Query.Position` reads as asking for a position, which is what the task does, but if that's unclear, `Action.Seek.Position` matches its `State.Position.Seeking` phase.
- **Retention.** How long entries live, and the per-character cap. Revision 4 puts retention on each occurrence definition and the cap on the character's component; monitoring should set the values. A context whose entries have all been pruned is forgotten, so a long-past exchange with an enemy no longer counts.

**Occurrence identity**
- **Concern: thread routing for merges.** Merging needs every repeat of a context on the same thread, so threads would be picked by a hash of the context key rather than purely by queue depth. A busy context, such as one AI under sustained fire, could then pile work onto one thread while others sit idle. Measure the imbalance under load before settling on it. One option is to hash by context key and fall back to the least-loaded thread when the hashed one is far behind, accepting that those repeats may not merge.
- **What counts as one activation.** `UAT_Repeat_Input` sends a gameplay event on an interval. If each event activates the fire ability, every round is a new activation and raises an occurrence. Should full auto become one activation that fires internally, as you suggested, with `UAT_Repeat_Input` kept for other repeats?
- **Resolved in revision 4: long bursts and late listeners.** Continuous actions re-raise every N physical events while active. Every candidate receives each repeat; merging and a novelty condition keep it from re-triggering reactions (see "Occurrence identity and repeats").
- **The custom effect context.** Recommended: ids only (character id, activation id, action tag), not our `FContextData`. Confirm, or name anything else the receiver needs.

**Self-reactions and State tags**
- **Resolved in revision 4: which State tags raise self occurrences.** Those with a definition in the purpose library. Phase tags like `State.Position.Moving` simply get no definition.
- **Visible state changes.** A State tag change reaches only its owner. When others should react to it, such as someone going down, does the downed mechanic perform an action (`Action.*`) that others perceive, or do others notice only on their next occurrence?
- **Self-reactions on a group's action.** When a member performs a squad task's body action, does that member's self-reaction see the squad task as its current behavior?

**Squads**
- **Who performs a squad task.** A member who perceived the occurrence and has the needed layer free. How is that member chosen among several: Intelligence, the leader, distance?
- **What a group's snapshot holds.** Which member perceived what, the members' positions and free layers, and recent communications? Members' current intents are private, so a group can't read them either. Is that right, or does a squad know its members' intents?
- **The leader.** No standing roles, but a squad still has a leader for "await orders". Is the leader fixed per squad, or chosen dynamically (the highest Intelligence still standing)?
- **Generalizing candidates.** Candidates are `UPurposeAbilityComponent` today. Should groups get a small shared interface, or a component of their own?

**Ability duplication**
- **Instancing doesn't remove `GiveAbilityDuplicate`.** GAS instantiates from the class (`CreateNewInstanceOfAbility` → `NewObject(Owner, Ability->GetClass())`, `AbilitySystemComponent_Abilities.cpp`). So per-purpose values on the inline template would be lost whatever the policy. The route in the `Design: GAS AbilitySpec SourceObject` note in `PurposeAbilityComponent.h` still applies: grant one spec per purpose per actor with the purpose asset as `SourceObject`, `InstancedPerActor`, and reuse it (Performance #4). Confirm before the refactor.

### Open questions (revision 4)

**Tags**
- **Placing the current intents under activities.** Is Cover an `Act` or a `React`? `React` is "a form of inaction" (holding, freezing, staying down), so moving to cover may be `Act`.
- **More activities** as behaviors are added.

**Signatures and equipment**
- **Resolved (2026-11-16): equipment carries GEs, applied to the wielder,** with modifiers tag-required to their action (see "Character attributes").
- **Resolved (2026-11-16): footsteps and other continuous actions** re-raise at an interval while active, like automatic fire.
- **Resolved (2026-11-17): re-raise interval.** A time on the action's definition, checked on each physical event, never by a timer. The record carries the count of rounds or steps since the last repeat.
- **Hits under the same rule?** Revision 3's hit rule is still a gate: a hit from the same activation as the latest one in its context only records. Should hits follow the action rule instead, raising every N hits and leaving novelty to a condition?
- **Resolved (2026-11-16): carried but not held.** Only equipment in hand is equipped. Holstered items are socketed world items.
- **Resolved (2026-11-16): latent equipment's signature.** The throwable keeps its own GE, never applied to the owner, and its magnitude is read directly when the detonation is raised.
- **Resolved (2026-11-17): action GE duration.** Isolation comes from the tag requirements, not the duration. A footstep GE's modifier requires `Action.Move`, so speech raised right after a step is evaluated for `Action.Communicate.*` and never includes it. The duration only removes the GE afterwards, so its exact length doesn't affect any value. It can't be an Instant GE, which would permanently change the attribute's base value.
- **Voice levels.** The innate GE from character data carries one modifier per `Action.Communicate.Verbal.*` level tag. Confirm.
- **Which attributes an action reads.** Does each occurrence definition name the attributes to capture (loudness, visibility…), or does every record capture a fixed set?
- **Equipment role name:** Equipment, or something broader.
- **What a piece of equipment is** (explosive, obscurant, how dangerous) from its attributes rather than fixed tags. Later.

**Observable attributes**
- **Resolved (2026-11-16): there's no observable/private mark.** Perception decides, with an attribute snapshot kept when a subject stops being perceived.
- **Snapshot contents.** All attributes, or only those some condition reads about other characters? Monitoring can decide, as with history.

**Occurrence records**
- **Finding a hit's cause record.** The effect context carries ids, not the record. The manager could keep live action records indexed by (character id, activation id), so a hit's record can point to its fire record. That map has to stay consistent with records being freed.
- **Resolved (2026-11-16): chain depth.** Links are readable for a set time and pruned a margin later.
- **Where the chain time lives:** on each occurrence definition, like retention, or one value for every link?
- **How communicated content links in.** When a report tells F's group about R2, is the communication's record what makes R2's link visible, or does F's group get its own history entry pointing to R2?

**Selection and individuality**
- **Resolved (2026-11-16): what a new candidate has to beat.** The current behaviors, re-scored at each new occurrence.
- **Resolved (2026-11-16): a current behavior that fails its own re-score** keeps running. Ending is left to the behavior's own finish conditions. Its low re-scored baseline means almost anything replaces it at the next occurrence.
- **Re-score cost.** One extra purpose per layer per candidate per occurrence, plus fresh snapshots of the current behaviors' subjects. Small, but it adds to snapshot building on the game thread.
- **The self weight default.** It's 0, unlike list weights at 1.0, so AI don't evaluate every one of their own actions. Confirm.

**Manager**
- **Subsystem plus director actor** (as written), or a single director actor.
- **Thread count**, and whether it scales with the host's core count.

## Design: spatial perception (proposed)

**Goal:** let conditions ask spatial questions such as:
- "Is the enemy moving to the left, where my flank is exposed?"
- "Is the enemy behind a wall in that direction, or about to be?"

They should be answerable without world traces on the evaluation thread. Proposed 2026-10-28; revised 2026-10-29 to the RGBA grid layout, and 2026-11-02 to a camera-captured bake with runtime overlays and repairs.

### Why a plain heightmap falls short

- **One value per column.** A 2D heightmap stores one height per texel, so multiple floors, bridges, overhangs and interiors are lost.
- **Cover has a direction.** A wall north of you covers you from the north and not from the east, which one scalar per texel can't express.
- **Holes are features of a vertical surface.** A top-down value can't say a wall has a window at 1.2 m.

### The grid

A grid of cells (texture-like, one slice per floor) where each cell stores directional data. The navmesh is not involved. It can serve as a mask to skip cells nobody can stand in, but nothing is added to it, so pathfinding cost doesn't change.

**Per floor slice, per cell:**

| Bytes | Content |
|---|---|
| 2 | Ground height, above the tile's minimum Z |
| 1 | Headroom to the ceiling (crouch-only spaces) |
| 1 | Flags (water, ladder, spare) |
| 8 | Per direction (N, NE, E… NW): distance to the first blocker at chest height, log-encoded |
| 8 | Per direction: see-through mask. Bit *b* set = at height band *b*, sight continues past the first blocker |
| 8 | Per direction: distance to the first non-penetrable blocker (bulletproof), log-encoded |

That's 28 bytes per cell. The floor index is the slice, so it needs no channel.

- **Holes, as raised in review.** A hole is described by which height bands see through, per direction. That replaces a hole height and size per cell: with 8 bands of 25 cm (0–2 m), a window at 1.0–1.5 m in the north wall sets bits 4–5 of the N mask.
  - **The hole's width** shows in the neighbouring cells: every cell whose N line passes through the window has those bits, and the others don't. There's no square assumption, and a ragged destruction hole works the same way.
  - **A per-cell field would fail** when a cell has more than one wall, each with its own hole, or a wall whose hole only lines up from some directions.
- **Penetrable vs. solid** must be per direction, for the same reason as cover. Two distances (first blocker, first bulletproof blocker) give concealment vs. cover: a plank fence conceals but doesn't stop rounds.
- **Size:** 16 km² with 30% standable is about 530k cells (roughly 15 MB) at 3 m cells, or about 4.8M cells (roughly 134 MB per floor before compression) at 1 m. It streams in tiles (see "Building the grid").
- **Viewable as textures.** The layout maps onto RGBA8 textures for an editor debug overlay, but the AI reads a CPU-side array.

**Queries are lookups, safe on the evaluation thread:**
- **Is B covered from A:** at B's cell, take the direction toward A. Covered if the first blocker is nearer than A, and the see-through mask has no open band at A's eye height.
- **My exposed side:** the directions at my cell with long distances toward where enemies are or could come from.
- **Is B heading somewhere covered from me:** predict B's position from position + velocity × t, then run the cover check at that cell.
- **"Enemy moving toward my exposed flank":** combines the enemy's observable velocity with my exposed directions.

For static geometry, this resolves the problem `UCondition_SubjectVisibility` has today.

Guerrilla described a similar per-waypoint approach (precomputed directional depth for line-of-fire and cover queries) for Killzone's AI. It's worth reading before building this.

### Building the grid: a baked map, plus overlays and repairs

Revision 2 rebuilt a fine area-of-operation tier at runtime with traces. At 80 rays per cell, a 100 × 100 m area at 1 m spacing is 800k rays per floor, about 4.4 s of traces at 2,000 per frame. Revision 3 bakes the whole map once and only fixes what changes.

| Approach | Accuracy | Runtime cost | Verdict |
|---|---|---|---|
| **Orthographic depth captures** | Matches what's rendered, including foliage without collision; penetrability needs extra data | A render, so offline only, or small and occasional | The bake |
| **Physics traces** | Matches bullets, but misses anything without collision | High, and only at safe points | `firstSolid`, and small repairs |
| **Analytic prop stamps** | Good for box-like props such as barrels | Very low, on a background task | Dynamic props |
| **Voxels, then rays through our own bit array** | Solid plus penetrable bits; holes smaller than a voxel close | About 3.2 MB for 100 × 100 m at 25 cm | Fallback if cells need to be finer |
| **Ray tracing or distance fields** | Good | Both disabled in `DefaultEngine.ini` | Ruled out |
| **Player cameras' depth at runtime** | Only where players look | Reuses depth already drawn | Rejected, below |

**The bake: orthographic depth captures, in a rendering editor session.**
- Don't capture from each cell, which would be millions of captures. Capture orthographic depth (`SCS_SceneDepth`) along each of the 8 directions, one pixel row per 25 cm height band, peeling several depth layers.
- Each pixel row is a line through many cells. The sorted surfaces along it give every cell its first blocker in that direction, and the see-through bits come from the same rows.
- The capture sees visual meshes, so foliage without collision becomes concealment, which traces miss.
- **Penetrability** needs a stencil value per material, or `firstSolid` kept from collision traces.
- It needs a rendering editor session, not a commandlet run with `-nullrhi`.
- Saved per World Partition cell and streamed with it. Try 1 m walkable cells first; roughly 134 MB per floor before compression for 16 km² at 30% walkable.

**Runtime: the baked tile plus overlays.**
- **Dynamic props** (barrels, vehicles, debris at rest) are stamped analytically from their bounds on a background task.
- **The area of operation** only marks where overlays apply, so moving it rebuilds nothing. It can still be the level between the director and platoons in occurrence routing.

**Repairs after destruction.** The fracture occurrence (server-side, per [Physics.md](Physics.md)) gives the broken bounds.
1. **Dirty set:** on a background task, check the cells within `MaxDistance` of those bounds. A direction is dirty only if its stored first blocker lies inside the bounds. That's arithmetic on our own data, and it usually leaves a small set.
2. **Repair**, one of two methods (open question):
   - **A. Traces:** queue `AsyncLineTraceByChannel` for the dirty directions within a per-frame budget. Traces don't see foliage, so repairs must touch only the blocker and solid data, kept in a separate layer from the captured concealment.
   - **B. Directed captures:** one small orthographic depth capture per direction, taken on demand with `CaptureScene()` and spread across frames. Set `CustomNearClippingPlane` at the near edge of the destroyed bounds. Nothing sat between the dirty cells and the bounds, so the first surface past the near edge is their new first blocker. That's a single layer with no peeling, about one pixel per cell column by one row per band. The bake and the repairs then use the same source, so concealment survives a repair.
3. **Publish** copy-on-write, as before.

**Capture costs to watch (method B):**
- **A capture is a render,** off-screen into a render target. There's no engine path that rasterizes Nanite meshes on the CPU.
- **Low resolution only cuts the per-pixel cost.** Each capture still pays a fixed setup cost: view setup, visibility and Nanite culling, the depth pass. Turn off every show flag the depth pass doesn't need, use the show-only list, spread directions across frames, and measure on the VR host.
- **Coarse pixels** can miss a thin pole between pixel centers, which is acceptable for cover.
- **Cosmetic debris** is client-local (Physics.md), so the host's own debris would appear in its capture. Exclude it with the show-only list (`PRM_UseShowOnlyList`) or a hidden list.
- **Readback** with `FRHIGPUTextureReadback`: enqueue the copy on the render thread, poll until it's ready a few frames later, then hand the bytes to a `UE::Tasks` job. Never `ReadSurfaceData`, which stalls the render thread until the GPU finishes; moving that call to another thread doesn't help.
- **Compute shaders** could turn depth rows into distances and see-through bits before readback, but the data is already small. They can't replace the capture, since they need geometry to read.

**Why not the players' cameras at runtime:**
- **Coverage:** it only updates where a player is looking, so AI behavior depends on head direction and is hard to reproduce.
- **Data shape:** depth gives camera rays, while cells store 8 fixed directions. It would need the voxel tier first.
- **Bad readings:** a soldier in frame looks like a wall, and depth can't tell whether something is penetrable.
- **Redundancy:** the game already knows when props move or something is destroyed.

### Threading

- **The game thread never reads the grid.** The game thread only hands each occurrence a pointer to the current published grid, and conditions read it on the evaluation threads.
  - Every condition in one occurrence sees the same version.
  - The version stays alive until the occurrence is done, with no locks during evaluation.
- **Building it does need physics queries, and those can't come from our own thread.** Tracing from a background thread races the game thread moving actors in the Chaos scene, the same problem as SubjectVisibility.
  - **The bake** runs offline in the editor, so the race doesn't apply.
  - **Trace repairs** use `UWorld::AsyncLineTraceByChannel`. The game thread only queues the traces; the engine runs them in parallel at a safe point and hands results back the next frame.
  - **Capture repairs** are issued from the game thread and read back with `FRHIGPUTextureReadback`. Our threads never touch the RHI.
  - **Packing** results into tiles, and stamping props, runs on a background task (`UE::Tasks::Launch`). The new version is published by swapping a pointer.
- **Copy-on-write.** A new version shares every unchanged tile with the previous one, so a rebuild copies only the tiles that changed.

### Code outline

```cpp
// TacticalGrid.h

/// One cell of one floor slice. See the table in AI.md "The grid"
struct FTacticalCell
{
	uint16 groundZ = MAX_uint16;	/// cm above the tile's minZ. MAX_uint16 = no floor in this slice
	uint8 headroom = 0;			/// cm / 4 to the ceiling
	uint8 flags = 0;
	uint8 firstBlocker[8] = {};		/// Log-encoded distance per direction, at chest height
	uint8 seeThrough[8] = {};		/// Bit per 25 cm height band: sight continues past the first blocker
	uint8 firstSolid[8] = {};		/// Log-encoded distance to the first non-penetrable blocker
};

struct FTacticalTile
{
	FIntPoint coord;
	double minZ = 0.0;
	int32 numFloors = 0;
	TArray<FTacticalCell> cells;	/// [floor][y][x]
};

/// Immutable once published. Occurrences hold a TSharedRef to one version for their lifetime
struct FTacticalGridSnapshot
{
	float cellSize = 100.0f;
	int32 cellsPerTile = 64;
	TMap<FIntPoint, TSharedRef<const FTacticalTile>> tiles;	/// Unchanged tiles are shared between versions

	/// The floor slice whose ground is nearest below location
	const FTacticalCell* FindCell(const FVector& location) const;
};

namespace TacticalGrid
{
	constexpr float MaxDistance = 5000.0f;
	constexpr float BandHeight = 25.0f;

	uint8 EncodeDistance(float cm);	/// Log scale: fine up close, coarse far away
	float DecodeDistance(uint8 encoded);
	int32 DirectionIndex(const FVector& direction);	/// Nearest of the 8 directions on XY

	/// Worker-safe. True if target's cell is shielded from observer in observer's direction
	bool IsCoveredFrom(const FTacticalGridSnapshot& grid, const FVector& target, const FVector& observerEye, bool bRequireSolid);
}
```

```cpp
// TacticalGrid.cpp

bool TacticalGrid::IsCoveredFrom(const FTacticalGridSnapshot& grid, const FVector& target, const FVector& observerEye, bool bRequireSolid)
{
	const FTacticalCell* cell = grid.FindCell(target);
	if (!cell) { return false; }

	const FVector toObserver = observerEye - target;
	const int32 dir = DirectionIndex(toObserver);
	const float blocker = DecodeDistance(bRequireSolid ? cell->firstSolid[dir] : cell->firstBlocker[dir]);
	if (blocker >= toObserver.Size2D()) { return false; }/// Nothing between them in that direction

	/// A hole at the observer's eye height still exposes the target
	const float eyeAboveGround = observerEye.Z - target.Z;/// Approximation: the band where the line crosses the wall
	const int32 band = FMath::Clamp(FMath::FloorToInt(eyeAboveGround / BandHeight), 0, 7);
	return (cell->seeThrough[dir] & (1 << band)) == 0;
}
```

The trace version below predates revision 3. The bake now comes from depth captures (see "Building the grid"); these traces remain the reference for `firstSolid` and for repair method A.

```cpp
// TacticalGridBakeCommandlet.cpp (editor only)

int32 UTacticalGridBakeCommandlet::Main(const FString& Params)
{
	/// 1. Load the map. With World Partition, load one region at a time so static geometry is present
	/// 2. For each tile and each column: BuildColumn → floors
	/// 3. For each floor: BuildDirections
	/// 4. Save one UTacticalGridData asset (tiles as byte arrays) per World Partition cell
	return 0;
}

/// Walks down a column, recording every walkable surface with room above it as a floor
void BuildColumn(const UWorld& world, const FVector2D& xy, double topZ, double bottomZ, TArray<FTacticalCell>& outFloors)
{
	double z = topZ;
	while (z > bottomZ)
	{
		FHitResult hit;
		if (!world.LineTraceSingleByChannel(hit, FVector(xy, z), FVector(xy, bottomZ), ECC_WorldStatic, QueryParams)) { break; }

		if (hit.ImpactNormal.Z >= WalkableZ)
		{
			const float headroom = TraceUp(world, hit.ImpactPoint, MaxHeadroom);
			if (headroom >= CrouchHeight) { outFloors.Add(MakeFloor(hit, headroom)); }
		}
		z = hit.ImpactPoint.Z - SlabSkip;/// Step below this surface and keep going (traces don't hit back faces)
	}
}

/// 8 directions × (1 chest trace + 8 band traces + 1 solid trace)
void BuildDirections(const UWorld& world, const FVector& ground, FTacticalCell& cell)
{
	for (int32 dir = 0; dir < 8; ++dir)
	{
		const FVector direction = FRotator(0.0, dir * 45.0, 0.0).Vector();

		const float first = TraceDistance(world, ground + FVector(0, 0, ChestHeight), direction, ECC_Visibility);
		cell.firstBlocker[dir] = TacticalGrid::EncodeDistance(first);

		for (int32 band = 0; band < 8; ++band)
		{
			const FVector start = ground + FVector(0, 0, (band + 0.5f) * TacticalGrid::BandHeight);
			if (TraceDistance(world, start, direction, ECC_Visibility) > first + SeeThroughMargin) { cell.seeThrough[dir] |= 1 << band; }
		}

		cell.firstSolid[dir] = TacticalGrid::EncodeDistance(TraceDistance(world, ground + FVector(0, 0, ChestHeight), direction, ECC_Bulletproof));/// A custom channel penetrable materials ignore
	}
}
```

```cpp
// TacticalGridSubsystem.h (runtime: streaming baked tiles and building AO tiles)

UCLASS()
class UTacticalGridSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:

	/// Game thread, pointer copy only. Stored in each FOccurrence when it's raised
	TSharedRef<const FTacticalGridSnapshot> Current() const { FReadScopeLock lock(publishLock); return published; }

	/// From a destruction occurrence. Only directions whose first blocker lies inside region become dirty
	void MarkDirty(const FBox& region);

	void Tick(float deltaTime) override
	{
		/// 1. Issue up to MaxTracesPerFrame AsyncLineTraceByChannel calls for dirty directions, with OnTraceDone as the delegate (repair method A)
		/// 2. OnTraceDone (game thread, next frame) moves each result into its pending tile
		/// 3. When a tile's traces are all back:
		///    UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, pending = MoveTemp(pendingTile)]()
		///    {
		///        TSharedRef<const FTacticalTile> tile = PackTile(pending);
		///        Publish(WithTile(Current(), tile));	/// New version shares every other tile
		///    });
	}

private:

	void Publish(TSharedRef<const FTacticalGridSnapshot> next) { FWriteScopeLock lock(publishLock); published = next; }

	mutable FRWLock publishLock;
	TSharedRef<const FTacticalGridSnapshot> published = MakeShared<FTacticalGridSnapshot>();
};
```

**Rough edges to settle in a prototype:**
- **Directions are quantized to 8.** Checking the two nearest directions and taking the stricter one avoids gaps between them.
- **Height difference is approximated.** The see-through band uses the observer's eye height above the target's ground, which ignores where the line actually crosses the wall. It's fine for level ground; steep height differences need the crossing point (from the first-blocker distance).
- **Bake cost.** Captures cover many cells per pixel row, so the bake scales with map area and peel depth, not with cells × rays. Measure how many peeled layers interiors need.

### Heat layers

Per-cell scalars on the same grid can hold:
- danger (recent fire),
- recent enemy presence,
- squad occupancy.

Occurrences write them, and they decay over time. This is history in spatial form, so it follows the relevance rule. Keep these layers per squad (what the squad knows), never as a global truth everyone reads.

### Condition examples

- **Covered From:** the candidate is covered from a subject (cover vs. concealment as a setting).
- **Exposed Toward:** a subject's velocity is heading toward the candidate's exposed side.
- **Will Be Covered:** a subject's predicted position, t seconds ahead, is covered from the candidate.

Intelligence can set the prediction time and the precision used (see "Character attributes").

### Spatial findings: computed, stored only when chosen (proposed 2026-11-08)

**Problem:** a query finds that the left flank is exposed. 30 seconds later the character is somewhere else, and that finding is wrong. Storing every finding means invalidating it, and the data piles up.

**Three kinds of finding, each with its own home:**

| Kind | Example | Home | Validity |
|---|---|---|---|
| A fact about where I am now | My left flank is exposed; I'm covered from E | Not stored. A condition looks it up in the grid at the candidate's current cell each time it scores | Always current, since it's recomputed from the current position and grid version |
| A position a behavior committed to | The cover point the EQS task chose | The behavior's Reference or destination, closed into its intent entry when it ends | Lives exactly as long as the behavior |
| The fact that I searched | Looked for cover here and found none | A history entry for the query action, with the result as its outcome | Ages like any entry; "failed to find cover 5 s ago" is a usable condition |

- **Most spatial questions need no storage at all.** Grid lookups are cheap and safe on the evaluation thread, so "is my flank exposed" is asked fresh at each evaluation. Nothing goes stale and nothing grows.
- **Committed positions are already covered** by Reference subjects and intent entries.
- **Last known positions of others** are already in relevance records.

**A cache only if profiling asks for one.** If some EQS query proves too expensive to rerun, keep one small spatial context chunk on the candidate, replaced rather than appended:
- **Entry per query type:** `{result, origin, time, grid version}`.
- **Valid while** the character is within a radius of the origin, the entry is younger than a maximum age, and the grid version hasn't changed in that area (destruction).
- **Size stays fixed:** one entry per query type, never a growing list.

### Open questions

- **Spatial findings cache:** needed at all, or are grid lookups plus Reference subjects enough? If needed, what radius and maximum age per query type?
- Cell size, band count and direction count. 1 m walkable cells first; voxels if that proves too coarse.
- **Repair method:** traces (A) or directed captures (B). B keeps foliage concealment through repairs, but its fixed per-capture GPU cost on the VR host needs measuring first.
- **Penetrability source:** a stencil value per material in the captures, or `firstSolid` from collision traces.
- **Destructible foliage:** if foliage can be destroyed, trace repairs (A) need their own rule for its concealment.
- How AO bounds are chosen, and how often they move. They only decide where overlays apply now.
- Characters aren't in the grid. Is a separate check needed for bodies blocking sight?
- Delegated brains: the client running an area's AI needs the grid. The baked tier ships with the map; overlays and repairs would have to be applied on that client too.
- **Generated maps (idea, 2026-11-11):** if maps are generated per session by PCG on mesh terrain, the grid could be built in parallel from the PCG data as terrain and props are placed, instead of baked from captures. Each client would build it from the same seed. See [Roadmap.md](Roadmap.md) "World generation".

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

- **Nanite skinned meshes, same test (2026-11-13, packaged, SteamVR, 100 close animated characters with the Animation Blueprint, shadows on, buffered occlusion queries).** The mannequin asset (`SKM_Manny_Simple_5_7_ColorGroups`) with Nanite enabled; `r.Nanite.AllowSkinnedMeshes` is on by default in 5.8. Proof it took effect: GPU skin-cache dispatches 107 → 2 per frame (only the hands), traditional base-pass draws 0.63 → 0.06 ms (the hands), and no traditional velocity draws at all. Against the 10-26 run, plus a same-day non-Nanite repeat (same build before the Nanite cook) to separate day-to-day drift:

  | | Non-Nanite 10-26 | Non-Nanite repeat 11-13 | **Nanite skinned** |
  |---|---|---|---|
  | GPU scene (`Scene`) | 5.50 ms | 5.83 ms | **4.22 ms** |
  | GPU skin cache | 0.33 ms | 0.27 ms | ~0 |
  | Velocity pass | 0.63 ms | 0.65 ms | **0** (Nanite writes velocity in its own raster) |
  | Base pass (traditional + Nanite) | 1.12 ms | 1.15 ms | 0.57 ms |
  | Shadow depths (VSM) | 0.74 ms | 0.76 ms | 0.52 ms |
  | Nanite VisBuffer (trees + crowd) | 0.73 ms | 0.90 ms | 0.77 ms |
  | Game thread work | 3.44 ms | 3.67 ms | 3.63 ms |
  | Render thread (`FDeferredShadingSceneRenderer_Render`) | 1.99 ms | 2.07 ms | 2.04 ms |
  | Worker threads, total foreground work | 14.2 ms | 14.2 ms | 13.3 ms |
  | Frames over 12.5 ms | 0.11% | 2.50% | 0.08% |

  **GPU about 1.3–1.6 ms cheaper (roughly −25% of the scene), CPU unchanged.** Nanite raster for the 100 characters costs almost nothing on top of the trees (VisBuffer +0.04 ms against 10-26), while the skin cache, the velocity pass (the ~0.6 ms flagged earlier as the biggest removable GPU item) and most of the traditional base pass disappear, and VSM shadow depths drop 0.23 ms. Game and render thread cost is the same, as expected: animation and component tick don't change, so the per-character CPU cost (~20 µs) is still the next limit. The 2.5% hitching in the same-day repeat came with `xrWaitFrame` spikes right after an OpenXR runtime switch (Virtual Desktop → SteamVR), not from the game. **Caveat:** the UE mannequin is low-poly, so this measures Nanite skinning's fixed per-character cost; the dense color-region meshes (the 278k–1.35M-tri torso) are where its automatic LOD should matter most, and are the next run once skinned. **Direction:** Nanite skinned for the merged character body; see [Rendering.md](Rendering.md) → "Character composition".
- **⚠ Nanite skinned meshes have no vertex colors in 5.8.** The Nanite build for skeletal meshes never copies them: the two lines are commented out under `// TODO: Nanite-Skinning` in `SkeletalMeshBuilder.cpp` (`InputMeshData.Vertices.Color`), and `NaniteBuilder.cpp` only stores colors when that array is filled. Every face then decodes as the same region, so the Nanite mannequins showed a single color. Static Nanite meshes keep their colors (`StaticMeshBuilder.cpp` fills them), and UV channels are copied for skinned Nanite too. Lines 707–708 and 777 of the local 5.8.4 source. **Decision (2026-11-14): wait for 5.9**, whose dev branch copies the colors, instead of patching the engine or moving region data to a UV channel (that option stays in [Rendering.md](Rendering.md) → "Color regions" if 5.9 slips). Until then, Nanite skinned test meshes show one region; static Nanite pieces are unaffected. The performance numbers above stand: with one region instead of four, the material and geometry cost the same.

**Socketed static meshes and instanced crowds (planned, 2026-11-13).** Rigid pieces are easier to author than perfect seams on a skinned mesh, and an instanced version is the route to large AI groups in the distance. Same scene and route as the Nanite skinned run (100 characters, VR, shadows on); the mannequin split into about 19 rigid pieces (twist bones merged into their limb, fingers into the hand), each a static Nanite mesh:
1. **Hidden skeleton + socketed pieces:** per character, a skeletal mesh component with Visible and Cast Shadow off and *Always Tick Pose and Refresh Bones* (so it keeps animating hidden), plus the pieces attached to their bones. Removes skeletal rendering; adds ~1,900 moving primitives (transform update on the game thread and GPU scene upload each frame). Answers whether pieces beat Nanite skinning in practice.
2. **No skeletal meshes at all (instanced):** the idle animation's bone transforms baked once into a data asset; one C++ manager plays them back by batch-updating one instanced static mesh (Nanite) per piece type, about 19 components for all 100 characters. Removes skeletal components and the per-component cost. This is the shape of the distant-crowd LOD (see "Bodies as baked-animation static meshes" above), and the floor for a scene without skeletal meshes.

Compare against the skinned and Nanite skinned rows above: game thread (per-character and per-component cost), render thread, GPU scene, VSM shadow depths, frames over 12.5 ms. Static pieces need the static vertex color encoding (MeshAuthoring.md).

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
