// Fill out your copyright notice in the Description page of Project Settings.


#include "Renderer/TyTSceneViewExtension.h"
#include "PostProcess/PostProcessInputs.h"

#include "Runtime/Renderer/Private/SceneRendering.h"
#include "PixelShaderUtils.h"
#include "ScreenPass.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

#include "Renderer/TyTPostProcessDrawUtils.h"
#include "Renderer/TyTPostProcessBloom.h"

DECLARE_GPU_DRAWCALL_STAT(TyTPostProcess);

#ifdef WITH_EDITOR
BEGIN_SHADER_PARAMETER_STRUCT(FRescaleEditorViewportPSParams, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InTexture)
	SHADER_PARAMETER(FVector2f, UVScaleFactor)
	SHADER_PARAMETER_SAMPLER(SamplerState, BilinearWrapSampler)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FRescaleEditorViewportPS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FRescaleEditorViewportPS, TYTPOSTPROCESS_API);
	using FParameters = FRescaleEditorViewportPSParams;
	SHADER_USE_PARAMETER_STRUCT(FRescaleEditorViewportPS, FGlobalShader);
};
IMPLEMENT_SHADER_TYPE(, FRescaleEditorViewportPS, TEXT("/TyTPostProcess/RescaleEditorViewportPixelShader.usf"), TEXT("MainPS"), SF_Pixel);

#endif

FTyTSceneViewExtension::FTyTSceneViewExtension(const FAutoRegister& AutoRegister):
	FSceneViewExtensionBase(AutoRegister)
{
}

FTyTSceneViewExtension::~FTyTSceneViewExtension()
{
}

void FTyTSceneViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FTyTSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
}

void FTyTSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

FVector2D CalcUVScaleFactor(const FIntRect& Input, const FIntPoint& Extent)
{
	// Based on
	// GetScreenPassTextureViewportParameters()
	// Engine/Source/Runtime/Renderer/Private/ScreenPass.cpp

	FVector2D ExtentInverse = FVector2D(1.0f / Extent.X, 1.0f / Extent.Y);

	FVector2D RectMin = FVector2D(Input.Min);
	FVector2D RectMax = FVector2D(Input.Max);

	FVector2D Min = RectMin * ExtentInverse;
	FVector2D Max = RectMax * ExtentInverse;

	return (Max - Min);
}

void FTyTSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessingInputs& Inputs)
{
	FSceneViewExtensionBase::PrePostProcessPass_RenderThread(GraphBuilder, InView, Inputs);

	// Unreal Insights
	RDG_GPU_STAT_SCOPE(GraphBuilder, TyTPostProcess);
	// Render Doc
	RDG_EVENT_SCOPE(GraphBuilder, "TyTPostProcess");

	// This is the color that actually has the shadow and the shade
	check(InView.bIsViewInfo);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InView);
	const FIntRect Viewport = ViewInfo.ViewRect;
	FScreenPassTexture SceneColorTexture((*Inputs.SceneTextures)->SceneColorTexture, Viewport);


#ifdef WITH_EDITOR
	// 에디터에서 Texture 사용방법에 따른 조치. 자세한 내용은 RescaleEditorViewportPixelShader.usf 파일을 참고.
	{
		const FString PassName = TEXT("Rescale Editor Viewport");
		FRDGTextureDesc RescaledTextureDesc;
		RescaledTextureDesc.Reset();
		RescaledTextureDesc.Extent = Viewport.Size();
		RescaledTextureDesc.Format = SceneColorTexture.Texture->Desc.Format;
		RescaledTextureDesc.ClearValue = FClearValueBinding::Transparent;
		FRDGTextureRef RescaledTexture = GraphBuilder.CreateTexture(RescaledTextureDesc, *PassName);

		FRescaleEditorViewportPS::FParameters* Params = 
			GraphBuilder.AllocParameters<FRescaleEditorViewportPS::FParameters>();
		Params->InTexture = SceneColorTexture.Texture;
		FScreenPassTextureViewport ScreenPassTextureViewport(SceneColorTexture);
		FVector2D UVScaleFactor = CalcUVScaleFactor(ScreenPassTextureViewport.Rect, ScreenPassTextureViewport.Extent);
		Params->UVScaleFactor = FVector2f(UVScaleFactor);
		Params->BilinearWrapSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		Params->RenderTargets[0] = FRenderTargetBinding(RescaledTexture, ERenderTargetLoadAction::ELoad);
		
		const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FTyTScreenPassVS> VS(GlobalShaderMap);
		TShaderMapRef<FRescaleEditorViewportPS> PS(GlobalShaderMap);

		AddTyTScreenPass(
			GraphBuilder,
			PassName,
			Params,
			VS,
			PS,
			TStaticBlendState<>::GetRHI(),
			Viewport
		);

		SceneColorTexture.Texture = RescaledTexture;
	}
#endif

	FRDGTextureRef HalfResSceneColor = AddTyTDownSamplePass(
		GraphBuilder,
		ViewInfo,
		TEXT("Half Res Scene Color"),
		SceneColorTexture.Texture,
		Viewport
	);
	FIntRect HalfResViewport = Viewport / 2;
	ViewInfo.GetInstancedViewUniformBuffer();
	
	FTyTBloomDownSampleChain BloomDownSampleChain;
	BloomDownSampleChain.Init(
		GraphBuilder,
		ViewInfo,
		AddTyTBloomSetupPass(GraphBuilder, ViewInfo, HalfResSceneColor, HalfResViewport),
		HalfResViewport
	);

	//CopyTexture2D(
	//	GraphBuilder,
	//	ViewInfo,
	//	TEXT("DebugCopy"),
	//	BloomDownSampleChain.GetLastTexture(),
	//	FRenderTargetBinding((*Inputs.SceneTextures)->SceneColorTexture, ERenderTargetLoadAction::ELoad),
	//	Viewport
	//);
}