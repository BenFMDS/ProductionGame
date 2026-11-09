#include "Heist/RobberCaseFileUI.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Dialogue/QuestDialogueCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Heist/RobberInteractables.h"
#include "Heist/RobberObjectiveMarkers.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "ProductionGameCharacter.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Dialogue/DialogueComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberEntryPoint.h"
#include "Heist/RobberJournal.h"
#include "Heist/RobberStage.h"
#include "Heist/RobberVault.h"
#include "Quest/PlayerQuestComponent.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"

// --- Shared ---

ARobberController* URobberCaseFileWidgetBase::GetRobber() const
{
	return Cast<ARobberController>(GetOwningPlayer());
}

const ARobberGameState* URobberCaseFileWidgetBase::GetRobberState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ARobberGameState>() : nullptr;
}

URobberJournalComponent* URobberCaseFileWidgetBase::GetJournal() const
{
	return GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<URobberJournalComponent>() : nullptr;
}

void URobberCaseFileWidgetBase::SetText(UTextBlock* Block, const FText& Text)
{
	// Comparing first: SetText invalidates layout even when nothing changed, and the HUD sets every frame.
	if (Block && !Block->GetText().EqualTo(Text))
	{
		Block->SetText(Text);
	}
}

void URobberCaseFileWidgetBase::Show(UWidget* Widget, bool bShown)
{
	const ESlateVisibility Wanted = bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	if (Widget && Widget->GetVisibility() != Wanted)
	{
		Widget->SetVisibility(Wanted);
	}
}

int32 URobberCaseFileWidgetBase::HeatLevel(const ARobberGameState* State)
{
	if (!State)
	{
		return 0;
	}
	return static_cast<int32>(State->GetHeatTier());
}

FLinearColor URobberCaseFileWidgetBase::HeatColour(int32 Level) const
{
	return HeatColours.IsValidIndex(Level) ? HeatColours[Level] : FLinearColor::White;
}

FLinearColor URobberCaseFileWidgetBase::CrewColour(const APlayerState* Player) const
{
	return Player && CrewColours.Num() > 0 ? CrewColours[Player->GetPlayerId() % CrewColours.Num()] : FLinearColor::White;
}

FText URobberCaseFileWidgetBase::Headline(int32 Level)
{
	static const FText Lines[] = {
		NSLOCTEXT("RobberCaseFile", "Headline0", "QUIET NIGHT AT THE MUSEUM"),
		NSLOCTEXT("RobberCaseFile", "Headline1", "STRANGE NOISES REPORTED"),
		NSLOCTEXT("RobberCaseFile", "Headline2", "BREAK-IN FEARED!"),
		NSLOCTEXT("RobberCaseFile", "Headline3", "THIEVES AT LARGE!"),
	};
	return Lines[FMath::Clamp(Level, 0, 3)];
}

FText URobberCaseFileWidgetBase::Subline(int32 Level)
{
	static const FText Lines[] = {
		NSLOCTEXT("RobberCaseFile", "Sub0", "Night guard reports \"absolutely nothing\" for the third night running."),
		NSLOCTEXT("RobberCaseFile", "Sub1", "Neighbours describe \"a sort of rattling\" somewhere near the museum."),
		NSLOCTEXT("RobberCaseFile", "Sub2", "Police \"very nearly\" on their way, says spokesman."),
		NSLOCTEXT("RobberCaseFile", "Sub3", "Alarm rings out across the city. Guards converge on the gallery."),
	};
	return Lines[FMath::Clamp(Level, 0, 3)];
}

FText URobberCaseFileWidgetBase::NightName(const ARobberGameState* State)
{
	if (!State)
	{
		return FText::GetEmpty();
	}
	return State->IsTutorial() ? NSLOCTEXT("RobberCaseFile", "Tutorial", "Tutorial")
		: FText::Format(NSLOCTEXT("RobberCaseFile", "NightN", "Night {0}"), FMath::Max(1, State->GetCurrentNight() + 1));
}

FText URobberCaseFileWidgetBase::Clock(float Seconds)
{
	const int32 Whole = FMath::CeilToInt(FMath::Max(0.f, Seconds));
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60));
}

TArray<APlayerState*> URobberCaseFileWidgetBase::SortedCrew(const ARobberGameState* State)
{
	// PlayerId order, so cards and colours match on every machine, like the map pins.
	TArray<APlayerState*> Crew;
	if (State)
	{
		for (APlayerState* Player : State->PlayerArray)
		{
			if (Player)
			{
				Crew.Add(Player);
			}
		}
	}
	Crew.Sort([](const APlayerState& A, const APlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	return Crew;
}

FText URobberCaseFileWidgetBase::CrewStatus(const ARobberGameState* State, const APlayerState* Player)
{
	if (!State || !Player)
	{
		return FText::GetEmpty();
	}
	const uint8 Stage = State->GetGameStage();
	if (RobberStage::IsInLobby(Stage))
	{
		return NSLOCTEXT("RobberCaseFile", "InLobby", "In the safe house");
	}
	const FRobberCrewStatus Status = State->GetCrewStatus(Player);
	const AProductionGameCharacter* Robber = Cast<AProductionGameCharacter>(Player->GetPawn());
	if (Robber && Robber->IsHiding())
	{
		return NSLOCTEXT("RobberCaseFile", "Hiding", "Hiding");
	}
	if (Status.bInside || Stage != RobberStage::BreakIn)
	{
		return NSLOCTEXT("RobberCaseFile", "Inside", "Inside");
	}
	return Status.bHasTicket ? NSLOCTEXT("RobberCaseFile", "Ticket", "Has a ticket") : NSLOCTEXT("RobberCaseFile", "Outside", "Getting in");
}

bool URobberCaseFileWidgetBase::VaultStillShut(const ARobberGameState* State)
{
	return State && State->GetJobs().ContainsByPredicate([](const FRobberJob& Job) { return Job.Id == ARobberVaultDoor::JobId && !Job.bDone; });
}

FText URobberCaseFileWidgetBase::PlayerName(const APlayerState* Player) const
{
	if (!Player)
	{
		return FText::GetEmpty();
	}
	return GetOwningPlayer() && Player == GetOwningPlayer()->PlayerState ? NSLOCTEXT("RobberCaseFile", "You", "You") : FText::FromString(Player->GetPlayerName().Left(12));
}

namespace
{
	FText CaseFileDigits(const TArray<int32>& Digits, int32 Length)
	{
		FString Out;
		for (int32 Index = 0; Index < Length; ++Index)
		{
			const int32 Digit = Digits.IsValidIndex(Index) ? Digits[Index] : INDEX_NONE;
			Out += (Index ? TEXT(" ") : TEXT("")) + (Digit == INDEX_NONE ? FString(TEXT("_")) : FString::FromInt(Digit));
		}
		return FText::FromString(Out);
	}

	FText CaseFileKeyName(FName Key)
	{
		// "GeneratorRoom" -> "Generator Room"
		const FString Raw = Key.ToString();
		FString Out;
		for (int32 Index = 0; Index < Raw.Len(); ++Index)
		{
			if (Index > 0 && FChar::IsUpper(Raw[Index]) && !FChar::IsUpper(Raw[Index - 1]))
			{
				Out += TEXT(' ');
			}
			Out += Raw[Index];
		}
		return FText::FromString(Out);
	}
}

// --- HUD ---

FText URobberCaseFileHUD::GetHeadline() const
{
	return Headline(HeatLevel(GetRobberState()));
}

FText URobberCaseFileHUD::GetCodeLine() const
{
	const URobberJournalComponent* Journal = GetJournal();
	const bool bKnowSome = Journal && Journal->GetVaultDigits().Num() > 0;
	if (!bKnowSome && !VaultStillShut(GetRobberState()))
	{
		return FText::GetEmpty();
	}
	return CaseFileDigits(Journal ? Journal->GetVaultDigits() : TArray<int32>(), ARobberVaultDoor::CodeLength);
}

void URobberCaseFileHUD::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ApplySizes();
	UCanvasPanel* Root = Cast<UCanvasPanel>(GetRootWidget());
	if (Root && WidgetTree)
	{
		BuildStoryWidgets(Root);
		BuildMarkerWidgets(Root);
	}
	if (Find(TEXT("SprintBar")) || !Root || !WidgetTree)
	{
		return;
	}
	// The WBP has no sprint bar of its own: build the default one. Adding SprintPanel/SprintBar in the WBP replaces it.
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("SprintBar"));
	FProgressBarStyle Style;
	Style.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(FColor(0xfd, 0xf6, 0xe3)), 5.f));
	Style.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 5.f));
	Style.SetMarqueeImage(FSlateRoundedBoxBrush(FLinearColor::White, 5.f));
	Bar->SetWidgetStyle(Style);
	Bar->SetFillColorAndOpacity(SprintColour);
	Bar->SetPercent(1.f);

	// Ink outline round the bar, like the rest of the Case File pieces.
	UBorder* Outline = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SprintOutline"));
	Outline->SetBrush(FSlateRoundedBoxBrush(TextColour, 8.f));
	Outline->SetPadding(FMargin(3.f));
	Outline->SetContent(Bar);

	USizeBox* Panel = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SprintPanel"));
	Panel->SetWidthOverride(320.f);
	Panel->SetHeightOverride(20.f);
	Panel->SetContent(Outline);
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	Panel->SetRenderOpacity(0.f);

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 1.f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 1.f));
	PanelSlot->SetAutoSize(true);
	PanelSlot->SetPosition(FVector2D(0.f, -110.f));
}

