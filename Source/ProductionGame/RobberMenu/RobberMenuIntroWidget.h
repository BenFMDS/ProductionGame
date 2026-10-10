#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RobberMenuIntroWidget.generated.h"

/**
 * Blueprint base for WBP_RobberMenuIntro.  The Blueprint owns the visuals;
 * the director only tells it when the pull-back has ended.
 */
UCLASS(Abstract, Blueprintable)
class PRODUCTIONGAME_API URobberMenuIntroWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintNativeEvent, Category = "RobberMenu|Intro")
	void SetWaitingForAnyKey(bool bWaiting);

	UFUNCTION(BlueprintNativeEvent, Category = "RobberMenu|Intro")
	void IntroDismissed();

protected:
	virtual void SetWaitingForAnyKey_Implementation(bool bWaiting);
	virtual void IntroDismissed_Implementation();

private:
	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> PressAnyKeyText;

	float LogoAnimationTime = 0.0f;
};
