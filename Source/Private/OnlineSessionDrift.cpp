// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemDriftPrivatePCH.h"
#include "OnlineSessionDrift.h"
#include "OnlineIdentityInterface.h"
#include "OnlineSubsystemDrift.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineAsyncTaskManagerDrift.h"

#include "DriftAPI.h"

#include "Interfaces/VoiceInterface.h"
#include "Containers/UnrealString.h"


FOnlineSessionInfoDrift::FOnlineSessionInfoDrift()
: SessionId{ TEXT("INVALID") }
{
}

void FOnlineSessionInfoDrift::Init(const FOnlineSubsystemDrift& Subsystem)
{
    FGuid OwnerGuid;
    FPlatformMisc::CreateGuid(OwnerGuid);
    SessionId = FUniqueNetIdString(OwnerGuid.ToString());
}

/**
 *    Async task for ending a Drift online session
 */
class FOnlineAsyncTaskDriftEndSession : public FOnlineAsyncTaskBasic<FOnlineSubsystemDrift>
{
private:
    /** Name of session ending */
    FName SessionName;

public:
    FOnlineAsyncTaskDriftEndSession(class FOnlineSubsystemDrift* InSubsystem, FName InSessionName) :
        FOnlineAsyncTaskBasic(InSubsystem),
        SessionName(InSessionName)
    {
    }

    ~FOnlineAsyncTaskDriftEndSession()
    {
    }

    /**
     *    Get a human readable description of task
     */
    virtual FString ToString() const override
    {
        return FString::Printf(TEXT("FOnlineAsyncTaskDriftEndSession bWasSuccessful: %d SessionName: %s"), WasSuccessful(), *SessionName.ToString());
    }

    /**
     * Give the async task time to do its work
     * Can only be called on the async task manager thread
     */
    virtual void Tick() override
    {
        bIsComplete = true;
        bWasSuccessful = true;
    }

    /**
     * Give the async task a chance to marshal its data back to the game thread
     * Can only be called on the game thread by the async task manager
     */
    virtual void Finalize() override
    {
        IOnlineSessionPtr SessionInt = Subsystem->GetSessionInterface();
        FNamedOnlineSession* Session = SessionInt->GetNamedSession(SessionName);
        if (Session)
        {
            Session->SessionState = EOnlineSessionState::Ended;
        }
    }

    /**
     *    Async task is given a chance to trigger it's delegates
     */
    virtual void TriggerDelegates() override
    {
        IOnlineSessionPtr SessionInt = Subsystem->GetSessionInterface();
        if (SessionInt.IsValid())
        {
            SessionInt->TriggerOnEndSessionCompleteDelegates(SessionName, bWasSuccessful);
        }
    }
};

/**
 *    Async task for destroying a Drift online session
 */
class FOnlineAsyncTaskDriftDestroySession : public FOnlineAsyncTaskBasic<FOnlineSubsystemDrift>
{
private:
    /** Name of session ending */
    FName SessionName;

public:
    FOnlineAsyncTaskDriftDestroySession(class FOnlineSubsystemDrift* InSubsystem, FName InSessionName) :
        FOnlineAsyncTaskBasic(InSubsystem),
        SessionName(InSessionName)
    {
    }

    ~FOnlineAsyncTaskDriftDestroySession()
    {
    }

    /**
     *    Get a human readable description of task
     */
    virtual FString ToString() const override
    {
        return FString::Printf(TEXT("FOnlineAsyncTaskDriftDestroySession bWasSuccessful: %d SessionName: %s"), WasSuccessful(), *SessionName.ToString());
    }

    /**
     * Give the async task time to do its work
     * Can only be called on the async task manager thread
     */
    virtual void Tick() override
    {
        bIsComplete = true;
        bWasSuccessful = true;
    }

    /**
     * Give the async task a chance to marshal its data back to the game thread
     * Can only be called on the game thread by the async task manager
     */
    virtual void Finalize() override
    {
        IOnlineSessionPtr SessionInt = Subsystem->GetSessionInterface();
        if (SessionInt.IsValid())
        {
            FNamedOnlineSession* Session = SessionInt->GetNamedSession(SessionName);
            if (Session)
            {
                SessionInt->RemoveNamedSession(SessionName);
            }
        }
    }

    /**
     *    Async task is given a chance to trigger it's delegates
     */
    virtual void TriggerDelegates() override
    {
        IOnlineSessionPtr SessionInt = Subsystem->GetSessionInterface();
        if (SessionInt.IsValid())
        {
            SessionInt->TriggerOnDestroySessionCompleteDelegates(SessionName, bWasSuccessful);
        }
    }
};

