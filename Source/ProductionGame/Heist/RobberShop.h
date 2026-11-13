#pragma once

#include "CoreMinimal.h"
#include "Heist/RobberCaseFileUI.h"
#include "RobberShop.generated.h"

/**
 * Logic for WBP_ShopCatalogue, the mail-order catalogue players open from the lectern in the lobby. It has no items yet. Widgets it uses, by name (all optional): PageSwitcher (widget switcher: cover, then one per spread),
 * PageLabel, EditionText, CashText, PrevButton, NextButton, DoneButton.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberShopCatalogue : public URobberCaseFileWidgetBase
{
	GENERATED_BODY()

public:
	URobberShopCatalogue(const FObjectInitializer& Initializer);

	/** Back to the cover and up to date with tonight's edition. */
	void OnOpened();

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ShowPage(int32 Index);

	int32 GetPage() const { return Page; }
	int32 GetPageCount() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	UFUNCTION() void OnPrev();
	UFUNCTION() void OnNext();
	UFUNCTION() void OnDone();

	int32 Page = 0;
};
