#include "Heist/RobberLaunchEnding.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "GameFramework/HUD.h"
#include "Heist/RobberDevMenu.h"
#include "Heist/RobberStage.h"
#include "Kismet/GameplayStatics.h"
#include "ProductionGameCharacter.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "RobberMenu/RobberCredits.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogRobberLaunchEnding, Log, All);

namespace RobberLaunchEndingLook
{
	// Above the credits (90) so the lobby reload and the credits starting stay behind the black; the dev menu is 100.
	constexpr int32 ZOrder = 95;
	constexpr float LineAt = URobberLaunchEnding::ShotsSeconds + URobberLaunchEnding::FadeSeconds + .8f;
	constexpr float LineFade = 1.2f;
	// The punchline lands on its own, after a beat.
	constexpr float YetAt = LineAt + 3.2f;
	constexpr float YetSlam = .35f;
	constexpr float OutroSeconds = 1.5f;
	// Under the black: the reload and the credits need a moment before the card lifts off them.
	constexpr float SettleSeconds = 1.5f;
	// If the server never ends the run (host gone), stop waiting for it.
	constexpr float GiveUpAfter = URobberLaunchEnding::RunEndsAt + 8.f;
	constexpr float MusicFadeOut = 2.f;

	const FLinearColor Ink(FColor(0xf4, 0xef, 0xe1));

	/** Pixels at 1080p; a UMG font size is in points. */
	FSlateFontInfo EndFont(float Pixels, const TCHAR* Typeface)
	{
		return FSlateFontInfo(LoadObject<UFont>(nullptr, TEXT("/Game/OurStuff/UI/CaseFile/Fonts/F_Fredoka.F_Fredoka")), Pixels * .75f, Typeface);
	}

	float Clamp01(float Value) { return FMath::Clamp(Value, 0.f, 1.f); }
}

URobberLaunchEnding* URobberLaunchEnding::Play(ARobberController* Controller, USoundBase* Music)
{
	if (!Controller || !Controller->IsLocalController())
	{
		return nullptr;
	}
	if (URobberLaunchEnding* Playing = Find(Controller))
	{
		return Playing;
	}
	URobberLaunchEnding* Ending = CreateWidget<URobberLaunchEnding>(Controller, StaticClass());
	if (Ending)
	{
		Ending->AddToViewport(RobberLaunchEndingLook::ZOrder);
		Ending->Start(Music);
	}
	return Ending;
}

URobberLaunchEnding* URobberLaunchEnding::Find(const ARobberController* Controller)
{
	if (!Controller)
	{
		return nullptr;
	}
	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller, Found, StaticClass(), true);
	for (UUserWidget* Widget : Found)
	{
		URobberLaunchEnding* Ending = Cast<URobberLaunchEnding>(Widget);
		if (Ending && Ending->GetOwningPlayer() == Controller && Ending->IsInViewport() && !Ending->bFinished)
		{
			return Ending;
		}
	}
	return nullptr;
}

