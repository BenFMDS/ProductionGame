// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProductionGameCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/Widget.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "ProductionGame.h"
#include "RobberMenu/RobberSettings.h"
#include "Heist/RobberCrewTag.h"
#include "Heist/RobberInteractables.h"
#include "EngineUtils.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

AProductionGameCharacter::AProductionGameCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	// Only the owner sees the first person arms.
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
}

void AProductionGameCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AProductionGameCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AProductionGameCharacter::DoJumpEnd);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProductionGameCharacter::MoveInput);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProductionGameCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AProductionGameCharacter::LookInput);
	}
	else
	{
		UE_LOG(LogProductionGame, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void AProductionGameCharacter::MoveInput(const FInputActionValue& Value)
{
	DoMove(Value.Get<FVector2D>().X, Value.Get<FVector2D>().Y);
}

void AProductionGameCharacter::LookInput(const FInputActionValue& Value)
{
	DoAim(Value.Get<FVector2D>().X, Value.Get<FVector2D>().Y);
}

void AProductionGameCharacter::DoAim(float Yaw, float Pitch)
{
	if (!GetController())
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const URobberSettingsSubsystem* SettingsSystem = GameInstance ? GameInstance->GetSubsystem<URobberSettingsSubsystem>() : nullptr;
	const URobberSettingsSave* Settings = SettingsSystem ? SettingsSystem->GetSettings() : nullptr;
	if (Settings)
	{
		Yaw *= Settings->LookSensitivity;
		Pitch *= Settings->LookSensitivity * (Settings->bInvertLook ? -1.f : 1.f);
		if (Settings->bSmoothMouse)
		{
			PendingLook += FVector2D(Yaw, Pitch);
			return;
		}
	}
	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}

void AProductionGameCharacter::BeginPlay()
{
	Super::BeginPlay();
	BaseWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	// Added here, not as a subobject, so the character Blueprints don't need re-saving.
	if (GetNetMode() != NM_DedicatedServer)
	{
		URobberCrewTagComponent* Tag = NewObject<URobberCrewTagComponent>(this, TEXT("CrewTag"));
		Tag->SetupAttachment(GetRootComponent());
		Tag->RegisterComponent();

		// Only L_MainMenu has a post process volume with the outline, so anywhere else a highlighted object drew nothing.
		// On the camera, so it works in every map without a volume in each.
		if (UMaterialInterface* Outline = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/OurStuff/Core/InteractionSystem/M_HoverOutline.M_HoverOutline")))
		{
			bool bVolumeHasIt = false;
			for (TActorIterator<APostProcessVolume> It(GetWorld()); It && !bVolumeHasIt; ++It)
			{
				bVolumeHasIt = It->Settings.WeightedBlendables.Array.ContainsByPredicate([Outline](const FWeightedBlendable& Each) { return Each.Object == Outline; });
			}
			if (!bVolumeHasIt)
			{
				FirstPersonCameraComponent->AddOrUpdateBlendable(Outline);
			}
		}
	}
}

void AProductionGameCharacter::SetSprintHeld(bool bHeld)
{
	if (bSprintHeld == bHeld)
	{
		return;
	}
	bSprintHeld = bHeld;
	if (!HasAuthority())
	{
		ServerSetSprintHeld(bHeld);
	}
}

void AProductionGameCharacter::ServerSetSprintHeld_Implementation(bool bHeld)
{
	bSprintHeld = bHeld;
}

void AProductionGameCharacter::TickSprint(float DeltaSeconds)
{
	// Only the owning client and the server move this character; both run the same numbers so the
	// client's predicted speed matches the server's and it doesn't get corrected.
	if (!IsLocallyControlled() && !HasAuthority())
	{
		return;
	}
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	// Standing still with the key down doesn't burn stamina.
	// Acceleration, not the input vector: the server doesn't have a remote player's input, but it does replay their moves.
	const bool bMoving = Movement->Velocity.SizeSquared2D() > FMath::Square(10.f) && !Movement->GetCurrentAcceleration().IsNearlyZero();
	bSprinting = bSprintHeld && !bWinded && bMoving && Stamina > 0.f;
	if (bSprinting)
	{
		SinceSprinted = 0.f;
		Stamina = FMath::Max(0.f, Stamina - DeltaSeconds / FMath::Max(0.1f, StaminaDrainSeconds));
		if (Stamina <= 0.f)
		{
			bWinded = true;
			bSprinting = false;
		}
	}
	else
	{
		SinceSprinted += DeltaSeconds;
		if (SinceSprinted >= StaminaRefillDelay)
		{
			Stamina = FMath::Min(1.f, Stamina + DeltaSeconds / FMath::Max(0.1f, StaminaRefillSeconds));
		}
		if (bWinded && Stamina >= WindedRecovery)
		{
			bWinded = false;
		}
	}
	if (BaseWalkSpeed > 0.f)
	{
		Movement->MaxWalkSpeed = BaseWalkSpeed * MoveSpeedMultiplier * CarrySpeedMultiplier * (bSprinting ? SprintSpeedMultiplier : 1.f);
	}
}

void AProductionGameCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickSprint(DeltaSeconds);
	if (HidingSpot && HasAuthority())
	{
		TickHidingOnServer(DeltaSeconds);
	}

	if (!PendingLook.IsNearlyZero(0.001))
	{
		// About 30 ms behind the hand, whatever the frame rate.
		const FVector2D Step = PendingLook * (1.f - FMath::Exp(-DeltaSeconds * 30.f));
		PendingLook -= Step;
		AddControllerYawInput(Step.X);
		AddControllerPitchInput(Step.Y);
	}
}

void AProductionGameCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void AProductionGameCharacter::DoJumpStart()
{
	Jump();
}

void AProductionGameCharacter::DoJumpEnd()
{
	StopJumping();
}

void AProductionGameCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProductionGameCharacter, HidingSpot);
	DOREPLIFETIME(AProductionGameCharacter, CarrySpeedMultiplier);
}

