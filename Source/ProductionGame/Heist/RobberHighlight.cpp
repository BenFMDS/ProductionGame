#include "Heist/RobberHighlight.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Dialogue/QuestDialogueCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Heist/RobberFocus.h"
#include "Heist/RobberInteractables.h"
#include "Robber/RobberGameState.h"

URobberHighlightComponent::URobberHighlightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URobberHighlightComponent::BeginPlay()
{
	Super::BeginPlay();
	if (URobberHighlightSubsystem* Subsystem = GetWorld()->GetSubsystem<URobberHighlightSubsystem>())
	{
		Subsystem->Register(this);
	}
}

void URobberHighlightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URobberHighlightSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<URobberHighlightSubsystem>() : nullptr)
	{
		Subsystem->Unregister(this);
	}
	Apply(false);
	Super::EndPlay(EndPlayReason);
}

void URobberHighlightComponent::SetManualHighlight(bool bOn)
{
	bManualOn = bOn;
	// Straight away, not next frame: callers (and tests) read IsHighlighted() right after.
	if (Mode == ERobberHighlightMode::Manual)
	{
		Apply(bOn);
	}
}

bool URobberHighlightComponent::IsInClearView(const AActor* Target, const APlayerController* Viewer, double InRange)
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
	if (FVector::Dist(Eye, Centre) > InRange)
	{
		return false;
	}
	// Otherwise the outline (or "!") would show straight through whatever is in front of it.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RobberClearView), false, Viewer->GetPawn());
	Params.AddIgnoredActor(Target);
	return !Target->GetWorld()->LineTraceTestByChannel(Eye, Centre, ECC_Visibility, Params);
}

void URobberHighlightComponent::GatherParts(TArray<TWeakObjectPtr<UPrimitiveComponent>>& Out) const
{
	for (UActorComponent* Component : GetOwner()->GetComponents())
	{
		UPrimitiveComponent* Part = Cast<UPrimitiveComponent>(Component);
		if (!Part || !Part->IsRegistered())
		{
			continue;
		}
		if (PartNames.Num() > 0)
		{
			if (PartNames.Contains(Part->GetFName()))
			{
				Out.Add(Part);
			}
			continue;
		}
		// Doors, valves and cases are several pieces, so every visible one.
		if (Part->IsVisible() && !Part->bHiddenInGame && !Part->IsA<UTextRenderComponent>() && !Part->IsA<UWidgetComponent>())
		{
			Out.Add(Part);
		}
	}
}

void URobberHighlightComponent::Apply(bool bOn)
{
	bHighlighted = bOn;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Now;
	if (bOn && GetOwner())
	{
		GatherParts(Now);
	}
	for (const TWeakObjectPtr<UPrimitiveComponent>& Part : LitParts)
	{
		if (Part.IsValid() && !Now.Contains(Part))
		{
			Part->SetRenderCustomDepth(false);
		}
	}
	for (const TWeakObjectPtr<UPrimitiveComponent>& Part : Now)
	{
		if (!Part->bRenderCustomDepth)
		{
			Part->SetRenderCustomDepth(true);
		}
	}
	LitParts = MoveTemp(Now);
}

bool URobberHighlightSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

bool URobberHighlightSubsystem::IsUsable(const AActor* Actor)
{
	return URobberFocusComponent::IsUsable(Actor);
}

void URobberHighlightSubsystem::Tick(float DeltaTime)
{
	// The local player's own view: a dedicated server has none and lights nothing.
	APlayerController* LocalPlayer = GetWorld()->GetFirstPlayerController();
	if (LocalPlayer && !LocalPlayer->IsLocalController())
	{
		LocalPlayer = nullptr;
	}
	const URobberFocusComponent* Focus = URobberFocusComponent::For(LocalPlayer);
	AActor* Target = Focus ? Focus->GetTarget() : nullptr;
	const URobberHighlightComponent* TargetHighlight = Target ? Target->FindComponentByClass<URobberHighlightComponent>() : nullptr;
	if (!TargetHighlight || TargetHighlight->Mode != ERobberHighlightMode::OnLook)
	{
		Target = nullptr;
	}
	LookTarget = Target;

	Components.RemoveAll([](const TWeakObjectPtr<URobberHighlightComponent>& Each) { return !Each.IsValid(); });
	for (const TWeakObjectPtr<URobberHighlightComponent>& Each : Components)
	{
		URobberHighlightComponent* Highlight = Each.Get();
		bool bOn = false;
		switch (Highlight->Mode)
		{
		case ERobberHighlightMode::OnLook: bOn = Target && Highlight->GetOwner() == Target; break;
		case ERobberHighlightMode::InRange: bOn = IsUsable(Highlight->GetOwner()) && URobberHighlightComponent::IsInClearView(Highlight->GetOwner(), LocalPlayer, Highlight->Range); break;
		case ERobberHighlightMode::Manual: bOn = Highlight->WantsManual(); break;
		}
		Highlight->Apply(bOn);
	}
}
