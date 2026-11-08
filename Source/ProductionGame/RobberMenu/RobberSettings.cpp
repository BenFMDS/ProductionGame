#include "RobberMenu/RobberSettings.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UObjectGlobals.h"

namespace RobberSettingsDetail
{
	const TCHAR* SlotName = TEXT("RobberSettings");
	const TCHAR* ContextPath = TEXT("/Game/Input/IMC_Default.IMC_Default");

	template <typename T>
	T* LoadAudio(const TCHAR* Name)
	{
		return LoadObject<T>(nullptr, *FString::Printf(TEXT("/Game/OurStuff/Audio/%s.%s"), Name, Name));
	}
}

const TArray<FRobberKeyAction>& URobberSettingsSubsystem::KeyActions()
{
	static const TArray<FRobberKeyAction> Actions = {
		{ TEXT("MoveForward"), EKeys::W, TEXT("IA_Move") },
		{ TEXT("MoveLeft"), EKeys::A, TEXT("IA_Move") },
		{ TEXT("MoveBack"), EKeys::S, TEXT("IA_Move") },
		{ TEXT("MoveRight"), EKeys::D, TEXT("IA_Move") },
		{ TEXT("Jump"), EKeys::SpaceBar, TEXT("IA_Jump") },
		{ TEXT("Interact"), EKeys::E, TEXT("IA_Interact") },
		{ TEXT("Use"), EKeys::LeftMouseButton, TEXT("IA_Use") },
		{ TEXT("Throw"), EKeys::RightMouseButton, TEXT("IA_Throw") },
		{ TEXT("Drop"), EKeys::G, TEXT("IA_Drop") },
		{ TEXT("Journal"), EKeys::J, nullptr },
		{ TEXT("Pause"), EKeys::P, nullptr },
		// Held, read by ARobberController.
		{ TEXT("Sprint"), EKeys::LeftShift, nullptr },
	};
	return Actions;
}

FText URobberSettingsSubsystem::KeyName(const FKey& Key)
{
	if (Key == EKeys::LeftMouseButton) return INVTEXT("LMB");
	if (Key == EKeys::RightMouseButton) return INVTEXT("RMB");
	if (Key == EKeys::MiddleMouseButton) return INVTEXT("MMB");
	if (Key == EKeys::ThumbMouseButton) return INVTEXT("Mouse 4");
	if (Key == EKeys::ThumbMouseButton2) return INVTEXT("Mouse 5");
	if (Key == EKeys::SpaceBar) return INVTEXT("Space");
	if (Key == EKeys::LeftShift) return INVTEXT("L Shift");
	if (Key == EKeys::RightShift) return INVTEXT("R Shift");
	if (Key == EKeys::LeftControl) return INVTEXT("L Ctrl");
	if (Key == EKeys::RightControl) return INVTEXT("R Ctrl");
	if (Key == EKeys::LeftAlt) return INVTEXT("L Alt");
	if (Key == EKeys::RightAlt) return INVTEXT("R Alt");
	return Key.GetDisplayName(false);
}

void URobberSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadSave();

	DefaultContext = LoadObject<UInputMappingContext>(nullptr, RobberSettingsDetail::ContextPath);
	if (DefaultContext)
	{
		// Matched by action and default key, because IA_Move has four keyboard mappings with different modifiers.
		const TArray<FEnhancedActionKeyMapping>& Mappings = DefaultContext->GetMappings();
		for (const FRobberKeyAction& Action : KeyActions())
		{
			if (!Action.InputAction)
			{
				continue;
			}
			for (int32 Index = 0; Index < Mappings.Num(); ++Index)
			{
				if (Mappings[Index].Action && Mappings[Index].Action->GetName() == Action.InputAction && Mappings[Index].Key == Action.DefaultKey)
				{
					MappingIndex.Add(Action.Id, Index);
					break;
				}
			}
		}
	}

	MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &URobberSettingsSubsystem::OnMapLoaded);
	ApplyKeys();
}

void URobberSettingsSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
	// The context is a shared asset: in the editor it would otherwise keep this player's keys after PIE ends.
	RestoreDefaultKeys();
	Super::Deinitialize();
}

FKey URobberSettingsSubsystem::GetKey(FName ActionId) const
{
	if (const FKey* Saved = Settings ? Settings->Keys.Find(ActionId) : nullptr)
	{
		return *Saved;
	}
	for (const FRobberKeyAction& Action : KeyActions())
	{
		if (Action.Id == ActionId)
		{
			return Action.DefaultKey;
		}
	}
	return EKeys::Invalid;
}

FName URobberSettingsSubsystem::SetKey(FName ActionId, const FKey& Key)
{
	FName Swapped = NAME_None;
	const FKey OldKey = GetKey(ActionId);
	for (const FRobberKeyAction& Action : KeyActions())
	{
		if (Action.Id != ActionId && GetKey(Action.Id) == Key)
		{
			Settings->Keys.Add(Action.Id, OldKey);
			Swapped = Action.Id;
		}
	}
	Settings->Keys.Add(ActionId, Key);
	return Swapped;
}

void URobberSettingsSubsystem::ResetKeys()
{
	Settings->Keys.Reset();
}

void URobberSettingsSubsystem::Save()
{
	UGameplayStatics::SaveGameToSlot(Settings, RobberSettingsDetail::SlotName, 0);
	Apply();
}

void URobberSettingsSubsystem::Apply()
{
	ApplyAudio(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	ApplyKeys();
}

void URobberSettingsSubsystem::LoadSave()
{
	Settings = UGameplayStatics::DoesSaveGameExist(RobberSettingsDetail::SlotName, 0)
		? Cast<URobberSettingsSave>(UGameplayStatics::LoadGameFromSlot(RobberSettingsDetail::SlotName, 0)) : nullptr;
	if (!Settings)
	{
		Settings = NewObject<URobberSettingsSave>(this);
	}
}

void URobberSettingsSubsystem::Revert()
{
	LoadSave();
	Apply();
}

void URobberSettingsSubsystem::OnMapLoaded(UWorld* World)
{
	if (World && World->GetGameInstance() == GetGameInstance())
	{
		ApplyAudio(World);
	}
}

void URobberSettingsSubsystem::ApplyAudio(UWorld* World)
{
	using namespace RobberSettingsDetail;
	USoundMix* Mix = LoadAudio<USoundMix>(TEXT("SM_Main"));
	if (!World || !Mix || World->IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	const auto SetVolume = [&](const TCHAR* ClassName, float Volume)
	{
		if (USoundClass* SoundClass = LoadAudio<USoundClass>(ClassName))
		{
			UGameplayStatics::SetSoundMixClassOverride(World, Mix, SoundClass, Volume, 1.f, 0.f, true);
		}
	};
	// SC_Main first: it applies to its children, and the two below then set their own.
	SetVolume(TEXT("SC_Main"), Settings->MusicVolume);
	SetVolume(TEXT("SC_SFX"), Settings->SfxVolume);
	SetVolume(TEXT("SC_Voices"), Settings->VoiceVolume);
	UGameplayStatics::PushSoundMixModifier(World, Mix);
}

void URobberSettingsSubsystem::ApplyKeys()
{
	if (DefaultContext)
	{
		for (const TPair<FName, int32>& Entry : MappingIndex)
		{
			DefaultContext->GetMapping(Entry.Value).Key = GetKey(Entry.Key);
		}
		if (const UGameInstance* GameInstance = GetGameInstance())
		{
			for (ULocalPlayer* Player : GameInstance->GetLocalPlayers())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
				{
					Input->RequestRebuildControlMappings();
				}
			}
		}
	}
	OnControlsChanged.Broadcast();
}

void URobberSettingsSubsystem::RestoreDefaultKeys()
{
	if (!DefaultContext)
	{
		return;
	}
	for (const FRobberKeyAction& Action : KeyActions())
	{
		if (const int32* Index = MappingIndex.Find(Action.Id))
		{
			DefaultContext->GetMapping(*Index).Key = Action.DefaultKey;
		}
	}
}
