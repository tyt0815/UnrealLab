// Fill out your copyright notice in the Description page of Project Settings.


#include "Renderer/TyTPostProcessDrawUtils.h"
#include "SceneRendering.h"

IMPLEMENT_GLOBAL_SHADER(FTyTScreenPassVS, "/TyTPostProcess/TyTScreenPassVertexShader.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FTyTScreenPassUseScreenPositionVS, "/TyTPostProcess/TyTScreenPassVertexShader.usf", "MainVS", SF_Vertex);

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
    Params->BilinearWrapSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

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
    SHADER_USE_PARAMETER_STRUCT(FDownSamplingCS, FGlobalShader);

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

DECLARE_GPU_DRAWCALL_STAT(TyTDownSample);

FRDGTextureRef AddTyTDownSamplePass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InTexture,
    const FIntRect& InViewport
)
{
    // Unreal Insights
    RDG_GPU_STAT_SCOPE(GraphBuilder, TyTDownSample);
    // Render Doc
    RDG_EVENT_SCOPE(GraphBuilder, "TyTDownSample");

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

BEGIN_SHADER_PARAMETER_STRUCT(FTyTGaussianBlurParameter, TYTPOSTPROCESS_API)
    SHADER_PARAMETER_ARRAY(FLinearColor, SampleWeights, [MAX_TYT_SAMPLE_COUNT])
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
    SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AdditiveTexture)
    SHADER_PARAMETER_SAMPLER(SamplerState, AdditiveSampler)
END_SHADER_PARAMETER_STRUCT()

void GetTyTGaussianBlurParameter(
    FTyTGaussianBlurParameter& OutParameters,
    TArrayView<const FLinearColor> SampleWeights,
    FRDGTextureRef InputTexture, FRHISamplerState* InputSamplerState,
    FRDGTextureRef AdditiveTexture, FRHISamplerState* AdditiveSamplerState
)
{
    for (int i = 0; i < SampleWeights.Num(); ++i)
    {
        OutParameters.SampleWeights[i] = SampleWeights[i];
    }
    OutParameters.InputTexture = InputTexture;
    OutParameters.InputSampler = InputSamplerState;
    OutParameters.AdditiveTexture = AdditiveTexture;
    OutParameters.AdditiveSampler = AdditiveSamplerState;
}

class FTyTGaussianBlurShader : public FGlobalShader
{
public:
    class FCombineAdditive : SHADER_PERMUTATION_BOOL("USE_COMBINE_ADDITIVE");
    class FBlurRadius : SHADER_PERMUTATION_INT("BLUR_RADIUS", MAX_TYT_BLUR_RADIUS + 1);

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("MAX_TYT_SAMPLE_COUNT"), MAX_TYT_SAMPLE_COUNT);
    }
};



class FTyTGaussianBlurXPS : public FTyTGaussianBlurShader
{
public:
    DECLARE_EXPORTED_GLOBAL_SHADER(FTyTGaussianBlurXPS, TYTPOSTPROCESS_API);
    SHADER_USE_PARAMETER_STRUCT(FTyTGaussianBlurXPS, FTyTGaussianBlurShader);
    using FPermutationDomain = TShaderPermutationDomain<FCombineAdditive, FBlurRadius>;

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, TYTPOSTPROCESS_API)
        SHADER_PARAMETER_STRUCT_INCLUDE(FTyTGaussianBlurParameter, GaussianBlur)
        SHADER_PARAMETER(float, InvTextureWidth)
        RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()
};
 IMPLEMENT_GLOBAL_SHADER(FTyTGaussianBlurXPS, "/TyTPostProcess/TyTGaussianBlurFilter.usf", "BlurX", SF_Pixel);

class FTyTGaussianBlurYPS : public FTyTGaussianBlurShader
{
public:
    DECLARE_EXPORTED_GLOBAL_SHADER(FTyTGaussianBlurYPS, TYTPOSTPROCESS_API);
    SHADER_USE_PARAMETER_STRUCT(FTyTGaussianBlurYPS, FTyTGaussianBlurShader);

