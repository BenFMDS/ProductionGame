// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "ProductionGameCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class PRODUCTIONGAME_API AProductionGameCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;
	
public:
	AProductionGameCharacter();

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	/** Feeds out look input held back by the player's mouse smoothing setting. */
	virtual void Tick(float DeltaSeconds) override;

	FVector2D PendingLook = FVector2D::ZeroVector;


public:

	// --- Sprint: hold the Sprint key (L Shift) to run while stamina lasts. Run out and you're winded until it's back to WindedRecovery. ---

	/** Local player's key went down/up. Sent to the server too, which runs the same stamina so the speeds agree. */
	void SetSprintHeld(bool bHeld);

	UFUNCTION(BlueprintPure, Category = "Sprint")
	float GetStamina() const { return Stamina; }

	UFUNCTION(BlueprintPure, Category = "Sprint")
	bool IsWinded() const { return bWinded; }

	UFUNCTION(BlueprintPure, Category = "Sprint")
	bool IsSprinting() const { return bSprinting; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint")
	float SprintSpeedMultiplier = 1.6f;

	/** Seconds of sprinting from full. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint")
	float StaminaDrainSeconds = 4.f;

	/** Seconds to refill from empty, once it starts refilling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint")
	float StaminaRefillSeconds = 3.f;

	/** Pause after sprinting stops before stamina refills. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint")
	float StaminaRefillDelay = 1.f;

	/** Winded players can sprint again once stamina is back to this fraction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint")
	float WindedRecovery = 0.3f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerSetSprintHeld(bool bHeld);

private:
	void TickSprint(float DeltaSeconds);

	/** Walk speed from the Blueprint, captured at BeginPlay; sprint multiplies it. */
	float BaseWalkSpeed = 0.f;
	float Stamina = 1.f;
	float SinceSprinted = 0.f;
	bool bSprintHeld = false;
	bool bSprinting = false;
	bool bWinded = false;

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

