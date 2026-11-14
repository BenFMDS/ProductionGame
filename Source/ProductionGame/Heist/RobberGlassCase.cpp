#include "Heist/RobberGlassCase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"

namespace RobberGlassCase
{
	constexpr int32 FrameParts = 9;
	constexpr int32 ShardCount = 8;
	constexpr double Post = 1.5;
}

ARobberGlassCase::ARobberGlassCase()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	Prompt = NSLOCTEXT("RobberGem", "SmashPrompt", "Smash the glass");

	// The base sits on the plinth, so the glass (Mesh) can grow up from it without moving the actor.
	Base = CreateDefaultSubobject<USceneComponent>(TEXT("Base"));
	RootComponent = Base;
	Mesh->SetupAttachment(Base);
	LabelText->SetupAttachment(Base);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glass(TEXT("/Game/OurStuff/Assets/Materials/M_Glass.M_Glass"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Brass(TEXT("/Game/OurStuff/Maps/DevLevels/MuseumGreybox/MI_Greybox_Brass.MI_Greybox_Brass"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetMaterial(0, Glass.Object);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	for (int32 Index = 0; Index < RobberGlassCase::FrameParts; ++Index)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Frame%d"), Index + 1));
		Part->SetupAttachment(Base);
		Part->SetStaticMesh(Cube.Object);
		Part->SetMaterial(0, Brass.Object);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Frame.Add(Part);
	}
	for (int32 Index = 0; Index < RobberGlassCase::ShardCount; ++Index)
	{
		UStaticMeshComponent* Shard = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Shard%d"), Index + 1));
		Shard->SetupAttachment(Base);
		Shard->SetStaticMesh(Cube.Object);
		Shard->SetMaterial(0, Glass.Object);
		Shard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shard->SetHiddenInGame(true);
		Shard->SetVisibility(false);
		Shards.Add(Shard);
	}

	// Ruby, topaz, diamond, then the repeats in new colours (emerald, aquamarine, sapphire), amethyst last.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Ruby(TEXT("/Game/OurStuff/Assets/Meshes/Objects/Rubytrue.Rubytrue"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Topaz(TEXT("/Game/OurStuff/Assets/Meshes/Objects/Topaz.Topaz"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Diamond(TEXT("/Game/OurStuff/Assets/Meshes/Objects/diamond.diamond"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Amethyst(TEXT("/Game/OurStuff/Assets/Meshes/Objects/Amethyst.Amethyst"));
	NightGems = {
		{ Ruby.Object, FLinearColor(0.80f, 0.04f, 0.04f) },
		{ Topaz.Object, FLinearColor(0.90f, 0.35f, 0.02f) },
		{ Diamond.Object, FLinearColor(0.90f, 0.75f, 0.05f) },
		{ Ruby.Object, FLinearColor(0.05f, 0.65f, 0.12f) },
		{ Diamond.Object, FLinearColor(0.04f, 0.65f, 0.75f) },
		{ Topaz.Object, FLinearColor(0.05f, 0.18f, 0.85f) },
		{ Amethyst.Object, FLinearColor(0.50f, 0.08f, 0.75f) },
	};
}

void ARobberGlassCase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const FVector S = CaseSize.ComponentMax(FVector(5.));
	Mesh->SetRelativeLocation(FVector(0., 0., S.Z));
	Mesh->SetRelativeScale3D(S / 50.);
	LabelText->SetRelativeLocation(FVector(0., 0., S.Z * 2. + 15.));

	// Posts at the four corners, rails round the top, a plate under it all (cube is 100 across).
	using RobberGlassCase::Post;
	int32 Next = 0;
	auto Place = [this, &Next](const FVector& Centre, const FVector& HalfSize)
	{
		Frame[Next]->SetRelativeLocation(Centre);
		Frame[Next++]->SetRelativeScale3D(HalfSize / 50.);
	};
	for (const FVector2D Corner : { FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(-1, -1) })
	{
		Place(FVector(Corner.X * S.X, Corner.Y * S.Y, S.Z), FVector(Post, Post, S.Z));
	}
	Place(FVector(S.X, 0., S.Z * 2.), FVector(Post, S.Y, Post));
	Place(FVector(-S.X, 0., S.Z * 2.), FVector(Post, S.Y, Post));
	Place(FVector(0., S.Y, S.Z * 2.), FVector(S.X, Post, Post));
	Place(FVector(0., -S.Y, S.Z * 2.), FVector(S.X, Post, Post));
	Place(FVector(0., 0., 1.), FVector(S.X + Post * 2., S.Y + Post * 2., 1.));

	// Scattered round the foot of the case, as if it burst outwards.
	for (int32 Index = 0; Index < Shards.Num(); ++Index)
	{
		const double Angle = Index * (2. * PI / Shards.Num()) + 0.4 * (Index % 3);
		const double Reach = 1.05 + 0.12 * ((Index * 7) % 4);
		Shards[Index]->SetRelativeLocation(FVector(FMath::Cos(Angle) * S.X * Reach, FMath::Sin(Angle) * S.Y * Reach, 2.5));
		Shards[Index]->SetRelativeRotation(FRotator(0., Index * 47., 0.));
		Shards[Index]->SetRelativeScale3D(FVector(0.06 + 0.02 * (Index % 3), 0.04 + 0.015 * ((Index + 1) % 3), 0.005));
	}
	ShowSmashed();
}

void ARobberGlassCase::BeginPlay()
{
	Super::BeginPlay();
	const int32 Number = GetNightNumber();
	UStaticMeshComponent* GemMesh = nullptr;
	if (AActor* Inside = FindGem())
	{
		// BP_InteractMaster's mesh, the one the interact outline lights up.
		for (UActorComponent* Component : Inside->GetComponents())
		{
			if (Component->GetFName() == TEXT("Mesh"))
			{
				GemMesh = Cast<UStaticMeshComponent>(Component);
			}
		}
	}
	const FRobberNightGem* NightGem = GetNightGem(this, Number);
	if (GemMesh && NightGem)
	{
		DressGem(GemMesh, *NightGem, GemWidth);
	}
	const UBodySetup* Body = NightGem && NightGem->Mesh ? NightGem->Mesh->GetBodySetup() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("%s: night %d gem %s on %s (%d collision shapes)"), *GetName(), Number,
		NightGem && NightGem->Mesh ? *NightGem->Mesh->GetName() : TEXT("-"), GemMesh ? *GemMesh->GetOwner()->GetName() : TEXT("no gem"),
		Body ? Body->AggGeom.GetElementCount() : -1);
}

