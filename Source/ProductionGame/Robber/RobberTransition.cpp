#include "Robber/RobberController.h"

#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace RobberTransition
{
	// Cooked through DirectoriesToAlwaysCook (/Game/OurStuff/Widgets): nothing else references it now.
	const TCHAR* WidgetPath = TEXT("/Game/OurStuff/Widgets/WBP_Transition.WBP_Transition_C");
	const FName FadeParameter(TEXT("Fade"));
}

void ARobberController::PlayTransition(float HoldSeconds)
{
	if (!IsLocalController())
	{
		return;
	}
	if (!TransitionWidget)
	{
		UClass* WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(RobberTransition::WidgetPath)).LoadSynchronous();
		TransitionWidget = WidgetClass ? CreateWidget<UUserWidget>(this, WidgetClass) : nullptr;
		UImage* Image = TransitionWidget ? Cast<UImage>(TransitionWidget->GetWidgetFromName(TEXT("FadeImage"))) : nullptr;
		TransitionMaterial = Image ? Image->GetDynamicMaterial() : nullptr;
		if (!TransitionMaterial)
		{
			TransitionWidget = nullptr;
			return;
		}
	}
	if (!TransitionWidget->IsInViewport())
	{
		TransitionWidget->AddToViewport(0);
	}
	TransitionWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	// 0 is shut, 1 open: BP_RobberCharacter's old timeline went 0 to 1 over a second.
	TransitionMaterial->SetScalarParameterValue(RobberTransition::FadeParameter, 0.f);
	TransitionTime = 0.f;
	TransitionHold = HoldSeconds;
}

void ARobberController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsLocalController())
	{
		return;
	}
	APawn* Now = GetPawn();
	if (Now && Now != TransitionPawn.Get())
	{
		PlayTransition(LoadFadeHold);
	}
	else if (Now && FVector::Dist(Now->GetActorLocation(), TransitionPawnLocation) > TeleportFadeDistance)
	{
		PlayTransition(TeleportFadeHold);
	}
	TransitionPawn = Now;
	if (Now)
	{
		TransitionPawnLocation = Now->GetActorLocation();
	}

	if (TransitionTime < 0.f || !TransitionMaterial)
	{
		return;
	}
	TransitionTime += DeltaTime;
	const float Open = FMath::Clamp((TransitionTime - TransitionHold) / FMath::Max(0.01f, FadeOpenSeconds), 0.f, 1.f);
	TransitionMaterial->SetScalarParameterValue(RobberTransition::FadeParameter, Open);
	if (Open >= 1.f)
	{
		TransitionTime = -1.f;
		TransitionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}