void URobberCaseFileHUD::ApplySizes()
{
	const auto Scale = [](UWidget* Widget, float Amount, const FVector2D& Pivot)
	{
		if (Widget)
		{
			Widget->SetRenderTransformPivot(Pivot);
			Widget->SetRenderScale(FVector2D(Amount));
		}
	};
	const auto Pin = [](UWidget* Widget, const FVector2D& Position)
	{
		if (UCanvasPanelSlot* PanelSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
		{
			PanelSlot->SetPosition(Position);
		}
	};
	UWidget* Note = Find(TEXT("NoteSize"));
	Scale(Note, NoteScale, FVector2D(0.f, 0.f));
	Pin(Note, FVector2D(CornerMargin, CornerMargin));

	// The whole column shrinks so the stamp and toast close up under the smaller paper; the toast is then scaled back.
	UWidget* Column = Find(TEXT("PaperColumn"));
	Scale(Column, PaperScale, FVector2D(1.f, 0.f));
	Pin(Column, FVector2D(-CornerMargin, CornerMargin));
	Scale(Find(TEXT("ToastPanel")), 1.f / FMath::Max(PaperScale, 0.01f), FVector2D(1.f, 0.f));

	// Teammates are tagged in the world now (URobberCrewTagComponent); the journal's CREW tab still lists them.
	Show(Find(TEXT("CrewCards")), false);
}


// --- Story quest note and stamp ---

namespace
{
	const FLinearColor StoryInk(FColor(0x1b, 0x1b, 0x1b));
	const FLinearColor StoryDone(FColor(0x86, 0x8e, 0x96));
	const FLinearColor StoryRed(FColor(0xc0, 0x39, 0x2b));
	const FLinearColor StoryGreen(FColor(0x2f, 0x9e, 0x44));
	const FLinearColor StoryPaper(FColor(0xfd, 0xf6, 0xe3));

	/** Sizes here are the mock-up's pixels at 1080p; a UMG font size is in points. */
	FSlateFontInfo StoryFont(const TCHAR* Name, float Pixels, FName Typeface = TEXT("Regular"))
	{
		const FString Path = FString::Printf(TEXT("/Game/OurStuff/UI/CaseFile/Fonts/%s.%s"), Name, Name);
		const UObject* Font = LoadObject<UFont>(nullptr, *Path);
		if (!Font)
		{
			// The handwriting font is optional; the note still reads in the game's own.
			Font = LoadObject<UFont>(nullptr, TEXT("/Game/OurStuff/UI/CaseFile/Fonts/F_Fredoka.F_Fredoka"));
		}
		return FSlateFontInfo(Font, Pixels * 0.75f, Typeface);
	}

	UImage* StorySolid(UWidgetTree* Tree, const FLinearColor& Colour)
	{
		UImage* Image = Tree->ConstructWidget<UImage>();
		Image->SetColorAndOpacity(Colour);
		return Image;
	}

	/** A thin line that stretches sideways. A fixed box, because an image's own desired size didn't hold in a slot. */
	USizeBox* StoryRule(UWidgetTree* Tree, const FLinearColor& Colour, float Height)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>();
		Box->SetHeightOverride(Height);
		Box->SetContent(StorySolid(Tree, Colour));
		return Box;
	}

	UTextBlock* StoryLabel(UWidgetTree* Tree, const FSlateFontInfo& Font, const FLinearColor& Colour)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Colour));
		return Text;
	}

	UBorder* StoryBox(UWidgetTree* Tree, const FLinearColor& Colour, const FMargin& Padding, UWidget* Content)
	{
		UBorder* Box = Tree->ConstructWidget<UBorder>();
		Box->SetBrush(FSlateColorBrush(Colour));
		Box->SetPadding(Padding);
		Box->SetContent(Content);
		return Box;
	}

	UOverlaySlot* StoryFill(UOverlay* Stack, UWidget* Child)
	{
		UOverlaySlot* NewSlot = Stack->AddChildToOverlay(Child);
		NewSlot->SetHorizontalAlignment(HAlign_Fill);
		NewSlot->SetVerticalAlignment(VAlign_Fill);
		return NewSlot;
	}

	/** Two ink strokes that wander a little, like the underline on a handwritten note. */
	UWidget* StoryUnderline(UWidgetTree* Tree, float Height)
	{
		UVerticalBox* Lines = Tree->ConstructWidget<UVerticalBox>();
		const float Angles[] = { -0.5f, 0.35f };
		for (const float Angle : Angles)
		{
			USizeBox* Line = StoryRule(Tree, StoryInk, 2.f);
			Line->SetRenderTransformAngle(Angle);
			UVerticalBoxSlot* LineSlot = Lines->AddChildToVerticalBox(Line);
			LineSlot->SetHorizontalAlignment(HAlign_Fill);
			LineSlot->SetPadding(FMargin(0.f, Height * 0.15f));
		}
		return Lines;
	}
}

FRobberStoryRowUI URobberCaseFileHUD::MakeStoryRow(const FSlateFontInfo& Font)
{
	FRobberStoryRowUI Ui;
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Ui.Row = Row;

	UOverlay* Marker = WidgetTree->ConstructWidget<UOverlay>();
	UTextBlock* Bullet = StoryLabel(WidgetTree, Font, StoryInk);
	Bullet->SetText(FText::FromString(TEXT("•")));
	UOverlaySlot* BulletSlot = Marker->AddChildToOverlay(Bullet);
	BulletSlot->SetHorizontalAlignment(HAlign_Center);
	Ui.Bullet = Bullet;

	// Drawn, because the handwriting font has no tick in it.
	UCanvasPanel* TickMark = WidgetTree->ConstructWidget<UCanvasPanel>();
	const struct { FVector2D Where; FVector2D Size; float Angle; } Strokes[] = {
		{ FVector2D(0., 11.), FVector2D(8., 3.), 45.f },
		{ FVector2D(4., 8.), FVector2D(14., 3.), -55.f },
	};
	for (const auto& Stroke : Strokes)
	{
		UImage* Bar = StorySolid(WidgetTree, StoryGreen);
		Bar->SetRenderTransformAngle(Stroke.Angle);
		UCanvasPanelSlot* BarSlot = TickMark->AddChildToCanvas(Bar);
		BarSlot->SetPosition(Stroke.Where);
		BarSlot->SetSize(Stroke.Size);
	}
	StoryFill(Marker, TickMark);
	Ui.Tick = TickMark;

	USizeBox* MarkerSize = WidgetTree->ConstructWidget<USizeBox>();
	MarkerSize->SetWidthOverride(20.f);
	MarkerSize->SetHeightOverride(22.f);
	MarkerSize->SetContent(Marker);
	Row->AddChildToHorizontalBox(MarkerSize)->SetVerticalAlignment(VAlign_Top);

	UOverlay* TextStack = WidgetTree->ConstructWidget<UOverlay>();
	UTextBlock* Text = StoryLabel(WidgetTree, Font, StoryInk);
	Text->SetAutoWrapText(true);
	StoryFill(TextStack, Text);
	Ui.Text = Text;
	USizeBox* Strike = StoryRule(WidgetTree, StoryDone, 2.f);
	UOverlaySlot* StrikeSlot = TextStack->AddChildToOverlay(Strike);
	StrikeSlot->SetHorizontalAlignment(HAlign_Fill);
	StrikeSlot->SetVerticalAlignment(VAlign_Center);
	Ui.Strike = Strike;
	Row->AddChildToHorizontalBox(TextStack)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	return Ui;
}

