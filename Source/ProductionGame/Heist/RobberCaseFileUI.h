#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Heist/RobberEntryPoint.h"
#include "RobberCaseFileUI.generated.h"

class ARobberController;
class ARobberGameState;
class APlayerState;
struct FRobberStoryLine;
class UBorder;
class UButton;
class UCanvasPanel;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class URobberJournalComponent;
struct FRobberJournalEntry;

/**
 * Shared by the Case File widgets: finds the Blueprint's widgets by name and the bits of game state they show.
 * The layout and look live in the Widget Blueprints (made by -run=CaseFileWidgets, then edited by hand); this only
 * fills them in. A widget the Blueprint doesn't have is simply skipped.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberCaseFileWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Heat colours by level: cool, warm, hot, alarm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	TArray<FLinearColor> HeatColours = { FLinearColor(FColor(0x74, 0xc0, 0xfc)), FLinearColor(FColor(0xfa, 0xb0, 0x05)), FLinearColor(FColor(0xff, 0x6b, 0x4a)), FLinearColor(FColor(0xd6, 0x33, 0x6c)) };

	/** Crew colours by PlayerId, the same order as the map pins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	TArray<FLinearColor> CrewColours = { FLinearColor(FColor(0xff, 0x87, 0x87)), FLinearColor(FColor(0x74, 0xc0, 0xfc)), FLinearColor(FColor(0x8c, 0xe9, 0x9a)), FLinearColor(FColor(0xd0, 0xbf, 0xff)) };

protected:
	ARobberController* GetRobber() const;
	const ARobberGameState* GetRobberState() const;
	URobberJournalComponent* GetJournal() const;

	template <typename T = UWidget>
	T* Find(const FString& Name) const { return Cast<T>(GetWidgetFromName(*Name)); }

	static void SetText(UTextBlock* Block, const FText& Text);
	static void Show(UWidget* Widget, bool bShown);

	/** 0 cool, 1 warm, 2 hot, 3 alarm. */
	static int32 HeatLevel(const ARobberGameState* State);
	FLinearColor HeatColour(int32 Level) const;
	FLinearColor CrewColour(const APlayerState* Player) const;
	static FText Headline(int32 Level);
	static FText Subline(int32 Level);
	static FText NightName(const ARobberGameState* State);
	static FText Clock(float Seconds);
	static TArray<APlayerState*> SortedCrew(const ARobberGameState* State);
	static FText CrewStatus(const ARobberGameState* State, const APlayerState* Player);
	static bool VaultStillShut(const ARobberGameState* State);
	FText PlayerName(const APlayerState* Player) const;
	/** Fills <RowName>0-3 with the crew in PlayerId order: the row, CrewColour, CrewName and CrewStatus widgets. */
	void FillCrew(const ARobberGameState* State, const TCHAR* RowName);
};

/** One quest marker on screen: the target's name and distance over a yellow diamond. */
USTRUCT()
struct FRobberPinUI
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UWidget> Root;
	UPROPERTY() TObjectPtr<UTextBlock> Name;
	UPROPERTY() TObjectPtr<UTextBlock> Detail;
	UPROPERTY() TObjectPtr<UWidget> Arrow;
};

/** One bullet on a story quest's pinned note. */
USTRUCT()
struct FRobberStoryRowUI
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UWidget> Row;
	UPROPERTY() TObjectPtr<UWidget> Bullet;
	UPROPERTY() TObjectPtr<UWidget> Tick;
	UPROPERTY() TObjectPtr<UTextBlock> Text;
	UPROPERTY() TObjectPtr<UWidget> Strike;
};

/** A pinned note: the quest's own, or the errand under it. */
USTRUCT()
struct FRobberStoryNoteUI
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UWidget> Root;
	UPROPERTY() TObjectPtr<UTextBlock> Title;
	UPROPERTY() TArray<FRobberStoryRowUI> Rows;
	UPROPERTY() TObjectPtr<UWidget> FooterPanel;
	UPROPERTY() TObjectPtr<UTextBlock> Footer;
};

