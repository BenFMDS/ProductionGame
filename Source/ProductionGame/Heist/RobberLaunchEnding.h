#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RobberLaunchEnding.generated.h"

class APawn;
class ARobberController;
class ARobberSpaceCamera;
class UAudioComponent;
class UImage;
class USoundBase;
class UTextBlock;

/**
 * The Escape Velocity ending on one machine, from the launch keys to the credits. Every widget already on screen goes
 * invisible, input stops and the launch music starts. The launch scene plays (RobberLaunchScene: the crew at Mission
 * Control turning the key, with the real players' robbers hidden; no scene in the level = the player's own view). At
 * ShotsSeconds it dips to black and comes up on the space shot (ARobberSpaceCamera and the space props hidden
 * above L_GameLevel), shows the end card over it, and holds while the server ends the run (Win at RunEndsAt) and the
 * lobby reloads underneath. Once the credits are up it dips to black again, hands the view back, fades away, gives
 * the HUD and input back and leaves the music to finish under the credits. With no space camera in the level the
 * card plays over plain black.
 */
UCLASS()
class PRODUCTIONGAME_API URobberLaunchEnding : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * The launch scene is 36 s, timed to the music, which starts 6.5 s into the track: the key turn and the rocket end
	 * where the first swell stops dead (25 s into the track), the suit-up scene where the second does (42.5 s). The dip
	 * is blackest at its end.
	 */
	static constexpr float ShotsSeconds = 35.f;
	static constexpr float FadeSeconds = 2.f;
	static constexpr float CardSeconds = 7.f;
	/** When the server ends the run. The card holds until then. */
	static constexpr float RunEndsAt = ShotsSeconds + FadeSeconds + CardSeconds;

	/** Local only. Starts the ending over everything, or returns the one already playing. */
	static URobberLaunchEnding* Play(ARobberController* Controller, USoundBase* Music);
	static URobberLaunchEnding* Find(const ARobberController* Controller);

	float GetElapsed() const { return Elapsed; }
	float GetBlackout() const;
	float GetLineOpacity() const;
	float GetPunchlineOpacity() const;
	FString GetCardText() const;
	int32 GetHiddenCount() const { return Hidden.Num(); }
	bool IsMusicPlaying() const;
	bool IsFinished() const { return bFinished; }
	bool IsInSpace() const { return SpaceCamera.IsValid(); }
	bool IsInScene() const { return bInScene; }
	/** How many times game music was found playing and stopped. */
	int32 GetMusicSilenced() const { return MusicSilenced; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Build();
	void Start(USoundBase* Music);
	void HideOtherWidgets();
	/** Stops every other music playing in the world, so only the launch track is heard. */
	void SilenceGameMusic();
	void EnterSpace();
	void LeaveSpace();
	void EndScene();
	void Finish();

	UPROPERTY(Transient)
	TObjectPtr<UImage> Blackout;

	/** The level's space camera, found at the start; none means the card plays over plain black. */
	TWeakObjectPtr<ARobberSpaceCamera> ShotCamera;
	/** Set while the view is on the space camera. */
	TWeakObjectPtr<ARobberSpaceCamera> SpaceCamera;
	/** The camera manager's own settings, given back after the shot. */
	bool bClientCameraUpdates = true;
	bool bClientSimulatingViewTarget = false;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Line;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Punchline;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicPlayer;

	/** Set while the launch scene plays. */
	bool bInScene = false;
	int32 MusicSilenced = 0;
	/** Players' robbers hidden for the scene (the stand-ins play them); only ones that were showing. */
	TArray<TWeakObjectPtr<APawn>> HiddenPawns;

	/** Widgets hidden for the ending, with the visibility to give back. */
	TArray<TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>> Hidden;

	float Elapsed = 0.f;
	/** World time at the start; Elapsed counts from it. */
	double StartedAt = -1.;
	/** Set once the run has ended and the outro fade has started. */
	float OutroAt = -1.f;
	bool bFinished = false;
	bool bHudWasShown = true;
};
