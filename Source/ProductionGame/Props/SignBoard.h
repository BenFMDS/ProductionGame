#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SignBoard.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;

/**
 * A board with a title and body text, either standing on a post (pivot at its foot, on the floor) or hanging from two
 * rods (pivot at the top of the rods, on the ceiling). The board grows to fit the text. Text renders two-sided, so each
 * face gets its own copy and the opaque board hides the far one; without the board you'd see both copies, one mirrored.
 * Used through BP_Sign_Standing and BP_Sign_Hanging, which only set defaults.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ASignBoard : public AActor
{
	GENERATED_BODY()

public:
	ASignBoard();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign")
	FText Title = FText::FromString(TEXT("Sign"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign", meta = (MultiLine = true))
	FText Body;

	/** Wrap the body at this many characters per line. 0 keeps the lines as typed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign", meta = (ClampMin = 0))
	int32 WrapAt = 40;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign", meta = (ClampMin = 1))
	float TitleSize = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign", meta = (ClampMin = 1))
	float BodySize = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign")
	FColor TextColour = FColor(0x3a, 0x3f, 0x45);

	/** Hang from the ceiling on two rods instead of standing on a post. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign")
	bool bHanging = false;

	/** Post height under a standing board, or rod length above a hanging one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign", meta = (ClampMin = 0))
	float StemLength = 120.f;

	/** Text on the back as well as the front. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign")
	bool bBothSides = true;

	/** The board is at least this wide and tall; it grows past it to fit the text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign|Board")
	FVector2D MinBoardSize = FVector2D(100., 50.);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign|Board", meta = (ClampMin = 0))
	float Margin = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign|Board", meta = (ClampMin = 1))
	float Thickness = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign|Board")
	TObjectPtr<UMaterialInterface> BoardMaterial;

	/** Post and rods. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sign|Board")
	TObjectPtr<UMaterialInterface> FrameMaterial;

	/** Size of the board as last built, for lining signs up. */
	UFUNCTION(BlueprintPure, Category = "Sign")
	FVector2D GetBoardSize() const { return BoardSize; }

	/** Breaks Text into lines of at most Width characters at spaces, keeping existing line breaks. */
	static FString WrapText(const FString& Text, int32 Width);

private:
	void Rebuild();

	UPROPERTY(VisibleAnywhere, Category = "Sign")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Sign")
	TObjectPtr<UStaticMeshComponent> Board;

	UPROPERTY(VisibleAnywhere, Category = "Sign")
	TObjectPtr<UStaticMeshComponent> Post;

	UPROPERTY(VisibleAnywhere, Category = "Sign")
	TObjectPtr<UStaticMeshComponent> RodLeft;

	UPROPERTY(VisibleAnywhere, Category = "Sign")
	TObjectPtr<UStaticMeshComponent> RodRight;

	UPROPERTY(VisibleAnywhere, Category = "Sign")
	TObjectPtr<UTextRenderComponent> TitleFront;

	UPROPERTY(VisibleAnywhere, Category = "Sign")
	TObjectPtr<UTextRenderComponent> TitleBack;

	UPROPERTY(VisibleAnywhere, Category = "Sign")
	TObjectPtr<UTextRenderComponent> BodyFront;

	UPROPERTY(VisibleAnywhere, Category = "Sign")
	TObjectPtr<UTextRenderComponent> BodyBack;

	UPROPERTY()
	FVector2D BoardSize = FVector2D::ZeroVector;
};
