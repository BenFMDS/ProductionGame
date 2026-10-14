#include "SDialogueManager.h"
#include "DialogueAuthoring.h"
#include "Dialogue/DialogueComponent.h"
#include "Quest/QuestGiverComponent.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "Engine/Blueprint.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "PropertyEditorDelegates.h"
#include "PropertyCustomizationHelpers.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/DataAssetFactory.h"
#include "FileHelpers.h"
#include "ScopedTransaction.h"
#include "Sound/SoundBase.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"

namespace
{
FText Txt(const FString& Value) { return FText::FromString(Value); }
TSharedRef<SWidget> Label(const FString& Value)
{
    return SNew(STextBlock).Text(Txt(Value)).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseForeground());
}
TSharedRef<SWidget> SectionTitle(const FString& Title)
{
    return SNew(STextBlock).Text(Txt(Title)).Font(FAppStyle::GetFontStyle("BoldFont"));
}
TSharedRef<SWidget> Button(const FString& Name, TFunction<void()> Action, bool bPrimary = false)
{
    return SNew(SButton)
        .ButtonStyle(bPrimary ? &FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton") : &FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
        .ContentPadding(FMargin(9, 5))
        .Text(Txt(Name))
        .OnClicked_Lambda([Action] { Action(); return FReply::Handled(); });
}
FName UniqueNodeID(const UDialogueDefinition& Dialogue)
{
    for (int32 Index = 1; ; ++Index)
    {
        FName Candidate(*FString::Printf(TEXT("Line_%03d"), Index));
        if (!Dialogue.FindNode(Candidate)) return Candidate;
    }
}
}

