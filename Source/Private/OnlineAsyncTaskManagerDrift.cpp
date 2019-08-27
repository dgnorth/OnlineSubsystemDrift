// Copyright 2016-2019 Directive Games Limited - All Rights Reserved.


#include "OnlineAsyncTaskManagerDrift.h"

#include "OnlineSubsystemDrift.h"


void FOnlineAsyncTaskManagerDrift::OnlineTick()
{
	check(DriftSubsystem);
	check(FPlatformTLS::GetCurrentThreadId() == OnlineThreadId || !FPlatformProcess::SupportsMultithreading());
}

