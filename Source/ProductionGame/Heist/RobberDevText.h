#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RobberDevText.generated.h"

/**
 * Greybox dev text (room names on the floor, labels over hiding spots and props, design notes on marker signs) is hidden
 * in game unless `Robber.ShowDevText 1` or the dev menu's Labels button. Editor viewports always show it.
 *
 * What counts is set per map by the tools (`-run=DevText -Apply`): a text component tagged `DevText` is hidden, and an
 * actor tagged `DevText` is hidden whole, collision too (marker sign posts). Gag text on signs and dressing isn't tagged.
 * Hiding uses SetVisibility, which the game's own label code doesn't touch (it uses SetHiddenInGame), so neither undoes
 * the other. Local to this machine.
 */
UCLASS()
class PRODUCTIONGAME_API URobberDevText : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static const FName Tag;

	static bool IsShown();
	static void SetShown(bool bShow);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Hides or shows the tagged text in one level (a night streaming in) or, with nullptr, the whole world. */
	void Apply(ULevel* Level);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void OnLevelAdded(ULevel* Level, UWorld* World);
	void ApplyToActor(AActor* Actor, bool bShow);

	FDelegateHandle LevelAddedHandle;
	/** What each component's visibility was before we hid it, so showing again doesn't show a sign's unused back text. */
	TMap<TWeakObjectPtr<USceneComponent>, bool> WasVisible;
};
