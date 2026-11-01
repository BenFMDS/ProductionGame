#include "Heist/RobberCaseFileUI.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Dialogue/DialogueComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberEntryPoint.h"
#include "Heist/RobberJournal.h"
#include "Heist/RobberStage.h"
#include "Heist/RobberVault.h"
#include "Quest/PlayerQuestComponent.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"

// --- Shared ---

ARobberController* URobberCaseFileWidgetBase::GetRobber() const
{
	return Cast<ARobberController>(GetOwningPlayer());
}

const ARobberGameState* URobberCaseFileWidgetBase::GetRobberState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ARobberGameState>() : nullptr;
}

URobberJournalComponent* URobberCaseFileWidgetBase::GetJournal() const
{
	return GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<URobberJournalComponent>() : nullptr;
}

void URobberCaseFileWidgetBase::SetText(UTextBlock* Block, const FText& Text)
{
	// Comparing first: SetText invalidates layout even when nothing changed, and the HUD sets every frame.
	if (Block && !Block->GetText().EqualTo(Text))
	{
		Block->SetText(Text);
	}
}

void URobberCaseFileWidgetBase::Show(UWidget* Widget, bool bShown)
{
	const ESlateVisibility Wanted = bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	if (Widget && Widget->GetVisibility() != Wanted)
	{
		Widget->SetVisibility(Wanted);
	}
}

int32 URobberCaseFileWidgetBase::HeatLevel(const ARobberGameState* State)
{
	if (!State)
	{
		return 0;
	}
	const float Heat = State->GetHeat();
	return State->IsAlarmRaised() || Heat >= ARobberGameState::MaxHeat ? 3 : Heat >= 50.f ? 2 : Heat >= 25.f ? 1 : 0;
}

FLinearColor URobberCaseFileWidgetBase::HeatColour(int32 Level) const
{
	return HeatColours.IsValidIndex(Level) ? HeatColours[Level] : FLinearColor::White;
}

FLinearColor URobberCaseFileWidgetBase::CrewColour(const APlayerState* Player) const
{
	return Player && CrewColours.Num() > 0 ? CrewColours[Player->GetPlayerId() % CrewColours.Num()] : FLinearColor::White;
}

FText URobberCaseFileWidgetBase::Headline(int32 Level)
{
	static const FText Lines[] = {
		NSLOCTEXT("RobberCaseFile", "Headline0", "QUIET NIGHT AT THE MUSEUM"),
		NSLOCTEXT("RobberCaseFile", "Headline1", "STRANGE NOISES REPORTED"),
		NSLOCTEXT("RobberCaseFile", "Headline2", "BREAK-IN FEARED!"),
		NSLOCTEXT("RobberCaseFile", "Headline3", "THIEVES AT LARGE!"),
	};
	return Lines[FMath::Clamp(Level, 0, 3)];
}

FText URobberCaseFileWidgetBase::Subline(int32 Level)
{
	static const FText Lines[] = {
		NSLOCTEXT("RobberCaseFile", "Sub0", "Night guard reports \"absolutely nothing\" for the third night running."),
		NSLOCTEXT("RobberCaseFile", "Sub1", "Neighbours describe \"a sort of rattling\" somewhere near the museum."),
		NSLOCTEXT("RobberCaseFile", "Sub2", "Police \"very nearly\" on their way, says spokesman."),
		NSLOCTEXT("RobberCaseFile", "Sub3", "Alarm rings out across the city. Guards converge on the gallery."),
	};
	return Lines[FMath::Clamp(Level, 0, 3)];
}

FText URobberCaseFileWidgetBase::NightName(const ARobberGameState* State)
{
	if (!State)
	{
		return FText::GetEmpty();
	}
	return State->IsTutorial() ? NSLOCTEXT("RobberCaseFile", "Tutorial", "Tutorial")
		: FText::Format(NSLOCTEXT("RobberCaseFile", "NightN", "Night {0}"), FMath::Max(1, State->GetCurrentNight() + 1));
}

FText URobberCaseFileWidgetBase::Clock(float Seconds)
{
	const int32 Whole = FMath::CeilToInt(FMath::Max(0.f, Seconds));
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60));
}