void URobberLaunchEnding::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Build();
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URobberLaunchEnding::Build()
{
	using namespace RobberLaunchEndingLook;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EndingRoot"));
	WidgetTree->RootWidget = Root;

	Blackout = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Blackout"));
	Blackout->SetColorAndOpacity(FLinearColor::Black);
	Blackout->SetRenderOpacity(0.f);
	UCanvasPanelSlot* BlackSlot = Root->AddChildToCanvas(Blackout);
	BlackSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BlackSlot->SetOffsets(FMargin(0.f));

	Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EndLine"));
	Line->SetFont(EndFont(54.f, TEXT("Regular")));
	Line->SetColorAndOpacity(FSlateColor(Ink));
	Line->SetJustification(ETextJustify::Center);
	Line->SetText(NSLOCTEXT("RobberLaunch", "EndLine", "You escaped to the one place capitalism hasn't reached."));
	Line->SetRenderOpacity(0.f);

	Yet = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EndYet"));
	Yet->SetFont(EndFont(110.f, TEXT("Bold")));
	Yet->SetColorAndOpacity(FSlateColor(Ink));
	Yet->SetJustification(ETextJustify::Center);
	Yet->SetText(NSLOCTEXT("RobberLaunch", "EndYet", "Yet."));
	Yet->SetRenderOpacity(0.f);
	Yet->SetRenderTransformPivot(FVector2D(.5f, .5f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EndCard"));
	Column->AddChildToVerticalBox(Line)->SetHorizontalAlignment(HAlign_Center);
	UVerticalBoxSlot* YetSlot = Column->AddChildToVerticalBox(Yet);
	YetSlot->SetHorizontalAlignment(HAlign_Center);
	YetSlot->SetPadding(FMargin(0.f, 36.f, 0.f, 0.f));
	UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(Column);
	CardSlot->SetAnchors(FAnchors(.5f, .5f));
	CardSlot->SetAlignment(FVector2D(.5f, .5f));
	CardSlot->SetAutoSize(true);
}

void URobberLaunchEnding::Start(USoundBase* Music)
{
	HideOtherWidgets();
	if (ARobberController* Controller = Cast<ARobberController>(GetOwningPlayer()))
	{
		Controller->SetIgnoreMoveInput(true);
		Controller->SetIgnoreLookInput(true);
		// Canvas-drawn HUD bits (outline boxes) aren't widgets, so opacity doesn't reach them.
		if (AHUD* Hud = Controller->GetHUD())
		{
			bHudWasShown = Hud->bShowHUD;
			Hud->bShowHUD = false;
		}
		if (AProductionGameCharacter* Robber = Cast<AProductionGameCharacter>(Controller->GetPawn()))
		{
			Robber->SetInteractPromptMuted(true);
		}
	}
	if (Music)
	{
		// Auto-destroys when it ends; the night swap is a level stream, so the world (and the sound) carries on.
		MusicPlayer = UGameplayStatics::CreateSound2D(GetOwningPlayer(), Music, 1.f, 1.f, 0.f, nullptr, false, true);
		if (MusicPlayer)
		{
			MusicPlayer->Play();
		}
	}
	UE_LOG(LogRobberLaunchEnding, Display, TEXT("Launch ending started: %d widgets hidden, music %s"), Hidden.Num(), Music ? *Music->GetName() : TEXT("none"));
}

void URobberLaunchEnding::HideOtherWidgets()
{
	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetOwningPlayer(), Found, UUserWidget::StaticClass(), true);
	for (UUserWidget* Widget : Found)
	{
		// Visibility, not render opacity: rounded-box outlines still drew at opacity 0. The dev menu stays usable.
		if (Widget == this || !Widget->IsInViewport() || !Widget->IsVisible() || Widget->IsA<URobberDevMenu>() || Widget->IsA<URobberCredits>())
		{
			continue;
		}
		// A widget that shows itself again (its own tick or a popup) is hidden again, keeping the first state seen.
		if (!Hidden.ContainsByPredicate([Widget](const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Entry) { return Entry.Key.Get() == Widget; }))
		{
			Hidden.Emplace(Widget, Widget->GetVisibility());
		}
		Widget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void URobberLaunchEnding::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	using namespace RobberLaunchEndingLook;
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bFinished)
	{
		return;
	}
	Elapsed += InDeltaTime;
	// Popups that turn up mid-ending (a heist message, an achievement) go too, until the run ends.
	if (Elapsed < RunEndsAt)
	{
		HideOtherWidgets();
	}

	const float Outro = OutroAt < 0.f ? 1.f : 1.f - Clamp01((Elapsed - OutroAt) / OutroSeconds);
	Blackout->SetRenderOpacity(GetBlackout() * Outro);
	Line->SetRenderOpacity(GetLineOpacity() * Outro);
	Yet->SetRenderOpacity(GetYetOpacity() * Outro);
	const float Slam = Clamp01((Elapsed - YetAt) / YetSlam);
	Yet->SetRenderScale(FVector2D(FMath::Lerp(1.8f, 1.f, 1.f - FMath::Square(1.f - Slam))));

	if (OutroAt < 0.f && Elapsed >= RunEndsAt + SettleSeconds)
	{
		const ARobberGameState* State = GetWorld() ? GetWorld()->GetGameState<ARobberGameState>() : nullptr;
		const ARobberController* Controller = Cast<ARobberController>(GetOwningPlayer());
		const bool bRunOver = State && RobberStage::IsInLobby(State->GetGameStage());
		// Credits only roll outside tests (ARobberController::bRollCreditsInTests), so don't wait for them there.
		const bool bCreditsReady = (Controller && Controller->GetCredits()) || (GIsAutomationTesting && !ARobberController::bRollCreditsInTests);
		if ((bRunOver && bCreditsReady) || Elapsed >= GiveUpAfter)
		{
			OutroAt = Elapsed;
		}
	}
	if (OutroAt >= 0.f && Elapsed >= OutroAt + OutroSeconds)
	{
		Finish();
	}
}

void URobberLaunchEnding::Finish()
{
	bFinished = true;
	for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Entry : Hidden)
	{
		// Only if it's still as we left it: one that changed its own visibility since (the iris wipe collapsing
		// itself) knows better, and giving a stale visible state back could leave the screen black.
		UUserWidget* Widget = Entry.Key.Get();
		if (Widget && Widget->GetVisibility() == ESlateVisibility::Hidden)
		{
			Widget->SetVisibility(Entry.Value);
		}
	}
	Hidden.Reset();
	ARobberController* Controller = Cast<ARobberController>(GetOwningPlayer());
	if (Controller)
	{
		Controller->SetIgnoreMoveInput(false);
		Controller->SetIgnoreLookInput(false);
		if (AHUD* Hud = Controller->GetHUD())
		{
			Hud->bShowHUD = bHudWasShown;
		}
		if (AProductionGameCharacter* Robber = Cast<AProductionGameCharacter>(Controller->GetPawn()))
		{
			Robber->SetInteractPromptMuted(false);
		}
	}
	// The music plays on under the credits and fades when they close; with no credits it fades now.
	if (UAudioComponent* Music = MusicPlayer.Get(); Music && Music->IsPlaying())
	{
		URobberCredits* Credits = Controller ? Controller->GetCredits() : nullptr;
		if (Credits)
		{
			Credits->OnFinished.AddWeakLambda(Music, [Music]() { Music->FadeOut(RobberLaunchEndingLook::MusicFadeOut, 0.f); });
		}
		else
		{
			Music->FadeOut(RobberLaunchEndingLook::MusicFadeOut, 0.f);
		}
	}
	MusicPlayer = nullptr;
	RemoveFromParent();
	UE_LOG(LogRobberLaunchEnding, Display, TEXT("Launch ending finished at %.1f s"), Elapsed);
}

float URobberLaunchEnding::GetBlackout() const
{
	return RobberLaunchEndingLook::Clamp01((Elapsed - ShotsSeconds) / FadeSeconds);
}

float URobberLaunchEnding::GetLineOpacity() const
{
	return RobberLaunchEndingLook::Clamp01((Elapsed - RobberLaunchEndingLook::LineAt) / RobberLaunchEndingLook::LineFade);
}

float URobberLaunchEnding::GetYetOpacity() const
{
	return RobberLaunchEndingLook::Clamp01((Elapsed - RobberLaunchEndingLook::YetAt) / RobberLaunchEndingLook::YetSlam);
}

FString URobberLaunchEnding::GetCardText() const
{
	return FString::Printf(TEXT("%s %s"), *Line->GetText().ToString(), *Yet->GetText().ToString());
}

bool URobberLaunchEnding::IsMusicPlaying() const
{
	return MusicPlayer && MusicPlayer->IsPlaying();
}