FNamedOnlineSession* FOnlineSessionDrift::AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings)
{
    FScopeLock ScopeLock(&SessionLock);
    return new (Sessions) FNamedOnlineSession(SessionName, SessionSettings);
}

class FNamedOnlineSession* FOnlineSessionDrift::AddNamedSession(FName SessionName, const FOnlineSession& Session)
{
    FScopeLock ScopeLock(&SessionLock);
    return new (Sessions) FNamedOnlineSession(SessionName, Session);
}


#if UE_VERSION_NEWER_THAN(4, 20, 0)
TSharedPtr<const FUniqueNetId> FOnlineSessionDrift::CreateSessionIdFromString(const FString& SessionIdStr)
{
    return {};
}
#endif // UE_VERSION_NEWER_THAN(4, 20, 0)


FNamedOnlineSession* FOnlineSessionDrift::GetNamedSession(FName SessionName)
{
    FScopeLock ScopeLock(&SessionLock);
    for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
    {
        if (Sessions[SearchIndex].SessionName == SessionName)
        {
            return &Sessions[SearchIndex];
        }
    }
    return nullptr;
}

void FOnlineSessionDrift::RemoveNamedSession(FName SessionName)
{
    FScopeLock ScopeLock(&SessionLock);
    for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
    {
        if (Sessions[SearchIndex].SessionName == SessionName)
        {
            Sessions.RemoveAtSwap(SearchIndex);
            return;
        }
    }
}

EOnlineSessionState::Type FOnlineSessionDrift::GetSessionState(FName SessionName) const
{
    FScopeLock ScopeLock(&SessionLock);
    for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
    {
        if (Sessions[SearchIndex].SessionName == SessionName)
        {
            return Sessions[SearchIndex].SessionState;
        }
    }

    return EOnlineSessionState::NoSession;
}

bool FOnlineSessionDrift::HasPresenceSession()
{
    FScopeLock ScopeLock(&SessionLock);
    for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
    {
        if (Sessions[SearchIndex].SessionSettings.bUsesPresence)
        {
            return true;
        }
    }

    return false;
}

bool FOnlineSessionDrift::CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
    uint32 Result = E_FAIL;

    FNamedOnlineSession* Session = GetNamedSession(SessionName);
    if (Session == nullptr)
    {
        Session = AddNamedSession(SessionName, NewSessionSettings);
        check(Session);
        Session->SessionState = EOnlineSessionState::Creating;
        Session->NumOpenPrivateConnections = NewSessionSettings.NumPrivateConnections;
        Session->NumOpenPublicConnections = NewSessionSettings.NumPublicConnections;

        Session->HostingPlayerNum = HostingPlayerNum;

        check(DriftSubsystem);

        Session->SessionSettings.BuildUniqueId = GetBuildUniqueId();

        FOnlineSessionInfoDrift* NewSessionInfo = new FOnlineSessionInfoDrift();
        NewSessionInfo->Init(*DriftSubsystem);
        Session->SessionInfo = MakeShareable(NewSessionInfo);
        
        if (auto Drift = DriftSubsystem->GetDrift())
        {
            onMatchAddedDelegateHandle = Drift->OnMatchAdded().AddRaw(this, &FOnlineSessionDrift::OnMatchAdded);
            FString mapName;
            Session->SessionSettings.Get(TEXT("map_name"), mapName);
            FString gameMode;
            Session->SessionSettings.Get(TEXT("game_mode"), gameMode);
            int32 numTeams{ 1 };
            Session->SessionSettings.Get(TEXT("num_teams"), numTeams);
            Drift->AddMatch(mapName, gameMode, numTeams, Session->NumOpenPublicConnections);
            Result = ERROR_IO_PENDING;
        }

        if (Result != ERROR_IO_PENDING)
        {
            Session->SessionState = EOnlineSessionState::Pending;

            if (Result != ERROR_SUCCESS)
            {
                RemoveNamedSession(SessionName);
            }
            else
            {
                RegisterLocalPlayers(Session);
            }
        }
    }
    else
    {
        UE_LOG_ONLINE(Warning, TEXT("Cannot create session '%s': session already exists."), *SessionName.ToString());
    }

    if (Result != ERROR_IO_PENDING)
    {
        TriggerOnCreateSessionCompleteDelegates(SessionName, (Result == ERROR_SUCCESS) ? true : false);
    }
    
    return Result == ERROR_IO_PENDING || Result == ERROR_SUCCESS;
}

bool FOnlineSessionDrift::CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
    // TODO: use proper HostingPlayerId
    return CreateSession(0, SessionName, NewSessionSettings);
}