TArray<APlayerState*> URobberCaseFileWidgetBase::SortedCrew(const ARobberGameState* State)
{
	// PlayerId order, so cards and colours match on every machine, like the map pins.
	TArray<APlayerState*> Crew;
	if (State)
	{
		for (APlayerState* Player : State->PlayerArray)
		{
			if (Player)
			{
				Crew.Add(Player);
			}
		}
	}
	Crew.Sort([](const APlayerState& A, const APlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	return Crew;
}

FText URobberCaseFileWidgetBase::CrewStatus(const ARobberGameState* State, const APlayerState* Player)
{
	if (!State || !Player)
	{
		return FText::GetEmpty();
	}
	const uint8 Stage = State->GetGameStage();
	if (RobberStage::IsInLobby(Stage))
	{
		return NSLOCTEXT("RobberCaseFile", "InLobby", "In the safe house");
	}
	const FRobberCrewStatus Status = State->GetCrewStatus(Player);
	if (Status.HidingSpot)
	{
		return NSLOCTEXT("RobberCaseFile", "Hiding", "Hiding");
	}
	if (Status.bInside || Stage != RobberStage::BreakIn)
	{
		return NSLOCTEXT("RobberCaseFile", "Inside", "Inside");
	}
	return Status.bHasTicket ? NSLOCTEXT("RobberCaseFile", "Ticket", "Has a ticket") : NSLOCTEXT("RobberCaseFile", "Outside", "Getting in");
}

bool URobberCaseFileWidgetBase::VaultStillShut(const ARobberGameState* State)
{
	return State && State->GetJobs().ContainsByPredicate([](const FRobberJob& Job) { return Job.Id == ARobberVaultDoor::JobId && !Job.bDone; });
}

FText URobberCaseFileWidgetBase::PlayerName(const APlayerState* Player) const
{
	if (!Player)
	{
		return FText::GetEmpty();
	}
	return GetOwningPlayer() && Player == GetOwningPlayer()->PlayerState ? NSLOCTEXT("RobberCaseFile", "You", "You") : FText::FromString(Player->GetPlayerName().Left(12));
}

namespace
{
	FText CaseFileDigits(const TArray<int32>& Digits, int32 Length)
	{
		FString Out;
		for (int32 Index = 0; Index < Length; ++Index)
		{
			const int32 Digit = Digits.IsValidIndex(Index) ? Digits[Index] : INDEX_NONE;
			Out += (Index ? TEXT(" ") : TEXT("")) + (Digit == INDEX_NONE ? FString(TEXT("_")) : FString::FromInt(Digit));
		}
		return FText::FromString(Out);
	}

	FText CaseFileKeyName(FName Key)
	{
		// "GeneratorRoom" -> "Generator Room"
		const FString Raw = Key.ToString();
		FString Out;
		for (int32 Index = 0; Index < Raw.Len(); ++Index)
		{
			if (Index > 0 && FChar::IsUpper(Raw[Index]) && !FChar::IsUpper(Raw[Index - 1]))
			{
				Out += TEXT(' ');
			}
			Out += Raw[Index];
		}
		return FText::FromString(Out);
	}
}

// --- HUD ---

FText URobberCaseFileHUD::GetHeadline() const
{
	return Headline(HeatLevel(GetRobberState()));
}

FText URobberCaseFileHUD::GetCodeLine() const
{
	const URobberJournalComponent* Journal = GetJournal();
	const bool bKnowSome = Journal && Journal->GetVaultDigits().Num() > 0;
	if (!bKnowSome && !VaultStillShut(GetRobberState()))
	{
		return FText::GetEmpty();
	}
	return CaseFileDigits(Journal ? Journal->GetVaultDigits() : TArray<int32>(), ARobberVaultDoor::CodeLength);
}

void URobberCaseFileHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const ARobberGameState* State = GetRobberState();
	if (!State)
	{
		return;
	}
	UpdateNote(State);
	UpdatePaper(State);
	UpdateCrew(State);

	const ARobberController* Robber = GetRobber();
	const bool bMessage = Robber && Robber->IsHeistMessageVisible();
	Show(Find(TEXT("ToastPanel")), bMessage);
	if (bMessage)
	{
		SetText(Find<UTextBlock>(TEXT("ToastText")), Robber->GetHeistMessage());
	}
	const URobberJournalComponent* Journal = GetJournal();
	const int32 Unread = Journal ? Journal->GetUnreadCount() : 0;
	Show(Find(TEXT("JournalNew")), Unread > 0);
	SetText(Find<UTextBlock>(TEXT("JournalNewText")), FText::Format(NSLOCTEXT("RobberCaseFile", "NewCount", "{0} new"), Unread));
}

void URobberCaseFileHUD::UpdateNote(const ARobberGameState* State)
{
	const uint8 Stage = State->GetGameStage();
	FText Title;
	switch (Stage)
	{
	case RobberStage::BreakIn: Title = NSLOCTEXT("RobberCaseFile", "TitleBreakIn", "GETTING IN"); break;
	case RobberStage::Investigate: Title = NSLOCTEXT("RobberCaseFile", "TitleInvestigate", "SNEAK ABOUT"); break;
	case RobberStage::Heist: Title = NSLOCTEXT("RobberCaseFile", "TitleHeist", "HEIST TIME!"); break;
	case RobberStage::Escape: Title = NSLOCTEXT("RobberCaseFile", "TitleEscape", "GET OUT!"); break;
	default: Title = NSLOCTEXT("RobberCaseFile", "TitlePlan", "THE PLAN"); break;
	}
	SetText(Find<UTextBlock>(TEXT("TitleText")), Title);
	SetText(Find<UTextBlock>(TEXT("NightText")), NightName(State));
	SetText(Find<UTextBlock>(TEXT("ObjectiveText")), GetOwningPlayer() ? State->GetObjectiveText(GetOwningPlayer()->PlayerState) : FText::GetEmpty());

	// Rows: the jobs, or in the lobby the vote counts (no tick box).
	struct FRow { FText Text; bool bDone; bool bBox; };
	TArray<FRow> Rows;
	if (RobberStage::IsInLobby(Stage))
	{
		TArray<const ARobberEntryPoint*> Counted;
		for (const FRobberEntryVote& Vote : State->GetVotes())
		{
			if (Vote.Entry && !Counted.Contains(Vote.Entry))
			{
				Counted.Add(Vote.Entry);
				Rows.Add({ FText::Format(NSLOCTEXT("RobberCaseFile", "VoteRow", "{0}: {1} {1}|plural(one=vote,other=votes)"), Vote.Entry->DisplayName, State->GetVoteCount(Vote.Entry)), false, false });
			}
		}
	}
	else
	{
		for (const FRobberJob& Job : State->GetJobs())
		{
			Rows.Add({ Job.Title, Job.bDone, true });
		}
	}
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const bool bRow = Rows.IsValidIndex(Index);
		Show(Find(FString::Printf(TEXT("JobRow%d"), Index)), bRow);
		if (!bRow)
		{
			continue;
		}
		UBorder* Box = Find<UBorder>(FString::Printf(TEXT("JobBox%d"), Index));
		Show(Box, Rows[Index].bBox);
		if (Box)
		{
			Box->SetBrushColor(Rows[Index].bDone ? TickColour : FLinearColor::White);
		}
		UTextBlock* Text = Find<UTextBlock>(FString::Printf(TEXT("JobText%d"), Index));
		SetText(Text, Rows[Index].Text);
		if (Text)
		{
			Text->SetColorAndOpacity(Rows[Index].bDone ? DoneTextColour : TextColour);
		}
	}

	const float TimeLeft = State->GetPhaseTimeRemaining();
	Show(Find(TEXT("TimerText")), TimeLeft >= 0.f);
	SetText(Find<UTextBlock>(TEXT("TimerText")), FText::Format(NSLOCTEXT("RobberCaseFile", "Closing", "CLOSING IN {0}"), Clock(TimeLeft)));

	const FText Code = GetCodeLine();
	Show(Find(TEXT("CodeRow")), !Code.IsEmpty());
	SetText(Find<UTextBlock>(TEXT("CodeText")), Code);
}

