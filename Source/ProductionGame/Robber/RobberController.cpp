// Copyright Epic Games, Inc. All Rights Reserved.

#include "Robber/RobberController.h"
#include "Robber/RobberAchievementTracker.h"
#include "RobberMenu/RobberSettings.h"

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
#include "Heist/RobberCaseFileUI.h"
#include "Heist/RobberJournal.h"
#include "Heist/RobberMapTable.h"
#include "Heist/RobberStage.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ProductionGameCharacter.h"
#include "Robber/RobberGameState.h"
#include "RobberMenu/RobberMenuWidgets.h"
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
	Journal = CreateDefaultSubobject<URobberJournalComponent>(TEXT("Journal"));

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
	PauseMenuWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/OurStuff/UI/CaseFile/Menus/WBP_RobberPauseMenu.WBP_RobberPauseMenu_C")));
	LobbySettingsClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/OurStuff/UI/CaseFile/Menus/WBP_RobberSettings.WBP_RobberSettings_C")));
	LobbyAchievementsClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/OurStuff/UI/CaseFile/Menus/WBP_RobberAchievements.WBP_RobberAchievements_C")));
	GameUIWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/OurStuff/Widgets/WBP_GameUI.WBP_GameUI_C")));
	DevUIWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/OurStuff/Widgets/WBP_DevUI.WBP_DevUI_C")));
	CaseFileHUDClass = TSoftClassPtr<URobberCaseFileHUD>(FSoftObjectPath(TEXT("/Game/OurStuff/UI/CaseFile/WBP_CaseFileHUD.WBP_CaseFileHUD_C")));
	JournalWidgetClass = TSoftClassPtr<URobberJournalWidget>(FSoftObjectPath(TEXT("/Game/OurStuff/UI/CaseFile/WBP_Journal.WBP_Journal_C")));
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

	// The Case File HUD sits under the interaction prompt and dev UI; it takes over the debug text and the tracker's heist card.
	// Without WBP_CaseFileHUD (not made yet) ARobberHUD's debug text and the tracker's heist card stay instead.
	if (UClass* HUDClass = CaseFileHUDClass.LoadSynchronous())
	{
		CaseFileHUD = CreateWidget<URobberCaseFileHUD>(this, HUDClass);
		if (CaseFileHUD)
		{
			CaseFileHUD->AddToViewport(-1);
		}
	}

	BindAchievementNotifications();
}

void ARobberController::ToggleJournal()
{
	if (!IsLocalController())
	{
		return;
	}
	if (IsJournalOpen())
	{
		CloseJournal();
		return;
	}
	if (!JournalWidget)
	{
		if (UClass* JournalClass = JournalWidgetClass.LoadSynchronous())
		{
			JournalWidget = CreateWidget<URobberJournalWidget>(this, JournalClass);
		}
	}
	if (!JournalWidget)
	{
		return;
	}
	JournalWidget->AddToViewport(20);
	JournalWidget->OnOpened();
	// The heist carries on: you can still walk, but the mouse is for the tabs.
	SetIgnoreLookInput(true);
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
}

void ARobberController::CloseJournal()
{
	if (!IsJournalOpen())
	{
		return;
	}
	JournalWidget->RemoveFromParent();
	SetIgnoreLookInput(false);
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
}

bool ARobberController::IsJournalOpen() const
{
	return JournalWidget && JournalWidget->IsInViewport();
}

void ARobberController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Slate content outlives the controller otherwise and stays on screen after PIE ends.
	CloseNightSwitcher();
	if (URobberSettingsSubsystem* SettingsSystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<URobberSettingsSubsystem>() : nullptr)
	{
		SettingsSystem->OnControlsChanged.Remove(ControlsChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ARobberController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	if (IsLocalPlayerController())
	{
		if (!PlayerKeysInput)
		{
			PlayerKeysInput = NewObject<UInputComponent>(this, TEXT("PlayerKeysInput"));
			PlayerKeysInput->RegisterComponent();
			PushInputComponent(PlayerKeysInput);
		}
		BindPlayerKeys();
		if (URobberSettingsSubsystem* SettingsSystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<URobberSettingsSubsystem>() : nullptr)
		{
			SettingsSystem->OnControlsChanged.Remove(ControlsChangedHandle);
			ControlsChangedHandle = SettingsSystem->OnControlsChanged.AddUObject(this, &ARobberController::BindPlayerKeys);
		}
	}
#if !UE_BUILD_SHIPPING
	InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &ARobberController::ToggleNightSwitcher);
#endif
	// Not consumed: Space is still jump and Esc still the menu when nobody's at the map.
	for (const FKey& Key : { EKeys::SpaceBar, EKeys::Escape })
	{
		InputComponent->BindKey(Key, IE_Pressed, this, &ARobberController::OnStepBackKey).bConsumeInput = false;
	}
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ARobberController::ClickMap).bConsumeInput = false;
	if (PauseInputAction)
	{
		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
		{
			EnhancedInput->BindAction(PauseInputAction, ETriggerEvent::Started, this, &ARobberController::TogglePauseMenu);
		}
	}
}

