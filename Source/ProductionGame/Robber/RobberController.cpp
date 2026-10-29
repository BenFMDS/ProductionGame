// Copyright Epic Games, Inc. All Rights Reserved.

#include "Robber/RobberController.h"

#include "AchievementBlueprintFunctionLibrary.h"
#include "AchievementDefinitionAsset.h"
#include "AchievementPopupWidget.h"
#include "AchievementSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Engine/GameViewportClient.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "Heist/RobberStage.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Robber/RobberGameState.h"
#include "Sound/SoundMix.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

ARobberController::ARobberController()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultMappingContext(
		TEXT("/Game/Input/IMC_Default.IMC_Default"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MouseLookMappingContext(
		TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
	static ConstructorHelpers::FClassFinder<UUserWidget> MobileControls(
		TEXT("/Game/Input/Touch/UI_TouchSimple"));
	static ConstructorHelpers::FObjectFinder<USoundMix> MuteVoice(
		TEXT("/Game/OurStuff/Core/Player/Game/VOIP/SCM_MuteVoice.SCM_MuteVoice"));
	static ConstructorHelpers::FObjectFinder<USoundMix> UnmuteVoice(
		TEXT("/Game/OurStuff/Core/Player/Game/VOIP/SCM_UnMuteVoice.SCM_UnMuteVoice"));
	static ConstructorHelpers::FObjectFinder<UInputAction> PauseAction(
		TEXT("/Game/Input/Actions/IA_Pause.IA_Pause"));

	DefaultMappingContexts.Reset();
	if (DefaultMappingContext.Succeeded())
	{
		DefaultMappingContexts.Add(DefaultMappingContext.Object);
	}

	MobileExcludedMappingContexts.Reset();
	if (MouseLookMappingContext.Succeeded())
	{
		MobileExcludedMappingContexts.Add(MouseLookMappingContext.Object);
	}

	MobileControlsWidgetClass = MobileControls.Class;
	// These widgets directly reference the controller. Keep the references soft
	// in the CDO to avoid an async-load cycle during editor startup.
	InteractionWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/OurStuff/Core/InteractionSystem/WBP_Interaction.WBP_Interaction_C")));
	PauseMenuWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/OurStuff/Widgets/Menus/WBP_PauseMenu.WBP_PauseMenu_C")));
	GameUIWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/OurStuff/Widgets/WBP_GameUI.WBP_GameUI_C")));
	DevUIWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/OurStuff/Widgets/WBP_DevUI.WBP_DevUI_C")));
	AchievementPopupClass = TSoftClassPtr<UAchievementPopupWidget>(FSoftObjectPath(TEXT("/AchievementPlugin/Widgets/WBP_AchievementPopUp.WBP_AchievementPopUp_C")));
	MuteVoiceSoundMix = MuteVoice.Object;
	UnmuteVoiceSoundMix = UnmuteVoice.Object;
	PauseInputAction = PauseAction.Object;
}

void ARobberController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (!InteractionWidgetClass.IsNull() && !InteractWidgetRef)
	{
		InteractWidgetRef = CreateWidget<UUserWidget>(this, InteractionWidgetClass.LoadSynchronous());
		if (InteractWidgetRef)
		{
			InteractWidgetRef->AddToViewport();
		}
	}

	if (!GameUIWidgetClass.IsNull() && !GameUIRef)
	{
		GameUIRef = CreateWidget<UUserWidget>(this, GameUIWidgetClass.LoadSynchronous());
		if (GameUIRef)
		{
			GameUIRef->AddToViewport();
		}
	}

	if (!DevUIWidgetClass.IsNull() && !DevUIRef)
	{
		DevUIRef = CreateWidget<UUserWidget>(this, DevUIWidgetClass.LoadSynchronous());
		if (DevUIRef)
		{
			DevUIRef->AddToViewport();
		}
	}

	BindAchievementNotifications();
}

void ARobberController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Slate content outlives the controller otherwise and stays on screen after PIE ends.
	CloseNightSwitcher();
	Super::EndPlay(EndPlayReason);
}

