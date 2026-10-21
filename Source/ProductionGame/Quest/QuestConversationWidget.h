#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuestConversationWidget.generated.h"

class UPlayerQuestComponent;
class UTextBlock;
class UAudioComponent;
class UButton;
class UPanelWidget;
class UProgressBar;

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

/**
 * One row of the tracker (a job, a vote or a favour). Layout lives in WBP_QuestJobRow / VoteRow / FavourRow;
 * every bound widget is optional, so a row Blueprint only needs the parts it shows.
 */
UCLASS(Abstract, Blueprintable)
class PRODUCTIONGAME_API UQuestTrackerRow : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetRow(const FText& Title, const FText& Detail, const FText& Count, float Fraction, bool bDone);

	/** Title colour once the row is done; the designer colour is used otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Tracker") FSlateColor DoneTitleColor = FLinearColor(0.58f, 0.6f, 0.64f);
	/** Progress fill colour once the row is done; the designer colour is used otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Tracker") FLinearColor DoneFillColor = FLinearColor(0.38f, 0.85f, 0.42f);
protected:
	/** Hook for extra Blueprint styling after C++ has filled the row. */
	UFUNCTION(BlueprintImplementableEvent, Category="Quest Tracker") void OnRowSet(bool bDone);

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DetailText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CountText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> Progress;
	/** Shown only when done (the job tick, the "return to the quest giver" line). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> DoneMark;
private:
	bool bCapturedColours = false;
	FSlateColor TitleColor;
	FLinearColor FillColor;
};

/**
 * Tracker with a separate modal dialogue layer above the existing game HUD. Layout lives in WBP_QuestTracker;
 * C++ fills the named widgets and adds rows to the lists.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API UQuestConversationWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	FText GetDisplayedQuestText() const;

	UPROPERTY(EditAnywhere, Category="Quest Tracker") TSubclassOf<UQuestTrackerRow> JobRowClass;
	UPROPERTY(EditAnywhere, Category="Quest Tracker") TSubclassOf<UQuestTrackerRow> VoteRowClass;
	UPROPERTY(EditAnywhere, Category="Quest Tracker") TSubclassOf<UQuestTrackerRow> FavourRowClass;
	/** Gap above every row but the first in a list. */
	UPROPERTY(EditAnywhere, Category="Quest Tracker") float RowSpacing = 6.f;
	UPROPERTY(EditAnywhere, Category="Quest Tracker") float FavourSpacing = 14.f;
	/** Timer colour for the last few seconds. */
	UPROPERTY(EditAnywhere, Category="Quest Tracker") FSlateColor TimerWarningColor = FLinearColor(1.f, 0.38f, 0.32f);
	UPROPERTY(EditAnywhere, Category="Quest Tracker") int32 TimerWarningSeconds = 10;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Whole sections are collapsed when they have nothing to show.
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> HeistCard;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> PhaseText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> MessageBox;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> TimerBox;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TimerText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ObjectiveText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> VotesBox;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UPanelWidget> VotesList;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> JobsBox;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UPanelWidget> JobsList;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> FavourCard;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UPanelWidget> FavourList;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> FeedbackText;
private:
	UPROPERTY(Transient) TObjectPtr<UPlayerQuestComponent> Player;
	/** Plain-text copy of the tracker; widgets are only touched when it changes. */
	FString DisplayedText;
	bool bTimerColourCaptured = false;
	FSlateColor TimerColor;
	UQuestTrackerRow* RowAt(UPanelWidget* List, TSubclassOf<UQuestTrackerRow> RowClass, float Spacing, int32 Index);
	UPROPERTY(Transient) TObjectPtr<UDialoguePresentationWidget> Dialogue;
	UFUNCTION() void RefreshQuests();
	UFUNCTION() void RefreshConversation();
	UFUNCTION() void ShowRejection(FText Reason);
};
