// Copyright Epic Games, Inc. All Rights Reserved.


#include "ProductionGamePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "ProductionGameCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "ProductionGame.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Quest/PlayerQuestComponent.h"

AProductionGamePlayerController::AProductionGamePlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = AProductionGameCameraManager::StaticClass();
	PlayerQuests = CreateDefaultSubobject<UPlayerQuestComponent>(TEXT("PlayerQuests"));
}

void AProductionGamePlayerController::BeginPlay()
{
	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogProductionGame, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AProductionGamePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (bBindNativeDialogueKey)
	{
		InputComponent->BindKey(EKeys::F, IE_Pressed, PlayerQuests.Get(), &UPlayerQuestComponent::TalkToLookedAtNPC);
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool AProductionGamePlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
