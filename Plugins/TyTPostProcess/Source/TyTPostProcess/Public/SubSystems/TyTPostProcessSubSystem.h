// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "TyTPostProcessSubSystem.generated.h"

class FSceneViewExtensionBase;
class UTyTPostProcessSettingsAsset;

UCLASS()
class TYTPOSTPROCESS_API UTyTPostProcessSubSystem : public UEngineSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	static UTyTPostProcessSubSystem* Get();

private:
	UPROPERTY()
	UTyTPostProcessSettingsAsset* PostProcessSettings;

	TSharedPtr<FSceneViewExtensionBase, ESPMode::ThreadSafe> TyTSceneViewExtension;

public:
	FORCEINLINE UTyTPostProcessSettingsAsset* GetPostProcessSettings() const
	{
		return PostProcessSettings;
	}
};
