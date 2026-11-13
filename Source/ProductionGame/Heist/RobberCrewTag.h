#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "Heist/RobberCaseFileUI.h"
#include "RobberCrewTag.generated.h"

/**
 * Logic for WBP_CrewTag, the small tag above a teammate's head. Fills TagColour (border), TagName and TagStatus
 * (collapsed unless there's something worth saying).
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberCrewTag : public URobberCaseFileWidgetBase
{
	GENERATED_BODY()

public:
	void Refresh(const APlayerState* Player, bool bFar);
	FString GetShownName() const;
	bool IsStatusShown() const;

	/** Size of the tag on screen once its owner is further away than the component's FarDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	float FarScale = 0.75f;
};

/**
 * Keeps a WBP_CrewTag above one player, facing the camera at a constant screen size. Added to every character on
 * clients and listen servers. Hidden on your own character and whenever something blocks the view of them.
 */
UCLASS()
class PRODUCTIONGAME_API URobberCrewTagComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	URobberCrewTagComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Beyond this distance (cm) the tag shrinks to the widget's FarScale. */
	UPROPERTY(EditAnywhere, Category = "Case File")
	float FarDistance = 1500.f;

	/** How far above the top of the capsule the tag sits (cm). */
	UPROPERTY(EditAnywhere, Category = "Case File")
	float HeadGap = 30.f;

	/** Whether this frame's checks left the tag showing. */
	bool WantsToShow() const { return bWantsToShow; }

private:
	bool bWantsToShow = false;
	bool ShouldShow(const APawn* Pawn, const APlayerController* Viewer) const;
};
