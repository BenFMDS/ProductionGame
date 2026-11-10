// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AchievementTypes.h"
#include "ProductionGamePlayerController.h"
#include "Robber/RobberAchievementTracker.h"
#include "Heist/RobberDevMenu.h"
#include "Heist/RobberHideout.h"
#include "RobberController.generated.h"

class ARobberMapObject;
class ARobberMapTable;
class URobberCaseFileHUD;
class URobberJournalComponent;
class URobberJournalWidget;
class URobberPowerCut;
class URobberServerConsole;
class URobberShopCatalogue;
class UMaterialInstanceDynamic;
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

	/** Dev tool, F10: WBP_DevMenu (pick a night, skip stages, cheats). Not in Shipping builds. */
	void ToggleDevMenu();
	void CloseDevMenu();
	bool IsDevMenuOpen() const;
	URobberDevMenu* GetDevMenu() const { return DevMenu; }

	/** What the dev menu's buttons send. Defined in RobberDevMenu.cpp. Ignored in Shipping. */
	UFUNCTION(Server, Reliable)
	void ServerDevAction(ERobberDevAction Action, int32 Value);

	/** Dev menu: who has what quest tonight. The deal only exists on the server. */
	UFUNCTION(Client, Reliable)
	void ClientDevQuestList(const TArray<FString>& Lines, const TArray<uint8>& CanFind);
	FString GetDevQuestList() const { return FString::Join(DevQuestLines, TEXT("\n")); }
	void ClearDevQuestList() { DevQuestLines.Reset(); DevQuestCanFind.Reset(); ++DevQuestRevision; }
	TArray<FString> DevQuestLines;
	TArray<uint8> DevQuestCanFind;
	int32 DevQuestRevision = 0;
	/** Server only: what each row's Find button goes to. */
	TArray<TWeakObjectPtr<AActor>> DevFindTargets;

	/** From the planning table's interact: lean over Table, or step back if already looking at it. */
	UFUNCTION(Client, Reliable)
	void ClientToggleMapView(ARobberMapTable* Table);

	/** From a hideout object in the lobby: opens its screen (settings, achievements, or the quit check) for this player. */
	UFUNCTION(Client, Reliable)
	void ClientOpenLobbyMenu(ERobberLobbyMenu Menu);

	/** Software Update's console. Action is an EConsoleAction. Defined in RobberSoftwareUpdate.cpp. */
	UFUNCTION(Server, Reliable)
	void ServerConsoleAction(uint8 Action, int32 Value, const FString& Text);
	UFUNCTION(Client, Reliable)
	void ClientServerConsole(bool bOpen);
	UFUNCTION(Client, Reliable)
	void ClientConsoleMessage(const FText& Message);
	URobberServerConsole* GetServerConsole() const { return ServerConsole.Get(); }

	/** Local only. The iris (WBP_Transition): shut for HoldSeconds, then opens. Defined in RobberTransition.cpp. */
	void PlayTransition(float HoldSeconds);
	bool IsTransitionPlaying() const { return TransitionTime >= 0.f; }
	virtual void PlayerTick(float DeltaTime) override;

	/** A new pawn counts as a level load; moving further than this in one frame counts as a teleport. */
	UPROPERTY(EditDefaultsOnly, Category = "Robber|Transition")
	float TeleportFadeDistance = 800.f;
	UPROPERTY(EditDefaultsOnly, Category = "Robber|Transition")
	float LoadFadeHold = 2.f;
	UPROPERTY(EditDefaultsOnly, Category = "Robber|Transition")
	float TeleportFadeHold = 0.3f;
	UPROPERTY(EditDefaultsOnly, Category = "Robber|Transition")
	float FadeOpenSeconds = 1.f;

	/** The settings or achievements screen a lobby object opened, while it's up. */
	UUserWidget* GetLobbyMenu() const { return LobbyMenu; }

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

	/** Sprint key held / let go: passed to the pawn. */
	void OnSprintPressed();
	void OnSprintReleased();
	bool IsJournalOpen() const;

	/** The shop, opened from the catalogue on the lobby's lectern. Takes the mouse and stops the player moving. */
	void OpenShopCatalogue();
	void CloseShopCatalogue();
	bool IsShopOpen() const;
	URobberShopCatalogue* GetShopWidget() const { return ShopWidget; }

	/** The generator's breaker panel for this player. These four are defined in RobberPowerCut.cpp. */
	UFUNCTION(Client, Reliable)
	void ClientOpenPowerCut(AActor* Generator);
	void ClosePowerCut();
	bool IsPowerCutOpen() const;
	URobberPowerCut* GetPowerCut() const { return PowerCutWidget; }

	/** Every breaker's down: the server runs the generator's CutPower, if this player's still next to it. */
	UFUNCTION(Server, Reliable)
	void ServerFinishPowerCut(AActor* Generator);

	/** A mistimed trip: the bang adds heat. */
	UFUNCTION(Server, Reliable)
	void ServerPowerCutMiss(AActor* Generator);

	/** The Case File HUD (null on non-local controllers); while it exists, the old debug text and heist card stay hidden. */
	URobberCaseFileHUD* GetCaseFileHUD() const { return CaseFileHUD; }
	URobberJournalWidget* GetJournalWidget() const { return JournalWidget; }
	URobberJournalComponent* GetJournal() const { return Journal; }

	/** A code typed into a code lock's screen. The server checks it, so the answer never has to reach a client. Defined in RobberPuzzle.cpp. */
	UFUNCTION(Server, Reliable)
	void ServerSubmitPuzzleCode(AActor* Lock, const FString& Typed);

	/** The server's verdict on that code, back to the screen that sent it. */
	UFUNCTION(Client, Reliable)
	void ClientPuzzleCodeResult(AActor* Lock, bool bCorrect);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
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
	TSoftClassPtr<UUserWidget> LobbySettingsClass;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|UI")
	TSoftClassPtr<UUserWidget> LobbyAchievementsClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LobbyMenu;

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

	UPROPERTY(Transient)
	TObjectPtr<URobberShopCatalogue> ShopWidget;

	/** Kept between openings, so the terms the player has scrolled through are still scrolled. */
	UPROPERTY(Transient)
	TObjectPtr<URobberServerConsole> ServerConsole;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> TransitionWidget;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TransitionMaterial;
	float TransitionTime = -1.f;
	float TransitionHold = 0.f;
	TWeakObjectPtr<APawn> TransitionPawn;
	FVector TransitionPawnLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<URobberDevMenu> DevMenu;

	UPROPERTY(Transient)
	TObjectPtr<URobberPowerCut> PowerCutWidget;
};
