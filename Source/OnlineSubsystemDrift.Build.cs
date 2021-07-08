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

using System.IO;
using UnrealBuildTool;

public class OnlineSubsystemDrift : ModuleRules
{
	public OnlineSubsystemDrift(ReadOnlyTargetRules TargetRules) : base(TargetRules)
    {
#if UE_4_24_OR_LATER
		bUseUnity = false;
#else
		bFasterWithoutUnity = true;
#endif

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDefinitions.Add("ONLINESUBSYSTEMDRIFT_PACKAGE=1");

        PublicIncludePaths.AddRange(
            new string[] {
                Path.Combine(ModuleDirectory, "Public"),
            }
            );


        PrivateIncludePaths.AddRange(
            new string[] {
                Path.Combine(ModuleDirectory, "Private"),
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
                "JsonArchive",
			}
			);
	}
}
