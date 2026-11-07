#pragma once

#include "CoreMinimal.h"
#include "Heist/RobberCaseFileUI.h"
#include "RobberMenu/RobberParty.h"
#include "RobberMenuWidgets.generated.h"

class UButton;
class UCheckBox;
class UImage;
class UPanelWidget;
class UProgressBar;
class USizeBox;
class USlider;
class UWidgetSwitcher;
class URobberSettingsSubsystem;
struct FAchievementDefinition;
struct FAchievementProgressState;

/**
 * Shared by the Case File menus. As with the HUD, layout and look live in the Widget Blueprints (made by
 * -run=RobberMenuWidgets, then edited by hand) and this finds widgets by name; a widget that's missing is skipped.
 * Every menu has BackButton and closes on Esc.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberMenuWidget : public URobberCaseFileWidgetBase
{
	GENERATED_BODY()

public:
	/** Back returns to this menu rather than to the game. */
	void SetReturnTo(URobberMenuWidget* Menu) { ReturnTo = Menu; }

	UFUNCTION(BlueprintCallable, Category = "Robber|Menu")
	virtual void CloseMenu();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	URobberPartySubsystem* GetParty() const;
	URobberSettingsSubsystem* GetSettingsSystem() const;
	void FocusThisMenu();

	UFUNCTION()
	void OnBackClicked();

	TWeakObjectPtr<URobberMenuWidget> ReturnTo;
};

class URobberPlayMenu;

