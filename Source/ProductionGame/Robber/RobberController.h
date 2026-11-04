// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AchievementTypes.h"
#include "ProductionGamePlayerController.h"
#include "Robber/RobberAchievementTracker.h"
#include "RobberController.generated.h"

class ARobberMapObject;
class ARobberMapTable;
class URobberCaseFileHUD;
class URobberJournalComponent;
class URobberJournalWidget;
class UAchievementDefinitionAsset;
class UAchievementPopupWidget;
class UInputAction;
class SWidget;
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

	/** Achievements save on each player's machine, so the server sends the news here. See RobberAchievements. */
	UFUNCTION(Client, Reliable)
	void ClientAchievement(FName AchievementId, ERobberAchievementStep Step, FName Detail, int32 Amount);

	/** Server only. Stops (or restores) this player's movement, e.g. while hiding during a break-in. */
	void SetHeistFrozen(bool bFrozen);

	/** A short note for this player, shown on the HUD for a few seconds. */
	UFUNCTION(Client, Reliable)
	void ClientHeistMessage(const FText& Message);

	const FText& GetHeistMessage() const { return HeistMessage; }
	bool IsHeistMessageVisible() const;

	UUserWidget* GetDevUIWidget() const { return DevUIRef; }
	UUserWidget* GetGameUIWidget() const { return GameUIRef; }

	/** Dev tool, F10: buttons for every night and the tutorial. Not in Shipping builds. */
	void ToggleNightSwitcher();
	bool IsNightSwitcherOpen() const;

	/** What the night switcher's buttons send. NightIndex as ARobberGameState::SwitchNight. Ignored in Shipping. */
	UFUNCTION(Server, Reliable)
	void ServerSwitchNight(int32 NightIndex);

	/** From the planning table's interact: lean over Table, or step back if already looking at it. */
	UFUNCTION(Client, Reliable)
	void ClientToggleMapView(ARobberMapTable* Table);

	/** Local only. Back to the pawn's own view. */
	void StepBackFromMap();

	ARobberMapTable* GetViewedMapTable() const { return ViewedMapTable; }

	/** Local only, while leaning over a map: the piece under the mouse cursor, or null. */
	ARobberMapObject* GetMapPieceUnderCursor() const;

	/** Local only. Left click while leaning over a map: votes for the piece under the cursor. */
	void ClickMap();

	/** For automation: stands in for the mouse position over the map (unset = the real mouse). */
	void SetMapCursorOverride(const TOptional<FVector2D>& Cursor) { MapCursorOverride = Cursor; }

	UFUNCTION(Server, Reliable)
	void ServerVoteOnMap(ARobberMapObject* Piece);

	/** J: open or close the robber's journal over the running game. */
	void ToggleJournal();
	void CloseJournal();
	bool IsJournalOpen() const;

	/** The Case File HUD (null on non-local controllers); while it exists, the old debug text and heist card stay hidden. */
	URobberCaseFileHUD* GetCaseFileHUD() const { return CaseFileHUD; }
	URobberJournalWidget* GetJournalWidget() const { return JournalWidget; }
	URobberJournalComponent* GetJournal() const { return Journal; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	void CloseNightSwitcher();

#if !UE_BUILD_SHIPPING
	TSharedPtr<SWidget> NightSwitcher;
#endif

	void TogglePauseMenu();
	void OpenPauseMenu();
	/** Journal and Pause are bound here, not in the input mapping context, and follow the player's key settings. */
	void BindPlayerKeys();

	/** A component of its own so rebinding can clear just these keys. */
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> PlayerKeysInput;

	FDelegateHandle ControlsChangedHandle;
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

	void OnStepBackKey();

	UPROPERTY(Transient)
	TObjectPtr<ARobberMapTable> ViewedMapTable;

	TOptional<FVector2D> MapCursorOverride;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Journal")
	TObjectPtr<URobberJournalComponent> Journal;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|UI")
	TSoftClassPtr<URobberCaseFileHUD> CaseFileHUDClass;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|UI")
	TSoftClassPtr<URobberJournalWidget> JournalWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<URobberCaseFileHUD> CaseFileHUD;

	UPROPERTY(Transient)
	TObjectPtr<URobberJournalWidget> JournalWidget;
};
