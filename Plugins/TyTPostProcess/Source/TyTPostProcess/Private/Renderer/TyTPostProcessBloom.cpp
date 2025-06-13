// Fill out your copyright notice in the Description page of Project Settings.


#include "Renderer/TyTPostProcessBloom.h"
#include "SceneRendering.h"
#include "SubSystems/TyTPostProcessSubSystem.h"
#include "DataAssets/TyTPostProcessSettingsAsset.h"

TAutoConsoleVariable<int32> CVarTyTBloomQuality(
    TEXT("r.TyT.BloomQuality"),
    128,
    TEXT("Defulat: 128"),
    ECVF_Scalability | ECVF_RenderThreadSafe
);

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

DECLARE_GPU_DRAWCALL_STAT(TyTBloomSetup)

FRDGTextureRef AddTyTBloomSetupPass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    FRDGTextureRef HalfResSceneColor,
    const FIntRect& HalfResViewport
)
{
    // Unreal Insights
    RDG_GPU_STAT_SCOPE(GraphBuilder, TyTBloomSetup);
    // Render Doc
    RDG_EVENT_SCOPE(GraphBuilder, "TyTBloomSetup");

    const FTyTBloomSettings* Settings = UTyTPostProcessSettingsAsset::GetBloomSettings();
    float Threshold = Settings ? Settings->Threshold : -1;
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

DECLARE_GPU_DRAWCALL_STAT(TyTBloom);

FRDGTextureRef AddTyTGaussianBloomPass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FTyTBloomDownSampleChain& DownSampleChain
)
{
    // Unreal Insights
    RDG_GPU_STAT_SCOPE(GraphBuilder, TyTBloom);
    // Render Doc
    RDG_EVENT_SCOPE(GraphBuilder, "TyTBloom");

    static const FString PassName = TEXT("TyTGaussianBloom");

    const FTyTBloomSettings* Settings = UTyTPostProcessSettingsAsset::GetBloomSettings();

    if (!Settings)
    {
        static FTyTBloomSettings DefaultSettings;
        Settings = &DefaultSettings;
    }
    TStaticArray<FLinearColor, FTyTBloomDownSampleChain::StageCount> TintColor = {
        Settings->Tint1, Settings->Tint2,
        Settings->Tint3, Settings->Tint4,
        Settings->Tint5, Settings->Tint6
    };
    float KernalSizeScale = Settings->KernalSizeScale;
    TStaticArray<float, FTyTBloomDownSampleChain::StageCount> KernalSizes = {
        Settings->KernalSize1, Settings->KernalSize2,
        Settings->KernalSize3, Settings->KernalSize4,
        Settings->KernalSize5, Settings->KernalSize6 
    };
    float Intensity = Settings->Intensity;

    int BloomQuality = CVarTyTBloomQuality.GetValueOnRenderThread();
    float TintScale = 1.0f / BloomQuality * Intensity;


    FRDGTextureRef OutputTexture = nullptr;
    for (int i = DownSampleChain.StageCount - 1; i >= 0; --i)
    {
        FRDGTextureRef InputTexture = DownSampleChain.GetTexture(i);
        const FIntRect InputViewport = DownSampleChain.GetViewport(i);

        int BlurRadius = CalcBlurRadius(InputViewport.Width(), KernalSizes[i] * KernalSizeScale);

        OutputTexture = AddTyTGaussianBlurPass(
            GraphBuilder,
            ViewInfo,
            PassName,
            InputTexture,
            InputViewport,
            OutputTexture,
            BlurRadius,
            TintColor[i] * TintScale
        );
    }

    return OutputTexture;
}

FRDGTextureRef AddTyTDualKawaseBloomPass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FTyTBloomDownSampleChain& DownSampleChain
)
{
    // Unreal Insights
    RDG_GPU_STAT_SCOPE(GraphBuilder, TyTBloom);
    // Render Doc
    RDG_EVENT_SCOPE(GraphBuilder, "TyTBloom");

    static const FString PassName = TEXT("TyTDualKawaseBloom");

    // TintColor * Intensity * Weight
    // TODO: TintColor, Intensity, SampleLevel 하드코딩
    static TStaticArray<FLinearColor, FTyTBloomDownSampleChain::StageCount> TintColor =
    {
        FLinearColor(0.3265, 0.3265, 0.3265),
        FLinearColor(0.138 , 0.138 , 0.138),
        FLinearColor(0.1176, 0.1176, 0.1176),
        FLinearColor(0.066 , 0.066 , 0.066),
        FLinearColor(0.066 , 0.066 , 0.066),
        FLinearColor(0.061 , 0.061 , 0.061),
    };
    static float Intensity = 0.675f;
    static TStaticArray<int, FTyTBloomDownSampleChain::StageCount> SampleLevels = {
        1, 1, 2, 2, 3, 3
    };

    FRDGTextureRef OutputTexture = nullptr;
    for (int i = DownSampleChain.StageCount - 1; i >= 0; --i)
    {
        FRDGTextureRef InputTexture = DownSampleChain.GetTexture(i);
        const FIntRect InputViewport = DownSampleChain.GetViewport(i);

        OutputTexture = AddTyTDualKawaseBlurPass(
            GraphBuilder,
            ViewInfo,
            TEXT("TyTDualKawaseBloom") + FString::Printf(TEXT("(%d/%d)"), 1, static_cast<int32>(FMath::Pow(2.0f, i + 1))),
            InputTexture,
            InputViewport,
            TintColor[i] * Intensity,
            SampleLevels[i],
            OutputTexture
        );
    }

    return OutputTexture;
}