void ARobberController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ARobberController::TogglePauseMenu);
#if !UE_BUILD_SHIPPING
	InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &ARobberController::ToggleNightSwitcher);
#endif
	if (PauseInputAction)
	{
		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
		{
			EnhancedInput->BindAction(PauseInputAction, ETriggerEvent::Started, this, &ARobberController::TogglePauseMenu);
		}
	}
}

void ARobberController::ToggleNightSwitcher()
{
#if !UE_BUILD_SHIPPING
	if (NightSwitcher.IsValid())
	{
		CloseNightSwitcher();
		return;
	}
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const ARobberGameState* RobberGameState = GetWorld() ? GetWorld()->GetGameState<ARobberGameState>() : nullptr;
	if (!Viewport || !RobberGameState)
	{
		return;
	}

	TWeakObjectPtr<ARobberController> WeakThis(this);
	auto MakeButton = [WeakThis](const FText& Label, int32 NightIndex) -> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.Padding(2.f)
			[
				SNew(SButton)
				.IsFocusable(false)
				.Text(Label)
				.OnClicked_Lambda([WeakThis, NightIndex]()
				{
					if (WeakThis.IsValid())
					{
						WeakThis->ServerSwitchNight(NightIndex);
					}
					return FReply::Handled();
				})
			];
	};

	TSharedRef<SHorizontalBox> NightButtons = SNew(SHorizontalBox);
	for (int32 NightIndex = 0; NightIndex < RobberGameState->GetNightCount(); ++NightIndex)
	{
		NightButtons->AddSlot().AutoWidth()[MakeButton(FText::AsNumber(NightIndex + 1), NightIndex)];
	}

	NightSwitcher = SNew(SBox)
		.Visibility(EVisibility::SelfHitTestInvisible)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 80.f, 20.f, 0.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.75f))
			.Padding(10.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2.f)
				[
					SNew(STextBlock).Text(INVTEXT("Dev: switch night (F10)"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2.f)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					.Text_Lambda([WeakThis]()
					{
						const ARobberGameState* State = WeakThis.IsValid() ? WeakThis->GetWorld()->GetGameState<ARobberGameState>() : nullptr;
						if (!State)
						{
							return FText::GetEmpty();
						}
						const FText Night = State->IsTutorial() ? INVTEXT("Tutorial")
							: State->GetCurrentNight() == INDEX_NONE ? INVTEXT("No night")
							: FText::Format(INVTEXT("Night {0}"), State->GetCurrentNight() + 1);
						return FText::Format(INVTEXT("Now: {0} - {1}"), Night, RobberStage::GetDisplayName(State->GetGameStage()));
					})
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					NightButtons
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeButton(INVTEXT("Tutorial"), INDEX_NONE)
				]
			]
		];
	Viewport->AddViewportWidgetContent(NightSwitcher.ToSharedRef(), 100);

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(true);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
#endif
}

bool ARobberController::IsNightSwitcherOpen() const
{
#if !UE_BUILD_SHIPPING
	return NightSwitcher.IsValid();
#else
	return false;
#endif
}

void ARobberController::CloseNightSwitcher()
{
#if !UE_BUILD_SHIPPING
	if (!NightSwitcher.IsValid())
	{
		return;
	}
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(NightSwitcher.ToSharedRef());
	}
	NightSwitcher.Reset();
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
#endif
}

void ARobberController::ServerSwitchNight_Implementation(int32 NightIndex)
{
#if !UE_BUILD_SHIPPING
	if (ARobberGameState* RobberGameState = GetWorld()->GetGameState<ARobberGameState>())
	{
		RobberGameState->SwitchNight(NightIndex);
	}
#endif
}

void ARobberController::TogglePauseMenu()
{
	if (IsValid(ActiveWidget))
	{
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu();
	}
}

