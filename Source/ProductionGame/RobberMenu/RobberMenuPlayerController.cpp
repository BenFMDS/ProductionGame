#include "RobberMenu/RobberMenuPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "ProductionGamePlayerController.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

ARobberMenuPlayerController::ARobberMenuPlayerController()
{
	static ConstructorHelpers::FClassFinder<AProductionGamePlayerController> MenuInputProfile(
		TEXT("/Game/OurStuff/Core/Player/Game/BP_RobberController"));
	if (MenuInputProfile.Succeeded())
	{
		InputProfileControllerClass = MenuInputProfile.Class;
	}
}

void ARobberMenuPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocalController())
	{
		RemoveDuplicateMenuWidgets();
	}
}

void ARobberMenuPlayerController::RemoveDuplicateMenuWidgets()
{
	TMap<UClass*, UUserWidget*> NewestWidgetByClass;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!IsValid(Widget) || Widget->GetOwningPlayer() != this || !Widget->IsInViewport())
		{
			continue;
		}

		const FString ClassPath = Widget->GetClass()->GetPathName();
		const bool bProjectWidget = ClassPath.StartsWith(TEXT("/Game/OurStuff/"));
		const bool bAchievementList = ClassPath.StartsWith(TEXT("/AchievementPlugin/Widgets/WBP_AchievementList"));
		const bool bIsInteractionWidget = ClassPath.StartsWith(
			TEXT("/Game/OurStuff/Core/InteractionSystem/WBP_Interaction"));
		if ((!bProjectWidget && !bAchievementList) || bIsInteractionWidget)
		{
			continue;
		}

		if (UUserWidget** Existing = NewestWidgetByClass.Find(Widget->GetClass()))
		{
			if ((*Existing)->GetUniqueID() < Widget->GetUniqueID())
			{
				(*Existing)->RemoveFromParent();
				*Existing = Widget;
			}
			else
			{
				Widget->RemoveFromParent();
			}
		}
		else
		{
			NewestWidgetByClass.Add(Widget->GetClass(), Widget);
		}
	}
}

void ARobberMenuPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController() || !InputProfileControllerClass)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	const AProductionGamePlayerController* InputProfile = InputProfileControllerClass->GetDefaultObject<AProductionGamePlayerController>();
	if (!Subsystem || !InputProfile)
	{
		return;
	}

	for (UInputMappingContext* MappingContext : InputProfile->GetDefaultMappingContexts())
	{
		if (MappingContext)
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}

	for (UInputMappingContext* MappingContext : InputProfile->GetMobileExcludedMappingContexts())
	{
		if (MappingContext)
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}
}
