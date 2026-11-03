#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RobberParty.generated.h"

class APlayerState;
class UTexture2D;

/** Someone in the shared menu room. */
struct FRobberPartyMember
{
	FString Name;
	TWeakObjectPtr<APlayerState> Player;
	bool bLocal = false;
	bool bLeader = false;
};

/** A row in the friends list. */
struct FRobberFriendInfo
{
	FString Name;
	FUniqueNetIdPtr Id;
	bool bOnline = false;
	bool bPlayingThisGame = false;
	bool bJoinable = false;
};

/**
 * Parties are the shared menu room: making one reopens L_MainMenu as a listen server with a session, invited friends
 * connect to it and walk around together, and the leader's Create Game takes everyone to L_GameLevel.
 * All session calls for the menus are here, replacing the CreateSession event on BP_GameInstance.
 */
UCLASS()
class PRODUCTIONGAME_API URobberPartySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static const TCHAR* MenuMap() { return TEXT("/Game/OurStuff/Maps/L_MainMenu"); }
	static const TCHAR* GameMap() { return TEXT("/Game/OurStuff/Maps/L_GameLevel"); }
	static constexpr int32 MaxPlayers = 4;

	/** Connected to or hosting other people's copy of the menu room. */
	bool IsInParty() const;
	bool IsLeader() const;
	/** In someone else's party: they run the job. */
	bool IsMember() const;
	TArray<FRobberPartyMember> GetMembers() const;
	FString GetLeaderName() const;

	/** Makes a session and reopens the menu room as a listen server. InviteAfter, if set, gets invited once it's up. */
	void MakeParty(bool bLan, bool bFriendsOnly, FUniqueNetIdPtr InviteAfter = nullptr);
	/** Invites a friend, making the party first if there isn't one. */
	void InviteFriend(const FRobberFriendInfo& Friend, bool bLan, bool bFriendsOnly);
	void KickMember(APlayerState* Player);
	/** Leaves or breaks up the party: back to a private menu room. */
	void LeaveParty();

	/** Solo: makes a session and opens the game. Leader: takes the party to the game. */
	void CreateGame(bool bLan, bool bFriendsOnly);
	void JoinFriend(const FRobberFriendInfo& Friend);
	/** From the pause menu: ends any session and opens the menu room. */
	void QuitToMenu();

	/** Names of friends invited and not here yet. Entries drop off after a minute. */
	TArray<FString> GetPendingInvites() const;
	bool IsInvited(const FString& Name) const;

	/** A friend's invite waiting for an answer in the Play menu. */
	bool HasIncomingInvite() const { return bHasIncomingInvite; }
	FString GetIncomingInviteName() const { return IncomingInviteName; }
	void AcceptIncomingInvite();
	void DeclineIncomingInvite();

	/** Steam friends, refreshed by RefreshFriends. Empty without a friends service (LAN, PIE without Steam). */
	void RefreshFriends();
	const TArray<FRobberFriendInfo>& GetFriends() const { return Friends; }
	/** False when nothing here can list friends. */
	bool HasFriendsService() const;

	/** Null if there's no avatar (no Steam, or it hasn't arrived yet). */
	static UTexture2D* GetAvatar(const FRobberFriendInfo& Friend);

	/** Last thing that went wrong, for the Play menu. */
	FText GetLastError() const { return LastError; }

	/** The Play menu asks to come back after the map reload a new party causes. */
	bool ConsumeReopenPlayMenu();

#if !UE_BUILD_SHIPPING
	/** For tests: rows to show without a friends service. */
	void SetTestFriends(const TArray<FRobberFriendInfo>& InFriends) { Friends = InFriends; bTestFriends = true; }
	void SetTestIncomingInvite(const FString& Name) { bHasIncomingInvite = true; IncomingInviteName = Name; }
	/** For tests: the port a listen server is on. */
	int32 GetListenPort() const;
#endif

private:
	struct FPendingInvite
	{
		FString Name;
		double SentTime = 0.;
	};

	enum class EAfterSession : uint8 { None, OpenParty, OpenGame };

	IOnlineSessionPtr GetSessions() const;
	FUniqueNetIdPtr GetLocalId() const;
	bool HasSession() const;
	void CreateSessionThen(bool bLan, bool bFriendsOnly, EAfterSession Then);
	void DestroySessionThen(TFunction<void()> Then);
	void JoinResult(const FOnlineSessionSearchResult& Result);
	void OpenMenuPrivately();

	void OnCreateComplete(FName SessionName, bool bOk);
	void OnDestroyComplete(FName SessionName, bool bOk);
	void OnJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnFindFriendComplete(int32 LocalUser, bool bOk, const TArray<FOnlineSessionSearchResult>& Results);
	void OnInviteReceived(const FUniqueNetId& Invited, const FUniqueNetId& Inviter, const FString& AppId, const FOnlineSessionSearchResult& Result);
	void OnInviteAccepted(bool bOk, int32 ControllerId, FUniqueNetIdPtr User, const FOnlineSessionSearchResult& Result);
	void OnMapLoaded(UWorld* World);

	EAfterSession AfterCreate = EAfterSession::None;
	TFunction<void()> AfterDestroy;
	FUniqueNetIdPtr InviteAfterParty;
	bool bReopenPlayMenu = false;

	TArray<FPendingInvite> PendingInvites;
	TArray<FRobberFriendInfo> Friends;
	bool bTestFriends = false;

	bool bHasIncomingInvite = false;
	FString IncomingInviteName;
	FOnlineSessionSearchResult IncomingInvite;

	FText LastError;

	FDelegateHandle CreateHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle FindFriendHandle;
	FDelegateHandle InviteReceivedHandle;
	FDelegateHandle InviteAcceptedHandle;
	FDelegateHandle MapLoadedHandle;
};