void ARobberController::OpenPauseMenu()
{
	if (PauseMenuWidgetClass.IsNull() || IsValid(ActiveWidget))
	{
		return;
	}

	ActiveWidget = CreateWidget<UUserWidget>(this, PauseMenuWidgetClass.LoadSynchronous());
	if (!ActiveWidget)
	{
		return;
	}

	ActiveWidget->AddToViewport();
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(ActiveWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(true);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void ARobberController::ClosePauseMenu()
{
	if (IsValid(ActiveWidget))
	{
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;
}

void ARobberController::Client_ToggleVoiceStart_Implementation()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	ConsoleCommand(TEXT("voice.MicInputGain 3"), true);
	UGameplayStatics::SetBaseSoundMix(this, MuteVoiceSoundMix);
	GetWorldTimerManager().SetTimer(VoiceUnmuteTimerHandle, this, &ARobberController::UnmuteVoice, 3.0f, false);
}

void ARobberController::UnmuteVoice()
{
	UGameplayStatics::SetBaseSoundMix(this, UnmuteVoiceSoundMix);
}

void ARobberController::BindAchievementNotifications()
{
	if (UAchievementSubsystem* AchievementSubsystem = UAchievementBlueprintFunctionLibrary::GetAchievementSubsystem(this))
	{
		AchievementSubsystem->OnAchievementCompleted.AddUniqueDynamic(this, &ARobberController::ShowAchievementCompletePopup);
		AchievementSubsystem->OnAchievementMilestoneReached.AddUniqueDynamic(this, &ARobberController::ShowAchievementProgressPopup);
	}
}

void ARobberController::SetHeistFrozen(bool bFrozen)
{
	ApplyHeistFrozen(bFrozen);
	if (!IsLocalController())
	{
		ClientSetHeistFrozen(bFrozen);
	}
}

void ARobberController::ClientSetHeistFrozen_Implementation(bool bFrozen)
{
	ApplyHeistFrozen(bFrozen);
}

void ARobberController::ApplyHeistFrozen(bool bFrozen)
{
	// Both sides change movement, so the owning client doesn't predict walking and get corrected every frame.
	if (bFrozen)
	{
		SetIgnoreMoveInput(true);
	}
	else
	{
		ResetIgnoreMoveInput();
	}
	if (ACharacter* RobberCharacter = GetCharacter())
	{
		if (bFrozen)
		{
			RobberCharacter->GetCharacterMovement()->StopMovementImmediately();
			RobberCharacter->GetCharacterMovement()->DisableMovement();
		}
		else
		{
			RobberCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}
}

void ARobberController::ClientHeistMessage_Implementation(const FText& Message)
{
	HeistMessage = Message;
	HeistMessageTime = GetWorld()->GetTimeSeconds();
}

bool ARobberController::IsHeistMessageVisible() const
{
	return !HeistMessage.IsEmpty() && GetWorld()->GetTimeSeconds() - HeistMessageTime < 5.;
}

void ARobberController::ShowAchievementCompletePopup(UAchievementDefinitionAsset* AchievementAsset, FAchievementProgressState ProgressState)
{
	UKismetSystemLibrary::PrintString(this, TEXT("ShowAchievementCompletePopup Called"), true, true, FLinearColor(0.0f, 0.66f, 1.0f), 5.0f);
	if (UClass* PopupClass = AchievementPopupClass.LoadSynchronous())
	{
		UAchievementPopupWidget::CreateAndShowPopup(this, PopupClass, AchievementAsset, ProgressState, 2.5f, 100);
	}
}

void ARobberController::ShowAchievementProgressPopup(UAchievementDefinitionAsset* AchievementAsset, FAchievementProgressState ProgressState)
{
	UKismetSystemLibrary::PrintString(this, TEXT("ShowAchievementProgressPopup Called"), true, true, FLinearColor(0.0f, 0.66f, 1.0f), 5.0f);
	if (UAchievementPopupWidget* Popup = CreateWidget<UAchievementPopupWidget>(this, UAchievementPopupWidget::StaticClass()))
	{
		Popup->SetPopupDataFromDefinition(AchievementAsset, ProgressState);
	}
}