void SDialogueManager::Construct(const FArguments& Args)
{
    auto& Properties = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs DetailArgs;
    DetailArgs.bHideSelectionTip = true;
    DetailArgs.bAllowSearch = true;
    DetailArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    DialogueDetails = Properties.CreateDetailView(DetailArgs);
    QuestDetails = Properties.CreateDetailView(DetailArgs);
    ObjectDetails = Properties.CreateDetailView(DetailArgs);
    DialogueDetails->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([this](const FPropertyAndParent& Property)
    {
        const FName Name = Property.Property.GetFName();
        if (Name == TEXT("Nodes")) return false;
        const bool bQuestSetting = Name == TEXT("ContextQuest") || Name == TEXT("ActiveEntryNode") || Name == TEXT("ReadyEntryNode") || Name == TEXT("CompletedEntryNode") || Name == TEXT("bIncludeQuestChoices");
        return !bQuestSetting || !Dialogue.IsValid() || Dialogue->UsesQuests();
    }));
    ObjectDetails->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& Property)
    {
        const FName Name = Property.Property.GetFName();
        return Name == TEXT("InteractionDistance") || Name == TEXT("IdleTimeout") || Name == TEXT("ProgressEvent") || Name == TEXT("Amount");
    }));
    auto Changed = [this](const FPropertyChangedEvent&) { bRefresh = true; };
    DialogueDetails->OnFinishedChangingProperties().AddLambda(Changed);
    QuestDetails->OnFinishedChangingProperties().AddLambda(Changed);
    ObjectDetails->OnFinishedChangingProperties().AddLambda(Changed);
    SelectionHandle = USelection::SelectionChangedEvent.AddSP(this, &SDialogueManager::SelectionChanged);
    GEditor->RegisterForUndo(this);

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 2)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(STextBlock).Text(Txt(TEXT("Dialogue Helper"))).Font(FAppStyle::GetFontStyle("HeadingSmall"))
            ]
            + SHorizontalBox::Slot().FillWidth(1)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(STextBlock).Text_Lambda([this] { return Txt(CanEdit() ? TEXT("Editing") : TEXT("Stop Play to edit")); }).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(8, 2)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Refresh"), [this] { Refresh(); })]
            + SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[Button(TEXT("Save"), [this] { Save(); }, true)]
            + SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[Button(TEXT("Validate"), [this] { Validate(); })]
            + SHorizontalBox::Slot().FillWidth(1)
            + SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
            [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged_Lambda([this](ECheckBoxState Value) { bFollowSelection = Value == ECheckBoxState::Checked; if (bFollowSelection) SelectionChanged(nullptr); })[Label(TEXT("Follow level selection"))] ]
        ]
        + SVerticalBox::Slot().FillHeight(1).Padding(8, 4)
        [
            SNew(SSplitter).IsEnabled_Lambda([this] { return CanEdit(); })
            + SSplitter::Slot().Value(.22f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()[SectionTitle(TEXT("OBJECTS"))]
                + SScrollBox::Slot().Padding(0, 4)[SNew(SSearchBox).HintText(Txt(TEXT("Search objects"))).OnTextChanged_Lambda([this](const FText& Value) { ObjectSearch = Value.ToString(); RefreshObjects(); })]
                + SScrollBox::Slot()[SNew(SBox).MaxDesiredHeight(220)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(ObjectList, SVerticalBox)]]]
                + SScrollBox::Slot().Padding(0, 8)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Txt(Actor.IsValid() ? TEXT("Placed object: ") + Actor->GetActorLabel() : TEXT("No object selected. You can still edit assets.")); })]
                + SScrollBox::Slot().Padding(0, 4)[Label(TEXT("This object uses one Talker Component. It owns its dialogue and optional offered quests."))]
                + SScrollBox::Slot()[Button(TEXT("Add Talker Component"), [this] { AddSupport(); })]
                + SScrollBox::Slot().Padding(0, 8)[Label(TEXT("Dialogue Asset"))]
                + SScrollBox::Slot()[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { auto* Assigned = DialogueAuthoring::GetDialogue(Actor.Get()); return Txt(TEXT("Object currently uses: ") + (Assigned ? Assigned->GetName() : TEXT("None"))); })]
                + SScrollBox::Slot()[SNew(SObjectPropertyEntryBox).AllowedClass(UDialogueDefinition::StaticClass()).ObjectPath_Lambda([this] { return Dialogue.IsValid() ? Dialogue->GetPathName() : FString(); }).OnObjectChanged_Lambda([this](const FAssetData& Asset) { SelectDialogue(Cast<UDialogueDefinition>(Asset.GetAsset())); })]
                + SScrollBox::Slot()[Button(TEXT("Apply Dialogue Asset"), [this] { AssignDialogue(); })]
                + SScrollBox::Slot()[Button(TEXT("New dialogue..."), [this] { CreateAsset(false); })]
                + SScrollBox::Slot()[Button(TEXT("New quest dialogue..."), [this] { CreateAsset(false, true); })]
                + SScrollBox::Slot().Padding(0, 8)[Label(TEXT("Offered Quest (optional)"))]
                + SScrollBox::Slot()[SNew(SObjectPropertyEntryBox).AllowedClass(UQuestDefinition::StaticClass()).ObjectPath_Lambda([this] { return Quest.IsValid() ? Quest->GetPathName() : FString(); }).OnObjectChanged_Lambda([this](const FAssetData& Asset) { SelectQuest(Cast<UQuestDefinition>(Asset.GetAsset())); })]
                + SScrollBox::Slot()[Button(TEXT("New quest..."), [this] { CreateAsset(true); })]
                + SScrollBox::Slot()[Button(TEXT("Add selected quest"), [this] { OfferQuest(true); })]
                + SScrollBox::Slot()[Button(TEXT("Remove selected quest"), [this] { OfferQuest(false); })]
                + SScrollBox::Slot().Padding(0, 6)[Label(TEXT("Current offered quests"))]
                + SScrollBox::Slot()[SAssignNew(Offers, SVerticalBox)]
            ]
            + SSplitter::Slot().Value(.48f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()[Button(TEXT("Dialogue"), [this] { bQuestTab = false; }, true)]
                    + SHorizontalBox::Slot().Padding(4, 0, 0, 0)[Button(TEXT("Quests"), [this] { bQuestTab = true; })]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(4)
                [SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]
                {
                    UObject* Asset = bQuestTab ? static_cast<UObject*>(Quest.Get()) : Dialogue.Get();
                    return Txt(Asset ? FString::Printf(TEXT("%s%s\nShared asset: edits affect every object using it."), *Asset->GetName(), Asset->GetOutermost()->IsDirty() ? TEXT(" * unsaved") : TEXT("")) : TEXT("Choose or create an asset on the left."));
                })]
                + SVerticalBox::Slot().FillHeight(1)
                [
                    SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return bQuestTab ? 1 : 0; })
                    + SWidgetSwitcher::Slot()
                    [SNew(SScrollBox)
                        + SScrollBox::Slot().Padding(0, 0, 0, 4)[SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()[Button(TEXT("Dialogue only"), [this]
                            {
                                ChangeDialogue([](UDialogueDefinition& D)
                                {
                                    D.DialogueType = EDialogueType::DialogueOnly;
                                });
                            }, Dialogue.IsValid() && Dialogue->DialogueType == EDialogueType::DialogueOnly)]
                            + SHorizontalBox::Slot().Padding(4, 0, 0, 0)[Button(TEXT("Dialogue + quests"), [this]
                            {
                                ChangeDialogue([](UDialogueDefinition& D) { D.DialogueType = EDialogueType::QuestDialogue; });
                            }, Dialogue.IsValid() && Dialogue->DialogueType == EDialogueType::QuestDialogue)]
                        ]
                        + SScrollBox::Slot()[SNew(SBox).HeightOverride(260)[DialogueDetails.ToSharedRef()]]
                        + SScrollBox::Slot().Padding(0, 8, 0, 3)[SectionTitle(TEXT("CONVERSATION LINES"))]
                        + SScrollBox::Slot()[SNew(SSearchBox).HintText(Txt(TEXT("Search line ID or text"))).OnTextChanged_Lambda([this](const FText& Value) { LineSearch = Value.ToString(); RefreshLines(); })]
                        + SScrollBox::Slot()[SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()[Button(TEXT("Add line"), [this] { AddNode(false); })]
                            + SHorizontalBox::Slot()[Button(TEXT("Duplicate line"), [this] { AddNode(true); })]
                            + SHorizontalBox::Slot()[Button(TEXT("Delete line"), [this] { DeleteNode(); })]]
                        + SScrollBox::Slot()[SNew(SBox).MaxDesiredHeight(180)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(Lines, SVerticalBox)]]]
                        + SScrollBox::Slot().Padding(0, 8)[SAssignNew(LineEditor, SVerticalBox)]
                    ]
                    + SWidgetSwitcher::Slot()
                    [SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(4)[Label(TEXT("Each quest has one objective. Match its Progress Event to the participating object's event. Rewards and prerequisites are not implemented in the current runtime."))]
                        + SVerticalBox::Slot()[QuestDetails.ToSharedRef()]
                    ]
                ]
            ]
            + SSplitter::Slot().Value(.30f)
            [SNew(SScrollBox)
                + SScrollBox::Slot()[SectionTitle(TEXT("SIMULATOR"))]
                + SScrollBox::Slot()[Button(TEXT("Restart conversation"), [this] { RestartPreview(); })]
                + SScrollBox::Slot()[Button(TEXT("Reset simulated quest progress"), [this] { SimulatedStates.Reset(); RestartPreview(); })]
                + SScrollBox::Slot().Padding(0, 8)[SAssignNew(Preview, SVerticalBox)]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(8, 4, 8, 8)
        [SNew(SBox).MaxDesiredHeight(140)[SNew(SScrollBox) + SScrollBox::Slot()[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Txt(Status); })]]]
    ];
    SelectionChanged(nullptr);
    Refresh();
}

