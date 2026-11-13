#include "Robber/RobberAchievementPopup.h"

#include "AchievementDefinitionAsset.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "RobberMenu/RobberMenuWidgets.h"

// Sizes are the board mock-up's at 1080p, about three-quarters of the mock-up's 1920 px width.
namespace
{
	const FLinearColor Ink(FColor(0x1b, 0x1b, 0x1b));
	const FLinearColor Red(FColor(0xc0, 0x39, 0x2b));
	const FLinearColor Newsprint(FColor(0xf4, 0xef, 0xe1));
	const FLinearColor ProofPaper(FColor(0xf1, 0xea, 0xd6));
	const FLinearColor ProofBlue(FColor(0x3c, 0x78, 0xc8));
	const FLinearColor ScrapPaper(FColor(0xe8, 0xe0, 0xc9));
	const FLinearColor SecretPurple(FColor(0x23, 0x1f, 0x5c));
	const FLinearColor SecretLabel(FColor(0xd0, 0xbf, 0xff));
	const FLinearColor PicBack(FColor(0xde, 0xd7, 0xc1));
	const FLinearColor PicYellow(FColor(0xff, 0xd4, 0x3b));

	constexpr float EdgeGap = 56.f;
	constexpr float FrontWidth = 520.f;
	constexpr float FrontIn = 1.4f;
	constexpr float FrontHold = 4.f;
	constexpr float DraftWidth = 430.f;
	constexpr float DraftIn = 1.8f;
	constexpr float DraftHold = 3.f;
	constexpr float SecretWidth = 660.f;
	constexpr float SecretHold = 4.f;
	constexpr float ExitTime = 0.45f;

	/** Pixels at 1080p; a UMG font size is in points. */
	FSlateFontInfo Font(const TCHAR* Name, float Pixels)
	{
		const FString Path = FString::Printf(TEXT("/Game/OurStuff/UI/CaseFile/Fonts/%s.%s"), Name, Name);
		return FSlateFontInfo(LoadObject<UFont>(nullptr, *Path), Pixels * 0.75f, TEXT("Regular"));
	}

	UImage* Solid(UWidgetTree* Tree, const FLinearColor& Colour)
	{
		UImage* Image = Tree->ConstructWidget<UImage>();
		Image->SetColorAndOpacity(Colour);
		return Image;
	}

	/** A line of fixed thickness that stretches along its slot. An image's own desired size doesn't hold in a slot. */
	USizeBox* Rule(UWidgetTree* Tree, const FLinearColor& Colour, float Height)
	{
		USizeBox* Line = Tree->ConstructWidget<USizeBox>();
		Line->SetHeightOverride(Height);
		Line->SetContent(Solid(Tree, Colour));
		return Line;
	}

