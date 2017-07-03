// Copyright 2016-2017 Directive Games Limited - All Rights Reserved.

#include "OnlineSubsystemDriftPrivatePCH.h"
#include "OnlineAsyncTaskManagerDrift.h"
#include "OnlineSubsystemDrift.h"

void FOnlineAsyncTaskManagerDrift::OnlineTick()
{
	check(DriftSubsystem);
	check(FPlatformTLS::GetCurrentThreadId() == OnlineThreadId || !FPlatformProcess::SupportsMultithreading());
}

