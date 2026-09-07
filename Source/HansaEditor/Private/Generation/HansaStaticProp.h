#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
class UStaticMesh;
class UTexture2D;
namespace Hansa::Editor::Generation
{
/** Provider-neutral HarborProp v1 normalization, QA and deterministic review scene. Editor only. */
class FHansaStaticProp final
{
public:
    static bool ValidateProfile(const TSharedPtr<FJsonObject>& Profile, FString& Error);
    static bool Prepare(const TArray<UObject*>& Assets, const TSharedPtr<FJsonObject>& Profile, FString& Error, TSharedPtr<FJsonObject>* OutNormalization = nullptr);
    static bool Validate(const TArray<UObject*>& Assets, const TSharedPtr<FJsonObject>& Profile, FString& Error);
    static bool RenderPreview(UStaticMesh* Mesh, const FString& Filename, UTexture2D*& Texture, FString& Error);
};
}