void FOnlineSessionDrift::OnMatchAdded(bool success)
{
    auto Session = GetNamedSession(GameSessionName);
    if (Session)
    {
        Session->SessionState = EOnlineSessionState::Pending;
    }
    if (auto Drift = DriftSubsystem->GetDrift())
    {
        Drift->OnMatchAdded().Remove(onMatchAddedDelegateHandle);
    }
    onMatchAddedDelegateHandle.Reset();
    TriggerOnCreateSessionCompleteDelegates(GameSessionName, (Session != nullptr) && success);
}


bool FOnlineSessionDrift::StartSession(FName SessionName)
{
    uint32 Result = E_FAIL;

    auto Session = GetNamedSession(SessionName);
    if (Session)
    {
        if (Session->SessionState == EOnlineSessionState::Pending ||
            Session->SessionState == EOnlineSessionState::Ended)
        {
            Session->SessionState = EOnlineSessionState::InProgress;
            if (auto Drift = DriftSubsystem->GetDrift())
            {
                Drift->UpdateServer(TEXT("running"), TEXT(""), FDriftServerStatusUpdatedDelegate{});
                Drift->UpdateMatch(TEXT("started"), TEXT(""), FDriftMatchStatusUpdatedDelegate{});
            }
        }
        else
        {
            UE_LOG_ONLINE(Warning, TEXT("Can't start an online session (%s) in state %s"),
                *SessionName.ToString(),
                EOnlineSessionState::ToString(Session->SessionState))
;
        }
    }
    else
    {
        UE_LOG_ONLINE(Warning, TEXT("Can't start an online game for session (%s) that hasn't been created"), *SessionName.ToString());
    }

    if (Result != ERROR_IO_PENDING)
    {
        TriggerOnStartSessionCompleteDelegates(SessionName, (Result == ERROR_SUCCESS) ? true : false);
    }

    return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
}

bool FOnlineSessionDrift::UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData)
{
    bool bWasSuccessful = true;

    auto Session = GetNamedSession(SessionName);
    if (Session)
    {
        // TODO: What should this do?
        Session->SessionSettings = UpdatedSessionSettings;
        TriggerOnUpdateSessionCompleteDelegates(SessionName, bWasSuccessful);
    }

    return bWasSuccessful;
}

bool FOnlineSessionDrift::EndSession(FName SessionName)
{
    uint32 Result = E_FAIL;

    FNamedOnlineSession* Session = GetNamedSession(SessionName);
    if (Session)
    {
        // Can't end a match that isn't in progress
        if (Session->SessionState == EOnlineSessionState::InProgress)
        {
            Session->SessionState = EOnlineSessionState::Ended;
            if (IsRunningDedicatedServer())
            {
                if (auto Drift = DriftSubsystem->GetDrift())
                {
                    Drift->UpdateMatch(TEXT("ended"), TEXT(""), FDriftMatchStatusUpdatedDelegate{});
                }
            }
        }
        else
        {
            UE_LOG_ONLINE(Warning, TEXT("Can't end session (%s) in state %s"),
                *SessionName.ToString(),
                EOnlineSessionState::ToString(Session->SessionState))
;
        }
    }
    else
    {
        UE_LOG_ONLINE(Warning, TEXT("Can't end an online game for session (%s) that hasn't been created"),
            *SessionName.ToString());
    }

    if (Result != ERROR_IO_PENDING)
    {
        if (Session)
        {
            Session->SessionState = EOnlineSessionState::Ended;
        }

        TriggerOnEndSessionCompleteDelegates(SessionName, (Result == ERROR_SUCCESS) ? true : false);
    }

    return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
}

bool FOnlineSessionDrift::DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
    uint32 Result = E_FAIL;
    // Find the session in question
    FNamedOnlineSession* Session = GetNamedSession(SessionName);
    if (Session)
    {
        // The session info is no longer needed
        RemoveNamedSession(Session->SessionName);
        if (IsRunningDedicatedServer())
        {
            if (auto Drift = DriftSubsystem->GetDrift())
            {
                Drift->UpdateMatch(TEXT("completed"), TEXT(""), FDriftMatchStatusUpdatedDelegate::CreateLambda([this, CompletionDelegate, SessionName](bool success)
                {
                    CompletionDelegate.ExecuteIfBound(SessionName, success);
                    TriggerOnDestroySessionCompleteDelegates(SessionName, success);
                }));
                Result = ERROR_IO_PENDING;
            }
        }
    }
    else
    {
        UE_LOG_ONLINE(Warning, TEXT("Can't destroy a null online session (%s)"), *SessionName.ToString());
    }

    if (Result != ERROR_IO_PENDING)
    {
        CompletionDelegate.ExecuteIfBound(SessionName, (Result == ERROR_SUCCESS) ? true : false);
        TriggerOnDestroySessionCompleteDelegates(SessionName, (Result == ERROR_SUCCESS) ? true : false);
    }

    return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
}