	UTextBlock* Label(UWidgetTree* Tree, const FSlateFontInfo& InFont, const FLinearColor& Colour, bool bWrap = false)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetFont(InFont);
		Text->SetColorAndOpacity(FSlateColor(Colour));
		Text->SetAutoWrapText(bWrap);
		return Text;
	}

	UBorder* Box(UWidgetTree* Tree, const FLinearColor& Colour, const FMargin& Padding, UWidget* Content)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>();
		Border->SetBrush(FSlateColorBrush(Colour));
		Border->SetPadding(Padding);
		Border->SetContent(Content);
		return Border;
	}

	UOverlaySlot* Fill(UOverlay* Stack, UWidget* Child)
	{
		UOverlaySlot* NewSlot = Stack->AddChildToOverlay(Child);
		NewSlot->SetHorizontalAlignment(HAlign_Fill);
		NewSlot->SetVerticalAlignment(VAlign_Fill);
		return NewSlot;
	}

	/** Content over a hard offset shadow, the Case File's printed-card look. */
	UOverlay* Shadowed(UWidgetTree* Tree, UWidget* Content, const FLinearColor& ShadowColour, const FVector2D& Offset)
	{
		UOverlay* Layers = Tree->ConstructWidget<UOverlay>();
		UImage* Shadow = Solid(Tree, ShadowColour);
		Shadow->SetRenderTranslation(Offset);
		Fill(Layers, Shadow);
		Fill(Layers, Content);
		return Layers;
	}

	/** The achievement's picture in an inked frame. */
	UBorder* Picture(UWidgetTree* Tree, const FLinearColor& Back, float Size, UImage*& OutImage)
	{
		OutImage = Tree->ConstructWidget<UImage>();
		USizeBox* Sized = Tree->ConstructWidget<USizeBox>();
		Sized->SetWidthOverride(Size);
		Sized->SetHeightOverride(Size);
		Sized->SetContent(OutImage);
		return Box(Tree, Ink, FMargin(4.f), Box(Tree, Back, FMargin(Size * 0.12f), Sized));
	}

	UVerticalBoxSlot* Stack(UVerticalBox* Column, UWidget* Child, float Below)
	{
		UVerticalBoxSlot* NewSlot = Column->AddChildToVerticalBox(Child);
		NewSlot->SetHorizontalAlignment(HAlign_Fill);
		NewSlot->SetPadding(FMargin(0.f, 0.f, 0.f, Below));
		return NewSlot;
	}

	UCanvasPanelSlot* Place(UCanvasPanel* Root, UWidget* Child, const FVector2D& Anchor, const FVector2D& Position)
	{
		UCanvasPanelSlot* NewSlot = Root->AddChildToCanvas(Child);
		NewSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		NewSlot->SetAlignment(Anchor);
		NewSlot->SetAutoSize(true);
		NewSlot->SetPosition(Position);
		NewSlot->SetZOrder(10);
		return NewSlot;
	}

	float Clamp01(float Value) { return FMath::Clamp(Value, 0.f, 1.f); }
	float EaseOut(float T) { return 1.f - FMath::Cube(1.f - Clamp01(T)); }

	/** Overshoots a little before settling, like the mock-up's cubic-bezier(.2, .8, .3, 1.15). */
	float EaseOutBack(float T)
	{
		const float U = Clamp01(T) - 1.f;
		constexpr float C1 = 1.2f;
		return 1.f + (C1 + 1.f) * U * U * U + C1 * U * U;
	}

	/** A rubber stamp or cut-out letter hitting the page: 0 -> 1.35 -> 1. */
	float SlapScale(float T)
	{
		T = Clamp01(T);
		return T < 0.6f ? FMath::Lerp(0.f, 1.35f, T / 0.6f) : FMath::Lerp(1.35f, 1.f, (T - 0.6f) / 0.4f);
	}

	float SlapOpacity(float T) { return Clamp01(T / 0.6f); }

	void ShowWidget(UWidget* Widget, bool bShown)
	{
		if (Widget)
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	void SetPicture(UWidget* Frame, UImage* Image, const TSoftObjectPtr<UTexture2D>& Texture)
	{
		UTexture2D* Loaded = Texture.LoadSynchronous();
		ShowWidget(Frame, Loaded != nullptr);
		if (Loaded && Image)
		{
			Image->SetBrushFromTexture(Loaded, false);
		}
	}
}

void URobberAchievementPopup::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Build();
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URobberAchievementPopup::Build()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PopupRoot"));
	WidgetTree->RootWidget = Root;

	// The front page's camera flash, over the whole screen.
	Flash = Solid(WidgetTree, FLinearColor::White);
	UCanvasPanelSlot* FlashSlot = Root->AddChildToCanvas(Flash);
	FlashSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	FlashSlot->SetOffsets(FMargin(0.f));
	ShowWidget(Flash, false);

	BuildFrontPage(Root);
	BuildDraftProof(Root);
	BuildSecret(Root);
}

