#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Heist/RobberInteract.h"
#include "QuestDialogueCharacter.generated.h"

class UDialogueComponent;
class UDialogueDefinition;
class UMaterialInterface;
class UQuestGiverComponent;
class UQuestDefinition;
class URobberHighlightComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EExhibitLook : uint8
{
	Cube,
	Painting,
	Statue
};

/** Parent for talking-character Blueprints. The Dialogue Component's asset is the runtime source of truth. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API AQuestDialogueCharacter : public ACharacter, public IRobberInteractOwner
{
	GENERATED_BODY()
public:
	AQuestDialogueCharacter();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(DisplayName="Dialogue Component")) TObjectPtr<UDialogueComponent> NativeDialogue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest", meta=(DisplayName="Quest Giver Component")) TObjectPtr<UQuestGiverComponent> NativeQuestGiver;
	/** Legacy compatibility field. It is copied to the Dialogue Component only when that component has no assigned asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(DisplayName="Dialogue Asset (Legacy)")) TObjectPtr<UDialogueDefinition> NativeDialogueDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(DisplayName="Available Quests")) TArray<TObjectPtr<UQuestDefinition>> NativeQuestDefinitions;
	/** Placeholder colour for the Model Mesh until this NPC has real art. Leave empty to keep the mesh's own material. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dialogue", meta=(DisplayName="Temp Cube Material")) TObjectPtr<UMaterialInterface> TempCubeMaterial;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Exhibit Look") EExhibitLook ExhibitLook = EExhibitLook::Cube;
	// The rest are set once on BP_TalkingMaster and inherited. They're properties, not paths, so the assets get cooked.
	UPROPERTY(EditDefaultsOnly, Category="Exhibit Look") TObjectPtr<UStaticMesh> PaintingPlane;
	UPROPERTY(EditDefaultsOnly, Category="Exhibit Look") TObjectPtr<UStaticMesh> FrameBar;
	UPROPERTY(EditDefaultsOnly, Category="Exhibit Look") TObjectPtr<UMaterialInterface> FrameMaterial;
	/** Width, height of the picture in cm, not counting the frame. */
	UPROPERTY(EditDefaultsOnly, Category="Exhibit Look") FVector2D PaintingSize = FVector2D(140., 180.);
	UPROPERTY(EditDefaultsOnly, Category="Exhibit Look") TObjectPtr<USkeletalMesh> StatueMesh;
	UPROPERTY(EditDefaultsOnly, Category="Exhibit Look") TArray<TObjectPtr<UMaterialInterface>> StatueMaterials;
	/** A statue with its own art (the Moai): shown instead of the stone mannequin, with the mesh's own materials. */
	UPROPERTY(EditDefaultsOnly, Category="Exhibit Look") TObjectPtr<UStaticMesh> StatueModel;
	/** How tall StatueModel stands, in cm; it's scaled evenly to this. */
	UPROPERTY(EditDefaultsOnly, Category="Exhibit Look") float StatueModelHeight = 240.f;
	/** Turns StatueModel so its face looks the way the exhibit faces; imported meshes face all sorts of ways. */
	UPROPERTY(EditDefaultsOnly, Category="Exhibit Look") float StatueModelYaw = 0.f;
	UPROPERTY(VisibleAnywhere, Category="Exhibit Look") TObjectPtr<UStaticMeshComponent> StatueModelMesh;
	UPROPERTY(VisibleAnywhere, Category="Exhibit Look") TArray<TObjectPtr<UStaticMeshComponent>> FrameBars;
	/** Red outline on whatever shows of the exhibit (painting and frame, statue, cube) while aimed at. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact") TObjectPtr<URobberHighlightComponent> Highlight;
	/**
	 * Prompt and press. BP_TalkingMaster's BPI_Interaction route still does the real talking until stage 5; this gives
	 * C++ (the outline now, the focus trace later) the same answer. Presses aren't server-only: TalkToNPC sends ServerTalkTo.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact") TObjectPtr<URobberInteractComponent> InteractComponent;
	/** BP_TalkingMaster's "Interact Text" ("Talk to <speaker>"); empty while talking to exhibits is off. */
	virtual FText GetInteractText(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual void OnInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	/** How far a painting's actor stands off its wall, so the capsule clears it. The picture sits back against the wall. */
	static constexpr double PaintingWallGap = 45.;
	/** Picture centre above the actor's origin: eye level for a capsule standing on the floor. */
	static constexpr double PaintingCentreHeight = 65.;
	static constexpr double FrameThickness = 12.;
	virtual void OnConstruction(const FTransform& Transform) override;
	/** BP_TalkingMaster's On Puzzle Solved (BPI_PuzzleSolver) calls this. Passes it to the night director, server only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest")
	void HandlePuzzleSolved(AActor* Puzzle, APawn* SolvedBy);
protected:
	virtual void BeginPlay() override;
private:
	void ApplyExhibitLook();
};
