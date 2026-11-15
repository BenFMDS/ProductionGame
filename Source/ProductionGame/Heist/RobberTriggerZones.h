#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobberTriggerZones.generated.h"

class APawn;
class ARobberEntryPoint;
class ARobberGameState;
class UBoxComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/** What walking into a trigger does. */
UENUM(BlueprintType)
enum class ERobberTriggerKind : uint8
{
	/** Shows Message to each player who walks in, and fires On Player Entered. Nothing else. */
	Message,
	/** In front of the gem room door: the first player in during Investigate starts the heist. */
	GemRoom,
	/** The way out: during Escape, the whole crew standing in it wins the night (only on a run that came in by OwningEntry). */
	EscapeExit,
	/** A way out of the lobby: everyone inside, with the winning entry leaving by ExitId, starts the break-in. */
	LobbyExit,
	/** The Sewer entry's lobby exit (ExitId "Manhole"), drawn as a lid on the floor. */
	Manhole,
};

/** How a trigger is drawn. Auto picks from the kind. */
UENUM(BlueprintType)
enum class ERobberTriggerLook : uint8
{
	/** Door/Gem room/Lobby exit = Door, Escape exit = Floor Pad, Manhole = Manhole Lid, Message = Hidden. */
	Auto,
	/** A see-through block filling the box. */
	Door,
	/** A thin pad on the floor of the box. */
	FloorPad,
	/** A round lid on the floor of the box. */
	ManholeLid,
	/** Nothing drawn and no label: an invisible trigger. */
	Hidden,
};

/**
 * The one walk-in trigger (BP_Trigger). Pick what it does with Kind, size it with Size, and style it with Look, Colour,
 * Material and Label. Placed in every night map, the game level and the dev maps.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberTriggerZone : public AActor
{
	GENERATED_BODY()

public:
	ARobberTriggerZone();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Player-controlled pawns standing in the zone, and the number of players in the game. */
	void CountPlayers(int32& OutInside, int32& OutTotal) const;

	/** Escape exits only show and count on a run that came in by their OwningEntry; every other kind is always active. */
	bool IsActive(const ARobberGameState* GameState) const;

	/** Every trigger of a kind in the world (Manhole counts as a lobby exit too). */
	static TArray<ARobberTriggerZone*> FindAll(const UWorld* World, ERobberTriggerKind Kind);
	static ARobberTriggerZone* Find(const UWorld* World, ERobberTriggerKind Kind);
	/** The lobby exit with this ExitId (None = the museum door). */
	static ARobberTriggerZone* FindLobbyExit(const UWorld* World, FName ExitId);

	bool IsLobbyExit() const { return Kind == ERobberTriggerKind::LobbyExit || Kind == ERobberTriggerKind::Manhole; }

	/**
	 * The coloured shapes are a level-design aid: always drawn in the editor, hidden in game unless this is on.
	 * Console `Robber.ShowTriggers 1`, or the dev menu's Triggers button. Local to this machine. Labels still show.
	 */
	static bool AreShapesShownInGame();
	static void SetShapesShownInGame(bool bShow);

	/** Whether this trigger's shape is drawing in game right now (for tests). */
	bool IsShapeShownInGame() const;

	UFUNCTION(BlueprintPure, Category = "Robber|Trigger")
	ERobberTriggerLook GetResolvedLook() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Trigger")
	ERobberTriggerKind Kind = ERobberTriggerKind::Message;

	/** Half the box's size in each direction, in cm (before the actor's scale). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Trigger", meta = (ClampMin = "1"))
	FVector Size = FVector(100, 150, 150);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Trigger|Look")
	ERobberTriggerLook Look = ERobberTriggerLook::Auto;

	/** Tints the shape (and the label). Alpha 0 = leave the material's own colour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Trigger|Look")
	FLinearColor Colour = FLinearColor(0, 0, 0, 0);

	/** The shape's material. None = the engine's plain shape material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Trigger|Look")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Trigger|Look")
	bool bShowLabel = true;

	/** The words over the trigger. Gem room, exits and doors add their own status lines under it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Trigger|Look")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Trigger|Look", meta = (ClampMin = "1"))
	float LabelSize = 36.f;

	/** Message kind: shown to each player who walks in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Trigger", meta = (EditCondition = "Kind == ERobberTriggerKind::Message", MultiLine = "true"))
	FText Message;

	/** Message kind: only the first time each player walks in, not every time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Trigger", meta = (EditCondition = "Kind == ERobberTriggerKind::Message"))
	bool bOncePerPlayer = true;

	/** Lobby exit: which ARobberEntryPoint::LobbyExit sends the crew in through here. None = the museum door. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Trigger", meta = (EditCondition = "Kind == ERobberTriggerKind::LobbyExit"))
	FName ExitId;

	/** Escape exit: the way in this is the way out for. None = any. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Trigger", meta = (EditCondition = "Kind == ERobberTriggerKind::EscapeExit"))
	TObjectPtr<ARobberEntryPoint> OwningEntry;

protected:
	virtual void BeginPlay() override;

	/** Server only, when a player's pawn walks in (any kind). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Robber|Trigger", meta = (DisplayName = "On Player Entered"))
	void ReceivePlayerEntered(APawn* Pawn);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Trigger")
	TObjectPtr<UBoxComponent> Box;

	/** Visual only: players walk through it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Trigger")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Trigger")
	TObjectPtr<UTextRenderComponent> LabelText;

	/** Box size, shape, material, colour and label from the settings. */
	void ApplyLook();

private:
	void ServerUpdate(ARobberGameState* GameState, int32 Inside, int32 Total);
	FText GetStatusLabel(const ARobberGameState* GameState, int32 Inside, int32 Total) const;
	void NoticeEntries();

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	TSet<TWeakObjectPtr<APawn>> PawnsInside;
	TSet<TWeakObjectPtr<APawn>> PawnsTold;
};

/** Old per-kind classes, kept so old maps and the build tools still load: each is just a trigger with its Kind preset. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberLobbyExit : public ARobberTriggerZone
{
	GENERATED_BODY()

public:
	ARobberLobbyExit();
};

UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberManhole : public ARobberLobbyExit
{
	GENERATED_BODY()

public:
	ARobberManhole();
};

UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberEscapeExit : public ARobberTriggerZone
{
	GENERATED_BODY()

public:
	ARobberEscapeExit();
};

UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberGemRoomApproach : public ARobberTriggerZone
{
	GENERATED_BODY()

public:
	ARobberGemRoomApproach();
};
