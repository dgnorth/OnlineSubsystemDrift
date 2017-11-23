// Copyright 2016-2017 Directive Games Limited - All Rights Reserved.

#pragma once

#include "ModuleInterface.h"

/**
 * Online subsystem module class (Drift Implementation)
 * Code related to the loading of the Drift module
 */
class FOnlineSubsystemDriftModule : public IModuleInterface
{
private:

    /** Class responsible for creating instance(s) of the subsystem */
    class FOnlineFactoryDrift* DriftFactory;

public:

    FOnlineSubsystemDriftModule()
    : DriftFactory(nullptr)
    {}

    virtual ~FOnlineSubsystemDriftModule() {}

    // IModuleInterface

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    virtual bool SupportsDynamicReloading() override
    {
        return false;
    }

    virtual bool SupportsAutomaticShutdown() override
    {
        return false;
    }
};