bool FOnlineSessionDrift::IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId)
{
    return IsPlayerInSessionImpl(this, SessionName, UniqueId);
}

bool FOnlineSessionDrift::StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
    /**
     * This bastardizes match making to some extent as the invites and accepts also have to use this system,
     * unless we rewrite the backend to deal with sessions in some other way, or Epic alters the interface.
     */

    // TODO: Consider at least some of the settings passed in

    CurrentSearch.Reset();
    SearchSettings->SearchResults.Empty();

    if (auto drift = DriftSubsystem->GetDrift())
    {
        SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;
        CurrentSessionSearch = SearchSettings;
        CurrentSessionSearchName = SessionName;
        FString friendId;
        FString token;
        if (SearchSettings->QuerySettings.Get(TEXT("friend_id"), friendId))
        {
            FUniqueNetIdDrift driftId{ friendId };
            drift->InvitePlayerToMatch(driftId.GetId(), FDriftJoinedMatchQueueDelegate::CreateRaw(this, &FOnlineSessionDrift::OnJoinedMatchQueue));
        }
        else if (SearchSettings->QuerySettings.Get(TEXT("invite_token"), token))
        {
            FMatchInvite invite;
            invite.token = token;
            drift->AcceptMatchInvite(invite, FDriftJoinedMatchQueueDelegate::CreateRaw(this, &FOnlineSessionDrift::OnJoinedMatchQueue));
        }
        else
        {
            drift->JoinMatchQueue(FDriftJoinedMatchQueueDelegate::CreateRaw(this, &FOnlineSessionDrift::OnJoinedMatchQueue));
        }
        return true;
    }

    UE_LOG_ONLINE(Warning, TEXT("Failed to initiate matchmaking."));

    return false;
}


void FOnlineSessionDrift::OnJoinedMatchQueue(bool success, const FMatchQueueStatus& status)
{
    if (success)
    {
        CurrentSearch = MakeShareable(new FMatchQueueSearch(DriftSubsystem));
        CurrentSearch->OnMatchQueueStatusChanged().AddRaw(this, &FOnlineSessionDrift::OnMatchSearchStatusChanged);
    }
    else
    {
        CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
        if (CurrentSessionSearch.IsValid())
        {
            CurrentSessionSearch = nullptr;
        }
        TriggerOnMatchmakingCompleteDelegates(CurrentSessionSearchName, false);
    }
}


void FOnlineSessionDrift::OnMatchSearchStatusChanged(FName status)
{
    if (CurrentSessionSearch.IsValid())
    {
        if (status == TEXT("matched"))
        {
            auto NewResult = new (CurrentSessionSearch->SearchResults) FOnlineSessionSearchResult{};
            auto& NewSession = NewResult->Session;
            auto DriftSessionInfo = new FOnlineSessionInfoDrift{};
            NewSession.SessionInfo = MakeShareable(DriftSessionInfo);
            auto match = CurrentSearch->GetCurrentMatch();
            DriftSessionInfo->Url = match.ue4_connection_url;
            auto& SessionSettings = NewSession.SessionSettings;
            SessionSettings.bAllowInvites = false;
            SessionSettings.bAllowJoinInProgress = false;
            SessionSettings.bAllowJoinViaPresence = false;
            SessionSettings.bAllowJoinViaPresenceFriendsOnly = false;
            SessionSettings.bAntiCheatProtected = false;
            SessionSettings.bIsDedicated = true;
            SessionSettings.bIsLANMatch = false;
            SessionSettings.bShouldAdvertise = false;
            SessionSettings.BuildUniqueId = 0;
            SessionSettings.bUsesPresence = false;
            SessionSettings.bUsesStats = false;
            SessionSettings.NumPrivateConnections = 0;
            SessionSettings.NumPublicConnections = 2;   // TODO: Fill in from result
            SessionSettings.Set(TEXT("match_id"), match.match_id, EOnlineDataAdvertisementType::Type::DontAdvertise);

            CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Done;
            CurrentSessionSearch.Reset();
            CurrentSearch.Reset();

            TriggerOnMatchmakingCompleteDelegates(CurrentSessionSearchName, true);
        }
        else if (status == TEXT("timedout") || status == TEXT("usurped"))
        {
            CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
            CurrentSessionSearch.Reset();
            CurrentSearch.Reset();

            TriggerOnMatchmakingCompleteDelegates(CurrentSessionSearchName, false);
        }
    }
}

