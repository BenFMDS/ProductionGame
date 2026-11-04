#include "RobberMenu/RobberParty.h"

#include "AdvancedSteamFriendsLibrary.h"
#include "BlueprintDataDefinitions.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "IPAddress.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogRobberParty, Log, All);

namespace RobberPartyDetail
{
	constexpr double InviteLifetime = 60.;

	/** Host first, then in join order. */
	AGameStateBase* GameStateOf(const UGameInstance* GameInstance)
	{
		const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
		return World ? World->GetGameState() : nullptr;
	}
}

void URobberPartySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &URobberPartySubsystem::OnMapLoaded);
}

void URobberPartySubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
	if (const IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnSessionInviteReceivedDelegate_Handle(InviteReceivedHandle);
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
	}
	Super::Deinitialize();
}

IOnlineSessionPtr URobberPartySubsystem::GetSessions() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World ? Online::GetSessionInterface(World) : nullptr;
}

FUniqueNetIdPtr URobberPartySubsystem::GetLocalId() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const IOnlineIdentityPtr Identity = World ? Online::GetIdentityInterface(World) : nullptr;
	return Identity ? Identity->GetUniquePlayerId(0) : nullptr;
}

bool URobberPartySubsystem::HasSession() const
{
	const IOnlineSessionPtr Sessions = GetSessions();
	return Sessions && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

bool URobberPartySubsystem::IsInParty() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World && World->GetNetMode() != NM_Standalone;
}

bool URobberPartySubsystem::IsLeader() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return !World || World->GetNetMode() != NM_Client;
}

bool URobberPartySubsystem::IsMember() const
{
	return !IsLeader();
}

TArray<FRobberPartyMember> URobberPartySubsystem::GetMembers() const
{
	TArray<FRobberPartyMember> Members;
	const AGameStateBase* GameState = RobberPartyDetail::GameStateOf(GetGameInstance());
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
	const APlayerState* LocalPlayer = LocalController ? LocalController->PlayerState : nullptr;
	if (!GameState)
	{
		return Members;
	}
	for (APlayerState* Player : GameState->PlayerArray)
	{
		if (!Player || Player->IsInactive())
		{
			continue;
		}
		FRobberPartyMember Member;
		Member.Name = Player->GetPlayerName();
		Member.Player = Player;
		Member.bLocal = Player == LocalPlayer;
		Members.Add(Member);
	}
	// PlayerId order is join order on every machine, and the host is always first in.
	Members.Sort([](const FRobberPartyMember& A, const FRobberPartyMember& B)
	{
		return A.Player->GetPlayerId() < B.Player->GetPlayerId();
	});
	if (Members.Num() > 0)
	{
		Members[0].bLeader = true;
	}
	return Members;
}

FString URobberPartySubsystem::GetLeaderName() const
{
	const TArray<FRobberPartyMember> Members = GetMembers();
	return Members.Num() > 0 ? Members[0].Name : FString();
}

void URobberPartySubsystem::CreateSessionThen(bool bLan, bool bFriendsOnly, EAfterSession Then)
{
	const IOnlineSessionPtr Sessions = GetSessions();
	const FUniqueNetIdPtr LocalId = GetLocalId();
	if (!Sessions || !LocalId)
	{
		// Nothing to make a session with (no online service): still play, as a plain listen server.
		UE_LOG(LogRobberParty, Warning, TEXT("No session service; going on without a session"));
		AfterCreate = Then;
		OnCreateComplete(NAME_GameSession, true);
		return;
	}

	AfterCreate = Then;
	LastError = FText::GetEmpty();
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &URobberPartySubsystem::OnCreateComplete));

	// Matches what BP_GameInstance's CreateAdvancedSession asked for, except that the room really is 4.
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = bLan;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowJoinViaPresenceFriendsOnly = bFriendsOnly;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = false;
	Settings.bAllowInvites = true;
	Settings.bIsDedicated = false;
	Settings.bUsesStats = false;
	Settings.bAntiCheatProtected = false;
	Sessions->CreateSession(*LocalId, NAME_GameSession, Settings);
}

void URobberPartySubsystem::OnCreateComplete(FName SessionName, bool bOk)
{
	const IOnlineSessionPtr Sessions = GetSessions();
	if (Sessions)
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}
	const EAfterSession Then = AfterCreate;
	AfterCreate = EAfterSession::None;
	if (!bOk)
	{
		UE_LOG(LogRobberParty, Warning, TEXT("Couldn't create the session"));
		LastError = NSLOCTEXT("RobberParty", "CreateFailed", "Couldn't set that up. Check Steam is running and try again.");
		bReopenPlayMenu = false;
		InviteAfterParty.Reset();
		return;
	}
	if (Sessions)
	{
		Sessions->StartSession(NAME_GameSession);
	}
	if (Then == EAfterSession::OpenParty)
	{
		bReopenPlayMenu = true;
		UGameplayStatics::OpenLevel(GetGameInstance()->GetWorld(), MenuMap(), true, TEXT("listen"));
	}
	else if (Then == EAfterSession::OpenGame)
	{
		UGameplayStatics::OpenLevel(GetGameInstance()->GetWorld(), GameMap(), true, TEXT("listen"));
	}
}