void URobberCaseFileHUD::UpdatePaper(const ARobberGameState* State)
{
	const int32 Level = HeatLevel(State);
	Show(Find(TEXT("PaperPanel")), !RobberStage::IsInLobby(State->GetGameStage()));
	SetText(Find<UTextBlock>(TEXT("DatelineText")), FText::Format(NSLOCTEXT("RobberCaseFile", "Dateline", "{0} · LATE EDITION"), FText::FromString(NightName(State).ToString().ToUpper())));
	SetText(Find<UTextBlock>(TEXT("HeatText")), FText::Format(NSLOCTEXT("RobberCaseFile", "HeatN", "HEAT {0}"), FMath::RoundToInt(State->GetHeat())));
	if (UProgressBar* Fuse = Find<UProgressBar>(TEXT("FuseBar")))
	{
		Fuse->SetPercent(State->GetHeat() / ARobberGameState::MaxHeat);
		Fuse->SetFillColorAndOpacity(HeatColour(Level));
	}
	SetText(Find<UTextBlock>(TEXT("HeadlineText")), Headline(Level));
	SetText(Find<UTextBlock>(TEXT("SublineText")), Subline(Level));

	const float Guards = State->GetGuardsTimeRemaining();
	Show(Find(TEXT("GuardsStamp")), Guards >= 0.f);
	SetText(Find<UTextBlock>(TEXT("GuardsText")), FText::Format(NSLOCTEXT("RobberCaseFile", "Guards", "GUARDS {0}"), Clock(Guards)));
}

