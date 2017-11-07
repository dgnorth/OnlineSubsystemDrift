/**
* This file is part of the Drift Unreal Engine Integration.
*
* Copyright (C) 2016-2017 Directive Games Limited. All Rights Reserved.
*
* Licensed under the MIT License (the "License");
*
* You may not use this file except in compliance with the License.
* You may obtain a copy of the license in the LICENSE file found at the top
* level directory of this module, and at https://mit-license.org/
*/

using UnrealBuildTool;

public class OnlineSubsystemDrift : ModuleRules
{
	public OnlineSubsystemDrift(ReadOnlyTargetRules TargetRules) : base(TargetRules)
    {
		PCHUsage = PCHUsageMode.NoSharedPCHs;
		
		Definitions.Add("ONLINESUBSYSTEMDRIFT_PACKAGE=1");

        PublicIncludePaths.AddRange(
            new string[] {
                "OnlineSubsystemDrift/Public",
            }
            );


        PrivateIncludePaths.AddRange(
            new string[] {
                "OnlineSubsystemDrift/Private",
            }
            );

        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"OnlineSubsystemUtils",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
                "Engine",
                "Sockets",
                "Voice",
                "OnlineSubsystem",
                "Drift",
			}
			);
	}
}
