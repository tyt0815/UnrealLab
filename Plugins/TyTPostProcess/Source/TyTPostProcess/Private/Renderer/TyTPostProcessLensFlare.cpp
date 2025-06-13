// Fill out your copyright notice in the Description page of Project Settings.


#include "Renderer/TyTPostProcessLensFlare.h"
#include "DataAssets/TyTPostProcessSettingsAsset.h"
#include "SceneRendering.h"
#include "ShaderParameterStruct.h"

DECLARE_GPU_DRAWCALL_STAT(TyTLensFlare)

FRDGTextureRef AddTyTLensFlareThresholdPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	FRDGTextureRef HalfSceneColor,
	const FIntRect& HalfResViewport
)
{
	RDG_EVENT_SCOPE(GraphBuilder, "Threshold");

	const FString PassName = TEXT("Threshold");

	FRDGTextureRef Threshold = AddTyTDownSamplePass(
		GraphBuilder,
		ViewInfo,
		PassName + TEXT(".DownSample"),
		HalfSceneColor,
		HalfResViewport
	);

	return AddTyTDualKawaseBlurPass(
		GraphBuilder,
		ViewInfo,
		PassName + TEXT(".DualKawaseBlur"),
		Threshold,
		HalfResViewport / 2,
		FLinearColor::White,
		1
	);
}

class FTyTLensFlareChromaPS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FTyTLensFlareChromaPS, TYTPOSTPROCESS_API);
	SHADER_USE_PARAMETER_STRUCT(FTyTLensFlareChromaPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, TYTPOSTPROCESS_API)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER(float, ChromaShift)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FTyTLensFlareChromaPS, "/TyTPostProcess/TyTLensFlareChromaPixelShader.usf", "MainPS", SF_Pixel);


FRDGTextureRef AddTyTLensFlareChromaPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	FRDGTextureRef ThresholdTexture,
	const FIntRect& HalfResViewport,
	const FTyTLensFlareSettings* Settings
)
{
	FRDGTextureDesc TextureDesc;
	TextureDesc.Reset();
	TextureDesc.Extent = HalfResViewport.Size();
	TextureDesc.Format = PF_FloatRGB;
	TextureDesc.ClearValue = FClearValueBinding::Black;
	FRDGTextureRef ChromaTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("Chroma"));

	FTyTLensFlareChromaPS::FParameters* PassParameters = GraphBuilder.AllocParameters< FTyTLensFlareChromaPS::FParameters>();
	PassParameters->InputTexture = ThresholdTexture;
	PassParameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI();
	PassParameters->ChromaShift = Settings->GhostChromaShift;
	PassParameters->RenderTargets[0] = FRenderTargetBinding(ChromaTexture, ERenderTargetLoadAction::ENoAction);

	TShaderMapRef<FTyTScreenPassVS> VertexShader(ViewInfo.ShaderMap);
	TShaderMapRef<FTyTLensFlareChromaPS> PixelShader(ViewInfo.ShaderMap);

	AddTyTScreenPass(
		GraphBuilder,
		TEXT("Chroma"),
		PassParameters,
		VertexShader,
		PixelShader,
		TStaticBlendState<>::GetRHI(),
		HalfResViewport
	);

	return ChromaTexture;
}

class FTyTLensFlareGhostsPS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FTyTLensFlareGhostsPS, TYTPOSTPROCESS_API);
	SHADER_USE_PARAMETER_STRUCT(FTyTLensFlareGhostsPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, TYTPOSTPROCESS_API)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER_ARRAY(FVector4f, GhostColors, [8])
		SHADER_PARAMETER_SCALAR_ARRAY(float, GhostScales, [8])
		SHADER_PARAMETER(float, Intensity)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FTyTLensFlareGhostsPS, "/TyTPostProcess/TyTLensFlareGhostsPixelShader.usf", "MainPS", SF_Pixel);