void URobberAchievementPopup::BuildFrontPage(UCanvasPanel* Root)
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();

	UTextBlock* Mast = Label(WidgetTree, Font(TEXT("F_PlayfairDisplay"), 46.f), Ink);
	Mast->SetText(NSLOCTEXT("RobberAchievementPopup", "Masthead", "The Museum Morning"));
	Mast->SetJustification(ETextJustify::Center);
	Stack(Column, Mast, 8.f);
	Stack(Column, Rule(WidgetTree, Ink, 3.f), 3.f);
	Stack(Column, Rule(WidgetTree, Ink, 3.f), 12.f);

	UTextBlock* Kicker = Label(WidgetTree, Font(TEXT("F_SpecialElite"), 23.f), Red);
	Kicker->SetText(NSLOCTEXT("RobberAchievementPopup", "FrontKicker", "EXTRA! IN PRINT"));
	Stack(Column, Kicker, 6.f);

	FrontTitle = Label(WidgetTree, Font(TEXT("F_PlayfairDisplay"), 60.f), Ink, true);
	Stack(Column, FrontTitle, 12.f);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UImage* Image = nullptr;
	UBorder* Pic = Picture(WidgetTree, PicBack, 96.f, Image);
	FrontPic = Pic;
	FrontImage = Image;
	Row->AddChildToHorizontalBox(Pic)->SetVerticalAlignment(VAlign_Top);
	FrontBody = Label(WidgetTree, Font(TEXT("F_SpecialElite"), 26.f), Ink, true);
	UHorizontalBoxSlot* BodySlot = Row->AddChildToHorizontalBox(FrontBody);
	BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BodySlot->SetPadding(FMargin(18.f, 0.f, 0.f, 0.f));
	Stack(Column, Row, 0.f);

	UBorder* Paper = Box(WidgetTree, Newsprint, FMargin(26.f, 20.f, 26.f, 26.f), Column);
	UBorder* Edge = Box(WidgetTree, Ink, FMargin(6.f), Paper);

	USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FrontPage"));
	Sized->SetWidthOverride(FrontWidth);
	Sized->SetContent(Shadowed(WidgetTree, Edge, Ink, FVector2D(16.f, 16.f)));
	Sized->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Place(Root, Sized, FVector2D(1.f, 0.f), FVector2D(-EdgeGap, EdgeGap));
	ShowWidget(Sized, false);
	FrontRoot = Sized;
}