/**
 * Logic for WBP_RobberFriendRow. Fills AvatarImage, InitialsText, NameText, StatusText, JoinButton + JoinText,
 * InviteButton, TagPanel + TagText.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberFriendRow : public URobberCaseFileWidgetBase
{
	GENERATED_BODY()

public:
	void SetFriend(URobberPlayMenu* InMenu, const FRobberFriendInfo& InFriend);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor DisabledColour = FLinearColor(FColor(0xe9, 0xec, 0xef));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor InPartyColour = FLinearColor(FColor(0x2b, 0x8a, 0x3e));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor InvitedColour = FLinearColor(FColor(0xc0, 0x39, 0x2b));

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void OnJoinClicked();

	UFUNCTION()
	void OnInviteClicked();

	TWeakObjectPtr<URobberPlayMenu> Menu;
	FRobberFriendInfo Friend;
	bool bJoining = false;
};

/**
 * Logic for WBP_RobberPlayMenu. Fills, by name:
 * NOTE    JobLineText, FriendsOnlyBox, LanBox, CreateButton + CreateText, CaseStamp, ErrorText.
 * PARTY   PartyCountText, LeaveButton + LeaveText, Card0-3 parts: CardColour, CardMark, CardName, CardNote, CardKick.
 * FRIENDS FriendCountText, FriendList (panel of RowClass), FriendsEmptyText,
 *         InviteBanner + InviteBannerText + InviteAcceptButton + InviteDeclineButton.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberPlayMenu : public URobberMenuWidget
{
	GENERATED_BODY()

public:
	/** Empty uses WBP_RobberFriendRow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	TSubclassOf<URobberFriendRow> RowClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor ReadyColour = FLinearColor(FColor(0xff, 0xd4, 0x3b));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor WaitingColour = FLinearColor(FColor(0xe9, 0xec, 0xef));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor OpenCardColour = FLinearColor(FColor(0xf1, 0xf3, 0xf5));

	bool WantsLan() const;
	bool WantsFriendsOnly() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void OnCreateClicked();

	UFUNCTION()
	void OnLeaveClicked();

	UFUNCTION()
	void OnKick1();

	UFUNCTION()
	void OnKick2();

	UFUNCTION()
	void OnKick3();

	UFUNCTION()
	void OnAcceptInvite();

	UFUNCTION()
	void OnDeclineInvite();

	void Kick(int32 Card);
	void Refresh();
	void RefreshParty();
	void RefreshFriends();

	/** Reused rows: a new UMG child has no size for its first frame, so recreating them flickers. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URobberFriendRow>> Rows;

	TArray<TWeakObjectPtr<APlayerState>> CardPlayers;
	float RefreshIn = 0.f;
	float FriendsIn = 0.f;
	bool bStarting = false;
};

/**
 * Logic for WBP_RobberSettings. Fills, by name:
 * TABS    TabSound + TabSoundSize, TabControls + TabControlsSize, TabSwitcher (0 audio + display, 1 keyboard + mouse).
 * AUDIO   {Music,Sfx,Voice}Slider, {..}Fill (progress bar), {..}Value, {..}Note.
 * DISPLAY ModeFullscreen, ModeWindowed, ModeBorderless, ResPrev, ResNext, ResText, ResNote.
 * KEYS    Key<ActionId> (button) + Key<ActionId>Text, KeysHint, ResetKeysButton.
 * MOUSE   SensSlider, SensFill, SensValue, SensNote, InvertBox, SmoothBox.
 * SAVE    ApplyButton, ApplyNote, SavedStamp, and the same again with a 2 on the second spread.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberSettingsMenu : public URobberMenuWidget
{
	GENERATED_BODY()

public:
	virtual void CloseMenu() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor SelectedColour = FLinearColor(FColor(0xff, 0xd4, 0x3b));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor IdleColour = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor NothingToSaveColour = FLinearColor(FColor(0xe9, 0xec, 0xef));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor ListeningTextColour = FLinearColor(FColor(0xc0, 0x39, 0x2b));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor HintTextColour = FLinearColor(FColor(0x49, 0x50, 0x57));

	UFUNCTION(BlueprintCallable, Category = "Robber|Menu")
	void ShowTab(int32 Index);

	/** Local: starts waiting for the new key, as a click on its cap does. */
	void StartListening(FName ActionId);
	FName GetListeningFor() const { return ListeningFor; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	UFUNCTION() void OnTabSound();
	UFUNCTION() void OnTabControls();
	UFUNCTION() void OnMusicChanged(float Value);
	UFUNCTION() void OnSfxChanged(float Value);
	UFUNCTION() void OnVoiceChanged(float Value);
	UFUNCTION() void OnSensChanged(float Value);
	UFUNCTION() void OnInvertChanged(bool bChecked);
	UFUNCTION() void OnSmoothChanged(bool bChecked);
	UFUNCTION() void OnFullscreen();
	UFUNCTION() void OnWindowed();
	UFUNCTION() void OnBorderless();
	UFUNCTION() void OnResPrev();
	UFUNCTION() void OnResNext();
	UFUNCTION() void OnApply();
	UFUNCTION() void OnResetKeys();
	UFUNCTION() void OnKeyMoveForward();
	UFUNCTION() void OnKeyMoveLeft();
	UFUNCTION() void OnKeyMoveBack();
	UFUNCTION() void OnKeyMoveRight();
	UFUNCTION() void OnKeyJump();
	UFUNCTION() void OnKeyInteract();
	UFUNCTION() void OnKeyUse();
	UFUNCTION() void OnKeyThrow();
	UFUNCTION() void OnKeyDrop();
	UFUNCTION() void OnKeyJournal();
	UFUNCTION() void OnKeyPause();
	UFUNCTION() void OnKeySprint();

	void LoadFromSettings();
	void RefreshAudio();
	void RefreshDisplay();
	void RefreshKeys();
	void RefreshMouse();
	void RefreshApply();
	void MarkChanged();
	void CaptureKey(const FKey& Key);
	void SetWindowMode(EWindowMode::Type Mode);
	void StepResolution(int32 Delta);

	TArray<FIntPoint> Resolutions;
	int32 ResolutionIndex = 0;
	FName ListeningFor = NAME_None;
	FText SwapNote;
	bool bChanged = false;
	bool bSaved = false;
	bool bLoading = false;
};

