#include "RobberMenu/RobberMenuWidgets.h"

#include "AchievementSubsystem.h"
#include "AchievementTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberStage.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Robber/RobberAchievementTracker.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "RobberMenu/RobberSettings.h"

namespace RobberMenuDetail
{
	const TCHAR* MenusRoot = TEXT("/Game/OurStuff/UI/CaseFile/Menus/");

	template <typename T>
	TSubclassOf<T> LoadMenuClass(const TCHAR* Name)
	{
		return LoadClass<T>(nullptr, *FString::Printf(TEXT("%s%s.%s_C"), MenusRoot, Name, Name));
	}

	void SetButtonColour(UButton* Button, const FLinearColor& Colour)
	{
		if (Button)
		{
			Button->SetBackgroundColor(Colour);
		}
	}

	void SetTextColour(UTextBlock* Block, const FLinearColor& Colour)
	{
		if (Block)
		{
			Block->SetColorAndOpacity(FSlateColor(Colour));
		}
	}

	template <typename T>
	T* SubsystemOf(const UUserWidget* Widget)
	{
		const UGameInstance* GameInstance = Widget ? Widget->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<T>() : nullptr;
	}

	FText MenuPercent(float Value)
	{
		return FText::AsNumber(FMath::RoundToInt(Value * 100.f));
	}

	FText AudioNote(float Value, const FText& Quiet, const FText& Normal, const FText& Loud)
	{
		return Value <= 0.f ? INVTEXT("Off.") : Value < 0.35f ? Quiet : Value < 0.85f ? Normal : Loud;
	}

	TArray<FAchievementDefinition> GetDefinitions(const UUserWidget* Widget, bool bSecret)
	{
		TArray<FAchievementDefinition> Result;
		if (const UAchievementSubsystem* Achievements = SubsystemOf<UAchievementSubsystem>(Widget))
		{
			for (const FAchievementDefinition& Definition : Achievements->GetAchievementDefinitions())
			{
				if (Definition.bSecret == bSecret)
				{
					Result.Add(Definition);
				}
			}
		}
		return Result;
	}

	UTexture2D* ImageFor(const FAchievementDefinition& Definition, bool bUnlocked)
	{
		UTexture2D* Image = bUnlocked ? Definition.UnlockedImage.LoadSynchronous() : Definition.LockedImage.LoadSynchronous();
		return Image ? Image : Definition.LockedImage.LoadSynchronous();
	}
}

// --- URobberMenuWidget ---

void URobberMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (UButton* Back = Find<UButton>(TEXT("BackButton")))
	{
		Back->OnClicked.AddUniqueDynamic(this, &URobberMenuWidget::OnBackClicked);
	}
}

void URobberMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	FocusThisMenu();
}

void URobberMenuWidget::FocusThisMenu()
{
	if (APlayerController* Controller = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(InputMode);
		Controller->bShowMouseCursor = true;
	}
}

void URobberMenuWidget::OnBackClicked()
{
	CloseMenu();
}

void URobberMenuWidget::CloseMenu()
{
	URobberMenuWidget* Parent = ReturnTo.Get();
	RemoveFromParent();
	if (Parent)
	{
		Parent->FocusThisMenu();
	}
	else if (APlayerController* Controller = GetOwningPlayer())
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->bShowMouseCursor = false;
	}
}

FReply URobberMenuWidget::NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(MyGeometry, InKeyEvent);
}

URobberPartySubsystem* URobberMenuWidget::GetParty() const
{
	return RobberMenuDetail::SubsystemOf<URobberPartySubsystem>(this);
}

URobberSettingsSubsystem* URobberMenuWidget::GetSettingsSystem() const
{
	return RobberMenuDetail::SubsystemOf<URobberSettingsSubsystem>(this);
}

// --- URobberFriendRow ---

void URobberFriendRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (UButton* Join = Find<UButton>(TEXT("JoinButton")))
	{
		Join->OnClicked.AddUniqueDynamic(this, &URobberFriendRow::OnJoinClicked);
	}
	if (UButton* Invite = Find<UButton>(TEXT("InviteButton")))
	{
		Invite->OnClicked.AddUniqueDynamic(this, &URobberFriendRow::OnInviteClicked);
	}
}

void URobberFriendRow::SetFriend(URobberPlayMenu* InMenu, const FRobberFriendInfo& InFriend)
{
	Menu = InMenu;
	Friend = InFriend;

	const URobberPartySubsystem* Party = RobberMenuDetail::SubsystemOf<URobberPartySubsystem>(this);
	if (!Party)
	{
		return;
	}

	SetText(Find<UTextBlock>(TEXT("NameText")), FText::FromString(Friend.Name));
	// First letters of the first two words, or the first two letters of a one-word name.
	TArray<FString> NameWords;
	Friend.Name.ParseIntoArrayWS(NameWords);
	const FString Initials = NameWords.Num() >= 2 ? NameWords[0].Left(1) + NameWords[1].Left(1) : Friend.Name.Left(2);
	SetText(Find<UTextBlock>(TEXT("InitialsText")), FText::FromString(Initials.ToUpper()));
	const FText Status = Friend.bJoinable ? NSLOCTEXT("RobberMenu", "FriendJoinable", "In a game")
		: Friend.bPlayingThisGame ? NSLOCTEXT("RobberMenu", "FriendPlaying", "Playing")
		: Friend.bOnline ? NSLOCTEXT("RobberMenu", "FriendOnline", "Online")
		: NSLOCTEXT("RobberMenu", "FriendOffline", "Offline");
	SetText(Find<UTextBlock>(TEXT("StatusText")), Status);

	if (UImage* Avatar = Find<UImage>(TEXT("AvatarImage")))
	{
		UTexture2D* Texture = URobberPartySubsystem::GetAvatar(Friend);
		if (Texture)
		{
			Avatar->SetBrushFromTexture(Texture);
		}
		Show(Avatar, Texture != nullptr);
	}

	const TArray<FRobberPartyMember> Members = Party->GetMembers();
	const bool bHere = Members.ContainsByPredicate([this](const FRobberPartyMember& Member) { return Member.Name == Friend.Name; });
	const bool bInvited = Party->IsInvited(Friend.Name);
	const bool bFull = Members.Num() + Party->GetPendingInvites().Num() >= URobberPartySubsystem::MaxPlayers;

	// Joining someone's game would leave the party behind, so it's for solo players only.
	const bool bCanJoin = Friend.bJoinable && !Party->IsInParty();
	// The size box, not the button: it also holds the button's hard shadow.
	Show(Find(TEXT("JoinSize")), bCanJoin);
	SetText(Find<UTextBlock>(TEXT("JoinText")), bJoining ? NSLOCTEXT("RobberMenu", "Joining", "JOINING") : NSLOCTEXT("RobberMenu", "Join", "JOIN"));

	const bool bCanInvite = Friend.bOnline && !bCanJoin && !bHere && !bInvited && !Party->IsMember();
	UButton* Invite = Find<UButton>(TEXT("InviteButton"));
	Show(Find(TEXT("InviteSize")), bCanInvite);
	if (Invite)
	{
		Invite->SetIsEnabled(!bFull);
		Invite->SetBackgroundColor(bFull ? DisabledColour : FLinearColor::White);
	}

	UWidget* Tag = Find(TEXT("TagPanel"));
	Show(Tag, bHere || bInvited);
	if (UTextBlock* TagText = Find<UTextBlock>(TEXT("TagText")))
	{
		TagText->SetText(bHere ? NSLOCTEXT("RobberMenu", "TagInParty", "IN PARTY") : NSLOCTEXT("RobberMenu", "TagInvited", "INVITED"));
		TagText->SetColorAndOpacity(FSlateColor(bHere ? InPartyColour : InvitedColour));
	}
}