FRobberStoryNoteUI URobberCaseFileHUD::MakeStoryNote(const FLinearColor& Paper, int32 RowCount, float Width)
{
	FRobberStoryNoteUI Ui;
	const FSlateFontInfo RowFont = StoryFont(TEXT("F_PatrickHand"), 21.f);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Ui.Title = StoryLabel(WidgetTree, StoryFont(TEXT("F_PatrickHand"), 30.f), StoryInk);
	Ui.Title->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(Ui.Title);
	UVerticalBoxSlot* LineSlot = Column->AddChildToVerticalBox(StoryUnderline(WidgetTree, 10.f));
	LineSlot->SetHorizontalAlignment(HAlign_Fill);
	LineSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));
	for (int32 Index = 0; Index < RowCount; ++Index)
	{
		FRobberStoryRowUI Row = MakeStoryRow(RowFont);
		UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row.Row);
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
		RowSlot->SetPadding(FMargin(0.f, 2.f));
		Ui.Rows.Add(Row);
	}
	Ui.Footer = StoryLabel(WidgetTree, StoryFont(TEXT("F_PatrickHand"), 19.f), StoryInk);
	UVerticalBox* FooterStack = WidgetTree->ConstructWidget<UVerticalBox>();
	USizeBox* Rule = StoryRule(WidgetTree, FLinearColor(StoryInk.R, StoryInk.G, StoryInk.B, 0.5f), 2.f);
	UVerticalBoxSlot* RuleSlot = FooterStack->AddChildToVerticalBox(Rule);
	RuleSlot->SetHorizontalAlignment(HAlign_Fill);
	RuleSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 4.f));
	FooterStack->AddChildToVerticalBox(Ui.Footer);
	Column->AddChildToVerticalBox(FooterStack)->SetHorizontalAlignment(HAlign_Fill);
	Ui.FooterPanel = FooterStack;

	UBorder* Face = StoryBox(WidgetTree, Paper, FMargin(14.f, 18.f, 14.f, 12.f), Column);
	UBorder* Ink = StoryBox(WidgetTree, StoryInk, FMargin(3.f), Face);

	USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>();
	Sized->SetWidthOverride(Width);
	UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>();
	UImage* Shadow = StorySolid(WidgetTree, StoryInk);
	Shadow->SetRenderTranslation(FVector2D(4., 4.));
	StoryFill(Stack, Shadow);
	StoryFill(Stack, Ink);

	UBorder* Pin = WidgetTree->ConstructWidget<UBorder>();
	Pin->SetBrush(FSlateRoundedBoxBrush(FLinearColor(FColor(0xd6, 0x33, 0x2a)), 10.f, StoryInk, 2.f));
	USizeBox* PinSize = WidgetTree->ConstructWidget<USizeBox>();
	PinSize->SetWidthOverride(20.f);
	PinSize->SetHeightOverride(20.f);
	PinSize->SetContent(Pin);
	PinSize->SetRenderTranslation(FVector2D(0., -10.));
	UOverlaySlot* PinSlot = Stack->AddChildToOverlay(PinSize);
	PinSlot->SetHorizontalAlignment(HAlign_Center);
	PinSlot->SetVerticalAlignment(VAlign_Top);

	Sized->SetContent(Stack);
	Sized->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Ui.Root = Sized;
	return Ui;
}

void URobberCaseFileHUD::BuildStoryWidgets(UCanvasPanel* Root)
{
	StoryNoteUi = MakeStoryNote(FLinearColor(FColor(0xff, 0xe9, 0x8a)), 3, 244.f);
	StoryErrandUi = MakeStoryNote(FLinearColor(FColor(0xd8, 0xf0, 0xc8)), 3, 244.f);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StoryColumn"));
	Column->SetVisibility(ESlateVisibility::HitTestInvisible);
	Column->AddChildToVerticalBox(StoryNoteUi.Root)->SetHorizontalAlignment(HAlign_Left);
	UVerticalBoxSlot* ErrandSlot = Column->AddChildToVerticalBox(StoryErrandUi.Root);
	ErrandSlot->SetHorizontalAlignment(HAlign_Left);
	ErrandSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
	Column->SetRenderTransformPivot(FVector2D(0.f, 0.f));
	Column->SetRenderScale(FVector2D(StoryScale));
	UCanvasPanelSlot* ColumnSlot = Root->AddChildToCanvas(Column);
	ColumnSlot->SetAutoSize(true);
	ColumnSlot->SetZOrder(5);
	StoryColumn = Column;

	StoryNoteUi.Root->SetRenderTransformAngle(1.6f);
	StoryErrandUi.Root->SetRenderTransformAngle(-1.2f);
	Show(StoryColumn, false);

	// The stamp: a double ring of ink round a paper card.
	UVerticalBox* StampText = WidgetTree->ConstructWidget<UVerticalBox>();
	StampKick = StoryLabel(WidgetTree, StoryFont(TEXT("F_Bangers"), 92.f), StoryRed);
	StampKick->SetJustification(ETextJustify::Center);
	StampTitle = StoryLabel(WidgetTree, StoryFont(TEXT("F_Fredoka"), 40.f, TEXT("Bold")), StoryInk);
	StampTitle->SetJustification(ETextJustify::Center);
	StampSubtitle = StoryLabel(WidgetTree, StoryFont(TEXT("F_Fredoka"), 22.f), StoryInk);
	StampSubtitle->SetJustification(ETextJustify::Center);
	StampSubtitle->SetAutoWrapText(true);
	StampText->AddChildToVerticalBox(StampKick)->SetHorizontalAlignment(HAlign_Center);
	StampText->AddChildToVerticalBox(StampTitle)->SetHorizontalAlignment(HAlign_Center);
	UVerticalBoxSlot* SubSlot = StampText->AddChildToVerticalBox(StampSubtitle);
	SubSlot->SetHorizontalAlignment(HAlign_Fill);
	SubSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

	UBorder* Card = StoryBox(WidgetTree, StoryPaper, FMargin(30.f, 16.f), StampText);
	StampInnerEdge = StoryBox(WidgetTree, StoryRed, FMargin(3.f), Card);
	UBorder* Gap = StoryBox(WidgetTree, StoryPaper, FMargin(4.f), StampInnerEdge);
	StampEdge = StoryBox(WidgetTree, StoryRed, FMargin(3.f), Gap);

	UOverlay* StampStack = WidgetTree->ConstructWidget<UOverlay>();
	UImage* StampShadow = StorySolid(WidgetTree, StoryInk);
	StampShadow->SetRenderTranslation(FVector2D(8., 8.));
	StoryFill(StampStack, StampShadow);
	StoryFill(StampStack, StampEdge);
	USizeBox* StampSized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StoryStamp"));
	StampSized->SetWidthOverride(720.f);
	StampSized->SetContent(StampStack);
	StampSized->SetVisibility(ESlateVisibility::Collapsed);
	StampSized->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	StampSized->SetRenderTransformAngle(-4.f);
	UCanvasPanelSlot* StampSlot = Root->AddChildToCanvas(StampSized);
	StampSlot->SetAnchors(FAnchors(0.5f, 0.f));
	StampSlot->SetAlignment(FVector2D(0.5f, 0.f));
	StampSlot->SetAutoSize(true);
	StampSlot->SetPosition(FVector2D(0.f, 170.f));
	StampSlot->SetZOrder(20);
	StampRoot = StampSized;
}

