// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Renderer/TyTPostProcessDrawUtils.h"

struct FTyTLensFlareSettings;

struct FTyTLensFlareInputs
{
	FRDGTextureRef HalfSceneColor = nullptr;
	FIntRect HalfResViewport;
	const FTyTLensFlareSettings* Settings;
};

struct FTyTLensFlareOutputs
{
	FRDGTextureRef GhostsHaloTexture = nullptr;
	FRDGTextureRef GlareTexture = nullptr;
	FVector2f GlareTexturePixelSize;
};

void AddTyTLensFlarePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	const FTyTLensFlareInputs& Inputs,
	FTyTLensFlareOutputs& Outputs
);