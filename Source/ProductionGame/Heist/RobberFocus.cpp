#include "Heist/RobberFocus.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Heist/RobberHighlight.h"
#include "Heist/RobberInteract.h"
#include "Heist/RobberPropBase.h"
#include "UObject/UnrealType.h"

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

void URobberFocusComponent::DriveInteraction(UActorComponent* Interaction)
{
	const APawn* Pawn = Interaction ? Cast<APawn>(Interaction->GetOwner()) : nullptr;
	const URobberFocusComponent* Focus = Pawn ? For(Cast<APlayerController>(Pawn->GetController())) : nullptr;
	if (!Focus)
	{
		return;
	}
	AActor* Aimed = Focus->GetTarget();
	FString Prompt = Focus->GetPrompt();
	if (!URobberInteractComponent::Of(Aimed))
	{
		// No prompt still takes the press, as the old trace did: an exhibit in Break In answers "won't talk yet".
		AActor* Hit = Aimed ? Aimed : Focus->GetHitActor();
		const bool bInteract = URobberInteractComponent::Of(Hit) != nullptr;
		Aimed = Hit && (bInteract || Hit->FindFunction(TEXT("GetInteractData"))) ? Hit : nullptr;
		Prompt = bInteract ? URobberInteractComponent::Of(Hit)->GetPrompt() : ARobberPropBase::GetInteractData(Aimed);
	}

	if (FObjectPropertyBase* ActorVar = FindFProperty<FObjectPropertyBase>(Interaction->GetClass(), TEXT("InteractActor")))
	{
		ActorVar->SetObjectPropertyValue_InContainer(Interaction, Aimed);
	}
	FObjectPropertyBase* WidgetRef = FindFProperty<FObjectPropertyBase>(Interaction->GetClass(), TEXT("Interaction Widget Ref"));
	UObject* Widget = WidgetRef ? WidgetRef->GetObjectPropertyValue_InContainer(Interaction) : nullptr;
	UFunction* UpdateWidget = Widget ? Widget->FindFunction(TEXT("UpdateWidget")) : nullptr;
	// WBP_Interaction's UpdateWidget(Visbale?, Data), the same call the old trace made.
	struct { bool bVisible; FString Data; } Params{ Aimed != nullptr, Prompt };
	if (UpdateWidget && UpdateWidget->ParmsSize == sizeof(Params))
	{
		Widget->ProcessEvent(UpdateWidget, &Params);
	}
}

void URobberFocusComponent::PressLocal(UActorComponent* Interaction)
{
	const FObjectPropertyBase* ActorVar = Interaction ? FindFProperty<FObjectPropertyBase>(Interaction->GetClass(), TEXT("InteractActor")) : nullptr;
	const AActor* Aimed = ActorVar ? Cast<AActor>(ActorVar->GetObjectPropertyValue_InContainer(Interaction)) : nullptr;
	URobberInteractComponent* Interact = URobberInteractComponent::Of(Aimed);
	if (Interact && !Interact->bServerOnly)
	{
		Interact->Press(Interaction->GetOwner());
	}
}
