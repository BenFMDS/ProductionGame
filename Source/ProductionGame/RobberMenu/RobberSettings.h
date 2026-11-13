#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RobberSettings.generated.h"

class UInputMappingContext;

/** The player's audio, key and mouse settings. Window mode and resolution live in UGameUserSettings as usual. */
UCLASS()
class PRODUCTIONGAME_API URobberSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() float MusicVolume = 1.f;
	UPROPERTY() float SfxVolume = 1.f;
	UPROPERTY() float VoiceVolume = 1.f;
	UPROPERTY() float LookSensitivity = 1.f;
	/** 1 = the UI as authored. Multiplies the project's ApplicationScale. */
	UPROPERTY() float UiSize = 1.f;
	UPROPERTY() bool bInvertLook = false;
	UPROPERTY() bool bSmoothMouse = false;

	/** Action id to key. An action with no entry uses its default. */
	UPROPERTY() TMap<FName, FKey> Keys;
};

/** One rebindable action. */
struct FRobberKeyAction
{
	FName Id;
	FKey DefaultKey;
	/** Enhanced Input action it drives, or empty for the keys ARobberController binds itself. */
	const TCHAR* InputAction;
};

DECLARE_MULTICAST_DELEGATE(FOnRobberControlsChanged);

/**
 * Holds the saved settings and applies them: at start, after every map load and when the menu calls Save.
 * Sensitivity is read by AProductionGameCharacter::DoAim, and the Journal and Pause keys by ARobberController.
 */
UCLASS()
class PRODUCTIONGAME_API URobberSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static const TArray<FRobberKeyAction>& KeyActions();

	/** Short name for a key cap: "Space", "LMB", "L Shift". */
	static FText KeyName(const FKey& Key);

	URobberSettingsSave* GetSettings() const { return Settings; }

	FKey GetKey(FName ActionId) const;

	/** Rebinds in the saved settings only. Another action on that key takes this one's old key. Returns the swapped action, if any. */
	FName SetKey(FName ActionId, const FKey& Key);
	void ResetKeys();

	/** Writes the save and applies everything. */
	void Save();
	void Apply();
	/** Drops unsaved changes and applies what's on disk. */
	void Revert();
	/** Rescales the whole UI. Separate from Apply so the slider can wait for the mouse to let go. */
	void ApplyUiSize();
	/** The configured ApplicationScale that UiSize multiplies. */
	float GetBaseUiScale() const { return BaseUiScale; }

	/** Fired after keys are applied, so controllers can rebind the ones they own. */
	FOnRobberControlsChanged OnControlsChanged;

private:
	void LoadSave();
	void OnMapLoaded(UWorld* World);
	void ApplyAudio(UWorld* World);
	float BaseUiScale = 1.f;
	void ApplyKeys();
	void RestoreDefaultKeys();

	UPROPERTY(Transient)
	TObjectPtr<URobberSettingsSave> Settings;

	/** Loaded once so its mappings can be changed in memory; the asset on disk is never saved. */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DefaultContext;

	/** Mapping index in DefaultContext per action id, found before anything was changed. */
	TMap<FName, int32> MappingIndex;

	FDelegateHandle MapLoadedHandle;
};