void ARobberController::BindPlayerKeys()
{
	const URobberSettingsSubsystem* SettingsSystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<URobberSettingsSubsystem>() : nullptr;
	if (!PlayerKeysInput || !SettingsSystem)
	{
		return;
	}
	PlayerKeysInput->KeyBindings.Reset();
	PlayerKeysInput->BindKey(SettingsSystem->GetKey(TEXT("Pause")), IE_Pressed, this, &ARobberController::TogglePauseMenu);
	PlayerKeysInput->BindKey(SettingsSystem->GetKey(TEXT("Journal")), IE_Pressed, this, &ARobberController::ToggleJournal);
	// Not consumed, so a game binding on the same key still gets it.
	PlayerKeysInput->BindKey(SettingsSystem->GetKey(TEXT("Sprint")), IE_Pressed, this, &ARobberController::OnSprintPressed).bConsumeInput = false;
	PlayerKeysInput->BindKey(SettingsSystem->GetKey(TEXT("Sprint")), IE_Released, this, &ARobberController::OnSprintReleased).bConsumeInput = false;
}

void ARobberController::OnSprintPressed()
{
	if (AProductionGameCharacter* Runner = Cast<AProductionGameCharacter>(GetPawn()))
	{
		Runner->SetSprintHeld(true);
	}
}

void ARobberController::OnSprintReleased()
{
	if (AProductionGameCharacter* Runner = Cast<AProductionGameCharacter>(GetPawn()))
	{
		Runner->SetSprintHeld(false);
	}
}

void ARobberController::ClientAchievement_Implementation(FName AchievementId, ERobberAchievementStep Step, FName Detail, int32 Amount)
{
	if (URobberAchievementTracker* Tracker = GetGameInstance() ? GetGameInstance()->GetSubsystem<URobberAchievementTracker>() : nullptr)
	{
		Tracker->Apply(AchievementId, Step, Detail, Amount);
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

void ARobberController::ClientOpenLobbyMenu_Implementation(ERobberLobbyMenu Menu)
{
	if (IsValid(ActiveWidget) || (IsValid(LobbyMenu) && LobbyMenu->IsInViewport()))
	{
		return;
	}
	if (Menu == ERobberLobbyMenu::Quit)
	{
		OpenPauseMenu();
		if (URobberPauseMenu* Pause = Cast<URobberPauseMenu>(ActiveWidget))
		{
			Pause->ShowQuitCheckOnly();
		}
		return;
	}
	const TSoftClassPtr<UUserWidget>& Class = Menu == ERobberLobbyMenu::Settings ? LobbySettingsClass : LobbyAchievementsClass;
	// The menu takes UI-only input when built and gives the game back when closed (URobberMenuWidget).
	LobbyMenu = Class.IsNull() ? nullptr : CreateWidget<UUserWidget>(this, Class.LoadSynchronous());
	if (LobbyMenu)
	{
		LobbyMenu->AddToViewport(10);
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

void ARobberController::ClientToggleMapView_Implementation(ARobberMapTable* Table)
{
	if (!Table || ViewedMapTable == Table)
	{
		StepBackFromMap();
		return;
	}
	if (!ViewedMapTable)
	{
		// The camera stays put and the mouse moves a cursor instead of the view.
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
		int32 SizeX = 0, SizeY = 0;
		GetViewportSize(SizeX, SizeY);
		SetMouseLocation(SizeX / 2, SizeY / 2);
	}
	ViewedMapTable = Table;
	Table->AimViewCamera();
	SetViewTargetWithBlend(Table, 0.5f, VTBlend_EaseInOut, 2.f);
}

void ARobberController::StepBackFromMap()
{
	if (!ViewedMapTable)
	{
		return;
	}
	ViewedMapTable = nullptr;
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
	if (APawn* MyPawn = GetPawn())
	{
		SetViewTargetWithBlend(MyPawn, 0.4f, VTBlend_EaseInOut, 2.f);
	}
}

ARobberMapObject* ARobberController::GetMapPieceUnderCursor() const
{
	if (!ViewedMapTable)
	{
		return nullptr;
	}
	FVector2D Cursor;
	if (MapCursorOverride.IsSet())
	{
		Cursor = MapCursorOverride.GetValue();
	}
	else if (!GetMousePosition(Cursor.X, Cursor.Y))
	{
		return nullptr;
	}
	FHitResult Hit;
	return GetHitResultAtScreenPosition(Cursor, ECC_Visibility, false, Hit) ? Cast<ARobberMapObject>(Hit.GetActor()) : nullptr;
}

void ARobberController::ClickMap()
{
	if (ARobberMapObject* Piece = GetMapPieceUnderCursor())
	{
		ServerVoteOnMap(Piece);
	}
}

void ARobberController::ServerVoteOnMap_Implementation(ARobberMapObject* Piece)
{
	// The same path as pressing Interact on the piece, so the usual checks (lobby only, entry tonight) apply.
	if (Piece && GetPawn())
	{
		Piece->HandleInteract(GetPawn());
	}
}

void ARobberController::OnStepBackKey()
{
	StepBackFromMap();
	CloseJournal();
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
