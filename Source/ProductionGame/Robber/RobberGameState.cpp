// Copyright Epic Games, Inc. All Rights Reserved.

#include "Robber/RobberGameState.h"

#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Robber/RobberController.h"
#include "UObject/UnrealType.h"

namespace
{
	// NewEnumerator3 in the E_GameStage Blueprint enum.
	constexpr uint8 EscapeGameStage = 3;
}

ARobberGameState::ARobberGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	AlarmClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/OurStuff/Blueprints/Props/BP_Alarm.BP_Alarm_C")));
}

void ARobberGameState::BeginPlay()
{
	Super::BeginPlay();

	PlayerControllerRef = Cast<ARobberController>(UGameplayStatics::GetPlayerController(this, 0));
	if (HasAuthority())
	{
		SetGameStage(GetLegacyGameState());
	}
	else
	{
		SetLegacyGameState(ReplicatedGameStage);
		// Widgets created before this actor replicated in cached a null GameStateRef.
		BindLocalStageReaders();
	}
}

void ARobberGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Blueprints (e.g. BP_Gem) still write the legacy field; promote those writes to the replicated stage.
	if (HasAuthority() && GetLegacyGameState() != ReplicatedGameStage)
	{
		SetGameStage(GetLegacyGameState());
	}
}

void ARobberGameState::ActivateAlarms()
{
	UClass* LoadedAlarmClass = AlarmClass.LoadSynchronous();
	if (!LoadedAlarmClass || !GetWorld())
	{
		return;
	}

	// BP_Alarm doesn't replicate, so its server/multicast RPCs never leave the server. Every machine
	// runs this on the stage change and calls the multicast body on its own level-placed copies.
	for (TActorIterator<AActor> It(GetWorld(), LoadedAlarmClass); It; ++It)
	{
		if (UFunction* ActivateAlarm = It->FindFunction(TEXT("ActivateAlarmMulti")))
		{
			It->ProcessEvent(ActivateAlarm, nullptr);
		}
	}
}

void ARobberGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberGameState, ReplicatedGameStage);
}

void ARobberGameState::SetGameStage(uint8 NewGameStage)
{
	const bool bStageChanged = ReplicatedGameStage != NewGameStage;
	ReplicatedGameStage = NewGameStage;
	SetLegacyGameState(NewGameStage);
	if (HasAuthority())
	{
		ForceNetUpdate();
	}

	// Clients react in OnRep; this covers the listen-server host.
	if (bStageChanged)
	{
		HandleGameStageChanged();
	}
}

void ARobberGameState::OnRep_GameState()
{
	// The old Blueprint declared this callback but intentionally left it empty.
}

void ARobberGameState::OnRep_ReplicatedGameStage()
{
	SetLegacyGameState(ReplicatedGameStage);
	OnRep_GameState();
	HandleGameStageChanged();
}

void ARobberGameState::HandleGameStageChanged()
{
	BindLocalStageReaders();
	NotifyDevUIOfStageChange();
	if (ReplicatedGameStage == EscapeGameStage)
	{
		ActivateAlarms();
	}
}

void ARobberGameState::BindLocalStageReaders()
{
	if (!PlayerControllerRef)
	{
		PlayerControllerRef = Cast<ARobberController>(UGameplayStatics::GetPlayerController(this, 0));
	}
	if (!PlayerControllerRef)
	{
		return;
	}

	// BP_RobberController (read by WBP_DevUI) and WBP_GameUI each cache GameStateRef once, at BeginPlay/Construct.
	UObject* Readers[] = { PlayerControllerRef, PlayerControllerRef->GetGameUIWidget() };
	for (UObject* Reader : Readers)
	{
		FObjectPropertyBase* RefProperty = Reader ? FindFProperty<FObjectPropertyBase>(Reader->GetClass(), TEXT("GameStateRef")) : nullptr;
		if (RefProperty && IsA(RefProperty->PropertyClass) && !RefProperty->GetObjectPropertyValue_InContainer(Reader))
		{
			RefProperty->SetObjectPropertyValue_InContainer(Reader, this);
		}
	}
}

uint8 ARobberGameState::GetLegacyGameState() const
{
	const FByteProperty* LegacyProperty = FindFProperty<FByteProperty>(GetClass(), TEXT("GameState"));
	return LegacyProperty ? LegacyProperty->GetPropertyValue_InContainer(this) : ReplicatedGameStage;
}

void ARobberGameState::SetLegacyGameState(uint8 NewGameStage)
{
	if (FByteProperty* LegacyProperty = FindFProperty<FByteProperty>(GetClass(), TEXT("GameState")))
	{
		LegacyProperty->SetPropertyValue_InContainer(this, NewGameStage);
	}
}

void ARobberGameState::NotifyDevUIOfStageChange()
{
	if (!PlayerControllerRef)
	{
		PlayerControllerRef = Cast<ARobberController>(UGameplayStatics::GetPlayerController(this, 0));
	}

	UUserWidget* DevUI = PlayerControllerRef ? PlayerControllerRef->GetDevUIWidget() : nullptr;
	if (DevUI)
	{
		// Must match the custom event name in WBP_DevUI exactly, spaces included.
		UFunction* UpdateStageText = DevUI->FindFunction(TEXT("Update StageText Server"));
		if (UpdateStageText && UpdateStageText->ParmsSize == 0)
		{
			DevUI->ProcessEvent(UpdateStageText, nullptr);
		}
	}
}
