# Vision

## Theme

A VR take on the **Call of Duty MW2 DMZ** feel:
- A **large map** to explore.
- **Static and dynamic events** across the map.
- The **threat of other players**.
- The **threat of AI** at varying levels of sophistication, in groups from small to large.
- **Extraction is necessary** to end a match successfully. That's kept from extraction shooters; the "loot limited resources and escape with them" loop is not.

## Pillars

1. **Performance first.** Performance is a design constraint from day one, not a late optimization pass. VR punishes missed frames with discomfort, not just choppiness. Every system is weighed against the frame budget before it's built.
2. **A living map.** Density of AI and events is what made DMZ feel alive. The technical goal is the most AI and world activity the hardware allows.
3. **Player-hosted multiplayer.** No dedicated server dependency (see [Networking.md](Networking.md)).

## Open question: why play?

DMZ's purpose was essentially "make money and play." That's not enough here, and the extraction-shooter loot loop isn't the answer either. The reason to drop in still needs defining, and it should make extraction necessary without making loot hoarding the point.

Directions to consider (not decided):
- **Objective-driven drops:** each match has a goal (intel, a rescue, sabotage, a high-value target), and extraction is how it counts.
- **Persistent faction/territory progress:** what squads do in a match shifts a shared map state (control, threat levels, unlocked areas) between sessions.
- **Information as the reward:** what you bring out is knowledge (map intel, enemy plans) that changes future matches, rather than gear.
- **Progression through capability, not inventory:** extraction unlocks skills, kit options or squad assets permanently, so death costs the run's progress but not a hoard.

## Immediate focus

Prove VR viability before building gameplay: profile the textureless single-material pipeline to find how many AI and static meshes a large map can hold in VR, and strip the engine of everything unneeded. See [Engine.md](Engine.md).