void URobberPartySubsystem::DestroySessionThen(TFunction<void()> Then)
{
	const IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions || !HasSession())
	{
		Then();
		return;
	}
	AfterDestroy = MoveTemp(Then);
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &URobberPartySubsystem::OnDestroyComplete));
	Sessions->DestroySession(NAME_GameSession);
}

void URobberPartySubsystem::OnDestroyComplete(FName SessionName, bool bOk)
{
	if (const IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	// Carry on even if it failed: staying stuck in a menu is worse than a stale session.
	if (AfterDestroy)
	{
		TFunction<void()> Then = MoveTemp(AfterDestroy);
		AfterDestroy = nullptr;
		Then();
	}
}

void URobberPartySubsystem::MakeParty(bool bLan, bool bFriendsOnly, FUniqueNetIdPtr InviteAfter)
{
	if (IsInParty())
	{
		return;
	}
	InviteAfterParty = InviteAfter;
	DestroySessionThen([this, bLan, bFriendsOnly]()
	{
		CreateSessionThen(bLan, bFriendsOnly, EAfterSession::OpenParty);
	});
}

void URobberPartySubsystem::InviteFriend(const FRobberFriendInfo& Friend, bool bLan, bool bFriendsOnly)
{
	if (IsMember() || !Friend.Id.IsValid())
	{
		return;
	}
	PendingInvites.Add({ Friend.Name, FPlatformTime::Seconds() });
	if (!IsInParty())
	{
		MakeParty(bLan, bFriendsOnly, Friend.Id);
		return;
	}
	if (const IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->SendSessionInviteToFriend(0, NAME_GameSession, *Friend.Id);
	}
}

void URobberPartySubsystem::KickMember(APlayerState* Player)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	APlayerController* Controller = Player ? Cast<APlayerController>(Player->GetOwningController()) : nullptr;
	if (GameMode && GameMode->GameSession && Controller && !Controller->IsLocalController())
	{
		GameMode->GameSession->KickPlayer(Controller, NSLOCTEXT("RobberParty", "Removed", "The party leader removed you."));
	}
}

void URobberPartySubsystem::OpenMenuPrivately()
{
	PendingInvites.Reset();
	UGameplayStatics::OpenLevel(GetGameInstance()->GetWorld(), MenuMap(), true);
}

void URobberPartySubsystem::LeaveParty()
{
	bReopenPlayMenu = true;
	DestroySessionThen([this]() { OpenMenuPrivately(); });
}

void URobberPartySubsystem::QuitToMenu()
{
	DestroySessionThen([this]() { OpenMenuPrivately(); });
}

void URobberPartySubsystem::CreateGame(bool bLan, bool bFriendsOnly)
{
	if (IsMember())
	{
		return;
	}
	if (IsInParty())
	{
		// The session already exists, and the crew comes along.
		UWorld* World = GetGameInstance()->GetWorld();
#if WITH_EDITOR
		// PIE listens on its own port, but the new map's ?listen uses the default while clients redial the old one.
		if (World->IsPlayInEditor() && World->GetNetDriver() && World->GetNetDriver()->GetLocalAddr().IsValid())
		{
			FURL::UrlConfig.DefaultPort = World->GetNetDriver()->GetLocalAddr()->GetPort();
		}
#endif
		World->ServerTravel(FString(GameMap()) + TEXT("?listen"), true);
		return;
	}
	DestroySessionThen([this, bLan, bFriendsOnly]()
	{
		CreateSessionThen(bLan, bFriendsOnly, EAfterSession::OpenGame);
	});
}

void URobberPartySubsystem::JoinFriend(const FRobberFriendInfo& Friend)
{
	const IOnlineSessionPtr Sessions = GetSessions();
	const FUniqueNetIdPtr LocalId = GetLocalId();
	if (!Sessions || !LocalId || !Friend.Id.IsValid())
	{
		return;
	}
	const FUniqueNetIdRef FriendId = Friend.Id.ToSharedRef();
	DestroySessionThen([this, FriendId, LocalId]()
	{
		if (const IOnlineSessionPtr Interface = GetSessions())
		{
			FindFriendHandle = Interface->AddOnFindFriendSessionCompleteDelegate_Handle(0, FOnFindFriendSessionCompleteDelegate::CreateUObject(this, &URobberPartySubsystem::OnFindFriendComplete));
			Interface->FindFriendSession(*LocalId, *FriendId);
		}
	});
}