SDialogueManager::~SDialogueManager()
{
    USelection::SelectionChangedEvent.Remove(SelectionHandle);
    if (GEditor) { GEditor->UnregisterForUndo(this); GEditor->ResetPreviewAudioComponent(); }
}
bool SDialogueManager::CanEdit() const { return GEditor && !GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor; }
void SDialogueManager::Tick(const FGeometry& Geometry, double Time, float Delta)
{
    SCompoundWidget::Tick(Geometry, Time, Delta);
    if (bRefresh) { bRefresh = false; Refresh(); }
}
void SDialogueManager::PostUndo(bool) { bRefresh = true; }
void SDialogueManager::SelectionChanged(UObject*)
{
    if (bFollowSelection && CanEdit()) SelectActor(Cast<AActor>(GEditor->GetSelectedActors()->GetTop(AActor::StaticClass())));
}
void SDialogueManager::SelectActor(AActor* InActor)
{
    if (InActor && InActor->GetWorld() != GEditor->GetEditorWorldContext().World()) return;
    Actor = InActor;
    SimulatedStates.Reset();
    SelectDialogue(DialogueAuthoring::GetDialogue(InActor));
    auto Offered = DialogueAuthoring::GetQuests(InActor);
    SelectQuest(Offered.IsEmpty() ? nullptr : Offered[0]);
    bRefresh = true;
}
void SDialogueManager::SelectDialogue(UDialogueDefinition* InDialogue)
{
    GEditor->ResetPreviewAudioComponent();
    Dialogue.Reset(InDialogue);
    SelectedNode = InDialogue && !InDialogue->Nodes.IsEmpty() ? InDialogue->Nodes[0].ID : NAME_None;
    DialogueDetails->SetObject(InDialogue);
    RestartPreview();
    bRefresh = true;
}
void SDialogueManager::SelectQuest(UQuestDefinition* InQuest)
{
    Quest.Reset(InQuest);
    QuestDetails->SetObject(InQuest);
    bRefresh = true;
}
void SDialogueManager::Refresh()
{
    RefreshObjects(); RefreshLines(); RefreshLineEditor(); RefreshPreview();
    Offers->ClearChildren();
    for (auto* Offered : DialogueAuthoring::GetQuests(Actor.Get()))
    {
        TWeakObjectPtr<UQuestDefinition> WeakQuest(Offered);
        Offers->AddSlot().AutoHeight()[Button(Offered->QuestName.IsEmpty() ? Offered->GetName() : Offered->QuestName.ToString(), [this, WeakQuest] { SelectQuest(WeakQuest.Get()); bQuestTab = true; })];
    }
    if (Actor.IsValid()) if (auto* Objective = Actor->FindComponentByClass<UQuestObjectiveComponent>())
    {
        auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
        TArray<FAssetData> Assets;
        Registry.GetAssetsByClass(UQuestDefinition::StaticClass()->GetClassPathName(), Assets, true);
        for (const auto& Asset : Assets)
        {
            auto* Related = Cast<UQuestDefinition>(Asset.GetAsset());
            if (!Related || Related->ProgressEvent.IsNone() || Related->ProgressEvent != Objective->ProgressEvent) continue;
            TWeakObjectPtr<UQuestDefinition> WeakQuest(Related);
            Offers->AddSlot().AutoHeight()[Button(TEXT("Pickup: ") + Related->QuestName.ToString(), [this, WeakQuest] { SelectQuest(WeakQuest.Get()); bQuestTab = true; })];
        }
    }
    TArray<UObject*> Settings;
    if (Actor.IsValid())
    {
        if (auto* Component = Actor->FindComponentByClass<UDialogueComponent>()) Settings.Add(Component);
        if (auto* Component = Actor->FindComponentByClass<UQuestObjectiveComponent>()) Settings.Add(Component);
    }
    // Different component classes need separate property trees; show pickup settings when applicable.
    ObjectDetails->SetObject(Settings.IsEmpty() ? nullptr : Settings.Last());
}
void SDialogueManager::RefreshObjects()
{
    ObjectList->ClearChildren();
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return;
    TArray<AActor*> Actors;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (!It->IsEditable() || !It->IsListedInSceneOutliner()) continue;
        bool bRelevant = It->FindComponentByClass<UDialogueComponent>() || It->FindComponentByClass<UQuestGiverComponent>() || It->FindComponentByClass<UQuestObjectiveComponent>();
        if ((!ObjectSearch.IsEmpty() || bRelevant) && It->GetActorLabel().Contains(ObjectSearch)) Actors.Add(*It);
    }
    Actors.Sort([](const AActor& A, const AActor& B) { return A.GetActorLabel() < B.GetActorLabel(); });
    for (AActor* Item : Actors)
    {
        TWeakObjectPtr<AActor> WeakActor(Item);
        ObjectList->AddSlot().AutoHeight()[Button(Item->GetActorLabel(), [this, WeakActor]
        {
            if (WeakActor.IsValid())
            {
                GEditor->SelectNone(false, true);
                GEditor->SelectActor(WeakActor.Get(), true, true);
                SelectActor(WeakActor.Get());
            }
        })];
    }
}
FDialogueNode* SDialogueManager::CurrentNode()
{
    return Dialogue.IsValid() ? Dialogue->Nodes.FindByPredicate([this](const FDialogueNode& Node) { return Node.ID == SelectedNode; }) : nullptr;
}
void SDialogueManager::ChangeDialogue(TFunctionRef<void(UDialogueDefinition&)> Change)
{
    if (!CanEdit() || !Dialogue.IsValid()) return;
    const FScopedTransaction Transaction(Txt(TEXT("Edit dialogue")));
    Dialogue->Modify();
    Change(*Dialogue);
    Dialogue->PostEditChange();
    Dialogue->MarkPackageDirty();
    DialogueDetails->ForceRefresh();
    bRefresh = true;
}
void SDialogueManager::RefreshLines()
{
    Lines->ClearChildren();
    if (!Dialogue.IsValid()) return;
    for (const auto& Node : Dialogue->Nodes)
    {
        if (!LineSearch.IsEmpty() && !Node.ID.ToString().Contains(LineSearch) && !Node.Text.ToString().Contains(LineSearch)) continue;
        FString Caption = (Node.ID == SelectedNode ? TEXT("> ") : TEXT("")) + Node.ID.ToString();
        if (Node.ID == Dialogue->EntryNode) Caption += TEXT(" [START]");
        Caption += TEXT("  ") + Node.Text.ToString().Left(65).Replace(TEXT("\n"), TEXT(" "));
        Lines->AddSlot().AutoHeight().Padding(0, 1)[Button(Caption, [this, ID = Node.ID] { SelectedNode = ID; bRefresh = true; }, Node.ID == SelectedNode)];
    }
}
TSharedRef<SWidget> SDialogueManager::NodePicker(FName Value, TFunction<void(FName)> OnPicked, bool bAllowEnd)
{
    return SNew(SComboButton).ButtonContent()[Label(Value.IsNone() ? TEXT("End conversation") : Value.ToString())]
        .OnGetMenuContent_Lambda([this, OnPicked, bAllowEnd]
        {
            FMenuBuilder Menu(true, nullptr);
            if (bAllowEnd) Menu.AddMenuEntry(Txt(TEXT("End conversation")), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([OnPicked] { OnPicked(NAME_None); })));
            if (Dialogue.IsValid()) for (const auto& Node : Dialogue->Nodes)
                Menu.AddMenuEntry(Txt(Node.ID.ToString()), Node.Text, FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([OnPicked, ID = Node.ID] { OnPicked(ID); })));
            return Menu.MakeWidget();
        });
}
void SDialogueManager::RefreshLineEditor()
{
    LineEditor->ClearChildren();
    auto* Node = CurrentNode();
    if (!Node) { LineEditor->AddSlot().AutoHeight()[Label(TEXT("Select or add a line."))]; return; }
    LineEditor->AddSlot().AutoHeight().Padding(0, 8, 0, 3)[SectionTitle(TEXT("LINE EDITOR  •  ") + Node->ID.ToString())];
    LineEditor->AddSlot().AutoHeight()[Label(TEXT("Line ID (renaming updates every link)"))];
    LineEditor->AddSlot().AutoHeight()[SNew(SEditableTextBox).Text(Txt(Node->ID.ToString())).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
    {
        const FName NewID(*Text.ToString().TrimStartAndEnd());
        if (NewID == SelectedNode || !CanEdit()) return;
        const FScopedTransaction Transaction(Txt(TEXT("Rename dialogue line")));
        if (DialogueAuthoring::RenameNode(Dialogue.Get(), SelectedNode, NewID)) { SelectedNode = NewID; RestartPreview(); }
        else Status = TEXT("Line ID must be unique and cannot be None or empty.");
        bRefresh = true;
    })];
    LineEditor->AddSlot().AutoHeight()[Button(TEXT("Use as starting line"), [this] { ChangeDialogue([this](auto& D) { D.EntryNode = SelectedNode; }); })];
    LineEditor->AddSlot().AutoHeight().Padding(0, 6)[SNew(SBox).MinDesiredHeight(100)[SNew(SMultiLineEditableTextBox).HintText(Txt(TEXT("Dialogue text"))).Text(Node->Text).AutoWrapText(true).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { if (auto* N = CurrentNode(); N && !N->Text.EqualTo(Text)) ChangeDialogue([this, Text](auto&) { if (auto* Current = CurrentNode()) Current->Text = Text; }); })]];
    LineEditor->AddSlot().AutoHeight()[Label(TEXT("Voice sound"))];
    LineEditor->AddSlot().AutoHeight()[SNew(SObjectPropertyEntryBox).AllowedClass(USoundBase::StaticClass()).ObjectPath(Node->Voice ? Node->Voice->GetPathName() : FString()).OnObjectChanged_Lambda([this](const FAssetData& Asset) { ChangeDialogue([this, Asset](auto&) { if (auto* N = CurrentNode()) N->Voice = Cast<USoundBase>(Asset.GetAsset()); }); })];
    LineEditor->AddSlot().AutoHeight()[SNew(SHorizontalBox)
        + SHorizontalBox::Slot()[Button(TEXT("Play voice"), [this] { PlayVoice(SelectedNode); })]
        + SHorizontalBox::Slot()[Button(TEXT("Stop"), [] { GEditor->ResetPreviewAudioComponent(); })]];
    LineEditor->AddSlot().AutoHeight().Padding(0, 10, 0, 3)[SectionTitle(TEXT("PLAYER RESPONSES"))];
    for (int32 Index = 0; Index < Node->Choices.Num(); ++Index)
    {
        const auto& Choice = Node->Choices[Index];
        LineEditor->AddSlot().AutoHeight().Padding(0, 5, 0, 2)[SNew(STextBlock).Text(FText::Format(NSLOCTEXT("DialogueHelper", "ResponseNumber", "Response {0}"), FText::AsNumber(Index + 1))).Font(FAppStyle::GetFontStyle("BoldFont"))];
        LineEditor->AddSlot().AutoHeight()[SNew(SMultiLineEditableTextBox).HintText(Txt(TEXT("What can the player say?"))).Text(Choice.Text).AutoWrapText(true).OnTextCommitted_Lambda([this, Index](const FText& Text, ETextCommit::Type)
        { if (auto* N = CurrentNode(); N && N->Choices.IsValidIndex(Index) && !N->Choices[Index].Text.EqualTo(Text)) ChangeDialogue([this, Index, Text](auto&) { if (auto* Current = CurrentNode(); Current && Current->Choices.IsValidIndex(Index)) Current->Choices[Index].Text = Text; }); })];
        LineEditor->AddSlot().AutoHeight()[NodePicker(Choice.NextNode, [this, Index](FName ID)
        { ChangeDialogue([this, Index, ID](auto&) { if (auto* N = CurrentNode(); N && N->Choices.IsValidIndex(Index)) N->Choices[Index].NextNode = ID; }); })];
        if (Dialogue->UsesQuests())
        {
            LineEditor->AddSlot().AutoHeight()[SNew(SComboButton).ButtonContent()[Label(StaticEnum<EDialogueQuestAction>()->GetDisplayNameTextByValue(static_cast<int64>(Choice.QuestAction)).ToString())].OnGetMenuContent_Lambda([this, Index]
            {
                FMenuBuilder Menu(true, nullptr);
                for (auto Action : {EDialogueQuestAction::None, EDialogueQuestAction::Accept, EDialogueQuestAction::TurnIn})
                    Menu.AddMenuEntry(StaticEnum<EDialogueQuestAction>()->GetDisplayNameTextByValue(static_cast<int64>(Action)), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, Index, Action]
                    { ChangeDialogue([this, Index, Action](auto&) { if (auto* N = CurrentNode(); N && N->Choices.IsValidIndex(Index)) N->Choices[Index].QuestAction = Action; }); })));
                return Menu.MakeWidget();
            })];
            LineEditor->AddSlot().AutoHeight()[SNew(SObjectPropertyEntryBox).AllowedClass(UQuestDefinition::StaticClass()).ObjectPath(Choice.Quest ? Choice.Quest->GetPathName() : FString()).OnObjectChanged_Lambda([this, Index](const FAssetData& Asset)
            { ChangeDialogue([this, Index, Asset](auto&) { if (auto* N = CurrentNode(); N && N->Choices.IsValidIndex(Index)) N->Choices[Index].Quest = Cast<UQuestDefinition>(Asset.GetAsset()); }); })];
        }
        LineEditor->AddSlot().AutoHeight()[Button(TEXT("Remove response"), [this, Index] { ChangeDialogue([this, Index](auto&) { if (auto* N = CurrentNode(); N && N->Choices.IsValidIndex(Index)) N->Choices.RemoveAt(Index); }); })];
    }
    LineEditor->AddSlot().AutoHeight().Padding(0, 4)[Button(TEXT("Add response"), [this] { ChangeDialogue([this](auto&) { if (auto* N = CurrentNode()) N->Choices.AddDefaulted(); }); })];
}