void URobberFriendRow::OnJoinClicked()
{
	if (URobberPartySubsystem* Party = RobberMenuDetail::SubsystemOf<URobberPartySubsystem>(this))
	{
		bJoining = true;
		SetText(Find<UTextBlock>(TEXT("JoinText")), NSLOCTEXT("RobberMenu", "Joining", "JOINING"));
		Party->JoinFriend(Friend);
	}
}

void URobberFriendRow::OnInviteClicked()
{
	URobberPartySubsystem* Party = RobberMenuDetail::SubsystemOf<URobberPartySubsystem>(this);
	const URobberPlayMenu* PlayMenu = Menu.Get();
	if (Party && PlayMenu)
	{
		Party->InviteFriend(Friend, PlayMenu->WantsLan(), PlayMenu->WantsFriendsOnly());
	}
}

// --- URobberPlayMenu ---

void URobberPlayMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!RowClass)
	{
		RowClass = RobberMenuDetail::LoadMenuClass<URobberFriendRow>(TEXT("WBP_RobberFriendRow"));
	}
	if (UButton* Create = Find<UButton>(TEXT("CreateButton"))) Create->OnClicked.AddUniqueDynamic(this, &URobberPlayMenu::OnCreateClicked);
	if (UButton* Leave = Find<UButton>(TEXT("LeaveButton"))) Leave->OnClicked.AddUniqueDynamic(this, &URobberPlayMenu::OnLeaveClicked);
	if (UButton* Kick1 = Find<UButton>(TEXT("Card1Kick"))) Kick1->OnClicked.AddUniqueDynamic(this, &URobberPlayMenu::OnKick1);
	if (UButton* Kick2 = Find<UButton>(TEXT("Card2Kick"))) Kick2->OnClicked.AddUniqueDynamic(this, &URobberPlayMenu::OnKick2);
	if (UButton* Kick3 = Find<UButton>(TEXT("Card3Kick"))) Kick3->OnClicked.AddUniqueDynamic(this, &URobberPlayMenu::OnKick3);
	if (UButton* Accept = Find<UButton>(TEXT("InviteAcceptButton"))) Accept->OnClicked.AddUniqueDynamic(this, &URobberPlayMenu::OnAcceptInvite);
	if (UButton* Decline = Find<UButton>(TEXT("InviteDeclineButton"))) Decline->OnClicked.AddUniqueDynamic(this, &URobberPlayMenu::OnDeclineInvite);
}

void URobberPlayMenu::NativeConstruct()
{
	Super::NativeConstruct();
	FriendsIn = 0.f;
	if (URobberPartySubsystem* Party = GetParty())
	{
		Party->RefreshFriends();
	}
	Refresh();
}

void URobberPlayMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	FriendsIn -= InDeltaTime;
	if (FriendsIn <= 0.f)
	{
		FriendsIn = 10.f;
		if (URobberPartySubsystem* Party = GetParty())
		{
			Party->RefreshFriends();
		}
	}
	RefreshIn -= InDeltaTime;
	if (RefreshIn <= 0.f)
	{
		RefreshIn = 0.25f;
		Refresh();
	}
}

bool URobberPlayMenu::WantsLan() const
{
	const UCheckBox* Box = Find<UCheckBox>(TEXT("LanBox"));
	return Box && Box->IsChecked();
}

bool URobberPlayMenu::WantsFriendsOnly() const
{
	const UCheckBox* Box = Find<UCheckBox>(TEXT("FriendsOnlyBox"));
	return !Box || Box->IsChecked();
}

void URobberPlayMenu::Refresh()
{
	RefreshParty();
	RefreshFriends();
}

void URobberPlayMenu::RefreshParty()
{
	const URobberPartySubsystem* Party = GetParty();
	if (!Party)
	{
		return;
	}
	using namespace RobberMenuDetail;

	const TArray<FRobberPartyMember> Members = Party->GetMembers();
	const TArray<FString> Invited = Party->GetPendingInvites();
	const bool bInParty = Party->IsInParty();
	const bool bMember = Party->IsMember();

	SetText(Find<UTextBlock>(TEXT("JobLineText")), bMember
		? FText::Format(NSLOCTEXT("RobberMenu", "JobLineMember", "{0} is running this job. Sit tight."), FText::FromString(Party->GetLeaderName()))
		: NSLOCTEXT("RobberMenu", "JobLine", "Up to 4 robbers. 7 nights. One very chatty museum."));
	SetText(Find<UTextBlock>(TEXT("PartyCountText")), FText::Format(NSLOCTEXT("RobberMenu", "PartyCount", "YOUR PARTY · {0} OF {1}"), Members.Num(), URobberPartySubsystem::MaxPlayers));

	const bool bCanLeave = bInParty;
	Show(Find(TEXT("LeaveButton")), bCanLeave);
	SetText(Find<UTextBlock>(TEXT("LeaveText")), bMember ? NSLOCTEXT("RobberMenu", "LeaveParty", "Leave party") : NSLOCTEXT("RobberMenu", "BreakUp", "Break up party"));

	CardPlayers.Reset();
	CardPlayers.SetNum(URobberPartySubsystem::MaxPlayers);
	for (int32 Card = 0; Card < URobberPartySubsystem::MaxPlayers; ++Card)
	{
		const FString Prefix = FString::Printf(TEXT("Card%d"), Card);
		FText Name = NSLOCTEXT("RobberMenu", "CardOpen", "Open");
		FText Note = NSLOCTEXT("RobberMenu", "CardInviteHint", "Nobody yet");
		FText Mark = INVTEXT("?");
		FLinearColor Colour = OpenCardColour;
		bool bCanKick = false;
		if (Members.IsValidIndex(Card))
		{
			const FRobberPartyMember& Member = Members[Card];
			Name = Member.bLocal ? NSLOCTEXT("RobberMenu", "CardYou", "You") : FText::FromString(Member.Name);
			Note = Member.bLeader ? NSLOCTEXT("RobberMenu", "CardLeader", "Leader") : NSLOCTEXT("RobberMenu", "CardReady", "Ready");
			Mark = FText::GetEmpty();
			Colour = CrewColours.IsValidIndex(Card) ? CrewColours[Card] : FLinearColor::White;
			bCanKick = !bMember && !Member.bLocal;
			CardPlayers[Card] = Member.Player;
		}
		else if (Invited.IsValidIndex(Card - Members.Num()))
		{
			Name = FText::FromString(Invited[Card - Members.Num()]);
			Note = NSLOCTEXT("RobberMenu", "CardInvited", "Invited...");
			Mark = INVTEXT("...");
		}
		SetText(Find<UTextBlock>(Prefix + TEXT("Name")), Name);
		SetText(Find<UTextBlock>(Prefix + TEXT("Note")), Note);
		SetText(Find<UTextBlock>(Prefix + TEXT("Mark")), Mark);
		if (UBorder* Photo = Find<UBorder>(Prefix + TEXT("Colour")))
		{
			Photo->SetBrushColor(Colour);
		}
		Show(Find(Prefix + TEXT("Kick")), bCanKick);
	}

	// Session options can't change once the party's session exists.
	if (UWidget* Box = Find(TEXT("FriendsOnlyBox"))) Box->SetIsEnabled(!bInParty);
	if (UWidget* Box = Find(TEXT("LanBox"))) Box->SetIsEnabled(!bInParty);

	UButton* Create = Find<UButton>(TEXT("CreateButton"));
	if (Create)
	{
		Create->SetIsEnabled(!bMember && !bStarting);
		SetButtonColour(Create, bMember ? WaitingColour : ReadyColour);
	}
	FText CreateWords = NSLOCTEXT("RobberMenu", "CreateGame", "CREATE GAME");
	if (bMember)
	{
		// Not the leader's name: player names can be far wider than the button.
		CreateWords = NSLOCTEXT("RobberMenu", "Waiting", "WAITING FOR THE LEADER");
	}
	else if (bStarting)
	{
		CreateWords = NSLOCTEXT("RobberMenu", "Creating", "CREATING GAME...");
	}
	else if (Members.Num() > 1)
	{
		CreateWords = FText::Format(NSLOCTEXT("RobberMenu", "CreateFor", "CREATE GAME FOR {0}"), Members.Num());
	}
	SetText(Find<UTextBlock>(TEXT("CreateText")), CreateWords);
	Show(Find(TEXT("CaseStamp")), bStarting);

	const FText Error = Party->GetLastError();
	if (!Error.IsEmpty())
	{
		bStarting = false;
	}
	SetText(Find<UTextBlock>(TEXT("ErrorText")), Error);
	Show(Find(TEXT("ErrorText")), !Error.IsEmpty());

	Show(Find(TEXT("InviteBanner")), Party->HasIncomingInvite());
	SetText(Find<UTextBlock>(TEXT("InviteBannerText")), FText::Format(NSLOCTEXT("RobberMenu", "InviteBanner", "{0} wants you in their party"), FText::FromString(Party->GetIncomingInviteName())));
}

