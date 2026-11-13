#include "Heist/RobberInteractPrompt.h"

#include "Components/TextBlock.h"

void URobberInteractPrompt::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (UWidget* Row = GetWidgetFromName(TEXT("PromptRow")))
	{
		// From the E button's end, so it stays where the player's eye already is.
		Row->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
		Row->SetRenderScale(FVector2D(PromptScale));
	}
}

void URobberInteractPrompt::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Looked up by name: the widgets live in the Blueprint's tree, and InteractText is a Blueprint variable its graph uses.
	const UTextBlock* Text = Cast<UTextBlock>(GetWidgetFromName(TEXT("InteractText")));
	if (UWidget* Row = GetWidgetFromName(TEXT("PromptRow")))
	{
		const ESlateVisibility Wanted = Text && !Text->GetText().IsEmpty() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
		if (Row->GetVisibility() != Wanted)
		{
			Row->SetVisibility(Wanted);
		}
	}
}
