#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "RobberMenuMarker.generated.h"

class UTextBlock;
class UWidget;

/**
 * The yellow "!" over a menu object (main menu and lobby hideout), in the map-marker style. The name tag above it
 * pops up once you're close enough to use the object. Built in code, no widget Blueprint.
 */
UCLASS()
class PRODUCTIONGAME_API URobberMenuMarker : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetName(const FText& Name);
	void SetNameShown(bool bShown);
	bool IsNameShown() const { return bNameShown; }
	FText GetName() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWidget> Tag;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> Badge;

	bool bNameShown = false;
	float Age = 0.f;
	float NameShownFor = 0.f;
};

/**
 * Keeps a URobberMenuMarker above a menu object at a constant screen size. URobberMenuObjectSubsystem adds one to
 * every BP_MenuObject and ARobberLobbyObject on machines with a local player. Only the viewer's own distance counts.
 */
UCLASS()
class PRODUCTIONGAME_API URobberMenuMarkerComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	URobberMenuMarkerComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** What the tag says, e.g. "Settings". */
	UPROPERTY(EditAnywhere, Category = "Robber|Menu Marker")
	FText Label;

	/** Within this distance (cm), with a clear view, the name tag shows. Same as the outline's range by default. */
	UPROPERTY(EditAnywhere, Category = "Robber|Menu Marker")
	float NameRange = 500.f;

	/** Beyond this distance (cm), or out of sight, the "!" itself goes too. */
	UPROPERTY(EditAnywhere, Category = "Robber|Menu Marker")
	float MarkRange = 2500.f;

	/** How far above the top of the object the "!" floats (cm). */
	UPROPERTY(EditAnywhere, Category = "Robber|Menu Marker")
	float Gap = 25.f;

	URobberMenuMarker* GetMarker() const;

private:
	void PlaceAboveOwner();
};
