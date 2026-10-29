#include "Quest/QuestConversationWidget.h"
#include "Quest/PlayerQuestComponent.h"
#include "Quest/QuestDefinition.h"
#include "Robber/RobberGameState.h"
#include "Robber/RobberController.h"
#include "Heist/RobberEntryPoint.h"
#include "Heist/RobberStage.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

UConversationChoiceWidget::UConversationChoiceWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	static ConstructorHelpers::FClassFinder<UUserWidget> Original(TEXT("/Game/OurStuff/Core/Dialogue/WBP_TextEntry"));
	LayoutClass = Original.Class;
}

TSharedRef<SWidget> UConversationChoiceWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget && LayoutClass)
	{
		Layout = CreateWidget<UUserWidget>(GetOwningPlayer(), LayoutClass);
		WidgetTree->RootWidget = Layout;
	}
	return Super::RebuildWidget();
}

void UConversationChoiceWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!Layout) return;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Layout->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (auto* Text = Cast<UTextBlock>(Layout->GetWidgetFromName(TEXT("TXT_ChoiceText")))) Text->SetText(Label);
	Button = Cast<UButton>(Layout->GetWidgetFromName(TEXT("BTN_Button")));
	if (Button)
	{
		Button->SetVisibility(ESlateVisibility::Visible);
		Button->SetIsEnabled(true);
		Button->OnClicked.AddUniqueDynamic(this, &UConversationChoiceWidget::Clicked);
	}
}

void UConversationChoiceWidget::Clicked()
{
	if (!Player.IsValid()) return;
	if (ChoiceID == INDEX_NONE) Player->ServerEndConversation(Revision);
	else Player->ServerChoose(ChoiceID, Revision);
}

UDialoguePresentationWidget::UDialoguePresentationWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true);
	static ConstructorHelpers::FClassFinder<UUserWidget> Original(TEXT("/Game/OurStuff/Core/Dialogue/WBP_TextDisplay"));
	LayoutClass = Original.Class;
}

TSharedRef<SWidget> UDialoguePresentationWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget && LayoutClass)
	{
		Layout = CreateWidget<UUserWidget>(GetOwningPlayer(), LayoutClass);
		WidgetTree->RootWidget = Layout;
	}
	return Super::RebuildWidget();
}

void UDialoguePresentationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	APlayerController* PC = GetOwningPlayer();
	Player = PC ? PC->FindComponentByClass<UPlayerQuestComponent>() : nullptr;
	TextBox = Layout ? Cast<UUserWidget>(Layout->GetWidgetFromName(TEXT("TextBox"))) : nullptr;
	if (!Player || !TextBox) return;
	SetVisibility(ESlateVisibility::Visible);
	bPreviousCursor = PC->bShowMouseCursor;
	PC->SetIgnoreLookInput(true);
	PC->SetIgnoreMoveInput(true);
	PC->bShowMouseCursor = true;
	// Release gameplay mouse capture and focus the modal above the existing HUD.
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	RefreshConversation();
}

void UDialoguePresentationWidget::SetText(FName Name, const FText& Text)
{
	if (TextBox)
		if (auto* Block = Cast<UTextBlock>(TextBox->GetWidgetFromName(Name))) Block->SetText(Text);
}

void UDialoguePresentationWidget::SetBoxVisibility(FName Name, ESlateVisibility NewVisibility)
{
	if (TextBox)
		if (UWidget* Widget = TextBox->GetWidgetFromName(Name)) Widget->SetVisibility(NewVisibility);
}

