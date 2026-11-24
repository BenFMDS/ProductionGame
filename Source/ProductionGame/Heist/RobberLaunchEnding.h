#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RobberLaunchEnding.generated.h"

class ARobberController;
class UAudioComponent;
class UImage;
class USoundBase;
class UTextBlock;

/**
 * The Escape Velocity ending on one machine, from the launch keys to the credits. Every widget already on screen goes
 * invisible, input stops and the launch music starts. After ShotsSeconds (where the rocket shots will go; for now the
 * player's own view) it fades to black, shows the end card, and stays black while the server ends the run (Win at
 * RunEndsAt) and the lobby reloads behind it. Once the credits are up it fades away, gives the HUD and input back and
 * leaves the music to finish under the credits.
 */
UCLASS()
class PRODUCTIONGAME_API URobberLaunchEnding : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr float ShotsSeconds = 4.f;
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
	float GetYetOpacity() const;
	FString GetCardText() const;
	int32 GetHiddenCount() const { return Hidden.Num(); }
	bool IsMusicPlaying() const;
	bool IsFinished() const { return bFinished; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Build();
	void Start(USoundBase* Music);
	void HideOtherWidgets();
	void Finish();

	UPROPERTY(Transient)
	TObjectPtr<UImage> Blackout;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Line;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Yet;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicPlayer;

	/** Widgets hidden for the ending, with the visibility to give back. */
	TArray<TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>> Hidden;

	float Elapsed = 0.f;
	/** Set once the run has ended and the outro fade has started. */
	float OutroAt = -1.f;
	bool bFinished = false;
	bool bHudWasShown = true;
};
