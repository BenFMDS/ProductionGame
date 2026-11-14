#include "Heist/RobberInteract.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Robber/RobberGameState.h"

URobberInteractComponent::URobberInteractComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

URobberInteractComponent* URobberInteractComponent::Of(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<URobberInteractComponent>() : nullptr;
}

bool URobberInteractComponent::IsActive(const ARobberGameState* GameState) const
{
	return !OwningEntry || (GameState && GameState->GetActiveEntry() == OwningEntry);
}

FString URobberInteractComponent::GetPrompt() const
{
	const IRobberInteractOwner* Owner = Cast<IRobberInteractOwner>(GetOwner());
	const UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return FString();
	}
	const ARobberGameState* GameState = World->GetGameState<ARobberGameState>();
	// No game state means a menu or editor world, where no way in has been voted for yet.
	if (GameState && !IsActive(GameState))
	{
		return FString();
	}
	const APlayerController* LocalPlayer = World->GetFirstPlayerController();
	return Owner->GetInteractText(GameState, LocalPlayer ? LocalPlayer->PlayerState : nullptr).ToString();
}

void URobberInteractComponent::Press(AActor* InteractingPlayer)
{
	IRobberInteractOwner* Owner = Cast<IRobberInteractOwner>(GetOwner());
	if (!Owner || (bServerOnly && !GetOwner()->HasAuthority()))
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(InteractingPlayer);
	if (const AController* Controller = Cast<AController>(InteractingPlayer))
	{
		Pawn = Controller->GetPawn();
	}
	APlayerState* Player = Pawn ? Pawn->GetPlayerState() : nullptr;
	ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	// Closing time plays with everyone tucked into a spot; climbing out mid-card would leave them unhidden for the night.
	if (!Player || !GameState || !IsActive(GameState) || GameState->IsClosingTime())
	{
		return;
	}
	if (bServerOnly)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (const double* Last = LastPressTime.Find(Player); Last && Now - *Last < 0.2)
		{
			return;
		}
		LastPressTime.Add(Player, Now);
	}
	Owner->OnInteract(Pawn, Player, GameState);
}
