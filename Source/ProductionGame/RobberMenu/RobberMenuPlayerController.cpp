#include "RobberMenu/RobberMenuPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "ProductionGamePlayerController.h"
#include "UObject/ConstructorHelpers.h"

ARobberMenuPlayerController::ARobberMenuPlayerController()
{
	static ConstructorHelpers::FClassFinder<AProductionGamePlayerController> MenuInputProfile(
		TEXT("/Game/OurStuff/Core/Player/BP_MenuPlayerController"));
	if (MenuInputProfile.Succeeded())
	{
		InputProfileControllerClass = MenuInputProfile.Class;
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
