#include "Heist/RobberPowerCut.h"

#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Heist/RobberStage.h"
#include "InputCoreTypes.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"

DEFINE_LOG_CATEGORY_STATIC(LogRobberPowerCut, Log, All);

namespace RobberPowerCut
{
	// Generous: the player stops moving while the panel's up, so this only catches a pawn that was shoved away.
	const double MaxDistance = 600.;
	const FName CutPowerEvent(TEXT("CutPower"));
	const FLinearColor LampOn(FColor(0x51, 0xcf, 0x66));
	const FLinearColor LampOff(FColor(0x3a, 0x3f, 0x3d));
	const FLinearColor BandOpen(FColor(0x51, 0xcf, 0x66));
	const FLinearColor BandLocked(FColor(0xe0, 0x31, 0x31));
	// How far the switch handle sits from the middle of its slot.
	const float HandleTravel = 22.f;

	bool IsNear(const AController* Controller, const AActor* Generator)
	{
		const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		return Pawn && Generator && FVector::Dist(Pawn->GetActorLocation(), Generator->GetActorLocation()) <= MaxDistance;
	}
}

URobberPowerCut::URobberPowerCut(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true);
}

void URobberPowerCut::BeginFromGenerator(AActor* Generator, AActor* InteractingPlayer)
{
	if (!Generator || !Generator->HasAuthority())
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(InteractingPlayer);
	AController* Controller = Pawn ? Pawn->GetController() : Cast<AController>(InteractingPlayer);
	ARobberController* Robber = Cast<ARobberController>(Controller);
	const ARobberGameState* State = Generator->GetWorld()->GetGameState<ARobberGameState>();
	if (!Robber || !State)
	{
		return;
	}
	if (!State->IsPowerOn())
	{
		Robber->ClientHeistMessage(NSLOCTEXT("RobberPowerCut", "AlreadyOff", "The power's already off."));
		return;
	}
	Robber->ClientOpenPowerCut(Generator);
}

void URobberPowerCut::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (UButton* Button = Find<UButton>(TEXT("TripButton"))) Button->OnClicked.AddUniqueDynamic(this, &URobberPowerCut::OnTrip);
	if (UButton* Button = Find<UButton>(TEXT("LeaveButton"))) Button->OnClicked.AddUniqueDynamic(this, &URobberPowerCut::OnLeave);
}

void URobberPowerCut::OnOpened(AActor* InGenerator)
{
	Generator = InGenerator;
	BreakersDown = 0;
	Needle = 0.f;
	Direction = 1.f;
	Lockout = 0.f;
	bDone = false;
	const ARobberGameState* State = GetRobberState();
	bCutWhenOpened = State && State->IsPowerCut();
	Show(Find<UWidget>(TEXT("LightsOut")), false);
	PickBand();
	SayNextBreaker();
	Draw();
}

void URobberPowerCut::SayNextBreaker()
{
	Say(FText::Format(NSLOCTEXT("RobberPowerCut", "Prompt", "Breaker {0} of {1}. Wait for the green."), BreakersDown + 1, BreakerCount));
}

float URobberPowerCut::CurrentSpeed() const
{
	return NeedleSpeeds.IsEmpty() ? 1.f : NeedleSpeeds[FMath::Min(BreakersDown, NeedleSpeeds.Num() - 1)];
}

float URobberPowerCut::CurrentBandWidth() const
{
	return BandWidths.IsEmpty() ? 0.2f : BandWidths[FMath::Min(BreakersDown, BandWidths.Num() - 1)];
}

void URobberPowerCut::PickBand()
{
	const float Half = CurrentBandWidth() / 2.f;
	const float Old = BandCentre;
	// Far enough from the last band that it can't be caught by not moving.
	for (int32 Try = 0; Try < 20 && (Try == 0 || FMath::Abs(BandCentre - Old) < 0.2f); ++Try)
	{
		BandCentre = FMath::FRandRange(Half + 0.05f, 1.f - Half - 0.05f);
	}
}

void URobberPowerCut::Say(const FText& Text)
{
	SetText(Find<UTextBlock>(TEXT("StatusText")), Text);
}

void URobberPowerCut::Press()
{
	ARobberController* Robber = GetRobber();
	if (bDone || Lockout > 0.f || !Robber)
	{
		return;
	}
	if (FMath::Abs(Needle - BandCentre) <= CurrentBandWidth() / 2.f)
	{
		++BreakersDown;
		if (BreakersDown >= BreakerCount)
		{
			bDone = true;
			CloseIn = CloseDelay;
			Say(NSLOCTEXT("RobberPowerCut", "Done", "Power off. The lasers and cameras are down."));
			Show(Find<UWidget>(TEXT("LightsOut")), true);
			Robber->ServerFinishPowerCut(Generator.Get());
		}
		else
		{
			PickBand();
			SayNextBreaker();
		}
	}
	else
	{
		BreakersDown = FMath::Max(0, BreakersDown - 1);
		Lockout = LockoutSeconds;
		PickBand();
		Say(FText::Format(NSLOCTEXT("RobberPowerCut", "Miss", "Bang. That was loud. Breaker {0} of {1}."), BreakersDown + 1, BreakerCount));
		Robber->ServerPowerCutMiss(Generator.Get());
	}
	Draw();
}

