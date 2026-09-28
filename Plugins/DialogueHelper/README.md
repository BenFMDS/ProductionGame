# DialogueHelper

Open **Window > DialogueHelper** (or the DialogueHelper toolbar button).

## Everyday workflow

1. Select an NPC or pickup in the level, or use the object list. With no search the list shows dialogue, quest-giver and objective actors; searching includes other editable actors.
2. Choose a dialogue or quest asset in the left panel. Choosing an asset opens it for editing; the explicit assignment buttons apply it to the placed object.
3. Use **Dialogue** to edit speaker/portrait, quest-state greetings, dialogue lines, voices and responses. Response destinations use a line dropdown. Renaming a line updates all response and greeting links.
4. Use **Quests** to edit the quest ID, title, description, objective text, type, amount, progress event and icon.
5. Use the simulator to click responses, audition voice clips, and set quest states. **Restart conversation** chooses the greeting for the current simulated state. **Reset simulated quest progress** returns to unaccepted quests.
6. **Validate** checks selected assets and object setup, including duplicate quest IDs across assets. **Save changes...** opens Unreal's normal save flow for dirty content and levels, including other unsaved editor work.

## Assignments and creation

- Assignments affect the selected placed actor. **Open Blueprint defaults** opens its Blueprint for class-wide changes.
- Dialogue assets are shared. **Make unique copy and assign...** duplicates the current dialogue before assigning the copy to the selected actor.
- **Add dialogue / quest-giver support** adds missing instance components and enables actor replication. This does not build a Blueprint interaction graph. Input must still reach `TalkToNPC`.
- Character assignments update both `NativeDialogueDefinition` / `NativeQuestDefinitions` and the components, so `BeginPlay` does not overwrite the changes.
- **New quest...** creates a quest asset. **Create starter dialogue for quest...** creates offer, active, ready and completed greetings, accept/turn-in responses, and exit responses for the chosen quest. With an actor selected it adds missing dialogue/giver support and assigns both assets.
- A pickup's related quests are found by matching `ProgressEvent`. **Link existing pickup to selected quest** changes the existing objective component's event. It does not add objective behavior to an arbitrary actor or alter pickup Blueprint wiring.
- Standard Unreal Undo/Redo applies to edits and assignments. Creating an asset uses Unreal's native asset creation flow.

## Preview boundaries

The simulator does not run gameplay or modify player progress. It assumes quests were accepted from the selected actor and filters quest actions against that actor's offered quests. Each quest's state can be set independently. Voice playback is manual.

The current runtime has one objective per quest and no authored rewards/prerequisite fields. Objective types identify the expected gameplay event; choosing a type does not implement that gameplay. Interaction input, collision, range and multiplayer still need Play-mode testing. The editor is disabled during Play/Simulate.

Delete-line leaves incoming links intact so validation can flag them for deliberate replacement. Undo restores the deleted line. Missing voices are allowed for text-only conversations.

## Verification

Build `ProductionGameEditor Win64 Development`. Run Unreal automation group `ProductionGame.DialogueHelper` for character assignment synchronization, link rename/validation with undo/redo, and Slate window construction.

The plugin is editor-only and depends on this project's `ProductionGame` data types. It is not a generic plugin for unrelated projects.