void URobberPlayMenu::RefreshFriends()
{
	URobberPartySubsystem* Party = GetParty();
	UPanelWidget* List = Find<UPanelWidget>(TEXT("FriendList"));
	if (!Party || !List || !RowClass)
	{
		return;
	}
	const TArray<FRobberFriendInfo>& Friends = Party->GetFriends();
	int32 Online = 0;
	for (int32 Index = 0; Index < Friends.Num(); ++Index)
	{
		Online += Friends[Index].bOnline;
		if (!Rows.IsValidIndex(Index))
		{
			URobberFriendRow* Row = CreateWidget<URobberFriendRow>(this, RowClass);
			List->AddChild(Row);
			Rows.Add(Row);
		}
		Rows[Index]->SetFriend(this, Friends[Index]);
		Rows[Index]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	for (int32 Index = Friends.Num(); Index < Rows.Num(); ++Index)
	{
		Rows[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetText(Find<UTextBlock>(TEXT("FriendCountText")), FText::Format(NSLOCTEXT("RobberMenu", "AtLarge", "{0} at large"), Online));
	Show(Find(TEXT("FriendsEmptyText")), Friends.Num() == 0);
	SetText(Find<UTextBlock>(TEXT("FriendsEmptyText")), Party->HasFriendsService()
		? NSLOCTEXT("RobberMenu", "NoFriends", "Nobody on your list yet.")
		: NSLOCTEXT("RobberMenu", "NoService", "Your friends list shows up here when Steam is running."));
}

void URobberPlayMenu::OnCreateClicked()
{
	if (URobberPartySubsystem* Party = GetParty())
	{
		bStarting = true;
		Party->CreateGame(WantsLan(), WantsFriendsOnly());
		RefreshParty();
	}
}

void URobberPlayMenu::OnLeaveClicked()
{
	if (URobberPartySubsystem* Party = GetParty())
	{
		Party->LeaveParty();
	}
}

void URobberPlayMenu::OnKick1() { Kick(1); }
void URobberPlayMenu::OnKick2() { Kick(2); }
void URobberPlayMenu::OnKick3() { Kick(3); }

void URobberPlayMenu::Kick(int32 Card)
{
	URobberPartySubsystem* Party = GetParty();
	if (Party && CardPlayers.IsValidIndex(Card) && CardPlayers[Card].IsValid())
	{
		Party->KickMember(CardPlayers[Card].Get());
	}
}

void URobberPlayMenu::OnAcceptInvite()
{
	if (URobberPartySubsystem* Party = GetParty())
	{
		Party->AcceptIncomingInvite();
	}
}

void URobberPlayMenu::OnDeclineInvite()
{
	if (URobberPartySubsystem* Party = GetParty())
	{
		Party->DeclineIncomingInvite();
	}
}

// --- URobberSettingsMenu ---

void URobberSettingsMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (UButton* Button = Find<UButton>(TEXT("TabSound"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnTabSound);
	if (UButton* Button = Find<UButton>(TEXT("TabControls"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnTabControls);
	if (USlider* Slider = Find<USlider>(TEXT("MusicSlider"))) Slider->OnValueChanged.AddUniqueDynamic(this, &URobberSettingsMenu::OnMusicChanged);
	if (USlider* Slider = Find<USlider>(TEXT("SfxSlider"))) Slider->OnValueChanged.AddUniqueDynamic(this, &URobberSettingsMenu::OnSfxChanged);
	if (USlider* Slider = Find<USlider>(TEXT("VoiceSlider"))) Slider->OnValueChanged.AddUniqueDynamic(this, &URobberSettingsMenu::OnVoiceChanged);
	if (USlider* Slider = Find<USlider>(TEXT("SensSlider"))) Slider->OnValueChanged.AddUniqueDynamic(this, &URobberSettingsMenu::OnSensChanged);
	if (UCheckBox* Box = Find<UCheckBox>(TEXT("InvertBox"))) Box->OnCheckStateChanged.AddUniqueDynamic(this, &URobberSettingsMenu::OnInvertChanged);
	if (UCheckBox* Box = Find<UCheckBox>(TEXT("SmoothBox"))) Box->OnCheckStateChanged.AddUniqueDynamic(this, &URobberSettingsMenu::OnSmoothChanged);
	if (UButton* Button = Find<UButton>(TEXT("ModeFullscreen"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnFullscreen);
	if (UButton* Button = Find<UButton>(TEXT("ModeWindowed"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnWindowed);
	if (UButton* Button = Find<UButton>(TEXT("ModeBorderless"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnBorderless);
	if (UButton* Button = Find<UButton>(TEXT("ResPrev"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnResPrev);
	if (UButton* Button = Find<UButton>(TEXT("ResNext"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnResNext);
	if (UButton* Button = Find<UButton>(TEXT("ApplyButton"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnApply);
	if (UButton* Button = Find<UButton>(TEXT("ApplyButton2"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnApply);
	if (UButton* Button = Find<UButton>(TEXT("ResetKeysButton"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnResetKeys);
	if (UButton* Button = Find<UButton>(TEXT("KeyMoveForward"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnKeyMoveForward);
	if (UButton* Button = Find<UButton>(TEXT("KeyMoveLeft"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnKeyMoveLeft);
	if (UButton* Button = Find<UButton>(TEXT("KeyMoveBack"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnKeyMoveBack);
	if (UButton* Button = Find<UButton>(TEXT("KeyMoveRight"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnKeyMoveRight);
	if (UButton* Button = Find<UButton>(TEXT("KeyJump"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnKeyJump);
	if (UButton* Button = Find<UButton>(TEXT("KeyInteract"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnKeyInteract);
	if (UButton* Button = Find<UButton>(TEXT("KeyUse"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnKeyUse);
	if (UButton* Button = Find<UButton>(TEXT("KeyThrow"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnKeyThrow);
	if (UButton* Button = Find<UButton>(TEXT("KeyDrop"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnKeyDrop);
	if (UButton* Button = Find<UButton>(TEXT("KeyJournal"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnKeyJournal);
	if (UButton* Button = Find<UButton>(TEXT("KeyPause"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnKeyPause);
	if (UButton* Button = Find<UButton>(TEXT("KeySprint"))) Button->OnClicked.AddUniqueDynamic(this, &URobberSettingsMenu::OnKeySprint);
	if (USlider* Slider = Find<USlider>(TEXT("UiSizeSlider")))
	{
		Slider->OnValueChanged.AddUniqueDynamic(this, &URobberSettingsMenu::OnUiSizeChanged);
		Slider->OnMouseCaptureEnd.AddUniqueDynamic(this, &URobberSettingsMenu::OnUiSizeReleased);
	}

	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	if (Resolutions.Num() == 0)
	{
		// Nothing reported (a window under test, say): the usual sizes.
		Resolutions = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160) };
	}
}

void URobberSettingsMenu::NativeConstruct()
{
	Super::NativeConstruct();
	bChanged = false;
	bSaved = false;
	LoadFromSettings();
	ShowTab(0);
}

void URobberSettingsMenu::CloseMenu()
{
	// Back without Apply drops the changes, which were only being previewed.
	if (bChanged)
	{
		if (URobberSettingsSubsystem* System = GetSettingsSystem())
		{
			System->Revert();
		}
		if (UGameUserSettings* Display = UGameUserSettings::GetGameUserSettings())
		{
			Display->LoadSettings(true);
		}
	}
	Super::CloseMenu();
}

void URobberSettingsMenu::ShowTab(int32 Index)
{
	ListeningFor = NAME_None;
	if (UWidgetSwitcher* Switcher = Find<UWidgetSwitcher>(TEXT("TabSwitcher")))
	{
		Switcher->SetActiveWidgetIndex(Index);
	}
	// The open tab stands taller, like the journal's.
	if (USizeBox* Size = Find<USizeBox>(TEXT("TabSoundSize"))) Size->SetHeightOverride(Index == 0 ? 75.f : 60.f);
	if (USizeBox* Size = Find<USizeBox>(TEXT("TabControlsSize"))) Size->SetHeightOverride(Index == 1 ? 75.f : 60.f);
	RefreshKeys();
}

void URobberSettingsMenu::OnTabSound() { ShowTab(0); }
void URobberSettingsMenu::OnTabControls() { ShowTab(1); }

void URobberSettingsMenu::LoadFromSettings()
{
	const URobberSettingsSubsystem* System = GetSettingsSystem();
	const URobberSettingsSave* Settings = System ? System->GetSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	// Setting a slider fires its changed event, which isn't the player changing anything.
	TGuardValue<bool> Loading(bLoading, true);
	const float Music = Settings->MusicVolume, Sfx = Settings->SfxVolume, Voice = Settings->VoiceVolume, Sens = Settings->LookSensitivity;
	if (USlider* Slider = Find<USlider>(TEXT("MusicSlider"))) Slider->SetValue(Music);
	if (USlider* Slider = Find<USlider>(TEXT("SfxSlider"))) Slider->SetValue(Sfx);
	if (USlider* Slider = Find<USlider>(TEXT("VoiceSlider"))) Slider->SetValue(Voice);
	if (USlider* Slider = Find<USlider>(TEXT("SensSlider"))) Slider->SetValue(Sens);
	if (USlider* Slider = Find<USlider>(TEXT("UiSizeSlider"))) Slider->SetValue(Settings->UiSize);
	RefreshUiSize();
	if (UCheckBox* Box = Find<UCheckBox>(TEXT("InvertBox"))) Box->SetIsChecked(Settings->bInvertLook);
	if (UCheckBox* Box = Find<UCheckBox>(TEXT("SmoothBox"))) Box->SetIsChecked(Settings->bSmoothMouse);

	if (const UGameUserSettings* Display = UGameUserSettings::GetGameUserSettings())
	{
		const FIntPoint Current = Display->GetScreenResolution();
		ResolutionIndex = Resolutions.IndexOfByKey(Current);
		if (ResolutionIndex == INDEX_NONE)
		{
			Resolutions.AddUnique(Current);
			ResolutionIndex = Resolutions.IndexOfByKey(Current);
		}
	}
	RefreshAudio();
	RefreshDisplay();
	RefreshKeys();
	RefreshMouse();
	RefreshApply();
}

void URobberSettingsMenu::RefreshAudio()
{
	const URobberSettingsSubsystem* System = GetSettingsSystem();
	const URobberSettingsSave* Settings = System ? System->GetSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	using namespace RobberMenuDetail;
	const auto Row = [this](const FString& Name, float Value, const FText& Note)
	{
		if (UProgressBar* Fill = Find<UProgressBar>(Name + TEXT("Fill"))) Fill->SetPercent(Value);
		SetText(Find<UTextBlock>(Name + TEXT("Value")), MenuPercent(Value));
		SetText(Find<UTextBlock>(Name + TEXT("Note")), Note);
	};
	Row(TEXT("Music"), Settings->MusicVolume, AudioNote(Settings->MusicVolume,
		NSLOCTEXT("RobberMenu", "MusicLow", "A distant hum."), NSLOCTEXT("RobberMenu", "MusicMid", "Sneaky, but not too sneaky."), NSLOCTEXT("RobberMenu", "MusicHigh", "The guards can hear it too.")));
	Row(TEXT("Sfx"), Settings->SfxVolume, AudioNote(Settings->SfxVolume,
		NSLOCTEXT("RobberMenu", "SfxLow", "Lasers go \"fzz\"."), NSLOCTEXT("RobberMenu", "SfxMid", "Alarms at a sensible volume."), NSLOCTEXT("RobberMenu", "SfxHigh", "Every footstep is a confession.")));
	Row(TEXT("Voice"), Settings->VoiceVolume, AudioNote(Settings->VoiceVolume,
		NSLOCTEXT("RobberMenu", "VoiceLow", "The exhibits are muttering."), NSLOCTEXT("RobberMenu", "VoiceMid", "The exhibits are chatting."), NSLOCTEXT("RobberMenu", "VoiceHigh", "The Mona Lisa will not stop talking.")));
}

void URobberSettingsMenu::RefreshDisplay()
{
	const UGameUserSettings* Display = UGameUserSettings::GetGameUserSettings();
	if (!Display)
	{
		return;
	}
	using namespace RobberMenuDetail;
	const EWindowMode::Type Mode = Display->GetFullscreenMode();
	SetButtonColour(Find<UButton>(TEXT("ModeFullscreen")), Mode == EWindowMode::Fullscreen ? SelectedColour : IdleColour);
	SetButtonColour(Find<UButton>(TEXT("ModeWindowed")), Mode == EWindowMode::Windowed ? SelectedColour : IdleColour);
	SetButtonColour(Find<UButton>(TEXT("ModeBorderless")), Mode == EWindowMode::WindowedFullscreen ? SelectedColour : IdleColour);
	if (Resolutions.IsValidIndex(ResolutionIndex))
	{
		SetText(Find<UTextBlock>(TEXT("ResText")), FText::FromString(FString::Printf(TEXT("%d x %d"), Resolutions[ResolutionIndex].X, Resolutions[ResolutionIndex].Y)));
	}
	SetText(Find<UTextBlock>(TEXT("ResNote")), Mode == EWindowMode::WindowedFullscreen
		? NSLOCTEXT("RobberMenu", "ResBorderless", "Borderless uses your desktop resolution.")
		: NSLOCTEXT("RobberMenu", "ResPick", "Pick what your screen supports."));
}

void URobberSettingsMenu::RefreshKeys()
{
	const URobberSettingsSubsystem* System = GetSettingsSystem();
	if (!System)
	{
		return;
	}
	using namespace RobberMenuDetail;
	for (const FRobberKeyAction& Action : URobberSettingsSubsystem::KeyActions())
	{
		const FString Name = FString::Printf(TEXT("Key%s"), *Action.Id.ToString());
		const bool bListening = ListeningFor == Action.Id;
		SetButtonColour(Find<UButton>(Name), bListening ? SelectedColour : IdleColour);
		SetText(Find<UTextBlock>(Name + TEXT("Text")), bListening ? INVTEXT("...") : URobberSettingsSubsystem::KeyName(System->GetKey(Action.Id)));
	}
	UTextBlock* Hint = Find<UTextBlock>(TEXT("KeysHint"));
	if (!ListeningFor.IsNone())
	{
		SetText(Hint, FText::Format(NSLOCTEXT("RobberMenu", "KeysListening", "Press a key or mouse button for {0}. Esc cancels."), FText::FromName(ListeningFor)));
		SetTextColour(Hint, ListeningTextColour);
	}
	else
	{
		SetText(Hint, SwapNote.IsEmpty() ? NSLOCTEXT("RobberMenu", "KeysHint", "Click a key to change it.") : SwapNote);
		SetTextColour(Hint, HintTextColour);
	}
}

void URobberSettingsMenu::RefreshMouse()
{
	const URobberSettingsSubsystem* System = GetSettingsSystem();
	const URobberSettingsSave* Settings = System ? System->GetSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	const USlider* Slider = Find<USlider>(TEXT("SensSlider"));
	const float Range = Slider ? Slider->GetMaxValue() - Slider->GetMinValue() : 1.f;
	if (UProgressBar* Fill = Find<UProgressBar>(TEXT("SensFill")))
	{
		Fill->SetPercent(Slider && Range > 0.f ? (Settings->LookSensitivity - Slider->GetMinValue()) / Range : 0.f);
	}
	FNumberFormattingOptions TwoPlaces;
	TwoPlaces.SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);
	SetText(Find<UTextBlock>(TEXT("SensValue")), FText::Format(INVTEXT("{0}x"), FText::AsNumber(Settings->LookSensitivity, &TwoPlaces)));
	SetText(Find<UTextBlock>(TEXT("SensNote")), Settings->LookSensitivity < 0.6f ? NSLOCTEXT("RobberMenu", "SensLow", "Slow and careful. Very museum.")
		: Settings->LookSensitivity <= 1.5f ? NSLOCTEXT("RobberMenu", "SensMid", "A steady hand on the crowbar.")
		: NSLOCTEXT("RobberMenu", "SensHigh", "One sneeze and you are facing the wall."));
}

void URobberSettingsMenu::RefreshApply()
{
	using namespace RobberMenuDetail;
	const FText Note = bSaved ? NSLOCTEXT("RobberMenu", "ApplySaved", "Filed away for next time.")
		: bChanged ? NSLOCTEXT("RobberMenu", "ApplyDirty", "You have changes that are not saved yet.")
		: NSLOCTEXT("RobberMenu", "ApplyNothing", "Nothing to save yet.");
	for (const TCHAR* Suffix : { TEXT(""), TEXT("2") })
	{
		SetButtonColour(Find<UButton>(FString(TEXT("ApplyButton")) + Suffix), bChanged ? SelectedColour : NothingToSaveColour);
		SetText(Find<UTextBlock>(FString(TEXT("ApplyNote")) + Suffix), Note);
		Show(Find(FString(TEXT("SavedStamp")) + Suffix), bSaved);
	}
}

void URobberSettingsMenu::MarkChanged()
{
	if (bLoading)
	{
		return;
	}
	bChanged = true;
	bSaved = false;
	RefreshApply();
}

void URobberSettingsMenu::ChangeSettings(TFunctionRef<void(URobberSettingsSave&)> Edit, bool bAudio)
{
	URobberSettingsSubsystem* System = bLoading ? nullptr : GetSettingsSystem();
	if (!System)
	{
		return;
	}
	Edit(*System->GetSettings());
	if (bAudio)
	{
		// Not for the UI size: rescaling mid-drag would move the slider out from under the mouse.
		System->Apply();
		RefreshAudio();
	}
	MarkChanged();
}

void URobberSettingsMenu::OnMusicChanged(float Value) { ChangeSettings([Value](URobberSettingsSave& Save) { Save.MusicVolume = Value; }, true); }
void URobberSettingsMenu::OnSfxChanged(float Value) { ChangeSettings([Value](URobberSettingsSave& Save) { Save.SfxVolume = Value; }, true); }
void URobberSettingsMenu::OnVoiceChanged(float Value) { ChangeSettings([Value](URobberSettingsSave& Save) { Save.VoiceVolume = Value; }, true); }
void URobberSettingsMenu::OnInvertChanged(bool bChecked) { ChangeSettings([bChecked](URobberSettingsSave& Save) { Save.bInvertLook = bChecked; }); }
void URobberSettingsMenu::OnSmoothChanged(bool bChecked) { ChangeSettings([bChecked](URobberSettingsSave& Save) { Save.bSmoothMouse = bChecked; }); }

void URobberSettingsMenu::OnSensChanged(float Value)
{
	ChangeSettings([Value](URobberSettingsSave& Save) { Save.LookSensitivity = Value; });
	RefreshMouse();
}

void URobberSettingsMenu::RefreshUiSize()
{
	const URobberSettingsSubsystem* System = GetSettingsSystem();
	const URobberSettingsSave* Settings = System ? System->GetSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	if (UProgressBar* Fill = Find<UProgressBar>(TEXT("UiSizeFill")))
	{
		Fill->SetPercent((Settings->UiSize - 0.75f) / 0.5f);
	}
	SetText(Find<UTextBlock>(TEXT("UiSizeValue")), FText::Format(INVTEXT("{0}%"), FText::AsNumber(FMath::RoundToInt(Settings->UiSize * 100.f))));
}

void URobberSettingsMenu::OnUiSizeChanged(float Value)
{
	ChangeSettings([Value](URobberSettingsSave& Save) { Save.UiSize = FMath::Clamp(FMath::RoundToFloat(Value * 20.f) / 20.f, 0.75f, 1.25f); });
	RefreshUiSize();
}

void URobberSettingsMenu::OnUiSizeReleased()
{
	// Rescaling mid-drag would move the slider out from under the mouse.
	if (URobberSettingsSubsystem* System = GetSettingsSystem())
	{
		System->ApplyUiSize();
	}
}

void URobberSettingsMenu::SetWindowMode(EWindowMode::Type Mode)
{
	if (UGameUserSettings* Display = UGameUserSettings::GetGameUserSettings())
	{
		Display->SetFullscreenMode(Mode);
		RefreshDisplay();
		MarkChanged();
	}
}

void URobberSettingsMenu::OnFullscreen() { SetWindowMode(EWindowMode::Fullscreen); }
void URobberSettingsMenu::OnWindowed() { SetWindowMode(EWindowMode::Windowed); }
void URobberSettingsMenu::OnBorderless() { SetWindowMode(EWindowMode::WindowedFullscreen); }

void URobberSettingsMenu::StepResolution(int32 Delta)
{
	UGameUserSettings* Display = UGameUserSettings::GetGameUserSettings();
	if (!Display || Resolutions.Num() == 0)
	{
		return;
	}
	ResolutionIndex = (ResolutionIndex + Delta + Resolutions.Num()) % Resolutions.Num();
	Display->SetScreenResolution(Resolutions[ResolutionIndex]);
	RefreshDisplay();
	MarkChanged();
}

void URobberSettingsMenu::OnResPrev() { StepResolution(-1); }
void URobberSettingsMenu::OnResNext() { StepResolution(1); }

void URobberSettingsMenu::OnApply()
{
	if (URobberSettingsSubsystem* System = GetSettingsSystem())
	{
		System->Save();
	}
	if (UGameUserSettings* Display = UGameUserSettings::GetGameUserSettings())
	{
		// False: a command line override shouldn't beat what the player just picked.
		Display->ApplySettings(false);
	}
	bChanged = false;
	bSaved = true;
	RefreshApply();
}

void URobberSettingsMenu::OnResetKeys()
{
	if (URobberSettingsSubsystem* System = GetSettingsSystem())
	{
		System->ResetKeys();
		ListeningFor = NAME_None;
		SwapNote = NSLOCTEXT("RobberMenu", "KeysReset", "Keys are back to the defaults.");
		RefreshKeys();
		MarkChanged();
	}
}

void URobberSettingsMenu::StartListening(FName ActionId)
{
	ListeningFor = ActionId;
	SwapNote = FText::GetEmpty();
	RefreshKeys();
	SetKeyboardFocus();
}

void URobberSettingsMenu::OnKeyMoveForward() { StartListening(TEXT("MoveForward")); }
void URobberSettingsMenu::OnKeyMoveLeft() { StartListening(TEXT("MoveLeft")); }
void URobberSettingsMenu::OnKeyMoveBack() { StartListening(TEXT("MoveBack")); }
void URobberSettingsMenu::OnKeyMoveRight() { StartListening(TEXT("MoveRight")); }
void URobberSettingsMenu::OnKeyJump() { StartListening(TEXT("Jump")); }
void URobberSettingsMenu::OnKeyInteract() { StartListening(TEXT("Interact")); }
void URobberSettingsMenu::OnKeyUse() { StartListening(TEXT("Use")); }
void URobberSettingsMenu::OnKeyThrow() { StartListening(TEXT("Throw")); }
void URobberSettingsMenu::OnKeyDrop() { StartListening(TEXT("Drop")); }
void URobberSettingsMenu::OnKeyJournal() { StartListening(TEXT("Journal")); }
void URobberSettingsMenu::OnKeyPause() { StartListening(TEXT("Pause")); }
void URobberSettingsMenu::OnKeySprint() { StartListening(TEXT("Sprint")); }

void URobberSettingsMenu::CaptureKey(const FKey& Key)
{
	URobberSettingsSubsystem* System = GetSettingsSystem();
	if (!System || ListeningFor.IsNone())
	{
		return;
	}
	const FName Action = ListeningFor;
	ListeningFor = NAME_None;
	const FName Swapped = System->SetKey(Action, Key);
	SwapNote = Swapped.IsNone() ? FText::GetEmpty()
		: FText::Format(NSLOCTEXT("RobberMenu", "KeysSwapped", "{0} was already in use, so the two keys swapped."), URobberSettingsSubsystem::KeyName(Key));
	RefreshKeys();
	MarkChanged();
}

FReply URobberSettingsMenu::NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (!ListeningFor.IsNone())
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			ListeningFor = NAME_None;
			RefreshKeys();
		}
		else
		{
			CaptureKey(InKeyEvent.GetKey());
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(MyGeometry, InKeyEvent);
}

FReply URobberSettingsMenu::NativeOnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Preview runs before any button under the mouse, so a click binds the mouse button instead of pressing it.
	if (!ListeningFor.IsNone())
	{
		CaptureKey(MouseEvent.GetEffectingButton());
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(MyGeometry, MouseEvent);
}

// --- Ransom letters ---

void RobberRansom::Fill(UPanelWidget* Panel, const FString& Text, int32 FontSize, float LetterHeight, UObject* Outer)
{
	if (!Panel)
	{
		return;
	}
	Panel->ClearChildren();

	static const TCHAR* FontNames[] = { TEXT("F_Bangers"), TEXT("F_PlayfairDisplay"), TEXT("F_SpecialElite"), TEXT("F_Fredoka") };
	struct FSwatch { FColor Back; FColor Ink; };
	static const FSwatch Swatches[] = {
		{ FColor(0xfb, 0xf6, 0xe9), FColor(0x1b, 0x1b, 0x1b) }, { FColor(0x1b, 0x1b, 0x1b), FColor(0xfb, 0xf6, 0xe9) }, { FColor(0xff, 0xd4, 0x3b), FColor(0x1b, 0x1b, 0x1b) },
		{ FColor(0xff, 0x87, 0x87), FColor(0x1b, 0x1b, 0x1b) }, { FColor(0x74, 0xc0, 0xfc), FColor(0x1b, 0x1b, 0x1b) }, { FColor(0xd0, 0xbf, 0xff), FColor(0x1b, 0x1b, 0x1b) }, { FColor(0xc0, 0x39, 0x2b), FColor::White },
	};

	uint32 Seed = 0;
	for (const TCHAR Char : Text)
	{
		Seed = (Seed * 31u + static_cast<uint32>(Char)) % 9973u;
	}

	TArray<FString> Words;
	Text.ParseIntoArray(Words, TEXT(" "));
	for (const FString& Word : Words)
	{
		// A box per word, so a title only wraps between words.
		UHorizontalBox* WordBox = NewObject<UHorizontalBox>(Outer);
		for (int32 Index = 0; Index < Word.Len(); ++Index)
		{
			Seed = (Seed * 17u + 13u + Index) % 9973u;
			const FSwatch& Swatch = Swatches[(Seed >> 2) % UE_ARRAY_COUNT(Swatches)];
			const TCHAR* FontName = FontNames[Seed % UE_ARRAY_COUNT(FontNames)];

			UTextBlock* Letter = NewObject<UTextBlock>(Outer);
			const FString Ch(1, &Word[Index]);
			Letter->SetText(FText::FromString(static_cast<uint32>(Index) % 3u == Seed % 3u ? Ch.ToLower() : Ch.ToUpper()));
			Letter->SetColorAndOpacity(FSlateColor(FLinearColor(Swatch.Ink)));
			if (UFont* Font = LoadObject<UFont>(nullptr, *FString::Printf(TEXT("/Game/OurStuff/UI/CaseFile/Fonts/%s.%s"), FontName, FontName)))
			{
				Letter->SetFont(FSlateFontInfo(Font, FontSize, TEXT("Regular")));
			}

			UBorder* Back = NewObject<UBorder>(Outer);
			Back->SetBrushColor(FLinearColor(Swatch.Back));
			Back->SetPadding(FMargin(FontSize * 0.14f, 0.f));
			Back->SetHorizontalAlignment(HAlign_Center);
			Back->SetVerticalAlignment(VAlign_Center);
			Back->SetContent(Letter);
			Back->SetRenderTransformAngle(static_cast<float>(static_cast<int32>((Seed >> 3) % 11u) - 5));

			USizeBox* Size = NewObject<USizeBox>(Outer);
			Size->SetHeightOverride(LetterHeight);
			Size->SetMinDesiredWidth(LetterHeight * 0.62f);
			Size->SetContent(Back);

			UHorizontalBoxSlot* Slot = WordBox->AddChildToHorizontalBox(Size);
			Slot->SetPadding(FMargin(1.f, 0.f));
		}
		Panel->AddChild(WordBox);
		if (UHorizontalBoxSlot* AsHorizontal = Cast<UHorizontalBoxSlot>(WordBox->Slot))
		{
			AsHorizontal->SetPadding(FMargin(0.f, 0.f, FontSize * 0.45f, 0.f));
		}
	}
}

// --- URobberAchievementStory ---

void URobberAchievementStory::SetStory(const FAchievementDefinition& Definition, const FAchievementProgressState& Progress, bool bFirstColumn)
{
	const bool bEarned = Progress.bUnlocked;
	const bool bCounting = Definition.Type == EAchievementType::Number && !bEarned;

	if (UTextBlock* Kicker = Find<UTextBlock>(TEXT("KickerText")))
	{
		Kicker->SetText(bEarned ? NSLOCTEXT("RobberMenu", "KickerEarned", "EXTRA! IN PRINT")
			: bCounting ? NSLOCTEXT("RobberMenu", "KickerCounting", "DEVELOPING STORY")
			: NSLOCTEXT("RobberMenu", "KickerLocked", "NOT YET REPORTED"));
		Kicker->SetColorAndOpacity(FSlateColor(bEarned ? EarnedKickerColour : FadedColour));
	}
	if (UTextBlock* Title = Find<UTextBlock>(TEXT("TitleText")))
	{
		Title->SetText(FText::FromString(Definition.Title.ToString().ToUpper()));
		Title->SetColorAndOpacity(FSlateColor(bEarned ? InkColour : FadedColour));
	}
	if (UTextBlock* Body = Find<UTextBlock>(TEXT("BodyText")))
	{
		Body->SetText(bEarned && !Definition.EarnedText.IsEmpty() ? Definition.EarnedText : Definition.Description);
		Body->SetColorAndOpacity(FSlateColor(bEarned ? InkColour : FadedColour));
	}
	if (UImage* Image = Find<UImage>(TEXT("StoryImage")))
	{
		Image->SetBrushFromTexture(RobberMenuDetail::ImageFor(Definition, bEarned));
	}
	Show(Find(TEXT("ColumnRule")), !bFirstColumn);
	Show(Find(TEXT("ProgressRow")), bCounting);
	if (bCounting)
	{
		const int32 Required = FMath::Max(Definition.RequiredAmount, 1);
		if (UProgressBar* Bar = Find<UProgressBar>(TEXT("ProgressBar")))
		{
			Bar->SetPercent(static_cast<float>(Progress.CurrentAmount) / Required);
		}
		SetText(Find<UTextBlock>(TEXT("ProgressText")), FText::Format(INVTEXT("{0}/{1}"), Progress.CurrentAmount, Required));
	}
}

// --- URobberAchievementScrap ---

void URobberAchievementScrap::SetScrap(const FAchievementDefinition& Definition, const FAchievementProgressState& Progress, int32 Index)
{
	const bool bEarned = Progress.bUnlocked;
	UTexture2D* Torn = TornPapers.Num() > 0 ? TornPapers[Index % TornPapers.Num()].Get() : nullptr;
	for (const TCHAR* Name : { TEXT("ScrapPaper"), TEXT("ScrapShadow") })
	{
		if (UImage* Image = Find<UImage>(Name); Image && Torn)
		{
			Image->SetBrushFromTexture(Torn);
		}
	}
	if (UImage* Paper = Find<UImage>(TEXT("ScrapPaper")))
	{
		Paper->SetColorAndOpacity(bEarned && EarnedPaperColours.Num() > 0 ? EarnedPaperColours[Index % EarnedPaperColours.Num()] : LockedPaperColour);
	}
	RobberRansom::Fill(Find<UPanelWidget>(TEXT("TitleLetters")), bEarned ? Definition.Title.ToString() : TEXT("? ? ?"), 28, 40.f, this);
	SetText(Find<UTextBlock>(TEXT("BodyText")), bEarned
		? (Definition.EarnedText.IsEmpty() ? Definition.Description : Definition.EarnedText)
		: Definition.LockedHint);
	if (UImage* Image = Find<UImage>(TEXT("ScrapImage")))
	{
		Image->SetBrushFromTexture(RobberMenuDetail::ImageFor(Definition, bEarned));
		Image->SetRenderTransformAngle(Index % 2 ? 4.f : -4.f);
	}
}

// --- URobberAchievementsMenu ---

void URobberAchievementsMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!StoryClass) StoryClass = RobberMenuDetail::LoadMenuClass<URobberAchievementStory>(TEXT("WBP_RobberAchievementStory"));
	if (!ScrapClass) ScrapClass = RobberMenuDetail::LoadMenuClass<URobberAchievementScrap>(TEXT("WBP_RobberAchievementScrap"));
	if (UButton* Button = Find<UButton>(TEXT("TabAll"))) Button->OnClicked.AddUniqueDynamic(this, &URobberAchievementsMenu::OnTabAll);
	if (UButton* Button = Find<UButton>(TEXT("TabEarned"))) Button->OnClicked.AddUniqueDynamic(this, &URobberAchievementsMenu::OnTabEarned);
	if (UButton* Button = Find<UButton>(TEXT("TabLocked"))) Button->OnClicked.AddUniqueDynamic(this, &URobberAchievementsMenu::OnTabLocked);
	if (UButton* Button = Find<UButton>(TEXT("TabSecret"))) Button->OnClicked.AddUniqueDynamic(this, &URobberAchievementsMenu::OnTabSecret);
}

void URobberAchievementsMenu::NativeConstruct()
{
	Super::NativeConstruct();
	ShowTab(0);
}

void URobberAchievementsMenu::OnTabAll() { ShowTab(0); }
void URobberAchievementsMenu::OnTabEarned() { ShowTab(1); }
void URobberAchievementsMenu::OnTabLocked() { ShowTab(2); }
void URobberAchievementsMenu::OnTabSecret() { ShowTab(3); }

void URobberAchievementsMenu::ShowTab(int32 Index)
{
	Tab = Index;
	const TCHAR* Names[] = { TEXT("TabAllSize"), TEXT("TabEarnedSize"), TEXT("TabLockedSize"), TEXT("TabSecretSize") };
	for (int32 Each = 0; Each < UE_ARRAY_COUNT(Names); ++Each)
	{
		if (USizeBox* Size = Find<USizeBox>(Names[Each]))
		{
			Size->SetHeightOverride(Each == Tab ? 75.f : 60.f);
		}
	}
	if (UWidgetSwitcher* Switcher = Find<UWidgetSwitcher>(TEXT("ViewSwitcher")))
	{
		Switcher->SetActiveWidgetIndex(Tab == 3 ? 1 : 0);
	}
	Refresh();
}

void URobberAchievementsMenu::Refresh()
{
	using namespace RobberMenuDetail;
	const UAchievementSubsystem* Achievements = SubsystemOf<UAchievementSubsystem>(this);
	if (!Achievements)
	{
		return;
	}

	const TArray<FAchievementDefinition> Normal = GetDefinitions(this, false);
	int32 Earned = 0;
	for (const FAchievementDefinition& Definition : Normal)
	{
		Earned += Achievements->IsAchievementUnlocked(Definition.AchievementId);
	}
	SetText(Find<UTextBlock>(TEXT("ViewNameText")), Tab == 1 ? NSLOCTEXT("RobberMenu", "ViewEarned", "ACHIEVEMENTS · THE STORIES SO FAR")
		: Tab == 2 ? NSLOCTEXT("RobberMenu", "ViewLocked", "ACHIEVEMENTS · STILL TO COME")
		: NSLOCTEXT("RobberMenu", "ViewAll", "ACHIEVEMENTS · COLLECTED EDITION"));
	SetText(Find<UTextBlock>(TEXT("EarnedCountText")), FText::Format(NSLOCTEXT("RobberMenu", "InPrint", "{0} OF {1} STORIES IN PRINT"), Earned, Normal.Num()));
	if (UProgressBar* Fill = Find<UProgressBar>(TEXT("EarnedFill")))
	{
		Fill->SetPercent(Normal.Num() > 0 ? static_cast<float>(Earned) / Normal.Num() : 0.f);
	}

	if (UUniformGridPanel* Grid = Find<UUniformGridPanel>(TEXT("StoryGrid")); Grid && StoryClass)
	{
		Grid->ClearChildren();
		int32 Count = 0;
		for (const FAchievementDefinition& Definition : Normal)
		{
			const FAchievementProgressState Progress = Achievements->GetAchievementProgress(Definition.AchievementId);
			if ((Tab == 1 && !Progress.bUnlocked) || (Tab == 2 && Progress.bUnlocked))
			{
				continue;
			}
			URobberAchievementStory* Story = CreateWidget<URobberAchievementStory>(this, StoryClass);
			Story->SetStory(Definition, Progress, Count % 4 == 0);
			Grid->AddChildToUniformGrid(Story, Count / 4, Count % 4);
			++Count;
		}
	}

	const TArray<FAchievementDefinition> Secret = GetDefinitions(this, true);
	int32 Found = 0;
	if (UCanvasPanel* Canvas = Find<UCanvasPanel>(TEXT("ScrapCanvas")); Canvas && ScrapClass)
	{
		Canvas->ClearChildren();
		// Six spots, deliberately uneven. Past that they wrap to more rows lower down.
		static const FVector2D Spots[] = { { 36.f, 117.f }, { 579.f, 105.f }, { 1113.f, 126.f }, { 54.f, 477.f }, { 597.f, 495.f }, { 1128.f, 468.f } };
		static const float Tilts[] = { -2.2f, 1.4f, -1.f, 1.8f, -1.5f, 2.4f };
		for (int32 Index = 0; Index < Secret.Num(); ++Index)
		{
			const FAchievementProgressState Progress = Achievements->GetAchievementProgress(Secret[Index].AchievementId);
			Found += Progress.bUnlocked;
			URobberAchievementScrap* Scrap = CreateWidget<URobberAchievementScrap>(this, ScrapClass);
			Scrap->SetScrap(Secret[Index], Progress, Index);
			Scrap->SetRenderTransformAngle(Tilts[Index % UE_ARRAY_COUNT(Tilts)]);
			UCanvasPanelSlot* ScrapSlot = Canvas->AddChildToCanvas(Scrap);
			ScrapSlot->SetAutoSize(true);
			const FVector2D Spot = Spots[Index % UE_ARRAY_COUNT(Spots)];
			ScrapSlot->SetPosition(FVector2D(Spot.X, Spot.Y + 720.f * (Index / UE_ARRAY_COUNT(Spots))));
		}
	}
	SetText(Find<UTextBlock>(TEXT("SecretCountText")), FText::Format(NSLOCTEXT("RobberMenu", "SecretCount", "{0} of {1} found. Nobody saw anything."), Found, Secret.Num()));
	RobberRansom::Fill(Find<UPanelWidget>(TEXT("HeadingLetters")), TEXT("Off the record"), 44, 66.f, this);
}

// --- URobberPauseMenu ---

void URobberPauseMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!SettingsClass) SettingsClass = RobberMenuDetail::LoadMenuClass<URobberMenuWidget>(TEXT("WBP_RobberSettings"));
	if (UButton* Button = Find<UButton>(TEXT("ResumeButton"))) Button->OnClicked.AddUniqueDynamic(this, &URobberPauseMenu::OnResume);
	if (UButton* Button = Find<UButton>(TEXT("SettingsButton"))) Button->OnClicked.AddUniqueDynamic(this, &URobberPauseMenu::OnSettings);
	if (UButton* Button = Find<UButton>(TEXT("QuitButton"))) Button->OnClicked.AddUniqueDynamic(this, &URobberPauseMenu::OnQuit);
	if (UButton* Button = Find<UButton>(TEXT("StayButton"))) Button->OnClicked.AddUniqueDynamic(this, &URobberPauseMenu::OnStay);
	if (UButton* Button = Find<UButton>(TEXT("ConfirmQuitButton"))) Button->OnClicked.AddUniqueDynamic(this, &URobberPauseMenu::OnConfirmQuit);
}

void URobberPauseMenu::CloseMenu()
{
	if (ARobberController* Controller = Cast<ARobberController>(GetOwningPlayer()))
	{
		// Clears the controller's own record of the open menu, or the pause key wouldn't reopen it.
		Controller->ClosePauseMenu();
		return;
	}
	Super::CloseMenu();
}

FReply URobberPauseMenu::NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// UI-only input stops the pause key reaching the controller, so it has to close the menu from here.
	const URobberSettingsSubsystem* System = GetSettingsSystem();
	if (System && InKeyEvent.GetKey() == System->GetKey(TEXT("Pause")))
	{
		CloseMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(MyGeometry, InKeyEvent);
}

void URobberPauseMenu::OnResume()
{
	CloseMenu();
}

void URobberPauseMenu::OnSettings()
{
	if (URobberMenuWidget* Settings = SettingsClass ? CreateWidget<URobberMenuWidget>(GetOwningPlayer(), SettingsClass) : nullptr)
	{
		Settings->SetReturnTo(this);
		Settings->AddToViewport(10);
	}
}

void URobberPauseMenu::OnQuit()
{
	const URobberPartySubsystem* Party = GetParty();
	const UWorld* World = GetWorld();
	const bool bHostWithCrew = Party && Party->IsLeader() && World && World->GetNetMode() == NM_ListenServer && Party->GetMembers().Num() > 1;
	SetText(Find<UTextBlock>(TEXT("QuitBodyText")), bHostWithCrew
		? NSLOCTEXT("RobberMenu", "QuitHost", "You're the host. If you leave, the night ends for everyone and the crew goes back to the menu.")
		: NSLOCTEXT("RobberMenu", "QuitNormal", "You'll leave this night and go back to the menu."));
	if (UWidgetSwitcher* Switcher = Find<UWidgetSwitcher>(TEXT("PauseSwitcher")))
	{
		Switcher->SetActiveWidgetIndex(1);
	}
}

void URobberPauseMenu::ShowQuitCheckOnly()
{
	bQuitCheckOnly = true;
	OnQuit();
}

void URobberPauseMenu::OnStay()
{
	if (bQuitCheckOnly)
	{
		CloseMenu();
		return;
	}
	if (UWidgetSwitcher* Switcher = Find<UWidgetSwitcher>(TEXT("PauseSwitcher")))
	{
		Switcher->SetActiveWidgetIndex(0);
	}
}

void URobberPauseMenu::OnConfirmQuit()
{
	SetText(Find<UTextBlock>(TEXT("ConfirmQuitText")), NSLOCTEXT("RobberMenu", "Leaving", "LEAVING..."));
	const ARobberGameState* State = GetRobberState();
	if (State && !RobberStage::IsInLobby(State->GetGameStage()))
	{
		if (URobberAchievementTracker* Tracker = GetGameInstance()->GetSubsystem<URobberAchievementTracker>())
		{
			Tracker->Apply(TEXT("BailedOut"), ERobberAchievementStep::Unlock, NAME_None, 0);
		}
	}
	if (APlayerController* Controller = GetOwningPlayer())
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->bShowMouseCursor = false;
	}
	if (URobberPartySubsystem* Party = GetParty())
	{
		Party->QuitToMenu();
	}
}
