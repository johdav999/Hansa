#include "Generation/HansaStagedMedia.h"
#include "Generation/HansaStaticProp.h"
#include "Generation/HansaAudioTake.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Sound/SoundWave.h"
#include "UObject/MetaData.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/SavePackage.h"

using namespace Hansa::Editor::Generation;
namespace
{
bool SaveJson(const FString& File, const TSharedRef<FJsonObject>& Json)
{
    FString Text;
    FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text));
    return FFileHelper::SaveStringToFile(Text, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
struct FMediaFixture
{
    FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FString SourceDirectory = TEXT("SourceArt/Generated/Audio/Test_") + Unique;
    FString DescriptorPath = SourceDirectory / TEXT("source.json");
    FString Destination = TEXT("/Game/Hansa/Audio/Test_") + Unique;
    FString Receipt;
    FString StageDirectory;
    bool Create(const FString& Extension, bool bHarborProp = false, const FString& AudioKind = TEXT(""))
    {
        if (Extension == TEXT("glb")) Destination = TEXT("/Game/Hansa/Meshes/Test_") + Unique;
        const FString Absolute = FPaths::ProjectDir() / SourceDirectory;
        IFileManager::Get().MakeDirectory(*Absolute, true);
        const FString GoldenName = Extension == TEXT("wav") ? TEXT("tone.wav") : bHarborProp ? TEXT("harbor-prop.glb") : TEXT("triangle.glb");
        const FString SourceFile = Absolute / (TEXT("source.") + Extension);
        if (IFileManager::Get().Copy(*SourceFile, *(FPaths::ProjectDir() / TEXT("Tests/Golden/Media") / GoldenName)) != COPY_OK) return false;
        TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
        Manifest->SetStringField(TEXT("status"), TEXT("Review"));
        Manifest->SetStringField(TEXT("manifestHash"), FString::ChrN(64, 'a'));
        TSharedRef<FJsonObject> Provider = MakeShared<FJsonObject>();
        Provider->SetStringField(TEXT("providerId"), TEXT("mock"));
        Provider->SetStringField(TEXT("modelVersion"), TEXT("golden-v1"));
        Manifest->SetObjectField(TEXT("provider"), Provider);
        TSharedRef<FJsonObject> Rights = MakeShared<FJsonObject>();
        Rights->SetBoolField(TEXT("acknowledged"), true);
        Manifest->SetObjectField(TEXT("rights"), Rights);
        if (!AudioKind.IsEmpty())
        {
            const bool Speech = AudioKind == TEXT("Speech");
            TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
            P->SetNumberField(TEXT("version"), 1); P->SetStringField(TEXT("kind"), AudioKind);
            P->SetStringField(TEXT("stableId"), Speech ? TEXT("Dialogue.DockGreeting") : TEXT("SFX.HarborBell"));
            P->SetNumberField(TEXT("variants"), 2);
            P->SetNumberField(TEXT("minimumDurationMs"), 100); P->SetNumberField(TEXT("maximumDurationMs"), 2000);
            P->SetNumberField(TEXT("maximumLeadingSilenceMs"), 100); P->SetNumberField(TEXT("maximumTrailingSilenceMs"), 150);
            P->SetStringField(TEXT("subtitle"), Speech ? TEXT("Welcome to the harbor.") : TEXT(""));
            P->SetStringField(TEXT("speakerId"), Speech ? TEXT("Speaker.Dockworker") : TEXT(""));
            P->SetStringField(TEXT("language"), Speech ? TEXT("en") : TEXT("")); P->SetBoolField(TEXT("loop"), false);
            TSharedRef<FJsonObject> Parameters = MakeShared<FJsonObject>();
            Parameters->SetObjectField(TEXT("audioTake"), P); Manifest->SetObjectField(TEXT("parameters"), Parameters);
            Manifest->SetStringField(TEXT("prompt"), Speech ? TEXT("Welcome to the harbor.") : TEXT("Harbor bell"));
            Rights->SetBoolField(TEXT("voiceAcknowledged"), true); Rights->SetBoolField(TEXT("englishTextAcknowledged"), true);
        }
        if (bHarborProp)
        {
            TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
            P->SetNumberField(TEXT("version"), 1);
            P->SetStringField(TEXT("role"), TEXT("HarborProp"));
            P->SetStringField(TEXT("stableId"), TEXT("Prop.HarborCrate"));
            P->SetNumberField(TEXT("heightCm"), 150);
            P->SetNumberField(TEXT("maximumTriangles"), 1000);
            P->SetNumberField(TEXT("maximumMaterials"), 2);
            P->SetNumberField(TEXT("maximumTextureSize"), 1024);
            P->SetStringField(TEXT("forwardAxis"), TEXT("+X"));
            P->SetStringField(TEXT("upAxis"), TEXT("+Z"));
            P->SetStringField(TEXT("pivot"), TEXT("bottom-center"));
            P->SetStringField(TEXT("collision"), TEXT("box"));
            TSharedRef<FJsonObject> Parameters = MakeShared<FJsonObject>();
            Parameters->SetObjectField(TEXT("staticProp"), P);
            Manifest->SetObjectField(TEXT("parameters"), Parameters);
        }
        if (!SaveJson(Absolute / TEXT("job-manifest.json"), Manifest)) return false;
        FString SourceHash, ManifestHash;
        if (!FHansaStagedMedia::HashFile(SourceFile, SourceHash) ||
            !FHansaStagedMedia::HashFile(Absolute / TEXT("job-manifest.json"), ManifestHash)) return false;
        TSharedRef<FJsonObject> Descriptor = MakeShared<FJsonObject>();
        Descriptor->SetNumberField(TEXT("schemaVersion"), 1);
        Descriptor->SetNumberField(TEXT("outputIndex"), 0);
        Descriptor->SetStringField(TEXT("jobId"), FGuid::NewGuid().ToString());
        Descriptor->SetStringField(TEXT("sha256"), SourceHash);
        Descriptor->SetStringField(TEXT("manifestHash"), FString::ChrN(64, 'a'));
        Descriptor->SetStringField(TEXT("manifestFileSha256"), ManifestHash);
        Descriptor->SetStringField(TEXT("sourcePath"), SourceDirectory / (TEXT("source.") + Extension));
        Descriptor->SetStringField(TEXT("manifestPath"), SourceDirectory / TEXT("job-manifest.json"));
        Descriptor->SetStringField(TEXT("mediaType"), Extension == TEXT("wav") ? TEXT("audio/wav") : TEXT("model/gltf-binary"));
        return SaveJson(FPaths::ProjectDir() / DescriptorPath, Descriptor);
    }
    void RememberStage()
    {
        FString Text;
        TSharedPtr<FJsonObject> Json;
        if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / Receipt)) &&
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json))
            StageDirectory = FPackageName::LongPackageNameToFilename(Json->GetStringField(TEXT("stagePath")));
    }
    bool RecordMockPreview(FString& Hash)
    {
        // An explicit test-only stand-in for opening the native asset editor.
        // No production API bypasses the preview receipt.
        if (!FHansaStagedMedia::HashFile(FPaths::ProjectDir() / Receipt, Hash)) return false;
        TSharedRef<FJsonObject> Preview = MakeShared<FJsonObject>();
        Preview->SetStringField(TEXT("reviewHash"), Hash);
        return SaveJson(FPaths::ProjectDir() / (Receipt + TEXT(".preview.json")), Preview);
    }
    ~FMediaFixture()
    {
        FHansaStagedMedia::bFailBeforeCommitForTests = false;
        // All paths derive from this test's fresh GUID under the project.
        IFileManager::Get().DeleteDirectory(*(FPaths::ProjectDir() / SourceDirectory), false, true);
        IFileManager::Get().DeleteDirectory(*FPackageName::LongPackageNameToFilename(Destination), false, true);
        if (!StageDirectory.IsEmpty()) IFileManager::Get().DeleteDirectory(*StageDirectory, false, true);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMediaPaths, "Hansa.Architecture.StagedMedia.Paths",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaMediaPaths::RunTest(const FString&)
{
    TestTrue(TEXT("Explicit audio destination"), FHansaStagedMedia::IsProductionDestination(TEXT("/Game/Hansa/Audio/HarborBellV1")));
    TestTrue(TEXT("Explicit mesh destination"), FHansaStagedMedia::IsProductionDestination(TEXT("/Game/Hansa/Meshes/CrateV1")));
    for (const FString& Path : {TEXT("/Game/Hansa/Generated/Staging/Job"), TEXT("/Game/Hansa/Developer/Test"),
        TEXT("/Game/Hansa/Audio/../Core"), TEXT("/Game/Hansa/Audio/"), TEXT("https://provider.invalid/task")})
        TestFalse(TEXT("Reject unsafe destination"), FHansaStagedMedia::IsProductionDestination(Path));
    TestTrue(TEXT("Staging root forbidden"), FHansaStagedMedia::IsForbiddenPackage(TEXT("/game/hansa/generated/staging")));
    TestTrue(TEXT("Developer forbidden"), FHansaStagedMedia::IsForbiddenPackage(TEXT("/Game/Hansa/Developer/Test")));
    for (const TCHAR* Path : {TEXT("/Game/__ExternalActors__/Hansa/Generated/Staging/Map/A/Actor"),
        TEXT("/game/__externalobjects__/hansa/generated/staging/Map/Object"),
        TEXT("/Game/__ExternalActors__/Developers/User/Map/Actor"),
        TEXT("/Game/__ExternalObjects__/Hansa/Developer/Map/Object")})
        TestTrue(TEXT("Staging and developer external packages forbidden"), FHansaStagedMedia::IsForbiddenPackage(Path));
    for (const TCHAR* Path : {TEXT("/Game/__ExternalActors__/Hansa/Maps/Lubeck/A/Actor"),
        TEXT("/Game/__ExternalObjects__/Hansa/Maps/Lubeck/Object"),
        TEXT("/Game/__ExternalActors__/Hansa/Generated/StagingEvil/Actor"),
        TEXT("/Game/Hansa/Generated/StagingEvil/Asset")})
        TestFalse(TEXT("Production and prefix near-misses remain audited sources"), FHansaStagedMedia::IsForbiddenPackage(Path));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMediaAudioPromotion, "Hansa.Architecture.StagedMedia.AudioPromotionRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaMediaAudioPromotion::RunTest(const FString&)
{
    FMediaFixture Fixture;
    if (!TestTrue(TEXT("Create original PCM fixture source"), Fixture.Create(TEXT("wav")))) return false;
    FString Error, Hash;
    if (!TestTrue(TEXT("Native isolated WAV import: ") + Error, FHansaStagedMedia::Stage(Fixture.DescriptorPath, Fixture.Receipt, Error))) { AddError(Error); return false; }
    Fixture.RememberStage();
    FHansaStagedMedia::HashFile(FPaths::ProjectDir() / Fixture.Receipt, Hash);
    auto Promote = [&](bool Approve, const FString& Reviewer, const FString& ReviewHash)
    { return FHansaStagedMedia::Promote(Fixture.Receipt, Fixture.Destination, TEXT("SFX.GoldenTone"), Reviewer, TEXT("Original mathematical fixture; automated review only."), ReviewHash, Approve, Error); };
    TestFalse(TEXT("Spend does not authorize promotion"), Promote(false, TEXT("Automation"), Hash));
    TestFalse(TEXT("Preview required"), Promote(true, TEXT("Automation"), Hash));
    Fixture.RecordMockPreview(Hash);
    TestFalse(TEXT("Reviewer required"), Promote(true, TEXT(" "), Hash));
    TestFalse(TEXT("Exact preview hash required"), Promote(true, TEXT("Automation"), TEXT("stale")));
    FHansaStagedMedia::bFailBeforeCommitForTests = true;
    TestFalse(TEXT("Injected failure"), Promote(true, TEXT("Automation"), Hash));
    TestFalse(TEXT("No partial final bundle"), IFileManager::Get().DirectoryExists(*FPackageName::LongPackageNameToFilename(Fixture.Destination)));
    TestFalse(TEXT("Uncommitted decision rolled back"), IFileManager::Get().FileExists(*(FPaths::ProjectDir() / (Fixture.Receipt + TEXT(".promotion.json")))));
    FHansaStagedMedia::bFailBeforeCommitForTests = false;
    if (!TestTrue(TEXT("Retry commits approved bundle"), Promote(true, TEXT("Automation"), Hash))) { AddError(Error); return false; }
    const FString ObjectPath = Fixture.Destination / TEXT("Asset_001.Asset_001");
    USoundWave* Sound = LoadObject<USoundWave>(nullptr, *ObjectPath);
    if (!TestNotNull(TEXT("Promoted native sound exists"), Sound)) return false;
    TestEqual(TEXT("Stable Hansa metadata"), Sound->GetPackage()->GetMetaData().GetValue(Sound, TEXT("Hansa.StableMediaId")), FString(TEXT("SFX.GoldenTone")));
    TestTrue(TEXT("Durable decision independent of Saved"), IFileManager::Get().FileExists(*(FPaths::ProjectDir() / (Fixture.Receipt + TEXT(".promotion.json")))));
    TestTrue(TEXT("Same approved operation reconciles idempotently"), Promote(true, TEXT("Automation"), Hash));
    IFileManager::Get().Delete(*(FPaths::ProjectDir() / (Fixture.Receipt + TEXT(".committed.json"))));
    TestTrue(TEXT("Final package hashes prove commit"), FHansaStagedMedia::VerifyPromotion(Fixture.Receipt, Error));
    TestTrue(TEXT("Missing post-commit marker recovered"), IFileManager::Get().FileExists(*(FPaths::ProjectDir() / (Fixture.Receipt + TEXT(".committed.json")))));
    const FString FinalFile = FPackageName::LongPackageNameToFilename(Fixture.Destination / TEXT("Asset_001"), TEXT(".uasset"));
    FFileHelper::SaveStringToFile(TEXT("tampered"), *FinalFile);
    TestFalse(TEXT("Changed production bytes are not silently replaced"), Promote(true, TEXT("Automation"), Hash));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMediaTamper, "Hansa.Architecture.StagedMedia.SourceTamper",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaMediaTamper::RunTest(const FString&)
{
    FMediaFixture Fixture;
    if (!Fixture.Create(TEXT("wav"))) return false;
    FFileHelper::SaveStringToFile(TEXT("tampered"), *(FPaths::ProjectDir() / Fixture.SourceDirectory / TEXT("source.wav")));
    FString Error;
    TestFalse(TEXT("Hash mismatch stops import"), FHansaStagedMedia::Stage(Fixture.DescriptorPath, Fixture.Receipt, Error));
    TestTrue(TEXT("Actionable hash error"), Error.Contains(TEXT("hash")));
    TestFalse(TEXT("Path traversal stops import"), FHansaStagedMedia::Stage(TEXT("SourceArt/Generated/../../Config/DefaultGame.ini"), Fixture.Receipt, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMediaMeshImport, "Hansa.Architecture.StagedMedia.GoldenMeshImport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaMediaMeshImport::RunTest(const FString&)
{
    FMediaFixture Fixture;
    if (!Fixture.Create(TEXT("glb"))) return false;
    FString Error;
    const bool Imported = FHansaStagedMedia::Stage(Fixture.DescriptorPath, Fixture.Receipt, Error);
    if (Imported) Fixture.RememberStage();
    TestTrue(TEXT("Golden GLB imports and validates natively"), Imported);
    if (!Imported) { AddError(Error); return false; }
    FString Hash;
    Fixture.RecordMockPreview(Hash);
    TestFalse(TEXT("Mesh cannot receive audio gameplay identity"), FHansaStagedMedia::Promote(Fixture.Receipt, Fixture.Destination,
        TEXT("SFX.WrongDomain"), TEXT("Automation"), TEXT("Original fixture"), Hash, true, Error));
    const bool Promoted = FHansaStagedMedia::Promote(Fixture.Receipt, Fixture.Destination,
        TEXT("Prop.GoldenTriangle"), TEXT("Automation"), TEXT("Original fixture"), Hash, true, Error);
    TestTrue(TEXT("Golden mesh bundle promotes with valid references"), Promoted);
    if (!Promoted) AddError(Error);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMediaReferenceGuard, "Hansa.Architecture.StagedMedia.ReferenceGuard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaMediaReferenceGuard::RunTest(const FString&)
{
    FMediaFixture Fixture;
    if (!Fixture.Create(TEXT("wav"))) return false;
    FString Error;
    if (!FHansaStagedMedia::Stage(Fixture.DescriptorPath, Fixture.Receipt, Error)) { AddError(Error); return false; }
    Fixture.RememberStage();
    FString ReceiptText;
    TSharedPtr<FJsonObject> ReceiptJson;
    FFileHelper::LoadFileToString(ReceiptText, *(FPaths::ProjectDir() / Fixture.Receipt));
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ReceiptText), ReceiptJson);
    UObject* Staged = LoadObject<UObject>(nullptr, *ReceiptJson->GetArrayField(TEXT("assets"))[0]->AsObject()->GetStringField(TEXT("objectPath")));
    const FString PackageName = Fixture.Destination / TEXT("BadReference");
    UPackage* Package = CreatePackage(*PackageName);
    UObjectRedirector* Bad = NewObject<UObjectRedirector>(Package, TEXT("BadReference"), RF_Public | RF_Standalone);
    Bad->DestinationObject = Staged;
    FAssetRegistryModule::AssetCreated(Bad);
    const FString File = FPackageName::LongPackageNameToFilename(PackageName, TEXT(".uasset"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    TestTrue(TEXT("Save test referencer"), UPackage::SavePackage(Package, Bad, *File, Args));
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.ScanFilesSynchronous({File}, true);
    TArray<FString> Errors;
    TestFalse(TEXT("Production to staging reference fails audit"), FHansaStagedMedia::AuditReferences(Errors));
    TestTrue(TEXT("Audit names the actual production referencer"), Errors.ContainsByPredicate([&](const FString& Value) { return Value.Contains(PackageName); }));
    FAssetRegistryModule::AssetDeleted(Bad);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaHarborPropQA, "Hansa.Architecture.StagedMedia.HarborPropQA",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaHarborPropQA::RunTest(const FString&)
{
    FMediaFixture Fixture;
    if (!Fixture.Create(TEXT("glb"), true)) return false;
    FString Error;
    if (!TestTrue(TEXT("Harbor prop imports with native normalization"), FHansaStagedMedia::Stage(Fixture.DescriptorPath, Fixture.Receipt, Error)))
    { AddError(Error); return false; }
    Fixture.RememberStage();
    FString Text;
    TSharedPtr<FJsonObject> Receipt;
    FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / Fixture.Receipt));
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Receipt);
    TArray<UObject*> Assets;
    UStaticMesh* Mesh = nullptr;
    for (const auto& Record : Receipt->GetArrayField(TEXT("assets")))
    {
        UObject* Asset = LoadObject<UObject>(nullptr, *Record->AsObject()->GetStringField(TEXT("objectPath")));
        Assets.Add(Asset);
        if (UStaticMesh* Candidate = Cast<UStaticMesh>(Asset)) Mesh = Candidate;
    }
    if (!TestNotNull(TEXT("One static prop"), Mesh)) return false;
    const auto Profile = Receipt->GetObjectField(TEXT("staticProp"));
    TestTrue(TEXT("Native full contract passes"), FHansaStaticProp::Validate(Assets, Profile, Error));
    TestTrue(TEXT("Height baked in centimeters"), FMath::IsNearlyEqual(Mesh->GetBoundingBox().GetSize().Z, 150.0, 0.1));
    TestTrue(TEXT("Pivot rests on ground"), FMath::Abs(Mesh->GetBoundingBox().Min.Z) < 0.1);
    TestEqual(TEXT("Simple collision box created"), Mesh->GetBodySetup()->AggGeom.BoxElems.Num(), 1);
    Mesh->GetBodySetup()->AggGeom.BoxElems[0].X += 10;
    TestFalse(TEXT("Collision drift fails review"), FHansaStaticProp::Validate(Assets, Profile, Error));
    Mesh->GetBodySetup()->AggGeom.BoxElems[0].X -= 10;
    Profile->SetNumberField(TEXT("maximumTriangles"), 1);
    TestFalse(TEXT("Triangle budget fails review"), FHansaStaticProp::Validate(Assets, Profile, Error));
    Profile->SetNumberField(TEXT("maximumTriangles"), 1000);
    Profile->SetStringField(TEXT("upAxis"), TEXT("+Y"));
    TestFalse(TEXT("Axis contract fails review"), FHansaStaticProp::Validate(Assets, Profile, Error));
    Profile->SetStringField(TEXT("upAxis"), TEXT("+Z"));
    UTexture2D* InvalidTexture = NewObject<UTexture2D>();
    Assets.Add(InvalidTexture);
    TestFalse(TEXT("Undecodable texture fails review"), FHansaStaticProp::Validate(Assets, Profile, Error));
    InvalidTexture->Source.Init(2048, 1, 1, 1, TSF_BGRA8);
    TestFalse(TEXT("Oversized texture fails review"), FHansaStaticProp::Validate(Assets, Profile, Error));
    Assets.Pop();
    TestTrue(TEXT("Normalization is recorded"), Receipt->GetObjectField(TEXT("normalization"))->GetNumberField(TEXT("uniformScale")) > 0);
    Profile->SetStringField(TEXT("stableId"), TEXT("Prop..Broken"));
    TestFalse(TEXT("Malformed canonical identity fails profile"), FHansaStaticProp::ValidateProfile(Profile, Error));
    Profile->SetStringField(TEXT("stableId"), TEXT("Prop.HarborCrate"));
    Profile->SetBoolField(TEXT("rig"), true);
    TestFalse(TEXT("Unknown profile setting rejected"), FHansaStaticProp::ValidateProfile(Profile, Error));
    Profile->RemoveField(TEXT("rig"));
    FString Hash;
    Fixture.RecordMockPreview(Hash);
    TestFalse(TEXT("Asset editor marker alone cannot approve harbor prop"), FHansaStagedMedia::Promote(Fixture.Receipt,
        Fixture.Destination, TEXT("Prop.HarborCrate"), TEXT("Automation"), TEXT("Original fixture"), Hash, true, Error));
    TestTrue(TEXT("Missing deterministic capture remedy"), Error.Contains(TEXT("capture")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaHarborPropPreview, "Hansa.Architecture.StagedMedia.HarborPropPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaHarborPropPreview::RunTest(const FString&)
{
    if (!FApp::CanEverRender()) { AddInfo(TEXT("Render proof requires -WithRendering; covered by the dedicated rendering run.")); return true; }
    FMediaFixture Fixture;
    if (!Fixture.Create(TEXT("glb"), true)) return false;
    FString Error;
    if (!FHansaStagedMedia::Stage(Fixture.DescriptorPath, Fixture.Receipt, Error)) { AddError(Error); return false; }
    Fixture.RememberStage();
    FString Text;
    TSharedPtr<FJsonObject> Receipt;
    FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / Fixture.Receipt));
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Receipt);
    UStaticMesh* Mesh = nullptr;
    for (const auto& Record : Receipt->GetArrayField(TEXT("assets")))
        if (UStaticMesh* Candidate = LoadObject<UStaticMesh>(nullptr, *Record->AsObject()->GetStringField(TEXT("objectPath")))) Mesh = Candidate;
    const FString CaptureFile = FPaths::ProjectSavedDir() / TEXT("Automation/HarborPropPreview.png");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(CaptureFile), true);
    UTexture2D* Capture = nullptr;
    if (!TestTrue(TEXT("Deterministic scene renders"), FHansaStaticProp::RenderPreview(Mesh, CaptureFile, Capture, Error)))
    { AddError(Error); return false; }
    TestEqual(TEXT("Native preview width"), Capture->Source.GetSizeX(), int64(1280));
    TestEqual(TEXT("Native preview height"), Capture->Source.GetSizeY(), int64(720));
    FString Hash;
    Fixture.RecordMockPreview(Hash);
    FString CaptureHash;
    FHansaStagedMedia::HashFile(CaptureFile, CaptureHash);
    IFileManager::Get().Copy(*(FPaths::ProjectDir() / (Fixture.Receipt + TEXT(".preview.png"))), *CaptureFile);
    TSharedRef<FJsonObject> Preview = MakeShared<FJsonObject>();
    Preview->SetStringField(TEXT("reviewHash"), Hash);
    Preview->SetStringField(TEXT("captureSha256"), CaptureHash);
    Preview->SetStringField(TEXT("scene"), TEXT("HarborProp-v1-1280x720-automation"));
    SaveJson(FPaths::ProjectDir() / (Fixture.Receipt + TEXT(".preview.json")), Preview);
    TestFalse(TEXT("Stable identity must match"), FHansaStagedMedia::Promote(Fixture.Receipt, Fixture.Destination,
        TEXT("Prop.Other"), TEXT("Automation"), TEXT("Original fixture"), Hash, true, Error));
    const bool Promoted = FHansaStagedMedia::Promote(Fixture.Receipt, Fixture.Destination,
        TEXT("Prop.HarborCrate"), TEXT("Automation"), TEXT("Original mathematical fixture; test-only human approval surrogate"), Hash, true, Error);
    TestTrue(TEXT("Reviewed capture and explicit approval promote prop"), Promoted);
    if (!Promoted) AddError(Error);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaAudioTakePromotion, "Hansa.Architecture.StagedMedia.AudioTakePromotion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaAudioTakePromotion::RunTest(const FString&)
{
    for (const FString& Kind : {FString(TEXT("SFX")), FString(TEXT("Speech"))})
    {
        FMediaFixture Fixture;
        if (!Fixture.Create(TEXT("wav"), false, Kind)) return false;
        FString Error;
        if (!FHansaStagedMedia::Stage(Fixture.DescriptorPath, Fixture.Receipt, Error)) { AddError(Error); return false; }
        Fixture.RememberStage();
        FString Text; TSharedPtr<FJsonObject> Receipt;
        FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / Fixture.Receipt));
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Receipt);
        const auto P = Receipt->GetObjectField(TEXT("audioTake"));
        USoundWave* Sound = LoadObject<USoundWave>(nullptr, *Receipt->GetArrayField(TEXT("assets"))[0]->AsObject()->GetStringField(TEXT("objectPath")));
        if (!TestNotNull(TEXT("Native selected take"), Sound)) return false;
        TArray<UObject*> Assets{Sound};
        TestTrue(TEXT("Native audio contract passes"), FHansaAudioTake::Validate(Assets, P, Error));
        Sound->bLooping = true;
        TestFalse(TEXT("Looping drift rejected"), FHansaAudioTake::Validate(Assets, P, Error));
        Sound->bLooping = false;
        if (Kind == TEXT("Speech"))
        {
            const FText Subtitle = Sound->Subtitles[0].Text;
            Sound->Subtitles[0].Text = FText::FromString(TEXT("Different line"));
            TestFalse(TEXT("Subtitle drift rejected"), FHansaAudioTake::Validate(Assets, P, Error));
            Sound->Subtitles[0].Text = Subtitle;
            TestEqual(TEXT("Speech category"), Sound->SoundGroup.GetValue(), SOUNDGROUP_Voice);
        }
        TArray<uint8> PCM; uint32 Rate = 0; uint16 Channels = 0;
        Sound->GetImportedSoundWaveData(PCM, Rate, Channels);
        TestTrue(TEXT("Decoded PCM validates"), FHansaAudioTake::ValidatePCM(PCM, Rate, Channels, P, Error));
        TArray<uint8> Bad = PCM; Bad[0] = 255; Bad[1] = 127;
        TestFalse(TEXT("Clipping rejected natively"), FHansaAudioTake::ValidatePCM(Bad, Rate, Channels, P, Error));
        Bad.Init(0, PCM.Num());
        TestFalse(TEXT("Silent take rejected natively"), FHansaAudioTake::ValidatePCM(Bad, Rate, Channels, P, Error));
        TestFalse(TEXT("Wrong sample rate rejected"), FHansaAudioTake::ValidatePCM(PCM, 8000, Channels, P, Error));
        Bad = PCM; FMemory::Memzero(Bad.GetData(), (Bad.Num() * 3 / 4) & ~1);
        TestFalse(TEXT("Long leading silence rejected"), FHansaAudioTake::ValidatePCM(Bad, Rate, Channels, P, Error));
        Bad = PCM; const int32 TailStart = (Bad.Num() / 4) & ~1;
        FMemory::Memzero(Bad.GetData() + TailStart, Bad.Num() - TailStart);
        TestFalse(TEXT("Long trailing silence rejected"), FHansaAudioTake::ValidatePCM(Bad, Rate, Channels, P, Error));
        P->SetNumberField(TEXT("minimumDurationMs"), 500);
        TestFalse(TEXT("Duration outside range rejected"), FHansaAudioTake::ValidatePCM(PCM, Rate, Channels, P, Error));
        P->SetNumberField(TEXT("minimumDurationMs"), 100);
        FString Hash; Fixture.RecordMockPreview(Hash);
        const FString StableId = P->GetStringField(TEXT("stableId"));
        TestFalse(TEXT("Playback is required"), FHansaStagedMedia::Promote(Fixture.Receipt, Fixture.Destination, StableId,
            TEXT("Automation"), TEXT("Original fixture"), Hash, true, Error));
        TestTrue(TEXT("Playback remedy"), Error.Contains(TEXT("playback")));
        TSharedRef<FJsonObject> Preview = MakeShared<FJsonObject>();
        Preview->SetStringField(TEXT("reviewHash"), Hash); Preview->SetBoolField(TEXT("audioPlaybackStarted"), true);
        SaveJson(FPaths::ProjectDir() / (Fixture.Receipt + TEXT(".preview.json")), Preview);
        TestFalse(TEXT("Cannot approve a different stable line"), FHansaStagedMedia::Promote(Fixture.Receipt, Fixture.Destination,
            Kind == TEXT("Speech") ? TEXT("Dialogue.Other") : TEXT("SFX.Other"), TEXT("Automation"), TEXT("Original fixture"), Hash, true, Error));
        if (!TestTrue(TEXT("Selected audio promotes after explicit approval"), FHansaStagedMedia::Promote(Fixture.Receipt, Fixture.Destination,
            StableId, TEXT("Automation"), TEXT("Original mathematical test fixture; test-only approval surrogate"), Hash, true, Error)))
        { AddError(Error); return false; }
        const FString Name = TEXT("SW_") + StableId.Replace(TEXT("."), TEXT("_"));
        USoundWave* Final = LoadObject<USoundWave>(nullptr, *(Fixture.Destination / Name + TEXT(".") + Name));
        if (!TestNotNull(TEXT("Final stable asset name"), Final)) return false;
        TestTrue(TEXT("Promoted native audio settings preserved"), FHansaAudioTake::Validate({Final}, P, Error));
        TestTrue(TEXT("Retained promotion verifies"), FHansaStagedMedia::VerifyPromotion(Fixture.Receipt, Error));
    }
    return true;
}