bool FOnlineSessionDrift::CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName)
{
    if (!CurrentSessionSearch.IsValid())
    {
        TriggerOnCancelMatchmakingCompleteDelegates(SessionName, true);
        return true;
    }

    if (auto drift = DriftSubsystem->GetDrift())
    {
        const auto matchQueueState = drift->GetMatchQueueState();
		if (matchQueueState == EMatchQueueState::Queued || matchQueueState == EMatchQueueState::Updating)
		{
			drift->LeaveMatchQueue(FDriftLeftMatchQueueDelegate::CreateLambda([this, SessionName](bool success)
			{
				if (success)
				{
					CurrentSearch.Reset();
					if (CurrentSessionSearch.IsValid())
					{
						CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
						CurrentSessionSearch.Reset();
					}
					TriggerOnCancelMatchmakingCompleteDelegates(SessionName, true);
				}
				else
				{
					TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
					return;
				}
			}));
		}
		else
		{
			TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
		}
        return true;
    }

    UE_LOG_ONLINE(Warning, TEXT("Failed to cancel matchmaking."));

    TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);

    return false;
}

bool FOnlineSessionDrift::CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName)
{
    return CancelMatchmaking(0, SessionName);
}

bool FOnlineSessionDrift::FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
    uint32 Return = E_FAIL;

    if (!CurrentSessionSearch.IsValid() && SearchSettings->SearchState != EOnlineAsyncTaskState::InProgress)
    {
        SearchSettings->SearchResults.Empty();
        CurrentSessionSearch = SearchSettings;

        // TODO: Actually use the search settings for something

        if (auto drift = DriftSubsystem->GetDrift())
        {
            drift->OnGotActiveMatches().AddRaw(this, &FOnlineSessionDrift::OnGotActiveMatches);
            DriftSearch = MakeShareable(new FMatchesSearch{});
            auto temp = DriftSearch.ToSharedRef();
            drift->GetActiveMatches(temp);
            Return = ERROR_IO_PENDING;
        }

        if (Return == ERROR_IO_PENDING)
        {
            SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;
        }
    }
    else
    {
        UE_LOG_ONLINE(Warning, TEXT("Ignoring game search request while one is pending"));
        Return = ERROR_IO_PENDING;
    }

    return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

bool FOnlineSessionDrift::FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
    // This function doesn't use the SearchingPlayerNum parameter, so passing in anything is fine.
    return FindSessions(0, SearchSettings);
}

bool FOnlineSessionDrift::FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegates)
{
    FOnlineSessionSearchResult EmptyResult;
    CompletionDelegates.ExecuteIfBound(0, false, EmptyResult);
    return true;
}

bool FOnlineSessionDrift::CancelFindSessions()
{
    uint32 Return = E_FAIL;
    if (CurrentSessionSearch.IsValid() && CurrentSessionSearch->SearchState == EOnlineAsyncTaskState::InProgress)
    {
        // Make sure it's the right type
        Return = ERROR_SUCCESS;

        CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
        CurrentSessionSearch = nullptr;
    }
    else
    {
        UE_LOG_ONLINE(Warning, TEXT("Can't cancel a search that isn't in progress"));
    }

    if (Return != ERROR_IO_PENDING)
    {
        TriggerOnCancelFindSessionsCompleteDelegates(true);
    }

    return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

void FOnlineSessionDrift::OnGotActiveMatches(bool success)
{
    if (DriftSearch.IsValid())
    {
        if (CurrentSessionSearch.IsValid())
        {
            for (const auto& activeMatch : DriftSearch->matches)
            {
                auto NewResult = new (CurrentSessionSearch->SearchResults) FOnlineSessionSearchResult{};
                auto& NewSession = NewResult->Session;
                auto DriftSessionInfo = new FOnlineSessionInfoDrift{};
                NewSession.SessionInfo = MakeShareable(DriftSessionInfo);
                DriftSessionInfo->Url = activeMatch.ue4_connection_url;
                auto& SessionSettings = NewSession.SessionSettings;
                SessionSettings.bAllowInvites = false;
                SessionSettings.bAllowJoinInProgress = false;
                SessionSettings.bAllowJoinViaPresence = false;
                SessionSettings.bAllowJoinViaPresenceFriendsOnly = false;
                SessionSettings.bAntiCheatProtected = false;
                SessionSettings.bIsDedicated = true;
                SessionSettings.bIsLANMatch = false;
                SessionSettings.bShouldAdvertise = false;
                SessionSettings.BuildUniqueId = 0;
                SessionSettings.bUsesPresence = false;
                SessionSettings.bUsesStats = false;
                SessionSettings.NumPrivateConnections = 0;
                SessionSettings.NumPublicConnections = activeMatch.max_players;
                NewSession.NumOpenPrivateConnections = 0;
                NewSession.NumOpenPublicConnections = activeMatch.max_players - activeMatch.num_players;
                SessionSettings.Set(TEXT("match_id"), activeMatch.match_id, EOnlineDataAdvertisementType::Type::DontAdvertise);
            }
            CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Done;
            CurrentSessionSearch.Reset();
        }
    }
    TriggerOnFindSessionsCompleteDelegates(success);
}


FOnlineSessionDrift::~FOnlineSessionDrift()
{
    if (auto drift = DriftSubsystem->GetDrift())
    {
        drift->OnGotActiveMatches().RemoveAll(this);
    }
}


bool FOnlineSessionDrift::JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
    uint32 Return = E_FAIL;
    auto Session = GetNamedSession(SessionName);
    if (Session == nullptr)
    {
        Session = AddNamedSession(SessionName, DesiredSession.Session);
        Session->HostingPlayerNum = PlayerNum;

        auto SessionInfo = new FOnlineSessionInfoDrift{};
        Session->SessionInfo = MakeShareable(SessionInfo);
        const auto DesiredSessionInfo = static_cast<FOnlineSessionInfoDrift*>(DesiredSession.Session.SessionInfo.Get());
        SessionInfo->Url = DesiredSessionInfo->Url;

        Session->SessionSettings.bShouldAdvertise = false;

        if (auto drift = DriftSubsystem->GetDrift())
        {
            drift->ResetMatchQueue();
        }

        RegisterLocalPlayers(Session);
        Return = ERROR_SUCCESS;
    }
    else
    {
        UE_LOG_ONLINE(Warning, TEXT("Session (%s) already exists, can't join twice"), *SessionName.ToString());
    }

    if (Return != ERROR_IO_PENDING)
    {
        // Just trigger the delegate as having failed
        TriggerOnJoinSessionCompleteDelegates(SessionName, Return == ERROR_SUCCESS ? EOnJoinSessionCompleteResult::Success : EOnJoinSessionCompleteResult::UnknownError);
    }

    return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

bool FOnlineSessionDrift::JoinSession(const FUniqueNetId& PlayerId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
    // Assuming player 0 should be OK here
    return JoinSession(0, SessionName, DesiredSession);
}

bool FOnlineSessionDrift::FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend)
{
    // this function has to exist due to interface definition, but it does not have a meaningful implementation in Drift subsystem
    TArray<FOnlineSessionSearchResult> EmptySearchResult;
    TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, EmptySearchResult);
    return false;
};

