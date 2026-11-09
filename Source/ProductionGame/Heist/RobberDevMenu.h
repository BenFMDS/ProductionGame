#pragma once

#include "CoreMinimal.h"
#include "Heist/RobberCaseFileUI.h"
#include "RobberDevMenu.generated.h"

/** What a dev menu button asks the server to do. Value depends on the action; see ARobberController::ServerDevAction. */
UENUM()
enum class ERobberDevAction : uint8
{
	SwitchNight,		// Value: night index, INDEX_NONE for the tutorial
	JumpToStage,		// Value: a RobberStage
	FinishNight,		// Value: 1 wins, 0 loses
	GiveVaultCode,
	GiveKeys,
	AddHeat,			// Value: how much
	CoolHeat,
	AddGuardsTime,		// Value: seconds
	ToggleImmunity,
	ShowQuests,
	FindExhibit,		// Value: row of the quest list
};

/** One button's action. UButton's click event carries no arguments, so each button gets its own listener. */
UCLASS()
class URobberDevMenuClick : public UObject
{
	GENERATED_BODY()

public:
	ERobberDevAction Action = ERobberDevAction::SwitchNight;
	int32 Value = 0;

	UFUNCTION()
	void OnClicked();
};

/**
 * Logic for WBP_DevMenu, opened with F10 (ARobberController::ToggleDevMenu). Not available in Shipping builds. Widgets it
 * uses, by name (all optional): StatusText, ResultText, Night_Tutorial, Night_1 to Night_7, Stage_BreakIn,
 * Stage_Investigate, Stage_Heist, Stage_Escape, End_Win, End_Lose, GiveCode, GiveKeys, HeatUp, HeatCool, GuardsMore.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberDevMenu : public URobberCaseFileWidgetBase
{
	GENERATED_BODY()

public:
	void Send(ERobberDevAction Action, int32 Value);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Bind(const FString& ButtonName, ERobberDevAction Action, int32 Value = 0);
	void RebuildQuestRows(const ARobberController* Robber);

	UPROPERTY(Transient) TObjectPtr<class UVerticalBox> QuestRows;
	UPROPERTY(Transient) TObjectPtr<class UTextBlock> QuestTextStyle;
	int32 ShownQuestRevision = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URobberDevMenuClick>> Clicks;
};
