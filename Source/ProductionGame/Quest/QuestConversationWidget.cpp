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
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

namespace QuestTracker
{
	const FLinearColor Panel(0.008f, 0.01f, 0.016f, 0.8f);
	const FLinearColor HeistAccent(1.f, 0.62f, 0.16f);
	const FLinearColor FavourAccent(0.32f, 0.74f, 0.95f);
	const FLinearColor Done(0.38f, 0.85f, 0.42f);
	const FLinearColor Primary(0.94f, 0.94f, 0.92f);
	const FLinearColor Muted(0.58f, 0.6f, 0.64f);
	const FLinearColor Track(1.f, 1.f, 1.f, 0.1f);

	FSlateBrush Rounded(const FLinearColor& Color, float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius));
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	UBorder* Box(UWidgetTree* Tree, const FLinearColor& Color, float Radius, const FMargin& Padding = FMargin(0.f))
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>();
		Border->SetBrush(Rounded(Color, Radius));
		Border->SetPadding(Padding);
		return Border;
	}

	UTextBlock* Text(UWidgetTree* Tree, const FString& String, int32 Size, const FLinearColor& Color, FName Typeface = TEXT("Regular"), int32 LetterSpacing = 0)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
		Font.LetterSpacing = LetterSpacing;
		Block->SetFont(Font);
		Block->SetText(FText::FromString(String));
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetShadowOffset(FVector2D(1.f, 1.f));
		Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		Block->SetAutoWrapText(true);
		return Block;
	}

	UWidget* Sized(UWidgetTree* Tree, UWidget* Child, float Width, float Height)
	{
		USizeBox* Size = Tree->ConstructWidget<USizeBox>();
		if (Width > 0.f) Size->SetWidthOverride(Width);
		if (Height > 0.f) Size->SetHeightOverride(Height);
		Size->AddChild(Child);
		return Size;
	}

	void Add(UVerticalBox* Box, UWidget* Child, const FMargin& Padding = FMargin(0.f))
	{
		Box->AddChildToVerticalBox(Child)->SetPadding(Padding);
	}

	UHorizontalBoxSlot* Add(UHorizontalBox* Box, UWidget* Child, float Fill = -1.f)
	{
		UHorizontalBoxSlot* Slot = Box->AddChildToHorizontalBox(Child);
		Slot->SetVerticalAlignment(VAlign_Center);
		if (Fill >= 0.f)
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = Fill;
			Slot->SetSize(Size);
		}
		return Slot;
	}

	UWidget* Checkbox(UWidgetTree* Tree, bool bChecked)
	{
		UBorder* Outer = Box(Tree, bChecked ? Done : Muted, 3.f, FMargin(2.f));
		Outer->AddChild(Box(Tree, bChecked ? Done : FLinearColor(0.02f, 0.025f, 0.035f, 1.f), 2.f));
		return Sized(Tree, Outer, 13.f, 13.f);
	}

	UWidget* ProgressBar(UWidgetTree* Tree, float Fraction, const FLinearColor& Color)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>();
		Add(Row, Box(Tree, Color, 3.f), Fraction);
		Add(Row, Tree->ConstructWidget<USpacer>(), 1.f - Fraction);
		UBorder* TrackBox = Box(Tree, Track, 3.f);
		TrackBox->AddChild(Row);
		return Sized(Tree, TrackBox, 0.f, 6.f);
	}
}

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

TSharedRef<SWidget> UQuestConversationWidget::RebuildWidget()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UVerticalBox* Tracker = WidgetTree->ConstructWidget<UVerticalBox>();
	HeistCard = MakeCard(Tracker, TEXT("THE HEIST"), QuestTracker::HeistAccent, HeistBody, &PhaseTag);
	FavourCard = MakeCard(Tracker, TEXT("FAVOURS"), QuestTracker::FavourAccent, FavourBody);
	UWidget* TrackerSized = QuestTracker::Sized(WidgetTree, Tracker, 340.f, 0.f);
	TrackerSized->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* QuestSlot = Canvas->AddChildToCanvas(TrackerSized);
	QuestSlot->SetAnchors(FAnchors(1.f, 0.f));
	QuestSlot->SetAlignment(FVector2D(1.f, 0.f));
	QuestSlot->SetPosition(FVector2D(-24.f, 40.f));
	QuestSlot->SetAutoSize(true);
	DisplayedText.Reset();
	FeedbackText = QuestTracker::Text(WidgetTree, FString(), 14, FLinearColor(1.f, 0.45f, 0.38f), TEXT("Bold"));
	FeedbackText->SetAutoWrapText(false);
	FeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* FeedbackSlot = Canvas->AddChildToCanvas(FeedbackText);
	FeedbackSlot->SetPosition(FVector2D(24.f, 40.f));
	FeedbackSlot->SetAutoSize(true);
	return Super::RebuildWidget();
}

void UQuestConversationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Player = GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<UPlayerQuestComponent>() : nullptr;
	if (!Player) return;
	Player->OnQuestsChanged.AddUniqueDynamic(this, &UQuestConversationWidget::RefreshQuests);
	Player->OnConversationChanged.AddUniqueDynamic(this, &UQuestConversationWidget::RefreshConversation);
	Player->OnConversationRejected.AddUniqueDynamic(this, &UQuestConversationWidget::ShowRejection);
	RefreshQuests();
	RefreshConversation();
}

FText UQuestConversationWidget::GetDisplayedQuestText() const
{
	return FText::FromString(DisplayedText);
}

UWidget* UQuestConversationWidget::MakeCard(UVerticalBox* Tracker, const FString& Title, const FLinearColor& Accent, TObjectPtr<UVerticalBox>& OutBody, TObjectPtr<UTextBlock>* OutTag)
{
	using namespace QuestTracker;
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Add(Header, Sized(WidgetTree, Box(WidgetTree, Accent, 2.f), 4.f, 14.f))->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	Add(Header, Text(WidgetTree, Title, 10, Accent, TEXT("Bold"), 250));
	if (OutTag)
	{
		Add(Header, WidgetTree->ConstructWidget<USpacer>(), 1.f);
		*OutTag = Text(WidgetTree, FString(), 9, Muted, TEXT("Bold"), 150);
		(*OutTag)->SetAutoWrapText(false);
		UBorder* Pill = Box(WidgetTree, Track, 4.f, FMargin(7.f, 2.f));
		Pill->AddChild(*OutTag);
		Add(Header, Pill);
	}
	Add(Content, Header);
	Add(Content, Sized(WidgetTree, Box(WidgetTree, Accent.CopyWithNewOpacity(0.25f), 0.f), 0.f, 1.f), FMargin(0.f, 8.f, 0.f, 10.f));
	OutBody = WidgetTree->ConstructWidget<UVerticalBox>();
	Add(Content, OutBody);
	UBorder* Card = Box(WidgetTree, Panel, 8.f, FMargin(14.f, 12.f, 14.f, 14.f));
	Card->AddChild(Content);
	Card->SetVisibility(ESlateVisibility::Collapsed);
	Add(Tracker, Card, FMargin(0.f, 0.f, 0.f, 10.f));
	return Card;
}

void UQuestConversationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// The heist objective changes per player (ticket bought, hidden) with no event to bind to.
	RefreshQuests();
}

