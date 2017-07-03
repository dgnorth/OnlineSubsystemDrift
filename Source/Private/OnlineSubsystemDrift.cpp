// Copyright 2015-2017 Directive Games Limited - All Rights Reserved

#include "OnlineSubsystemDriftPrivatePCH.h"
#include "OnlineSubsystemDrift.h"
#include "OnlineAsyncTaskManagerDrift.h"
#include "OnlineSessionDrift.h"
/*
#include "OnlineLeaderboardInterfaceDrift.h"
 */
#include "OnlineIdentityDrift.h"
#include "VoiceInterfaceDrift.h"
/*
#include "OnlineAchievementsInterfaceDrift.h"
*/

#include "DriftUtils.h"

IOnlineSessionPtr FOnlineSubsystemDrift::GetSessionInterface() const
{
    return SessionInterface;
}

IOnlineFriendsPtr FOnlineSubsystemDrift::GetFriendsInterface() const
{
    return nullptr;
}

IOnlinePartyPtr FOnlineSubsystemDrift::GetPartyInterface() const
{
    return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemDrift::GetGroupsInterface() const
{
    return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemDrift::GetSharedCloudInterface() const
{
    return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemDrift::GetUserCloudInterface() const
{
    return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemDrift::GetEntitlementsInterface() const
{
    return nullptr;
};

IOnlineLeaderboardsPtr FOnlineSubsystemDrift::GetLeaderboardsInterface() const
{
    return nullptr;//LeaderboardsInterface;
}

IOnlineVoicePtr FOnlineSubsystemDrift::GetVoiceInterface() const
{
    if (VoiceInterface.IsValid() && !bVoiceInterfaceInitialized)
    {   
        if (!VoiceInterface->Init())
        {
            VoiceInterface = nullptr;
        }

        bVoiceInterfaceInitialized = true;
    }

    return VoiceInterface;
}

IOnlineExternalUIPtr FOnlineSubsystemDrift::GetExternalUIInterface() const
{
    return nullptr;
}

IOnlineTimePtr FOnlineSubsystemDrift::GetTimeInterface() const
{
    return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemDrift::GetIdentityInterface() const
{
    return IdentityInterface;
}

IOnlineTitleFilePtr FOnlineSubsystemDrift::GetTitleFileInterface() const
{
    return nullptr;
}

IOnlineStorePtr FOnlineSubsystemDrift::GetStoreInterface() const
{
    return nullptr;
}

IOnlineEventsPtr FOnlineSubsystemDrift::GetEventsInterface() const
{
    return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemDrift::GetAchievementsInterface() const
{
    return nullptr;//AchievementsInterface;
}

IOnlineSharingPtr FOnlineSubsystemDrift::GetSharingInterface() const
{
    return nullptr;
}

IOnlineUserPtr FOnlineSubsystemDrift::GetUserInterface() const
{
    return nullptr;
}

IOnlineMessagePtr FOnlineSubsystemDrift::GetMessageInterface() const
{
    return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemDrift::GetPresenceInterface() const
{
    return nullptr;
}

IOnlineChatPtr FOnlineSubsystemDrift::GetChatInterface() const
{
    return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemDrift::GetTurnBasedInterface() const
{
    return nullptr;
}

bool FOnlineSubsystemDrift::Tick(float DeltaTime)
{
    if (!FOnlineSubsystemImpl::Tick(DeltaTime))
    {
        return false;
    }

    if (OnlineAsyncTaskThreadRunnable)
    {
        OnlineAsyncTaskThreadRunnable->GameTick();
    }

    if (SessionInterface.IsValid())
    {
        SessionInterface->Tick(DeltaTime);
    }

    if (VoiceInterface.IsValid() && bVoiceInterfaceInitialized)
    {
        VoiceInterface->Tick(DeltaTime);
    }

    return true;
}

bool FOnlineSubsystemDrift::Init()
{
    const bool bDriftInit = true;
    
    if (bDriftInit)
    {
        // Create the online async task thread
        OnlineAsyncTaskThreadRunnable = new FOnlineAsyncTaskManagerDrift(this);
        check(OnlineAsyncTaskThreadRunnable);
        OnlineAsyncTaskThread = FRunnableThread::Create(OnlineAsyncTaskThreadRunnable, *FString::Printf(TEXT("OnlineAsyncTaskThreadDrift %s"), *InstanceName.ToString()), 128 * 1024, TPri_Normal);
        check(OnlineAsyncTaskThread);
        UE_LOG_ONLINE(Verbose, TEXT("Created thread (ID:%d)."), OnlineAsyncTaskThread->GetThreadID());

        SessionInterface = MakeShareable(new FOnlineSessionDrift(this));
//        LeaderboardsInterface = MakeShareable(new FOnlineLeaderboardsDrift(this));
        IdentityInterface = MakeShareable(new FOnlineIdentityDrift(this));
//        AchievementsInterface = MakeShareable(new FOnlineAchievementsDrift(this));
        VoiceInterface = MakeShareable(new FOnlineVoiceDrift(this));
    }
    else
    {
        Shutdown();
    }

    return bDriftInit;
}

bool FOnlineSubsystemDrift::Shutdown()
{
    UE_LOG_ONLINE(Display, TEXT("FOnlineSubsystemDrift::Shutdown()"));

    FOnlineSubsystemImpl::Shutdown();

    if (OnlineAsyncTaskThread)
    {
        // Destroy the online async task thread
        delete OnlineAsyncTaskThread;
        OnlineAsyncTaskThread = nullptr;
    }

    if (OnlineAsyncTaskThreadRunnable)
    {
        delete OnlineAsyncTaskThreadRunnable;
        OnlineAsyncTaskThreadRunnable = nullptr;
    }

    if (VoiceInterface.IsValid() && bVoiceInterfaceInitialized)
    {
        VoiceInterface->Shutdown();
    }
    
    #define DESTRUCT_INTERFACE(Interface) \
    if (Interface.IsValid()) \
    { \
        ensure(Interface.IsUnique()); \
        Interface = nullptr; \
    }
 
    // Destruct the interfaces
    DESTRUCT_INTERFACE(VoiceInterface);
    //DESTRUCT_INTERFACE(AchievementsInterface);
    DESTRUCT_INTERFACE(IdentityInterface);
    //DESTRUCT_INTERFACE(LeaderboardsInterface);
    DESTRUCT_INTERFACE(SessionInterface);
    
    #undef DESTRUCT_INTERFACE
    
    return true;
}

FString FOnlineSubsystemDrift::GetAppId() const
{
    return TEXT("");
}

bool FOnlineSubsystemDrift::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
    if (FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar))
    {
        return true;
    }
    return false;
}

bool FOnlineSubsystemDrift::IsEnabled()
{
    return true;
}


IDriftAPI* FOnlineSubsystemDrift::GetDrift()
{
    return FDriftWorldHelper{GetInstanceName()}.GetInstance();
}