    using FPermutationDomain = TShaderPermutationDomain<FCombineAdditive, FBlurRadius>;

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, TYTPOSTPROCESS_API)
        SHADER_PARAMETER_STRUCT_INCLUDE(FTyTGaussianBlurParameter, GaussianBlur)
        SHADER_PARAMETER(float, InvTextureHeight)
        RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FTyTGaussianBlurYPS, "/TyTPostProcess/TyTGaussianBlurFilter.usf", "BlurY", SF_Pixel);

/*
* @param Width 텍스처 너비
* @param KernalSizePercent 백분률값
* @return Width의 KernalSizePercent% 값
*/
int CalcBlurRadius(const float& Width, const float& KernalSizePercent)
{
    return FMath::Min(Width * KernalSizePercent * 0.01, MAX_TYT_BLUR_RADIUS);
}

TArray<float> CalcGaussianWeights(const int& Radius)
{
    check(Radius <= MAX_TYT_BLUR_RADIUS);   
    const float Sigma = FMath::Max(Radius / 2, 1.0f);
    const float TwoSigma2 = 2 * Sigma * Sigma;
    TArray<float> Weights;

    float WeightsSum = 0;
    for (int i = -Radius; i <= Radius; ++i)
    {
        float Weight = FMath::Exp(-(i * i) / (TwoSigma2));
        WeightsSum += Weight;
        Weights.Add(Weight);
    }
    for (float& Weight : Weights)
    {
        Weight /= WeightsSum;
    }

    return Weights;
}

FRDGTextureRef AddTyTGaussianBlurXPass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InputTexture,
    const FIntRect& InputTextureViewport,
    int BlurRadius,
    TArrayView<const FLinearColor> Weights
)
{
    check(BlurRadius <= MAX_TYT_BLUR_RADIUS);
    check(Weights.Num() == BlurRadius * 2 + 1);

    FRDGTextureDesc Desc;
    Desc.Reset();
    Desc.Extent = InputTextureViewport.Size();
    Desc.Format = EPixelFormat::PF_FloatRGB;
    Desc.ClearValue = FClearValueBinding::Green;
    FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(Desc, TEXT("BloomX"));

    FTyTGaussianBlurXPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTyTGaussianBlurXPS::FParameters>();
    GetTyTGaussianBlurParameter(
        PassParameters->GaussianBlur,
        Weights,
        InputTexture,
        TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI(),
        nullptr,
        nullptr
    );
    PassParameters->InvTextureWidth = 1.0f / InputTextureViewport.Size().X;
    PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ELoad);

    FTyTGaussianBlurXPS::FPermutationDomain Permutation;
    Permutation.Set<FTyTGaussianBlurXPS::FCombineAdditive>(false);
    Permutation.Set<FTyTGaussianBlurXPS::FBlurRadius>(BlurRadius);

    TShaderMapRef<FTyTScreenPassVS> VS(ViewInfo.ShaderMap);
    TShaderMapRef<FTyTGaussianBlurXPS> PS(ViewInfo.ShaderMap, Permutation);

    AddTyTScreenPass(
        GraphBuilder,
        PassName + ".X",
        PassParameters,
        VS,
        PS,
        TStaticBlendState<>::GetRHI(),
        InputTextureViewport
    );

    return OutputTexture;
}

FRDGTextureRef AddTyTGaussianBlurYPass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InputTexture,
    const FIntRect InputTextureViewport,
    FRDGTextureRef AdditiveTexture,
    int BlurRadius,
    TArrayView<const FLinearColor> Weights
)
{
    check(BlurRadius <= MAX_TYT_BLUR_RADIUS);
    check(Weights.Num() == BlurRadius * 2 + 1);

    FRDGTextureDesc Desc;
    Desc.Reset();
    Desc.Extent = InputTextureViewport.Size();
    Desc.Format = EPixelFormat::PF_FloatRGB;
    Desc.ClearValue = FClearValueBinding::Green;
    FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(Desc, TEXT("BloomY"));

    FTyTGaussianBlurYPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTyTGaussianBlurYPS::FParameters>();
    GetTyTGaussianBlurParameter(
        PassParameters->GaussianBlur,
        Weights,
        InputTexture,
        TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI(),
        AdditiveTexture,
        TStaticSamplerState<SF_Bilinear>::GetRHI()
    );
    PassParameters->InvTextureHeight = 1.0f / InputTextureViewport.Size().Y;
    PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ELoad);

    FTyTGaussianBlurYPS::FPermutationDomain Permutation;
    Permutation.Set<FTyTGaussianBlurYPS::FCombineAdditive>(AdditiveTexture != nullptr);
    Permutation.Set<FTyTGaussianBlurYPS::FBlurRadius>(BlurRadius);

    TShaderMapRef<FTyTScreenPassVS> VS(ViewInfo.ShaderMap);
    TShaderMapRef<FTyTGaussianBlurYPS> PS(ViewInfo.ShaderMap, Permutation);

    AddTyTScreenPass(
        GraphBuilder,
        PassName + ".Y",
        PassParameters,
        VS,
        PS,
        TStaticBlendState<>::GetRHI(),
        InputTextureViewport
    );

    return OutputTexture;
}