void URobberAchievementPopup::BuildDraftProof(UCanvasPanel* Root)
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();

	UTextBlock* Proof = Label(WidgetTree, Font(TEXT("F_SpecialElite"), 18.f), ProofBlue);
	Proof->SetText(NSLOCTEXT("RobberAchievementPopup", "GalleyProof", "GALLEY PROOF"));
	Stack(Column, Proof, 7.f);
	Stack(Column, Rule(WidgetTree, ProofBlue.CopyWithNewOpacity(0.6f), 1.f), 9.f);

	UTextBlock* Kicker = Label(WidgetTree, Font(TEXT("F_SpecialElite"), 22.f), Red);
	Kicker->SetText(NSLOCTEXT("RobberAchievementPopup", "DraftKicker", "DEVELOPING STORY"));
	Stack(Column, Kicker, 6.f);

	DraftTitle = Label(WidgetTree, Font(TEXT("F_PlayfairDisplay"), 42.f), Ink, true);
	Stack(Column, DraftTitle, 8.f);
	DraftBody = Label(WidgetTree, Font(TEXT("F_SpecialElite"), 22.f), Ink, true);
	Stack(Column, DraftBody, 12.f);

	// The editor's red pen along a pencilled track, then the count circled.
	UHorizontalBox* Pen = WidgetTree->ConstructWidget<UHorizontalBox>();
	UOverlay* Track = WidgetTree->ConstructWidget<UOverlay>();
	UOverlaySlot* PencilSlot = Track->AddChildToOverlay(Rule(WidgetTree, Ink.CopyWithNewOpacity(0.35f), 2.f));
	PencilSlot->SetHorizontalAlignment(HAlign_Fill);
	PencilSlot->SetVerticalAlignment(VAlign_Center);
	DraftInk = WidgetTree->ConstructWidget<USizeBox>();
	DraftInk->SetHeightOverride(7.f);
	DraftInk->SetWidthOverride(0.f);
	UImage* InkLine = WidgetTree->ConstructWidget<UImage>();
	InkLine->SetBrush(FSlateRoundedBoxBrush(Red, 3.5f));
	DraftInk->SetContent(InkLine);
	UOverlaySlot* InkSlot = Track->AddChildToOverlay(DraftInk);
	InkSlot->SetHorizontalAlignment(HAlign_Left);
	InkSlot->SetVerticalAlignment(VAlign_Center);
	DraftTrack = Track;
	UHorizontalBoxSlot* TrackSlot = Pen->AddChildToHorizontalBox(Track);
	TrackSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TrackSlot->SetVerticalAlignment(VAlign_Fill);

	DraftCount = Label(WidgetTree, Font(TEXT("F_SpecialElite"), 25.f), Red);
	UBorder* Circle = WidgetTree->ConstructWidget<UBorder>();
	Circle->SetBrush(FSlateRoundedBoxBrush(FLinearColor::Transparent, FVector4(24.f, 24.f, 24.f, 24.f), Red, 3.f));
	Circle->SetPadding(FMargin(14.f, 8.f));
	Circle->SetContent(DraftCount);
	Circle->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UHorizontalBoxSlot* CircleSlot = Pen->AddChildToHorizontalBox(Circle);
	CircleSlot->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
	CircleSlot->SetVerticalAlignment(VAlign_Center);
	DraftCircle = Circle;
	Stack(Column, Pen, 0.f);

	// The page: faint blue layout lines and a red margin rule behind the type, a DRAFT stamp on top.
	UOverlay* Page = WidgetTree->ConstructWidget<UOverlay>();
	Fill(Page, Solid(WidgetTree, ProofPaper));

	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>();
	for (int32 Index = 0; Index < 24; ++Index)
	{
		Stack(Lines, Rule(WidgetTree, ProofBlue.CopyWithNewOpacity(0.16f), 1.f), 19.f);
	}
	// Desired height 1 so the lines never make the page taller; they're clipped to whatever height the type needs.
	USizeBox* Grid = WidgetTree->ConstructWidget<USizeBox>();
	Grid->SetMaxDesiredHeight(1.f);
	Grid->SetClipping(EWidgetClipping::ClipToBounds);
	Grid->SetContent(Lines);
	Fill(Page, Grid);

	USizeBox* Margin = WidgetTree->ConstructWidget<USizeBox>();
	Margin->SetWidthOverride(4.f);
	Margin->SetContent(Solid(WidgetTree, Red.CopyWithNewOpacity(0.55f)));
	UOverlaySlot* MarginSlot = Page->AddChildToOverlay(Margin);
	MarginSlot->SetHorizontalAlignment(HAlign_Left);
	MarginSlot->SetVerticalAlignment(VAlign_Fill);

	Fill(Page, Column)->SetPadding(FMargin(34.f, 16.f, 28.f, 22.f));

	UTextBlock* StampText = Label(WidgetTree, Font(TEXT("F_Bangers"), 34.f), Red);
	StampText->SetText(NSLOCTEXT("RobberAchievementPopup", "DraftStamp", "DRAFT"));
	UBorder* Stamp = WidgetTree->ConstructWidget<UBorder>();
	Stamp->SetBrush(FSlateRoundedBoxBrush(FLinearColor::Transparent, FVector4(3.f, 3.f, 3.f, 3.f), Red, 4.f));
	Stamp->SetPadding(FMargin(14.f, 4.f, 14.f, 2.f));
	Stamp->SetContent(StampText);
	Stamp->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UOverlaySlot* StampSlot = Page->AddChildToOverlay(Stamp);
	StampSlot->SetHorizontalAlignment(HAlign_Right);
	StampSlot->SetVerticalAlignment(VAlign_Top);
	StampSlot->SetPadding(FMargin(0.f, 8.f, 18.f, 0.f));
	DraftStamp = Stamp;

	USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DraftProof"));
	Sized->SetWidthOverride(DraftWidth);
	Sized->SetContent(Shadowed(WidgetTree, Page, FLinearColor(0.f, 0.f, 0.f, 0.6f), FVector2D(11.f, 12.f)));
	Sized->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	// Sits above the HUD's JOURNAL tab in the bottom-right corner.
	Place(Root, Sized, FVector2D(1.f, 1.f), FVector2D(-EdgeGap, -150.f));
	ShowWidget(Sized, false);
	DraftRoot = Sized;
}