FRDGTextureRef AddTyTLensFlareGhostsPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	FRDGTextureRef ChromaTexture,
	const FIntRect& HalfResViewport,
	const FTyTLensFlareSettings* Settings
)
{
	FRDGTextureDesc TextureDesc;
	TextureDesc.Reset();
	TextureDesc.Extent = HalfResViewport.Size();
	TextureDesc.Format = PF_FloatRGB;
	TextureDesc.ClearValue = FClearValueBinding::Black;
	FRDGTextureRef GhostsTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("Ghosts"));

	FTyTLensFlareGhostsPS::FParameters* PassParameters = GraphBuilder.AllocParameters< FTyTLensFlareGhostsPS::FParameters>();
	PassParameters->InputTexture = ChromaTexture;
	PassParameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI();
	PassParameters->Intensity = Settings->GhostIntensity;
	PassParameters->GhostColors[0] = Settings->Ghost1.Color;
	PassParameters->GhostColors[1] = Settings->Ghost2.Color;
	PassParameters->GhostColors[2] = Settings->Ghost3.Color;
	PassParameters->GhostColors[3] = Settings->Ghost4.Color;
	PassParameters->GhostColors[4] = Settings->Ghost5.Color;
	PassParameters->GhostColors[5] = Settings->Ghost6.Color;
	PassParameters->GhostColors[6] = Settings->Ghost7.Color;
	PassParameters->GhostColors[7] = Settings->Ghost8.Color;
	GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 0) = Settings->Ghost1.Scale;
	GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 1) = Settings->Ghost2.Scale;
	GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 2) = Settings->Ghost3.Scale;
	GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 3) = Settings->Ghost4.Scale;
	GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 4) = Settings->Ghost5.Scale;
	GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 5) = Settings->Ghost6.Scale;
	GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 6) = Settings->Ghost7.Scale;
	GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 7) = Settings->Ghost8.Scale;
	PassParameters->RenderTargets[0] = FRenderTargetBinding(GhostsTexture, ERenderTargetLoadAction::ENoAction);

	TShaderMapRef<FTyTScreenPassUseScreenPositionVS> VertexShader(ViewInfo.ShaderMap);
	TShaderMapRef<FTyTLensFlareGhostsPS> PixelShader(ViewInfo.ShaderMap);

	AddTyTScreenPass(
		GraphBuilder,
		TEXT("Ghosts"),
		PassParameters,
		VertexShader,
		PixelShader,
		TStaticBlendState<>::GetRHI(),
		HalfResViewport
	);

	return GhostsTexture;
}

class FTyTLensFlareHaloPS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FTyTLensFlareHaloPS, TYTPOSTPROCESS_API);
	SHADER_USE_PARAMETER_STRUCT(FTyTLensFlareHaloPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, TYTPOSTPROCESS_API)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER(float, Width)
		SHADER_PARAMETER(float, Mask)
		SHADER_PARAMETER(float, Compression)
		SHADER_PARAMETER(float, Intensity)
		SHADER_PARAMETER(float, ChromaShift)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FTyTLensFlareHaloPS, "/TyTPostProcess/TyTLesnFlareHaloPixelShader.usf", "MainPS", SF_Pixel);

FRDGTextureRef AddTyTLensFlareHaloPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	FRDGTextureRef ThresholdTexture,
	FRDGTextureRef GhostsTexture,
	const FIntRect& HalfResViewport,
	const FTyTLensFlareSettings* Settings
)
{
	RDG_EVENT_SCOPE(GraphBuilder, "Halo");

	FTyTLensFlareHaloPS::FParameters* PassParameters = GraphBuilder.AllocParameters< FTyTLensFlareHaloPS::FParameters>();
	PassParameters->InputTexture = ThresholdTexture;
	PassParameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI();
	PassParameters->Intensity = Settings->HaloIntensity;
	PassParameters->Mask = Settings->HaloMask;
	PassParameters->Width = Settings->HaloWidth;
	PassParameters->ChromaShift = Settings->HaloChromaShift;
	PassParameters->Compression = Settings->HaloCompression;
	PassParameters->RenderTargets[0] = FRenderTargetBinding(GhostsTexture, ERenderTargetLoadAction::ELoad);

	TShaderMapRef<FTyTScreenPassUseScreenPositionVS> VertexShader(ViewInfo.ShaderMap);
	TShaderMapRef<FTyTLensFlareHaloPS> PixelShader(ViewInfo.ShaderMap);

	AddTyTScreenPass(
		GraphBuilder,
		TEXT("Halo"),
		PassParameters,
		VertexShader,
		PixelShader,
		TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI(),
		HalfResViewport
	);

	return AddTyTDualKawaseBlurPass(
		GraphBuilder,
		ViewInfo,
		TEXT("Halo.DualKawaseBlur"),
		GhostsTexture,
		HalfResViewport,
		FLinearColor::White,
		1
	);
}