void SDialogueManager::AddNode(bool bDuplicate)
{
    ChangeDialogue([this, bDuplicate](UDialogueDefinition& D)
    {
        FDialogueNode NewNode;
        if (bDuplicate) { if (auto* Node = CurrentNode()) NewNode = *Node; else return; }
        NewNode.ID = UniqueNodeID(D);
        D.Nodes.Add(NewNode);
        SelectedNode = NewNode.ID;
        if (D.Nodes.Num() == 1) D.EntryNode = NewNode.ID;
    });
}
void SDialogueManager::DeleteNode()
{
    if (!CurrentNode()) return;
    if (FMessageDialog::Open(EAppMsgType::YesNo, Txt(TEXT("Delete this line? Links to it will remain visible as validation errors so you can choose their replacements. Undo restores the line."))) != EAppReturnType::Yes) return;
    ChangeDialogue([this](auto& D) { D.Nodes.RemoveAll([this](const auto& N) { return N.ID == SelectedNode; }); SelectedNode = D.Nodes.IsEmpty() ? NAME_None : D.Nodes[0].ID; });
}
void SDialogueManager::AssignDialogue()
{
    if (!CanEdit()) return;
    const FScopedTransaction Transaction(Txt(TEXT("Assign dialogue to object")));
    Status = DialogueAuthoring::AssignDialogue(Actor.Get(), Dialogue.Get()) ? TEXT("Dialogue assignment updated on the placed object. Save the level to keep it.") : TEXT("Select an object with a Dialogue Component first.");
    bRefresh = true;
}
void SDialogueManager::OfferQuest(bool bOffer)
{
    if (!CanEdit()) return;
    const FScopedTransaction Transaction(Txt(TEXT("Change offered quest")));
    Status = DialogueAuthoring::SetQuestOffered(Actor.Get(), Quest.Get(), bOffer) ? TEXT("Offered quests updated. Save the level to keep the assignment.") : TEXT("Choose a quest and an object with a Talker Component first.");
    bRefresh = true;
}
void SDialogueManager::AddSupport()
{
    if (!CanEdit() || !Actor.IsValid()) { Status = TEXT("Select a placed object first."); return; }
    const FScopedTransaction Transaction(Txt(TEXT("Add dialogue support")));
    AActor* Target = Actor.Get();
    Target->Modify();
    auto Add = [Target](UClass* Class)
    {
        if (Target->FindComponentByClass(Class)) return;
        auto* Component = NewObject<UActorComponent>(Target, Class, NAME_None, RF_Transactional);
        Target->AddInstanceComponent(Component);
        Component->OnComponentCreated();
        Component->RegisterComponent();
    };
    Add(UDialogueComponent::StaticClass());
    Target->SetReplicates(true);
    Target->MarkPackageDirty();
    Status = TEXT("Added a Talker Component and enabled replication. Assign a Dialogue Asset before testing. Offered quests live on this same component.");
    bRefresh = true;
}
void SDialogueManager::LinkObjective()
{
    if (!CanEdit() || !Actor.IsValid() || !Quest.IsValid()) { Status = TEXT("Select a pickup object and quest first."); return; }
    auto* Objective = Actor->FindComponentByClass<UQuestObjectiveComponent>();
    if (!Objective) { Status = TEXT("This object has no Quest Objective Component. Select an existing quest pickup."); return; }
    if (Quest->ProgressEvent.IsNone()) { Status = TEXT("Set the quest Progress Event before linking a pickup."); return; }
    const FScopedTransaction Transaction(Txt(TEXT("Link quest pickup")));
    Actor->Modify(); Objective->Modify();
    Objective->ProgressEvent = Quest->ProgressEvent;
    Objective->PostEditChange(); Actor->MarkPackageDirty();
    Status = TEXT("Pickup event now matches the selected quest. Other quests using the same event also receive progress.");
    bRefresh = true;
}
void SDialogueManager::CreateAsset(bool bQuest, bool bQuestDialogue)
{
    if (!CanEdit()) return;
    UClass* Class = bQuest ? UQuestDefinition::StaticClass() : UDialogueDefinition::StaticClass();
    auto* Factory = NewObject<UDataAssetFactory>();
    Factory->DataAssetClass = Class;
    auto& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    UObject* Asset = Tools.CreateAssetWithDialog(bQuest ? TEXT("DA_NewQuest") : (bQuestDialogue ? TEXT("DA_NewQuestDialogue") : TEXT("DA_NewDialogue")), TEXT("/Game"), Class, Factory, NAME_None, false);
    if (auto* NewQuest = Cast<UQuestDefinition>(Asset))
    {
        NewQuest->QuestID = NewQuest->GetFName();
        NewQuest->QuestName = Txt(NewQuest->GetName());
        NewQuest->ProgressEvent = FName(*(NewQuest->GetName() + TEXT("_Progress")));
        NewQuest->MarkPackageDirty(); SelectQuest(NewQuest); bQuestTab = true;
    }
    if (auto* NewDialogue = Cast<UDialogueDefinition>(Asset))
    {
        NewDialogue->DialogueType = bQuestDialogue ? EDialogueType::QuestDialogue : EDialogueType::DialogueOnly;
        NewDialogue->bIncludeQuestChoices = bQuestDialogue;
        NewDialogue->MarkPackageDirty();
        SelectDialogue(NewDialogue); AddNode(false); bQuestTab = false;
    }
}
void SDialogueManager::DuplicateDialogue()
{
    if (!CanEdit() || !Dialogue.IsValid()) return;
    auto& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    if (auto* Copy = Cast<UDialogueDefinition>(Tools.DuplicateAssetWithDialog(Dialogue->GetName() + TEXT("_Copy"), FPackageName::GetLongPackagePath(Dialogue->GetOutermost()->GetName()), Dialogue.Get())))
    { SelectDialogue(Copy); if (Actor.IsValid()) AssignDialogue(); }
}
void SDialogueManager::CreateStarterDialogue()
{
    if (!CanEdit() || !Quest.IsValid()) { Status = TEXT("Create or choose a quest first."); return; }
    auto* Factory = NewObject<UDataAssetFactory>(); Factory->DataAssetClass = UDialogueDefinition::StaticClass();
    auto& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    auto* Created = Cast<UDialogueDefinition>(Tools.CreateAssetWithDialog(TEXT("DA_QuestDialogue"), TEXT("/Game"), UDialogueDefinition::StaticClass(), Factory, NAME_None, false));
    if (!Created) return;
    SelectDialogue(Created);
    ChangeDialogue([this](auto& D)
    {
        D.DialogueType = EDialogueType::QuestDialogue;
        D.SpeakerName = Txt(Actor.IsValid() ? Actor->GetActorLabel() : TEXT("Speaker"));
        D.ContextQuest = Quest.Get(); D.bIncludeQuestChoices = false;
        D.EntryNode = TEXT("Offer"); D.ActiveEntryNode = TEXT("Active"); D.ReadyEntryNode = TEXT("Ready"); D.CompletedEntryNode = TEXT("Completed");
        auto Add = [&D](FName ID, const FString& Text) -> FDialogueNode& { auto& N = D.Nodes.AddDefaulted_GetRef(); N.ID = ID; N.Text = Txt(Text); return N; };
        auto& Offer = Add(TEXT("Offer"), TEXT("Can you help me with ") + Quest->QuestName.ToString() + TEXT("?"));
        FDialogueChoice Accept; Accept.Text = Txt(TEXT("I'll help.")); Accept.QuestAction = EDialogueQuestAction::Accept; Accept.Quest = Quest.Get(); Accept.NextNode = TEXT("Active"); Offer.Choices.Add(Accept);
        Add(TEXT("Active"), Quest->ObjectiveText.IsEmpty() ? TEXT("Let me know when you have finished.") : Quest->ObjectiveText.ToString());
        auto& Ready = Add(TEXT("Ready"), TEXT("Have you finished?"));
        FDialogueChoice TurnIn; TurnIn.Text = Txt(TEXT("Yes, it's done.")); TurnIn.QuestAction = EDialogueQuestAction::TurnIn; TurnIn.Quest = Quest.Get(); TurnIn.NextNode = TEXT("Completed"); Ready.Choices.Add(TurnIn);
        Add(TEXT("Completed"), TEXT("Thank you for your help."));
        for (auto& N : D.Nodes) { auto& Exit = N.Choices.AddDefaulted_GetRef(); Exit.Text = Txt(TEXT("Goodbye.")); }
        SelectedNode = D.EntryNode;
    });
    bQuestTab = false;
    if (Actor.IsValid()) { AddSupport(); AssignDialogue(); OfferQuest(true); }
    RestartPreview();
}
void SDialogueManager::Save()
{
    if (!CanEdit()) return;
    // Native save UI includes levels, external actors, and assets edited earlier in this window.
    FEditorFileUtils::SaveDirtyPackages(true, true, true);
}
void SDialogueManager::Validate()
{
    auto Issues = DialogueAuthoring::Validate(Dialogue.Get(), Quest.Get(), Actor.Get());
    if (Quest.IsValid() && !Quest->QuestID.IsNone())
    {
        auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
        TArray<FAssetData> Assets;
        Registry.GetAssetsByClass(UQuestDefinition::StaticClass()->GetClassPathName(), Assets, true);
        for (const auto& Asset : Assets) if (auto* Other = Cast<UQuestDefinition>(Asset.GetAsset()); Other && Other != Quest.Get() && Other->QuestID == Quest->QuestID)
            Issues.Add(TEXT("Duplicate Quest ID in ") + Other->GetPathName());
    }
    Status = Issues.IsEmpty() ? TEXT("No data issues found in the selected dialogue, quest and object. Test interaction wiring and multiplayer in Play mode.") : FString::Join(Issues, TEXT("\n"));
}
void SDialogueManager::PlayVoice(FName NodeID)
{
    GEditor->ResetPreviewAudioComponent();
    if (Dialogue.IsValid()) if (const auto* Node = Dialogue->FindNode(NodeID)) if (Node->Voice) GEditor->PlayPreviewSound(Node->Voice);
}
int32 SDialogueManager::State(UQuestDefinition* InQuest) const
{
    const int32* Found = SimulatedStates.Find(TWeakObjectPtr<UQuestDefinition>(InQuest));
    return Found ? *Found : 0;
}
bool SDialogueManager::CanSimulateAction(UQuestDefinition* InQuest, EDialogueQuestAction Action) const
{
    if (Action == EDialogueQuestAction::None) return true;
    if (!InQuest || InQuest->QuestID.IsNone() || InQuest->RequiredAmount < 1 || !DialogueAuthoring::GetQuests(Actor.Get()).Contains(InQuest)) return false;
    return State(InQuest) == (Action == EDialogueQuestAction::Accept ? 0 : 2);
}
void SDialogueManager::RestartPreview()
{
    PreviewNode = Dialogue.IsValid() ? Dialogue->EntryNode : NAME_None;
    if (Dialogue.IsValid() && Dialogue->UsesQuests() && Dialogue->ContextQuest)
    {
        int32 Value = State(Dialogue->ContextQuest);
        FName Entry = Value == 1 ? Dialogue->ActiveEntryNode : Value == 2 ? Dialogue->ReadyEntryNode : Value == 3 ? Dialogue->CompletedEntryNode : Dialogue->EntryNode;
        if (Dialogue->FindNode(Entry)) PreviewNode = Entry;
    }
    if (Preview.IsValid()) RefreshPreview();
}
void SDialogueManager::SimulateChoice(const FDialogueChoice& Choice)
{
    if (!CanSimulateAction(Choice.Quest, Choice.QuestAction)) return;
    if (Dialogue.IsValid() && Dialogue->UsesQuests() && Choice.QuestAction != EDialogueQuestAction::None) SimulatedStates.Add(Choice.Quest, Choice.QuestAction == EDialogueQuestAction::Accept ? 1 : 3);
    PreviewNode = Choice.NextNode;
    RefreshPreview();
}
void SDialogueManager::RefreshPreview()
{
    Preview->ClearChildren();
    TArray<UQuestDefinition*> SimulationQuests = DialogueAuthoring::GetQuests(Actor.Get());
    if (Quest.IsValid()) SimulationQuests.AddUnique(Quest.Get());
    if (Dialogue.IsValid() && Dialogue->UsesQuests() && Dialogue->ContextQuest) SimulationQuests.AddUnique(Dialogue->ContextQuest);
    const TCHAR* StateNames[] = { TEXT("Not accepted"), TEXT("Active"), TEXT("Ready to turn in"), TEXT("Completed") };
    for (auto* Q : SimulationQuests)
    {
        TWeakObjectPtr<UQuestDefinition> WeakQuest(Q);
        Preview->AddSlot().AutoHeight()[Label(Q->QuestName.ToString())];
        Preview->AddSlot().AutoHeight()[SNew(SComboButton).ButtonContent()[Label(StateNames[State(Q)])].OnGetMenuContent_Lambda([this, WeakQuest]
        {
            FMenuBuilder Menu(true, nullptr);
            const TCHAR* Names[] = { TEXT("Not accepted"), TEXT("Active"), TEXT("Ready to turn in"), TEXT("Completed") };
            for (int32 Index = 0; Index < 4; ++Index) Menu.AddMenuEntry(Txt(Names[Index]), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, WeakQuest, Index]
            { if (WeakQuest.IsValid()) SimulatedStates.Add(WeakQuest, Index); RestartPreview(); })));
            return Menu.MakeWidget();
        })];
    }
    if (!Dialogue.IsValid()) { Preview->AddSlot().AutoHeight()[Label(TEXT("Choose a dialogue asset to preview."))]; return; }
    const auto* Node = Dialogue->FindNode(PreviewNode);
    if (!Node) { Preview->AddSlot().AutoHeight().Padding(0, 12)[Label(PreviewNode.IsNone() ? TEXT("Conversation ended.") : TEXT("Missing line: ") + PreviewNode.ToString())]; return; }
    Preview->AddSlot().AutoHeight().Padding(0, 12)[Label(Dialogue->SpeakerName.ToString() + TEXT("  [") + Node->ID.ToString() + TEXT("]"))];
    Preview->AddSlot().AutoHeight()[Label(Node->Text.ToString())];
    Preview->AddSlot().AutoHeight().Padding(0, 6)[Button(TEXT("Play this line's voice"), [this] { PlayVoice(PreviewNode); })];
    for (const auto& Choice : Node->Choices)
    {
        if (!CanSimulateAction(Choice.Quest, Choice.QuestAction) || (!Choice.NextNode.IsNone() && !Dialogue->FindNode(Choice.NextNode))) continue;
        Preview->AddSlot().AutoHeight().Padding(0, 2)[Button(Choice.Text.ToString(), [this, Choice] { SimulateChoice(Choice); })];
    }
    if (Dialogue->UsesQuests() && Dialogue->bIncludeQuestChoices) for (auto* Q : DialogueAuthoring::GetQuests(Actor.Get()))
    {
        const auto Action = State(Q) == 0 ? EDialogueQuestAction::Accept : EDialogueQuestAction::TurnIn;
        if (!CanSimulateAction(Q, Action)) continue;
        FDialogueChoice Choice; Choice.Quest = Q; Choice.QuestAction = Action; Choice.NextNode = PreviewNode;
        Preview->AddSlot().AutoHeight()[Button((Action == EDialogueQuestAction::Accept ? TEXT("Accept: ") : TEXT("Complete: ")) + Q->QuestName.ToString(), [this, Choice] { SimulateChoice(Choice); })];
    }
    Preview->AddSlot().AutoHeight().Padding(0, 6)[Button(TEXT("Close conversation"), [this] { PreviewNode = NAME_None; GEditor->ResetPreviewAudioComponent(); RefreshPreview(); })];
}
