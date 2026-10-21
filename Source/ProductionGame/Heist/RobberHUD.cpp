#include "Heist/RobberHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "Heist/RobberEntryPoint.h"
#include "Heist/RobberStage.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"

TArray<FString> ARobberHUD::BuildLines() const
{
	TArray<FString> Lines;
	const ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	const APlayerController* Controller = GetOwningPlayerController();
	if (!GameState || !Controller)
	{
		return Lines;
	}

	const uint8 Stage = GameState->GetGameStage();
	// Objective and jobs live in the quest box (UQuestConversationWidget).
	Lines.Add(RobberStage::GetDisplayName(Stage).ToString().ToUpper());

	const float TimeLeft = GameState->GetPhaseTimeRemaining();
	if (TimeLeft >= 0.f)
	{
		const int32 Seconds = FMath::CeilToInt(TimeLeft);
		Lines.Add(FString::Printf(TEXT("Closing in %d:%02d"), Seconds / 60, Seconds % 60));
	}

	if (Stage == RobberStage::Lobby)
	{
		TArray<const ARobberEntryPoint*> Counted;
		for (const FRobberEntryVote& Vote : GameState->GetVotes())
		{
			if (Vote.Entry && !Counted.Contains(Vote.Entry))
			{
				Counted.Add(Vote.Entry);
				Lines.Add(FString::Printf(TEXT("  %s: %d"), *Vote.Entry->DisplayName.ToString(), GameState->GetVoteCount(Vote.Entry)));
			}
		}
	}

	const ARobberController* RobberController = Cast<ARobberController>(Controller);
	if (RobberController && RobberController->IsHeistMessageVisible())
	{
		Lines.Add(RobberController->GetHeistMessage().ToString());
	}
	return Lines;
}

void ARobberHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	float Y = Canvas->ClipY * 0.06f;
	const float X = Canvas->ClipX * 0.5f;
	for (const FString& Line : BuildLines())
	{
		float Width = 0.f, Height = 0.f;
		GetTextSize(Line, Width, Height, Font, 1.25f);
		DrawText(Line, FLinearColor::Black, X - Width * 0.5f + 2.f, Y + 2.f, Font, 1.25f);
		DrawText(Line, FLinearColor::White, X - Width * 0.5f, Y, Font, 1.25f);
		Y += Height + 4.f;
	}
}
