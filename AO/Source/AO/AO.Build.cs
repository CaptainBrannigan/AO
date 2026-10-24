// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class AO : ModuleRules
{
	public AO(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "NavigationSystem" });

		PrivateDependencyModuleNames.AddRange(new string[] { "NetCore" });

		SetupIrisSupport(Target);
	}
}