void URobberCaseFileHUD::UpdateCrew(const ARobberGameState* State)
{
	const TArray<APlayerState*> Crew = SortedCrew(State);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const APlayerState* Player = Crew.IsValidIndex(Index) ? Crew[Index] : nullptr;
		Show(Find(FString::Printf(TEXT("CrewCard%d"), Index)), Player != nullptr);
		if (!Player)
		{
			continue;
		}
		if (UBorder* Colour = Find<UBorder>(FString::Printf(TEXT("CrewColour%d"), Index)))
		{
			Colour->SetBrushColor(CrewColour(Player));
		}
		SetText(Find<UTextBlock>(FString::Printf(TEXT("CrewName%d"), Index)), PlayerName(Player));
		SetText(Find<UTextBlock>(FString::Printf(TEXT("CrewStatus%d"), Index)), CrewStatus(State, Player));
	}
}

// --- Journal entry ---

void URobberJournalEntryWidget::SetEntry(const FRobberJournalEntry& Entry, bool bNew, float Now)
{
	SetText(Find<UTextBlock>(TEXT("SpeakerText")), Entry.Speaker);
	const int32 Minutes = FMath::FloorToInt(FMath::Max(0.f, Now - Entry.HeardAt) / 60.f);
	SetText(Find<UTextBlock>(TEXT("WhenText")), Minutes < 1 ? NSLOCTEXT("RobberJournal", "JustNow", "just now") : FText::Format(NSLOCTEXT("RobberJournal", "MinAgo", "{0} min ago"), Minutes));
	Show(Find(TEXT("QuoteText")), !Entry.Quote.IsEmpty());
	SetText(Find<UTextBlock>(TEXT("QuoteText")), FText::Format(NSLOCTEXT("RobberJournal", "Quote", "\"{0}\""), Entry.Quote));
	Show(Find(TEXT("ClueText")), !Entry.Clue.IsEmpty());
	SetText(Find<UTextBlock>(TEXT("ClueText")), Entry.Clue);
	Show(Find(TEXT("NewStamp")), bNew);

	TArray<FString> Words;
	Entry.Speaker.ToString().ParseIntoArrayWS(Words);
	FString Initials;
	for (const FString& Word : Words)
	{
		if (Initials.Len() < 2 && !Word.IsEmpty() && FChar::IsAlpha(Word[0]))
		{
			Initials += FChar::ToUpper(Word[0]);
		}
	}
	SetText(Find<UTextBlock>(TEXT("InitialsText")), FText::FromString(Initials.IsEmpty() ? TEXT("?") : Initials));
}

// --- Journal ---

void URobberJournalWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UButton* Tab = Find<UButton>(TEXT("TabClues")))
	{
		Tab->OnClicked.AddUniqueDynamic(this, &URobberJournalWidget::OnCluesClicked);
	}
	if (UButton* Tab = Find<UButton>(TEXT("TabCrew")))
	{
		Tab->OnClicked.AddUniqueDynamic(this, &URobberJournalWidget::OnCrewClicked);
	}
	if (UButton* Tab = Find<UButton>(TEXT("TabNight")))
	{
		Tab->OnClicked.AddUniqueDynamic(this, &URobberJournalWidget::OnNightClicked);
	}
	if (!EntryClass)
	{
		EntryClass = LoadClass<URobberJournalEntryWidget>(nullptr, TEXT("/Game/OurStuff/UI/CaseFile/WBP_JournalEntry.WBP_JournalEntry_C"));
	}
}

void URobberJournalWidget::OnCluesClicked() { ShowTab(0); }
void URobberJournalWidget::OnCrewClicked() { ShowTab(1); }
void URobberJournalWidget::OnNightClicked() { ShowTab(2); }

void URobberJournalWidget::ShowTab(int32 Index)
{
	if (UWidgetSwitcher* Switcher = Find<UWidgetSwitcher>(TEXT("TabSwitcher")))
	{
		Switcher->SetActiveWidgetIndex(Index);
	}
	// The open tab sits a little higher, like a pulled-up file tab.
	const TCHAR* Tabs[] = { TEXT("TabClues"), TEXT("TabCrew"), TEXT("TabNight") };
	for (int32 Each = 0; Each < UE_ARRAY_COUNT(Tabs); ++Each)
	{
		if (UWidget* Tab = Find(Tabs[Each]))
		{
			Tab->SetRenderTranslation(FVector2D(0.f, Each == Index ? -6.f : 0.f));
		}
	}
}

void URobberJournalWidget::OnOpened()
{
	if (URobberJournalComponent* Journal = GetJournal())
	{
		OpenedReadRevision = Journal->GetReadRevision();
		Journal->MarkAllRead();
	}
	Refresh();
}

void URobberJournalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// "2 min ago", crew whereabouts and newly heard entries change while it's open.
	RefreshIn -= InDeltaTime;
	if (RefreshIn <= 0.f)
	{
		Refresh();
	}
}

