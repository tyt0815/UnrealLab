# TyTPostProcess
플러그인 기반 커스텀 포스트 프로세스 효과
블룸과 렌즈 플레어 효과가 구현되어 있습니다.

`FSceneViewExtensionBase` 클래스를 상속하여 렌더 패스를 추가하였으며 렌더링 코드는 [Renderer/TyTSceneViewExtension.cpp](/Source/TyTPostProcess/Private/Renderer/TyTSceneViewExtension.cpp)의 `FTyTSceneViewExtension::PrePostProcessPass_RenderThread` 함수를 확인해 주세요.

`TyTEngineSubSystem`클래스에서는 `FTyTSceneViewExtension` 인스턴스를 생성하고 포스트 프로세스 효과의 Detail을 설정할 수 있는 DataAsset를 Load합니다.

DA_PostProcessSettings asset은 플러그인의 Content폴더에 위치해 있으며 해당 에셋의 경로를 변경할 경우 `TyTEngineSubSystem`이 에셋을 로드 할 수 없으니 주의하세요. 해당 에셋은 [TyTPostProcessSettingsAsset](Source/TyTPostProcess/Public/DataAssets/TyTPostProcessSettingsAsset.h)클래스로 정의되고 있습니다.

[데모 영상](https://youtu.be/G2PwW-XIONs)