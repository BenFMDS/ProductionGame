// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProductionGameCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "ProductionGame.h"
#include "RobberMenu/RobberSettings.h"

AProductionGameCharacter::AProductionGameCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
}

void AProductionGameCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AProductionGameCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AProductionGameCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProductionGameCharacter::MoveInput);

		// Looking/Aiming
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
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void AProductionGameCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

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

	// pass the rotation inputs
	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}

void AProductionGameCharacter::BeginPlay()
{
	Super::BeginPlay();
	BaseWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
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
		Movement->MaxWalkSpeed = BaseWalkSpeed * MoveSpeedMultiplier * (bSprinting ? SprintSpeedMultiplier : 1.f);
	}
}

void AProductionGameCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickSprint(DeltaSeconds);

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
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void AProductionGameCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void AProductionGameCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}
