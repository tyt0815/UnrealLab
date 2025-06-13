// Fill out your copyright notice in the Description page of Project Settings.


#include "SubSystems/TyTPostProcessSubSystem.h"
#include "Renderer/TyTSceneViewExtension.h"
#include "DataAssets/TyTPostProcessSettingsAsset.h"

void UTyTPostProcessSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UEngineSubsystem::Initialize(Collection);
	
	// SceneViewEnxtension 실행
	TyTSceneViewExtension = FSceneViewExtensions::NewExtension<FTyTSceneViewExtension>();

	// PostProcess 설정 에셋 로드
	PostProcessSettings = LoadObject<UTyTPostProcessSettingsAsset>(nullptr, TEXT("/Script/TyTPostProcess.TyTPostProcessSettingsAsset'/TyTPostProcess/DA_PostProcessSettings.DA_PostProcessSettings'"));
	if (GEngine)
	{
		if (PostProcessSettings == nullptr)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5, FColor::Red, TEXT("TyTPostProcess설정 에셋 로드에 실패하였습니다."));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5, FColor::Green, TEXT("TyTPostProcess설정 에셋 로드에 성공하였습니다."));
		}
	}
}

void UTyTPostProcessSubSystem::Deinitialize()
{
	TyTSceneViewExtension.Reset();
	UEngineSubsystem::Deinitialize();
}

// static
UTyTPostProcessSubSystem* UTyTPostProcessSubSystem::Get()
{
	return GEngine->GetEngineSubsystem<UTyTPostProcessSubSystem>();
}
