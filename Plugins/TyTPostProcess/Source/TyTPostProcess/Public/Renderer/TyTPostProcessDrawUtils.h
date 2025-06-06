// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ScreenPass.h"
#include "Runtime/Renderer/Private/PostProcess/SceneFilterRendering.h"

class FTyTScreenPassVS : public FGlobalShader
{
public:
    DECLARE_EXPORTED_GLOBAL_SHADER(FTyTScreenPassVS, TYTPOSTPROCESS_API);
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