bool FOnlineSessionDrift::FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend)
{
    // this function has to exist due to interface definition, but it does not have a meaningful implementation in Drift subsystem
    TArray<FOnlineSessionSearchResult> EmptySearchResult;
    TriggerOnFindFriendSessionCompleteDelegates(0, false, EmptySearchResult);
    return false;
}


bool FOnlineSessionDrift::FindFriendSession(const FUniqueNetId & LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList)
{
    // this function has to exist due to interface definition, but it does not have a meaningful implementation in Drift subsystem
    TArray<FOnlineSessionSearchResult> EmptySearchResult;
    TriggerOnFindFriendSessionCompleteDelegates(0, false, EmptySearchResult);
    return false;
}


bool FOnlineSessionDrift::SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend)
{
    // this function has to exist due to interface definition, but it does not have a meaningful implementation in Drift subsystem
    return false;
};

bool FOnlineSessionDrift::SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend)
{
    // this function has to exist due to interface definition, but it does not have a meaningful implementation in Drift subsystem
    return false;
}

bool FOnlineSessionDrift::SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
    // this function has to exist due to interface definition, but it does not have a meaningful implementation in Drift subsystem
    return false;
};

bool FOnlineSessionDrift::SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
    // this function has to exist due to interface definition, but it does not have a meaningful implementation in Drift subsystem
    return false;
}

bool FOnlineSessionDrift::PingSearchResults(const FOnlineSessionSearchResult& SearchResult)
{
    return false;
}

/** Get a resolved connection string from a session info */
static bool GetConnectStringFromSessionInfo(TSharedPtr<FOnlineSessionInfoDrift>& SessionInfo, FString& ConnectInfo)
{
    bool bSuccess = false;
    if (SessionInfo.IsValid())
    {
        if (!SessionInfo->Url.IsEmpty())
        {
            ConnectInfo = SessionInfo->Url;
            bSuccess = true;
        }
    }

    return bSuccess;
}

bool FOnlineSessionDrift::GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType)
{
    bool bSuccess = false;
    // Find the session
    FNamedOnlineSession* Session = GetNamedSession(SessionName);
    if (Session != nullptr)
    {
        TSharedPtr<FOnlineSessionInfoDrift> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoDrift>(Session->SessionInfo);
        bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo);
        if (!bSuccess)
        {
            UE_LOG_ONLINE(Warning, TEXT("Invalid session info for session %s in GetResolvedConnectString()"), *SessionName.ToString());
        }
    }
    else
    {
        UE_LOG_ONLINE(Warning,
            TEXT("Unknown session name (%s) specified to GetResolvedConnectString()"),
            *SessionName.ToString());
    }

    return bSuccess;
}

