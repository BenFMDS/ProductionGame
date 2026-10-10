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
#include "Components/Button.h"
#include "GameFramework/GameUserSettings.h"
#include "UObject/UObjectIterator.h"

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

void AProductionGamePlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsLocalController())
	{
		RemoveDuplicateInteractionMenus();
		BindSettingsApplyButton();
	}
}

void AProductionGamePlayerController::RemoveDuplicateInteractionMenus()
{
	// BP_MenuObject currently receives both the local interaction and its
	// server-forwarded copy.  Keep the newest copy of each project menu until
	// that Blueprint's graph can be opened and resaved with a direct guard.
	TMap<UClass*, UUserWidget*> NewestMenuByClass;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!IsValid(Widget) || Widget->GetOwningPlayer() != this || !Widget->IsInViewport())
		{
			continue;
		}

		const FString ClassPath = Widget->GetClass()->GetPathName();
		if (!ClassPath.StartsWith(TEXT("/Game/OurStuff/Widgets/Menus/")))
		{
			continue;
		}

		if (UUserWidget** Existing = NewestMenuByClass.Find(Widget->GetClass()))
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
			NewestMenuByClass.Add(Widget->GetClass(), Widget);
		}
	}
}

void AProductionGamePlayerController::BindSettingsApplyButton()
{
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* SettingsWidget = *It;
		if (!IsValid(SettingsWidget) || SettingsWidget->GetOwningPlayer() != this || !SettingsWidget->IsInViewport()
			|| !SettingsWidget->GetClass()->GetPathName().StartsWith(TEXT("/Game/OurStuff/Widgets/Menus/WBP_Settings")))
		{
			continue;
		}

		if (UButton* ApplyButton = Cast<UButton>(SettingsWidget->GetWidgetFromName(TEXT("BTN_Apply"))))
		{
			ApplyButton->OnClicked.AddUniqueDynamic(this, &AProductionGamePlayerController::ApplyDisplaySettings);
		}
	}
}

void AProductionGamePlayerController::ApplyDisplaySettings()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		// Do not let a launcher/editor command line overwrite the user's pending choice.
		Settings->ApplySettings(false);
		Settings->SaveSettings();
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
