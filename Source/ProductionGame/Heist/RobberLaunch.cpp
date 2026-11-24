#include "Heist/RobberLaunch.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberInteract.h"
#include "Heist/RobberLaunchEnding.h"
#include "Heist/RobberStage.h"
#include "Net/UnrealNetwork.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogRobberLaunch, Log, All);

namespace RobberLaunchLook
{
	// NadaSet's screen colours, so the board matches the readouts round it.
	const FColor Go(0x5c, 0xff, 0x7a);
	const FColor NoGo(0xff, 0x40, 0x30);
	const FColor Heading(0xf0, 0xf4, 0xff);

	// Panel size (cm): fits the main screen between its countdown and caption.
	constexpr double Width = 840.;
	constexpr double Height = 230.;
	constexpr double RowStep = 30.;
	constexpr float RowSize = 22.f;
}

ARobberLaunchTask::ARobberLaunchTask()
{
	bReplicates = true;
	InteractComponent->bSilent = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Lamp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(Mesh);
	Lamp->SetStaticMesh(Cylinder.Object);
	// Absolute scale so the lamp stays 10 cm whatever size the task's box is; it sits on the box's top face.
	Lamp->SetAbsolute(false, false, true);
	Lamp->SetRelativeScale3D(FVector(.1, .1, .03));
	Lamp->SetRelativeLocation(FVector(0., 0., 51.));
	Lamp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lamp->SetCanEverAffectNavigation(false);
}

void ARobberLaunchTask::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberLaunchTask, bDone);
}

void ARobberLaunchTask::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Done();
}

TArray<ARobberLaunchTask*> ARobberLaunchTask::All(const UWorld* World)
{
	TArray<ARobberLaunchTask*> Tasks;
	if (World)
	{
		for (TActorIterator<ARobberLaunchTask> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Tasks.Add(*It);
			}
		}
	}
	Tasks.Sort([](const ARobberLaunchTask& A, const ARobberLaunchTask& B) { return A.Order < B.Order; });
	return Tasks;
}

bool ARobberLaunchTask::AllDone(const UWorld* World)
{
	const TArray<ARobberLaunchTask*> Tasks = All(World);
	return Tasks.Num() > 0 && !Tasks.ContainsByPredicate([](const ARobberLaunchTask* Task) { return !Task->IsDone(); });
}

void ARobberLaunchTask::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (bDone || RobberStage::IsInLobby(GameState->GetGameStage()))
	{
		return;
	}
	MarkDone();
	UE_LOG(LogRobberLaunch, Display, TEXT("%s done by %s"), *TaskName.ToString(), *Player->GetPlayerName());
}

void ARobberLaunchTask::MarkDone()
{
	if (HasAuthority() && !bDone)
	{
		bDone = true;
		OnRep_Done();
	}
}

void ARobberLaunchTask::OnRep_Done()
{
	if (UMaterialInterface* Material = bDone ? OnMaterial : OffMaterial)
	{
		Lamp->SetMaterial(0, Material);
	}
}

ARobberLaunchKeys::ARobberLaunchKeys()
{
	bReplicates = true;
	// Silent until it's ready: with no prompt, aiming at it must look the same as aiming at a task.
	InteractComponent->bSilent = true;
	Prompt = NSLOCTEXT("RobberLaunch", "Launch", "Launch");
	LaunchMusic = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/OurStuff/Assets/Audio/Music/Richard_Strauss_-_Also_Sprach_Zarathustra_Cue.Richard_Strauss_-_Also_Sprach_Zarathustra_Cue")));
}

bool ARobberLaunchKeys::bPlayEndingInTests = true;

void ARobberLaunchKeys::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberLaunchKeys, bLaunched);
}

bool ARobberLaunchKeys::IsReady() const
{
	return !bLaunched && ARobberLaunchTask::AllDone(GetWorld());
}

FText ARobberLaunchKeys::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	return IsReady() && !RobberStage::IsInLobby(GameState->GetGameStage()) ? Prompt : FText::GetEmpty();
}

void ARobberLaunchKeys::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (!IsReady() || RobberStage::IsInLobby(GameState->GetGameStage()))
	{
		return;
	}
	bLaunched = true;
	UE_LOG(LogRobberLaunch, Display, TEXT("Launch keys turned by %s"), *Player->GetPlayerName());
	if (!GIsAutomationTesting || bPlayEndingInTests)
	{
		GameState->StartLaunch(URobberLaunchEnding::RunEndsAt);
	}
	MulticastLaunch(Player);
}

