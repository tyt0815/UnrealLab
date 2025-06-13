	// Copyright Epic Games, Inc. All Rights Reserved.

#include "TyTPostProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Renderer/TyTPostProcessDrawUtils.h"

#define LOCTEXT_NAMESPACE "FTyTPostProcessModule"

void FTyTPostProcessModule::StartupModule()
{
	FString ShaderPath = IPluginManager::Get().FindPlugin(TEXT("TyTPostProcess"))->GetBaseDir() / TEXT("Shaders");
	if (!AllShaderSourceDirectoryMappings().Contains("/TyTPostProcess"))
	{
		AddShaderSourceDirectoryMapping("/TyTPostProcess", ShaderPath);
	}
}

void FTyTPostProcessModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTyTPostProcessModule, TyTPostProcess)