void URobberPartySubsystem::OnFindFriendComplete(int32 LocalUser, bool bOk, const TArray<FOnlineSessionSearchResult>& Results)
{
	if (const IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnFindFriendSessionCompleteDelegate_Handle(0, FindFriendHandle);
	}
	if (bOk && Results.Num() > 0)
	{
		JoinResult(Results[0]);
	}
	else
	{
		LastError = NSLOCTEXT("RobberParty", "FriendGone", "Couldn't find that game. They may have just left it.");
	}
}

void URobberPartySubsystem::JoinResult(const FOnlineSessionSearchResult& Result)
{
	const IOnlineSessionPtr Sessions = GetSessions();
	const FUniqueNetIdPtr LocalId = GetLocalId();
	if (!Sessions || !LocalId)
	{
		return;
	}
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &URobberPartySubsystem::OnJoinComplete));
	Sessions->JoinSession(*LocalId, NAME_GameSession, Result);
}

void URobberPartySubsystem::OnJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	const IOnlineSessionPtr Sessions = GetSessions();
	if (Sessions)
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}
	FString Address;
	APlayerController* Controller = GetGameInstance()->GetFirstLocalPlayerController();
	if (Result == EOnJoinSessionCompleteResult::Success && Sessions && Controller && Sessions->GetResolvedConnectString(NAME_GameSession, Address))
	{
		Controller->ClientTravel(Address, TRAVEL_Absolute);
		return;
	}
	UE_LOG(LogRobberParty, Warning, TEXT("Couldn't join: result %d"), static_cast<int32>(Result));
	LastError = Result == EOnJoinSessionCompleteResult::SessionIsFull
		? NSLOCTEXT("RobberParty", "Full", "That crew is full.")
		: NSLOCTEXT("RobberParty", "JoinFailed", "Couldn't join that game.");
}

TArray<FString> URobberPartySubsystem::GetPendingInvites() const
{
	TArray<FString> Names;
	const TArray<FRobberPartyMember> Members = GetMembers();
	for (const FPendingInvite& Invite : PendingInvites)
	{
		const bool bHere = Members.ContainsByPredicate([&Invite](const FRobberPartyMember& Member) { return Member.Name == Invite.Name; });
		if (!bHere && FPlatformTime::Seconds() - Invite.SentTime < RobberPartyDetail::InviteLifetime)
		{
			Names.Add(Invite.Name);
		}
	}
	return Names;
}

bool URobberPartySubsystem::IsInvited(const FString& Name) const
{
	return GetPendingInvites().Contains(Name);
}

void URobberPartySubsystem::OnInviteReceived(const FUniqueNetId& Invited, const FUniqueNetId& Inviter, const FString& AppId, const FOnlineSessionSearchResult& Result)
{
	bHasIncomingInvite = true;
	IncomingInvite = Result;
	IncomingInviteName = Result.Session.OwningUserName;
	UE_LOG(LogRobberParty, Display, TEXT("Party invite from %s"), *IncomingInviteName);
}

void URobberPartySubsystem::AcceptIncomingInvite()
{
	bHasIncomingInvite = false;
	const FOnlineSessionSearchResult Result = IncomingInvite;
	DestroySessionThen([this, Result]() { JoinResult(Result); });
}

void URobberPartySubsystem::DeclineIncomingInvite()
{
	bHasIncomingInvite = false;
}

void URobberPartySubsystem::OnInviteAccepted(bool bOk, int32 ControllerId, FUniqueNetIdPtr User, const FOnlineSessionSearchResult& Result)
{
	// BP_GameInstance's OnSessionInviteAccepted joins it, but that fails while we're in our own session, so clear the way here.
	if (bOk && HasSession())
	{
		DestroySessionThen([this, Result]() { JoinResult(Result); });
	}
}

