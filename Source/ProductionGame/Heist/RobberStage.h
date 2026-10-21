#pragma once

#include "CoreMinimal.h"

/**
 * Byte values of the E_GameStage Blueprint enum. The values follow the order entries were added in the
 * editor, not the order the phases run in: Investigate was appended last, after Win.
 */
namespace RobberStage
{
	constexpr uint8 Lobby = 0;
	constexpr uint8 BreakIn = 1;
	constexpr uint8 Heist = 2;
	constexpr uint8 Escape = 3;
	constexpr uint8 Lose = 4;
	constexpr uint8 Continue = 5;
	constexpr uint8 Win = 6;
	constexpr uint8 Investigate = 7;

	inline FText GetDisplayName(uint8 Stage)
	{
		switch (Stage)
		{
		case Lobby: return NSLOCTEXT("RobberStage", "Lobby", "Lobby");
		case BreakIn: return NSLOCTEXT("RobberStage", "BreakIn", "Break In");
		case Investigate: return NSLOCTEXT("RobberStage", "Investigate", "Investigate");
		case Heist: return NSLOCTEXT("RobberStage", "Heist", "Heist");
		case Escape: return NSLOCTEXT("RobberStage", "Escape", "Escape");
		case Lose: return NSLOCTEXT("RobberStage", "Lose", "Lose");
		case Continue: return NSLOCTEXT("RobberStage", "Continue", "Continue");
		case Win: return NSLOCTEXT("RobberStage", "Win", "Win");
		default: return FText::AsNumber(Stage);
		}
	}
}
