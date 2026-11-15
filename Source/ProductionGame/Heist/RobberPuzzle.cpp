#include "Heist/RobberPuzzle.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberJournal.h"
#include "Net/UnrealNetwork.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogRobberPuzzle, Log, All);

URobberPuzzleComponent::FOnExhibitPuzzleSolved URobberPuzzleComponent::OnExhibitPuzzleSolved;
URobberPuzzleComponent::FOnTargetCalled URobberPuzzleComponent::OnTargetCalled;
const FName URobberPuzzleComponent::OnPuzzleSolvedName(TEXT("On Puzzle Solved"));

namespace RobberPuzzle
{
	const TCHAR* SolverInterfacePath = TEXT("/Game/OurStuff/Core/InteractionSystem/BPI_PuzzleSolver.BPI_PuzzleSolver_C");
	/** The code lock screen's event the server's verdict goes to. */
	const FName CodeResultEvent(TEXT("On Code Result"));
	/** Code lock: the server won't take a code from further away than this. */
	constexpr double MaxCodeDistance = 600.;

	// The puzzle Blueprints are the team's, so their variables are read by name. FName compares ignore case.
	UObject* ObjectVar(const UObject* Owner, const TCHAR* Name)
	{
		const FObjectPropertyBase* Property = Owner ? FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Name) : nullptr;
		return Property ? Property->GetObjectPropertyValue_InContainer(Owner) : nullptr;
	}

	bool BoolVar(const UObject* Owner, const TCHAR* Name)
	{
		const FBoolProperty* Property = Owner ? FindFProperty<FBoolProperty>(Owner->GetClass(), Name) : nullptr;
		return Property && Property->GetPropertyValue_InContainer(Owner);
	}

	TArray<AActor*> ActorArrayVar(const UObject* Owner, const TCHAR* Name)
	{
		TArray<AActor*> Result;
		const FArrayProperty* Property = Owner ? FindFProperty<FArrayProperty>(Owner->GetClass(), Name) : nullptr;
		const FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
		if (Inner)
		{
			FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Owner));
			for (int32 Index = 0; Index < Array.Num(); ++Index)
			{
				Result.Add(Cast<AActor>(Inner->GetObjectPropertyValue(Array.GetRawPtr(Index))));
			}
		}
		return Result;
	}

	TArray<int32> IntArrayVar(const UObject* Owner, const TCHAR* Name)
	{
		TArray<int32> Result;
		const FArrayProperty* Property = Owner ? FindFProperty<FArrayProperty>(Owner->GetClass(), Name) : nullptr;
		if (Property && Property->Inner->IsA<FIntProperty>())
		{
			Result = *Property->ContainerPtrToValuePtr<TArray<int32>>(Owner);
		}
		return Result;
	}

	/** Wire_red_ -> "red". The wire meshes are only told apart by name. */
	FString WireColour(const AActor* Wire)
	{
		const UStaticMesh* Mesh = Cast<UStaticMesh>(ObjectVar(Wire, TEXT("Uncut Mesh")));
		FString Name = Mesh ? Mesh->GetName() : FString();
		Name.RemoveFromStart(TEXT("Wire_"), ESearchCase::IgnoreCase);
		Name.ReplaceInline(TEXT("_"), TEXT(""));
		return Name.ToLower();
	}

	FString JoinWords(const TArray<FString>& Words, const TCHAR* Last)
	{
		if (Words.Num() <= 1)
		{
			return Words.Num() == 1 ? Words[0] : FString();
		}
		return FString::Join(TArrayView<const FString>(Words.GetData(), Words.Num() - 1), TEXT(", ")) + Last + Words.Last();
	}
}

// --- Puzzle component ---

URobberPuzzleComponent::URobberPuzzleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	RequiredPlates = { 0, 1 };
}

void URobberPuzzleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URobberPuzzleComponent, bIsSolved);
	DOREPLIFETIME(URobberPuzzleComponent, AnswerLength);
}

