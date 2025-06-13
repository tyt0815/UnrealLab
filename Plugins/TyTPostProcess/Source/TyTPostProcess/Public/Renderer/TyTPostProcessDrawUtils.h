// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ScreenPass.h"
#include "Runtime/Renderer/Private/PostProcess/SceneFilterRendering.h"


class FTyTScreenPassVS : public FGlobalShader
{
public:
    DECLARE_EXPORTED_GLOBAL_SHADER(FTyTScreenPassVS, TYTPOSTPROCESS_API);
};

class FTyTScreenPassUseScreenPositionVS : public FGlobalShader
{
public:
    DECLARE_EXPORTED_GLOBAL_SHADER(FTyTScreenPassUseScreenPositionVS, TYTPOSTPROCESS_API);

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("USE_SCREEN_POSITION"), 1);
    }
};

template<typename TShaderParameters, typename TShaderClassVertex, typename TShaderClassPixel>
inline void AddTyTScreenPass(
    FRDGBuilder& GraphBuilder,
    const FString& PassName,
    TShaderParameters* PassParameters,
    TShaderMapRef<TShaderClassVertex> VertexShader,
    TShaderMapRef<TShaderClassPixel> PixelShader,
    FRHIBlendState* BlendState,
    const FIntRect& Viewport
)
{
    const FScreenPassPipelineState PipelineState(VertexShader, PixelShader, BlendState);

    GraphBuilder.AddPass(
        FRDGEventName(TEXT("%s"), *PassName),
        PassParameters,
        ERDGPassFlags::Raster,
        [PixelShader, PassParameters, Viewport, PipelineState](FRHICommandListImmediate& RHICmdList)
        {
            RHICmdList.SetViewport(
                Viewport.Min.X, Viewport.Min.Y, 0.0f,
                Viewport.Max.X, Viewport.Max.Y, 1.0f
            );

            SetScreenPassPipelineState(RHICmdList, PipelineState);

            SetShaderParameters(
                RHICmdList,
                PixelShader,
                PixelShader.GetPixelShader(),
                *PassParameters
            );

            DrawRectangle(
                RHICmdList,                            
                0.0f, 0.0f,                            
                Viewport.Width(), Viewport.Height(),   
                Viewport.Min.X, Viewport.Min.Y,        
                Viewport.Width(),                      
                Viewport.Height(),                     
                Viewport.Size(),                       
                Viewport.Size(),                       
                PipelineState.VertexShader,            
                EDrawRectangleFlags::EDRF_Default      
            );
        }
    );
}

void CopyTexture2D(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InPutTexture,
    const FRenderTargetBinding& RenderTarget,
    const FIntRect& RenderTargetViewport
);

FRDGTextureRef DownSampleTextureCS(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InTexture,
    const FIntRect& Viewport,
    uint32 ScaleFactor
);

FRDGTextureRef AddTyTDownSamplePass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InTexture,
    const FIntRect& Viewport
);

#define MAX_TYT_BLUR_RADIUS 5
#define MAX_TYT_SAMPLE_COUNT (MAX_TYT_BLUR_RADIUS * 2 + 1)

FRDGTextureRef AddTyTGaussianBlurXPass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InputTexture,
    const FIntRect& InputTextureViewport,
    int BlurRadius,
    TArrayView<const FLinearColor> Weights
);

FRDGTextureRef AddTyTGaussianBlurYPass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InputTexture,
    const FIntRect InputTextureViewport,
    FRDGTextureRef AdditiveTexture,
    int BlurRadius,
    TArrayView<const FLinearColor> Weights
);

FRDGTextureRef AddTyTGaussianBlurPass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InputTexture,
    const FIntRect InputTextureViewport,
    FRDGTextureRef AdditiveTexture,
    int BlurRadius,
    const FLinearColor& Tint
);

int CalcBlurRadius(const float& Width, const float& KernalSizePercent);
TArray<float> CalcGaussianWeights(const int& Radius);

FRDGTextureRef AddTyTDualKawaseBlurPass(
    FRDGBuilder& GraphBuilder,
    const FViewInfo& ViewInfo,
    const FString& PassName,
    FRDGTextureRef InputTexture,
    const FIntRect& InputViewport,
    const FLinearColor& Tint,
    int SampleLevel,
    FRDGTextureRef AdditiveTexture = nullptr
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
    TStaticArray<FIntRect, StageCount> Viewports;

public:
    FORCEINLINE FRDGTextureRef GetTexture(uint32 i) const
    {
        return Textures[i];
    }
    FORCEINLINE FRDGTextureRef GetLastTexture() const
    {
        return Textures[StageCount - 1];
    }
    FORCEINLINE FIntRect GetViewport(uint32 i)	const
    {
        return Viewports[i];
    }
    FORCEINLINE FIntRect GetLastViewport() const
    {
        return Viewports[StageCount - 1];
    }
};