void UQuestConversationWidget::RefreshQuests()
{
	using namespace QuestTracker;
	if (!Player || !HeistBody || !FavourBody) return;
	const ARobberGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARobberGameState>() : nullptr;
	const APlayerController* PC = GetOwningPlayer();
	const bool bHeist = GameState && PC;
	const uint8 Stage = bHeist ? GameState->GetGameStage() : 0;
	const FString Phase = bHeist ? RobberStage::GetDisplayName(Stage).ToString().ToUpper() : FString();
	const float TimeLeft = bHeist ? GameState->GetPhaseTimeRemaining() : -1.f;
	const int32 Seconds = TimeLeft >= 0.f ? FMath::CeilToInt(TimeLeft) : INDEX_NONE;
	const ARobberController* RobberController = Cast<ARobberController>(PC);
	const FString Message = RobberController && RobberController->IsHeistMessageVisible() ? RobberController->GetHeistMessage().ToString() : FString();
	const FString Objective = bHeist ? GameState->GetObjectiveText(PC->PlayerState).ToString() : FString();
	TArray<TPair<FString, int32>> Votes;
	if (bHeist && Stage == RobberStage::Lobby)
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
	if (Seconds != INDEX_NONE) Summary += FString::Printf(TEXT("\nClosing in %d:%02d"), Seconds / 60, Seconds % 60);
	Summary += TEXT("\n") + Objective;
	for (const TPair<FString, int32>& Vote : Votes) Summary += FString::Printf(TEXT("\n%s: %d"), *Vote.Key, Vote.Value);
	for (const FRobberJob& Job : Jobs)
		Summary += FString::Printf(TEXT("\n[%s] %s"), Job.bDone ? TEXT("x") : TEXT(" "), *Job.Title.ToString());
	for (const FPlayerQuest* Quest : Favours)
		Summary += FString::Printf(TEXT("\n%s\n%s %d / %d%s"), *Quest->Definition->QuestName.ToString(), *Quest->Definition->ObjectiveText.ToString(),
			Quest->Amount, Quest->Definition->RequiredAmount, Quest->State == EPlayerQuestState::ReadyToTurnIn ? TEXT(" (return to the quest giver)") : TEXT(""));
	if (Summary == DisplayedText && !DisplayedText.IsEmpty()) return;
	DisplayedText = Summary;

	HeistBody->ClearChildren();
	if (PhaseTag) PhaseTag->SetText(FText::FromString(Phase));
	if (!Message.IsEmpty())
	{
		UBorder* Callout = Box(WidgetTree, HeistAccent.CopyWithNewOpacity(0.16f), 6.f, FMargin(10.f, 8.f));
		Callout->AddChild(Text(WidgetTree, Message, 12, Primary, TEXT("Bold")));
		Add(HeistBody, Callout, FMargin(0.f, 0.f, 0.f, 10.f));
	}
	if (Seconds != INDEX_NONE)
	{
		const FLinearColor TimerColor = Seconds <= 10 ? FLinearColor(1.f, 0.38f, 0.32f) : Primary;
		UHorizontalBox* Timer = WidgetTree->ConstructWidget<UHorizontalBox>();
		Add(Timer, Text(WidgetTree, TEXT("CLOSING IN"), 9, Muted, TEXT("Bold"), 200), 1.f);
		Add(Timer, Text(WidgetTree, FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60), 20, TimerColor, TEXT("Bold")));
		Add(HeistBody, Timer, FMargin(0.f, 0.f, 0.f, 8.f));
	}
	if (!Objective.IsEmpty()) Add(HeistBody, Text(WidgetTree, Objective, 14, Primary));
	if (!Votes.IsEmpty())
	{
		Add(HeistBody, Text(WidgetTree, TEXT("VOTES"), 9, Muted, TEXT("Bold"), 200), FMargin(0.f, 12.f, 0.f, 2.f));
		const int32 Crew = FMath::Max(1, GameState->PlayerArray.Num());
		for (const TPair<FString, int32>& Vote : Votes)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			Add(Row, Text(WidgetTree, Vote.Key, 12, Primary), 1.f);
			Add(Row, Text(WidgetTree, FString::FromInt(Vote.Value), 12, HeistAccent, TEXT("Bold")));
			Add(HeistBody, Row, FMargin(0.f, 6.f, 0.f, 3.f));
			Add(HeistBody, ProgressBar(WidgetTree, FMath::Clamp(float(Vote.Value) / Crew, 0.f, 1.f), HeistAccent));
		}
	}
	if (!Jobs.IsEmpty())
	{
		Add(HeistBody, Text(WidgetTree, TEXT("JOBS"), 9, Muted, TEXT("Bold"), 200), FMargin(0.f, Objective.IsEmpty() ? 0.f : 12.f, 0.f, 2.f));
		for (const FRobberJob& Job : Jobs)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			Add(Row, Checkbox(WidgetTree, Job.bDone))->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
			Add(Row, Text(WidgetTree, Job.Title.ToString(), 12, Job.bDone ? Muted : Primary), 1.f);
			Add(HeistBody, Row, FMargin(0.f, 6.f, 0.f, 0.f));
		}
	}
	HeistCard->SetVisibility(HeistBody->HasAnyChildren() || !Phase.IsEmpty() ?ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	FavourBody->ClearChildren();
	for (int32 Index = 0; Index < Favours.Num(); ++Index)
	{
		const FPlayerQuest& Quest = *Favours[Index];
		const bool bReady = Quest.State == EPlayerQuestState::ReadyToTurnIn;
		const int32 Required = FMath::Max(1, Quest.Definition->RequiredAmount);
		UVerticalBox* Entry = WidgetTree->ConstructWidget<UVerticalBox>();
		Add(Entry, Text(WidgetTree, Quest.Definition->QuestName.ToString(), 13, Primary, TEXT("Bold")));
		Add(Entry, Text(WidgetTree, Quest.Definition->ObjectiveText.ToString(), 11, Muted), FMargin(0.f, 2.f, 0.f, 0.f));
		UHorizontalBox* Progress = WidgetTree->ConstructWidget<UHorizontalBox>();
		Add(Progress, ProgressBar(WidgetTree, FMath::Clamp(float(Quest.Amount) / Required, 0.f, 1.f), bReady ? Done : FavourAccent), 1.f);
		Add(Progress, Text(WidgetTree, FString::Printf(TEXT("%d / %d"), Quest.Amount, Required), 10, bReady ? Done : Muted, TEXT("Bold")))
			->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		Add(Entry, Progress, FMargin(0.f, 6.f, 0.f, 0.f));
		if (bReady) Add(Entry, Text(WidgetTree, TEXT("Return to the quest giver"), 11, Done, TEXT("Bold")), FMargin(0.f, 4.f, 0.f, 0.f));
		Add(FavourBody, Entry, FMargin(0.f, Index == 0 ? 0.f : 14.f, 0.f, 0.f));
	}
	FavourCard->SetVisibility(Favours.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
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
	FeedbackText->SetText(FText::GetEmpty());
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