UClass* URobberPuzzleComponent::GetSolverInterface()
{
	static TWeakObjectPtr<UClass> Cached;
	if (!Cached.IsValid())
	{
		Cached = LoadClass<UInterface>(nullptr, RobberPuzzle::SolverInterfacePath);
	}
	return Cached.Get();
}

URobberPuzzleComponent* URobberPuzzleComponent::Find(const AActor* Puzzle)
{
	return Puzzle ? Puzzle->FindComponentByClass<URobberPuzzleComponent>() : nullptr;
}

void URobberPuzzleComponent::BeginPlay()
{
	Super::BeginPlay();
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	// Puzzles placed before the component existed point at their door through their own variable. Keep those working
	// without resaving every map that has one.
	if (SolveTargets.IsEmpty())
	{
		for (const TCHAR* Name : { TEXT("Door"), TEXT("SolveObject") })
		{
			if (AActor* Old = Cast<AActor>(RobberPuzzle::ObjectVar(Owner, Name)))
			{
				SolveTargets.Add(Old);
				break;
			}
		}
	}
	if (Answer.IsEmpty())
	{
		// Same for a code lock's old Code number. A night director rolls a fresh code over this.
		if (const FIntProperty* Code = FindFProperty<FIntProperty>(Owner->GetClass(), TEXT("Code")))
		{
			const int32 Value = Code->GetPropertyValue_InContainer(Owner);
			if (Value > 0)
			{
				SetAnswer(FString::FromInt(Value));
			}
		}
	}
	else
	{
		SetAnswer(Answer);
	}
	if (UClass* Interface = GetSolverInterface())
	{
		for (const AActor* Target : SolveTargets)
		{
			if (Target && !Target->GetClass()->ImplementsInterface(Interface))
			{
				UE_LOG(LogRobberPuzzle, Warning, TEXT("%s: Solve Target %s doesn't implement BPI_PuzzleSolver, so solving won't reach it."), *Owner->GetName(), *Target->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogRobberPuzzle, Error, TEXT("Can't load %s; puzzles can't tell anything they're solved."), RobberPuzzle::SolverInterfacePath);
	}
}

void URobberPuzzleComponent::Solve(APawn* SolvedBy)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bIsSolved)
	{
		return;
	}
	bIsSolved = true;
	Owner->ForceNetUpdate();
	OnSolved.Broadcast(SolvedBy);

	UClass* Interface = GetSolverInterface();
	// Copied: a target can destroy itself (Props/BP_Door) or the director can add to the list while we go.
	const TArray<TObjectPtr<AActor>> Targets = SolveTargets;
	for (AActor* Target : Targets)
	{
		if (!IsValid(Target))
		{
			continue;
		}
		UFunction* Function = Interface && Target->GetClass()->ImplementsInterface(Interface) ? Target->FindFunction(OnPuzzleSolvedName) : nullptr;
		if (!Function)
		{
			UE_LOG(LogRobberPuzzle, Warning, TEXT("%s: %s has no On Puzzle Solved (BPI_PuzzleSolver missing)."), *Owner->GetName(), *Target->GetName());
			continue;
		}
		// Same as a Blueprint message node: fill the event's two inputs by name and call it.
		uint8* Parms = static_cast<uint8*>(FMemory_Alloca_Aligned(FMath::Max<int32>(Function->ParmsSize, 1), Function->GetMinAlignment()));
		FMemory::Memzero(Parms, Function->ParmsSize);
		if (const FObjectPropertyBase* PuzzleParm = FindFProperty<FObjectPropertyBase>(Function, TEXT("Puzzle")))
		{
			PuzzleParm->SetObjectPropertyValue_InContainer(Parms, Owner);
		}
		if (const FObjectPropertyBase* ByParm = FindFProperty<FObjectPropertyBase>(Function, TEXT("Solved By")))
		{
			ByParm->SetObjectPropertyValue_InContainer(Parms, SolvedBy);
		}
		Target->ProcessEvent(Function, Parms);
		OnTargetCalled.Broadcast(this, Target);
	}
}

