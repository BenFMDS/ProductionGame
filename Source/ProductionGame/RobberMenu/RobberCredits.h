#pragma once

#include "CoreMinimal.h"
#include "RobberMenu/RobberMenuWidgets.h"
#include "RobberCredits.generated.h"

class UAudioComponent;
class USoundBase;

DECLARE_MULTICAST_DELEGATE(FRobberCreditsFinished);

/**
 * Logic for WBP_RobberCredits: rolls CreditsRoll up the screen from below the bottom edge until it's gone off the top,
 * then closes. Esc or the BackButton ("SKIP") closes it early. The words live in the WBP, made from CREDITS.txt by
 * -run=RobberMenuWidgets -Credits (edit either; rebuilding from the .txt overwrites hand edits).
 * Rolls on every machine when the last night is won (ARobberGameState), and from the dev menu.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberCredits : public URobberMenuWidget
{
	GENERATED_BODY()

public:
	/** Fires once, whether the roll finished or was skipped. Endings that need to carry on afterwards hook in here. */
	FRobberCreditsFinished OnFinished;

	/** Stage pixels (1920 x 1080 stage) per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits", meta = (ClampMin = "10"))
	float ScrollSpeed = 70.f;

	/** Played while the credits roll, stopped when they close. Empty = silence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits")
	TObjectPtr<USoundBase> Music;

	float GetScrolled() const { return Scrolled; }
	virtual void CloseMenu() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	float Scrolled = 0.f;
	bool bFinished = false;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicPlayer;
};
