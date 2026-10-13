// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ProductionGamePlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UPlayerQuestComponent;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 */
UCLASS(abstract, config="Game")
class PRODUCTIONGAME_API AProductionGamePlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	AProductionGamePlayerController();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest")
	TObjectPtr<UPlayerQuestComponent> PlayerQuests;

	/** Read-only access for specialized controllers that reuse only this input profile. */
	const TArray<UInputMappingContext*>& GetDefaultMappingContexts() const { return DefaultMappingContexts; }
	const TArray<UInputMappingContext*>& GetMobileExcludedMappingContexts() const { return MobileExcludedMappingContexts; }

	/** F is a fallback for native NPCs; existing interaction can call ServerTalkTo instead. */
	UPROPERTY(EditDefaultsOnly, Category="Quest")
	bool bBindNativeDialogueKey = true;

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Ensures an interaction cannot leave two copies of the same menu on this player's screen. */
	virtual void Tick(float DeltaSeconds) override;

	/** Commits the mode selected in WBP_Settings when the player presses Apply. */
	UFUNCTION()
	void ApplyDisplaySettings();

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	void RemoveDuplicateProjectWidgets();
	void BindSettingsApplyButton();
};