void URobberPuzzleComponent::OnRep_IsSolved()
{
	if (bIsSolved)
	{
		OnSolved.Broadcast(nullptr);
	}
}

bool URobberPuzzleComponent::ArePlatesPressed(const TArray<AActor*>& Plates) const
{
	if (RequiredPlates.IsEmpty())
	{
		return false;
	}
	for (const int32 Index : RequiredPlates)
	{
		if (!Plates.IsValidIndex(Index) || !RobberPuzzle::BoolVar(Plates[Index], TEXT("IsPressed")))
		{
			return false;
		}
	}
	return true;
}

void URobberPuzzleComponent::CheckPlates(const TArray<AActor*>& Plates)
{
	if (GetOwner() && GetOwner()->HasAuthority() && !bIsSolved && ArePlatesPressed(Plates))
	{
		Solve(nullptr);
	}
}

void URobberPuzzleComponent::SetAnswer(const FString& NewAnswer)
{
	Answer = NewAnswer;
	AnswerLength = Answer.Len();
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
}

FText URobberPuzzleComponent::GetHintText() const
{
	if (!Hint.IsEmpty())
	{
		return Hint;
	}
	const AActor* Owner = GetOwner();
	TArray<FString> Words;
	switch (PuzzleType)
	{
	case ERobberPuzzleType::CodeLock:
		for (const TCHAR Digit : Answer)
		{
			Words.Add(FString::Chr(Digit));
		}
		return FText::AsCultureInvariant(FString::Printf(TEXT("the code is %s"), *FString::Join(Words, TEXT(" "))));
	case ERobberPuzzleType::Combination:
		for (const int32 Digit : RobberPuzzle::IntArrayVar(Owner, TEXT("Correct")))
		{
			Words.Add(FString::FromInt(Digit));
		}
		return Words.IsEmpty() ? FText::GetEmpty() : FText::AsCultureInvariant(FString::Printf(TEXT("the numbers are %s"), *FString::Join(Words, TEXT(" "))));
	case ERobberPuzzleType::Wire:
		// BP_WireController wants Wires cut in array order.
		for (const AActor* Wire : RobberPuzzle::ActorArrayVar(Owner, TEXT("Wires")))
		{
			const FString Colour = RobberPuzzle::WireColour(Wire);
			if (!Colour.IsEmpty())
			{
				Words.Add(Colour);
			}
		}
		return Words.IsEmpty() ? FText::GetEmpty() : FText::AsCultureInvariant(FString::Printf(TEXT("cut %s"), *RobberPuzzle::JoinWords(Words, TEXT(", then "))));
	case ERobberPuzzleType::PressurePlate:
		for (const int32 Index : RequiredPlates)
		{
			Words.Add(FString::FromInt(Index + 1));
		}
		return FText::AsCultureInvariant(FString::Printf(TEXT("stand on plates %s at the same time"), *RobberPuzzle::JoinWords(Words, TEXT(" and "))));
	default:
		return FText::GetEmpty();
	}
}

FVector URobberPuzzleComponent::GetPuzzleLocation() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}
	// Controllers can sit anywhere; the crew stands at the wires, buttons or plates.
	for (const TCHAR* Name : { TEXT("Wires"), TEXT("Buttons"), TEXT("Plates") })
	{
		for (const AActor* Part : RobberPuzzle::ActorArrayVar(Owner, Name))
		{
			if (Part)
			{
				return Part->GetActorLocation();
			}
		}
	}
	return Owner->GetActorLocation();
}

int32 URobberPuzzleComponent::GetCodeLength(AActor* Lock)
{
	const URobberPuzzleComponent* Puzzle = Find(Lock);
	return Puzzle && Puzzle->AnswerLength > 0 ? Puzzle->AnswerLength : 4;
}

