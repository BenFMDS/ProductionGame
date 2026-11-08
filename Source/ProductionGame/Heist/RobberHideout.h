#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Heist/RobberInteractables.h"
#include "RobberHideout.generated.h"

class UStaticMeshComponent;

/** What a hideout object in the lobby opens for the player who used it. */
UENUM(BlueprintType)
enum class ERobberLobbyMenu : uint8
{
	Settings,
	Achievements,
	Quit,
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

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Lobby")
	ERobberLobbyMenu Menu = ERobberLobbyMenu::Settings;

	bool IsHovered() const { return bHovered; }

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;

private:
	bool bHovered = false;
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
