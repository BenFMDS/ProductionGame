#include "AchievementBlueprintFunctionLibrary.h"

#include "AchievementDefinitionAsset.h"
#include "AchievementPopupWidget.h"
#include "AchievementSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	UWorld* ResolveWorldContext(const UObject* WorldContextObject)
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}

		if (WorldContextObject != nullptr)
		{
			if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
			{
				return World;
			}
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() == nullptr)
			{
				continue;
			}

			switch (Context.WorldType)
			{
			case EWorldType::Game:
			case EWorldType::PIE:
			case EWorldType::GamePreview:
				return Context.World();
			default:
				break;
			}
		}

		return nullptr;
	}

	UAchievementPopupWidget* CreateAchievementPopup(
		UWorld* World,
		TSubclassOf<UAchievementPopupWidget> PopupWidgetClass,
		UAchievementDefinitionAsset* AchievementAsset,
		const FAchievementProgressState& ProgressState,
		const float AutoHideDelay,
		const int32 ZOrder)
	{
		UAchievementPopupWidget* PopupWidget = UAchievementPopupWidget::CreateAndShowPopup(
			World, PopupWidgetClass, AchievementAsset, ProgressState, AutoHideDelay, ZOrder);
		if (PopupWidget == nullptr)
		{
			return nullptr;
		}

		PopupWidget->SetAlignmentInViewport(FVector2D(1.0f, 1.0f));
		PopupWidget->SetPositionInViewport(FVector2D(-24.0f, -24.0f), false);
		return PopupWidget;
	}
}

UAchievementSubsystem* UAchievementBlueprintFunctionLibrary::GetAchievementSubsystem(const UObject* WorldContextObject)
{
	if (GEngine == nullptr)
	{
		return nullptr;
	}

	if (UWorld* World = ResolveWorldContext(WorldContextObject))
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UAchievementSubsystem>();
		}
	}

	return nullptr;
}

bool UAchievementBlueprintFunctionLibrary::AddAchievementProgress(const UObject* WorldContextObject, const FName AchievementId, const int32 Amount)
{
	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		return Subsystem->AddAchievementProgress(AchievementId, Amount);
	}

	return false;
}

bool UAchievementBlueprintFunctionLibrary::UnlockAchievement(const UObject* WorldContextObject, const FName AchievementId)
{
	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		return Subsystem->UnlockAchievement(AchievementId);
	}

	return false;
}

bool UAchievementBlueprintFunctionLibrary::UpdateAchievementProgress(const UObject* WorldContextObject, const FName AchievementId, const int32 CurrentAmount)
{
	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		return Subsystem->UpdateAchievementProgress(AchievementId, CurrentAmount);
	}

	return false;
}

bool UAchievementBlueprintFunctionLibrary::RemoveAchievementProgress(const UObject* WorldContextObject, const FName AchievementId)
{
	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		return Subsystem->RemoveAchievementProgress(AchievementId);
	}

	return false;
}

bool UAchievementBlueprintFunctionLibrary::ResetAchievementProgress(const UObject* WorldContextObject, const FName AchievementId)
{
	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		return Subsystem->ResetAchievementProgress(AchievementId);
	}

	return false;
}

bool UAchievementBlueprintFunctionLibrary::ResetAllAchievementProgress(const UObject* WorldContextObject)
{
	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		return Subsystem->ResetAllAchievementProgress();
	}

	return false;
}

int32 UAchievementBlueprintFunctionLibrary::GetAchievementCurrentAmount(const UObject* WorldContextObject, const FName AchievementId)
{
	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		return Subsystem->GetAchievementCurrentAmount(AchievementId);
	}

	return 0;
}

int32 UAchievementBlueprintFunctionLibrary::GetAchievementRequiredAmount(const UObject* WorldContextObject, const FName AchievementId)
{
	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		return Subsystem->GetAchievementRequiredAmount(AchievementId);
	}

	return 0;
}

float UAchievementBlueprintFunctionLibrary::GetAchievementProgressPercent(const UObject* WorldContextObject, const FName AchievementId)
{
	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		return Subsystem->GetAchievementProgressPercent(AchievementId);
	}

	return 0.0f;
}

bool UAchievementBlueprintFunctionLibrary::IsAchievementUnlocked(const UObject* WorldContextObject, const FName AchievementId)
{
	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		return Subsystem->IsAchievementUnlocked(AchievementId);
	}

	return false;
}

FAchievementProgressState UAchievementBlueprintFunctionLibrary::GetAchievementProgress(const UObject* WorldContextObject, const FName AchievementId)
{
	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		return Subsystem->GetAchievementProgress(AchievementId);
	}

	return FAchievementProgressState();
}

TArray<FAchievementDefinition> UAchievementBlueprintFunctionLibrary::GetAchievementDefinitions(const UObject* WorldContextObject)
{
	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		return Subsystem->GetAchievementDefinitions();
	}

	return TArray<FAchievementDefinition>();
}

UAchievementPopupWidget* UAchievementBlueprintFunctionLibrary::ShowAchievementPopup(
	const UObject* WorldContextObject,
	TSubclassOf<UAchievementPopupWidget> PopupWidgetClass,
	UAchievementDefinitionAsset* AchievementAsset,
	const FAchievementProgressState ProgressState,
	const float AutoHideDelay,
	const int32 ZOrder)
{
	if (PopupWidgetClass == nullptr || GEngine == nullptr)
	{
		return nullptr;
	}

	if (UWorld* World = ResolveWorldContext(WorldContextObject))
	{
		if (AchievementAsset == nullptr)
		{
			return nullptr;
		}

		return CreateAchievementPopup(
			World,
			PopupWidgetClass,
			AchievementAsset,
			ProgressState,
			AutoHideDelay,
			ZOrder);
	}

	return nullptr;
}

UAchievementPopupWidget* UAchievementBlueprintFunctionLibrary::ShowAchievementPopupById(
	const UObject* WorldContextObject,
	const FName AchievementId,
	TSubclassOf<UAchievementPopupWidget> PopupWidgetClass,
	const float AutoHideDelay,
	const int32 ZOrder)
{
	if (PopupWidgetClass == nullptr || GEngine == nullptr)
	{
		return nullptr;
	}

	if (UAchievementSubsystem* Subsystem = GetAchievementSubsystem(WorldContextObject))
	{
		UAchievementDefinitionAsset* AchievementAsset = Subsystem->GetAchievementDefinitionAsset(AchievementId);
		if (AchievementAsset == nullptr)
		{
			return nullptr;
		}

		const FAchievementProgressState ProgressState = Subsystem->GetAchievementProgress(AchievementId);

		if (UWorld* World = ResolveWorldContext(WorldContextObject))
		{
			return CreateAchievementPopup(
				World,
				PopupWidgetClass,
				AchievementAsset,
				ProgressState,
				AutoHideDelay,
				ZOrder);
		}
	}

	return nullptr;
}
