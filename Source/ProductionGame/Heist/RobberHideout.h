#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "Heist/RobberHighlight.h"
#include "Heist/RobberInteractables.h"
#include "RobberHideout.generated.h"

class UStaticMeshComponent;
class URobberMarkerComponent;

/** What a hideout object in the lobby opens for the player who used it. */
UENUM(BlueprintType)
enum class ERobberLobbyMenu : uint8
{
	Settings,
	Achievements,
	Quit,
	Catalogue,
};

/**
 * A main-menu object in the lobby hideout (L_GameLevel): looks like its BP_MenuObject twin in L_MainMenu (a box
 * prop, outlined when aimed at, its name floating above and turned to the viewer) and opens the same screen, but
 * only for the player who pressed it. Quit asks first, like the pause menu.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberLobbyObject : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberLobbyObject();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Lobby")
	ERobberLobbyMenu Menu = ERobberLobbyMenu::Settings;

	/** Lit from anywhere in HighlightRange with a clear view, not only when aimed at. */
	bool IsHovered() const;

	/** How close (cm) a player has to be for a menu object to light up and name its "!". */
	static constexpr float HighlightRange = 500.f;

	/** What this object opens, e.g. "Settings": the prompt and the name over its "!". */
	FText GetMenuName() const;

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override { return GetMenuName(); }
	/** The name and what it does are on the prompt now; nothing floats over the object. */
	virtual FText GetLabel(const ARobberGameState* GameState) const override { return FText::GetEmpty(); }
};

/**
 * Does for the main menu's Blueprint BP_MenuObjects what ARobberLobbyObject does for itself: no floating text, lit up
 * in range (a URobberHighlightComponent), a prompt saying what it does. No world objects or Blueprints to change, so
 * it can't be lost in a merge. Also puts a "!" (URobberMarkerComponent) over every menu object in both rooms, named
 * when you're in range.
 */
UCLASS()
class PRODUCTIONGAME_API URobberMenuObjectSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& World) override;

	/** The objects it has taken over, for tests. */
	const TArray<TWeakObjectPtr<AActor>>& GetObjects() const { return Objects; }

	/** The "!" it put over every menu object, BP_MenuObject and ARobberLobbyObject alike. */
	const TArray<TWeakObjectPtr<URobberMarkerComponent>>& GetMarkers() const { return Markers; }

private:
	void AddMarker(AActor* Object, const FText& Name);
	void AddHighlight(AActor* Object);

	TArray<TWeakObjectPtr<AActor>> Objects;
	TArray<TWeakObjectPtr<URobberMarkerComponent>> Markers;
};

/**
 * Seven gem spots in the hideout. Gem N shows once Night N has been won this run: nights before the loaded one are
 * done, and all seven stay out after the last night until the crew starts Night 1 again. Empty in the main menu.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberGemCase : public AActor
{
	GENERATED_BODY()

public:
	ARobberGemCase();

	virtual void Tick(float DeltaSeconds) override;

	/** Gems on show from the game state; 0 outside the game level. */
	int32 GetShownCount() const { return ShownCount; }

	static int32 GemsWon(const class ARobberGameState* GameState);

	static constexpr int32 GemCount = 7;

	/** Where each night's gem sits, relative to the actor, Night 1 first. Fewer than seven: a straight row, Spacing apart. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Gems", meta = (MakeEditWidget = "true"))
	TArray<FVector> GemSpots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Gems")
	double Spacing = 40.;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void ShowGems(int32 Count);

	UPROPERTY(VisibleAnywhere, Category = "Robber|Gems")
	TObjectPtr<USceneComponent> CaseRoot;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Gems")
	TArray<TObjectPtr<UStaticMeshComponent>> Gems;

	int32 ShownCount = -1;
};