BEGIN_SHADER_PARAMETER_STRUCT(FTyTLensFlareGlarePassParameters, TYTPOSTPROCESS_API)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FTyTLensFlareGlareVS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FTyTLensFlareGlareVS, TYTPOSTPROCESS_API);
	SHADER_USE_PARAMETER_STRUCT(FTyTLensFlareGlareVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, TYTPOSTPROCESS_API)
		SHADER_PARAMETER_STRUCT_INCLUDE(FTyTLensFlareGlarePassParameters, PassParameters)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER(FIntPoint, TileCount)
		SHADER_PARAMETER(FVector2f, PixelSize)
		SHADER_PARAMETER(FVector2f, BufferSize)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FTyTLensFlareGlareVS, "/TyTPostProcess/TyTLensFlareGlareVertexShader.usf", "MainVS", SF_Vertex);

class FTyTLensFlareGlareGS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FTyTLensFlareGlareGS, TYTPOSTPROCESS_API);
	SHADER_USE_PARAMETER_STRUCT(FTyTLensFlareGlareGS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, TYTPOSTPROCESS_API)
		SHADER_PARAMETER(FVector2f, BufferSize)
		SHADER_PARAMETER(FVector2f, BufferRatio)
		SHADER_PARAMETER(float, GlareIntensity)
		SHADER_PARAMETER(float, GlareDivider)
		SHADER_PARAMETER(FVector4f, GlareTint)
		SHADER_PARAMETER_SCALAR_ARRAY(float, GlareScales, [3])
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FTyTLensFlareGlareGS, "/TyTPostProcess/TyTLensFlareGlareGeometryShader.usf", "MainGS", SF_Geometry);

class FTyTLensFlareGlarePS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FTyTLensFlareGlarePS, TYTPOSTPROCESS_API);
	SHADER_USE_PARAMETER_STRUCT(FTyTLensFlareGlarePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, TYTPOSTPROCESS_API)
		SHADER_PARAMETER_SAMPLER(SamplerState, GlareSampler)
		SHADER_PARAMETER_TEXTURE(Texture2D, GlareTexture)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FTyTLensFlareGlarePS, "/TyTPostProcess/TyTLensFlareGlarePixelShader.usf", "MainPS", SF_Pixel);