void URobberJournalWidget::Refresh()
{
	RefreshIn = 0.5f;
	const ARobberGameState* State = GetRobberState();
	const URobberJournalComponent* Journal = GetJournal();
	const float Now = State ? State->GetServerWorldTimeSeconds() : 0.f;
	SetText(Find<UTextBlock>(TEXT("NightText")), NightName(State));

	// --- OVERHEARD ---
	TArray<FRobberJournalEntry> Entries = Journal ? Journal->GetEntries() : TArray<FRobberJournalEntry>();
	Entries.Sort([](const FRobberJournalEntry& A, const FRobberJournalEntry& B) { return A.Revision > B.Revision; });
	if (UPanelWidget* List = Find<UPanelWidget>(TEXT("EntryList")); List && EntryClass)
	{
		while (EntryWidgets.Num() < Entries.Num())
		{
			URobberJournalEntryWidget* Row = CreateWidget<URobberJournalEntryWidget>(GetOwningPlayer(), EntryClass);
			if (!Row)
			{
				break;
			}
			List->AddChild(Row);
			EntryWidgets.Add(Row);
		}
		for (int32 Index = 0; Index < EntryWidgets.Num(); ++Index)
		{
			const bool bUsed = Entries.IsValidIndex(Index);
			Show(EntryWidgets[Index], bUsed);
			if (bUsed)
			{
				EntryWidgets[Index]->SetEntry(Entries[Index], Entries[Index].Revision > OpenedReadRevision, Now);
			}
		}
	}
	Show(Find(TEXT("EmptyText")), Entries.IsEmpty());
	int32 Unasked = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const FName Key = It->GetFName();
			Unasked += It->GetLevel() != World->PersistentLevel && It->FindComponentByClass<UDialogueComponent>()
				&& !Entries.ContainsByPredicate([Key](const FRobberJournalEntry& Entry) { return Entry.TalkerName == Key; });
		}
	}
	Show(Find(TEXT("UnaskedText")), Unasked > 0);
	SetText(Find<UTextBlock>(TEXT("UnaskedText")), FText::Format(NSLOCTEXT("RobberJournal", "Unasked", "??? · {0} {0}|plural(one=exhibit,other=exhibits) tonight you haven't asked yet."), Unasked));

	// --- CLUES ---
	const TArray<int32> Digits = Journal ? Journal->GetVaultDigits() : TArray<int32>();
	const TArray<FText> Sources = Journal ? Journal->GetDigitSources() : TArray<FText>();
	Show(Find(TEXT("VaultSection")), !Digits.IsEmpty() || VaultStillShut(State));
	for (int32 Index = 0; Index < ARobberVaultDoor::CodeLength; ++Index)
	{
		const bool bKnown = Digits.IsValidIndex(Index) && Digits[Index] != INDEX_NONE;
		SetText(Find<UTextBlock>(FString::Printf(TEXT("Digit%d"), Index)), bKnown ? FText::AsNumber(Digits[Index]) : FText::FromString(TEXT("?")));
		SetText(Find<UTextBlock>(FString::Printf(TEXT("DigitFrom%d"), Index)), bKnown && Sources.IsValidIndex(Index) ? Sources[Index] : NSLOCTEXT("RobberJournal", "NobodyYet", "nobody yet"));
		if (UBorder* Box = Find<UBorder>(FString::Printf(TEXT("DigitBox%d"), Index)))
		{
			Box->SetBrushColor(bKnown ? KnownDigitColour : UnknownDigitColour);
		}
	}
	const UPlayerQuestComponent* Quests = GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<UPlayerQuestComponent>() : nullptr;
	TArray<FString> Keys;
	for (const FName Key : Quests ? Quests->Keys : TArray<FName>())
	{
		Keys.Add(FText::Format(NSLOCTEXT("RobberJournal", "KeyRow", "{0} · in your pocket"), CaseFileKeyName(Key)).ToString());
	}
	SetText(Find<UTextBlock>(TEXT("KeysText")), Keys.IsEmpty() ? NSLOCTEXT("RobberJournal", "NoKeys", "No keys yet.") : FText::FromString(FString::Join(Keys, TEXT("\n"))));

	// --- CREW ---
	const TArray<APlayerState*> Crew = SortedCrew(State);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const APlayerState* Player = Crew.IsValidIndex(Index) ? Crew[Index] : nullptr;
		Show(Find(FString::Printf(TEXT("CrewRow%d"), Index)), Player != nullptr);
		if (!Player)
		{
			continue;
		}
		if (UBorder* Colour = Find<UBorder>(FString::Printf(TEXT("CrewColour%d"), Index)))
		{
			Colour->SetBrushColor(CrewColour(Player));
		}
		SetText(Find<UTextBlock>(FString::Printf(TEXT("CrewName%d"), Index)), PlayerName(Player));
		SetText(Find<UTextBlock>(FString::Printf(TEXT("CrewStatus%d"), Index)), CrewStatus(State, Player));
	}

	// --- NIGHT ---
	TArray<FString> Jobs;
	if (State)
	{
		for (const FRobberJob& Job : State->GetJobs())
		{
			Jobs.Add((Job.bDone ? TEXT("[x] ") : TEXT("[  ] ")) + Job.Title.ToString());
		}
	}
	const FText JobsText = !Jobs.IsEmpty() ? FText::FromString(FString::Join(Jobs, TEXT("\n")))
		: State && GetOwningPlayer() ? State->GetObjectiveText(GetOwningPlayer()->PlayerState) : FText::GetEmpty();
	SetText(Find<UTextBlock>(TEXT("JobsText")), JobsText);
	SetText(Find<UTextBlock>(TEXT("PaperLineText")), FText::Format(NSLOCTEXT("RobberJournal", "PaperLine", "HEAT {0} · today's paper: \"{1}\""),
		FMath::RoundToInt(State ? State->GetHeat() : 0.f), Headline(HeatLevel(State))));
}
