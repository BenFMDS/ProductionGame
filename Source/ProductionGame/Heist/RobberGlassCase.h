#pragma once

#include "CoreMinimal.h"
#include "Heist/RobberInteractables.h"
#include "RobberGlassCase.generated.h"

class UStaticMesh;

/** One night's gem: which mesh and what colour. */
USTRUCT(BlueprintType)
struct FRobberNightGem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Gem")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Gem")
	FLinearColor Colour = FLinearColor::White;
};

/**
 * The glass case round the night's gem (BP_Gem). The glass blocks the interact trace, so the gem can't be taken
 * through it: the first Interact smashes the glass, then the gem's own Interact takes it and starts the escape.
 * Also dresses the gem as tonight's one (mesh and colour from NightGems).
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberGlassCase : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberGlassCase();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The gem inside. Empty: the nearest BP_Gem within the case. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Gem")
	TObjectPtr<AActor> Gem;

	/** Which night's gem this map shows, 1-7. 0 reads it from the night the game state has loaded. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Gem", meta = (ClampMin = "0", ClampMax = "7"))
	int32 Night = 0;

	/** Half the glass box's size (cm). The actor sits on the plinth: the glass goes up from there. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Gem")
	FVector CaseSize = FVector(30., 30., 35.);

	/** The gem is scaled so it's about this wide (cm), whatever size its mesh was made at. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Gem", meta = (ClampMin = "1"))
	float GemWidth = 24.f;

	/** Night 1 first. Repeated meshes get a different colour. A night past the end of the list wraps round. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Gem")
	TArray<FRobberNightGem> NightGems;

	bool IsSmashed() const { return bSmashed; }
	AActor* FindGem() const;

	/** NightGems[Night - 1], wrapped; from the class defaults when no case is placed (the hideout's trophy gems). */
	static const FRobberNightGem* GetNightGem(const ARobberGlassCase* Case, int32 Night);

	/** Puts a night's gem on a mesh component, scaled to about Width across and centred on the component's parent. */
	static void DressGem(UStaticMeshComponent* Target, const FRobberNightGem& NightGem, float Width);

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Gem")
	TObjectPtr<USceneComponent> Base;

	/** Corner posts, top rails and the base plate. */
	UPROPERTY(VisibleAnywhere, Category = "Robber|Gem")
	TArray<TObjectPtr<UStaticMeshComponent>> Frame;

	/** Broken glass on the plinth, shown once smashed. */
	UPROPERTY(VisibleAnywhere, Category = "Robber|Gem")
	TArray<TObjectPtr<UStaticMeshComponent>> Shards;

private:
	int32 GetNightNumber() const;
	void ShowSmashed();

	UPROPERTY(ReplicatedUsing = OnRep_Smashed)
	bool bSmashed = false;

	UFUNCTION()
	void OnRep_Smashed();
};
