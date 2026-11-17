#include "Heist/RobberDevText.h"

#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"

const FName URobberDevText::Tag(TEXT("DevText"));

namespace
{
	void ReapplyAll(IConsoleVariable*)
	{
		if (!GEngine)
		{
			return;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World())
			{
				if (URobberDevText* DevText = World->GetSubsystem<URobberDevText>())
				{
					DevText->Apply(nullptr);
				}
			}
		}
	}

	// Console changes go through the callback too, so `Robber.ShowDevText 1` works without the menu.
	TAutoConsoleVariable<bool> CVarShowDevText(TEXT("Robber.ShowDevText"), false,
		TEXT("Show the greybox's dev text in game: room names on the floor, labels over hiding spots and props, marker signs."),
		FConsoleVariableDelegate::CreateStatic(&ReapplyAll), ECVF_Default);
}

bool URobberDevText::IsShown()
{
	return CVarShowDevText.GetValueOnGameThread();
}

void URobberDevText::SetShown(bool bShow)
{
	CVarShowDevText->Set(bShow, ECVF_SetByConsole);
}

bool URobberDevText::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URobberDevText::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &URobberDevText::OnLevelAdded);
}

void URobberDevText::Deinitialize()
{
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	Super::Deinitialize();
}

void URobberDevText::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Apply(nullptr);
}

void URobberDevText::OnLevelAdded(ULevel* Level, UWorld* World)
{
	if (World == GetWorld() && Level)
	{
		Apply(Level);
	}
}

void URobberDevText::Apply(ULevel* Level)
{
	const bool bShow = IsShown();
	if (Level)
	{
		for (AActor* Actor : Level->Actors)
		{
			ApplyToActor(Actor, bShow);
		}
		return;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		ApplyToActor(*It, bShow);
	}
}

void URobberDevText::ApplyToActor(AActor* Actor, bool bShow)
{
	if (!IsValid(Actor))
	{
		return;
	}
	if (Actor->Tags.Contains(Tag))
	{
		Actor->SetActorHiddenInGame(!bShow);
		Actor->SetActorEnableCollision(bShow);
		return;
	}
	TArray<UTextRenderComponent*> Texts;
	Actor->GetComponents(Texts);
	for (UTextRenderComponent* Text : Texts)
	{
		if (!Text->ComponentHasTag(Tag))
		{
			continue;
		}
		if (bShow)
		{
			if (const bool* Was = WasVisible.Find(Text))
			{
				Text->SetVisibility(*Was);
				WasVisible.Remove(Text);
			}
		}
		else if (!WasVisible.Contains(Text))
		{
			WasVisible.Add(Text, Text->GetVisibleFlag());
			Text->SetVisibility(false);
		}
	}
}
