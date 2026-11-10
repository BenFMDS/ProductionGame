#include "Heist/RobberHideout.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Heist/RobberStage.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"

ARobberLobbyObject::ARobberLobbyObject()
{
	// Every frame, so the highlight comes on the moment you step into range.
	PrimaryActorTick.TickInterval = 0.f;
	Prompt = NSLOCTEXT("RobberHideout", "Prompt", "Interact With Object");
}

bool ARobberLobbyObject::IsInHighlightRange(const AActor* Target, const APlayerController* Viewer)
{
	const APlayerCameraManager* Camera = Viewer ? Viewer->PlayerCameraManager.Get() : nullptr;
	if (!Target || !Camera)
	{
		return false;
	}
	const FVector Eye = Camera->GetCameraLocation();
	FVector Centre;
	FVector Extent;
	Target->GetActorBounds(true, Centre, Extent);
	if (FVector::Dist(Eye, Centre) > HighlightRange)
	{
		return false;
	}
	// Otherwise the outline would draw straight through whatever is in front of it.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MenuObjectHighlight), false, Viewer->GetPawn());
	Params.AddIgnoredActor(Target);
	return !Target->GetWorld()->LineTraceTestByChannel(Eye, Centre, ECC_Visibility, Params);
}

void ARobberLobbyObject::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bInRange = IsInHighlightRange(this, GetWorld()->GetFirstPlayerController());
	if (bInRange != bHovered)
	{
		bHovered = bInRange;
		Mesh->SetRenderCustomDepth(bHovered);
	}
}

FText ARobberLobbyObject::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	switch (Menu)
	{
	case ERobberLobbyMenu::Settings: return NSLOCTEXT("RobberHideout", "PromptSettings", "Settings");
	case ERobberLobbyMenu::Achievements: return NSLOCTEXT("RobberHideout", "PromptAchievements", "Achievements");
	case ERobberLobbyMenu::Quit: return NSLOCTEXT("RobberHideout", "PromptQuit", "Quit game");
	case ERobberLobbyMenu::Catalogue: return NSLOCTEXT("RobberHideout", "PromptCatalogue", "Open catalogue");
	default: return Prompt;
	}
}

bool URobberMenuObjectSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void URobberMenuObjectSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		if (!It->GetClass()->GetName().StartsWith(TEXT("BP_MenuObject")))
		{
			continue;
		}
		Objects.Add(*It);
		for (UActorComponent* Component : It->GetComponents())
		{
			if (UTextRenderComponent* Text = Cast<UTextRenderComponent>(Component))
			{
				Text->SetVisibility(false);
			}
		}
		// The Blueprint's own label says which object this is, so that picks the prompt.
		const FTextProperty* DisplayText = FindFProperty<FTextProperty>(It->GetClass(), TEXT("DisplayText"));
		const FStrProperty* InteractText = FindFProperty<FStrProperty>(It->GetClass(), TEXT("Interact Text"));
		if (!DisplayText || !InteractText)
		{
			continue;
		}
		const FString Name = DisplayText->GetPropertyValue_InContainer(*It).ToString();
		FString Prompt = Name;
		if (Name.Contains(TEXT("Play"))) Prompt = NSLOCTEXT("RobberHideout", "MenuPlay", "Play game").ToString();
		else if (Name.Contains(TEXT("Setting"))) Prompt = NSLOCTEXT("RobberHideout", "MenuSettings", "Settings").ToString();
		else if (Name.Contains(TEXT("Achievement"))) Prompt = NSLOCTEXT("RobberHideout", "MenuAchievements", "Achievements").ToString();
		else if (Name.Contains(TEXT("Dev"))) Prompt = NSLOCTEXT("RobberHideout", "MenuDev", "Dev levels").ToString();
		else if (Name.Contains(TEXT("Quit"))) Prompt = NSLOCTEXT("RobberHideout", "MenuQuit", "Quit game").ToString();
		InteractText->SetPropertyValue_InContainer(*It, Prompt);
	}
}