void URobberAchievementPopup::BuildSecret(UCanvasPanel* Root)
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();

	UTextBlock* Tag = Label(WidgetTree, Font(TEXT("F_SpecialElite"), 22.f), SecretLabel);
	Tag->SetText(NSLOCTEXT("RobberAchievementPopup", "SecretTag", "OFF THE RECORD · SECRET FOUND"));
	Stack(Column, Tag, 14.f);

	SecretLetters = WidgetTree->ConstructWidget<UWrapBox>();
	SecretLetters->SetInnerSlotPadding(FVector2D(22.f, 8.f));
	SecretLetters->SetExplicitWrapSize(true);
	SecretLetters->SetWrapSize(SecretWidth - 64.f);
	Stack(Column, SecretLetters, 18.f);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UImage* Image = nullptr;
	UBorder* Pic = Picture(WidgetTree, PicYellow, 80.f, Image);
	Pic->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Pic->SetRenderTransformAngle(-4.f);
	SecretPic = Pic;
	SecretImage = Image;
	Row->AddChildToHorizontalBox(Pic)->SetVerticalAlignment(VAlign_Center);
	SecretBody = Label(WidgetTree, Font(TEXT("F_SpecialElite"), 25.f), Ink, true);
	UHorizontalBoxSlot* BodySlot = Row->AddChildToHorizontalBox(SecretBody);
	BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BodySlot->SetVerticalAlignment(VAlign_Center);
	BodySlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	UBorder* ScrapRow = Box(WidgetTree, ScrapPaper, FMargin(16.f, 14.f), Row);
	ScrapRow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	ScrapRow->SetRenderTransformAngle(1.f);
	Stack(Column, ScrapRow, 0.f);

	UBorder* Card = Box(WidgetTree, SecretPurple, FMargin(26.f, 22.f, 26.f, 26.f), Column);
	UBorder* Edge = Box(WidgetTree, Ink, FMargin(6.f), Card);

	USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SecretCard"));
	Sized->SetWidthOverride(SecretWidth);
	Sized->SetContent(Shadowed(WidgetTree, Edge, Ink, FVector2D(16.f, 16.f)));
	Sized->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Place(Root, Sized, FVector2D(0.5f, 0.f), FVector2D(0.f, EdgeGap));
	ShowWidget(Sized, false);
	SecretRoot = Sized;
}

void URobberAchievementPopup::Enqueue(UAchievementDefinitionAsset* Asset, const FAchievementProgressState& State, bool bProgress)
{
	if (!Asset)
	{
		return;
	}
	FEntry& Entry = Queue.AddDefaulted_GetRef();
	Entry.Asset = Asset;
	Entry.State = State;
	Entry.Kind = bProgress ? ERobberPopupKind::DraftProof : Asset->bSecret ? ERobberPopupKind::Secret : ERobberPopupKind::FrontPage;
}

FName URobberAchievementPopup::GetShowingId() const
{
	const UAchievementDefinitionAsset* Asset = Showing.Asset.Get();
	return Showing.Kind != ERobberPopupKind::None && Asset ? Asset->AchievementId : NAME_None;
}

