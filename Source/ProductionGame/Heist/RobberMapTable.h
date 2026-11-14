#pragma once

#include "CoreMinimal.h"
#include "Heist/RobberInteractables.h"
#include "RobberMapTable.generated.h"

class APlayerController;
class ARobberMapObject;
class UCameraComponent;
class UMaterialInstanceDynamic;
class USceneCaptureComponent2D;
class UStaticMesh;
class UTextureRenderTarget2D;

/**
 * The lobby's planning table: a flat top-down picture of the loaded night, with a BP_MapObject standing on each way in.
 * Interact to lean over it: the view moves to a fixed overhead camera and the mouse cursor comes up, so pieces are
 * hovered and clicked like a strategy map. Interact again, Space or Esc to step back.
 *
 * The picture is the night's base colour seen straight down (no lighting), captured on each machine whenever the
 * streamed-in levels change, so any night built on the base plate gets a map without anyone drawing one.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberMapTable : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberMapTable();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Where WorldLocation sits on the map's top surface. False when it's outside the mapped area. */
	bool ProjectToMap(const FVector& WorldLocation, FVector& OutMapLocation) const;

	/** Points the overhead camera straight down, with the way the crew faces across the table up on screen. */
	void AimViewCamera();

	/**
	 * Whether Interact on the table leans over it (overhead camera, mouse cursor, click to vote). Off for now: the crew
	 * votes by pressing Interact on a piece from beside the table.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robber|Map")
	bool bCanLeanIn = false;

	/** The world area the map shows: the base plate every night is built on, in uu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Map")
	FVector2D MapWorldCentre = FVector2D(4800., 3300.);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Map")
	FVector2D MapWorldSize = FVector2D(18400., 15400.);

	/** Map size / world size. 1:80 makes the plate a 2.3 x 1.9 m map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Map")
	double MapScale = 1. / 80.;

	/** Height of the map surface above the actor's origin (the floor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Map")
	double MapTopZ = 90.;

	/** Which way (relative to the table) the crew faces across it. The overhead view keeps that way up on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Map")
	float ViewerYaw = 90.f;

	/**
	 * Overhead camera height above the map: high enough to show it all on a 16:9 screen, and under the 300 uu interact
	 * reach, so the centre-screen prompt still finds the table.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Map")
	double ViewHeight = 275.;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Map")
	FIntPoint MapResolution = FIntPoint(1024, 856);

	UTextureRenderTarget2D* GetMapTexture() const { return MapTexture; }
	int32 GetCaptureCount() const { return CaptureCount; }

protected:
	virtual void BeginPlay() override;
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual FText GetLabel(const ARobberGameState* GameState) const override { return FText::GetEmpty(); }

private:
	void CaptureMap();
	void UpdateHover(APlayerController* LocalPlayer);
	void UpdateViewer(APlayerController* LocalPlayer);
	FString GetShownLevels() const;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Map")
	TObjectPtr<USceneComponent> TableRoot;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Map")
	TObjectPtr<UStaticMeshComponent> MapSurface;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Map")
	TArray<TObjectPtr<UStaticMeshComponent>> Legs;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Map")
	TObjectPtr<UCameraComponent> ViewCamera;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Map")
	TObjectPtr<USceneCaptureComponent2D> MapCapture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> MapTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MapMaterial;

	UPROPERTY(Transient)
	TObjectPtr<ARobberMapObject> HoveredPiece;

	FString CapturedLevels;
	double LevelsCheckTime = 0.;
	/** World times of the captures still to take: one once a new night is shown, one later for streamed-in textures. */
	TArray<double> PendingCaptures;
	int32 CaptureCount = 0;
};

/**
 * A piece on the planning table for one way in (EntryId), like BP_MenuObject: outlined when aimed at, its name on
 * screen and floating above it. Interact to vote. Every machine moves it to its entry's spot on the map whenever a
 * night loads (hidden on nights without that entry) and stands a pin in each voter's colour round it.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberMapObject : public ARobberVoteStation
{
	GENERATED_BODY()

public:
	ARobberMapObject();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Map")
	TObjectPtr<ARobberMapTable> Table;

	/** Name shown above the piece and on screen. Empty uses the entry's DisplayName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Map")
	FText DisplayText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Map")
	TObjectPtr<UStaticMesh> ObjectMesh;

	/** ObjectMesh's scale; it stands on the base, so a 100 uu engine shape at 0.1 is 10 cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Map")
	FVector ObjectScale = FVector(0.08);

	/** Local only, from the table's hover trace. */
	void SetHovered(bool bHovered);
	bool IsHovered() const { return bHovered; }
	/** The table's own hover trace lights it. */
	virtual bool OutlinesItself() const override { return true; }
	bool IsOnMap() const { return bOnMap; }
	int32 GetPinCount() const;

protected:
	virtual void BeginPlay() override;
	virtual FText GetLabel(const ARobberGameState* GameState) const override;

private:
	struct FPin
	{
		TWeakObjectPtr<APlayerState> Voter;
		double DropTime = 0.;
	};

	void ApplyObjectMesh();
	void UpdatePlacement(const ARobberGameState* GameState);
	void UpdatePins(const ARobberGameState* GameState);
	void FaceLabelToCamera();
	void AddPinParts();

	UPROPERTY(VisibleAnywhere, Category = "Robber|Map")
	TObjectPtr<USceneComponent> PieceRoot;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Map")
	TObjectPtr<UStaticMeshComponent> Base;

	/** Needle + ball per pin, made as votes come in and reused after. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PinNeedles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PinBalls;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PinColours;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PinNeedleMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PinBallMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PinMaterial;

	TArray<FPin> Pins;
	bool bOnMap = true;
	bool bHovered = false;
	double PlacementCheckTime = -1.;
};