FRDGTextureRef AddTyTGaussianBlurPass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InputTexture,
    const FIntRect InputTextureViewport,
    FRDGTextureRef AdditiveTexture,
    int BlurRadius,
    const FLinearColor& Tint
)
{
    TArray<float> GaussianWeights = CalcGaussianWeights(BlurRadius);

    TArray<FLinearColor> Weights;
    for (float GaussianWeight : GaussianWeights)
    {
        Weights.Add(FLinearColor(GaussianWeight, GaussianWeight, GaussianWeight, GaussianWeight));
    }

    FRDGTextureRef BloomX = AddTyTGaussianBlurXPass(
        GraphBuilder,
        ViewInfo,
        PassName + ".GaussianBlur",
        InputTexture,
        InputTextureViewport,
        BlurRadius,
        Weights
    );

    for (FLinearColor& Weight : Weights)
    {
        Weight = Tint * Weight.R;
    }

    FRDGTextureRef Bloom = AddTyTGaussianBlurYPass(
        GraphBuilder,
        ViewInfo,
        PassName + ".GaussianBlur",
        BloomX,
        InputTextureViewport,
        AdditiveTexture,
        BlurRadius,
        Weights
    );

    return Bloom;
}

BEGIN_SHADER_PARAMETER_STRUCT(FTyTDualKawaseBlurShaderParameters, TYTPOSTPROCESS_API)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
    SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
    SHADER_PARAMETER(FVector2f, InvInputTextureViewportSize)
    SHADER_PARAMETER(FVector4f, InputTint)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AdditiveTexture)
    SHADER_PARAMETER_SAMPLER(SamplerState, AdditiveSampler)

    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FTyTDualKawaseBlurShader : public FGlobalShader
{
public:
    using FParameters = FTyTDualKawaseBlurShaderParameters;
    SHADER_USE_PARAMETER_STRUCT(FTyTDualKawaseBlurShader, FGlobalShader);

    class FTint : SHADER_PERMUTATION_BOOL("USE_TINT");
    class FCombineAdditive : SHADER_PERMUTATION_BOOL("USE_COMBINE_ADDITIVE");

    using FPermutationDomain = TShaderPermutationDomain<FCombineAdditive, FTint>;
};

class FTyTDualKawaseBlurDownSamplePS : public FTyTDualKawaseBlurShader
{
public:
    DECLARE_EXPORTED_GLOBAL_SHADER(FTyTDualKawaseBlurDownSamplePS, TYTPOSTPROCESS_API);
    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FTyTDualKawaseBlurShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("DOWNSAMPLE"), 1);
    }
};
IMPLEMENT_GLOBAL_SHADER(FTyTDualKawaseBlurDownSamplePS, "/TyTPostProcess/TyTDualKawaseBlurPixel.usf", "DownSamplePS", SF_Pixel);

class FTyTDualKawaseBlurUpSamplePS : public FTyTDualKawaseBlurShader
{
public:
    DECLARE_EXPORTED_GLOBAL_SHADER(FTyTDualKawaseBlurUpSamplePS, TYTPOSTPROCESS_API);
    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FTyTDualKawaseBlurShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("UPSAMPLE"), 1);
    }
};
IMPLEMENT_GLOBAL_SHADER(FTyTDualKawaseBlurUpSamplePS, "/TyTPostProcess/TyTDualKawaseBlurPixel.usf", "UpSamplePS", SF_Pixel);

