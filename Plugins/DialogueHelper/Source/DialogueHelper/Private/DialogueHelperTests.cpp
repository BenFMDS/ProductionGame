#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DialogueAuthoring.h"
#include "SDialogueManager.h"
#include "Dialogue/QuestDialogueCharacter.h"
#include "Dialogue/DialogueComponent.h"
#include "Quest/QuestGiverComponent.h"
#include "Engine/World.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueHelperLinksTest, "ProductionGame.DialogueHelper.RenameAndValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDialogueHelperLinksTest::RunTest(const FString&)
{
    auto* Dialogue = NewObject<UDialogueDefinition>(GetTransientPackage(), NAME_None, RF_Transactional);
    auto& Start = Dialogue->Nodes.AddDefaulted_GetRef(); Start.ID = TEXT("Start"); Start.Text = FText::FromString(TEXT("Hello"));
    auto& Choice = Start.Choices.AddDefaulted_GetRef(); Choice.Text = FText::FromString(TEXT("Continue")); Choice.NextNode = TEXT("End");
    auto& End = Dialogue->Nodes.AddDefaulted_GetRef(); End.ID = TEXT("End"); End.Text = FText::FromString(TEXT("Goodbye"));
    Dialogue->EntryNode = TEXT("Start"); Dialogue->ActiveEntryNode = TEXT("End");
    Dialogue->ContextQuest = NewObject<UQuestDefinition>();
    TestTrue(TEXT("Valid graph has no issues"), DialogueAuthoring::Validate(Dialogue, nullptr, nullptr).IsEmpty());
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("Test rename")));
        TestTrue(TEXT("Rename succeeds"), DialogueAuthoring::RenameNode(Dialogue, TEXT("End"), TEXT("Finish")));
    }
    TestEqual(TEXT("Response destination follows rename"), Dialogue->Nodes[0].Choices[0].NextNode, FName(TEXT("Finish")));
    TestEqual(TEXT("Quest greeting follows rename"), Dialogue->ActiveEntryNode, FName(TEXT("Finish")));
    GEditor->UndoTransaction();
    TestEqual(TEXT("Undo restores response destination"), Dialogue->Nodes[0].Choices[0].NextNode, FName(TEXT("End")));
    GEditor->RedoTransaction();
    TestEqual(TEXT("Redo restores rename"), Dialogue->ActiveEntryNode, FName(TEXT("Finish")));
    TestFalse(TEXT("Duplicate IDs rejected"), DialogueAuthoring::RenameNode(Dialogue, TEXT("Finish"), TEXT("Start")));
    TestFalse(TEXT("None ID rejected"), DialogueAuthoring::RenameNode(Dialogue, TEXT("Finish"), NAME_None));
    Dialogue->Nodes[0].Choices[0].NextNode = TEXT("Missing");
    auto Issues = DialogueAuthoring::Validate(Dialogue, nullptr, nullptr);
    TestTrue(TEXT("Broken links detected"), Issues.ContainsByPredicate([](const FString& Issue) { return Issue.Contains(TEXT("missing line")); }));
    Dialogue->Nodes[0].Choices[0].NextNode = TEXT("Start");
    Dialogue->ActiveEntryNode = NAME_None;
    Issues = DialogueAuthoring::Validate(Dialogue, nullptr, nullptr);
    TestTrue(TEXT("Cycle traversal terminates and detects unreachable node"), Issues.ContainsByPredicate([](const FString& Issue) { return Issue.Contains(TEXT("Unreachable")); }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueHelperAssignmentTest, "ProductionGame.DialogueHelper.CharacterAssignments", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDialogueHelperAssignmentTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
    auto* Character = World->SpawnActor<AQuestDialogueCharacter>();
    auto* Dialogue = NewObject<UDialogueDefinition>();
    auto* Quest = NewObject<UQuestDefinition>();
    Character->NativeDialogueDefinition = nullptr;
    Character->NativeDialogue->Dialogue = Dialogue;
    TestEqual(TEXT("Component asset is the dialogue source of truth"), DialogueAuthoring::GetDialogue(Character), Dialogue);
    TestTrue(TEXT("Assignment accepted"), DialogueAuthoring::AssignDialogue(Character, Dialogue));
    TestTrue(TEXT("Runtime override matches component"), Character->NativeDialogueDefinition == Dialogue && Character->NativeDialogue->Dialogue == Dialogue);
    TestTrue(TEXT("Quest offered"), DialogueAuthoring::SetQuestOffered(Character, Quest, true));
    DialogueAuthoring::SetQuestOffered(Character, Quest, true);
    TestTrue(TEXT("Talker stores offered quest"), Character->NativeDialogue->OfferedQuests.Contains(Quest));
    TestEqual(TEXT("Quest not duplicated"), Character->NativeQuestDefinitions.Num(), 1);
    TestTrue(TEXT("Giver and character fields agree"), Character->NativeQuestGiver->AvailableQuests.Contains(Quest));
    DialogueAuthoring::SetQuestOffered(Character, Quest, false);
    TestTrue(TEXT("Quest removed from all compatibility fields"), Character->NativeDialogue->OfferedQuests.IsEmpty() && Character->NativeQuestDefinitions.IsEmpty() && Character->NativeQuestGiver->AvailableQuests.IsEmpty());
    DialogueAuthoring::AssignDialogue(Character, nullptr);
    TestTrue(TEXT("Clear removes runtime override"), !Character->NativeDialogueDefinition && !Character->NativeDialogue->Dialogue);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueHelperWindowTest, "ProductionGame.DialogueHelper.WindowConstruction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDialogueHelperWindowTest::RunTest(const FString&)
{
    auto* Menu = UToolMenus::Get()->FindMenu(TEXT("LevelEditor.MainMenu.Window"));
    auto* Section = Menu ? Menu->FindSection(TEXT("WindowLayout")) : nullptr;
    TestTrue(TEXT("Unique DialogueHelper entry survives other plugins' menu registration"), Section && Section->FindEntry(TEXT("OpenDialogueHelperWindow")));
    TSharedRef<SDialogueManager> Window = SNew(SDialogueManager);
    Window->SlatePrepass();
    TestTrue(TEXT("Window builds a visible layout"), Window->GetDesiredSize().X > 0.f);
    return true;
}
#endif