void URobberPowerCut::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ARobberController* Robber = GetRobber();
	const ARobberGameState* State = GetRobberState();
	if (!Robber || !State)
	{
		return;
	}
	if (bDone)
	{
		CloseIn -= InDeltaTime;
		if (CloseIn <= 0.f)
		{
			Robber->ClosePowerCut();
		}
		return;
	}
	// The night ended under them, or a teammate at the other panel got there first.
	if (RobberStage::IsInLobby(State->GetGameStage()) || (State->IsPowerCut() && !bCutWhenOpened))
	{
		Robber->ClosePowerCut();
		return;
	}
	Lockout = FMath::Max(0.f, Lockout - InDeltaTime);
	Needle += Direction * CurrentSpeed() * InDeltaTime;
	if (Needle >= 1.f || Needle <= 0.f)
	{
		Needle = FMath::Clamp(Needle, 0.f, 1.f);
		Direction = -Direction;
	}
	Draw();
}

void URobberPowerCut::Draw()
{
	using namespace RobberPowerCut;
	const float Width = CurrentBandWidth();
	if (UBorder* Band = Find<UBorder>(TEXT("MeterBand")))
	{
		Band->SetBrushColor(Lockout > 0.f ? BandLocked : BandOpen);
		if (UCanvasPanelSlot* BandSlot = Cast<UCanvasPanelSlot>(Band->Slot))
		{
			BandSlot->SetPosition(FVector2D((BandCentre - Width / 2.f) * MeterWidth, 0.f));
			BandSlot->SetSize(FVector2D(Width * MeterWidth, BandSlot->GetSize().Y));
		}
	}
	if (UWidget* NeedleWidget = Find<UWidget>(TEXT("MeterNeedle")))
	{
		NeedleWidget->SetRenderTranslation(FVector2D(Needle * MeterWidth, 0.f));
	}
	if (UWidget* Panel = Find<UWidget>(TEXT("PanelBox")))
	{
		// The panel kicks for the first half of the lockout after a miss.
		const bool bShaking = Lockout > LockoutSeconds / 2.f;
		Panel->SetRenderTranslation(bShaking ? FVector2D(FMath::FRandRange(-6.f, 6.f), FMath::FRandRange(-3.f, 3.f)) : FVector2D::ZeroVector);
	}
	for (int32 Index = 0; Index < BreakerCount; ++Index)
	{
		const bool bDown = Index < BreakersDown;
		if (UBorder* Lamp = Find<UBorder>(FString::Printf(TEXT("Lamp_%d"), Index)))
		{
			Lamp->SetBrushColor(bDown ? LampOff : LampOn);
		}
		if (UWidget* Handle = Find<UWidget>(FString::Printf(TEXT("Handle_%d"), Index)))
		{
			Handle->SetRenderTranslation(FVector2D(0.f, bDown ? HandleTravel * 2.f : 0.f));
		}
		SetText(Find<UTextBlock>(FString::Printf(TEXT("State_%d"), Index)), bDown ? INVTEXT("OFF") : INVTEXT("ON"));
	}
}

void URobberPowerCut::OnTrip()
{
	Press();
}

void URobberPowerCut::OnLeave()
{
	if (ARobberController* Robber = GetRobber())
	{
		Robber->ClosePowerCut();
	}
}

FReply URobberPowerCut::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape)
	{
		OnLeave();
		return FReply::Handled();
	}
	if (Key == EKeys::SpaceBar || Key == EKeys::Enter || Key == EKeys::E || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		// Held from the press that opened the panel, or from a mistimed mash.
		if (!Event.IsRepeat())
		{
			Press();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

FReply URobberPowerCut::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	// A click on the dim background would otherwise take keyboard focus away and kill the trip key.
	return FReply::Handled().SetUserFocus(TakeWidget());
}

void ARobberController::ClientOpenPowerCut_Implementation(AActor* Generator)
{
	if (IsPowerCutOpen())
	{
		return;
	}
	CloseJournal();
	CloseShopCatalogue();
	CloseDevMenu();
	if (!PowerCutWidget)
	{
		if (UClass* WidgetClass = StaticLoadClass(URobberPowerCut::StaticClass(), nullptr, TEXT("/Game/OurStuff/UI/CaseFile/WBP_PowerCut.WBP_PowerCut_C"), nullptr, LOAD_Quiet | LOAD_NoWarn))
		{
			PowerCutWidget = CreateWidget<URobberPowerCut>(this, WidgetClass);
		}
	}
	if (!PowerCutWidget)
	{
		UE_LOG(LogRobberPowerCut, Error, TEXT("WBP_PowerCut is missing, so the generator can't be switched off."));
		return;
	}
	PowerCutWidget->AddToViewport(25);
	PowerCutWidget->OnOpened(Generator);
	EnterModal(this, PowerCutWidget);
}

void ARobberController::ClosePowerCut()
{
	if (!IsPowerCutOpen())
	{
		return;
	}
	PowerCutWidget->RemoveFromParent();
	LeaveModal(this, false);
}

bool ARobberController::IsPowerCutOpen() const
{
	return PowerCutWidget && PowerCutWidget->IsInViewport();
}

void ARobberController::ServerFinishPowerCut_Implementation(AActor* Generator)
{
	const ARobberGameState* State = GetWorld()->GetGameState<ARobberGameState>();
	UFunction* CutPower = Generator ? Generator->FindFunction(RobberPowerCut::CutPowerEvent) : nullptr;
	if (CutPower && State && State->IsPowerOn() && RobberPowerCut::IsNear(this, Generator))
	{
		Generator->ProcessEvent(CutPower, nullptr);
	}
}

void ARobberController::ServerPowerCutMiss_Implementation(AActor* Generator)
{
	ARobberGameState* State = GetWorld()->GetGameState<ARobberGameState>();
	if (State && RobberPowerCut::IsNear(this, Generator))
	{
		State->AddHeat(URobberPowerCut::MissHeat, TEXT("Generator panel"));
	}
}