void URobberMenuObjectSubsystem::Tick(float DeltaTime)
{
	const APlayerController* LocalPlayer = GetWorld()->GetFirstPlayerController();
	for (const TWeakObjectPtr<AActor>& Object : Objects)
	{
		if (!Object.IsValid())
		{
			continue;
		}
		const bool bInRange = ARobberLobbyObject::IsInHighlightRange(Object.Get(), LocalPlayer);
		// Every frame: BPC_Interaction switches the outline off when your aim leaves the object, even if you're still in range.
		for (UActorComponent* Component : Object->GetComponents())
		{
			UPrimitiveComponent* Primitive = Component->GetFName() == TEXT("Mesh") ? Cast<UPrimitiveComponent>(Component) : nullptr;
			if (Primitive && Primitive->bRenderCustomDepth != bInRange)
			{
				Primitive->SetRenderCustomDepth(bInRange);
			}
		}
	}
}

void ARobberLobbyObject::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (ARobberController* Controller = Pawn ? Cast<ARobberController>(Pawn->GetController()) : nullptr)
	{
		Controller->ClientOpenLobbyMenu(Menu);
	}
}

namespace RobberGems
{
	// One colour per night, Night 1 first.
	const FLinearColor Colours[ARobberGemCase::GemCount] = {
		FLinearColor(0.80f, 0.04f, 0.04f), FLinearColor(0.90f, 0.35f, 0.02f), FLinearColor(0.90f, 0.75f, 0.05f), FLinearColor(0.05f, 0.65f, 0.12f),
		FLinearColor(0.04f, 0.65f, 0.75f), FLinearColor(0.05f, 0.18f, 0.85f), FLinearColor(0.50f, 0.08f, 0.75f),
	};
}

ARobberGemCase::ARobberGemCase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	CaseRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CaseRoot"));
	RootComponent = CaseRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	// Has the "Color" parameter BeginPlay tints; the sphere's own default material doesn't.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	for (int32 Index = 0; Index < GemCount; ++Index)
	{
		UStaticMeshComponent* Gem = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Gem%d"), Index + 1));
		Gem->SetupAttachment(CaseRoot);
		if (Sphere.Succeeded())
		{
			Gem->SetStaticMesh(Sphere.Object);
		}
		if (ShapeMaterial.Succeeded())
		{
			Gem->SetMaterial(0, ShapeMaterial.Object);
		}
		Gem->SetRelativeScale3D(FVector(0.22));
		Gem->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Gems.Add(Gem);
	}
}

void ARobberGemCase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	for (int32 Index = 0; Index < Gems.Num(); ++Index)
	{
		Gems[Index]->SetRelativeLocation(GemSpots.IsValidIndex(Index) && GemSpots.Num() >= GemCount ? GemSpots[Index]
			: FVector((Index - (GemCount - 1) / 2.) * Spacing, 0., 0.));
	}
}

void ARobberGemCase::BeginPlay()
{
	Super::BeginPlay();
	for (int32 Index = 0; Index < Gems.Num(); ++Index)
	{
		if (UMaterialInstanceDynamic* Material = Gems[Index]->CreateAndSetMaterialInstanceDynamic(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), RobberGems::Colours[Index]);
		}
	}
	ShowGems(GemsWon(GetWorld()->GetGameState<ARobberGameState>()));
}

int32 ARobberGemCase::GemsWon(const ARobberGameState* GameState)
{
	if (!GameState)
	{
		return 0;
	}
	if (GameState->GetGameStage() == RobberStage::Win)
	{
		return GemCount;
	}
	// The loaded night is the one being tried, so every night before it was won. The tutorial reads -1.
	return FMath::Clamp(GameState->GetCurrentNight(), 0, GemCount);
}

void ARobberGemCase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ShowGems(GemsWon(GetWorld()->GetGameState<ARobberGameState>()));
}

void ARobberGemCase::ShowGems(int32 Count)
{
	if (Count == ShownCount)
	{
		return;
	}
	ShownCount = Count;
	for (int32 Index = 0; Index < Gems.Num(); ++Index)
	{
		Gems[Index]->SetVisibility(Index < Count);
	}
}
