#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobberMenuIntroDirector.generated.h"

class ACameraActor;
class UUserWidget;
class URobberMenuIntroWidget;

/**
 * Place one of these in L_MainMenu and assign a camera plus the new intro
 * widget.  It runs a close-to-wide camera pull-back, waits for any key, and
 * blends to the player's normal first-person camera.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberMenuIntroDirector : public AActor
{
	GENERATED_BODY()

public:
	ARobberMenuIntroDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Camera controlled during the opening pull-back. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "RobberMenu|Camera")
	TObjectPtr<ACameraActor> IntroCamera;

	/** Close composition at the very start of the intro. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobberMenu|Camera")
	FTransform PullbackStart = FTransform::Identity;

	/** Wide composition which should visually match the player's start view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobberMenu|Camera")
	FTransform PullbackEnd = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobberMenu|Camera", meta = (ClampMin = "0.1"))
	float PullbackDuration = 3.5f;

	/** Forced logo/title hold before the input prompt can appear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobberMenu|Flow", meta = (ClampMin = "0.0"))
	float LogoHoldDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobberMenu|Camera", meta = (ClampMin = "0.0"))
	float PlayerCameraBlendDuration = 0.65f;

	/** A new WBP derived from URobberMenuIntroWidget; existing widgets are never used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RobberMenu|UI")
	TSubclassOf<URobberMenuIntroWidget> IntroWidgetClass;

	/** Optional new in-world/menu overlay widget to show after the intro. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RobberMenu|UI")
	TSubclassOf<UUserWidget> WalkaroundMenuWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobberMenu|Flow")
	bool bWaitForAnyKey = true;

	UFUNCTION(BlueprintCallable, Category = "RobberMenu|Flow")
	void StartIntro();

	UFUNCTION(BlueprintCallable, Category = "RobberMenu|Flow")
	void ContinueFromIntro();

private:
	void BeginPullback();
	void BeginWaitingForKey();
	void FinishIntro();
	void RemoveQuestWidgets();
	void EnableWalkaround();
	void BindAnyKey();

	float ElapsedPullbackTime = 0.0f;
	bool bIntroPlaying = false;
	bool bWaitingForKey = false;
	bool bHandedToPlayer = false;
	FTimerHandle LogoHoldTimer;
	FTimerHandle WalkaroundEnableTimer;

	UPROPERTY(Transient)
	TObjectPtr<URobberMenuIntroWidget> IntroWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> WalkaroundMenuWidget;
};