bool FOnlineSessionDrift::GetResolvedConnectString(const class FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo)
{
    bool bSuccess = false;
    if (SearchResult.Session.SessionInfo.IsValid())
    {
        TSharedPtr<FOnlineSessionInfoDrift> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoDrift>(SearchResult.Session.SessionInfo);
        bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo);
    }
    
    if (!bSuccess || ConnectInfo.IsEmpty())
    {
        UE_LOG_ONLINE(Warning, TEXT("Invalid session info in search result to GetResolvedConnectString()"));
    }

    return bSuccess;
}

FOnlineSessionSettings* FOnlineSessionDrift::GetSessionSettings(FName SessionName) 
{
    FNamedOnlineSession* Session = GetNamedSession(SessionName);
    if (Session)
    {
        return &Session->SessionSettings;
    }
    return nullptr;
}

void FOnlineSessionDrift::RegisterLocalPlayers(FNamedOnlineSession* Session)
{
    if (!DriftSubsystem->IsDedicated())
    {
        IOnlineVoicePtr VoiceInt = DriftSubsystem->GetVoiceInterface();
        if (VoiceInt.IsValid())
        {
            for (int32 Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
            {
                // Register the local player as a local talker
                VoiceInt->RegisterLocalTalker(Index);
            }
        }
    }
}

void FOnlineSessionDrift::RegisterVoice(const FUniqueNetId& PlayerId)
{
    IOnlineVoicePtr VoiceInt = DriftSubsystem->GetVoiceInterface();
    if (VoiceInt.IsValid())
    {
        if (!DriftSubsystem->IsLocalPlayer(PlayerId))
        {
            VoiceInt->RegisterRemoteTalker(PlayerId);
        }
        else
        {
            // This is a local player. In case their PlayerState came last during replication, reprocess muting
            VoiceInt->ProcessMuteChangeNotification();
        }
    }
}

void FOnlineSessionDrift::UnregisterVoice(const FUniqueNetId& PlayerId)
{
    IOnlineVoicePtr VoiceInt = DriftSubsystem->GetVoiceInterface();
    if (VoiceInt.IsValid())
    {
        if (!DriftSubsystem->IsLocalPlayer(PlayerId))
        {
            if (VoiceInt.IsValid())
            {
                VoiceInt->UnregisterRemoteTalker(PlayerId);
            }
        }
    }
}

bool FOnlineSessionDrift::RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited)
{
    TArray< TSharedRef<const FUniqueNetId> > Players;
    Players.Add(MakeShareable(new FUniqueNetIdDrift(PlayerId)));
    return RegisterPlayers(SessionName, Players, bWasInvited);
}

bool FOnlineSessionDrift::RegisterPlayers(FName SessionName, const TArray<TSharedRef<const FUniqueNetId>>& Players, bool bWasInvited)
{
    bool bSuccess = false;
    FNamedOnlineSession* Session = GetNamedSession(SessionName);
    if (Session)
    {
        bSuccess = true;

        for (const auto& PlayerId : Players)
        {
            FUniqueNetIdMatcher PlayerMatch(*PlayerId);
            if (Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch) == INDEX_NONE)
            {
                Session->RegisteredPlayers.Add(PlayerId);
                RegisterVoice(*PlayerId);

                if (IsRunningDedicatedServer())
                {
                    if (auto Drift = DriftSubsystem->GetDrift())
                    {
                        Drift->AddPlayerToMatch(FUniqueNetIdDrift{ *PlayerId }.GetId(), 0, FDriftPlayerAddedDelegate::CreateLambda([this, SessionName, Session, Players](bool success)
                        {
                            if (success)
                            {

                            }
                            else
                            {
                                UE_LOG_ONLINE(Warning, TEXT("Failed to register player with Drift session"));
                            }
                            TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, success);
                        }));
                    }
                }

                if (Session->NumOpenPublicConnections > 0)
                {
                    Session->NumOpenPublicConnections--;
                }
                else if (Session->NumOpenPrivateConnections > 0)
                {
                    Session->NumOpenPrivateConnections--;
                }
                return bSuccess;
            }
            else
            {
                RegisterVoice(*PlayerId);
                UE_LOG_ONLINE(Log, TEXT("Player %s already registered in session %s"), *PlayerId->ToDebugString(), *SessionName.ToString());
            }            
        }
    }
    else
    {
        UE_LOG_ONLINE(Warning, TEXT("No game present to join for session (%s)"), *SessionName.ToString());
    }

    TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, bSuccess);
    return bSuccess;
}

