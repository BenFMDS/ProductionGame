#include "Heist/RobberFocus.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Heist/RobberHighlight.h"
#include "Heist/RobberInteract.h"

URobberFocusComponent::URobberFocusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the pawn and controller have moved, so the highlight subsystem (ticked after every group) reads this frame's aim.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

URobberFocusComponent* URobberFocusComponent::For(APlayerController* LocalPlayer)
{
	if (!LocalPlayer || !LocalPlayer->IsLocalController())
	{
		return nullptr;
	}
	if (URobberFocusComponent* Existing = LocalPlayer->FindComponentByClass<URobberFocusComponent>())
	{
		return Existing;
	}
	// Added here rather than in each controller's constructor: L_MainMenu, the hideout and the nights use different controllers.
	URobberFocusComponent* Focus = NewObject<URobberFocusComponent>(LocalPlayer, TEXT("RobberFocus"));
	Focus->RegisterComponent();
	Focus->Refresh();
	return Focus;
}

URobberFocusComponent* URobberFocusComponent::GetLocalFocus(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? For(World->GetFirstPlayerController()) : nullptr;
}

FString URobberFocusComponent::GetPrompt() const
{
	const URobberInteractComponent* Interact = URobberInteractComponent::Of(Target.Get());
	return Interact ? Interact->GetPrompt() : FString();
}

bool URobberFocusComponent::IsUsable(const AActor* Actor)
{
	const URobberInteractComponent* Interact = URobberInteractComponent::Of(Actor);
	return !Interact || !Interact->GetPrompt().IsEmpty();
}

void URobberFocusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Refresh();
}

void URobberFocusComponent::Refresh()
{
	HitActor = nullptr;
	Target = nullptr;
	const APlayerController* Player = Cast<APlayerController>(GetOwner());
	const APlayerCameraManager* Camera = Player ? Player->PlayerCameraManager.Get() : nullptr;
	if (!Camera)
	{
		return;
	}
	const FVector Eye = Camera->GetCameraLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RobberFocus), false, Player->GetPawn());
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Camera->GetCameraRotation().Vector() * Reach, ECC_Visibility, Params))
	{
		return;
	}
	AActor* Actor = Hit.GetActor();
	HitActor = Actor;
	if (Actor && (URobberInteractComponent::Of(Actor) || Actor->FindComponentByClass<URobberHighlightComponent>()) && IsUsable(Actor))
	{
		Target = Actor;
	}
}
