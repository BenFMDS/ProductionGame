#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TextRenderComponent.h"
#include "RobberFloorMap.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

USTRUCT(BlueprintType)
struct FRobberFloorMapRoom
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor Map") FString Name;
	/** Picks the colour from Materials: exhibit, public, staff, storage... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor Map") FName Kind;
	/** Plan rectangles in metres (world X, Y divided by 100). More than one for L-shaped rooms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor Map") TArray<FBox2D> Rects;
};

/**
 * A "you are here" board (BP_FloorMap): one actor holding the board and a plan of its floor, built from its own transform, so
 * moving or turning it redraws the plan with YOU ARE HERE in the right place and up being the way the reader faces.
 * Set bDrawPlan off to keep just the board, for a Blueprint child with proper art.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberFloorMap : public AActor
{
	GENERATED_BODY()

public:
	ARobberFloorMap();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Map") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Map") TObjectPtr<UStaticMeshComponent> Board;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Map") bool bDrawPlan = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Map") FText FloorName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Map") TArray<FRobberFloorMapRoom> Rooms;
	/** Stair openings on this floor, in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Map") TArray<FBox2D> Stairs;
	/** By kind, plus "sign" for the board, "rail" for stairs and "red" for the marker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Map") TMap<FName, TObjectPtr<UMaterialInterface>> Materials;
	/** Thickness, width, height in cm. The plan and legend are laid out for the default size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Map") FVector BoardSize = FVector(12., 380., 280.);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Map") FColor Ink = FColor(0x1d, 0x21, 0x25);
	/** How far in front of the board the reader stands, in metres: where YOU ARE HERE points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Map") float ReaderDistance = 1.f;

	UFUNCTION(BlueprintCallable, Category = "Floor Map") void Rebuild();
	int32 GetPlanPartCount() const { return Generated.Num(); }
	/** Where the marker sits on the board, in the actor's space. */
	FVector GetHereOnBoard() const { return HereOnBoard; }

private:
	UStaticMeshComponent* AddBlock(const FVector& Location, const FVector& Size, const FQuat& Rotation, FName MaterialName);
	UTextRenderComponent* AddText(const FString& Text, const FVector& Location, float Size, const FColor& Colour, EHorizTextAligment Align = EHTA_Center);

	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Generated;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	FVector HereOnBoard = FVector::ZeroVector;
};
