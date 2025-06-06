// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SceneViewExtension.h"

/**
 * 
 */
class TYTPOSTPROCESS_API FTyTSceneViewExtension : public FSceneViewExtensionBase
{
public:
	FTyTSceneViewExtension(const FAutoRegister& AutoRegister);
	~FTyTSceneViewExtension();

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

	virtual void PrePostProcessPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& InView,
		const FPostProcessingInputs& Inputs
	) override;
};