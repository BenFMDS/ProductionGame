#include "Heist/RobberObjectiveMarkers.h"

#include "EngineUtils.h"
#include "Heist/RobberStage.h"
#include "Heist/RobberTriggerZones.h"
#include "Heist/RobberVault.h"
#include "Robber/RobberGameState.h"

namespace
{
	/** The generator and the gem only exist as Blueprints, and aren't needed anywhere else in C++. */
	const AActor* FindByClassName(const UWorld* World, const TCHAR* Prefix)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName().StartsWith(Prefix))
			{
				return *It;
			}
		}
		return nullptr;
	}

	const ARobberVaultDoor* FindShutVault(const UWorld* World)
	{
		for (TActorIterator<ARobberVaultDoor> It(World); It; ++It)
		{
			if (!It->IsOpen())
			{
				return *It;
			}
		}
		return nullptr;
	}

	bool IsJobOpen(const ARobberGameState* State, FName Id)
	{
		return State->GetJobs().ContainsByPredicate([Id](const FRobberJob& Job) { return Job.Id == Id && !Job.bDone; });
	}
}

void RobberMarkers::Gather(const ARobberGameState* State, const APlayerState* Viewer, TArray<FRobberMarkerTarget>& Out)
{
	const UWorld* World = State ? State->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	const ARobberVaultDoor* Vault = FindShutVault(World);
	switch (State->GetGameStage())
	{
	case RobberStage::BreakIn:
		if (const ARobberEntryPoint* Entry = State->GetActiveEntry())
		{
			Entry->GetBreakInMarkers(State, Viewer, Out);
		}
		break;
	case RobberStage::Investigate:
		for (TActorIterator<ARobberGemRoomApproach> It(World); It; ++It)
		{
			Out.Add({ It->GetActorLocation(), Vault ? NSLOCTEXT("RobberMarker", "Vault", "Vault door") : NSLOCTEXT("RobberMarker", "GemDoor", "Gem room door") });
			break;
		}
		break;
	case RobberStage::Heist:
	{
		if (Vault && IsJobOpen(State, ARobberVaultDoor::JobId))
		{
			Out.Add({ Vault->GetActorLocation(), NSLOCTEXT("RobberMarker", "Vault", "Vault door") });
		}
		if (IsJobOpen(State, TEXT("Power")))
		{
			if (const AActor* Generator = FindByClassName(World, TEXT("BP_PowerGenerator")))
			{
				Out.Add({ Generator->GetActorLocation(), NSLOCTEXT("RobberMarker", "Generator", "Generator") });
			}
		}
		// Behind the vault door: pointing at it any sooner would just lead them to a wall.
		if (!Vault && IsJobOpen(State, TEXT("Gem")))
		{
			if (const AActor* Gem = FindByClassName(World, TEXT("BP_Gem")))
			{
				Out.Add({ Gem->GetActorLocation(), NSLOCTEXT("RobberMarker", "Gem", "The gem") });
			}
		}
		break;
	}
	case RobberStage::Escape:
		for (TActorIterator<ARobberEscapeExit> It(World); It; ++It)
		{
			if (It->IsActive(State))
			{
				Out.Add({ It->GetActorLocation(), NSLOCTEXT("RobberMarker", "WayOut", "Way out") });
				break;
			}
		}
		break;
	default:
		break;
	}
}
