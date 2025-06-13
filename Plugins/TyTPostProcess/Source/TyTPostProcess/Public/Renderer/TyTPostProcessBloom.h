// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Renderer/TyTPostProcessDrawUtils.h"

FRDGTextureRef AddTyTBloomSetupPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	FRDGTextureRef HalfResSceneColor,
	const FIntRect& HalfResViewport
);

FRDGTextureRef AddTyTGaussianBloomPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	const FTyTBloomDownSampleChain& DownSampleChain
);

FRDGTextureRef AddTyTDualKawaseBloomPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	const FTyTBloomDownSampleChain& DownSampleChain
);