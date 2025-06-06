// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

FRDGTextureRef AddTyTBloomSetupPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	FRDGTextureRef HalfResSceneColor,
	const FIntRect& HalfResViewport
);

class FTyTBloomDownSampleChain
{
public:
	static const uint32 StageCount = 6;
	FTyTBloomDownSampleChain() = default;

	void Init(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& ViewInfo,
		FRDGTextureRef BloomSetupTexture,
		const FIntRect& Viewport
	);

private:
	TStaticArray<FRDGTextureRef, StageCount> Textures;

public:
	FORCEINLINE FRDGTextureRef GetTexture(uint32 i) const
	{
		return Textures[i];
	}
	FORCEINLINE FRDGTextureRef GetLastTexture() const
	{
		return Textures[StageCount - 1];
	}
};

void AddTyTBloomPass(
	const FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	const FTyTBloomDownSampleChain& DownSampleChain,
	FRenderTargetBinding& RenderTarget,
	const FIntRect& RenderTargetViewport
);