void URobberCaseFileHUD::OnStoryStamp(bool bComplete, const FText& Title, const FText& Subtitle)
{
	if (!StampRoot)
	{
		return;
	}
	bStampComplete = bComplete;
	StampClock = 0.f;
	const FLinearColor Colour = bComplete ? StoryGreen : StoryRed;
	StampEdge->SetBrush(FSlateColorBrush(Colour));
	StampInnerEdge->SetBrush(FSlateColorBrush(Colour));
	StampKick->SetColorAndOpacity(FSlateColor(Colour));
	StampKick->SetText(bComplete ? NSLOCTEXT("RobberCaseFile", "QuestComplete", "QUEST COMPLETE") : NSLOCTEXT("RobberCaseFile", "NewQuest", "NEW QUEST"));
	StampTitle->SetText(Title);
	StampSubtitle->SetText(Subtitle);
	StampRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URobberCaseFileHUD::FillStoryNote(FRobberStoryNoteUI& Ui, const FText& Title, const TArray<FRobberStoryLine>& Lines, const FText& Footer)
{
	SetText(Ui.Title, Title);
	for (int32 Index = 0; Index < Ui.Rows.Num(); ++Index)
	{
		const FRobberStoryRowUI& Row = Ui.Rows[Index];
		const bool bUsed = Lines.IsValidIndex(Index);
		Show(Row.Row, bUsed);
		if (bUsed)
		{
			const FRobberStoryLine& Line = Lines[Index];
			SetText(Row.Text, Line.Text);
			Row.Text->SetColorAndOpacity(FSlateColor(Line.bDone ? StoryDone : Line.bWarning ? StoryRed : StoryInk));
			Show(Row.Bullet, !Line.bDone);
			Show(Row.Tick, Line.bDone);
			Show(Row.Strike, Line.bDone);
		}
	}
	Show(Ui.FooterPanel, !Footer.IsEmpty());
	SetText(Ui.Footer, Footer);
}

void URobberCaseFileHUD::UpdateStory(float DeltaTime)
{
	const APlayerController* PC = GetOwningPlayer();
	UPlayerQuestComponent* Quests = PC ? PC->FindComponentByClass<UPlayerQuestComponent>() : nullptr;
	if (!StoryColumn || !Quests)
	{
		return;
	}
	if (!bStoryBound)
	{
		bStoryBound = true;
		Quests->OnStoryStamp.AddUObject(this, &URobberCaseFileHUD::OnStoryStamp);
	}

	// Under the job note, whose height changes with its list.
	if (const UWidget* JobNote = Find(TEXT("NoteSize")))
	{
		if (UCanvasPanelSlot* ColumnSlot = Cast<UCanvasPanelSlot>(StoryColumn->Slot))
		{
			ColumnSlot->SetPosition(FVector2D(CornerMargin + 14.f, CornerMargin + JobNote->GetDesiredSize().Y * NoteScale + 2.f));
		}
	}

	// --- Stamp: slams in, shakes the screen, holds, then goes. ---
	constexpr float SlamTime = 0.13f, ShakeTime = 0.2f, HoldTime = 1.9f, FadeTime = 0.28f;
	const bool bStampRunning = StampClock >= 0.f;
	if (bStampRunning)
	{
		StampClock += DeltaTime;
		const float Time = StampClock;
		FVector2D Shake = FVector2D::ZeroVector;
		float Scale = 1.f;
		float Opacity = 1.f;
		FVector2D Rise = FVector2D::ZeroVector;
		if (Time < SlamTime)
		{
			const float Alpha = Time / SlamTime;
			Scale = FMath::Lerp(3.2f, 1.f, Alpha * Alpha);
			Opacity = Alpha;
		}
		else if (Time < SlamTime + ShakeTime)
		{
			const float Alpha = (Time - SlamTime) / ShakeTime;
			const float Fade = 1.f - Alpha;
			Shake = FVector2D(FMath::Sin(Alpha * 22.f) * 9.f, FMath::Cos(Alpha * 19.f) * 7.f) * Fade;
		}
		else if (Time > SlamTime + HoldTime)
		{
			const float Alpha = FMath::Clamp((Time - SlamTime - HoldTime) / FadeTime, 0.f, 1.f);
			Opacity = 1.f - Alpha;
			if (bStampComplete)
			{
				Rise = FVector2D(0., -30. * Alpha);
			}
		}
		StampRoot->SetRenderScale(FVector2D(Scale * StoryScale));
		StampRoot->SetRenderOpacity(Opacity);
		StampRoot->SetRenderTranslation(Rise);
		if (UWidget* Root = GetRootWidget())
		{
			Root->SetRenderTranslation(Shake);
		}
		if (Time >= SlamTime + HoldTime + FadeTime)
		{
			StampClock = -1.f;
			StampRoot->SetVisibility(ESlateVisibility::Collapsed);
			if (UWidget* Root = GetRootWidget())
			{
				Root->SetRenderTranslation(FVector2D::ZeroVector);
			}
			if (!bStampComplete)
			{
				NotePopClock = 0.f;
			}
		}
	}

	// --- The notes: held back until a NEW QUEST stamp has gone, and dropped when the quest is over. ---
	const FRobberStoryNote& Note = Quests->StoryNote;
	const bool bHeldBack = StampClock >= 0.f && !bStampComplete;
	const bool bWanted = Note.bShown && !bHeldBack;
	if (bWanted)
	{
		NoteDropClock = -1.f;
		StoryColumn->SetRenderOpacity(1.f);
		StoryColumn->SetRenderTranslation(FVector2D::ZeroVector);
		StoryColumn->SetRenderTransformAngle(0.f);
		FillStoryNote(StoryNoteUi, Note.Title, Note.Lines, Note.Footer);
		const bool bErrand = !Note.ErrandTitle.IsEmpty();
		Show(StoryErrandUi.Root, bErrand);
		if (bErrand)
		{
			FillStoryNote(StoryErrandUi, Note.ErrandTitle, Note.ErrandLines, FText::GetEmpty());
		}
		Show(StoryColumn, true);
		bStoryNoteUp = true;
	}
	else if (bStoryNoteUp && !Note.bShown)
	{
		bStoryNoteUp = false;
		NoteDropClock = 0.f;
	}
	else if (!bStoryNoteUp && NoteDropClock < 0.f)
	{
		Show(StoryColumn, false);
	}
	if (NotePopClock >= 0.f)
	{
		NotePopClock += DeltaTime;
		const float Alpha = FMath::Clamp(NotePopClock / 0.16f, 0.f, 1.f);
		StoryNoteUi.Root->SetRenderScale(FVector2D(FMath::Lerp(1.12f, 1.f, Alpha)));
		if (Alpha >= 1.f)
		{
			NotePopClock = -1.f;
		}
	}
	if (NoteDropClock >= 0.f)
	{
		NoteDropClock += DeltaTime;
		const float Alpha = FMath::Clamp(NoteDropClock / 0.65f, 0.f, 1.f);
		const float Eased = Alpha * Alpha;
		StoryColumn->SetRenderTranslation(FVector2D(0., 240. * Eased));
		StoryColumn->SetRenderTransformAngle(18.f * Eased);
		StoryColumn->SetRenderOpacity(1.f - Alpha);
		if (Alpha >= 1.f)
		{
			NoteDropClock = -1.f;
			Show(StoryColumn, false);
		}
	}
}

namespace
{
	const FLinearColor MarkerYellow = FLinearColor::FromSRGBColor(FColor(255, 210, 63));
	const FLinearColor MarkerInk = FLinearColor::FromSRGBColor(FColor(35, 32, 26));
	constexpr int32 MaxPins = 4;
	constexpr int32 MaxBubbles = 8;
	constexpr double BubbleRange = 800.;
	constexpr double PinFadeNear = 400.;
	constexpr double PinFadeFar = 600.;
	constexpr double FloorHeight = 500.;

	UBorder* MarkerBox(UWidgetTree* Tree, const FLinearColor& Fill, float Radius, const FMargin& Padding, UWidget* Content)
	{
		UBorder* Box = Tree->ConstructWidget<UBorder>();
		Box->SetBrush(FSlateRoundedBoxBrush(Fill, Radius, FLinearColor::Black, 2.f));
		Box->SetPadding(Padding);
		Box->SetContent(Content);
		return Box;
	}

	void PlaceOnCanvas(UWidget* Widget, const FVector2D& Position)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			CanvasSlot->SetPosition(Position);
		}
	}
}

