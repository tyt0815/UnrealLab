// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TyTPostProcessSettingsAsset.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct FTyTBloomSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General)
	float Intensity = 0.675f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General)
	float Threshold = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	float KernalSizeScale = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	float KernalSize1 = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	float KernalSize2 = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	float KernalSize3 = 2.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	float KernalSize4 = 10.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	float KernalSize5 = 30.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	float KernalSize6 = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	FLinearColor Tint1 = FLinearColor(0.3265, 0.3265, 0.3265);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	FLinearColor Tint2 = FLinearColor(0.138, 0.138, 0.138);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	FLinearColor Tint3 = FLinearColor(0.1176, 0.1176, 0.1176);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	FLinearColor Tint4 = FLinearColor(0.066, 0.066, 0.066);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	FLinearColor Tint5 = FLinearColor(0.066, 0.066, 0.066);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Advanced)
	FLinearColor Tint6 = FLinearColor(0.061, 0.061, 0.061);
};

USTRUCT(BlueprintType)
struct FTyTLensFlareGhostSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General)
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General)
	float Scale = 1.0f;
};

USTRUCT(BlueprintType)
struct FTyTLensFlareSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General)
	bool bEnableLensFlare = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General, meta = (EditCondition = "bEnableLensFlare"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General, meta = (EditCondition = "bEnableLensFlare"))
	FLinearColor Tint = FLinearColor(1.0f, 0.85f, 0.7f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General, meta = (EditCondition = "bEnableLensFlare"))
	UTexture2D* Gradient = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General, meta = (EditCondition = "bEnableLensFlare"))
	float ThresholdLevel = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General, meta = (EditCondition = "bEnableLensFlare"))
	float ThresholdRange = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Threshold", meta = (UIMin = "0.0", UIMax = "10.0", EditCondition = "bEnableLensFlare"))
	float GhostIntensity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Threshold", meta = (UIMin = "0.0", UIMax = "10.0", EditCondition = "bEnableLensFlare"))
	float GhostChromaShift = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghosts, meta = (EditCondition = "bEnableLensFlare"))
	FTyTLensFlareGhostSettings Ghost1 = { FLinearColor(1.0f, 0.8f, 0.4f, 1.0f), -1.5 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghosts, meta = (EditCondition = "bEnableLensFlare"))
	FTyTLensFlareGhostSettings Ghost2 = { FLinearColor(1.0f, 1.0f, 0.6f, 1.0f),  2.5 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghosts, meta = (EditCondition = "bEnableLensFlare"))
	FTyTLensFlareGhostSettings Ghost3 = { FLinearColor(0.8f, 0.8f, 1.0f, 1.0f), -5.0 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghosts, meta = (EditCondition = "bEnableLensFlare"))
	FTyTLensFlareGhostSettings Ghost4 = { FLinearColor(0.5f, 1.0f, 0.4f, 1.0f), 10.0 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghosts, meta = (EditCondition = "bEnableLensFlare"))
	FTyTLensFlareGhostSettings Ghost5 = { FLinearColor(0.5f, 0.8f, 1.0f, 1.0f),  0.7 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghosts, meta = (EditCondition = "bEnableLensFlare"))
	FTyTLensFlareGhostSettings Ghost6 = { FLinearColor(0.9f, 1.0f, 0.8f, 1.0f), -0.4 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghosts, meta = (EditCondition = "bEnableLensFlare"))
	FTyTLensFlareGhostSettings Ghost7 = { FLinearColor(1.0f, 0.8f, 0.4f, 1.0f), -0.2 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghosts, meta = (EditCondition = "bEnableLensFlare"))
	FTyTLensFlareGhostSettings Ghost8 = { FLinearColor(0.9f, 0.7f, 0.7f, 1.0f), -0.1 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Halo, meta = (UIMin = "0.0", UIMax = "1.0", EditCondition = "bEnableLensFlare"))
	float HaloIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Halo, meta = (UIMin = "0.0", UIMax = "1.0", EditCondition = "bEnableLensFlare"))
	float HaloWidth = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Halo, meta = (UIMin = "0.0", UIMax = "1.0", EditCondition = "bEnableLensFlare"))
	float HaloMask = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Halo, meta = (UIMin = "0.0", UIMax = "1.0", EditCondition = "bEnableLensFlare"))
	float HaloCompression = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Halo, meta = (UIMin = "0.0", UIMax = "1.0", EditCondition = "bEnableLensFlare"))
	float HaloChromaShift = 0.015f;

	UPROPERTY(EditAnywhere, Category = "Glare", meta = (UIMin = "0", UIMax = "10", EditCondition = "bEnableLensFlare"))
	float GlareIntensity = 0.02f;

	UPROPERTY(EditAnywhere, Category = "Glare", meta = (UIMin = "0.01", UIMax = "200", EditCondition = "bEnableLensFlare"))
	float GlareDivider = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Glare", meta = (UIMin = "0.0", UIMax = "10.0", EditCondition = "bEnableLensFlare"))
	FVector GlareScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Glare", meta = (EditCondition = "bEnableLensFlare"))
	FLinearColor GlareTint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Glare", meta = (EditCondition = "bEnableLensFlare"))
	UTexture2D* GlareLineMask = nullptr;
};

UCLASS()
class TYTPOSTPROCESS_API UTyTPostProcessSettingsAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
	static UTyTPostProcessSettingsAsset* Get();
	static FTyTBloomSettings* GetBloomSettings();
	static FTyTLensFlareSettings* GetLensFlareSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General)
	FTyTBloomSettings BloomSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General)
	FTyTLensFlareSettings LensFlareSettings;
};
