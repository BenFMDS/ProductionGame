#include "Heist/RobberMarker.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Heist/RobberHighlight.h"

namespace
{
	// The map markers' colours (RobberCaseFileUI.cpp), so the "!" reads as part of the same HUD.
	const FLinearColor MarkYellow = FLinearColor::FromSRGBColor(FColor(255, 210, 63));
	const FLinearColor MarkInk = FLinearColor::FromSRGBColor(FColor(35, 32, 26));

	FSlateFontInfo MarkFont(const TCHAR* Name, float Pixels, FName Typeface = TEXT("Regular"))
	{
		const FString Path = FString::Printf(TEXT("/Game/OurStuff/UI/CaseFile/Fonts/%s.%s"), Name, Name);
		const UObject* Font = LoadObject<UFont>(nullptr, *Path);
		return FSlateFontInfo(Font, Pixels * 0.75f, Typeface);
	}

	UBorder* MarkBox(UWidgetTree* Tree, float Radius, const FMargin& Padding, UWidget* Content)
	{
		UBorder* Box = Tree->ConstructWidget<UBorder>();
		Box->SetBrush(FSlateRoundedBoxBrush(MarkYellow, Radius, MarkInk, 2.f));
		Box->SetPadding(Padding);
		Box->SetHorizontalAlignment(HAlign_Center);
		Box->SetVerticalAlignment(VAlign_Center);
		Box->SetContent(Content);
		return Box;
	}
}

void URobberMarkerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MarkerColumn"));
	WidgetTree->RootWidget = Column;

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MarkerName"));
	NameText->SetFont(MarkFont(TEXT("F_Fredoka"), 20.f, TEXT("Bold")));
	NameText->SetColorAndOpacity(FSlateColor(MarkInk));
	NameText->SetJustification(ETextJustify::Center);
	Tag = MarkBox(WidgetTree, 5.f, FMargin(10.f, 4.f), NameText);
	Tag->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
	Tag->SetVisibility(ESlateVisibility::Hidden);
	UVerticalBoxSlot* TagSlot = Column->AddChildToVerticalBox(Tag);
	TagSlot->SetHorizontalAlignment(HAlign_Center);

	UTextBlock* Bang = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MarkerBang"));
	Bang->SetFont(MarkFont(TEXT("F_Bangers"), 30.f));
	Bang->SetColorAndOpacity(FSlateColor(MarkInk));
	Bang->SetJustification(ETextJustify::Center);
	Bang->SetText(FText::FromString(TEXT("!")));
	USizeBox* BadgeSize = WidgetTree->ConstructWidget<USizeBox>();
	BadgeSize->SetWidthOverride(34.f);
	BadgeSize->SetHeightOverride(34.f);
	BadgeSize->SetContent(MarkBox(WidgetTree, 17.f, FMargin(0.f, 2.f, 0.f, 0.f), Bang));
	Badge = BadgeSize;
	Badge->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UVerticalBoxSlot* BadgeSlot = Column->AddChildToVerticalBox(Badge);
	BadgeSlot->SetHorizontalAlignment(HAlign_Center);
	// Room for the bob, so the badge never leaves the widget's own box.
	BadgeSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 8.f));

	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URobberMarkerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Age += InDeltaTime;
	if (Badge)
	{
		Badge->SetRenderTranslation(FVector2D(0.f, FMath::Sin(Age * 3.f) * 4.f));
	}
	if (Tag && bNameShown)
	{
		// Pops up from the badge with a little overshoot.
		NameShownFor += InDeltaTime;
		const float T = FMath::Clamp(NameShownFor / 0.18f, 0.f, 1.f);
		const float Scale = T < 1.f ? FMath::Lerp(0.6f, 1.08f, T) : FMath::FInterpTo(Tag->GetRenderTransform().Scale.X, 1.f, InDeltaTime, 12.f);
		Tag->SetRenderScale(FVector2D(Scale));
	}
}

void URobberMarkerWidget::SetName(const FText& Name)
{
	if (NameText)
	{
		NameText->SetText(Name);
	}
}

FText URobberMarkerWidget::GetName() const
{
	return NameText ? NameText->GetText() : FText::GetEmpty();
}

void URobberMarkerWidget::SetNameShown(bool bShown)
{
	if (bShown == bNameShown || !Tag)
	{
		return;
	}
	bNameShown = bShown;
	NameShownFor = 0.f;
	// Hidden, not collapsed: the badge stays where it is instead of dropping into the tag's place.
	Tag->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

URobberMarkerComponent::URobberMarkerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	SetPivot(FVector2D(0.5f, 1.f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetWidgetClass(URobberMarkerWidget::StaticClass());
}

void URobberMarkerComponent::BeginPlay()
{
	PlaceAboveOwner();
	Super::BeginPlay();
	if (URobberMarkerWidget* Marker = GetMarker())
	{
		Marker->SetName(Label);
	}
	SetVisibility(false);
}

void URobberMarkerComponent::PlaceAboveOwner()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	// Colliding parts only: the old floating name text would otherwise lift the mark off into the air.
	FVector Centre;
	FVector Extent;
	Owner->GetActorBounds(true, Centre, Extent);
	if (Extent.IsNearlyZero())
	{
		Owner->GetActorBounds(false, Centre, Extent);
	}
	SetWorldLocation(FVector(Centre.X, Centre.Y, Centre.Z + Extent.Z + Gap));
}

URobberMarkerWidget* URobberMarkerComponent::GetMarker() const
{
	return Cast<URobberMarkerWidget>(GetUserWidgetObject());
}

void URobberMarkerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APlayerController* Viewer = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	// A dedicated server's first controller is someone else's: it has no view of its own.
	if (Viewer && !Viewer->IsLocalController())
	{
		Viewer = nullptr;
	}
	const bool bShowMark = bMarkerEnabled && IsWithin(Viewer, MarkRange);
	if (IsVisible() != bShowMark)
	{
		SetVisibility(bShowMark);
	}
	if (URobberMarkerWidget* Marker = GetMarker())
	{
		if (!Marker->GetName().EqualTo(Label))
		{
			Marker->SetName(Label);
		}
		Marker->SetNameShown(bShowMark && !Label.IsEmpty() && IsWithin(Viewer, NameRange));
	}
}

bool URobberMarkerComponent::IsWithin(const APlayerController* Viewer, double Range) const
{
	if (bNeedsClearView)
	{
		return URobberHighlightComponent::IsInClearView(GetOwner(), Viewer, Range);
	}
	const APlayerCameraManager* Camera = Viewer ? Viewer->PlayerCameraManager.Get() : nullptr;
	return Camera && FVector::Dist(Camera->GetCameraLocation(), GetOwner()->GetActorLocation()) <= Range;
}