void AProductionGameCharacter::EnterHiding(ARobberHidingSpot* Spot, bool bInSeenGoingIn)
{
	if (!HasAuthority() || !Spot || HidingSpot)
	{
		return;
	}
	HidingSpot = Spot;
	bSeenGoingIn = bInSeenGoingIn;
	SinceGuardCheck = 0.f;
	Spot->SetOccupant(this);
	ApplyHidingState();
	ForceNetUpdate();
}

void AProductionGameCharacter::ExitHiding(bool bClimbOut)
{
	if (!HasAuthority() || !HidingSpot)
	{
		return;
	}
	ARobberHidingSpot* Spot = HidingSpot;
	Spot->SetOccupant(nullptr);
	HidingSpot = nullptr;
	bSeenGoingIn = false;
	ApplyHidingState();
	if (bClimbOut)
	{
		// TeleportTo nudges them clear of small things. If the front is properly blocked, try the other sides rather than leave them inside it.
		bool bOut = TeleportTo(Spot->GetExitLocation(), GetActorRotation());
		const FVector Scale = Spot->GetActorScale3D();
		const FVector Side = Spot->GetExitLocation() - Spot->GetActorLocation();
		const double Reach = FMath::Max(Scale.X, Scale.Y) * 50. + 90.;
		for (int32 Turn = 1; Turn < 4 && !bOut; ++Turn)
		{
			const FVector Flat = FVector(Side.X, Side.Y, 0.).GetSafeNormal().RotateAngleAxis(Turn * 90., FVector::UpVector);
			bOut = TeleportTo(Spot->GetActorLocation() + Flat * Reach + FVector(0., 0., Side.Z), GetActorRotation());
		}
		// Teleporting can leave them Falling for a frame; they're being put down on the floor.
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	ForceNetUpdate();
}

void AProductionGameCharacter::OnRep_HidingSpot()
{
	ApplyHidingState();
}

void AProductionGameCharacter::ApplyHidingState()
{
	if (AppliedHidingSpot == HidingSpot)
	{
		return;
	}
	const bool bHiding = HidingSpot != nullptr;
	AppliedHidingSpot = HidingSpot;
	SetActorHiddenInGame(bHiding);
	SetActorEnableCollision(!bHiding);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(bHiding ? MOVE_None : MOVE_Walking);
	if (bHiding)
	{
		// Also on clients: with movement off, a teleport from the server may never reach the owning client.
		SetActorLocation(HidingSpot->GetHideLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (IsLocallyControlled())
	{
		ApplyLocalHidingView(bHiding);
	}
}

void AProductionGameCharacter::ApplyLocalHidingView(bool bHiding)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!Camera)
	{
		return;
	}
	Camera->StartCameraFade(1.f, 0.f, 0.35f, FLinearColor::Black, false, false);
	if (LocalViewSpot)
	{
		LocalViewSpot->SetViewedFromInside(false);
	}
	LocalViewSpot = HidingSpot;
	if (LocalViewSpot)
	{
		LocalViewSpot->SetViewedFromInside(true);
	}
	if (bCameraLowered)
	{
		FirstPersonCameraComponent->SetRelativeLocation(SavedCameraOffset);
		bCameraLowered = false;
	}
	if (bHiding && HidingSpot->HiddenEyeHeight > 0.f)
	{
		// Only the view drops: the pawn stays at standing height, so guard sight and catch distances work as for any spot.
		SavedCameraOffset = FirstPersonCameraComponent->GetRelativeLocation();
		// Centred on the hide location: the head bone sits forward of it, which in a space this small could put the eye in the front panel.
		const FVector Eye(GetActorLocation().X, GetActorLocation().Y, HidingSpot->GetFloorZ() + HidingSpot->HiddenEyeHeight);
		FirstPersonCameraComponent->SetWorldLocation(Eye);
		bCameraLowered = true;
	}
	if (bHiding)
	{
		SavedViewPitchMin = Camera->ViewPitchMin;
		SavedViewPitchMax = Camera->ViewPitchMax;
		SavedViewYawMin = Camera->ViewYawMin;
		SavedViewYawMax = Camera->ViewYawMax;
		const FRotator Facing = HidingSpot->GetFacing();
		PC->SetControlRotation(Facing);
		Camera->ViewPitchMin = -25.f;
		Camera->ViewPitchMax = 25.f;
		Camera->ViewYawMin = Facing.Yaw - 55.f;
		Camera->ViewYawMax = Facing.Yaw + 55.f;
	}
	else
	{
		Camera->ViewPitchMin = SavedViewPitchMin;
		Camera->ViewPitchMax = SavedViewPitchMax;
		Camera->ViewYawMin = SavedViewYawMin;
		Camera->ViewYawMax = SavedViewYawMax;
	}

	// From inside, BPC_Interaction's trace can't hit the spot. Freezing it with the spot still targeted means E goes to
	// the spot, which is what climbs back out. It's a Blueprint component, so this goes by name.
	UActorComponent* Interaction = nullptr;
	for (UActorComponent* Component : GetComponents())
	{
		if (Component && Component->GetClass()->GetName().StartsWith(TEXT("BPC_Interaction")))
		{
			Interaction = Component;
			break;
		}
	}
	if (!Interaction)
	{
		return;
	}
	Interaction->SetComponentTickEnabled(!bHiding);
	if (FObjectPropertyBase* Target = FindFProperty<FObjectPropertyBase>(Interaction->GetClass(), TEXT("InteractActor")))
	{
		Target->SetObjectPropertyValue_InContainer(Interaction, bHiding ? HidingSpot.Get() : nullptr);
	}
	FObjectPropertyBase* WidgetRef = FindFProperty<FObjectPropertyBase>(Interaction->GetClass(), TEXT("Interaction Widget Ref"));
	UObject* Widget = WidgetRef ? WidgetRef->GetObjectPropertyValue_InContainer(Interaction) : nullptr;
	UFunction* UpdateWidget = Widget ? Widget->FindFunction(TEXT("UpdateWidget")) : nullptr;
	struct { bool bVisible; FString Data; } Params{ bHiding, bHiding ? NSLOCTEXT("RobberTour", "ClimbOut", "Climb out").ToString() : FString() };
	if (UpdateWidget && UpdateWidget->ParmsSize == sizeof(Params))
	{
		Widget->ProcessEvent(UpdateWidget, &Params);
	}
}

void AProductionGameCharacter::SetInteractPromptMuted(bool bMuted)
{
	// Visibility, not render opacity: the prompt's rounded-box outlines ignore opacity and stayed on screen. Only used while
	// hiding, when BPC_Interaction doesn't tick, so its graph can't flip the visibility back mid-card.
	for (UActorComponent* Component : GetComponents())
	{
		if (!Component || !Component->GetClass()->GetName().StartsWith(TEXT("BPC_Interaction")))
		{
			continue;
		}
		FObjectPropertyBase* WidgetRef = FindFProperty<FObjectPropertyBase>(Component->GetClass(), TEXT("Interaction Widget Ref"));
		if (UWidget* Widget = WidgetRef ? Cast<UWidget>(WidgetRef->GetObjectPropertyValue_InContainer(Component)) : nullptr)
		{
			if (bMuted && !bPromptMuted)
			{
				PromptVisibilityBeforeMute = Widget->GetVisibility();
				Widget->SetVisibility(ESlateVisibility::Collapsed);
			}
			else if (!bMuted && bPromptMuted)
			{
				Widget->SetVisibility(PromptVisibilityBeforeMute);
			}
			bPromptMuted = bMuted;
		}
		return;
	}
}

void AProductionGameCharacter::TickHidingOnServer(float DeltaSeconds)
{
	SinceGuardCheck += DeltaSeconds;
	if (SinceGuardCheck < 0.25f)
	{
		return;
	}
	SinceGuardCheck = 0.f;
	// BPC_Caught teleports them to jail: they're not in the spot any more.
	for (UActorComponent* Component : GetComponents())
	{
		const FBoolProperty* Caught = Component && Component->GetClass()->GetName().StartsWith(TEXT("BPC_Caught")) ? FindFProperty<FBoolProperty>(Component->GetClass(), TEXT("IsCaught")) : nullptr;
		if (Caught && Caught->GetPropertyValue_InContainer(Component))
		{
			ExitHiding(false);
			return;
		}
	}
	// Once no guard is after them any more, they've got away with it.
	if (bSeenGoingIn && !ARobberHidingSpot::IsGuardOnto(this, 0.f))
	{
		bSeenGoingIn = false;
	}
}

UAISense_Sight::EVisibilityResult AProductionGameCharacter::CanBeSeenFrom(const FCanBeSeenFromContext& Context, FVector& OutSeenLocation, int32& OutNumberOfLoSChecksPerformed, int32& OutNumberOfAsyncLosCheckRequested, float& OutSightStrength, int32* UserData, const FOnPendingVisibilityQueryProcessedDelegate* Delegate)
{
	OutNumberOfLoSChecksPerformed = 0;
	OutNumberOfAsyncLosCheckRequested = 0;
	OutSeenLocation = GetActorLocation();
	OutSightStrength = 1.f;
	if (bDevImmune)
	{
		return UAISense_Sight::EVisibilityResult::NotVisible;
	}
	if (HidingSpot)
	{
		return bSeenGoingIn ? UAISense_Sight::EVisibilityResult::Visible : UAISense_Sight::EVisibilityResult::NotVisible;
	}
	// The engine skips its own sight test for anything with this interface, so this is that test.
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(AILineOfSight), true, Context.IgnoreActor);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Context.ObserverLocation, OutSeenLocation, ECC_Visibility, Params);
	++OutNumberOfLoSChecksPerformed;
	return !bHit || Hit.GetActor() == this ? UAISense_Sight::EVisibilityResult::Visible : UAISense_Sight::EVisibilityResult::NotVisible;
}
