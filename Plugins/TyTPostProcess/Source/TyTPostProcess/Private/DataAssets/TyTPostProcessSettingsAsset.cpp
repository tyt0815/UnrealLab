// Fill out your copyright notice in the Description page of Project Settings.


#include "DataAssets/TyTPostProcessSettingsAsset.h"
#include "SubSystems/TyTPostProcessSubSystem.h"

UTyTPostProcessSettingsAsset* UTyTPostProcessSettingsAsset::Get()
{
    return UTyTPostProcessSubSystem::Get()->GetPostProcessSettings();
}

FTyTBloomSettings* UTyTPostProcessSettingsAsset::GetBloomSettings()
{
    static UTyTPostProcessSettingsAsset* PostProcessSettings = Get();
    static FTyTBloomSettings* BloomSettings = nullptr;
    if (!BloomSettings && PostProcessSettings)
    {
        BloomSettings = &PostProcessSettings->BloomSettings;
    }
    return BloomSettings;
}

FTyTLensFlareSettings* UTyTPostProcessSettingsAsset::GetLensFlareSettings()
{
    static UTyTPostProcessSettingsAsset* PostProcessSettings = Get();
    static FTyTLensFlareSettings* LensFlareSettings = nullptr;
    if (!LensFlareSettings && PostProcessSettings)
    {
        LensFlareSettings = &PostProcessSettings->LensFlareSettings;
    }
    return LensFlareSettings;
}
