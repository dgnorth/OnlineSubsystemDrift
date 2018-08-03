// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/EngineVersionComparison.h"
#include "OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemDriftTypes.h"
#include "OnlineSubsystemDriftPackage.h"

#include "DriftAPI.h"


class FOnlineSubsystemDrift;

DECLARE_MULTICAST_DELEGATE_OneParam(FMatchQueueStatusChangedDelegate, FName);


class FMatchQueueSearch : public TSharedFromThis<FMatchQueueSearch>
{
public:
    FMatchQueueSearch(FOnlineSubsystemDrift* subsystem);
    void Tick(float deltaTime);

    FMatchQueueStatusChangedDelegate& OnMatchQueueStatusChanged() { return onMatchQueueStatusChanged;  }

    const FMatchQueueMatch& GetCurrentMatch() { return currentMatch; }

private:
    void PollQueue();
    void OnPollQueueComplete(bool success, const FMatchQueueStatus& status);

    FMatchQueueStatusChangedDelegate onMatchQueueStatusChanged;

    const float POLL_FREQUENCY{ 3.0f };

    bool isPolling{ false };
    float delay{ POLL_FREQUENCY };
    FOnlineSubsystemDrift* DriftSubsystem;
    FName queueStatus;
    FMatchQueueMatch currentMatch;
};

/**
 * Interface definition for the online services session services 
 * Session services are defined as anything related managing a session 
 * and its state within a platform service
 */
class FOnlineSessionDrift : public IOnlineSession
{
private:

    /** Reference to the main Drift subsystem */
    FOnlineSubsystemDrift* DriftSubsystem;

    /** Hidden on purpose */
    FOnlineSessionDrift() :
        DriftSubsystem(nullptr),
        CurrentSessionSearch(nullptr)
    {}

PACKAGE_SCOPE:

    /** Critical sections for thread safe operation of session lists */
    mutable FCriticalSection SessionLock;

    /** Current session settings */
    TArray<FNamedOnlineSession> Sessions;

    /** Current search object */
    TSharedPtr<FOnlineSessionSearch> CurrentSessionSearch;
    FName CurrentSessionSearchName;
    TSharedPtr<FMatchesSearch> DriftSearch;

    TSharedPtr<FMatchQueueSearch> CurrentSearch;

    FDelegateHandle onMatchAddedDelegateHandle;

    FOnlineSessionDrift(class FOnlineSubsystemDrift* InSubsystem) :
        DriftSubsystem(InSubsystem),
        CurrentSessionSearch(nullptr)
    {}

    void Tick(float DeltaTime);

    // IOnlineSession
    class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings) override;
    class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSession& Session) override;

    /**
    * Registers and updates voice data for the given player id
    *
    * @param PlayerId player to register with the voice subsystem
    */
    void RegisterVoice(const FUniqueNetId& PlayerId);

    /**
    * Unregisters a given player id from the voice subsystem
    *
    * @param PlayerId player to unregister with the voice subsystem
    */
    void UnregisterVoice(const FUniqueNetId& PlayerId);

    /**
     * Registers all local players with the current session
     *
     * @param Session the session that they are registering in
     */
    void RegisterLocalPlayers(class FNamedOnlineSession* Session);

    void OnJoinedMatchQueue(bool success, const FMatchQueueStatus& status);
    void OnMatchSearchStatusChanged(FName status);
    void OnMatchAdded(bool success);
    void OnGotActiveMatches(bool success);

public:

    virtual ~FOnlineSessionDrift();

    // IOnlineSession
#if UE_VERSION_NEWER_THAN(4, 20, 0)
    virtual TSharedPtr<const FUniqueNetId> CreateSessionIdFromString(const FString& SessionIdStr) override;
#endif // UE_VERSION_NEWER_THAN(4, 20, 0)
    virtual FNamedOnlineSession* GetNamedSession(FName SessionName) override;
    virtual void RemoveNamedSession(FName SessionName) override;
    virtual EOnlineSessionState::Type GetSessionState(FName SessionName) const override;
    virtual bool HasPresenceSession() override;

    virtual bool CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
    virtual bool CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
    virtual bool StartSession(FName SessionName) override;
    virtual bool UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData = true) override;
    virtual bool EndSession(FName SessionName) override;
    virtual bool DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate = FOnDestroySessionCompleteDelegate()) override;
    virtual bool IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId) override;
    virtual bool StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
    virtual bool CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName) override;
    virtual bool CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName) override;
    virtual bool FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
    virtual bool FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
    virtual bool FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate) override;
    virtual bool CancelFindSessions() override;
    virtual bool PingSearchResults(const FOnlineSessionSearchResult& SearchResult) override;
    virtual bool JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
    virtual bool JoinSession(const FUniqueNetId& PlayerId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
    virtual bool FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend) override;
    virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend) override;
    virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList) override;
    virtual bool SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend) override;
    virtual bool SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend) override;
    virtual bool SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray<TSharedRef<const FUniqueNetId>>& Friends) override;
    virtual bool SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends) override;
    virtual bool GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType = GamePort) override;
    virtual bool GetResolvedConnectString(const class FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo) override;
    virtual FOnlineSessionSettings* GetSessionSettings(FName SessionName) override;
    virtual bool RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited) override;
    virtual bool RegisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players, bool bWasInvited = false) override;
    virtual bool UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId) override;
    virtual bool UnregisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players) override;
    virtual void RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate) override;
    virtual void UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate) override;
    virtual int32 GetNumSessions() override;
    virtual void DumpSessionState() override;
};

typedef TSharedPtr<FOnlineSessionDrift, ESPMode::ThreadSafe> FOnlineSessionDriftPtr;
