#include "Heist/RobberShop.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "InputCoreTypes.h"
#include "Robber/RobberController.h"

URobberShopCatalogue::URobberShopCatalogue(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true);
}

void URobberShopCatalogue::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (UButton* Button = Find<UButton>(TEXT("PrevButton"))) Button->OnClicked.AddUniqueDynamic(this, &URobberShopCatalogue::OnPrev);
	if (UButton* Button = Find<UButton>(TEXT("NextButton"))) Button->OnClicked.AddUniqueDynamic(this, &URobberShopCatalogue::OnNext);
	if (UButton* Button = Find<UButton>(TEXT("DoneButton"))) Button->OnClicked.AddUniqueDynamic(this, &URobberShopCatalogue::OnDone);
}

int32 URobberShopCatalogue::GetPageCount() const
{
	const UWidgetSwitcher* Switcher = Find<UWidgetSwitcher>(TEXT("PageSwitcher"));
	return Switcher ? Switcher->GetNumWidgets() : 1;
}

void URobberShopCatalogue::OnOpened()
{
	SetText(Find<UTextBlock>(TEXT("EditionText")), FText::Format(NSLOCTEXT("RobberShop", "Edition", "{0} edition"), NightName(GetRobberState())));
	// Cash only exists in Blueprints so far; nothing can be bought until the shop has stock anyway.
	SetText(Find<UTextBlock>(TEXT("CashText")), INVTEXT("$0"));
	ShowPage(0);
}

void URobberShopCatalogue::ShowPage(int32 Index)
{
	Page = FMath::Clamp(Index, 0, GetPageCount() - 1);
	if (UWidgetSwitcher* Switcher = Find<UWidgetSwitcher>(TEXT("PageSwitcher")))
	{
		Switcher->SetActiveWidgetIndex(Page);
	}
	const FText Label = Page == 0 ? NSLOCTEXT("RobberShop", "Cover", "Cover")
		: FText::Format(NSLOCTEXT("RobberShop", "Pages", "Pages {0} and {1}"), Page * 2, Page * 2 + 1);
	SetText(Find<UTextBlock>(TEXT("PageLabel")), Label);
	const auto Enable = [this](const TCHAR* Name, bool bEnabled)
	{
		if (UButton* Button = Find<UButton>(Name))
		{
			Button->SetIsEnabled(bEnabled);
			Button->SetRenderOpacity(bEnabled ? 1.f : 0.45f);
		}
	};
	Enable(TEXT("PrevButton"), Page > 0);
	Enable(TEXT("NextButton"), Page < GetPageCount() - 1);
}

void URobberShopCatalogue::OnPrev() { ShowPage(Page - 1); }
void URobberShopCatalogue::OnNext() { ShowPage(Page + 1); }

void URobberShopCatalogue::OnDone()
{
	if (ARobberController* Robber = GetRobber())
	{
		Robber->CloseShopCatalogue();
	}
}

FReply URobberShopCatalogue::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		OnDone();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}
