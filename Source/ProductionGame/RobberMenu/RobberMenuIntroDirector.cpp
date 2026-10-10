#include "RobberMenu/RobberMenuIntroDirector.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Quest/QuestConversationWidget.h"
#include "RobberMenu/RobberMenuIntroWidget.h"
#include "TimerManager.h"

ARobberMenuIntroDirector::ARobberMenuIntroDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	IntroWidgetClass = URobberMenuIntroWidget::StaticClass();
}

void ARobberMenuIntroDirector::BeginPlay()
{
	Super::BeginPlay();

	if (GetWorld()->GetFirstPlayerController())
	{
		RemoveQuestWidgets();
		StartIntro();
		GetWorldTimerManager().SetTimerForNextTick(this, &ARobberMenuIntroDirector::RemoveQuestWidgets);
	}
}

void ARobberMenuIntroDirector::RemoveQuestWidgets()
{
	for (TObjectIterator<UQuestConversationWidget> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld())
		{
			It->RemoveFromParent();
		}
	}

	for (TObjectIterator<UDialoguePresentationWidget> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld())
		{
			It->RemoveFromParent();
		}
	}
}

void ARobberMenuIntroDirector::StartIntro()
{
	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PlayerController || !IntroCamera || bHandedToPlayer)
	{
		return;
	}

	if (IntroWidgetClass && !IntroWidget)
	{
		IntroWidget = CreateWidget<URobberMenuIntroWidget>(PlayerController, IntroWidgetClass);
		if (IntroWidget)
		{
			IntroWidget->AddToPlayerScreen(100);
			IntroWidget->SetWaitingForAnyKey(false);
		}
	}

	ElapsedPullbackTime = 0.0f;
	bIntroPlaying = false;
	bWaitingForKey = bWaitForAnyKey;
	IntroCamera->SetActorTransform(PullbackStart);
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
	PlayerController->SetViewTarget(IntroCamera);

	if (IntroWidget)
	{
		IntroWidget->SetWaitingForAnyKey(false);
	}

	if (LogoHoldDuration > KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().SetTimer(
			LogoHoldTimer,
			this,
			&ARobberMenuIntroDirector::BeginWaitingForKey,
			LogoHoldDuration,
			false);
	}
	else
	{
		BeginWaitingForKey();
	}
}

void ARobberMenuIntroDirector::BeginWaitingForKey()
{
	if (bHandedToPlayer)
	{
		return;
	}

	if (!bWaitForAnyKey)
	{
		BeginPullback();
		return;
	}

	bWaitingForKey = true;
	if (IntroWidget)
	{
		IntroWidget->SetWaitingForAnyKey(true);
	}
	BindAnyKey();
}

void ARobberMenuIntroDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIntroPlaying || !IntroCamera)
	{
		return;
	}

	ElapsedPullbackTime = FMath::Min(ElapsedPullbackTime + DeltaSeconds, PullbackDuration);
	const float LinearAlpha = PullbackDuration > KINDA_SMALL_NUMBER ? ElapsedPullbackTime / PullbackDuration : 1.0f;
	const float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, LinearAlpha, 2.0f);
	FTransform CurrentCameraTransform;
	CurrentCameraTransform.Blend(PullbackStart, PullbackEnd, EasedAlpha);
	IntroCamera->SetActorTransform(CurrentCameraTransform);

	if (LinearAlpha < 1.0f)
	{
		return;
	}

	bIntroPlaying = false;
	SetActorTickEnabled(false);

	FinishIntro();
}

void ARobberMenuIntroDirector::BindAnyKey()
{
	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		return;
	}

	EnableInput(PlayerController);
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &ARobberMenuIntroDirector::ContinueFromIntro);
	}
}

void ARobberMenuIntroDirector::ContinueFromIntro()
{
	if (bHandedToPlayer)
	{
		return;
	}

	if (bWaitingForKey)
	{
		bWaitingForKey = false;
		DisableInput(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
		if (IntroWidget)
		{
			IntroWidget->SetWaitingForAnyKey(false);
		}
		BeginPullback();
	}
}

void ARobberMenuIntroDirector::BeginPullback()
{
	if (bIntroPlaying || bHandedToPlayer)
	{
		return;
	}

	ElapsedPullbackTime = 0.0f;
	bIntroPlaying = true;
	SetActorTickEnabled(true);
}

void ARobberMenuIntroDirector::FinishIntro()
{
	if (bHandedToPlayer)
	{
		return;
	}

	bWaitingForKey = false;
	bHandedToPlayer = true;
	DisableInput(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!PlayerController || !PlayerPawn)
	{
		return;
	}

	PlayerController->SetControlRotation(PullbackEnd.Rotator());
	PlayerController->SetViewTargetWithBlend(PlayerPawn, PlayerCameraBlendDuration, VTBlend_EaseInOut, 2.0f);
	if (IntroWidget)
	{
		IntroWidget->IntroDismissed();
		IntroWidget->RemoveFromParent();
	}

	GetWorldTimerManager().SetTimer(
		WalkaroundEnableTimer,
		this,
		&ARobberMenuIntroDirector::EnableWalkaround,
		FMath::Max(PlayerCameraBlendDuration, KINDA_SMALL_NUMBER),
		false);
}

void ARobberMenuIntroDirector::EnableWalkaround()
{
	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		return;
	}

	PlayerController->SetIgnoreMoveInput(false);
	PlayerController->SetIgnoreLookInput(false);

	if (WalkaroundMenuWidgetClass && !WalkaroundMenuWidget)
	{
		WalkaroundMenuWidget = CreateWidget<UUserWidget>(PlayerController, WalkaroundMenuWidgetClass);
		if (WalkaroundMenuWidget)
		{
			WalkaroundMenuWidget->AddToPlayerScreen(10);
		}
	}
}
