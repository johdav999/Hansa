#include "Generation/HansaStaticProp.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "StaticMeshCompiler.h"
#include "AssetCompilingManager.h"
#include "PreviewScene.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/LineBatchComponent.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "RenderingThread.h"

namespace Hansa::Editor::Generation
{
namespace
{
bool Fail(FString& Error, const TCHAR* Message) { Error = Message; return false; }
FString Text(const TSharedPtr<FJsonObject>& P, const TCHAR* Key) { FString S; if (P) P->TryGetStringField(Key, S); return S; }
double Number(const TSharedPtr<FJsonObject>& P, const TCHAR* Key) { double N = 0; if (P) P->TryGetNumberField(Key, N); return N; }
}
bool FHansaStaticProp::ValidateProfile(const TSharedPtr<FJsonObject>& P, FString& Error)
{
    if (!P || P->Values.Num() != 11 || Number(P, TEXT("version")) != 1 || Text(P, TEXT("role")) != TEXT("HarborProp") ||
        !Text(P, TEXT("stableId")).StartsWith(TEXT("Prop.")) ||
        Text(P, TEXT("forwardAxis")) != TEXT("+X") || Text(P, TEXT("upAxis")) != TEXT("+Z") ||
        Text(P, TEXT("pivot")) != TEXT("bottom-center") || Text(P, TEXT("collision")) != TEXT("box"))
        return Fail(Error, TEXT("HarborProp v1 requires Prop.* identity, +X forward, +Z up, bottom-center pivot and box collision."));
    const FString StableId = Text(P, TEXT("stableId"));
    TArray<FString> IdParts;
    StableId.ParseIntoArray(IdParts, TEXT("."), false);
    if (StableId.Len() > 100 || IdParts.Num() < 2)
        return Fail(Error, TEXT("Static prop requires a bounded Prop.* stable ID."));
    for (const FString& Part : IdParts)
    {
        if (Part.IsEmpty()) return Fail(Error, TEXT("Static prop stable ID contains an empty segment."));
        for (TCHAR C : Part)
            if (!((C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '_'))
                return Fail(Error, TEXT("Static prop stable ID contains an unsupported character."));
    }
    for (const auto& Limit : TArray<TTuple<FString, double, double>>{
        {TEXT("heightCm"), 10, 1000}, {TEXT("maximumTriangles"), 1, 50000},
        {TEXT("maximumMaterials"), 1, 8}, {TEXT("maximumTextureSize"), 64, 4096}})
    {
        const double N = Number(P, *Limit.Get<0>());
        if (!FMath::IsFinite(N) || N < Limit.Get<1>() || N > Limit.Get<2>() || N != FMath::FloorToDouble(N))
            return Fail(Error, TEXT("Static-prop profile exceeds a canonical size or resource limit."));
    }
    return true;
}
bool FHansaStaticProp::Prepare(const TArray<UObject*>& Assets, const TSharedPtr<FJsonObject>& P, FString& Error, TSharedPtr<FJsonObject>* OutNormalization)
{
    if (!ValidateProfile(P, Error)) return false;
    FAssetCompilingManager::Get().FinishAllCompilation();
    int32 MeshCount = 0;
    for (UObject* Asset : Assets)
    {
        UStaticMesh* Mesh = Cast<UStaticMesh>(Asset);
        if (!Mesh) continue;
        if (++MeshCount > 1 || Mesh->GetNumLODs() != 1 || Mesh->GetNumTriangles(0) > Number(P, TEXT("maximumTriangles")))
            return Fail(Error, TEXT("HarborProp requires one static mesh LOD within its triangle limit."));
        FMeshDescription* Description = Mesh->GetMeshDescription(0);
        if (!Description) return Fail(Error, TEXT("Imported prop lacks editable mesh geometry."));
        FStaticMeshAttributes Attributes(*Description);
        auto Positions = Attributes.GetVertexPositions();
        FBox Bounds(ForceInit);
        for (FVertexID Vertex : Description->Vertices().GetElementIDs())
        {
            const FVector Position(Positions[Vertex]);
            if (Position.ContainsNaN() || Position.GetAbsMax() > 100000)
                return Fail(Error, TEXT("Imported prop contains invalid geometry coordinates."));
            Bounds += Position;
        }
        const FVector Size = Bounds.GetSize();
        if (!Bounds.IsValid || Size.GetMin() < 0.01)
            return Fail(Error, TEXT("Harbor props must have nonzero extent on every axis."));
        // Unreal's glTF importer converts meters/Y-up to centimeters/Z-up.
        // Uniformly fit the approved height and bake a bottom-center pivot.
        const double Scale = Number(P, TEXT("heightCm")) / Size.Z;
        const FVector Origin(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z);
        if (OutNormalization)
        {
            *OutNormalization = MakeShared<FJsonObject>();
            (*OutNormalization)->SetNumberField(TEXT("version"), 1);
            (*OutNormalization)->SetStringField(TEXT("importedBoundsMinCm"), Bounds.Min.ToString());
            (*OutNormalization)->SetStringField(TEXT("importedBoundsMaxCm"), Bounds.Max.ToString());
            (*OutNormalization)->SetStringField(TEXT("subtractedOriginCm"), Origin.ToString());
            (*OutNormalization)->SetNumberField(TEXT("uniformScale"), Scale);
            (*OutNormalization)->SetStringField(TEXT("axisConversion"), TEXT("glTF(X,Y,Z) meters -> Unreal(X,Z,Y) centimeters; +X forward preserved"));
        }
        for (FVertexID Vertex : Description->Vertices().GetElementIDs())
            Positions[Vertex] = FVector3f((FVector(Positions[Vertex]) - Origin) * Scale);
        Mesh->CommitMeshDescription(0);
        Mesh->Build(false);
        Mesh->PostEditChange();
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
        Mesh->CreateBodySetup();
        UBodySetup* Body = Mesh->GetBodySetup();
        Body->RemoveSimpleCollision();
        const FBox Final = Mesh->GetBoundingBox();
        FKBoxElem Box;
        Box.Center = Final.GetCenter();
        Box.X = Final.GetSize().X; Box.Y = Final.GetSize().Y; Box.Z = Final.GetSize().Z;
        Body->AggGeom.BoxElems.Add(Box);
        Body->CollisionTraceFlag = CTF_UseSimpleAndComplex;
        Body->InvalidatePhysicsData();
        Body->CreatePhysicsMeshes();
        Mesh->MarkPackageDirty();
    }
    return Validate(Assets, P, Error);
}
bool FHansaStaticProp::Validate(const TArray<UObject*>& Assets, const TSharedPtr<FJsonObject>& P, FString& Error)
{
    if (!ValidateProfile(P, Error)) return false;
    int32 MeshCount = 0, MaterialCount = 0, TextureCount = 0;
    for (UObject* Asset : Assets)
    {
        if (const UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
        {
            ++MeshCount;
            const FBox B = Mesh->GetBoundingBox();
            const FVector S = B.GetSize(), C = B.GetCenter();
            if (Mesh->GetNumLODs() != 1 || Mesh->GetNumTriangles(0) < 1 ||
                Mesh->GetNumTriangles(0) > Number(P, TEXT("maximumTriangles")) ||
                Mesh->GetStaticMaterials().Num() > Number(P, TEXT("maximumMaterials")) ||
                !B.IsValid || S.ContainsNaN() || S.GetMin() <= 0 || S.GetMax() > 10000 ||
                FMath::Abs(S.Z - Number(P, TEXT("heightCm"))) > 0.1 ||
                FMath::Abs(C.X) > 0.1 || FMath::Abs(C.Y) > 0.1 || FMath::Abs(B.Min.Z) > 0.1)
                return Fail(Error, TEXT("Static prop failed centimeter dimensions, bottom-center pivot or geometry/material limits."));
            const UBodySetup* Body = Mesh->GetBodySetup();
            if (!Body || Body->AggGeom.GetElementCount() != 1 || Body->AggGeom.BoxElems.Num() != 1 ||
                Body->CollisionTraceFlag != CTF_UseSimpleAndComplex)
                return Fail(Error, TEXT("Static prop requires one simple box collision shape."));
            const FKBoxElem& Box = Body->AggGeom.BoxElems[0];
            if (!Box.Center.Equals(C, 0.1) || !FVector(Box.X, Box.Y, Box.Z).Equals(S, 0.1) || !Box.Rotation.IsNearlyZero())
                return Fail(Error, TEXT("Simple collision no longer matches the prop bounds."));
        }
        else if (Asset->IsA<UMaterialInterface>()) ++MaterialCount;
        else if (const UTexture2D* Texture = Cast<UTexture2D>(Asset))
        {
            ++TextureCount;
            if (!Texture->Source.IsValid() || Texture->Source.GetSizeX() < 1 || Texture->Source.GetSizeY() < 1 ||
                Texture->Source.GetSizeX() > Number(P, TEXT("maximumTextureSize")) ||
                Texture->Source.GetSizeY() > Number(P, TEXT("maximumTextureSize")))
                return Fail(Error, TEXT("Static prop texture failed native decode or dimensions budget."));
        }
        else return Fail(Error, TEXT("HarborProp bundle contains an unsupported asset class."));
    }
    if (MeshCount != 1 || MaterialCount > Number(P, TEXT("maximumMaterials")) ||
        TextureCount > Number(P, TEXT("maximumMaterials")) * 4)
        return Fail(Error, TEXT("HarborProp bundle exceeds mesh/material/texture count limits."));
    return true;
}
bool FHansaStaticProp::RenderPreview(UStaticMesh* Mesh, const FString& Filename, UTexture2D*& Texture, FString& Error)
{
    Texture = nullptr;
    if (!Mesh || !FApp::CanEverRender()) return Fail(Error, TEXT("A rendering-enabled Editor is required for the prop review scene."));
    FAssetCompilingManager::Get().FinishAllCompilation();
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetLightRotation(FRotator(-40, 120, 0))
        .SetLightBrightness(4).SetSkyBrightness(1).SetCreatePhysicsScene(false).SetTransactional(false));
    UDirectionalLightComponent* Fill = NewObject<UDirectionalLightComponent>();
    Fill->SetIntensity(1.5f);
    Fill->SetCastShadows(false);
    Scene.AddComponent(Fill, FTransform(FRotator(-25, -30, 0)));
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>();
    Component->SetStaticMesh(Mesh);
    Scene.AddComponent(Component, FTransform::Identity);
    UStaticMeshComponent* Floor = NewObject<UStaticMeshComponent>();
    Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    const double Radius = Mesh->GetBounds().SphereRadius;
    Scene.AddComponent(Floor, FTransform(FRotator::ZeroRotator, FVector(0, 0, -2), FVector(Radius / 20, Radius / 20, 0.04)));
    // Axis markers keep the geometric convention visible; semantic facing is a human review.
    Scene.GetLineBatcher()->DrawLine(FVector::ZeroVector, FVector(Radius,0,0), FLinearColor::Red, 0, 3, 0);
    Scene.GetLineBatcher()->DrawLine(FVector::ZeroVector, FVector(0,Radius,0), FLinearColor::Green, 0, 3, 0);
    Scene.GetLineBatcher()->DrawLine(FVector::ZeroVector, FVector(0,0,Radius), FLinearColor::Blue, 0, 3, 0);
    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
    Target->ClearColor = FLinearColor(FColor(0x15, 0x2a, 0x35));
    Target->InitCustomFormat(1280, 720, PF_B8G8R8A8, false);
    Target->UpdateResourceImmediate(true);
    USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>();
    Capture->TextureTarget = Target;
    Capture->CaptureSource = SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->FOVAngle = 45;
    Capture->ShowFlags.SetMotionBlur(false);
    Capture->ShowFlags.SetTemporalAA(false);
    Capture->ShowFlags.SetLumenGlobalIllumination(false);
    Capture->ShowFlags.SetLumenReflections(false);
    Capture->ShowFlags.SetScreenSpaceReflections(false);
    Capture->ShowFlags.SetAmbientOcclusion(false);
    Capture->PostProcessSettings.bOverride_FilmGrainIntensity = true;
    Capture->PostProcessSettings.FilmGrainIntensity = 0;
    Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    const FVector Center = Mesh->GetBounds().Origin;
    const FVector Camera = Center + FVector(1.6, -2.4, 1.5).GetSafeNormal() * Radius * 6;
    Scene.AddComponent(Capture, FTransform((Center - Camera).Rotation(), Camera));
    Scene.GetWorld()->SendAllEndOfFrameUpdates();
    Scene.UpdateCaptureContents();
    Capture->CaptureScene();
    FlushRenderingCommands();
    TArray<FColor> Pixels;
    if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels) || Pixels.Num() != 1280 * 720)
        return Fail(Error, TEXT("Could not read deterministic prop preview pixels."));
    for (FColor& Pixel : Pixels) Pixel.A = 255;
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(1280, 720, Pixels, Png);
    if (!FFileHelper::SaveArrayToFile(Png, *Filename))
        return Fail(Error, TEXT("Could not retain the native 1280x720 review capture."));
    FCreateTexture2DParameters Parameters;
    Parameters.bDeferCompression = true;
    Texture = FImageUtils::CreateTexture2D(1280, 720, Pixels, GetTransientPackage(),
        TEXT("HansaHarborPropReview_") + FGuid::NewGuid().ToString(EGuidFormats::Digits), RF_Transient, Parameters);
    return Texture != nullptr;
}
}
