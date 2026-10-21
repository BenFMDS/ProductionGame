// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AchievementTypes.h"
#include "ProductionGamePlayerController.h"
#include "RobberController.generated.h"

class UAchievementDefinitionAsset;
class UAchievementPopupWidget;
class UInputAction;
class USoundMix;
class UUserWidget;

/**
 * Native controller for the Robber gameplay maps. The identically named
 * Blueprint asset is deliberately only a content-facing child of this class.
 */
UCLASS(Blueprintable, Config = Game)
class PRODUCTIONGAME_API ARobberController : public AProductionGamePlayerController
{
	GENERATED_BODY()

public:
	ARobberController();

	/** Allows menus that referenced the old Blueprint custom event to close themselves. */
	UFUNCTION(BlueprintCallable, Category = "Robber|UI")
	void ClosePauseMenu();

	/** Retains the old client RPC name for callers that start the VOIP warmup flow. */
	UFUNCTION(Client, Reliable, BlueprintCallable, Category = "Robber|Voice")
	void Client_ToggleVoiceStart();

	UFUNCTION(BlueprintCallable, Category = "Robber|Achievements")
	void ShowAchievementCompletePopup(UAchievementDefinitionAsset* AchievementAsset, FAchievementProgressState ProgressState);

	UFUNCTION(BlueprintCallable, Category = "Robber|Achievements")
	void ShowAchievementProgressPopup(UAchievementDefinitionAsset* AchievementAsset, FAchievementProgressState ProgressState);

	/** Server only. Stops (or restores) this player's movement, e.g. while hiding during a break-in. */
	void SetHeistFrozen(bool bFrozen);

	/** A short note for this player, shown on the HUD for a few seconds. */
	UFUNCTION(Client, Reliable)
	void ClientHeistMessage(const FText& Message);

	const FText& GetHeistMessage() const { return HeistMessage; }
	bool IsHeistMessageVisible() const;

	UUserWidget* GetDevUIWidget() const { return DevUIRef; }
	UUserWidget* GetGameUIWidget() const { return GameUIRef; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void TogglePauseMenu();
	void OpenPauseMenu();
	void UnmuteVoice();
	void BindAchievementNotifications();
	void ApplyHeistFrozen(bool bFrozen);

	UFUNCTION(Client, Reliable)
	void ClientSetHeistFrozen(bool bFrozen);

	FText HeistMessage;
	double HeistMessageTime = -100.;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Robber|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> ActiveWidget;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Robber|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> InteractWidgetRef;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Robber|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> GameUIRef;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Robber|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> DevUIRef;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|UI")
	TSoftClassPtr<UUserWidget> InteractionWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|UI")
	TSoftClassPtr<UUserWidget> PauseMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|UI")
	TSoftClassPtr<UUserWidget> GameUIWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|UI")
	TSoftClassPtr<UUserWidget> DevUIWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|Achievements")
	TSoftClassPtr<UAchievementPopupWidget> AchievementPopupClass;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|Voice")
	TObjectPtr<USoundMix> MuteVoiceSoundMix;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|Voice")
	TObjectPtr<USoundMix> UnmuteVoiceSoundMix;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|Input")
	TObjectPtr<UInputAction> PauseInputAction;

	FTimerHandle VoiceUnmuteTimerHandle;
};
