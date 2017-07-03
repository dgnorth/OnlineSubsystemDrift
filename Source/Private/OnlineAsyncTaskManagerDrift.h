// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"

/**
 *	Drift version of the async task manager to register the various Drift callbacks with the engine
 */
class FOnlineAsyncTaskManagerDrift : public FOnlineAsyncTaskManager
{
protected:

	/** Cached reference to the main online subsystem */
	class FOnlineSubsystemDrift* DriftSubsystem;

public:

	FOnlineAsyncTaskManagerDrift(class FOnlineSubsystemDrift* InOnlineSubsystem)
		: DriftSubsystem(InOnlineSubsystem)
	{
	}

	~FOnlineAsyncTaskManagerDrift() 
	{
	}

	// FOnlineAsyncTaskManager
	virtual void OnlineTick() override;
};