void URobberAchievementPopup::Start(const FEntry& Entry)
{
	Showing = Entry;
	Clock = 0.f;
	ShowWidget(FrontRoot, false);
	ShowWidget(DraftRoot, false);
	ShowWidget(SecretRoot, false);
	ShowWidget(Flash, false);

	const UAchievementDefinitionAsset* Asset = Entry.Asset.Get();
	if (!Asset)
	{
		Showing.Kind = ERobberPopupKind::None;
		return;
	}
	const FText Title = FText::FromString(Asset->Title.ToString().ToUpper());
	const FText Earned = Asset->EarnedText.IsEmpty() ? Asset->Description : Asset->EarnedText;

	switch (Entry.Kind)
	{
	case ERobberPopupKind::FrontPage:
		FrontTitle->SetText(Title);
		FrontBody->SetText(Earned);
		SetPicture(FrontPic, FrontImage, Asset->UnlockedImage);
		ShowWidget(FrontRoot, true);
		break;

	case ERobberPopupKind::DraftProof:
	{
		const int32 Required = FMath::Max(1, Asset->RequiredAmount);
		const int32 Current = FMath::Clamp(Entry.State.CurrentAmount, 0, Required);
		InkTo = static_cast<float>(Current) / Required;
		// Only the stretch since the last quarter draws in, so the player sees it go up.
		InkFrom = FMath::Clamp((FMath::FloorToFloat(InkTo * 4.f) - 1.f) / 4.f, 0.f, InkTo);
		DraftTitle->SetText(Title);
		DraftBody->SetText(Asset->Description);
		DraftCount->SetText(FText::Format(NSLOCTEXT("RobberAchievementPopup", "DraftCount", "{0}/{1}"), Current, Required));
		ShowWidget(DraftRoot, true);
		break;
	}

	case ERobberPopupKind::Secret:
	{
		RobberRansom::Fill(SecretLetters, Asset->Title.ToString(), 34, 50.f, this);
		Letters.Reset();
		for (UWidget* Word : SecretLetters->GetAllChildren())
		{
			if (const UPanelWidget* WordBox = Cast<UPanelWidget>(Word))
			{
				for (UWidget* Letter : WordBox->GetAllChildren())
				{
					Letter->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
					Letters.Add(Letter);
				}
			}
		}
		SecretBody->SetText(Earned);
		SetPicture(SecretPic, SecretImage, Asset->UnlockedImage);
		ShowWidget(SecretRoot, true);
		break;
	}

	default:
		break;
	}
}

float URobberAchievementPopup::Duration() const
{
	switch (Showing.Kind)
	{
	case ERobberPopupKind::FrontPage:
		return FrontIn + FrontHold + ExitTime;
	case ERobberPopupKind::DraftProof:
		return DraftIn + DraftHold + ExitTime;
	case ERobberPopupKind::Secret:
	{
		const float LettersDone = 0.3f + 0.08f * FMath::Max(0, Letters.Num() - 1) + 0.3f;
		return FMath::Max(LettersDone, 0.4f) + SecretHold + ExitTime;
	}
	default:
		return 0.f;
	}
}

void URobberAchievementPopup::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Showing.Kind == ERobberPopupKind::None)
	{
		if (Queue.Num() == 0)
		{
			return;
		}
		const FEntry Next = Queue[0];
		Queue.RemoveAt(0);
		Start(Next);
	}
	else
	{
		Clock += InDeltaTime;
		if (Clock >= Duration())
		{
			ShowWidget(FrontRoot, false);
			ShowWidget(DraftRoot, false);
			ShowWidget(SecretRoot, false);
			ShowWidget(Flash, false);
			Showing = FEntry();
			return;
		}
	}
	Animate(MyGeometry.GetLocalSize());
}

void URobberAchievementPopup::Animate(const FVector2D& ScreenSize)
{
	switch (Showing.Kind)
	{
	case ERobberPopupKind::FrontPage: AnimateFrontPage(ScreenSize); break;
	case ERobberPopupKind::DraftProof: AnimateDraftProof(); break;
	case ERobberPopupKind::Secret: AnimateSecret(); break;
	default: break;
	}
}

