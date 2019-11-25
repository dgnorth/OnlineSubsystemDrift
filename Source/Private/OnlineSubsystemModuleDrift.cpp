// Copyright 2016-2017 Directive Games Limited - All Rights Reserved.

#include "OnlineSubsystemDriftPrivate.h"

#include "OnlineSubsystemDrift.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FOnlineSubsystemDriftModule, OnlineSubsystemDrift);

/**
 * Class responsible for creating instance(s) of the subsystem
 */
class FOnlineFactoryDrift : public IOnlineFactory
{
public:

    FOnlineFactoryDrift() {}

    virtual ~FOnlineFactoryDrift() {}

    virtual IOnlineSubsystemPtr CreateSubsystem(FName InstanceName)
    {
        FOnlineSubsystemDriftPtr OnlineSub = MakeShareable(new FOnlineSubsystemDrift(InstanceName));
        if (OnlineSub->IsEnabled())
        {
            if(!OnlineSub->Init())
            {
                UE_LOG_ONLINE(Warning, TEXT("Drift API failed to initialize!"));
                OnlineSub->Shutdown();
                OnlineSub = nullptr;
            }
        }
        else
        {
            UE_LOG_ONLINE(Warning, TEXT("Drift API disabled!"));
            OnlineSub->Shutdown();
            OnlineSub = nullptr;
        }

        return OnlineSub;
    }
};

void FOnlineSubsystemDriftModule::StartupModule()
{
    FModuleManager::Get().LoadModuleChecked("Drift");

    DriftFactory = new FOnlineFactoryDrift();

    // Create and register our singleton factory with the main online subsystem for easy access
    FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
    OSS.RegisterPlatformService(DRIFT_SUBSYSTEM, DriftFactory);
}

void FOnlineSubsystemDriftModule::ShutdownModule()
{
    FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
    OSS.UnregisterPlatformService(DRIFT_SUBSYSTEM);
    
    delete DriftFactory;
    DriftFactory = nullptr;
}