void ARobberLaunchKeys::MulticastLaunch_Implementation(APlayerState* Launcher)
{
	if (!GIsAutomationTesting || bPlayEndingInTests)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ARobberController* Robber = Cast<ARobberController>(It->Get()); Robber && Robber->IsLocalController())
			{
				URobberLaunchEnding::Play(Robber, LaunchMusic.LoadSynchronous());
			}
		}
	}
	OnLaunch.Broadcast(Launcher);
}

ARobberLaunchBoard::ARobberLaunchBoard()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = .25f;
	KeysName = NSLOCTEXT("RobberLaunch", "KeysRow", "LAUNCH KEYS");

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
	Panel->SetupAttachment(RootComponent);
	Panel->SetStaticMesh(Cube.Object);
	// Front face on the actor's origin, so the text at X = 1 sits just in front of it.
	Panel->SetRelativeLocation(FVector(-1., 0., 0.));
	Panel->SetRelativeScale3D(FVector(.02, RobberLaunchLook::Width / 100., RobberLaunchLook::Height / 100.));
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Panel->SetCanEverAffectNavigation(false);

	Title = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Title"));
	Title->SetupAttachment(RootComponent);
	Title->SetText(NSLOCTEXT("RobberLaunch", "Title", "LAUNCH CHECKLIST"));
	Title->SetRelativeLocation(FVector(1., 0., RobberLaunchLook::Height / 2. - 20.));
	Title->SetWorldSize(28.f);
	Title->SetTextRenderColor(RobberLaunchLook::Heading);
	Title->SetHorizontalAlignment(EHTA_Center);
	Title->SetVerticalAlignment(EVRTA_TextCenter);
	Title->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARobberLaunchBoard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Refresh();
}

void ARobberLaunchBoard::MakeRows(int32 Count)
{
	for (UTextRenderComponent* Each : Names) Each->DestroyComponent();
	for (UTextRenderComponent* Each : Statuses) Each->DestroyComponent();
	Names.Reset();
	Statuses.Reset();
	const auto Make = [this](int32 Row, EHorizTextAligment Align, double Y)
	{
		UTextRenderComponent* Text = NewObject<UTextRenderComponent>(this);
		Text->SetupAttachment(RootComponent);
		Text->SetRelativeLocation(FVector(1., Y, RobberLaunchLook::Height / 2. - 55. - Row * RobberLaunchLook::RowStep));
		Text->SetWorldSize(RobberLaunchLook::RowSize);
		Text->SetHorizontalAlignment(Align);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Text->RegisterComponent();
		return Text;
	};
	// The viewer's left is +Y: names start there, statuses end at the right edge.
	const double Edge = RobberLaunchLook::Width / 2. - 40.;
	for (int32 Row = 0; Row < Count; ++Row)
	{
		Names.Add(Make(Row, EHTA_Left, Edge));
		Statuses.Add(Make(Row, EHTA_Right, -Edge));
	}
}

void ARobberLaunchBoard::Refresh()
{
	const TArray<ARobberLaunchTask*> Tasks = ARobberLaunchTask::All(GetWorld());
	const ARobberLaunchKeys* Keys = nullptr;
	for (TActorIterator<ARobberLaunchKeys> It(GetWorld()); It && !Keys; ++It)
	{
		Keys = *It;
	}
	const int32 Count = Tasks.Num() + (Keys ? 1 : 0);
	if (Names.Num() != Count)
	{
		MakeRows(Count);
	}
	const auto Show = [this](int32 Row, const FText& Name, bool bGo)
	{
		const FColor Colour = bGo ? RobberLaunchLook::Go : RobberLaunchLook::NoGo;
		Names[Row]->SetText(Name);
		Names[Row]->SetTextRenderColor(Colour);
		Statuses[Row]->SetText(bGo ? NSLOCTEXT("RobberLaunch", "Go", "GO") : NSLOCTEXT("RobberLaunch", "NoGo", "NO-GO"));
		Statuses[Row]->SetTextRenderColor(Colour);
	};
	for (int32 Row = 0; Row < Tasks.Num(); ++Row)
	{
		Show(Row, Tasks[Row]->TaskName, Tasks[Row]->IsDone());
	}
	if (Keys)
	{
		Show(Tasks.Num(), KeysName, Keys->IsLaunched());
	}
}

bool ARobberLaunchBoard::GetRow(int32 Index, FString& OutName, FString& OutStatus, FColor& OutColour) const
{
	if (!Names.IsValidIndex(Index))
	{
		return false;
	}
	OutName = Names[Index]->Text.ToString();
	OutStatus = Statuses[Index]->Text.ToString();
	OutColour = Names[Index]->TextRenderColor;
	return true;
}