/**
 * Logic for WBP_RobberAchievementStory, a normal achievement as a newspaper story. Fills KickerText, TitleText,
 * BodyText, StoryImage, ColumnRule (hidden on the first column), ProgressRow + ProgressBar + ProgressText.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberAchievementStory : public URobberCaseFileWidgetBase
{
	GENERATED_BODY()

public:
	void SetStory(const FAchievementDefinition& Definition, const FAchievementProgressState& Progress, bool bFirstColumn);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor EarnedKickerColour = FLinearColor(FColor(0xc0, 0x39, 0x2b));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor InkColour = FLinearColor(FColor(0x1b, 0x1b, 0x1b));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor FadedColour = FLinearColor(FColor(0x49, 0x50, 0x57));
};

/**
 * Logic for WBP_RobberAchievementScrap, a secret achievement as a torn scrap. Fills ScrapPaper (image: tinted),
 * TitleLetters (wrap box of cut-out letters), BodyText, ScrapImage.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberAchievementScrap : public URobberCaseFileWidgetBase
{
	GENERATED_BODY()

public:
	void SetScrap(const FAchievementDefinition& Definition, const FAchievementProgressState& Progress, int32 Index);

	/** One per scrap, in turn: the torn edges. Set by -run=RobberMenuWidgets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	TArray<TObjectPtr<UTexture2D>> TornPapers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	TArray<FLinearColor> EarnedPaperColours = { FLinearColor(FColor(0xe8, 0xe0, 0xc9)), FLinearColor(FColor(0xd9, 0xe4, 0xef)), FLinearColor(FColor(0xef, 0xe0, 0xd3)) };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	FLinearColor LockedPaperColour = FLinearColor(FColor(0xac, 0xa7, 0xc4));
};

/**
 * Logic for WBP_RobberAchievements. Fills, by name:
 * TABS   TabAll, TabEarned, TabLocked, TabSecret, each with a {Name}Size; ViewSwitcher (0 paper, 1 secrets).
 * PAPER  ViewNameText, EarnedFill, EarnedCountText, StoryGrid (uniform grid of StoryClass).
 * SECRET HeadingLetters (panel of cut-out letters), SecretCountText, ScrapCanvas (canvas of ScrapClass).
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberAchievementsMenu : public URobberMenuWidget
{
	GENERATED_BODY()

public:
	/** Empty uses WBP_RobberAchievementStory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	TSubclassOf<URobberAchievementStory> StoryClass;

	/** Empty uses WBP_RobberAchievementScrap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	TSubclassOf<URobberAchievementScrap> ScrapClass;

	/** Tab order: all, earned, locked, secret. */
	UFUNCTION(BlueprintCallable, Category = "Robber|Menu")
	void ShowTab(int32 Index);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION() void OnTabAll();
	UFUNCTION() void OnTabEarned();
	UFUNCTION() void OnTabLocked();
	UFUNCTION() void OnTabSecret();

	void Refresh();

	int32 Tab = 0;
};

/**
 * Logic for WBP_RobberPauseMenu. Fills, by name: PauseSwitcher (0 menu, 1 quit check), ResumeButton, SettingsButton,
 * QuitButton, QuitBodyText, StayButton, ConfirmQuitButton + ConfirmQuitText.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberPauseMenu : public URobberMenuWidget
{
	GENERATED_BODY()

public:
	virtual void CloseMenu() override;

	/** Opens straight on "are you sure?" (the hideout's back door); Stay then closes the menu instead of going back to it. */
	void ShowQuitCheckOnly();

	/** Empty uses WBP_RobberSettings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case File")
	TSubclassOf<URobberMenuWidget> SettingsClass;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	bool bQuitCheckOnly = false;

	UFUNCTION() void OnResume();
	UFUNCTION() void OnSettings();
	UFUNCTION() void OnQuit();
	UFUNCTION() void OnStay();
	UFUNCTION() void OnConfirmQuit();
};

/** Makes cut-out ransom-note letters. Same text, same letters, so a page doesn't reshuffle when it refreshes. */
namespace RobberRansom
{
	void Fill(UPanelWidget* Panel, const FString& Text, int32 FontSize, float LetterHeight, UObject* Outer);
}