void URobberPartySubsystem::RefreshFriends()
{
	if (bTestFriends)
	{
		return;
	}
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const IOnlineFriendsPtr FriendsInterface = World ? Online::GetFriendsInterface(World) : nullptr;
	if (!FriendsInterface)
	{
		return;
	}
	FriendsInterface->ReadFriendsList(0, EFriendsLists::ToString(EFriendsLists::Default), FOnReadFriendsListComplete::CreateWeakLambda(this,
		[this](int32 LocalUser, bool bOk, const FString& ListName, const FString& Error)
		{
			const UWorld* CurrentWorld = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
			const IOnlineFriendsPtr Interface = CurrentWorld ? Online::GetFriendsInterface(CurrentWorld) : nullptr;
			if (!bOk || !Interface)
			{
				return;
			}
			TArray<TSharedRef<FOnlineFriend>> Found;
			Interface->GetFriendsList(0, ListName, Found);
			Friends.Reset();
			for (const TSharedRef<FOnlineFriend>& Friend : Found)
			{
				const FOnlineUserPresence& Presence = Friend->GetPresence();
				FRobberFriendInfo Info;
				Info.Name = Friend->GetDisplayName();
				Info.Id = Friend->GetUserId();
				Info.bOnline = Presence.bIsOnline;
				Info.bPlayingThisGame = Presence.bIsPlayingThisGame;
				Info.bJoinable = Presence.bIsJoinable;
				Friends.Add(MoveTemp(Info));
			}
			// Same order as the old list: joinable, in this game, online, offline.
			const auto Rank = [](const FRobberFriendInfo& Info) { return Info.bJoinable ? 0 : Info.bPlayingThisGame ? 1 : Info.bOnline ? 2 : 3; };
			Friends.StableSort([&Rank](const FRobberFriendInfo& A, const FRobberFriendInfo& B) { return Rank(A) < Rank(B); });
		}));
}

bool URobberPartySubsystem::HasFriendsService() const
{
	if (bTestFriends)
	{
		return true;
	}
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World && Online::GetFriendsInterface(World).IsValid();
}

UTexture2D* URobberPartySubsystem::GetAvatar(const FRobberFriendInfo& Friend)
{
	const IOnlineSubsystem* Online = IOnlineSubsystem::Get();
	if (!Friend.Id.IsValid() || !Online || Online->GetSubsystemName() != STEAM_SUBSYSTEM)
	{
		return nullptr;
	}
	// The plugin doesn't export this class to other modules, so it's called the way a Blueprint would.
	UClass* Library = FindObject<UClass>(nullptr, TEXT("/Script/AdvancedSteamSessions.AdvancedSteamFriendsLibrary"));
	UFunction* Function = Library ? Library->FindFunctionByName(TEXT("GetSteamFriendAvatar")) : nullptr;
	const FStructProperty* IdParam = Function ? FindFProperty<FStructProperty>(Function, TEXT("UniqueNetId")) : nullptr;
	const FEnumProperty* ResultParam = Function ? FindFProperty<FEnumProperty>(Function, TEXT("Result")) : nullptr;
	const FEnumProperty* SizeParam = Function ? FindFProperty<FEnumProperty>(Function, TEXT("AvatarSize")) : nullptr;
	const FObjectProperty* ReturnParam = Function ? CastField<FObjectProperty>(Function->GetReturnProperty()) : nullptr;
	if (!IdParam || !ResultParam || !SizeParam || !ReturnParam)
	{
		return nullptr;
	}

	FBPUniqueNetId NetId;
	NetId.SetUniqueNetId(Friend.Id);
	uint8* Params = static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
	Function->InitializeStruct(Params);
	IdParam->CopyCompleteValue(IdParam->ContainerPtrToValuePtr<void>(Params), &NetId);
	SizeParam->GetUnderlyingProperty()->SetIntPropertyValue(SizeParam->ContainerPtrToValuePtr<void>(Params), static_cast<int64>(SteamAvatarSize::SteamAvatar_Medium));
	Library->GetDefaultObject()->ProcessEvent(Function, Params);
	const int64 Result = ResultParam->GetUnderlyingProperty()->GetSignedIntPropertyValue(ResultParam->ContainerPtrToValuePtr<void>(Params));
	UTexture2D* Avatar = Cast<UTexture2D>(ReturnParam->GetObjectPropertyValue_InContainer(Params));
	Function->DestroyStruct(Params);
	return Result == static_cast<int64>(EBlueprintAsyncResultSwitch::OnSuccess) ? Avatar : nullptr;
}

bool URobberPartySubsystem::ConsumeReopenPlayMenu()
{
	const bool bReopen = bReopenPlayMenu;
	bReopenPlayMenu = false;
	return bReopen;
}

void URobberPartySubsystem::OnMapLoaded(UWorld* World)
{
	if (!World || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	// Session interfaces can be per world in PIE, so hook up after each load.
	if (const IOnlineSessionPtr Sessions = GetSessions())
	{
		if (!InviteReceivedHandle.IsValid())
		{
			InviteReceivedHandle = Sessions->AddOnSessionInviteReceivedDelegate_Handle(FOnSessionInviteReceivedDelegate::CreateUObject(this, &URobberPartySubsystem::OnInviteReceived));
			InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &URobberPartySubsystem::OnInviteAccepted));
		}
		if (InviteAfterParty.IsValid() && HasSession())
		{
			Sessions->SendSessionInviteToFriend(0, NAME_GameSession, *InviteAfterParty);
			InviteAfterParty.Reset();
		}
	}
}

#if !UE_BUILD_SHIPPING
int32 URobberPartySubsystem::GetListenPort() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World ? World->URL.Port : 0;
}
#endif
