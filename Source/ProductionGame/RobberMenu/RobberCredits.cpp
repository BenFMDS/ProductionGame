#include "RobberMenu/RobberCredits.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace RobberCreditsDetail
{
	/** The WBP's stage is 1920 x 1080. The roll is laid out at the top and pushed down this far to start below the bottom edge. */
	constexpr float StageHeight = 1080.f;
}

void URobberCredits::NativeConstruct()
{
	Super::NativeConstruct();
	Scrolled = 0.f;
	bFinished = false;
	if (UWidget* Roll = Find<UWidget>(TEXT("CreditsRoll")))
	{
		Roll->SetRenderTranslation(FVector2D(0.f, RobberCreditsDetail::StageHeight));
	}
	if (Music)
	{
		MusicPlayer = UGameplayStatics::SpawnSound2D(this, Music);
	}
}

void URobberCredits::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UWidget* Roll = Find<UWidget>(TEXT("CreditsRoll"));
	if (!Roll || bFinished)
	{
		return;
	}
	Scrolled += ScrollSpeed * InDeltaTime;
	Roll->SetRenderTranslation(FVector2D(0.f, RobberCreditsDetail::StageHeight - Scrolled));
	// A new widget measures zero on its first frame, which would end the roll at once.
	const float RollHeight = Roll->GetDesiredSize().Y;
	if (RollHeight > 0.f && Scrolled >= RobberCreditsDetail::StageHeight + RollHeight)
	{
		CloseMenu();
	}
}

void URobberCredits::CloseMenu()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	if (MusicPlayer)
	{
		MusicPlayer->Stop();
		MusicPlayer = nullptr;
	}
	Super::CloseMenu();
	OnFinished.Broadcast();
}
