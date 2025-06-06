// Fill out your copyright notice in the Description page of Project Settings.


#include "Renderer/TyTPostProcessDrawUtils.h"
#include "SceneRendering.h"

IMPLEMENT_GLOBAL_SHADER(FTyTScreenPassVS, "/TyTPostProcess/TyTScreenPassVertexShader.usf", "MainVS", SF_Vertex);

BEGIN_SHADER_PARAMETER_STRUCT(FTyTScreenPassPSParameters, TYTPOSTPROCESS_API)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InTexture)
    SHADER_PARAMETER_SAMPLER(SamplerState, BilinearWrapSampler)
    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FTyTScreenPassPS : public FGlobalShader
{
public:
    DECLARE_EXPORTED_GLOBAL_SHADER(FTyTScreenPassPS, TYTPOSTPROCESS_API);
    using FParameters = FTyTScreenPassPSParameters;
    SHADER_USE_PARAMETER_STRUCT(FTyTScreenPassPS, FGlobalShader);
};
IMPLEMENT_GLOBAL_SHADER(FTyTScreenPassPS, "/TyTPostProcess/TyTScreenPassPixelShader.usf", "MainPS", SF_Pixel);

void CopyTexture2D(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InTexture,
    const FRenderTargetBinding& RenderTarget,
    const FIntRect& RenderTargetViewport
)
{
    FTyTScreenPassPS::FParameters* Params = GraphBuilder.AllocParameters< FTyTScreenPassPS::FParameters>();
    Params->RenderTargets[0] = RenderTarget;
    Params->InTexture = InTexture;
    Params->BilinearWrapSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

    TShaderMapRef<FTyTScreenPassVS> VS(ViewInfo.ShaderMap);
    TShaderMapRef<FTyTScreenPassPS> PS(ViewInfo.ShaderMap);

    AddTyTScreenPass(
        GraphBuilder,
        PassName,
        Params,
        VS,
        PS,
        TStaticBlendState<>::GetRHI(),
        RenderTargetViewport
    );
}

BEGIN_SHADER_PARAMETER_STRUCT(FDownSampleCSParameters, TYTPOSTPROCESS_API)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InTexture)
    SHADER_PARAMETER(FUintVector2, InTextureViewport)
    SHADER_PARAMETER(FVector2f, InTextureSize)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, OutTexture)
    SHADER_PARAMETER(FUintVector2, OutTextureViewport)
    SHADER_PARAMETER_SAMPLER(SamplerState, BilinearWrapSampler)
END_SHADER_PARAMETER_STRUCT()

class FDownSamplingCS : public FGlobalShader
{
public:
    DECLARE_EXPORTED_GLOBAL_SHADER(FDownSamplingCS, TYTPOSTPROCESS_API);
    using FParameters = FDownSampleCSParameters;
    SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FDownSamplingCS, FGlobalShader);

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

        SET_SHADER_DEFINE(OutEnvironment, THREADS_X, 16);
        SET_SHADER_DEFINE(OutEnvironment, THREADS_Y, 16);
    }
};
IMPLEMENT_GLOBAL_SHADER(FDownSamplingCS, "/TyTPostProcess/DownSampleComputeShader.usf", "MainCS", SF_Compute);

FRDGTextureRef DownSampleTextureCS(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InTexture,
    const FIntRect& Viewport,
    uint32 ScaleFactor
)
{
    FIntRect ScaledViewport = Viewport / ScaleFactor;

    FRDGTextureDesc TextureDesc;
    TextureDesc.Reset();
    TextureDesc.Extent = ScaledViewport.Size();
    TextureDesc.Format = EPixelFormat::PF_FloatRGB;
    TextureDesc.ClearValue = FClearValueBinding::Black;
    TextureDesc.Flags |= ETextureCreateFlags::UAV;
    FRDGTextureRef OutTexture = GraphBuilder.CreateTexture(TextureDesc, *PassName);
    FRDGTextureUAVRef OutTextureUAV = GraphBuilder.CreateUAV(OutTexture);

    FDownSamplingCS::FParameters* Parameters = GraphBuilder.AllocParameters< FDownSamplingCS::FParameters>();
    Parameters->InTexture = InTexture;
    Parameters->InTextureViewport = FUintVector2(Viewport.Width(), Viewport.Height());
    Parameters->InTextureSize = FVector2f(1, 1);
    Parameters->OutTexture = OutTextureUAV;
    Parameters->OutTextureViewport = FUintVector2(ScaledViewport.Width(), ScaledViewport.Height());
    Parameters->BilinearWrapSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

    TShaderMapRef<FDownSamplingCS> CS(ViewInfo.ShaderMap);

    FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ScaledViewport.Size(), FIntPoint(16, 16));

    FComputeShaderUtils::AddPass(
        GraphBuilder,
        FRDGEventName(*PassName),
        ERDGPassFlags::Compute,
        CS,
        Parameters,
        GroupCount
    );

    return OutTexture;
}

BEGIN_SHADER_PARAMETER_STRUCT(FTyTDownSamplePSParameters, TYTPOSTPROCESS_API)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InTexture)
    SHADER_PARAMETER_SAMPLER(SamplerState, BilinearClampSampler)
    SHADER_PARAMETER(FVector2f, InInvViewport)
    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FTyTDownSamplePS : public FGlobalShader
{
public:
    DECLARE_EXPORTED_GLOBAL_SHADER(FTyTDownSamplePS, TYTPOSTPROCESS_API);
    using FParameters = FTyTDownSamplePSParameters;
    SHADER_USE_PARAMETER_STRUCT(FTyTDownSamplePS, FGlobalShader);
};
IMPLEMENT_GLOBAL_SHADER(FTyTDownSamplePS, "/TyTPostProcess/TyTDownSamplePixelShader.usf", "MainPS", SF_Pixel);

FRDGTextureRef AddTyTDownSamplePass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InTexture,
    const FIntRect& InViewport
)
{
    FIntRect OutViewport = InViewport / 2.0f;

    FRDGTextureDesc TextureDesc;
    TextureDesc.Reset();
    TextureDesc.Extent = OutViewport.Size();
    TextureDesc.Format = EPixelFormat::PF_FloatRGB;
    TextureDesc.ClearValue = FClearValueBinding::Black;
    TextureDesc.Flags |= ETextureCreateFlags::UAV;
    FRDGTextureRef OutTexture = GraphBuilder.CreateTexture(TextureDesc, *PassName);

    FTyTDownSamplePS::FParameters* Parameters = GraphBuilder.AllocParameters<FTyTDownSamplePS::FParameters>();
    Parameters->InTexture = InTexture;
    Parameters->InInvViewport = FVector2f(1.0f / InViewport.Size().X, 1.0f / InViewport.Size().Y);
    Parameters->BilinearClampSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
    Parameters->RenderTargets[0] = FRenderTargetBinding(OutTexture, ERenderTargetLoadAction::ELoad);

    TShaderMapRef<FTyTScreenPassVS> VS(ViewInfo.ShaderMap);
    TShaderMapRef<FTyTDownSamplePS> PS(ViewInfo.ShaderMap);

    AddTyTScreenPass(
        GraphBuilder,
        PassName,
        Parameters,
        VS,
        PS,
        TStaticBlendState<>::GetRHI(),
        OutViewport
    );

    return OutTexture;
}