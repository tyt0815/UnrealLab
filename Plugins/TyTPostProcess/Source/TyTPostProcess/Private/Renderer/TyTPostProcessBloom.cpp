// Fill out your copyright notice in the Description page of Project Settings.


#include "Renderer/TyTPostProcessBloom.h"
#include "Renderer/TyTPostProcessDrawUtils.h"
#include "SceneRendering.h"

BEGIN_SHADER_PARAMETER_STRUCT(FTyTBloomSetupPSParameters, TYTPOSTPROCESS_API)
    SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformShaderParameters)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
    SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
    SHADER_PARAMETER(float, BloomThreshold)
    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FTyTBloomSetupPS : public FGlobalShader
{
public:
    DECLARE_EXPORTED_GLOBAL_SHADER(FTyTBloomSetupPS, TYTPOSTPROCESS_API);

    class FThresholdDim : SHADER_PERMUTATION_BOOL("USE_THRESHOLD");
    using FPermutationDomain = TShaderPermutationDomain<FThresholdDim>;

    using FParameters = FTyTBloomSetupPSParameters;
    SHADER_USE_PARAMETER_STRUCT(FTyTBloomSetupPS, FGlobalShader);
};
IMPLEMENT_GLOBAL_SHADER(FTyTBloomSetupPS, "/TyTPostProcess/TyTBloomSetup.usf", "MainPS", SF_Pixel);

FRDGTextureRef AddTyTBloomSetupPass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    FRDGTextureRef HalfResSceneColor,
    const FIntRect& HalfResViewport
)
{
    // TODO: Threshold값 하드코딩
    float Threshold = 1.7576;
    const bool bThresholdEnabled = Threshold > -1;

    FRDGTextureDesc TextureDesc;
    TextureDesc.Reset();
    TextureDesc.Extent = HalfResViewport.Size();
    TextureDesc.Format = EPixelFormat::PF_FloatRGB;
    TextureDesc.ClearValue = FClearValueBinding::Black;
    FRDGTextureRef OutTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("TyTBloomSetup"));
	
    FTyTBloomSetupPS::FParameters* Parameters = GraphBuilder.AllocParameters<FTyTBloomSetupPS::FParameters>();
    Parameters->ViewUniformShaderParameters = ViewInfo.ViewUniformBuffer;
    Parameters->InputTexture = HalfResSceneColor;
    Parameters->InputSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
    Parameters->BloomThreshold = Threshold;
    Parameters->RenderTargets[0] = FRenderTargetBinding(OutTexture, ERenderTargetLoadAction::ELoad);

    FTyTBloomSetupPS::FPermutationDomain PermutationVector;
    PermutationVector.Set<FTyTBloomSetupPS::FThresholdDim>(bThresholdEnabled);

    TShaderMapRef<FTyTScreenPassVS> VS(ViewInfo.ShaderMap);
    TShaderMapRef<FTyTBloomSetupPS> PS(ViewInfo.ShaderMap, PermutationVector);

    AddTyTScreenPass(
        GraphBuilder,
        TEXT("TyTBloomSetup(1/2)"),
        Parameters,
        VS,
        PS,
        TStaticBlendState<>::GetRHI(),
        HalfResViewport
    );

	return OutTexture;
}

void FTyTBloomDownSampleChain::Init(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    FRDGTextureRef BloomSetupTexture,
    const FIntRect& Viewport
)
{
    static const TCHAR* PassNames[StageCount] = {
        nullptr,
        TEXT("TyTBloomSetup(1/4)"),
        TEXT("TyTBloomSetup(1/8)"),
        TEXT("TyTBloomSetup(1/16)"),
        TEXT("TyTBloomSetup(1/32)"),
        TEXT("TyTBloomSetup(1/64)"),
    };
    static_assert(UE_ARRAY_COUNT(PassNames) == StageCount, "PassNames size must equal StageCount");

    Textures[0] = BloomSetupTexture;

    for (int i = 1; i < StageCount; ++i)
    {
        Textures[i] = AddTyTDownSamplePass(
            GraphBuilder,
            ViewInfo,
            PassNames[i],
            Textures[i - 1],
            Viewport / FMath::Pow(2.0f, i - 1)
        );
    }
}



void AddTyTBloomPass(
    const FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FTyTBloomDownSampleChain& DownSampleChain,
    FRenderTargetBinding& RenderTarget,
    const FIntRect& RenderTargetViewport
)
{
    const TCHAR* PassName = TEXT("TyTBloom");


}