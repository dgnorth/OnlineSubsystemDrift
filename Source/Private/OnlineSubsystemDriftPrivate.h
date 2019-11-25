// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine.h"
#include "OnlineSubsystemDriftModule.h"
#include "OnlineSubsystemModule.h"
#include "OnlineSubsystem.h"
#include "Modules/ModuleManager.h"

#define INVALID_INDEX -1

/** URL Prefix when using Drift socket connection */
#define NULL_URL_PREFIX TEXT("Drift.")

/** pre-pended to all Drift logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("DRIFT: ")
