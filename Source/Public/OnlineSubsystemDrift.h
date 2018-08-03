// Copyright 2016-2017 Directive Games Limited - All Rights Reserved.

#pragma once

#include "Misc/EngineVersionComparison.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemDriftPackage.h"


typedef TSharedPtr<class FOnlineSessionDrift, ESPMode::ThreadSafe> FOnlineSessionDriftPtr;
typedef TSharedPtr<class FOnlineProfileDrift, ESPMode::ThreadSafe> FOnlineProfileDriftPtr;
typedef TSharedPtr<class FOnlineFriendsDrift, ESPMode::ThreadSafe> FOnlineFriendsDriftPtr;
typedef TSharedPtr<class FOnlineUserCloudDrift, ESPMode::ThreadSafe> FOnlineUserCloudDriftPtr;
typedef TSharedPtr<class FOnlineLeaderboardsDrift, ESPMode::ThreadSafe> FOnlineLeaderboardsDriftPtr;
typedef TSharedPtr<class FOnlineVoiceDrift, ESPMode::ThreadSafe> FOnlineVoiceDriftPtr;
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

    virtual IOnlineSessionPtr GetSessionInterface() const override;
    virtual IOnlineFriendsPtr GetFriendsInterface() const override;
    virtual IOnlinePartyPtr GetPartyInterface() const override;
    virtual IOnlineGroupsPtr GetGroupsInterface() const override;
    virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
    virtual IOnlineUserCloudPtr GetUserCloudInterface() const override;
    virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
    virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
    virtual IOnlineVoicePtr GetVoiceInterface() const override;
    virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;   
    virtual IOnlineTimePtr GetTimeInterface() const override;
    virtual IOnlineIdentityPtr GetIdentityInterface() const override;
    virtual IOnlineTitleFilePtr GetTitleFileInterface() const override;
    virtual IOnlineStorePtr GetStoreInterface() const override;
    virtual IOnlineStoreV2Ptr GetStoreV2Interface() const override { return nullptr; }
    virtual IOnlinePurchasePtr GetPurchaseInterface() const override { return nullptr; }
    virtual IOnlineEventsPtr GetEventsInterface() const override;
    virtual IOnlineAchievementsPtr GetAchievementsInterface() const override;
    virtual IOnlineSharingPtr GetSharingInterface() const override;
    virtual IOnlineUserPtr GetUserInterface() const override;
    virtual IOnlineMessagePtr GetMessageInterface() const override;
    virtual IOnlinePresencePtr GetPresenceInterface() const override;
    virtual IOnlineChatPtr GetChatInterface() const override;
    virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override;
    
    virtual bool Init() override;
    virtual bool Shutdown() override;
    virtual FString GetAppId() const override;
    virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

    virtual FText GetOnlineServiceName() const override;

    // FTickerObjectBase
    
    virtual bool Tick(float DeltaTime) override;

    // FOnlineSubsystemDrift

    /**
     * Is the Drift API available for use
     * @return true if Drift functionality is available, false otherwise
     */
#if UE_VERSION_NEWER_THAN(4, 20, 0)
    bool IsEnabled() const;
#else
    bool IsEnabled();
#endif // UE_VERSION_NEWER_THAN(4, 20, 0)

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

    FOnlineSubsystemDrift() :
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
    mutable FOnlineVoiceDriftPtr VoiceInterface;

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