void URobberCaseFileHUD::BuildMarkerWidgets(UCanvasPanel* Root)
{
	for (int32 Index = 0; Index < MaxPins; ++Index)
	{
		FRobberPinUI Pin;
		UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>();
		Pin.Name = StoryLabel(WidgetTree, StoryFont(TEXT("F_Fredoka"), 18.f, TEXT("Bold")), MarkerInk);
		Pin.Detail = StoryLabel(WidgetTree, StoryFont(TEXT("F_Fredoka"), 15.f), MarkerInk);
		Pin.Name->SetJustification(ETextJustify::Center);
		Pin.Detail->SetJustification(ETextJustify::Center);
		Text->AddChildToVerticalBox(Pin.Name);
		Text->AddChildToVerticalBox(Pin.Detail);

		USizeBox* DiamondSize = WidgetTree->ConstructWidget<USizeBox>();
		DiamondSize->SetWidthOverride(22.f);
		DiamondSize->SetHeightOverride(22.f);
		DiamondSize->SetContent(MarkerBox(WidgetTree, MarkerYellow, 2.f, FMargin(0.f), nullptr));
		DiamondSize->SetRenderTransformAngle(45.f);

		UTextBlock* Arrow = StoryLabel(WidgetTree, StoryFont(TEXT("F_Bangers"), 18.f), MarkerInk);
		Arrow->SetText(FText::FromString(TEXT(">")));
		Arrow->SetJustification(ETextJustify::Center);
		Pin.Arrow = Arrow;

		UOverlay* Mark = WidgetTree->ConstructWidget<UOverlay>();
		StoryFill(Mark, DiamondSize);
		UOverlaySlot* ArrowSlot = Mark->AddChildToOverlay(Arrow);
		ArrowSlot->SetHorizontalAlignment(HAlign_Center);
		ArrowSlot->SetVerticalAlignment(VAlign_Center);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		UVerticalBoxSlot* TagSlot = Column->AddChildToVerticalBox(MarkerBox(WidgetTree, MarkerYellow, 5.f, FMargin(9.f, 4.f), Text));
		TagSlot->SetHorizontalAlignment(HAlign_Center);
		UVerticalBoxSlot* MarkSlot = Column->AddChildToVerticalBox(Mark);
		MarkSlot->SetHorizontalAlignment(HAlign_Center);
		MarkSlot->SetPadding(FMargin(0.f, 7.f, 0.f, 0.f));
		Column->SetVisibility(ESlateVisibility::Collapsed);

		UCanvasPanelSlot* PinSlot = Root->AddChildToCanvas(Column);
		PinSlot->SetAutoSize(true);
		PinSlot->SetAlignment(FVector2D(0.5f, 1.f));
		PinSlot->SetZOrder(-5);
		Pin.Root = Column;
		Pins.Add(Pin);
	}

	for (int32 Index = 0; Index < MaxBubbles; ++Index)
	{
		UTextBlock* Dots = StoryLabel(WidgetTree, StoryFont(TEXT("F_Fredoka"), 26.f, TEXT("Bold")), MarkerInk);
		Dots->SetText(FText::FromString(TEXT("...")));
		Dots->SetJustification(ETextJustify::Center);
		UBorder* Bubble = MarkerBox(WidgetTree, FLinearColor::White, 10.f, FMargin(12.f, 0.f, 12.f, 6.f), Dots);

		USizeBox* TailSize = WidgetTree->ConstructWidget<USizeBox>();
		TailSize->SetWidthOverride(9.f);
		TailSize->SetHeightOverride(9.f);
		TailSize->SetContent(StorySolid(WidgetTree, FLinearColor::White));
		TailSize->SetRenderTransformAngle(45.f);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Column->AddChildToVerticalBox(Bubble);
		UVerticalBoxSlot* TailSlot = Column->AddChildToVerticalBox(TailSize);
		TailSlot->SetHorizontalAlignment(HAlign_Center);
		// Tucked up behind the bubble so only its point shows.
		TailSlot->SetPadding(FMargin(0.f, -5.f, 0.f, 0.f));
		Column->SetVisibility(ESlateVisibility::Collapsed);

		UCanvasPanelSlot* BubbleSlot = Root->AddChildToCanvas(Column);
		BubbleSlot->SetAutoSize(true);
		BubbleSlot->SetAlignment(FVector2D(0.5f, 1.f));
		BubbleSlot->SetZOrder(-6);
		Bubbles.Add(Column);
	}

	// Looking out through the gaps in whatever you're inside. Fractions of the screen, top to bottom.
	UCanvasPanel* View = WidgetTree->ConstructWidget<UCanvasPanel>();
	const FVector2D Bands[] = { { 0.f, 0.34f }, { 0.38f, 0.44f }, { 0.505f, 0.56f }, { 0.60f, 1.f } };
	for (const FVector2D& Band : Bands)
	{
		UCanvasPanelSlot* BandSlot = View->AddChildToCanvas(StorySolid(WidgetTree, FLinearColor(0.004f, 0.003f, 0.002f)));
		BandSlot->SetAnchors(FAnchors(0.f, Band.X, 1.f, Band.Y));
		BandSlot->SetOffsets(FMargin(0.f));
	}
	UTextBlock* Title = StoryLabel(WidgetTree, StoryFont(TEXT("F_Bangers"), 46.f), MarkerYellow);
	Title->SetText(NSLOCTEXT("RobberTour", "HiddenTitle", "HIDDEN"));
	UCanvasPanelSlot* TitleSlot = View->AddChildToCanvas(Title);
	TitleSlot->SetAutoSize(true);
	TitleSlot->SetAnchors(FAnchors(0.5f, 0.13f));
	TitleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	HidingName = StoryLabel(WidgetTree, StoryFont(TEXT("F_SpecialElite"), 22.f), FLinearColor(0.75f, 0.72f, 0.62f));
	UCanvasPanelSlot* NameSlot = View->AddChildToCanvas(HidingName);
	NameSlot->SetAutoSize(true);
	NameSlot->SetAnchors(FAnchors(0.5f, 0.19f));
	NameSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	View->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* ViewSlot = Root->AddChildToCanvas(View);
	ViewSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	ViewSlot->SetOffsets(FMargin(0.f));
	ViewSlot->SetZOrder(-10);
	HidingView = View;
}

