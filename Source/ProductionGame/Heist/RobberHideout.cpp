#include "Heist/RobberHideout.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Heist/RobberGlassCase.h"
#include "Heist/RobberHighlight.h"
#include "Heist/RobberMarker.h"
#include "Heist/RobberStage.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"

ARobberLobbyObject::ARobberLobbyObject()
{
	Prompt = NSLOCTEXT("RobberHideout", "Prompt", "Interact With Object");
	Highlight->Mode = ERobberHighlightMode::InRange;
	Highlight->Range = HighlightRange;
	// Like its BP_MenuObject twin, whose outline is on Mesh alone.
	Highlight->PartNames = { TEXT("Mesh") };
}

bool ARobberLobbyObject::IsHovered() const
{
	return Highlight->IsHighlighted();
}

FText ARobberLobbyObject::GetMenuName() const
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

void URobberMenuObjectSubsystem::AddMarker(AActor* Object, const FText& Name)
{
	// The "!" is only drawn for a local player; a dedicated server has none.
	if (!Object || !Object->GetRootComponent() || Object->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	URobberMarkerComponent* Marker = NewObject<URobberMarkerComponent>(Object, TEXT("MenuMarker"));
	Marker->Label = Name;
	Marker->NameRange = ARobberLobbyObject::HighlightRange;
	Marker->SetupAttachment(Object->GetRootComponent());
	Marker->RegisterComponent();
	Markers.Add(Marker);
}

void URobberMenuObjectSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	for (TActorIterator<ARobberLobbyObject> It(&World); It; ++It)
	{
		AddMarker(*It, It->GetMenuName());
	}
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		if (!It->GetClass()->GetName().StartsWith(TEXT("BP_MenuObject")))
		{
			continue;
		}
		Objects.Add(*It);
		AddHighlight(*It);
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
		AddMarker(*It, FText::FromString(Prompt));
	}
}

void URobberMenuObjectSubsystem::AddHighlight(AActor* Object)
{
	// BPC_Interaction lights BP_MenuObject's Mesh only while aimed at; this keeps it lit anywhere in range.
	URobberHighlightComponent* Highlight = NewObject<URobberHighlightComponent>(Object, TEXT("RobberHighlight"));
	Highlight->Mode = ERobberHighlightMode::InRange;
	Highlight->Range = ARobberLobbyObject::HighlightRange;
	Highlight->PartNames = { TEXT("Mesh") };
	Highlight->RegisterComponent();
}

void ARobberLobbyObject::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (ARobberController* Controller = Pawn ? Cast<ARobberController>(Pawn->GetController()) : nullptr)
	{
		Controller->ClientOpenLobbyMenu(Menu);
	}
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
		Gem->SetStaticMesh(Sphere.Object);
		Gem->SetMaterial(0, ShapeMaterial.Object);
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
	// The same gem each night's glass case holds, so the trophies match what was stolen.
	for (int32 Index = 0; Index < Gems.Num(); ++Index)
	{
		if (const FRobberNightGem* NightGem = ARobberGlassCase::GetNightGem(nullptr, Index + 1))
		{
			ARobberGlassCase::DressGem(Gems[Index], *NightGem, Gems[Index]->GetRelativeScale3D().X * 100.f);
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