void URobberAchievementPopup::AnimateFrontPage(const FVector2D& ScreenSize)
{
	FVector2D Size = FrontRoot->GetDesiredSize();
	if (Size.X < 1.f)
	{
		Size = FVector2D(FrontWidth, 380.f);
	}
	// Spins out of the middle of the screen, three turns, and lands in the corner a touch too big.
	const FVector2D Landed(ScreenSize.X - EdgeGap - Size.X * 0.5f, EdgeGap + Size.Y * 0.5f);
	const FVector2D From = ScreenSize * 0.5f - Landed;
	const float SpinTime = FrontIn * 0.75f;

	FVector2D Offset = FVector2D::ZeroVector;
	float Scale = 1.f;
	float Angle = -3.f;
	if (Clock < SpinTime)
	{
		const float E = EaseOut(Clock / SpinTime);
		Offset = From * (1.f - E);
		Scale = FMath::Lerp(0.05f, 1.12f, E);
		Angle = FMath::Lerp(-1080.f, -3.f, E);
	}
	else if (Clock < FrontIn)
	{
		Scale = FMath::Lerp(1.12f, 1.f, (Clock - SpinTime) / (FrontIn - SpinTime));
	}
	const float Out = Clamp01((Clock - (Duration() - ExitTime)) / ExitTime);
	Offset.X += (Size.X + EdgeGap + 40.f) * Out * Out;
	FrontRoot->SetRenderTransform(FWidgetTransform(Offset, FVector2D(Scale), FVector2D::ZeroVector, Angle));

	// The flash goes off just as it lands.
	const float F = Clock / FrontIn;
	const float FlashOpacity = F < 0.7f ? 0.f : F < 0.76f ? (F - 0.7f) / 0.06f * 0.35f : F < 1.f ? 0.35f * (1.f - (F - 0.76f) / 0.24f) : 0.f;
	ShowWidget(Flash, FlashOpacity > 0.f);
	Flash->SetRenderOpacity(FlashOpacity);
}

void URobberAchievementPopup::AnimateDraftProof()
{
	const float E = EaseOutBack(Clock / 0.8f);
	const float Out = Clamp01((Clock - (Duration() - ExitTime)) / ExitTime);
	const FVector2D Offset(FMath::Lerp(DraftWidth * 1.3f, 0.f, E) + (DraftWidth + EdgeGap + 40.f) * Out * Out, 0.f);
	DraftRoot->SetRenderTransform(FWidgetTransform(Offset, FVector2D(1.f), FVector2D::ZeroVector, FMath::Lerp(6.f, -0.5f, E)));

	const float StampT = (Clock - 0.5f) / 0.3f;
	DraftStamp->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(SlapScale(StampT)), FVector2D::ZeroVector, -8.f));
	DraftStamp->SetRenderOpacity(0.85f * SlapOpacity(StampT));

	const float TrackWidth = DraftTrack->GetCachedGeometry().GetLocalSize().X;
	const float Drawn = FMath::Lerp(InkFrom, InkTo, EaseOut((Clock - 0.7f) / 0.9f));
	DraftInk->SetWidthOverride((TrackWidth > 1.f ? TrackWidth : 260.f) * Drawn);

	const float CircleT = (Clock - 1.5f) / 0.3f;
	DraftCircle->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(SlapScale(CircleT)), FVector2D::ZeroVector, -6.f));
	DraftCircle->SetRenderOpacity(SlapOpacity(CircleT));
}

void URobberAchievementPopup::AnimateSecret()
{
	const float In = Clamp01(Clock / 0.4f);
	const float Out = Clamp01((Clock - (Duration() - ExitTime)) / ExitTime);
	const FVector2D Offset(0.f, FMath::Lerp(-80.f, 0.f, EaseOut(In)) - 40.f * Out);
	SecretRoot->SetRenderTransform(FWidgetTransform(Offset, FVector2D(1.f), FVector2D::ZeroVector, -1.5f));
	SecretRoot->SetRenderOpacity(In * (1.f - Out));

	for (int32 Index = 0; Index < Letters.Num(); ++Index)
	{
		const float T = (Clock - (0.3f + 0.08f * Index)) / 0.3f;
		Letters[Index]->SetRenderScale(FVector2D(SlapScale(T)));
		Letters[Index]->SetRenderOpacity(SlapOpacity(T));
	}
}