void URobberCaseFileHUD::UpdateMarkers(const FGeometry& MyGeometry, float DeltaTime)
{
	APlayerController* PC = GetOwningPlayer();
	const APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
	const ARobberGameState* State = GetRobberState();
	if (!Camera || !State)
	{
		return;
	}
	SinceMarkerScan += DeltaTime;
	if (SinceMarkerScan >= 0.5f)
	{
		SinceMarkerScan = 0.f;
		MarkerTargets.Reset();
		RobberMarkers::Gather(State, PC->PlayerState, MarkerTargets);
	}
	const FVector2D Size = MyGeometry.GetLocalSize();
	const FVector Eye = Camera->GetCameraLocation();
	const FRotationMatrix View(Camera->GetCameraRotation());
	const AProductionGameCharacter* Robber = Cast<AProductionGameCharacter>(PC->GetPawn());
	const bool bHiding = Robber && Robber->IsHiding();

	TArray<TPair<UWidget*, FVector2D>> Placed;
	for (int32 Index = 0; Index < Pins.Num(); ++Index)
	{
		const FRobberPinUI& Pin = Pins[Index];
		const bool bHasTarget = !bHiding && MarkerTargets.IsValidIndex(Index);
		const double Distance = bHasTarget ? FVector::Dist(Eye, MarkerTargets[Index].Location) : 0.;
		const float Fade = bHasTarget ? FMath::GetMappedRangeValueClamped(FVector2D(PinFadeNear, PinFadeFar), FVector2D(0.f, 1.f), Distance) : 0.f;
		Pin.Root->SetVisibility(Fade > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Fade <= 0.f)
		{
			continue;
		}
		const FRobberMarkerTarget& Target = MarkerTargets[Index];
		Pin.Root->SetRenderOpacity(Fade);
		Pin.Name->SetText(Target.Label);

		const int32 Metres = FMath::RoundToInt32(Distance / 100.);
		const int32 Floors = FMath::RoundToInt32((Target.Location.Z - Eye.Z) / FloorHeight);
		Pin.Detail->SetText(Floors == 0
			? FText::Format(NSLOCTEXT("RobberMarker", "Metres", "{0} m"), Metres)
			: FText::Format(Floors > 0 ? NSLOCTEXT("RobberMarker", "MetresUp", "{0} m, {1} {1}|plural(one=floor,other=floors) up") : NSLOCTEXT("RobberMarker", "MetresDown", "{0} m, {1} {1}|plural(one=floor,other=floors) down"), Metres, FMath::Abs(Floors)));

		const float Margin = 80.f;
		FVector2D Position;
		const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Target.Location, Position, true);
		const bool bOnScreen = bProjected && FVector::DotProduct(Target.Location - Eye, View.GetUnitAxis(EAxis::X)) > 0.
			&& Position.X > Margin && Position.X < Size.X - Margin && Position.Y > Margin && Position.Y < Size.Y - Margin;
		Pin.Arrow->SetVisibility(bOnScreen ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (!bOnScreen)
		{
			// Off screen or behind: slide to the screen edge in the direction you'd turn. Camera axes, because a point behind you projects mirrored.
			const FVector Delta = Target.Location - Eye;
			FVector2D Direction(FVector::DotProduct(Delta, View.GetUnitAxis(EAxis::Y)), -FVector::DotProduct(Delta, View.GetUnitAxis(EAxis::Z)));
			if (!Direction.Normalize())
			{
				Direction = FVector2D(0.f, 1.f);
			}
			const FVector2D Half = Size * 0.5f;
			const FVector2D Room = Half - FVector2D(Margin * 1.6f, Margin * 1.2f);
			const double Scale = FMath::Min(Direction.X != 0. ? Room.X / FMath::Abs(Direction.X) : DBL_MAX, Direction.Y != 0. ? Room.Y / FMath::Abs(Direction.Y) : DBL_MAX);
			Position = Half + Direction * Scale;
			Pin.Arrow->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)));
		}
		Placed.Add({ Pin.Root.Get(), Position });
	}

	// Targets in the same direction would otherwise sit exactly on top of each other, most of all at the screen edge.
	Placed.Sort([](const TPair<UWidget*, FVector2D>& A, const TPair<UWidget*, FVector2D>& B) { return A.Value.Y < B.Value.Y; });
	const float PinHeight = 78.f;
	const float PinWidth = 190.f;
	for (int32 Index = 0; Index < Placed.Num(); ++Index)
	{
		for (int32 Earlier = 0; Earlier < Index; ++Earlier)
		{
			if (FMath::Abs(Placed[Index].Value.X - Placed[Earlier].Value.X) < PinWidth && Placed[Index].Value.Y - Placed[Earlier].Value.Y < PinHeight)
			{
				Placed[Index].Value.Y = Placed[Earlier].Value.Y + PinHeight;
			}
		}
		PlaceOnCanvas(Placed[Index].Key, Placed[Index].Value);
	}
}

void URobberCaseFileHUD::UpdateExhibitBubbles(const FGeometry& MyGeometry, float DeltaTime)
{
	APlayerController* PC = GetOwningPlayer();
	const APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
	const ARobberGameState* State = GetRobberState();
	if (!Camera || !State)
	{
		return;
	}
	SinceBubbleScan += DeltaTime;
	if (SinceBubbleScan >= 0.25f)
	{
		SinceBubbleScan = 0.f;
		BubbleExhibits.Reset();
		const AProductionGameCharacter* Robber = Cast<AProductionGameCharacter>(PC->GetPawn());
		if (State->CanTalkToExhibits() && !RobberStage::IsInLobby(State->GetGameStage()) && !(Robber && Robber->IsHiding()))
		{
			const FVector Eye = Camera->GetCameraLocation();
			TArray<TPair<double, AActor*>> Near;
			for (TActorIterator<AQuestDialogueCharacter> It(GetWorld()); It; ++It)
			{
				const double Distance = FVector::Dist(Eye, It->GetActorLocation());
				if (Distance > BubbleRange)
				{
					continue;
				}
				FCollisionQueryParams Params(SCENE_QUERY_STAT(ExhibitBubble), false, PC->GetPawn());
				Params.AddIgnoredActor(*It);
				if (!GetWorld()->LineTraceTestByChannel(Eye, It->GetActorLocation(), ECC_Visibility, Params))
				{
					Near.Add({ Distance, *It });
				}
			}
			Near.Sort([](const TPair<double, AActor*>& A, const TPair<double, AActor*>& B) { return A.Key < B.Key; });
			for (int32 Index = 0; Index < FMath::Min(Near.Num(), MaxBubbles); ++Index)
			{
				BubbleExhibits.Add(Near[Index].Value);
			}
		}
	}
	for (int32 Index = 0; Index < Bubbles.Num(); ++Index)
	{
		const AActor* Exhibit = BubbleExhibits.IsValidIndex(Index) ? BubbleExhibits[Index].Get() : nullptr;
		FVector2D Position;
		bool bShow = false;
		if (Exhibit)
		{
			FVector Centre;
			FVector Extent;
			Exhibit->GetActorBounds(true, Centre, Extent);
			const FVector Top(Centre.X, Centre.Y, Centre.Z + Extent.Z + 25.);
			bShow = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Top, Position, true)
				&& FVector::DotProduct(Top - Camera->GetCameraLocation(), Camera->GetCameraRotation().Vector()) > 0.;
		}
		Bubbles[Index]->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShow)
		{
			PlaceOnCanvas(Bubbles[Index], Position);
		}
	}
}

void URobberCaseFileHUD::UpdateHidingView()
{
	const AProductionGameCharacter* Robber = Cast<AProductionGameCharacter>(GetOwningPlayer() ? GetOwningPlayer()->GetPawn() : nullptr);
	const ARobberHidingSpot* Spot = Robber ? Robber->GetHidingSpot() : nullptr;
	if (HidingView)
	{
		HidingView->SetVisibility(Spot ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Spot && HidingName)
	{
		HidingName->SetText(Spot->GetDisplayLabel());
	}
}

void URobberCaseFileHUD::UpdateSprint(float DeltaTime)
{
	const AProductionGameCharacter* Character = Cast<AProductionGameCharacter>(GetOwningPlayerPawn());
	UWidget* Panel = Find(TEXT("SprintPanel"));
	UProgressBar* Bar = Find<UProgressBar>(TEXT("SprintBar"));
	if (Bar)
	{
		Bar->SetPercent(Character ? Character->GetStamina() : 1.f);
		Bar->SetFillColorAndOpacity(Character && Character->IsWinded() ? WindedColour : SprintColour);
	}
	if (Panel)
	{
		// Out of the way while full; back as soon as it's being used.
		const bool bWanted = Character && (Character->GetStamina() < 0.999f || Character->IsSprinting());
		const float Target = bWanted ? 1.f : 0.f;
		Panel->SetRenderOpacity(FMath::FInterpConstantTo(Panel->GetRenderOpacity(), Target, DeltaTime, bWanted ? 6.f : 1.5f));
	}
}

void URobberCaseFileHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateSprint(InDeltaTime);
	UpdateStory(InDeltaTime);
	const ARobberGameState* State = GetRobberState();
	if (!State)
	{
		return;
	}
	UpdateMarkers(MyGeometry, InDeltaTime);
	UpdateExhibitBubbles(MyGeometry, InDeltaTime);
	UpdateHidingView();
	UpdateNote(State);
	UpdatePaper(State);
	UpdateCrew(State);

	const ARobberController* Robber = GetRobber();
	const bool bMessage = Robber && Robber->IsHeistMessageVisible();
	Show(Find(TEXT("ToastPanel")), bMessage);
	if (bMessage)
	{
		SetText(Find<UTextBlock>(TEXT("ToastText")), Robber->GetHeistMessage());
	}
	const URobberJournalComponent* Journal = GetJournal();
	const int32 Unread = Journal ? Journal->GetUnreadCount() : 0;
	Show(Find(TEXT("JournalNew")), Unread > 0);
	SetText(Find<UTextBlock>(TEXT("JournalNewText")), FText::Format(NSLOCTEXT("RobberCaseFile", "NewCount", "{0} new"), Unread));
}

