// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "TyTSceneViewExtensionSubSystem.generated.h"

class FSceneViewExtensionBase;

UCLASS()
class TYTPOSTPROCESS_API UTyTSceneViewExtensionSubSystem : public UEngineSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

private:
	TSharedPtr<FSceneViewExtensionBase, ESPMode::ThreadSafe> TyTSceneViewExtension;
};
