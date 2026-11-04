#include "Heist/RobberJournal.h"

#include "Dialogue/DialogueComponent.h"
#include "Dialogue/DialogueDefinition.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Robber/RobberController.h"

URobberJournalComponent::URobberJournalComponent()
{
	SetIsReplicatedByDefault(true);
}

void URobberJournalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(URobberJournalComponent, Entries, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URobberJournalComponent, Asked, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URobberJournalComponent, VaultDigits, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URobberJournalComponent, DigitSources, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URobberJournalComponent, Revision, COND_OwnerOnly);
}

void URobberJournalComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		ConversationHandle = UDialogueComponent::OnConversationStarted.AddUObject(this, &URobberJournalComponent::OnConversationStarted);
	}
}

void URobberJournalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UDialogueComponent::OnConversationStarted.Remove(ConversationHandle);
	Super::EndPlay(EndPlayReason);
}

void URobberJournalComponent::OnConversationStarted(AActor* Talker, APlayerState* Player)
{
	const AController* Owner = Cast<AController>(GetOwner());
	if (!Talker || !Owner || Player != Owner->PlayerState || Talker->GetWorld() != GetWorld())
	{
		return;
	}
	const UDialogueComponent* Dialogue = Talker->FindComponentByClass<UDialogueComponent>();
	Asked.AddUnique(Talker->GetFName());
	if (Dialogue && !Dialogue->bNotedInJournal)
	{
		GetOwner()->ForceNetUpdate();
		OnRep_Journal();
		return;
	}
	const FText Speaker =Dialogue && Dialogue->Dialogue && !Dialogue->Dialogue->SpeakerName.IsEmpty() ? Dialogue->Dialogue->SpeakerName : NSLOCTEXT("RobberJournal", "AnExhibit", "An exhibit");
	Hear(Talker, Speaker, !Dialogue ? FText::GetEmpty() : !Dialogue->JournalQuote.IsEmpty() ? Dialogue->JournalQuote : Dialogue->GetCurrentLine());
}

FRobberJournalEntry& URobberJournalComponent::FindOrAdd(AActor* Talker, const FText& Speaker)
{
	const FName Key = Talker->GetFName();
	FRobberJournalEntry* Entry = Entries.FindByPredicate([Key](const FRobberJournalEntry& Each) { return Each.TalkerName == Key; });
	if (!Entry)
	{
		Entry = &Entries.AddDefaulted_GetRef();
		Entry->TalkerName = Key;
		Entry->Speaker = Speaker;
	}
	Entry->HeardAt = GetWorld()->GetTimeSeconds();
	Entry->Revision = ++Revision;
	return *Entry;
}

void URobberJournalComponent::Hear(AActor* Talker, const FText& Speaker, const FText& Quote)
{
	if (!GetOwner()->HasAuthority() || !Talker)
	{
		return;
	}
	FRobberJournalEntry& Entry = FindOrAdd(Talker, Speaker);
	if (!Quote.IsEmpty())
	{
		Entry.Quote = Quote;
	}
	Changed(Speaker);
}

void URobberJournalComponent::LearnVaultDigit(AActor* Talker, const FText& Speaker, int32 Index, int32 Digit, int32 CodeLength)
{
	if (!GetOwner()->HasAuthority() || !Talker || Index < 0 || Index >= CodeLength)
	{
		return;
	}
	if (VaultDigits.Num() != CodeLength)
	{
		VaultDigits.Init(INDEX_NONE, CodeLength);
		DigitSources.Init(FText::GetEmpty(), CodeLength);
	}
	VaultDigits[Index] = Digit;
	DigitSources[Index] = Speaker;
	FRobberJournalEntry& Entry = FindOrAdd(Talker, Speaker);
	// One sentence per exhibit: "Vault code: number 1 is 6, number 3 is 5."
	const FText Part = FText::Format(NSLOCTEXT("RobberJournal", "DigitPart", "number {0} is {1}"), Index + 1, Digit);
	Entry.CluePartsText.Add(Part.ToString());
	JoinClue(Entry);
	Changed(Speaker);
}

void URobberJournalComponent::LearnFact(AActor* Talker, const FText& Speaker, const FText& Fact)
{
	if (!GetOwner()->HasAuthority() || !Talker || Fact.IsEmpty())
	{
		return;
	}
	const FName Key = Talker->GetFName();
	const FRobberJournalEntry* Existing = Entries.FindByPredicate([Key](const FRobberJournalEntry& Each) { return Each.TalkerName == Key; });
	if (Existing && Existing->Facts.Contains(Fact.ToString()))
	{
		return;
	}
	FRobberJournalEntry& Entry = FindOrAdd(Talker, Speaker);
	Entry.Facts.Add(Fact.ToString());
	JoinClue(Entry);
	Changed(Speaker);
}

void URobberJournalComponent::JoinClue(FRobberJournalEntry& Entry)
{
	TArray<FString> Sentences = Entry.Facts;
	if (!Entry.CluePartsText.IsEmpty())
	{
		Sentences.Add(FText::Format(NSLOCTEXT("RobberJournal", "Digits", "Vault code: {0}."), FText::FromString(FString::Join(Entry.CluePartsText, TEXT(", ")))).ToString());
	}
	Entry.Clue = FText::FromString(FString::Join(Sentences, TEXT(" ")));
}

void URobberJournalComponent::ResetForNight()
{
	Entries.Reset();
	Asked.Reset();
	VaultDigits.Reset();
	DigitSources.Reset();
	++Revision;
	OnRep_Journal();
	GetOwner()->ForceNetUpdate();
}

void URobberJournalComponent::Changed(const FText& Speaker)
{
	GetOwner()->ForceNetUpdate();
	OnRep_Journal();
	if (ARobberController* Robber = Cast<ARobberController>(GetOwner()))
	{
		Robber->ClientHeistMessage(FText::Format(NSLOCTEXT("RobberJournal", "Jotted", "OVERHEARD · {0}: jotted in your journal (J)"), Speaker));
	}
}

int32 URobberJournalComponent::GetUnreadCount() const
{
	int32 Count = 0;
	for (const FRobberJournalEntry& Entry : Entries)
	{
		Count += IsNew(Entry);
	}
	return Count;
}

void URobberJournalComponent::OnRep_Journal()
{
	OnJournalChanged.Broadcast();
}
