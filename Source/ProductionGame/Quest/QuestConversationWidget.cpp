#include "Quest/QuestConversationWidget.h"
#include "Quest/PlayerQuestComponent.h"
#include "Quest/QuestDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
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

TSharedRef<SWidget> UQuestConversationWidget::RebuildWidget()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UBorder* QuestPanel = WidgetTree->ConstructWidget<UBorder>();
	QuestPanel->SetBrushColor(FLinearColor(0.025f, 0.03f, 0.045f, 0.9f));
	QuestPanel->SetPadding(FMargin(18.f));
	QuestPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	QuestText = WidgetTree->ConstructWidget<UTextBlock>();
	QuestText->SetAutoWrapText(true);
	UScrollBox* QuestScroll = WidgetTree->ConstructWidget<UScrollBox>();
	QuestScroll->AddChild(QuestText);
	QuestPanel->AddChild(QuestScroll);
	UCanvasPanelSlot* QuestSlot = Canvas->AddChildToCanvas(QuestPanel);
	QuestSlot->SetAnchors(FAnchors(1.f, 0.f));
	QuestSlot->SetAlignment(FVector2D(1.f, 0.f));
	QuestSlot->SetPosition(FVector2D(-24.f, 40.f));
	QuestSlot->SetSize(FVector2D(360.f, 360.f));
	FeedbackText = WidgetTree->ConstructWidget<UTextBlock>();
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
	return QuestText ? QuestText->GetText() : FText::GetEmpty();
}

void UQuestConversationWidget::RefreshQuests()
{
	if (!Player || !QuestText) return;
	FString Text;
	for (const FPlayerQuest& Quest : Player->Quests)
	{
		if (!IsValid(Quest.Definition) || Quest.State == EPlayerQuestState::Completed) continue;
		Text += TEXT("\n") + Quest.Definition->QuestName.ToString() + TEXT("\n");
		{
			Text += Quest.Definition->ObjectiveText.ToString();
			Text += FString::Printf(TEXT("  %d / %d\n"), Quest.Amount, Quest.Definition->RequiredAmount);
			if (Quest.State == EPlayerQuestState::ReadyToTurnIn) Text += TEXT("Objective complete - return to the quest giver\n");
		}
	}
	QuestText->SetText(FText::FromString(Text.TrimStartAndEnd()));
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