bool URobberPuzzleComponent::ShouldOpenCodeScreen(AActor* Lock, AActor* InteractingPlayer)
{
	// Interact runs on the player's machine and again on the server. Only the player's own machine has a screen to
	// open, and on a listen host both calls land on the same machine, so the second one finds the screen already up.
	const APawn* Pawn = Cast<APawn>(InteractingPlayer);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return false;
	}
	const URobberPuzzleComponent* Puzzle = Find(Lock);
	if (Puzzle && Puzzle->IsSolved())
	{
		return false;
	}
	const UUserWidget* Open = Cast<UUserWidget>(RobberPuzzle::ObjectVar(Lock, TEXT("CodeLockUIRef")));
	return !(Open && Open->IsInViewport());
}

void URobberPuzzleComponent::SubmitCode(AActor* Lock, int32 TypedNumber, int32 Digits)
{
	UWorld* World = Lock ? Lock->GetWorld() : nullptr;
	ARobberController* Controller = World ? Cast<ARobberController>(World->GetFirstPlayerController()) : nullptr;
	if (!Controller)
	{
		UE_LOG(LogRobberPuzzle, Warning, TEXT("SubmitCode: no robber controller to send the code through."));
		return;
	}
	// The screen builds the code as a number, which drops leading zeros; the digit count puts them back.
	Controller->ServerSubmitPuzzleCode(Lock, FString::Printf(TEXT("%0*d"), FMath::Clamp(Digits, 1, 9), TypedNumber));
}

void ARobberController::ServerSubmitPuzzleCode_Implementation(AActor* Lock, const FString& Typed)
{
	URobberPuzzleComponent* Puzzle = URobberPuzzleComponent::Find(Lock);
	APawn* Robber = GetPawn();
	if (!Puzzle || !Robber)
	{
		return;
	}
	const bool bNear = FVector::Dist(Robber->GetActorLocation(), Lock->GetActorLocation()) <= RobberPuzzle::MaxCodeDistance;
	const bool bCorrect = bNear && Puzzle->CheckAnswer(Typed);
	if (bCorrect)
	{
		Puzzle->Solve(Robber);
	}
	ClientPuzzleCodeResult(Lock, bCorrect);
}

void ARobberController::ClientPuzzleCodeResult_Implementation(AActor* Lock, bool bCorrect)
{
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets)
	{
		UFunction* Event = Widget && RobberPuzzle::ObjectVar(Widget, TEXT("CodeLockRef")) == Lock ? Widget->FindFunction(RobberPuzzle::CodeResultEvent) : nullptr;
		if (!Event)
		{
			continue;
		}
		uint8* Parms = static_cast<uint8*>(FMemory_Alloca_Aligned(FMath::Max<int32>(Event->ParmsSize, 1), Event->GetMinAlignment()));
		FMemory::Memzero(Parms, Event->ParmsSize);
		for (TFieldIterator<FBoolProperty> It(Event); It; ++It)
		{
			It->SetPropertyValue_InContainer(Parms, bCorrect);
			break;
		}
		Widget->ProcessEvent(Event, Parms);
	}
}

// --- Puzzle chest ---

ARobberPuzzleChest::ARobberPuzzleChest()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Label = NSLOCTEXT("RobberPuzzle", "ChestLabel", "CHEST");
	Prompt = NSLOCTEXT("RobberPuzzle", "ChestLocked", "Locked. A puzzle somewhere opens it.");
	Mesh->SetRelativeScale3D(FVector(0.9, 0.6, 0.5));

	Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));
	Lid->SetupAttachment(Mesh);
	// Shape is in BP_PuzzleChest. In the body's scaled space: a thin slab on top.
	Lid->SetRelativeLocation(FVector(0., 0., 58.));
	Lid->SetRelativeScale3D(FVector(1.04, 1.04, 0.16));
}

void ARobberPuzzleChest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberPuzzleChest, bOpen);
	DOREPLIFETIME(ARobberPuzzleChest, bHasNote);
}