void URobberCaseFileHUD::UpdateNote(const ARobberGameState* State)
{
	const uint8 Stage = State->GetGameStage();
	FText Title;
	switch (Stage)
	{
	case RobberStage::BreakIn: Title = NSLOCTEXT("RobberCaseFile", "TitleBreakIn", "GETTING IN"); break;
	case RobberStage::Investigate: Title = NSLOCTEXT("RobberCaseFile", "TitleInvestigate", "SNEAK ABOUT"); break;
	case RobberStage::Heist: Title = NSLOCTEXT("RobberCaseFile", "TitleHeist", "HEIST TIME!"); break;
	case RobberStage::Escape: Title = NSLOCTEXT("RobberCaseFile", "TitleEscape", "GET OUT!"); break;
	default: Title = NSLOCTEXT("RobberCaseFile", "TitlePlan", "THE PLAN"); break;
	}
	SetText(Find<UTextBlock>(TEXT("TitleText")), Title);
	SetText(Find<UTextBlock>(TEXT("NightText")), NightName(State));
	SetText(Find<UTextBlock>(TEXT("ObjectiveText")), GetOwningPlayer() ? State->GetObjectiveText(GetOwningPlayer()->PlayerState) : FText::GetEmpty());

	// Rows: the jobs, or in the lobby the vote counts (no tick box).
	struct FRow { FText Text; bool bDone; bool bBox; };
	TArray<FRow> Rows;
	if (RobberStage::IsInLobby(Stage))
	{
		TArray<const ARobberEntryPoint*> Counted;
		for (const FRobberEntryVote& Vote : State->GetVotes())
		{
			if (Vote.Entry && !Counted.Contains(Vote.Entry))
			{
				Counted.Add(Vote.Entry);
				Rows.Add({ FText::Format(NSLOCTEXT("RobberCaseFile", "VoteRow", "{0}: {1} {1}|plural(one=vote,other=votes)"), Vote.Entry->DisplayName, State->GetVoteCount(Vote.Entry)), false, false });
			}
		}
	}
	else
	{
		for (const FRobberJob& Job : State->GetJobs())
		{
			Rows.Add({ Job.Title, Job.bDone, true });
		}
	}
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const bool bRow = Rows.IsValidIndex(Index);
		Show(Find(FString::Printf(TEXT("JobRow%d"), Index)), bRow);
		if (!bRow)
		{
			continue;
		}
		UBorder* Box = Find<UBorder>(FString::Printf(TEXT("JobBox%d"), Index));
		Show(Box, Rows[Index].bBox);
		if (Box)
		{
			Box->SetBrushColor(Rows[Index].bDone ? TickColour : FLinearColor::White);
		}
		UTextBlock* Text = Find<UTextBlock>(FString::Printf(TEXT("JobText%d"), Index));
		SetText(Text, Rows[Index].Text);
		if (Text)
		{
			Text->SetColorAndOpacity(Rows[Index].bDone ? DoneTextColour : TextColour);
		}
	}

	const float TimeLeft = State->GetPhaseTimeRemaining();
	Show(Find(TEXT("TimerText")), TimeLeft >= 0.f);
	SetText(Find<UTextBlock>(TEXT("TimerText")), FText::Format(NSLOCTEXT("RobberCaseFile", "Closing", "CLOSING IN {0}"), Clock(TimeLeft)));

	const FText Code = GetCodeLine();
	Show(Find(TEXT("CodeRow")), !Code.IsEmpty());
	SetText(Find<UTextBlock>(TEXT("CodeText")), Code);
}

void URobberCaseFileHUD::UpdatePaper(const ARobberGameState* State)
{
	const int32 Level = HeatLevel(State);
	Show(Find(TEXT("PaperPanel")), !RobberStage::IsInLobby(State->GetGameStage()));
	SetText(Find<UTextBlock>(TEXT("DatelineText")), FText::Format(NSLOCTEXT("RobberCaseFile", "Dateline", "{0} · LATE EDITION"), FText::FromString(NightName(State).ToString().ToUpper())));
	SetText(Find<UTextBlock>(TEXT("HeatText")), FText::Format(NSLOCTEXT("RobberCaseFile", "HeatN", "HEAT {0}"), FMath::RoundToInt(State->GetHeat())));
	if (UProgressBar* Fuse = Find<UProgressBar>(TEXT("FuseBar")))
	{
		Fuse->SetPercent(State->GetHeat() / ARobberGameState::MaxHeat);
		Fuse->SetFillColorAndOpacity(HeatColour(Level));
	}
	SetText(Find<UTextBlock>(TEXT("HeadlineText")), Headline(Level));
	SetText(Find<UTextBlock>(TEXT("SublineText")), Subline(Level));

	const float Guards = State->GetGuardsTimeRemaining();
	Show(Find(TEXT("GuardsStamp")), Guards >= 0.f);
	SetText(Find<UTextBlock>(TEXT("GuardsText")), FText::Format(NSLOCTEXT("RobberCaseFile", "Guards", "GUARDS {0}"), Clock(Guards)));
}

void URobberCaseFileHUD::UpdateCrew(const ARobberGameState* State)
{
	const TArray<APlayerState*> Crew = SortedCrew(State);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const APlayerState* Player = Crew.IsValidIndex(Index) ? Crew[Index] : nullptr;
		Show(Find(FString::Printf(TEXT("CrewCard%d"), Index)), Player != nullptr);
		if (!Player)
		{
			continue;
		}
		if (UBorder* Colour = Find<UBorder>(FString::Printf(TEXT("CrewColour%d"), Index)))
		{
			Colour->SetBrushColor(CrewColour(Player));
		}
		SetText(Find<UTextBlock>(FString::Printf(TEXT("CrewName%d"), Index)), PlayerName(Player));
		SetText(Find<UTextBlock>(FString::Printf(TEXT("CrewStatus%d"), Index)), CrewStatus(State, Player));
	}
}

// --- Journal entry ---

void URobberJournalEntryWidget::SetEntry(const FRobberJournalEntry& Entry, bool bNew, float Now)
{
	SetText(Find<UTextBlock>(TEXT("SpeakerText")), Entry.Speaker);
	const int32 Minutes = FMath::FloorToInt(FMath::Max(0.f, Now - Entry.HeardAt) / 60.f);
	SetText(Find<UTextBlock>(TEXT("WhenText")), Minutes < 1 ? NSLOCTEXT("RobberJournal", "JustNow", "just now") : FText::Format(NSLOCTEXT("RobberJournal", "MinAgo", "{0} min ago"), Minutes));
	Show(Find(TEXT("QuoteText")), !Entry.Quote.IsEmpty());
	SetText(Find<UTextBlock>(TEXT("QuoteText")), FText::Format(NSLOCTEXT("RobberJournal", "Quote", "\"{0}\""), Entry.Quote));
	Show(Find(TEXT("ClueText")), !Entry.Clue.IsEmpty());
	SetText(Find<UTextBlock>(TEXT("ClueText")), Entry.Clue);
	Show(Find(TEXT("NewStamp")), bNew);

	TArray<FString> Words;
	Entry.Speaker.ToString().ParseIntoArrayWS(Words);
	FString Initials;
	for (const FString& Word : Words)
	{
		if (Initials.Len() < 2 && !Word.IsEmpty() && FChar::IsAlpha(Word[0]))
		{
			Initials += FChar::ToUpper(Word[0]);
		}
	}
	SetText(Find<UTextBlock>(TEXT("InitialsText")), FText::FromString(Initials.IsEmpty() ? TEXT("?") : Initials));
}

// --- Journal ---

void URobberJournalWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UButton* Tab = Find<UButton>(TEXT("TabClues")))
	{
		Tab->OnClicked.AddUniqueDynamic(this, &URobberJournalWidget::OnCluesClicked);
	}
	if (UButton* Tab = Find<UButton>(TEXT("TabCrew")))
	{
		Tab->OnClicked.AddUniqueDynamic(this, &URobberJournalWidget::OnCrewClicked);
	}
	if (UButton* Tab = Find<UButton>(TEXT("TabNight")))
	{
		Tab->OnClicked.AddUniqueDynamic(this, &URobberJournalWidget::OnNightClicked);
	}
	if (!EntryClass)
	{
		EntryClass = LoadClass<URobberJournalEntryWidget>(nullptr, TEXT("/Game/OurStuff/UI/CaseFile/WBP_JournalEntry.WBP_JournalEntry_C"));
	}
}

void URobberJournalWidget::OnCluesClicked() { ShowTab(0); }
void URobberJournalWidget::OnCrewClicked() { ShowTab(1); }
void URobberJournalWidget::OnNightClicked() { ShowTab(2); }

void URobberJournalWidget::ShowTab(int32 Index)
{
	if (UWidgetSwitcher* Switcher = Find<UWidgetSwitcher>(TEXT("TabSwitcher")))
	{
		Switcher->SetActiveWidgetIndex(Index);
	}
	// The open tab sits a little higher, like a pulled-up file tab.
	const TCHAR* Tabs[] = { TEXT("TabClues"), TEXT("TabCrew"), TEXT("TabNight") };
	for (int32 Each = 0; Each < UE_ARRAY_COUNT(Tabs); ++Each)
	{
		if (UWidget* Tab = Find(Tabs[Each]))
		{
			Tab->SetRenderTranslation(FVector2D(0.f, Each == Index ? -6.f : 0.f));
		}
	}
}

void URobberJournalWidget::OnOpened()
{
	if (UWidget* Book = Find(TEXT("Book")))
	{
		Book->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Book->SetRenderScale(FVector2D(BookScale));
	}
	if (URobberJournalComponent* Journal = GetJournal())
	{
		OpenedReadRevision = Journal->GetReadRevision();
		Journal->MarkAllRead();
	}
	Refresh();
}

void URobberJournalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// "2 min ago", crew whereabouts and newly heard entries change while it's open.
	RefreshIn -= InDeltaTime;
	if (RefreshIn <= 0.f)
	{
		Refresh();
	}
}

void URobberJournalWidget::Refresh()
{
	RefreshIn = 0.5f;
	const ARobberGameState* State = GetRobberState();
	const URobberJournalComponent* Journal = GetJournal();
	const float Now = State ? State->GetServerWorldTimeSeconds() : 0.f;
	SetText(Find<UTextBlock>(TEXT("NightText")), NightName(State));

	// --- OVERHEARD ---
	TArray<FRobberJournalEntry> Entries = Journal ? Journal->GetEntries() : TArray<FRobberJournalEntry>();
	Entries.Sort([](const FRobberJournalEntry& A, const FRobberJournalEntry& B) { return A.Revision > B.Revision; });
	if (UPanelWidget* List = Find<UPanelWidget>(TEXT("EntryList")); List && EntryClass)
	{
		while (EntryWidgets.Num() < Entries.Num())
		{
			URobberJournalEntryWidget* Row = CreateWidget<URobberJournalEntryWidget>(GetOwningPlayer(), EntryClass);
			if (!Row)
			{
				break;
			}
			List->AddChild(Row);
			EntryWidgets.Add(Row);
		}
		for (int32 Index = 0; Index < EntryWidgets.Num(); ++Index)
		{
			const bool bUsed = Entries.IsValidIndex(Index);
			Show(EntryWidgets[Index], bUsed);
			if (bUsed)
			{
				EntryWidgets[Index]->SetEntry(Entries[Index], Entries[Index].Revision > OpenedReadRevision, Now);
			}
		}
	}
	Show(Find(TEXT("EmptyText")), Entries.IsEmpty());
	int32 Unasked = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const FName Key = It->GetFName();
			Unasked += It->GetLevel() != World->PersistentLevel && It->FindComponentByClass<UDialogueComponent>()
				&& !(Journal && Journal->HasAsked(Key));
		}
	}
	Show(Find(TEXT("UnaskedText")), Unasked > 0);
	SetText(Find<UTextBlock>(TEXT("UnaskedText")), FText::Format(NSLOCTEXT("RobberJournal", "Unasked", "??? · {0} {0}|plural(one=exhibit,other=exhibits) tonight you haven't asked yet."), Unasked));

	// --- CLUES ---
	const TArray<int32> Digits = Journal ? Journal->GetVaultDigits() : TArray<int32>();
	const TArray<FText> Sources = Journal ? Journal->GetDigitSources() : TArray<FText>();
	Show(Find(TEXT("VaultSection")), !Digits.IsEmpty() || VaultStillShut(State));
	for (int32 Index = 0; Index < ARobberVaultDoor::CodeLength; ++Index)
	{
		const bool bKnown = Digits.IsValidIndex(Index) && Digits[Index] != INDEX_NONE;
		SetText(Find<UTextBlock>(FString::Printf(TEXT("Digit%d"), Index)), bKnown ? FText::AsNumber(Digits[Index]) : FText::FromString(TEXT("?")));
		SetText(Find<UTextBlock>(FString::Printf(TEXT("DigitFrom%d"), Index)), bKnown && Sources.IsValidIndex(Index) ? Sources[Index] : NSLOCTEXT("RobberJournal", "NobodyYet", "nobody yet"));
		if (UBorder* Box = Find<UBorder>(FString::Printf(TEXT("DigitBox%d"), Index)))
		{
			Box->SetBrushColor(bKnown ? KnownDigitColour : UnknownDigitColour);
		}
	}
	const UPlayerQuestComponent* Quests = GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<UPlayerQuestComponent>() : nullptr;
	TArray<FString> Keys;
	for (const FName Key : Quests ? Quests->Keys : TArray<FName>())
	{
		Keys.Add(FText::Format(NSLOCTEXT("RobberJournal", "KeyRow", "{0} · in your pocket"), CaseFileKeyName(Key)).ToString());
	}
	SetText(Find<UTextBlock>(TEXT("KeysText")), Keys.IsEmpty() ? NSLOCTEXT("RobberJournal", "NoKeys", "No keys yet.") : FText::FromString(FString::Join(Keys, TEXT("\n"))));

	// --- CREW ---
	const TArray<APlayerState*> Crew = SortedCrew(State);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const APlayerState* Player = Crew.IsValidIndex(Index) ? Crew[Index] : nullptr;
		Show(Find(FString::Printf(TEXT("CrewRow%d"), Index)), Player != nullptr);
		if (!Player)
		{
			continue;
		}
		if (UBorder* Colour = Find<UBorder>(FString::Printf(TEXT("CrewColour%d"), Index)))
		{
			Colour->SetBrushColor(CrewColour(Player));
		}
		SetText(Find<UTextBlock>(FString::Printf(TEXT("CrewName%d"), Index)), PlayerName(Player));
		SetText(Find<UTextBlock>(FString::Printf(TEXT("CrewStatus%d"), Index)), CrewStatus(State, Player));
	}

	// --- NIGHT ---
	TArray<FString> Jobs;
	if (State)
	{
		for (const FRobberJob& Job : State->GetJobs())
		{
			Jobs.Add((Job.bDone ? TEXT("[x] ") : TEXT("[  ] ")) + Job.Title.ToString());
		}
	}
	const FText JobsText = !Jobs.IsEmpty() ? FText::FromString(FString::Join(Jobs, TEXT("\n")))
		: State && GetOwningPlayer() ? State->GetObjectiveText(GetOwningPlayer()->PlayerState) : FText::GetEmpty();
	SetText(Find<UTextBlock>(TEXT("JobsText")), JobsText);
	SetText(Find<UTextBlock>(TEXT("PaperLineText")), FText::Format(NSLOCTEXT("RobberJournal", "PaperLine", "HEAT {0} · today's paper: \"{1}\""),
		FMath::RoundToInt(State ? State->GetHeat() : 0.f), Headline(HeatLevel(State))));
}
