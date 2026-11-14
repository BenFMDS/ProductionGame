#include "Dialogue/QuestDialogueCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dialogue/DialogueComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Heist/RobberHighlight.h"
#include "Heist/RobberPuzzle.h"
#include "Quest/QuestGiverComponent.h"

AQuestDialogueCharacter::AQuestDialogueCharacter()
{
	bReplicates = true;
	NativeDialogue = CreateDefaultSubobject<UDialogueComponent>(TEXT("NativeDialogue"));
	NativeQuestGiver = CreateDefaultSubobject<UQuestGiverComponent>(TEXT("NativeQuestGiver"));
	for (const TCHAR* Name : { TEXT("FrameTop"), TEXT("FrameBottom"), TEXT("FrameLeft"), TEXT("FrameRight") })
	{
		UStaticMeshComponent* Bar = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Bar->SetupAttachment(GetCapsuleComponent());
		Bar->SetMobility(EComponentMobility::Movable);
		Bar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bar->SetGenerateOverlapEvents(false);
		Bar->SetVisibility(false);
		FrameBars.Add(Bar);
	}
	// Looks only: talking still aims at ModelMesh, so this needs no collision of its own.
	StatueModelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatueModelMesh"));
	StatueModelMesh->SetupAttachment(GetCapsuleComponent());
	StatueModelMesh->SetMobility(EComponentMobility::Movable);
	StatueModelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StatueModelMesh->SetGenerateOverlapEvents(false);
	StatueModelMesh->SetVisibility(false);
	// BPC_Interaction only outlines BP_InteractMaster children, so talkers had no outline of their own.
	Highlight = CreateDefaultSubobject<URobberHighlightComponent>(TEXT("RobberHighlight"));
}

void AQuestDialogueCharacter::ApplyExhibitLook()
{
	// ModelMesh is a component on BP_TalkingMaster, so it can only be found by name from here.
	UStaticMeshComponent* Model = nullptr;
	TInlineComponentArray<UStaticMeshComponent*> Parts;
	GetComponents(Parts);
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part->GetFName() == TEXT("ModelMesh"))
		{
			Model = Part;
		}
	}
	if (!Model)
	{
		return;
	}
	if (TempCubeMaterial)
	{
		Model->SetMaterial(0, TempCubeMaterial);
	}

	const bool bPainting = ExhibitLook == EExhibitLook::Painting && PaintingPlane && FrameBar && FrameMaterial;
	for (UStaticMeshComponent* Bar : FrameBars)
	{
		Bar->SetVisibility(bPainting);
	}
	if (bPainting)
	{
		const double Width = PaintingSize.X;
		const double Height = PaintingSize.Y;
		const double PlaneX = 3. - PaintingWallGap;
		const double BarX = PlaneX + 1.;
		const double OuterWidth = Width + 2. * FrameThickness;
		Model->SetStaticMesh(PaintingPlane);
		// The plane faces +Z with its X along the height, so pitching it stands it up facing the room.
		Model->SetRelativeLocationAndRotation(FVector(PlaneX, 0., PaintingCentreHeight), FRotator(-90., 0., 0.));
		Model->SetRelativeScale3D(FVector(Height / 100., Width / 100., 1.));
		const struct { FVector Where; FVector Size; } Bars[] = {
			{ FVector(BarX, 0., PaintingCentreHeight + (Height + FrameThickness) / 2.), FVector(8., OuterWidth, FrameThickness) },
			{ FVector(BarX, 0., PaintingCentreHeight - (Height + FrameThickness) / 2.), FVector(8., OuterWidth, FrameThickness) },
			{ FVector(BarX, -(Width + FrameThickness) / 2., PaintingCentreHeight), FVector(8., FrameThickness, Height) },
			{ FVector(BarX, (Width + FrameThickness) / 2., PaintingCentreHeight), FVector(8., FrameThickness, Height) },
		};
		for (int32 Index = 0; Index < FrameBars.Num(); ++Index)
		{
			FrameBars[Index]->SetStaticMesh(FrameBar);
			FrameBars[Index]->SetMaterial(0, FrameMaterial);
			FrameBars[Index]->SetRelativeLocation(Bars[Index].Where);
			FrameBars[Index]->SetRelativeScale3D(Bars[Index].Size / 100.);
		}
	}
	else if (ExhibitLook == EExhibitLook::Statue && StatueMesh)
	{
		// Invisible but still colliding: the interact trace is on Visibility, which the capsule and the mannequin ignore.
		Model->SetVisibility(false);
		Model->SetRelativeScale3D(FVector(0.7, 0.7, 1.8));
		USkeletalMeshComponent* Body = GetMesh();
		const double Floor = -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		if (StatueModel)
		{
			Body->SetVisibility(false);
			const FBox Bounds = StatueModel->GetBoundingBox();
			const double Scale = Bounds.GetSize().Z > 1. ? StatueModelHeight / Bounds.GetSize().Z : 1.;
			StatueModelMesh->SetStaticMesh(StatueModel);
			StatueModelMesh->SetRelativeScale3D(FVector(Scale));
			// Stood on the floor, centred over the capsule whatever the mesh's own pivot.
			const FRotator Turn(0., StatueModelYaw, 0.);
			const FVector Centre = Turn.RotateVector(FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, 0.) * Scale);
			StatueModelMesh->SetRelativeLocationAndRotation(FVector(-Centre.X, -Centre.Y, Floor - Bounds.Min.Z * Scale), Turn);
			StatueModelMesh->SetVisibility(true);
			return;
		}
		Body->SetSkeletalMeshAsset(StatueMesh);
		Body->SetRelativeLocationAndRotation(FVector(0., 0., Floor), FRotator(0., -90., 0.));
		for (int32 Slot = 0; Slot < StatueMaterials.Num(); ++Slot)
		{
			Body->SetMaterial(Slot, StatueMaterials[Slot]);
		}
	}
}

void AQuestDialogueCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyExhibitLook();
}

void AQuestDialogueCharacter::BeginPlay()
{
	// Actors already placed in a saved map don't rerun construction, so this catches a look changed since the map was saved.
	ApplyExhibitLook();
	// Blueprint component defaults are the authored source of truth. Older assets may only
	// have the legacy field populated, so use it solely as a fallback and then keep it synced.
	if (!NativeDialogue->Dialogue && NativeDialogueDefinition)
	{
		NativeDialogue->Dialogue = NativeDialogueDefinition;
	}
	NativeDialogueDefinition = NativeDialogue->Dialogue;
	for (UQuestDefinition* Quest : NativeQuestDefinitions) if (Quest) NativeDialogue->OfferedQuests.AddUnique(Quest);
	// Preserve old Blueprint data, but the Talker Component is now the runtime source.
	if (NativeQuestGiver) NativeQuestGiver->AvailableQuests = NativeQuestDefinitions;
	Super::BeginPlay();
}

void AQuestDialogueCharacter::HandlePuzzleSolved(AActor* Puzzle, APawn* SolvedBy)
{
	if (HasAuthority()) URobberPuzzleComponent::OnExhibitPuzzleSolved.Broadcast(this, Puzzle, SolvedBy);
}