void ARobberPuzzleChest::SetContents(const FRobberReward& InReward, const FText& InNote, const FText& InCrewMessage, const TArray<int32>& InDigitIndexes, const TArray<int32>& InDigitValues, int32 InCodeLength)
{
	Contents = InReward;
	Note = InNote;
	CrewMessage = InCrewMessage;
	DigitIndexes = InDigitIndexes;
	DigitValues = InDigitValues;
	CodeLength = InCodeLength;
	bHasNote = HasNote();
}

bool ARobberPuzzleChest::HasNote() const
{
	switch (Contents.Kind)
	{
	case ERobberRewardKind::VaultDigits: return !DigitIndexes.IsEmpty();
	case ERobberRewardKind::TipOff:
	case ERobberRewardKind::PuzzleHint: return !Note.IsEmpty();
	default: return false;
	}
}

void ARobberPuzzleChest::OpenChest(APawn* SolvedBy)
{
	if (!HasAuthority() || bOpen)
	{
		return;
	}
	bOpen = true;
	OnRep_Open();
	ForceNetUpdate();

	ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	switch (Contents.Kind)
	{
	case ERobberRewardKind::Key:
	case ERobberRewardKind::Tool:
		if (KeyPickupClass)
		{
			// A pickup, not straight into a pocket: whoever grabs it has it, as with keys lying about the museum.
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.OverrideLevel = GetLevel();
			Params.bDeferConstruction = true;
			const FTransform At(GetActorRotation(), GetActorLocation() + FVector(0., 0., 55.), FVector(0.15));
			if (ARobberKeyPickup* Key = GetWorld()->SpawnActor<ARobberKeyPickup>(KeyPickupClass, At, Params))
			{
				Key->KeyId = Contents.KeyId;
				Key->KeyName = Contents.KeyName;
				Key->FinishSpawning(At);
				SpawnedKey = Key;
			}
		}
		else
		{
			UE_LOG(LogRobberPuzzle, Error, TEXT("%s has no Key Pickup Class, so its %s is lost."), *GetName(), *Contents.KeyId.ToString());
		}
		break;
	case ERobberRewardKind::GuardDistraction:
	case ERobberRewardKind::ExtraTime:
		if (GameState)
		{
			Contents.Kind == ERobberRewardKind::GuardDistraction ? GameState->CoolHeat(Contents.Value) : GameState->AddGuardsTime(Contents.Value);
			if (!CrewMessage.IsEmpty())
			{
				GameState->TellCrew(CrewMessage);
			}
		}
		break;
	default:
		break;
	}
}

void ARobberPuzzleChest::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (!bOpen || !HasNote() || !Pawn)
	{
		return;
	}
	URobberJournalComponent* Journal = URobberJournalComponent::Of(Player);
	if (!Journal)
	{
		return;
	}
	const FText Speaker = NSLOCTEXT("RobberPuzzle", "ChestSpeaker", "Note in a chest");
	if (Contents.Kind == ERobberRewardKind::VaultDigits)
	{
		for (int32 Index = 0; Index < DigitIndexes.Num() && DigitValues.IsValidIndex(Index); ++Index)
		{
			Journal->LearnVaultDigit(this, Speaker, DigitIndexes[Index], DigitValues[Index], CodeLength);
		}
	}
	else
	{
		Journal->LearnFact(this, Speaker, Note);
	}
	ARobberController::Tell(Pawn, NSLOCTEXT("RobberPuzzle", "NoteRead", "There's a note in the chest. It's in your journal."));
}

FText ARobberPuzzleChest::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (!bOpen)
	{
		return Prompt;
	}
	return bHasNote ? NSLOCTEXT("RobberPuzzle", "ReadNote", "Read the note") : FText::GetEmpty();
}

void ARobberPuzzleChest::OnRep_Open()
{
	if (bOpen)
	{
		// Flipped up and back off the hinge edge.
		Lid->SetRelativeLocation(FVector(-55., 0., 110.));
		Lid->SetRelativeRotation(FRotator(80., 0., 0.));
	}
}