/**
 * Logic for WBP_CaseFileHUD. Widgets it fills, by name (all optional):
 * NOTE   TitleText, NightText, ObjectiveText, JobRow0-5 + JobBox0-5 (border: tick colour) + JobText0-5,
 *        TimerText, CodeRow + CodeText.
 * PAPER  PaperPanel, DatelineText, HeatText, FuseBar (progress bar), HeadlineText, SublineText.
 * BELOW  GuardsStamp + GuardsText, ToastPanel + ToastText.
 * CREW   CrewCard0-3, CrewColour0-3 (border), CrewName0-3, CrewStatus0-3.
 * TAB    JournalNew + JournalNewText.
 * SPRINT SprintPanel (faded out while stamina's full), SprintBar (progress bar). Built in code at the bottom centre
 *        when the WBP doesn't have a SprintBar.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberCaseFileHUD : public URobberCaseFileWidgetBase
{
	GENERATED_BODY()

public:
	/** The headline the paper shows right now. */
	FText GetHeadline() const;
	/** The note's vault code line as this player knows it ("4 _ 0 _"), empty when there's no vault tonight. */
	FText GetCodeLine() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor TickColour = FLinearColor(FColor(0x51, 0xcf, 0x66));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor DoneTextColour = FLinearColor(FColor(0x86, 0x8e, 0x96));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor TextColour = FLinearColor(FColor(0x1b, 0x1b, 0x1b));

	/** Sprint bar fill, and the fill while winded (out of stamina, can't sprint yet). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor SprintColour = FLinearColor(FColor(0xff, 0xd4, 0x3b));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor WindedColour = FLinearColor(FColor(0xff, 0x6b, 0x4a));

	/** Drawn sizes of the two big corner pieces. The toast under the paper keeps its own size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File|Sizes")
	float NoteScale = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File|Sizes")
	float PaperScale = 0.65f;

	/** Gap between the note and paper and the screen edges. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File|Sizes")
	float CornerMargin = 27.f;

	/** The story quest's pinned notes and stamp, on top of the sizes the mock-up used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File|Sizes")
	float StoryScale = 1.f;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void ApplySizes();
	void UpdateSprint(float DeltaTime);
	void UpdateNote(const ARobberGameState* State);
	void UpdatePaper(const ARobberGameState* State);

	FRobberStoryRowUI MakeStoryRow(const FSlateFontInfo& Font);
	FRobberStoryNoteUI MakeStoryNote(const FLinearColor& Paper, int32 RowCount, float Width);
	void BuildStoryWidgets(UCanvasPanel* Root);
	void FillStoryNote(FRobberStoryNoteUI& Ui, const FText& Title, const TArray<FRobberStoryLine>& Lines, const FText& Footer);
	void UpdateStory(float DeltaTime);
	void OnStoryStamp(bool bComplete, const FText& Title, const FText& Subtitle);
	void BuildMarkerWidgets(UCanvasPanel* Root);
	void UpdateMarkers(const FGeometry& MyGeometry, float DeltaTime);
	void UpdateExhibitBubbles(const FGeometry& MyGeometry, float DeltaTime);
	void UpdateHidingView();

	UPROPERTY(Transient) TArray<FRobberPinUI> Pins;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> Bubbles;
	UPROPERTY(Transient) TObjectPtr<UWidget> HidingView;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HidingName;
	TArray<struct FRobberMarkerTarget> MarkerTargets;
	TArray<TWeakObjectPtr<AActor>> BubbleExhibits;
	float SinceMarkerScan = 1.f;
	float SinceBubbleScan = 1.f;

	UPROPERTY(Transient) FRobberStoryNoteUI StoryNoteUi;
	UPROPERTY(Transient) FRobberStoryNoteUI StoryErrandUi;
	UPROPERTY(Transient) TObjectPtr<UWidget> StoryColumn;
	UPROPERTY(Transient) TObjectPtr<UWidget> StampRoot;
	UPROPERTY(Transient) TObjectPtr<UBorder> StampEdge;
	UPROPERTY(Transient) TObjectPtr<UBorder> StampInnerEdge;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StampKick;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StampTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StampSubtitle;
	bool bStoryBound = false;
	bool bStampComplete = false;
	bool bStoryNoteUp = false;
	/** Seconds into each animation; below zero when it isn't running. */
	float StampClock = -1.f;
	float NotePopClock = -1.f;
	float NoteDropClock = -1.f;
};

/**
 * Logic for WBP_JournalEntry, one OVERHEARD entry. Fills SpeakerText, WhenText, QuoteText, ClueText, InitialsText,
 * NewStamp (shown for entries newer than the last time the journal was opened).
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberJournalEntryWidget : public URobberCaseFileWidgetBase
{
	GENERATED_BODY()

public:
	void SetEntry(const FRobberJournalEntry& Entry, bool bNew, float Now);
};

/**
 * Logic for WBP_Journal (J). Widgets it fills, by name (all optional):
 * LEFT   NightText, EntryList (panel; one EntryClass widget per exhibit, newest first), EmptyText, UnaskedText.
 * TABS   TabClues / TabCrew / TabNight (buttons), TabSwitcher (widget switcher: 0 clues, 1 crew, 2 night).
 * CLUES  VaultSection, Digit0-3 (text), DigitBox0-3 (border: known/unknown colour), DigitFrom0-3, KeysText.
 * CREW   CrewRow0-3, CrewColour0-3 (border), CrewName0-3, CrewStatus0-3.
 * NIGHT  JobsText, PaperLineText.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberJournalWidget : public URobberCaseFileWidgetBase
{
	GENERATED_BODY()

public:
	/** The player opened it: entries newer than what they'd read get NEW stamps, and now count as read. */
	void OnOpened();

	UFUNCTION(BlueprintCallable, Category = "Case File")
	void ShowTab(int32 Index);

	/** One OVERHEARD entry. Empty uses WBP_JournalEntry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	TSubclassOf<URobberJournalEntryWidget> EntryClass;

	/** Drawn size of the book. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File|Sizes")
	float BookScale = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor KnownDigitColour = FLinearColor(FColor(0xff, 0xf3, 0xbf));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor UnknownDigitColour = FLinearColor::White;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void OnCluesClicked();

	UFUNCTION()
	void OnCrewClicked();

	UFUNCTION()
	void OnNightClicked();

	void Refresh();

	/** Reused entry widgets (a new UMG child has no size for its first frame, so recreating them flickers). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URobberJournalEntryWidget>> EntryWidgets;

	int32 OpenedReadRevision = 0;
	float RefreshIn = 0.f;
};