void UDialoguePresentationWidget::RefreshConversation()
{
	if (!Player || !TextBox || DisplayedRevision == Player->Conversation.Revision) return;
	const FPlayerConversation& Snapshot = Player->Conversation;
	DisplayedRevision = Snapshot.Revision;
	bShowingChoices = false;
	SetText(TEXT("TXT_SpeakerName"), Snapshot.Speaker);
	SetText(TEXT("TXT_SpeakerText"), Snapshot.Text);
	SetText(TEXT("TXT_PlayerName"), NSLOCTEXT("Dialogue", "Player", "Player"));
	SetBoxVisibility(TEXT("VBOX_Speaker"), ESlateVisibility::SelfHitTestInvisible);
	SetBoxVisibility(TEXT("VBOX_Choice"), ESlateVisibility::Collapsed);
	SetBoxVisibility(TEXT("TextBlock_0"), ESlateVisibility::HitTestInvisible);
	if (auto* Image = Cast<UImage>(TextBox->GetWidgetFromName(TEXT("IMG_Picture"))))
	{
		Image->SetBrushFromTexture(Snapshot.Portrait);
		Image->SetVisibility(Snapshot.Portrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (Voice) Voice->Stop();
	Voice = Snapshot.Voice ? UGameplayStatics::SpawnSound2D(this, Snapshot.Voice) : nullptr;
	ChoiceWidgets.Reset();
	if (auto* Choices = Cast<UPanelWidget>(TextBox->GetWidgetFromName(TEXT("SBOX_Choice"))))
	{
		Choices->ClearChildren();
		// Authored choices include declines; only add Goodbye for a terminal line.
		for (int32 Index = 0; Index < FMath::Max(1, Snapshot.Choices.Num()); ++Index)
		{
			auto* Choice = CreateWidget<UConversationChoiceWidget>(GetOwningPlayer());
			Choice->Player = Player;
			Choice->Revision = Snapshot.Revision;
			Choice->ChoiceID = Snapshot.ChoiceIDs.IsValidIndex(Index) ? Snapshot.ChoiceIDs[Index] : INDEX_NONE;
			Choice->Label = Snapshot.Choices.IsValidIndex(Index) ? Snapshot.Choices[Index] : NSLOCTEXT("Dialogue", "Close", "Goodbye");
			ChoiceWidgets.Add(Choice);
			Choices->AddChild(Choice);
		}
	}
	SetUserFocus(GetOwningPlayer());
}

void UDialoguePresentationWidget::ShowChoices()
{
	if (!Player || !TextBox) return;
	bShowingChoices = true;
	SetBoxVisibility(TEXT("VBOX_Speaker"), ESlateVisibility::Collapsed);
	SetBoxVisibility(TEXT("VBOX_Choice"), ESlateVisibility::SelfHitTestInvisible);
	SetBoxVisibility(TEXT("TextBlock_0"), ESlateVisibility::Collapsed);
	SetBoxVisibility(TEXT("IMG_Picture"), ESlateVisibility::Hidden);
	if (!ChoiceWidgets.IsEmpty() && ChoiceWidgets[0]->GetChoiceButton())
		ChoiceWidgets[0]->GetChoiceButton()->SetUserFocus(GetOwningPlayer());
}

FReply UDialoguePresentationWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bShowingChoices && Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		ShowChoices();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
}

FReply UDialoguePresentationWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Player && Event.GetKey() == EKeys::Escape)
	{
		Player->ServerEndConversation(Player->Conversation.Revision);
		return FReply::Handled();
	}
	if (!bShowingChoices && (Event.GetKey() == EKeys::SpaceBar || Event.GetKey() == EKeys::Enter))
	{
		ShowChoices();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

void UDialoguePresentationWidget::NativeDestruct()
{
	if (Voice) Voice->Stop();
	if (Player)
		if (auto* PC = GetOwningPlayer())
		{
			PC->SetIgnoreLookInput(false);
			PC->SetIgnoreMoveInput(false);
			PC->bShowMouseCursor = bPreviousCursor;
			PC->SetInputMode(FInputModeGameOnly());
		}
	Super::NativeDestruct();
}

void UQuestTrackerRow::SetRow(const FText& Title, const FText& Detail, const FText& Count, float Fraction, bool bDone)
{
	if (!bCapturedColours)
	{
		bCapturedColours = true;
		if (TitleText) TitleColor = TitleText->GetColorAndOpacity();
		if (Progress) FillColor = Progress->GetFillColorAndOpacity();
	}
	if (TitleText)
	{
		TitleText->SetText(Title);
		TitleText->SetColorAndOpacity(bDone ? DoneTitleColor : TitleColor);
	}
	if (DetailText)
	{
		DetailText->SetText(Detail);
		DetailText->SetVisibility(Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (CountText) CountText->SetText(Count);
	if (Progress)
	{
		Progress->SetPercent(Fraction);
		Progress->SetFillColorAndOpacity(bDone ? DoneFillColor : FillColor);
	}
	if (DoneMark) DoneMark->SetVisibility(bDone ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	OnRowSet(bDone);
}

void UQuestConversationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Player = GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<UPlayerQuestComponent>() : nullptr;
	if (!Player) return;
	Player->OnQuestsChanged.AddUniqueDynamic(this, &UQuestConversationWidget::RefreshQuests);
	Player->OnConversationChanged.AddUniqueDynamic(this, &UQuestConversationWidget::RefreshConversation);
	Player->OnConversationRejected.AddUniqueDynamic(this, &UQuestConversationWidget::ShowRejection);
	DisplayedText.Reset();
	RefreshQuests();
	RefreshConversation();
}

FText UQuestConversationWidget::GetDisplayedQuestText() const
{
	return FText::FromString(DisplayedText);
}

void UQuestConversationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// The heist objective changes per player (ticket bought, hidden) with no event to bind to.
	RefreshQuests();
}

namespace
{
	void SetShown(UWidget* Widget, bool bShown)
	{
		if (Widget) Widget->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	void SetTextShown(UTextBlock* Block, const FString& String)
	{
		if (!Block) return;
		Block->SetText(FText::FromString(String));
		SetShown(Block, !String.IsEmpty());
	}
}

UQuestTrackerRow* UQuestConversationWidget::RowAt(UPanelWidget* List, TSubclassOf<UQuestTrackerRow> RowClass, float Spacing, int32 Index)
{
	if (!List || !RowClass) return nullptr;
	// Reuse rows: a new row has no size for its first frame, so recreating them made rows overlap on every change.
	UQuestTrackerRow* Row = Cast<UQuestTrackerRow>(List->GetChildAt(Index));
	if (!Row || Row->GetClass() != RowClass)
	{
		while (List->GetChildrenCount() > Index) List->RemoveChildAt(Index);
		Row = CreateWidget<UQuestTrackerRow>(GetOwningPlayer(), RowClass);
		if (!Row) return nullptr;
		List->AddChild(Row);
	}
	if (auto* VerticalSlot = Cast<UVerticalBoxSlot>(Row->Slot)) VerticalSlot->SetPadding(FMargin(0.f, Index == 0 ? 0.f : Spacing, 0.f, 0.f));
	return Row;
}

namespace
{
	void TrimRows(UPanelWidget* List, int32 Count)
	{
		while (List && List->GetChildrenCount() > Count) List->RemoveChildAt(Count);
	}
}

void UQuestConversationWidget::RefreshQuests()
{
	if (!Player) return;
	const ARobberGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARobberGameState>() : nullptr;
	const APlayerController* PC = GetOwningPlayer();
	const bool bHeist = GameState && PC;
	const uint8 Stage = bHeist ? GameState->GetGameStage() : 0;
	const FString Phase = bHeist ? RobberStage::GetDisplayName(Stage).ToString().ToUpper() : FString();
	const float TimeLeft = bHeist ? GameState->GetPhaseTimeRemaining() : -1.f;
	const int32 Seconds = TimeLeft >= 0.f ? FMath::CeilToInt(TimeLeft) : INDEX_NONE;
	const FString Timer = Seconds != INDEX_NONE ? FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60) : FString();
	const ARobberController* RobberController = Cast<ARobberController>(PC);
	const FString Message = RobberController && RobberController->IsHeistMessageVisible() ? RobberController->GetHeistMessage().ToString() : FString();
	const FString Objective = bHeist ? GameState->GetObjectiveText(PC->PlayerState).ToString() : FString();
	TArray<TPair<FString, int32>> Votes;
	if (bHeist && RobberStage::IsInLobby(Stage))
	{
		TArray<const ARobberEntryPoint*> Counted;
		for (const FRobberEntryVote& Vote : GameState->GetVotes())
			if (Vote.Entry && !Counted.Contains(Vote.Entry))
			{
				Counted.Add(Vote.Entry);
				Votes.Emplace(Vote.Entry->DisplayName.ToString(), GameState->GetVoteCount(Vote.Entry));
			}
	}
	static const TArray<FRobberJob> NoJobs;
	const TArray<FRobberJob>& Jobs = bHeist ? GameState->GetJobs() : NoJobs;
	TArray<const FPlayerQuest*> Favours;
	for (const FPlayerQuest& Quest : Player->Quests)
		if (IsValid(Quest.Definition) && Quest.State != EPlayerQuestState::Completed) Favours.Add(&Quest);

	// Ticks every frame, so compare a plain-text summary before touching any widgets.
	FString Summary = Phase;
	if (!Message.IsEmpty()) Summary += TEXT("\n") + Message;
	if (!Timer.IsEmpty()) Summary += TEXT("\nClosing in ") + Timer;
	Summary += TEXT("\n") + Objective;
	for (const TPair<FString, int32>& Vote : Votes) Summary += FString::Printf(TEXT("\n%s: %d"), *Vote.Key, Vote.Value);
	for (const FRobberJob& Job : Jobs)
		Summary += FString::Printf(TEXT("\n[%s] %s"), Job.bDone ? TEXT("x") : TEXT(" "), *Job.Title.ToString());
	for (const FPlayerQuest* Quest : Favours)
		Summary += FString::Printf(TEXT("\n%s\n%s %d / %d%s"), *Quest->Definition->QuestName.ToString(), *Quest->Definition->ObjectiveText.ToString(),
			Quest->Amount, Quest->Definition->RequiredAmount, Quest->State == EPlayerQuestState::ReadyToTurnIn ? TEXT(" (return to the quest giver)") : TEXT(""));
	if (Summary == DisplayedText && !DisplayedText.IsEmpty()) return;
	DisplayedText = Summary;

	SetShown(HeistCard, bHeist);
	if (PhaseText) PhaseText->SetText(FText::FromString(Phase));
	if (MessageText) MessageText->SetText(FText::FromString(Message));
	SetShown(MessageBox, !Message.IsEmpty());
	if (TimerText)
	{
		if (!bTimerColourCaptured)
		{
			bTimerColourCaptured = true;
			TimerColor = TimerText->GetColorAndOpacity();
		}
		TimerText->SetText(FText::FromString(Timer));
		TimerText->SetColorAndOpacity(Seconds != INDEX_NONE && Seconds <= TimerWarningSeconds ? TimerWarningColor : TimerColor);
	}
	SetShown(TimerBox, !Timer.IsEmpty());
	SetTextShown(ObjectiveText, Objective);

	const int32 Crew = bHeist ? FMath::Max(1, GameState->PlayerArray.Num()) : 1;
	for (int32 Index = 0; Index < Votes.Num(); ++Index)
		if (UQuestTrackerRow* Row = RowAt(VotesList, VoteRowClass, RowSpacing, Index))
			Row->SetRow(FText::FromString(Votes[Index].Key), FText::GetEmpty(), FText::AsNumber(Votes[Index].Value),
				FMath::Clamp(float(Votes[Index].Value) / Crew, 0.f, 1.f), false);
	TrimRows(VotesList, Votes.Num());
	SetShown(VotesBox, !Votes.IsEmpty());

	for (int32 Index = 0; Index < Jobs.Num(); ++Index)
		if (UQuestTrackerRow* Row = RowAt(JobsList, JobRowClass, RowSpacing, Index))
			Row->SetRow(Jobs[Index].Title, FText::GetEmpty(), FText::GetEmpty(), Jobs[Index].bDone ? 1.f : 0.f, Jobs[Index].bDone);
	TrimRows(JobsList, Jobs.Num());
	SetShown(JobsBox, !Jobs.IsEmpty());

	for (int32 Index = 0; Index < Favours.Num(); ++Index)
	{
		const FPlayerQuest& Quest = *Favours[Index];
		const int32 Required = FMath::Max(1, Quest.Definition->RequiredAmount);
		if (UQuestTrackerRow* Row = RowAt(FavourList, FavourRowClass, FavourSpacing, Index))
			Row->SetRow(Quest.Definition->QuestName, Quest.Definition->ObjectiveText,
				FText::FromString(FString::Printf(TEXT("%d / %d"), Quest.Amount, Required)),
				FMath::Clamp(float(Quest.Amount) / Required, 0.f, 1.f), Quest.State == EPlayerQuestState::ReadyToTurnIn);
	}
	TrimRows(FavourList, Favours.Num());
	SetShown(FavourCard, !Favours.IsEmpty());
}

void UQuestConversationWidget::RefreshConversation()
{
	if (!Player) return;
	if (!Player->Conversation.NPC)
	{
		if (Dialogue) Dialogue->RemoveFromParent();
		Dialogue = nullptr;
		return;
	}
	if (FeedbackText) FeedbackText->SetText(FText::GetEmpty());
	if (!Dialogue)
	{
		Dialogue = CreateWidget<UDialoguePresentationWidget>(GetOwningPlayer());
		// Player-screen layers are below viewport HUDs regardless of their local Z order.
		Dialogue->AddToViewport(1000);
	}
	else Dialogue->RefreshConversation();
}

void UQuestConversationWidget::ShowRejection(FText Reason) { if (FeedbackText) FeedbackText->SetText(Reason); }

void UQuestConversationWidget::NativeDestruct()
{
	if (Player)
	{
		Player->OnQuestsChanged.RemoveDynamic(this, &UQuestConversationWidget::RefreshQuests);
		Player->OnConversationChanged.RemoveDynamic(this, &UQuestConversationWidget::RefreshConversation);
		Player->OnConversationRejected.RemoveDynamic(this, &UQuestConversationWidget::ShowRejection);
	}
	if (Dialogue) Dialogue->RemoveFromParent();
	Dialogue = nullptr;
	Super::NativeDestruct();
}
