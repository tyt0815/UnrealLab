// Fill out your copyright notice in the Description page of Project Settings.


#include "SubSystems/TyTSceneViewExtensionSubSystem.h"
#include "Renderer/TyTSceneViewExtension.h"

void UTyTSceneViewExtensionSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UEngineSubsystem::Initialize(Collection);
	
	TyTSceneViewExtension = FSceneViewExtensions::NewExtension<FTyTSceneViewExtension>();
}

void UTyTSceneViewExtensionSubSystem::Deinitialize()
{
	TyTSceneViewExtension.Reset();
	UEngineSubsystem::Deinitialize();
}
