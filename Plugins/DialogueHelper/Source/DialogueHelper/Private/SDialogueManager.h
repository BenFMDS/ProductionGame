#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "EditorUndoClient.h"
#include "UObject/StrongObjectPtr.h"
#include "Dialogue/DialogueDefinition.h"
#include "Quest/QuestDefinition.h"

class IDetailsView;
class SVerticalBox;
class AActor;

class SDialogueManager : public SCompoundWidget, public FEditorUndoClient
{
public:
    SLATE_BEGIN_ARGS(SDialogueManager) {} SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual ~SDialogueManager() override;
    virtual void Tick(const FGeometry&, double, float) override;
    virtual void PostUndo(bool bSuccess) override;
    virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
private:
    TWeakObjectPtr<AActor> Actor;
    TStrongObjectPtr<UDialogueDefinition> Dialogue;
    TStrongObjectPtr<UQuestDefinition> Quest;
    TSharedPtr<IDetailsView> DialogueDetails, QuestDetails, ObjectDetails;
    TSharedPtr<SVerticalBox> ObjectList, Lines, LineEditor, Preview, Offers;
    FDelegateHandle SelectionHandle;
    FName SelectedNode, PreviewNode;
    FString ObjectSearch, LineSearch, Status;
    TMap<TWeakObjectPtr<UQuestDefinition>, int32> SimulatedStates;
    bool bRefresh = false;
    bool bQuestTab = false;
    bool bFollowSelection = true;
    bool CanEdit() const;
    void SelectionChanged(UObject*);
    void SelectActor(AActor* InActor);
    void SelectDialogue(UDialogueDefinition* InDialogue);
    void SelectQuest(UQuestDefinition* InQuest);
    void Refresh();
    void RefreshObjects();
    void RefreshLines();
    void RefreshLineEditor();
    void RefreshPreview();
    void RestartPreview();
    void ChangeDialogue(TFunctionRef<void(UDialogueDefinition&)> Change);
    void AddNode(bool bDuplicate);
    void DeleteNode();
    void AssignDialogue();
    void OfferQuest(bool bOffer);
    void AddSupport();
    void LinkObjective();
    void CreateAsset(bool bQuest, bool bQuestDialogue = false);
    void DuplicateDialogue();
    void CreateStarterDialogue();
    void Save();
    void Validate();
    void PlayVoice(FName NodeID);
    TSharedRef<SWidget> NodePicker(FName Value, TFunction<void(FName)> OnPicked, bool bAllowEnd = true);
    FDialogueNode* CurrentNode();
    int32 State(UQuestDefinition* InQuest) const;
    bool CanSimulateAction(UQuestDefinition* InQuest, EDialogueQuestAction Action) const;
    void SimulateChoice(const FDialogueChoice& Choice);
};
