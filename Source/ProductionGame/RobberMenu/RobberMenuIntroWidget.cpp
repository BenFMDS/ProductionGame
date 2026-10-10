#include "RobberMenu/RobberMenuIntroWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

TSharedRef<SWidget> URobberMenuIntroWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RobberMenuIntroCanvas"));
		WidgetTree->RootWidget = Canvas;

		UBorder* Vignette = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RobberMenuVignette"));
		Vignette->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.38f));
		UCanvasPanelSlot* VignetteSlot = Canvas->AddChildToCanvas(Vignette);
		VignetteSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		VignetteSlot->SetOffsets(FMargin(0.0f));

		UVerticalBox* TitleStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RobberMenuTitleStack"));
		UCanvasPanelSlot* TitleSlot = Canvas->AddChildToCanvas(TitleStack);
		TitleSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		TitleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		TitleSlot->SetAutoSize(true);

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RobberMenuTitle"));
		Title->SetText(FText::FromString(TEXT("ROBBER MENU")));
		Title->SetJustification(ETextJustify::Center);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.67f, 1.0f, 0.73f)));
		FSlateFontInfo TitleFont = Title->GetFont();
		TitleFont.Size = 56;
		Title->SetFont(TitleFont);
		TitleStack->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign_Center);
		TitleText = Title;

		PressAnyKeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RobberMenuPressAnyKey"));
		PressAnyKeyText->SetText(FText::FromString(TEXT("PRESS ANY KEY")));
		PressAnyKeyText->SetJustification(ETextJustify::Center);
		PressAnyKeyText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo PromptFont = PressAnyKeyText->GetFont();
		PromptFont.Size = 18;
		PressAnyKeyText->SetFont(PromptFont);
		UVerticalBoxSlot* PromptSlot = TitleStack->AddChildToVerticalBox(PressAnyKeyText);
		PromptSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 0.0f));
		PromptSlot->SetHorizontalAlignment(HAlign_Center);
		PressAnyKeyText->SetVisibility(ESlateVisibility::Hidden);
	}

	return Super::RebuildWidget();
}

void URobberMenuIntroWidget::NativeConstruct()
{
	Super::NativeConstruct();

	LogoAnimationTime = 0.0f;
	if (TitleText)
	{
		TitleText->SetRenderOpacity(0.0f);
		TitleText->SetRenderScale(FVector2D(0.78f, 0.78f));
	}
}

void URobberMenuIntroWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!TitleText)
	{
		return;
	}

	LogoAnimationTime += InDeltaTime;
	const float FadeAlpha = FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(LogoAnimationTime / 0.65f, 0.0f, 1.0f), 2.0f);
	const float Pulse = 1.0f + 0.018f * FMath::Sin(LogoAnimationTime * 3.5f);
	TitleText->SetRenderOpacity(FadeAlpha);
	TitleText->SetRenderScale(FVector2D((0.78f + 0.22f * FadeAlpha) * Pulse));
}

void URobberMenuIntroWidget::SetWaitingForAnyKey_Implementation(bool bWaiting)
{
	if (PressAnyKeyText)
	{
		PressAnyKeyText->SetVisibility(bWaiting ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void URobberMenuIntroWidget::IntroDismissed_Implementation()
{
	RemoveFromParent();
}
