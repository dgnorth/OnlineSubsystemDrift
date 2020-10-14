// Copyright 2016-2017 Directive Games Limited - All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemDriftPackage.h"

typedef TSharedPtr<class FOnlineSessionDrift, ESPMode::ThreadSafe> FOnlineSessionDriftPtr;
typedef TSharedPtr<class FOnlineProfileDrift, ESPMode::ThreadSafe> FOnlineProfileDriftPtr;
typedef TSharedPtr<class FOnlineFriendsDrift, ESPMode::ThreadSafe> FOnlineFriendsDriftPtr;
typedef TSharedPtr<class FOnlineUserCloudDrift, ESPMode::ThreadSafe> FOnlineUserCloudDriftPtr;
typedef TSharedPtr<class FOnlineLeaderboardsDrift, ESPMode::ThreadSafe> FOnlineLeaderboardsDriftPtr;
typedef TSharedPtr<class FOnlineVoiceImpl, ESPMode::ThreadSafe> FOnlineVoiceImplPtr;
typedef TSharedPtr<class FOnlineExternalUIDrift, ESPMode::ThreadSafe> FOnlineExternalUIDriftPtr;
typedef TSharedPtr<class FOnlineIdentityDrift, ESPMode::ThreadSafe> FOnlineIdentityDriftPtr;
typedef TSharedPtr<class FOnlineAchievementsDrift, ESPMode::ThreadSafe> FOnlineAchievementsDriftPtr;

class IDriftAPI;

#ifndef DRIFT_SUBSYSTEM
#define DRIFT_SUBSYSTEM FName(TEXT("DRIFT"))
#endif

/**
 *  OnlineSubsystemDrift - Implementation of the online subsystem for Drift services
 */
class ONLINESUBSYSTEMDRIFT_API FOnlineSubsystemDrift : public FOnlineSubsystemImpl
{

public:

    virtual ~FOnlineSubsystemDrift()
    {
    }

    // IOnlineSubsystem

    IOnlineSessionPtr GetSessionInterface() const override;
    IOnlineFriendsPtr GetFriendsInterface() const override;
    IOnlinePartyPtr GetPartyInterface() const override;
    IOnlineGroupsPtr GetGroupsInterface() const override;
    IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
    IOnlineUserCloudPtr GetUserCloudInterface() const override;
    IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
    IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
    IOnlineVoicePtr GetVoiceInterface() const override;
    IOnlineExternalUIPtr GetExternalUIInterface() const override;
    IOnlineTimePtr GetTimeInterface() const override;
    IOnlineIdentityPtr GetIdentityInterface() const override;
    IOnlineTitleFilePtr GetTitleFileInterface() const override;
    IOnlineStorePtr GetStoreInterface() const override;
    IOnlineStoreV2Ptr GetStoreV2Interface() const override { return nullptr; }
    IOnlinePurchasePtr GetPurchaseInterface() const override { return nullptr; }
    IOnlineEventsPtr GetEventsInterface() const override;
    IOnlineAchievementsPtr GetAchievementsInterface() const override;
    IOnlineSharingPtr GetSharingInterface() const override;
    IOnlineUserPtr GetUserInterface() const override;
    IOnlineMessagePtr GetMessageInterface() const override;
    IOnlinePresencePtr GetPresenceInterface() const override;
    IOnlineChatPtr GetChatInterface() const override;
	IOnlineStatsPtr GetStatsInterface() const override;
	IOnlineTurnBasedPtr GetTurnBasedInterface() const override;
	IOnlineTournamentPtr GetTournamentInterface() const override;

    bool Init() override;
    bool Shutdown() override;
    FString GetAppId() const override;
    bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	FText GetOnlineServiceName() const override;

	// FTickerObjectBase

    virtual bool Tick(float DeltaTime) override;

    // FOnlineSubsystemDrift

    /**
     * Is the Drift API available for use
     * @return true if Drift functionality is available, false otherwise
     */
    //bool IsEnabled();

PACKAGE_SCOPE:

    FOnlineSubsystemDrift(FName InInstanceName) :
        FOnlineSubsystemImpl(DRIFT_SUBSYSTEM, InInstanceName),
        SessionInterface(nullptr),
        VoiceInterface(nullptr),
        bVoiceInterfaceInitialized(false),
        LeaderboardsInterface(nullptr),
        IdentityInterface(nullptr),
        AchievementsInterface(nullptr),
        OnlineAsyncTaskThreadRunnable(nullptr),
        OnlineAsyncTaskThread(nullptr)
    {}

    IDriftAPI* GetDrift();

private:

    /** Interface to the session services */
    FOnlineSessionDriftPtr SessionInterface;

    /** Interface for voice communication */
    mutable FOnlineVoiceImplPtr VoiceInterface;

    /** Interface for voice communication */
    mutable bool bVoiceInterfaceInitialized;

    /** Interface to the leaderboard services */
    FOnlineLeaderboardsDriftPtr LeaderboardsInterface;

    /** Interface to the identity registration/auth services */
    FOnlineIdentityDriftPtr IdentityInterface;

    /** Interface for achievements */
    FOnlineAchievementsDriftPtr AchievementsInterface;

    /** Online async task runnable */
    class FOnlineAsyncTaskManagerDrift* OnlineAsyncTaskThreadRunnable;

    /** Online async task thread */
    class FRunnableThread* OnlineAsyncTaskThread;

    /** Task counter for generating unique thread names */
    static FThreadSafeCounter TaskCounter;
};

typedef TSharedPtr<FOnlineSubsystemDrift, ESPMode::ThreadSafe> FOnlineSubsystemDriftPtr;
