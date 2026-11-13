#include "Heist/RobberCrewTag.h"

#include "ProductionGameCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Border.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Heist/HeistTypes.h"
#include "Heist/RobberStage.h"
#include "Robber/RobberGameState.h"

void URobberCrewTag::Refresh(const APlayerState* Player, bool bFar)
{
	SetText(Find<UTextBlock>(TEXT("TagName")), Player ? FText::FromString(Player->GetPlayerName()) : FText::GetEmpty());
	if (UBorder* Colour = Find<UBorder>(TEXT("TagColour")))
	{
		Colour->SetBrushColor(CrewColour(Player));
	}
	const AProductionGameCharacter* Robber = Player ? Cast<AProductionGameCharacter>(Player->GetPawn()) : nullptr;
	const bool bHiding = Robber && Robber->IsHiding();
	UTextBlock* Status = Find<UTextBlock>(TEXT("TagStatus"));
	if (bHiding)
	{
		SetText(Status, NSLOCTEXT("RobberCaseFile", "Hiding", "Hiding"));
	}
	Show(Status, bHiding);
	SetRenderTransformPivot(FVector2D(0.5f, 1.f));
	SetRenderScale(FVector2D(bFar ? FarScale : 1.f));
}

FString URobberCrewTag::GetShownName() const
{
	const UTextBlock* Name = Find<UTextBlock>(TEXT("TagName"));
	return Name ? Name->GetText().ToString() : FString();
}

bool URobberCrewTag::IsStatusShown() const
{
	const UWidget* Status = Find(TEXT("TagStatus"));
	return Status && Status->GetVisibility() != ESlateVisibility::Collapsed;
}

URobberCrewTagComponent::URobberCrewTagComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	SetPivot(FVector2D(0.5f, 1.f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
}

void URobberCrewTagComponent::BeginPlay()
{
	// Soft path: this component is added at runtime, and the folder is always cooked (DefaultGame.ini).
	SetWidgetClass(StaticLoadClass(URobberCrewTag::StaticClass(), nullptr, TEXT("/Game/OurStuff/UI/CaseFile/WBP_CrewTag.WBP_CrewTag_C"), nullptr, LOAD_Quiet | LOAD_NoWarn));
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		SetRelativeLocation(FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + HeadGap));
	}
	Super::BeginPlay();
}

bool URobberCrewTagComponent::ShouldShow(const APawn* Pawn, const APlayerController* Viewer) const
{
	if (!Pawn || !Viewer || !Viewer->PlayerCameraManager || !Pawn->GetPlayerState() || Pawn->IsLocallyControlled())
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrewTagSight), false, Pawn);
	Params.AddIgnoredActor(Viewer->GetPawn());
	return !GetWorld()->LineTraceTestByChannel(Viewer->PlayerCameraManager->GetCameraLocation(), GetComponentLocation(), ECC_Visibility, Params);
}

void URobberCrewTagComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* Viewer = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	bWantsToShow = ShouldShow(Pawn, Viewer);
	if (IsVisible() != bWantsToShow)
	{
		SetVisibility(bWantsToShow);
	}
	if (URobberCrewTag* Tag = bWantsToShow ? Cast<URobberCrewTag>(GetUserWidgetObject()) : nullptr)
	{
		Tag->Refresh(Pawn->GetPlayerState(), FVector::Dist(Viewer->PlayerCameraManager->GetCameraLocation(), GetComponentLocation()) > FarDistance);
	}
}
