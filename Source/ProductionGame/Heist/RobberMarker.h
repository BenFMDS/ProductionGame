#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "RobberMarker.generated.h"

class UTextBlock;
class UWidget;

/**
 * The yellow "!" over something worth walking to (menu objects, keys), in the map-marker style. The name tag above it
 * pops up once you're close enough to use the object. Built in code, no widget Blueprint.
 */
UCLASS()
class PRODUCTIONGAME_API URobberMarkerWidget : public UUserWidget
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
 * Keeps a URobberMarkerWidget above its owner at a constant screen size. Add it to any prop; URobberMenuObjectSubsystem
 * also adds one to every BP_MenuObject and ARobberLobbyObject. Local only: just the viewer's own distance counts.
 */
UCLASS(ClassGroup = (Robber), meta = (BlueprintSpawnableComponent))
class PRODUCTIONGAME_API URobberMarkerComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	URobberMarkerComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** What the tag says, e.g. "Settings". Empty: just the "!". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robber|Marker")
	FText Label;

	/** Within this distance (cm) the name tag shows. Same as the menu objects' outline range by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robber|Marker")
	float NameRange = 500.f;

	/** Beyond this distance (cm) the "!" itself goes too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robber|Marker")
	float MarkRange = 2500.f;

	/** Hide the "!" while something is in the way. Off shows it through walls (a key hung under a shelf). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robber|Marker")
	bool bNeedsClearView = true;

	/** How far above the top of the object the "!" floats (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robber|Marker")
	float Gap = 25.f;

	/** The owner's own rule for when there's anything to mark (key not taken yet, right stage). */
	UFUNCTION(BlueprintCallable, Category = "Robber|Marker")
	void SetMarkerEnabled(bool bEnabled) { bMarkerEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Robber|Marker")
	bool IsMarkShowing() const { return IsVisible(); }

	URobberMarkerWidget* GetMarker() const;

private:
	void PlaceAboveOwner();
	bool IsWithin(const APlayerController* Viewer, double Range) const;

	bool bMarkerEnabled = true;
};