AActor* ARobberGlassCase::FindGem() const
{
	if (Gem)
	{
		return Gem;
	}
	AActor* Best = nullptr;
	double BestDistance = (CaseSize.Size() + 50.) * (CaseSize.Size() + 50.);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const double Distance = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
		if (It->GetClass()->GetName().StartsWith(TEXT("BP_Gem")) && Distance < BestDistance)
		{
			Best = *It;
			BestDistance = Distance;
		}
	}
	return Best;
}

int32 ARobberGlassCase::GetNightNumber() const
{
	if (Night > 0)
	{
		return Night;
	}
	const ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	return GameState && GameState->GetCurrentNight() >= 0 ? GameState->GetCurrentNight() + 1 : 1;
}

const FRobberNightGem* ARobberGlassCase::GetNightGem(const ARobberGlassCase* Case, int32 NightNumber)
{
	if (!Case)
	{
		// The Blueprint's defaults are the ones people edit; the C++ ones only if it's missing.
		const UClass* Class = LoadClass<ARobberGlassCase>(nullptr, TEXT("/Game/OurStuff/Blueprints/FunctionalScene/BP_GlassCase.BP_GlassCase_C"));
		Case = Class ? Class->GetDefaultObject<ARobberGlassCase>() : GetDefault<ARobberGlassCase>();
	}
	const TArray<FRobberNightGem>& Table = Case->NightGems;
	return Table.Num() ? &Table[(FMath::Max(NightNumber, 1) - 1) % Table.Num()] : nullptr;
}

void ARobberGlassCase::DressGem(UStaticMeshComponent* Target, const FRobberNightGem& NightGem, float Width)
{
	if (!Target || !NightGem.Mesh)
	{
		return;
	}
	Target->SetMobility(EComponentMobility::Movable);
	Target->SetStaticMesh(NightGem.Mesh);
	const FBoxSphereBounds Bounds = NightGem.Mesh->GetBounds();
	const double Across = FMath::Max(FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y), Bounds.BoxExtent.Z) * 2.;
	const double Scale = Across > UE_KINDA_SMALL_NUMBER ? Width / Across : 1.;
	Target->SetRelativeScale3D(FVector(Scale));
	// Centred on where the gem was, unless the mesh is the actor's root (moving that would move the actor).
	if (Target->GetAttachParent())
	{
		Target->SetRelativeLocation(Target->GetRelativeLocation() - Bounds.Origin * Scale);
	}
	// BasicShapeMaterial has the "Color" parameter; the gem meshes come with no material of their own.
	if (UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Shape, Target);
		Material->SetVectorParameterValue(TEXT("Color"), NightGem.Colour);
		for (int32 Slot = 0; Slot < FMath::Max(Target->GetNumMaterials(), 1); ++Slot)
		{
			Target->SetMaterial(Slot, Material);
		}
	}
}

void ARobberGlassCase::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (bSmashed)
	{
		return;
	}
	bSmashed = true;
	ShowSmashed();
	ForceNetUpdate();
	GameState->TellCrew(FText::Format(NSLOCTEXT("RobberGem", "Smashed", "{0} smashed the glass. Grab the gem!"), FText::FromString(Player->GetPlayerName())));
}

FText ARobberGlassCase::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	return bSmashed ? FText::GetEmpty() : Prompt;
}

void ARobberGlassCase::OnRep_Smashed()
{
	ShowSmashed();
}

void ARobberGlassCase::ShowSmashed()
{
	ShowPart(Mesh, !bSmashed);
	for (UStaticMeshComponent* Shard : Shards)
	{
		Shard->SetVisibility(bSmashed);
		Shard->SetHiddenInGame(!bSmashed);
	}
}

void ARobberGlassCase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberGlassCase, bSmashed);
}