bool FOnlineSessionDrift::UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId)
{
    TArray< TSharedRef<const FUniqueNetId> > Players;
    Players.Add(MakeShareable(new FUniqueNetIdDrift(PlayerId)));
    return UnregisterPlayers(SessionName, Players);
}

bool FOnlineSessionDrift::UnregisterPlayers(FName SessionName, const TArray<TSharedRef<const FUniqueNetId>>& Players)
{
    bool bSuccess = true;

    FNamedOnlineSession* Session = GetNamedSession(SessionName);
    if (Session)
    {
        for (const auto& PlayerId : Players)
        {
            FUniqueNetIdMatcher PlayerMatch(*PlayerId);
            int32 RegistrantIndex = Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch);
            if (RegistrantIndex != INDEX_NONE)
            {
                Session->RegisteredPlayers.RemoveAtSwap(RegistrantIndex);
                UnregisterVoice(*PlayerId);

                if (IsRunningDedicatedServer())
                {
                    if (auto Drift = DriftSubsystem->GetDrift())
                    {
                        Drift->RemovePlayerFromMatch(FUniqueNetIdDrift{ *PlayerId }.GetId(), FDriftPlayerRemovedDelegate::CreateLambda([this, SessionName, Session, Players](bool success)
                        {
                            if (success)
                            {

                            }
                            else
                            {
                                UE_LOG_ONLINE(Warning, TEXT("Failed to unregister player with Drift session"));
                            }
                            TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, success);
                        }));
                    }
                }

                if (Session->NumOpenPublicConnections < Session->SessionSettings.NumPublicConnections)
                {
                    Session->NumOpenPublicConnections++;
                }
                else if (Session->NumOpenPrivateConnections < Session->SessionSettings.NumPrivateConnections)
                {
                    Session->NumOpenPrivateConnections++;
                }
                return bSuccess;
            }
            else
            {
                UE_LOG_ONLINE(Warning, TEXT("Player %s is not part of session (%s)"), *PlayerId->ToDebugString(), *SessionName.ToString());
            }
        }
    }
    else
    {
        UE_LOG_ONLINE(Warning, TEXT("No game present to leave for session (%s)"), *SessionName.ToString());
        bSuccess = false;
    }

    TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, bSuccess);
    return bSuccess;
}

void FOnlineSessionDrift::Tick(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_Session_Interface);

    if (CurrentSearch.IsValid())
    {
        CurrentSearch->Tick(DeltaTime);
    }
}

int32 FOnlineSessionDrift::GetNumSessions()
{
    FScopeLock ScopeLock(&SessionLock);
    return Sessions.Num();
}

void FOnlineSessionDrift::DumpSessionState()
{
    FScopeLock ScopeLock(&SessionLock);

    for (int32 SessionIdx=0; SessionIdx < Sessions.Num(); SessionIdx++)
    {
        DumpNamedSession(&Sessions[SessionIdx]);
    }
}

void FOnlineSessionDrift::RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate)
{
    Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::Success);
}

void FOnlineSessionDrift::UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate)
{
    Delegate.ExecuteIfBound(PlayerId, true);
}


FMatchQueueSearch::FMatchQueueSearch(FOnlineSubsystemDrift* subsystem)
    : DriftSubsystem(subsystem)
{
}

void FMatchQueueSearch::Tick(float deltaTime)
{
    if (delay > 0.0f)
    {
        delay -= deltaTime;
    }

    if (queueStatus != TEXT("matched") && delay < 0.0f && !isPolling)
    {
        PollQueue();
    }
}

void FMatchQueueSearch::PollQueue()
{
    if (auto drift = DriftSubsystem->GetDrift())
    {
        isPolling = true;
        drift->PollMatchQueue(FDriftPolledMatchQueueDelegate::CreateSP(this, &FMatchQueueSearch::OnPollQueueComplete));
    }
}

void FMatchQueueSearch::OnPollQueueComplete(bool success, const FMatchQueueStatus& status)
{
    isPolling = false;
    delay = POLL_FREQUENCY;
    if (success)
    {
        const FName oldStatus{ queueStatus };
        if (status.status == FName(TEXT("waiting")))
        {
            currentMatch = FMatchQueueMatch{};
        }
        else if (status.status == FName(TEXT("matched")))
        {
            currentMatch = status.match;
        }
        else if (status.status == FName(TEXT("timedout")))
        {
            currentMatch = FMatchQueueMatch{};
        }
        else if (status.status == FName(TEXT("usurped")))
        {
            currentMatch = FMatchQueueMatch{};
        }

        if (oldStatus != status.status)
        {
            onMatchQueueStatusChanged.Broadcast(status.status);
        }
    }
}
