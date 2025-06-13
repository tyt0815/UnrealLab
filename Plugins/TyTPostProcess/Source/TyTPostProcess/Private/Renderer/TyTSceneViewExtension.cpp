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
#include "Renderer/TyTPostProcessLensFlare.h"
#include "DataAssets/TyTPostProcessSettingsAsset.h"

TAutoConsoleVariable<bool> CVarTyTPostProcessPass(
	TEXT("r.TyT.PostProcessPass"),
	false,
	TEXT(" true: Enable")
	TEXT(" false: Disable"),
	ECVF_RenderThreadSafe
);

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

class FTyTPostProcessPS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FTyTPostProcessPS, TYTPOSTPROCESS_API);
	SHADER_USE_PARAMETER_STRUCT(FTyTPostProcessPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, TYTPOSTPROCESS_API)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)

		// Bloom
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BloomTexture)

		// Lens Flare
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LensFlareGhostsHaloTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LensFlareGlareTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, LensFlareGradientTexture)
		SHADER_PARAMETER(FVector4f, LensFlareTint)
		SHADER_PARAMETER(float, LensFlareIntensity)
		SHADER_PARAMETER(FVector2f, LensFlareGlareTexturePixelSize)


		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FBloom : SHADER_PERMUTATION_BOOL("USE_BLOOM");
	class FLensFlareGhostsHalo : SHADER_PERMUTATION_BOOL("USE_LENS_FLARE_GHOSTS_HALO");
	class FLensFlareGlare : SHADER_PERMUTATION_BOOL("USE_LENS_FLARE_GLARE");
	class FLensFlareGradient : SHADER_PERMUTATION_BOOL("USE_LENS_FLARE_GRADIENT");

	using FPermutationDomain = TShaderPermutationDomain<FBloom, FLensFlareGhostsHalo, FLensFlareGlare, FLensFlareGradient>;
};
IMPLEMENT_GLOBAL_SHADER(FTyTPostProcessPS, "/TyTPostProcess/TyTPostProcessPixelShader.usf", "MainPS", SF_Pixel);

struct FTyTPostProcessInputs
{
	struct FTyTBloomDatas
	{
		FRDGTextureRef BloomTexture = nullptr;
	};

	struct FTyTLensFlareDatas
	{
		FRDGTextureRef GhostsHaloTexture = nullptr;
		FRDGTextureRef GlareTexture = nullptr;
		FTextureRHIRef GradientTexture = nullptr;
		FVector4f Tint;
		FVector2f GlareTexturePixelSize;
		float Intensity;
	};

	FRenderTargetBinding* RenderTargetBinding = nullptr;
	FIntRect RenderTargetViewport;
	FRDGTextureRef SceneColor = nullptr;
	
	FTyTBloomDatas Bloom;
	FTyTLensFlareDatas LensFlare;
};

void AddTyTPostProcessPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	const FTyTPostProcessInputs& Inputs
)
{
	// Unreal Insights
	RDG_GPU_STAT_SCOPE(GraphBuilder, TyTPostProcess);
	// Render Doc
	RDG_EVENT_SCOPE(GraphBuilder, "TyTPostProcess");

	FTyTPostProcessPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTyTPostProcessPS::FParameters>();
	PassParameters->RenderTargets[0] = *Inputs.RenderTargetBinding;
	PassParameters->InputSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	PassParameters->SceneColorTexture = Inputs.SceneColor;

	// Bloom
	PassParameters->BloomTexture = Inputs.Bloom.BloomTexture;

	// LensFlare
	PassParameters->LensFlareGlareTexture = Inputs.LensFlare.GlareTexture;
	PassParameters->LensFlareGhostsHaloTexture = Inputs.LensFlare.GhostsHaloTexture;
	PassParameters->LensFlareIntensity = Inputs.LensFlare.Intensity;
	PassParameters->LensFlareGlareTexturePixelSize = Inputs.LensFlare.GlareTexturePixelSize;
	PassParameters->LensFlareTint = Inputs.LensFlare.Tint;
	PassParameters->LensFlareGradientTexture = Inputs.LensFlare.GradientTexture;

	FTyTPostProcessPS::FPermutationDomain Permutation;
	Permutation.Set<FTyTPostProcessPS::FBloom>(PassParameters->BloomTexture != nullptr);
	Permutation.Set<FTyTPostProcessPS::FLensFlareGhostsHalo>(PassParameters->LensFlareGhostsHaloTexture != nullptr);
	Permutation.Set<FTyTPostProcessPS::FLensFlareGlare>(PassParameters->LensFlareGlareTexture != nullptr);
	Permutation.Set<FTyTPostProcessPS::FLensFlareGradient>(PassParameters->LensFlareGradientTexture != nullptr);

	TShaderMapRef<FTyTScreenPassVS> VertexShader(ViewInfo.ShaderMap);
	TShaderMapRef<FTyTPostProcessPS> PixelShader(ViewInfo.ShaderMap, Permutation);

	AddTyTScreenPass(
		GraphBuilder,
		TEXT("TyTPostProcess"),
		PassParameters,
		VertexShader,
		PixelShader,
		TStaticBlendState<>::GetRHI(),
		Inputs.RenderTargetViewport
	);
}

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
		RescaledTextureDesc.Flags |= ETextureCreateFlags::UAV;
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
	FRenderTargetBinding RenderTargetBinding((*Inputs.SceneTextures)->SceneColorTexture, ERenderTargetLoadAction::ENoAction);
	UTyTPostProcessSettingsAsset* Settings = UTyTPostProcessSettingsAsset::Get();
	if (!CVarTyTPostProcessPass.GetValueOnRenderThread() || !Settings)
	{
		return;
	}

	FRDGTextureRef HalfSceneColor = nullptr;
	FRDGTextureRef Bloom = nullptr;

	FIntRect HalfResViewport = Viewport / 2;

	bool bBloom = Settings->BloomSettings.Intensity > SMALL_NUMBER;

	if (bBloom)
	{
		HalfSceneColor = AddTyTDownSamplePass(
			GraphBuilder,
			ViewInfo,
			TEXT("Half Res Scene Color"),
			SceneColorTexture.Texture,
			Viewport
		);

		FTyTBloomDownSampleChain BloomDownSampleChain;
		BloomDownSampleChain.Init(
			GraphBuilder,
			ViewInfo,
			AddTyTBloomSetupPass(GraphBuilder, ViewInfo, HalfSceneColor, HalfResViewport),
			HalfResViewport
		);

		Bloom = AddTyTGaussianBloomPass(GraphBuilder, ViewInfo, BloomDownSampleChain);
	}

	bool bLensFlare = bBloom &&
		Settings->LensFlareSettings.bEnableLensFlare &&
		Settings->LensFlareSettings.Intensity >	SMALL_NUMBER
		;
	FTyTLensFlareOutputs LensFlareOutputs;
	if (bLensFlare)
	{
		FTyTLensFlareInputs LensFlareInputs;
		LensFlareInputs.HalfSceneColor = HalfSceneColor;
		LensFlareInputs.HalfResViewport = HalfResViewport;
		LensFlareInputs.Settings = &Settings->LensFlareSettings;
		AddTyTLensFlarePass(
			GraphBuilder,
			ViewInfo,
			LensFlareInputs,
			LensFlareOutputs
		);
	}
	
	bool bPostProcess = bBloom;
	if (bPostProcess)
	{
		FTyTPostProcessInputs TyTPostProcessInputs;
		TyTPostProcessInputs.RenderTargetBinding = &RenderTargetBinding;
		TyTPostProcessInputs.RenderTargetViewport = Viewport;
		TyTPostProcessInputs.SceneColor = SceneColorTexture.Texture;
		TyTPostProcessInputs.Bloom.BloomTexture = Bloom;
		TyTPostProcessInputs.LensFlare.GlareTexture = LensFlareOutputs.GlareTexture;
		TyTPostProcessInputs.LensFlare.GhostsHaloTexture = LensFlareOutputs.GhostsHaloTexture;
		TyTPostProcessInputs.LensFlare.GradientTexture = Settings->LensFlareSettings.Gradient ?
			Settings->LensFlareSettings.Gradient->GetResource()->TextureRHI : nullptr;
		TyTPostProcessInputs.LensFlare.Intensity = Settings->LensFlareSettings.Intensity;
		TyTPostProcessInputs.LensFlare.GlareTexturePixelSize = LensFlareOutputs.GlareTexturePixelSize;
		TyTPostProcessInputs.LensFlare.Tint = Settings->LensFlareSettings.Tint;
		AddTyTPostProcessPass(GraphBuilder, ViewInfo, TyTPostProcessInputs);
	}
}