FRDGTextureRef AddTyTLensFlareGlarePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	FRDGTextureRef ThresholdTexture,
	const FIntRect& HalfResViewport,
	const FTyTLensFlareSettings* Settings,
	FVector2f& GlareTexturePixelSize
)
{
	if (Settings->GlareIntensity < SMALL_NUMBER)
	{
		return nullptr;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "Glare");

	const FIntRect QuaterResViewport = HalfResViewport / 2;

	const FString PassName("LensFlareGlare");

	// 그려질 point를 카운트 한다.
	FIntPoint TileCount = QuaterResViewport.Size();
	TileCount.X /= 2;
	TileCount.Y /= 2;
	int32 Amount = TileCount.X * TileCount.Y;

	FVector2f BufferRatio = FVector2f(
		static_cast<float>(QuaterResViewport.Height()) / QuaterResViewport.Width(),
		1.0f
	);

	FRDGTextureDesc TextureDesc;
	TextureDesc.Reset();
	TextureDesc.Extent = QuaterResViewport.Size();
	TextureDesc.Format = PF_FloatRGB;
	TextureDesc.ClearValue = FClearValueBinding::Black;
	FRDGTextureRef GlareTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("TyTLensFlareGlare"));

	GlareTexturePixelSize.X = 1.0f / QuaterResViewport.Width();
	GlareTexturePixelSize.Y = 1.0f / QuaterResViewport.Height();

	FVector2f BufferSize = FVector2f(QuaterResViewport.Size());
	FTyTLensFlareGlarePassParameters* PassParameters = GraphBuilder.AllocParameters<FTyTLensFlareGlarePassParameters>();
	PassParameters->RenderTargets[0] = FRenderTargetBinding(GlareTexture, ERenderTargetLoadAction::EClear);
	PassParameters->InputTexture = ThresholdTexture;

	FTyTLensFlareGlareVS::FParameters VertexParameters;
	VertexParameters.PassParameters = *PassParameters;
	VertexParameters.InputSampler = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI();
	VertexParameters.TileCount = TileCount;
	VertexParameters.PixelSize = GlareTexturePixelSize;
	VertexParameters.BufferSize = BufferSize;

	FTyTLensFlareGlareGS::FParameters GeometryParameters;
	GeometryParameters.BufferSize = BufferSize;
	GeometryParameters.BufferRatio = BufferRatio;
	GeometryParameters.GlareDivider = Settings->GlareDivider;
	GeometryParameters.GlareIntensity = Settings->GlareIntensity;
	GET_SCALAR_ARRAY_ELEMENT(GeometryParameters.GlareScales, 0) = Settings->GlareScale.X;
	GET_SCALAR_ARRAY_ELEMENT(GeometryParameters.GlareScales, 1) = Settings->GlareScale.Y;
	GET_SCALAR_ARRAY_ELEMENT(GeometryParameters.GlareScales, 2) = Settings->GlareScale.Z;
	GeometryParameters.GlareTint = Settings->GlareTint;

	FTyTLensFlareGlarePS::FParameters PixelParameters;
	PixelParameters.GlareTexture = Settings->GlareLineMask ?
		Settings->GlareLineMask->GetResource()->TextureRHI : GWhiteTexture->TextureRHI;
	PixelParameters.GlareSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();


	TShaderMapRef<FTyTLensFlareGlareVS> VertexShader(ViewInfo.ShaderMap);
	TShaderMapRef<FTyTLensFlareGlareGS> GeometryShader(ViewInfo.ShaderMap);
	TShaderMapRef<FTyTLensFlareGlarePS> PixelShader(ViewInfo.ShaderMap);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("%s", *PassName),
		PassParameters,
		ERDGPassFlags::Raster,
		[
			VertexShader, VertexParameters,
			GeometryShader, GeometryParameters,
			PixelShader, PixelParameters,
			QuaterResViewport, Amount
		] (FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.SetViewport(
				QuaterResViewport.Min.X, QuaterResViewport.Min.Y, 0.0f,
				QuaterResViewport.Max.X, QuaterResViewport.Max.Y, 1.0f
			);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.SetGeometryShader(GeometryShader.GetGeometryShader());
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_PointList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VertexParameters);
			SetShaderParameters(RHICmdList, GeometryShader, GeometryShader.GetGeometryShader(), GeometryParameters);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PixelParameters);

			RHICmdList.SetStreamSource(0, nullptr, 0);
			RHICmdList.DrawPrimitive(0, 1, Amount);
		});

	return GlareTexture;
}

void AddTyTLensFlarePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& ViewInfo,
	const FTyTLensFlareInputs& Inputs,
	FTyTLensFlareOutputs& Outputs
)
{
	check(Inputs.HalfSceneColor);
	check(Inputs.Settings);

	RDG_GPU_STAT_SCOPE(GraphBuilder, TyTLensFlare);
	RDG_EVENT_SCOPE(GraphBuilder, "TyTLensFlare");

	bool bGhostPass = Inputs.Settings->GhostIntensity > SMALL_NUMBER || Inputs.Settings->HaloIntensity > SMALL_NUMBER;
	bool bGlarePass = Inputs.Settings->GlareIntensity > SMALL_NUMBER;
	
	if (!bGhostPass && !bGlarePass)
	{
		return;
	}

	FRDGTextureRef ThresholdTexture = AddTyTLensFlareThresholdPass(
		GraphBuilder,
		ViewInfo,
		Inputs.HalfSceneColor,
		Inputs.HalfResViewport
	);

	if (bGhostPass)
	{
		FRDGTextureRef ChromaTexture = AddTyTLensFlareChromaPass(
			GraphBuilder,
			ViewInfo,
			ThresholdTexture,
			Inputs.HalfResViewport,
			Inputs.Settings
		);

		FRDGTextureRef GhostsTexture = AddTyTLensFlareGhostsPass(
			GraphBuilder,
			ViewInfo,
			ChromaTexture,
			Inputs.HalfResViewport,
			Inputs.Settings
		);

		Outputs.GhostsHaloTexture = AddTyTLensFlareHaloPass(
			GraphBuilder,
			ViewInfo,
			ThresholdTexture,
			GhostsTexture,
			Inputs.HalfResViewport,
			Inputs.Settings
		);
	}
	

	if (bGlarePass)
	{
		Outputs.GlareTexture = AddTyTLensFlareGlarePass(
			GraphBuilder,
			ViewInfo,
			ThresholdTexture,
			Inputs.HalfResViewport,
			Inputs.Settings,
			Outputs.GlareTexturePixelSize
		);
	}
}