void AddTyTDualKwaseDownSamplePass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InputTexture,
    FVector2f InvInputTextureViewportSize,
    FRDGTextureRef RenderTarget,
    FIntRect RenderTargetViewport,
    TShaderMapRef<FTyTScreenPassVS> VertexShader,
    TShaderMapRef<FTyTDualKawaseBlurDownSamplePS> PixelShader
)
{
    FTyTDualKawaseBlurShaderParameters* PassParameters = GraphBuilder.AllocParameters<FTyTDualKawaseBlurShaderParameters>();
    PassParameters->InputTexture = InputTexture;
    PassParameters->InputSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
    PassParameters->InvInputTextureViewportSize = InvInputTextureViewportSize;
    PassParameters->RenderTargets[0] = FRenderTargetBinding(RenderTarget, ERenderTargetLoadAction::ENoAction);

    AddTyTScreenPass(
        GraphBuilder,
        PassName + FString::Printf(TEXT(".DownSample_%dx%d"), RenderTargetViewport.Size().X, RenderTargetViewport.Size().Y),
        PassParameters,
        VertexShader,
        PixelShader,
        TStaticBlendState<>::GetRHI(),
        RenderTargetViewport
    );
}

void AddTyTDualKwaseUpSamplePass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InputTexture,
    FVector2f InvInputTextureViewportSize,
    const FVector4f& InputTint,
    FRDGTextureRef AdditiveTexture,
    FRDGTextureRef RenderTarget,
    FIntRect RenderTargetViewport,
    TShaderMapRef<FTyTScreenPassVS> VertexShader,
    TShaderMapRef<FTyTDualKawaseBlurUpSamplePS> PixelShader
)
{
    FTyTDualKawaseBlurShaderParameters* PassParameters = GraphBuilder.AllocParameters<FTyTDualKawaseBlurShaderParameters>();
    PassParameters->InputTexture = InputTexture;
    PassParameters->InputSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
    PassParameters->InvInputTextureViewportSize = InvInputTextureViewportSize;
    PassParameters->InputTint = InputTint;
    PassParameters->AdditiveTexture = AdditiveTexture;
    PassParameters->AdditiveSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
    PassParameters->RenderTargets[0] = FRenderTargetBinding(RenderTarget, ERenderTargetLoadAction::ENoAction);

    AddTyTScreenPass(
        GraphBuilder,
        PassName + FString::Printf(TEXT(".UpSample_%dx%d"), RenderTargetViewport.Size().X, RenderTargetViewport.Size().Y),
        PassParameters,
        VertexShader,
        PixelShader,
        TStaticBlendState<>::GetRHI(),
        RenderTargetViewport
    );
}

DECLARE_GPU_DRAWCALL_STAT(TyTKawaseBlur);

