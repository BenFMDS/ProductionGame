#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuestConversationWidget.generated.h"

class UPlayerQuestComponent;
class UTextBlock;
class UAudioComponent;
class UButton;
class UVerticalBox;

/** Uses the original WBP_TextEntry artwork, with native RPC handling. */
UCLASS()
class PRODUCTIONGAME_API UConversationChoiceWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UConversationChoiceWidget(const FObjectInitializer& Initializer);
	FText Label;
	int32 ChoiceID = INDEX_NONE;
	int32 Revision = 0;
	TWeakObjectPtr<UPlayerQuestComponent> Player;
	UButton* GetChoiceButton() const { return Button; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
private:
	UPROPERTY() TSubclassOf<UUserWidget> LayoutClass;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> Layout;
	UPROPERTY(Transient) TObjectPtr<UButton> Button;
	UFUNCTION() void Clicked();
};

/** Modal using the original designer-authored TextDisplay/TextBox layout. */
UCLASS()
class PRODUCTIONGAME_API UDialoguePresentationWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UDialoguePresentationWidget(const FObjectInitializer& Initializer);
	void RefreshConversation();
	void ShowChoices();
	bool IsShowingChoices() const { return bShowingChoices; }
	const TArray<TObjectPtr<UConversationChoiceWidget>>& GetChoiceWidgets() const { return ChoiceWidgets; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	UPROPERTY() TSubclassOf<UUserWidget> LayoutClass;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> Layout;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> TextBox;
	UPROPERTY(Transient) TObjectPtr<UPlayerQuestComponent> Player;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Voice;
	UPROPERTY(Transient) TArray<TObjectPtr<UConversationChoiceWidget>> ChoiceWidgets;
	bool bShowingChoices = false;
	bool bPreviousCursor = false;
	int32 DisplayedRevision = INDEX_NONE;
	void SetText(FName Name, const FText& Text);
	void SetBoxVisibility(FName Name, ESlateVisibility NewVisibility);
};

/** Tracker with a separate modal dialogue layer above the existing game HUD. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API UQuestConversationWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	FText GetDisplayedQuestText() const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
	UPROPERTY(Transient) TObjectPtr<UPlayerQuestComponent> Player;
	UPROPERTY(Transient) TObjectPtr<UWidget> HeistCard;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PhaseTag;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> HeistBody;
	UPROPERTY(Transient) TObjectPtr<UWidget> FavourCard;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> FavourBody;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FeedbackText;
	/** Plain-text copy of the tracker; rows are only rebuilt when it changes. */
	FString DisplayedText;
	UWidget* MakeCard(UVerticalBox* Tracker, const FString& Title, const FLinearColor& Accent, TObjectPtr<UVerticalBox>& OutBody, TObjectPtr<UTextBlock>* OutTag = nullptr);
	UPROPERTY(Transient) TObjectPtr<UDialoguePresentationWidget> Dialogue;
	UFUNCTION() void RefreshQuests();
	UFUNCTION() void RefreshConversation();
	UFUNCTION() void ShowRejection(FText Reason);
};
