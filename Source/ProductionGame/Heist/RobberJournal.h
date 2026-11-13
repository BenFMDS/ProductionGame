#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RobberJournal.generated.h"

class APlayerState;

/** What one exhibit told this player tonight. */
USTRUCT(BlueprintType)
struct FRobberJournalEntry
{
	GENERATED_BODY()

	/** The talker's object name: one entry per exhibit. */
	UPROPERTY(BlueprintReadOnly, Category = "Robber|Journal")
	FName TalkerName;

	UPROPERTY(BlueprintReadOnly, Category = "Robber|Journal")
	FText Speaker;

	/** Their opening line when you asked. */
	UPROPERTY(BlueprintReadOnly, Category = "Robber|Journal")
	FText Quote;

	/** What slipped out (vault code digits), empty if nothing. */
	UPROPERTY(BlueprintReadOnly, Category = "Robber|Journal")
	FText Clue;

	/** Server only: the pieces Clue is joined from. */
	UPROPERTY(NotReplicated)
	TArray<FString> CluePartsText;

	/** Server only: other things they told you, also joined into Clue. */
	UPROPERTY(NotReplicated)
	TArray<FString> Facts;

	UPROPERTY(BlueprintReadOnly, Category = "Robber|Journal")
	float HeardAt = 0.f;

	/** Journal revision this entry last changed in; newer than what the player has read = NEW. */
	UPROPERTY()
	int32 Revision = 0;
};

/**
 * The robber's journal (J): everything the exhibits have told THIS player tonight, and the vault digits among it.
 * Personal on purpose: it only replicates to its owner, so the crew has to share what they heard by talking.
 * Lives on the player controller; ARobberGameState::ResetCrewToLobby clears it for the next night.
 */
UCLASS(ClassGroup = (Robber), meta = (BlueprintSpawnableComponent))
class PRODUCTIONGAME_API URobberJournalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URobberJournalComponent();
	/** The journal of the player behind this player state, or null. */
	static URobberJournalComponent* Of(const APlayerState* Player);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<FRobberJournalEntry>& GetEntries() const { return Entries; }

	/** Vault code digits this player knows, -1 where unknown (empty on nights without a vault). */
	const TArray<int32>& GetVaultDigits() const { return VaultDigits; }
	/** Who gave each digit, matching GetVaultDigits. */
	const TArray<FText>& GetDigitSources() const { return DigitSources; }

	bool IsNew(const FRobberJournalEntry& Entry) const { return Entry.Revision > ReadRevision; }
	int32 GetUnreadCount() const;
	/** Local: the player opened the journal. */
	void MarkAllRead() { ReadRevision = Revision; }
	int32 GetReadRevision() const { return ReadRevision; }

	/** Server only. Talker said Quote to this player (adds or refreshes their entry). */
	void Hear(AActor* Talker, const FText& Speaker, const FText& Quote);
	/** Server only. Talker let slip digit Index of a CodeLength-digit vault code. */
	void LearnVaultDigit(AActor* Talker, const FText& Speaker, int32 Index, int32 Digit, int32 CodeLength);
	/** Server only. Talker told this player something worth writing down. Hearing it again adds nothing. */
	void LearnFact(AActor* Talker, const FText& Speaker, const FText& Fact);
	/** This player has spoken to the talker tonight, including those with nothing worth writing down. */
	bool HasAsked(FName TalkerName) const { return Asked.Contains(TalkerName); }
	/** Server only. A fresh night. */
	void ResetForNight();

	/** Local, after replication or a server-side change. */
	FSimpleMulticastDelegate OnJournalChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FRobberJournalEntry& FindOrAdd(AActor* Talker, const FText& Speaker);
	void JoinClue(FRobberJournalEntry& Entry);
	void Changed(const FText& Speaker);
	void OnConversationStarted(AActor* Talker, APlayerState* Player);

	UFUNCTION()
	void OnRep_Journal();

	UPROPERTY(ReplicatedUsing = OnRep_Journal)
	TArray<FRobberJournalEntry> Entries;

	UPROPERTY(ReplicatedUsing = OnRep_Journal)
	TArray<FName> Asked;

	UPROPERTY(ReplicatedUsing = OnRep_Journal)
	TArray<int32> VaultDigits;

	UPROPERTY(ReplicatedUsing = OnRep_Journal)
	TArray<FText> DigitSources;

	UPROPERTY(ReplicatedUsing = OnRep_Journal)
	int32 Revision = 0;

	/** Local only. */
	int32 ReadRevision = 0;

	FDelegateHandle ConversationHandle;
};
