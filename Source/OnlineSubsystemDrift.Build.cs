// Copyright 2015-2017 Directive Games Limited - All Rights Reserved

using UnrealBuildTool;

public class OnlineSubsystemDrift : ModuleRules
{
	public OnlineSubsystemDrift(ReadOnlyTargetRules TargetRules) : base(TargetRules)
    {
		bFasterWithoutUnity = true;
		PCHUsage = PCHUsageMode.NoSharedPCHs;
		
		Definitions.Add("ONLINESUBSYSTEMDRIFT_PACKAGE=1");

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