FRDGTextureRef AddTyTDualKawaseBlurPass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InputTexture,
    const FIntRect& InputViewport,
    const FLinearColor& Tint,
    int SampleLevel,
    FRDGTextureRef AdditiveTexture
)
{
    // TODO: 함수 정리좀 하자 이친구야
    // Unreal Insights
    RDG_GPU_STAT_SCOPE(GraphBuilder, TyTKawaseBlur);
    // Render Doc
    RDG_EVENT_SCOPE(GraphBuilder, "TyTKawaseBlur");

    if (SampleLevel < 1)
    {
        return InputTexture;
    }
    
    TArray<FIntRect> ViewportLevel;
    ViewportLevel.SetNum(SampleLevel + 1);
    ViewportLevel[0] = InputViewport;
    for (int i = 1; i < ViewportLevel.Num(); ++i)
    {
        ViewportLevel[i] = ViewportLevel[i - 1] / 2.0f;
    }
    TArray<FRDGTextureRef> TextureLevel;
    TextureLevel.SetNum(SampleLevel + 1);

    FRDGTextureDesc Desc;
    Desc.Reset();
    Desc.Format = PF_R16G16B16A16_UNORM;
    Desc.ClearValue = FClearValueBinding::Black;
    for (int i = 0; i < TextureLevel.Num(); ++i)
    {
        Desc.Extent = ViewportLevel[i].Size();
        TextureLevel[i] = GraphBuilder.CreateTexture(Desc, *(TEXT("Kawase ") + ViewportLevel[i].Size().ToString()));
    }
    TArray<FVector2f> InvInputTextureViewportSizeLevel;
    InvInputTextureViewportSizeLevel.SetNum(SampleLevel + 1);
    for (int i = 0; i < InvInputTextureViewportSizeLevel.Num(); ++i)
    {
        InvInputTextureViewportSizeLevel[i] = FVector2f(1.0f / ViewportLevel[i].Size().X, 1.0f / ViewportLevel[i].Size().Y);
    }

    TShaderMapRef<FTyTScreenPassVS> VertexShader(ViewInfo.ShaderMap);

    FTyTDualKawaseBlurShader::FPermutationDomain Permutation;
    Permutation.Set<FTyTDualKawaseBlurShader::FTint>(false);
    Permutation.Set<FTyTDualKawaseBlurShader::FCombineAdditive>(false);
    int InputTextureLevel = 0;

    // Down Sampling
    TShaderMapRef<FTyTDualKawaseBlurDownSamplePS> DownSamplePS(ViewInfo.ShaderMap, Permutation);
    AddTyTDualKwaseDownSamplePass(
        GraphBuilder,
        ViewInfo,
        PassName,
        InputTexture,
        InvInputTextureViewportSizeLevel[InputTextureLevel],
        TextureLevel[InputTextureLevel + 1],
        ViewportLevel[InputTextureLevel + 1],
        VertexShader,
        DownSamplePS
    );

    ++InputTextureLevel;

    while (InputTextureLevel < SampleLevel)
    {
        AddTyTDualKwaseDownSamplePass(
            GraphBuilder,
            ViewInfo,
            PassName,
            TextureLevel[InputTextureLevel],
            InvInputTextureViewportSizeLevel[InputTextureLevel],
            TextureLevel[InputTextureLevel + 1],
            ViewportLevel[InputTextureLevel + 1],
            VertexShader,
            DownSamplePS
        );

        ++InputTextureLevel;
    }

    // TODO: Up Sampling
    // UPSampling
    TShaderMapRef<FTyTDualKawaseBlurUpSamplePS> UpSamplePS(ViewInfo.ShaderMap, Permutation);
    while (InputTextureLevel > 1)
    {
        AddTyTDualKwaseUpSamplePass(
            GraphBuilder,
            ViewInfo,
            PassName,
            TextureLevel[InputTextureLevel],
            InvInputTextureViewportSizeLevel[InputTextureLevel],
            Tint,
            nullptr,
            TextureLevel[InputTextureLevel - 1],
            ViewportLevel[InputTextureLevel - 1],
            VertexShader,
            UpSamplePS
        );
        --InputTextureLevel;
    }

    Permutation.Set<FTyTDualKawaseBlurShader::FCombineAdditive>(AdditiveTexture != nullptr);
    Permutation.Set< FTyTDualKawaseBlurShader::FTint>(true);
    UpSamplePS = TShaderMapRef<FTyTDualKawaseBlurUpSamplePS>(ViewInfo.ShaderMap, Permutation);

    AddTyTDualKwaseUpSamplePass(
        GraphBuilder,
        ViewInfo,
        PassName,
        TextureLevel[InputTextureLevel],
        InvInputTextureViewportSizeLevel[InputTextureLevel],
        Tint,
        AdditiveTexture,
        TextureLevel[InputTextureLevel - 1],
        ViewportLevel[InputTextureLevel - 1],
        VertexShader,
        UpSamplePS
    );

    return TextureLevel[0];
}

DECLARE_GPU_DRAWCALL_STAT(TyTDownSampleChain)

void FTyTBloomDownSampleChain::Init(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    FRDGTextureRef BloomSetupTexture,
    const FIntRect& Viewport
)
{
    // Unreal Insights
    RDG_GPU_STAT_SCOPE(GraphBuilder, TyTDownSampleChain);
    // Render Doc
    RDG_EVENT_SCOPE(GraphBuilder, "TyTDownSampleChain");


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
    Viewports = Viewport;

    for (int i = 1; i < StageCount; ++i)
    {
        Viewports[i] = Viewports[i - 1] / 2;
        Textures[i] = AddTyTDownSamplePass(
            GraphBuilder,
            ViewInfo,
            PassNames[i],
            Textures[i - 1],
            Viewports[i - 1]
        );
    }
}