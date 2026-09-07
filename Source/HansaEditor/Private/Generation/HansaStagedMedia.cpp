#include "Generation/HansaStagedMedia.h"
#include "Generation/HansaStaticProp.h"
#include "Generation/HansaAudioTake.h"
#include "Engine/Texture2D.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetImportTask.h"
#include "EditorFramework/AssetImportData.h"
#include "UObject/UObjectHash.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundWave.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Misc/DataValidation.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/MetaData.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace Hansa::Editor::Generation
{
bool FHansaStagedMedia::bFailBeforeCommitForTests = false;
namespace
{
bool Fail(FString& Error, const TCHAR* Message) { Error = Message; return false; }
FString Field(const TSharedPtr<FJsonObject>& Json, const TCHAR* Name)
{
    FString Value;
    if (Json) Json->TryGetStringField(Name, Value);
    return Value;
}
bool ReadJson(const FString& File, TSharedPtr<FJsonObject>& Json)
{
    FString Text;
    return FFileHelper::LoadFileToString(Text, *File) &&
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) && Json.IsValid();
}
bool WriteJson(const FString& File, const TSharedRef<FJsonObject>& Json)
{
    FString Text;
    FJsonSerializer::Serialize(Json, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
    return FFileHelper::SaveStringToFile(Text, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
bool Token(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 100) return false;
    for (TCHAR Char : Value) if (!FChar::IsAlnum(Char) && Char != '_') return false;
    return true;
}
bool SafeSourcePath(const FString& Relative, FString& Absolute)
{
    if (!FPaths::IsRelative(Relative) || !Relative.StartsWith(TEXT("SourceArt/Generated/")) ||
        Relative.Contains(TEXT("..")) || Relative.Contains(TEXT("\\")) || Relative.Contains(TEXT(":"))) return false;
    Absolute = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Relative);
#if PLATFORM_WINDOWS
    FString Current = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    TArray<FString> Parts;
    Relative.ParseIntoArray(Parts, TEXT("/"), true);
    for (const FString& Part : Parts)
    {
        Current /= Part;
        const DWORD Attributes = GetFileAttributesW(*Current);
        if (Attributes != INVALID_FILE_ATTRIBUTES && (Attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    }
#endif
    return true;
}
bool Source(const FString& Descriptor, TSharedPtr<FJsonObject>& Json, FString& Absolute, FString& Error)
{
    FString DescriptorFile;
    if (!SafeSourcePath(Descriptor, DescriptorFile) || !ReadJson(DescriptorFile, Json))
        return Fail(Error, TEXT("Select a retained SourceArt/Generated source.json descriptor."));
    double Version = 0;
    if (!Json->TryGetNumberField(TEXT("schemaVersion"), Version) || Version != 1 ||
        !SafeSourcePath(Field(Json, TEXT("sourcePath")), Absolute))
        return Fail(Error, TEXT("Invalid media source contract or path."));
    FString ManifestFile, Hash;
    if (!FHansaStagedMedia::HashFile(Absolute, Hash) || Hash != Field(Json, TEXT("sha256")) ||
        !SafeSourcePath(Field(Json, TEXT("manifestPath")), ManifestFile) ||
        !FHansaStagedMedia::HashFile(ManifestFile, Hash) || Hash != Field(Json, TEXT("manifestFileSha256")))
        return Fail(Error, TEXT("Immutable source or manifest hash changed. Retain a new reviewed revision."));
    TSharedPtr<FJsonObject> Manifest;
    if (!ReadJson(ManifestFile, Manifest) || Field(Manifest, TEXT("status")) != TEXT("Review") ||
        Field(Manifest, TEXT("manifestHash")) != Field(Json, TEXT("manifestHash")))
        return Fail(Error, TEXT("Completed Review manifest required."));
    const TSharedPtr<FJsonObject>* Provider = nullptr;
    const TSharedPtr<FJsonObject>* Rights = nullptr;
    bool Acknowledged = false;
    if (!Manifest->TryGetObjectField(TEXT("provider"), Provider) ||
        Field(*Provider, TEXT("providerId")).IsEmpty() || Field(*Provider, TEXT("modelVersion")).IsEmpty() ||
        !Manifest->TryGetObjectField(TEXT("rights"), Rights) ||
        !(*Rights)->TryGetBoolField(TEXT("acknowledged"), Acknowledged) || !Acknowledged)
        return Fail(Error, TEXT("Provider, pinned model and acknowledged input rights required."));
    const TSharedPtr<FJsonObject>* Parameters = nullptr;
    const TSharedPtr<FJsonObject>* Profile = nullptr;
    const bool HasProfile = Manifest->TryGetObjectField(TEXT("parameters"), Parameters) &&
        (*Parameters)->TryGetObjectField(TEXT("staticProp"), Profile);
    if (HasProfile)
    {
        if (!FHansaStaticProp::ValidateProfile(*Profile, Error)) return false;
        Json->SetObjectField(TEXT("staticProp"), *Profile); // derived from the hash-bound manifest, never caller settings
    }
    else if (Field(*Provider, TEXT("providerId")) == TEXT("tripo"))
        return Fail(Error, TEXT("Tripo media requires the canonical static-prop profile."));
    const TSharedPtr<FJsonObject>* Audio = nullptr;
    if (Parameters && (*Parameters)->TryGetObjectField(TEXT("audioTake"), Audio))
    {
        if (!FHansaAudioTake::ValidateProfile(*Audio, Error)) return false;
        if (Field(*Audio, TEXT("kind")) == TEXT("Speech"))
        {
            bool Voice = false, English = false;
            if (!(*Rights)->TryGetBoolField(TEXT("voiceAcknowledged"), Voice) || !Voice ||
                !(*Rights)->TryGetBoolField(TEXT("englishTextAcknowledged"), English) || !English ||
                Field(Manifest, TEXT("prompt")) != Field(*Audio, TEXT("subtitle")))
                return Fail(Error, TEXT("Speech requires acknowledged voice/English rights and an exact subtitle/source-text match."));
        }
        double SelectedTake = -1, VariantCount = 0;
        if (!Json->TryGetNumberField(TEXT("outputIndex"), SelectedTake) ||
            !(*Audio)->TryGetNumberField(TEXT("variants"), VariantCount) ||
            SelectedTake < 0 || SelectedTake >= VariantCount || SelectedTake != FMath::FloorToDouble(SelectedTake))
            return Fail(Error, TEXT("Audio source does not select an approved take index."));
        if (Field(*Provider, TEXT("providerId")) == TEXT("elevenlabs"))
        {
            const TSharedPtr<FJsonObject>* Validation = nullptr;
            const TSharedPtr<FJsonObject>* Original = nullptr;
            FString OriginalFile;
            if (!Json->TryGetObjectField(TEXT("validation"), Validation) ||
                !(*Validation)->TryGetObjectField(TEXT("providerOriginal"), Original) ||
                Field(*Original, TEXT("relativePath")) != TEXT("provider-original.mp3") ||
                !SafeSourcePath(FPaths::GetPath(Descriptor) / TEXT("provider-original.mp3"), OriginalFile) ||
                !FHansaStagedMedia::HashFile(OriginalFile, Hash) || Hash != Field(*Original, TEXT("sha256")))
                return Fail(Error, TEXT("Original ElevenLabs source hash changed or is missing."));
        }
        Json->SetObjectField(TEXT("audioTake"), *Audio);
    }
    else if (Field(*Provider, TEXT("providerId")) == TEXT("elevenlabs"))
        return Fail(Error, TEXT("ElevenLabs sources require the canonical AudioTake profile."));
    const FString Media = Field(Json, TEXT("mediaType"));
    if (Media != TEXT("audio/wav") && Media != TEXT("model/gltf-binary"))
        return Fail(Error, TEXT("Media contract v1 accepts only WAV and self-contained GLB."));
    if (IFileManager::Get().FileSize(*Absolute) <= 0 || IFileManager::Get().FileSize(*Absolute) > 67108864)
        return Fail(Error, TEXT("Source exceeds the 64 MiB import limit."));
    return true;
}
bool SaveAsset(UObject* Asset, const FString& File)
{
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Asset->GetPackage(), Asset, *File, Args);
}
bool Allowed(UObject* Asset)
{
    return Asset && (Asset->IsA<UStaticMesh>() || Asset->IsA<USoundWave>() ||
        Asset->IsA<UMaterialInterface>() || Asset->IsA<UTexture>());
}
bool Validate(UObject* Asset, FString& Error)
{
    if (!Allowed(Asset)) return Fail(Error, TEXT("Imported bundle contains a prohibited asset class."));
    FDataValidationContext Context;
    if (Asset->IsDataValid(Context) == EDataValidationResult::Invalid)
        return Fail(Error, TEXT("Native asset validation failed. Inspect the staging asset."));
    if (const UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
        if (Mesh->GetNumLODs() < 1 || Mesh->GetNumTriangles(0) < 1)
            return Fail(Error, TEXT("Static mesh has no renderable geometry."));
    if (const USoundWave* Sound = Cast<USoundWave>(Asset))
        if (Sound->Duration <= 0 || Sound->NumChannels < 1 || Sound->NumChannels > 2)
            return Fail(Error, TEXT("Audio did not decode to a nonempty mono/stereo sound."));
    return true;
}

// Audit both hard and soft references recursively, including imported subobjects,
// before any production bytes become visible.
class FMediaReferenceAudit final : public FArchiveUObject
{
public:
    TArray<FString> Forbidden;
    UObject* Root;
    TSet<UObject*> Visited;
    explicit FMediaReferenceAudit(UObject* InRoot) : Root(InRoot)
    {
        ArIsObjectReferenceCollector = true;
        ArIgnoreOuterRef = true;
        ArIgnoreArchetypeRef = true;
        Visit(Root);
    }
    void Visit(UObject* Object)
    {
        if (!Object || Visited.Contains(Object)) return;
        Visited.Add(Object);
        Object->Serialize(*this);
    }
    virtual FArchive& operator<<(UObject*& Object) override
    {
        if (Object && FHansaStagedMedia::IsForbiddenPackage(Object->GetPackage()->GetName()))
            Forbidden.AddUnique(Object->GetPathName());
        if (Object && Object->IsIn(Root)) Visit(Object);
        return *this;
    }
    virtual FArchive& operator<<(FSoftObjectPath& Value) override
    {
        if (FHansaStagedMedia::IsForbiddenPackage(Value.GetLongPackageName()))
            Forbidden.AddUnique(Value.ToString());
        return *this;
    }
};

void Discard(TArray<UObject*>& Assets)
{
    TSet<UPackage*> Packages;
    for (UObject* Asset : Assets)
    {
        Packages.Add(Asset->GetPackage());
        FAssetRegistryModule::AssetDeleted(Asset);
        Asset->ClearFlags(RF_Public | RF_Standalone);
        Asset->GetPackage()->SetDirtyFlag(false);
        Asset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        Asset->MarkAsGarbage();
    }
    for (UPackage* Package : Packages)
        Package->Rename(*(TEXT("/Temp/HansaDiscard_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)), nullptr, REN_DontCreateRedirectors | REN_NonTransactional);
}
}

bool FHansaStagedMedia::HashFile(const FString& Filename, FString& Hash)
{
    TArray<uint8> Bytes;
    if (IFileManager::Get().FileSize(*Filename) > 268435456) return false;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename)) return false;
#if PLATFORM_WINDOWS
    BCRYPT_ALG_HANDLE Algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    uint8 Digest[32] = {};
    const NTSTATUS Status = BCryptHash(Algorithm, nullptr, 0, Bytes.GetData(), Bytes.Num(), Digest, 32);
    BCryptCloseAlgorithmProvider(Algorithm, 0);
    if (Status < 0) return false;
    Hash = BytesToHex(Digest, 32).ToLower();
    return true;
#else
    return false;
#endif
}

bool FHansaStagedMedia::IsForbiddenPackage(const FString& Package)
{
    return Package.Equals(TEXT("/Game/Hansa/Generated/Staging"), ESearchCase::IgnoreCase) ||
        Package.StartsWith(TEXT("/Game/Hansa/Generated/Staging/"), ESearchCase::IgnoreCase) ||
        Package.Equals(TEXT("/Game/Hansa/Developer"), ESearchCase::IgnoreCase) ||
        Package.StartsWith(TEXT("/Game/Hansa/Developer/"), ESearchCase::IgnoreCase) ||
        Package.StartsWith(TEXT("/Game/Developers/"), ESearchCase::IgnoreCase);
}
bool FHansaStagedMedia::IsProductionDestination(const FString& Package)
{
    if (!Package.StartsWith(TEXT("/Game/Hansa/Meshes/")) && !Package.StartsWith(TEXT("/Game/Hansa/Audio/"))) return false;
    TArray<FString> Parts;
    Package.ParseIntoArray(Parts, TEXT("/"), false);
    for (int32 Index = 1; Index < Parts.Num(); ++Index) if (!Token(Parts[Index])) return false;
    return !IsForbiddenPackage(Package) && !Package.EndsWith(TEXT("/")) && FPackageName::IsValidLongPackageName(Package);
}

bool FHansaStagedMedia::AuditReferences(TArray<FString>& Errors)
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(true);
    TArray<FAssetData> Assets;
    Registry.GetAssetsByPath(TEXT("/Game"), Assets, true);
    TSet<FName> Checked;
    for (const FAssetData& Asset : Assets)
    {
        if (IsForbiddenPackage(Asset.PackageName.ToString()) || Checked.Contains(Asset.PackageName)) continue;
        Checked.Add(Asset.PackageName);
        TArray<FName> Dependencies;
        Registry.GetDependencies(Asset.PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
        for (FName Dependency : Dependencies)
            if (IsForbiddenPackage(Dependency.ToString()))
                Errors.Add(Asset.PackageName.ToString() + TEXT(" references forbidden ") + Dependency.ToString());
    }
    return Errors.IsEmpty();
}

bool FHansaStagedMedia::Stage(const FString& DescriptorPath, FString& OutReceiptPath, FString& Error)
{
    check(IsInGameThread());
    TSharedPtr<FJsonObject> Descriptor;
    FString SourceFile;
    if (!Source(DescriptorPath, Descriptor, SourceFile, Error)) return false;
    const FString Job = Field(Descriptor, TEXT("jobId"));
    FGuid Guid;
    if (!FGuid::Parse(Job, Guid)) return Fail(Error, TEXT("Invalid Hansa job identity."));
    // Each import attempt gets an isolated bundle. Never reimport over an earlier review.
    const FString Bundle = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString StagePath = TEXT("/Game/Hansa/Generated/Staging/") + Guid.ToString(EGuidFormats::Digits) + TEXT("/") + Bundle;
    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    Task->Filename = SourceFile;
    Task->DestinationPath = StagePath;
    Task->DestinationName = TEXT("Artifact");
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bReplaceExisting = false;
    FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks({Task});
    // Importers may create materials/textures in addition to task results.
    TArray<UObject*> Assets;
    for (TObjectIterator<UObject> It; It; ++It)
        if (It->IsAsset() && It->GetPackage()->GetName().StartsWith(StagePath + TEXT("/"))) Assets.Add(*It);
    Assets.Sort([](const UObject& A, const UObject& B) { return A.GetPathName() < B.GetPathName(); });
    if (Assets.IsEmpty()) return Fail(Error, TEXT("Importer produced no staging assets."));
    const TSharedPtr<FJsonObject>* Profile = nullptr;
    TSharedPtr<FJsonObject> Normalization;
    if (Descriptor->TryGetObjectField(TEXT("staticProp"), Profile) && !FHansaStaticProp::Prepare(Assets, *Profile, Error, &Normalization))
    { Discard(Assets); return false; }
    const TSharedPtr<FJsonObject>* AudioProfile = nullptr;
    if (Descriptor->TryGetObjectField(TEXT("audioTake"), AudioProfile) && !FHansaAudioTake::Prepare(Assets, *AudioProfile, Error))
    { Discard(Assets); return false; }
    TArray<TSharedPtr<FJsonValue>> Records;
    for (UObject* Asset : Assets)
    {
        if (!Validate(Asset, Error)) { Discard(Assets); return false; }
        const FString File = FPackageName::LongPackageNameToFilename(Asset->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
        if (!SaveAsset(Asset, File)) return Fail(Error, TEXT("Could not save staging bundle; production is unchanged."));
        FString Hash;
        if (!HashFile(File, Hash)) return Fail(Error, TEXT("Could not hash staged package."));
        TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
        Record->SetStringField(TEXT("objectPath"), Asset->GetPathName());
        Record->SetStringField(TEXT("sha256"), Hash);
        Records.Add(MakeShared<FJsonValueObject>(Record));
    }
    TSharedRef<FJsonObject> Receipt = MakeShared<FJsonObject>();
    Receipt->SetNumberField(TEXT("schemaVersion"), 1);
    FString DescriptorFile, DescriptorHash;
    if (!SafeSourcePath(DescriptorPath, DescriptorFile) || !HashFile(DescriptorFile, DescriptorHash))
        return Fail(Error, TEXT("Could not bind the source descriptor to this import."));
    Receipt->SetStringField(TEXT("descriptorPath"), DescriptorPath);
    Receipt->SetStringField(TEXT("descriptorHash"), DescriptorHash);
    Receipt->SetStringField(TEXT("stagePath"), StagePath);
    Receipt->SetArrayField(TEXT("assets"), Records);
    if (AudioProfile)
    {
        Receipt->SetObjectField(TEXT("audioTake"), *AudioProfile);
        Receipt->SetStringField(TEXT("audioQA"), TEXT("AudioTake-v1: native PCM decode, channels/rate/duration, no clipping, bounded leading/trailing silence, PCM compression, subtitle and category"));
    }
    if (Profile)
    {
        Receipt->SetObjectField(TEXT("staticProp"), *Profile);
        Receipt->SetObjectField(TEXT("normalization"), Normalization);
        Receipt->SetStringField(TEXT("nativeQA"), TEXT("HarborProp-v1: centimeters, +X-forward/+Z-up contract, baked uniform height and bottom-center pivot, triangle/material/texture limits, box collision"));
    }
    OutReceiptPath = FPaths::GetPath(DescriptorPath) / (TEXT("import-") + Bundle + TEXT(".json"));
    FString ReceiptFile;
    if (!SafeSourcePath(OutReceiptPath, ReceiptFile) || !WriteJson(ReceiptFile, Receipt))
        return Fail(Error, TEXT("Could not persist staging receipt."));
    return true;
}

bool FHansaStagedMedia::LoadReview(const FString& ReceiptPath, TSharedPtr<FJsonObject>& Receipt,
    TArray<UObject*>& Assets, FString& ReviewHash, FString& Error)
{
    FString File;
    if (!SafeSourcePath(ReceiptPath, File) || !ReadJson(File, Receipt) || !HashFile(File, ReviewHash))
        return Fail(Error, TEXT("Invalid retained import receipt."));
    double ReceiptVersion = 0;
    if (!Receipt->TryGetNumberField(TEXT("schemaVersion"), ReceiptVersion) || ReceiptVersion != 1)
        return Fail(Error, TEXT("Unsupported staging receipt version; re-stage from retained source."));
    TSharedPtr<FJsonObject> Descriptor;
    FString SourceFile;
    if (!Source(Field(Receipt, TEXT("descriptorPath")), Descriptor, SourceFile, Error)) return false;
    FString DescriptorFile, DescriptorHash;
    if (!SafeSourcePath(Field(Receipt, TEXT("descriptorPath")), DescriptorFile) ||
        !HashFile(DescriptorFile, DescriptorHash) || DescriptorHash != Field(Receipt, TEXT("descriptorHash")))
        return Fail(Error, TEXT("Reviewed source descriptor changed; create a new import receipt."));
    const FString StagePath = Field(Receipt, TEXT("stagePath"));
    if (!StagePath.StartsWith(TEXT("/Game/Hansa/Generated/Staging/")) || !FPackageName::IsValidLongPackageName(StagePath))
        return Fail(Error, TEXT("Receipt does not identify isolated staging."));
    const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
    if (!Receipt->TryGetArrayField(TEXT("assets"), Records) || Records->IsEmpty() || Records->Num() > 64)
        return Fail(Error, TEXT("Receipt must contain 1 through 64 staged assets."));
    TSet<FString> Seen;
    for (const auto& Record : *Records)
    {
        const TSharedPtr<FJsonObject> Item = Record->AsObject();
        const FString ObjectPath = Field(Item, TEXT("objectPath"));
        const FString Package = FPackageName::ObjectPathToPackageName(ObjectPath);
        FString Hash;
        if (!Package.StartsWith(StagePath + TEXT("/")) || Seen.Contains(Package) ||
            !HashFile(FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension()), Hash) ||
            Hash != Field(Item, TEXT("sha256")))
            return Fail(Error, TEXT("Staging bytes changed since import; create and preview a new receipt."));
        Seen.Add(Package);
        UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
        if (!Asset || Asset->GetPackage()->IsDirty() || !Validate(Asset, Error))
            return Fail(Error, TEXT("Staged asset is missing, modified or invalid; re-stage and preview."));
        Assets.Add(Asset);
    }
    const TSharedPtr<FJsonObject>* Profile = nullptr;
    if (Descriptor->TryGetObjectField(TEXT("staticProp"), Profile) && !FHansaStaticProp::Validate(Assets, *Profile, Error)) return false;
    const TSharedPtr<FJsonObject>* AudioProfile = nullptr;
    if (Descriptor->TryGetObjectField(TEXT("audioTake"), AudioProfile) && !FHansaAudioTake::Validate(Assets, *AudioProfile, Error)) return false;
    TArray<FString> ReferenceErrors;
    if (!AuditReferences(ReferenceErrors))
    { Error = FString::Join(ReferenceErrors, TEXT("\n")); return false; }
    return true;
}

bool FHansaStagedMedia::Preview(const FString& ReceiptPath, FString& OutReviewHash, FString& Error)
{
    TSharedPtr<FJsonObject> Receipt;
    TArray<UObject*> Assets;
    if (!LoadReview(ReceiptPath, Receipt, Assets, OutReviewHash, Error)) return false;
    if (!GEditor || IsRunningCommandlet()) return Fail(Error, TEXT("Open the Editor to preview staged mesh/audio assets."));
    FString ReceiptFile;
    SafeSourcePath(ReceiptPath, ReceiptFile);
    TSharedRef<FJsonObject> PreviewRecord = MakeShared<FJsonObject>();
    if (Receipt->HasField(TEXT("staticProp")))
    {
        UStaticMesh* Mesh = nullptr;
        for (UObject* Asset : Assets) if (UStaticMesh* Candidate = Cast<UStaticMesh>(Asset)) Mesh = Candidate;
        UTexture2D* Capture = nullptr;
        if (!FHansaStaticProp::RenderPreview(Mesh, ReceiptFile + TEXT(".preview.png"), Capture, Error)) return false;
        Assets.Add(Capture);
        FString CaptureHash;
        if (!HashFile(ReceiptFile + TEXT(".preview.png"), CaptureHash)) return Fail(Error, TEXT("Could not hash prop review capture."));
        PreviewRecord->SetStringField(TEXT("captureSha256"), CaptureHash);
        PreviewRecord->SetStringField(TEXT("scene"), TEXT("HarborProp-v1-1280x720-camera(1.6,-2.4,1.5)-distance6r-fov45-key(-40,120)-fill(-25,-30)-exposureManual"));
    }
    if (!GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(Assets))
        return Fail(Error, TEXT("Native asset preview could not open."));
    if (Receipt->HasField(TEXT("audioTake")))
    {
        USoundWave* Sound = Assets.Num() == 1 ? Cast<USoundWave>(Assets[0]) : nullptr;
        if (!Sound || !GEditor->PlayPreviewSound(Sound))
            return Fail(Error, TEXT("Audio playback could not start. Enable Editor audio and listen to the selected take before approval."));
        PreviewRecord->SetBoolField(TEXT("audioPlaybackStarted"), true);
    }
    PreviewRecord->SetStringField(TEXT("reviewHash"), OutReviewHash);
    PreviewRecord->SetStringField(TEXT("openedAt"), FDateTime::UtcNow().ToIso8601());
    return WriteJson(ReceiptFile + TEXT(".preview.json"), PreviewRecord);
}


bool FHansaStagedMedia::VerifyPromotion(const FString& ReceiptPath, FString& Error)
{
    FString ReceiptFile;
    TSharedPtr<FJsonObject> Receipt, Decision;
    if (!SafeSourcePath(ReceiptPath, ReceiptFile) || !ReadJson(ReceiptFile, Receipt) ||
        !ReadJson(ReceiptFile + TEXT(".promotion.json"), Decision))
        return Fail(Error, TEXT("Retained promotion decision not found."));
    FString Hash;
    if (!HashFile(ReceiptFile, Hash) || Hash != Field(Decision, TEXT("reviewHash")))
        return Fail(Error, TEXT("Promotion receipt changed."));
    double ReceiptVersion = 0;
    if (!Receipt->TryGetNumberField(TEXT("schemaVersion"), ReceiptVersion) || ReceiptVersion != 1)
        return Fail(Error, TEXT("Unsupported staging receipt version; re-stage from retained source."));
    TSharedPtr<FJsonObject> Descriptor;
    FString SourceFile;
    if (!Source(Field(Receipt, TEXT("descriptorPath")), Descriptor, SourceFile, Error)) return false;
    FString DescriptorFile;
    if (!SafeSourcePath(Field(Receipt, TEXT("descriptorPath")), DescriptorFile) ||
        !HashFile(DescriptorFile, Hash) || Hash != Field(Receipt, TEXT("descriptorHash")))
        return Fail(Error, TEXT("Reviewed source descriptor changed."));
    const FString Destination = Field(Decision, TEXT("destination"));
    if (!IsProductionDestination(Destination)) return Fail(Error, TEXT("Invalid retained destination."));
    const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
    if (!Decision->TryGetArrayField(TEXT("assets"), Assets) || Assets->IsEmpty())
        return Fail(Error, TEXT("Promotion decision lacks final package hashes."));
    for (const auto& Value : *Assets)
    {
        const auto Asset = Value->AsObject();
        const FString Package = FPackageName::ObjectPathToPackageName(Field(Asset, TEXT("objectPath")));
        if (!Package.StartsWith(Destination + TEXT("/")) ||
            !HashFile(FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension()), Hash) ||
            Hash != Field(Asset, TEXT("sha256")))
            return Fail(Error, TEXT("Promoted package is missing or changed; preserve evidence and review a new revision."));
    }
    // Recreate the terminal marker after a crash between commit and bookkeeping.
    const FString MarkerFile = ReceiptFile + TEXT(".committed.json");
    if (!IFileManager::Get().FileExists(*MarkerFile))
    {
        Decision->SetStringField(TEXT("state"), TEXT("Promoted"));
        if (!WriteJson(MarkerFile, Decision.ToSharedRef()))
            return Fail(Error, TEXT("Production is committed but its retained marker could not be saved. Run Hansa.Media.Verify."));
    }
    return true;
}

bool FHansaStagedMedia::Promote(const FString& ReceiptPath, const FString& Destination,
    const FString& StableId, const FString& Reviewer, const FString& RightsStatement,
    const FString& ReviewedHash, bool bApprove, FString& Error)
{
    check(IsInGameThread());
    if (!bApprove || Reviewer.TrimStartAndEnd().IsEmpty() || RightsStatement.TrimStartAndEnd().IsEmpty())
        return Fail(Error, TEXT("Explicit named promotion approval and output-rights statement required."));
    if (Reviewer.Len() > 128 || RightsStatement.Len() > 1024 || !IsProductionDestination(Destination))
        return Fail(Error, TEXT("Choose a new /Game/Hansa/Meshes/Feature or /Game/Hansa/Audio/Feature directory."));
    TArray<FString> IdParts;
    StableId.ParseIntoArray(IdParts, TEXT("."), false);
    if (IdParts.Num() < 2 || (IdParts[0] != TEXT("Prop") && IdParts[0] != TEXT("SFX") && IdParts[0] != TEXT("Dialogue")))
        return Fail(Error, TEXT("Use a stable Hansa Prop.*, SFX.* or Dialogue.* identity."));
    for (const FString& Part : IdParts) if (!Token(Part)) return Fail(Error, TEXT("Invalid stable Hansa media ID."));
    TSharedPtr<FJsonObject> Receipt;
    TArray<UObject*> Staged;
    FString Hash;
    if (!LoadReview(ReceiptPath, Receipt, Staged, Hash, Error)) return false;
    if (Hash != ReviewedHash) return Fail(Error, TEXT("Approval does not match the exact reviewed bundle."));
    const bool HasMesh = Staged.ContainsByPredicate([](const UObject* Asset) { return Asset->IsA<UStaticMesh>(); });
    const bool HasAudio = Staged.ContainsByPredicate([](const UObject* Asset) { return Asset->IsA<USoundWave>(); });
    if (HasMesh == HasAudio || (HasMesh && (!Destination.StartsWith(TEXT("/Game/Hansa/Meshes/")) || IdParts[0] != TEXT("Prop"))) ||
        (HasAudio && (!Destination.StartsWith(TEXT("/Game/Hansa/Audio/")) || IdParts[0] == TEXT("Prop"))))
        return Fail(Error, TEXT("Media class, stable ID domain and destination domain must agree."));
    TSharedPtr<FJsonObject> SourceDescriptor;
    FString OriginalSource;
    if (!Source(Field(Receipt, TEXT("descriptorPath")), SourceDescriptor, OriginalSource, Error)) return false;
    const TSharedPtr<FJsonObject>* AudioProfile = nullptr;
    if (SourceDescriptor->TryGetObjectField(TEXT("audioTake"), AudioProfile))
    {
        if (StableId != Field(*AudioProfile, TEXT("stableId")))
            return Fail(Error, TEXT("Audio promotion identity must match its approved SFX/dialogue line."));
        FString AudioReceiptFile;
        TSharedPtr<FJsonObject> AudioPreview;
        bool Played = false;
        SafeSourcePath(ReceiptPath, AudioReceiptFile);
        if (!ReadJson(AudioReceiptFile + TEXT(".preview.json"), AudioPreview) ||
            !AudioPreview->TryGetBoolField(TEXT("audioPlaybackStarted"), Played) || !Played)
            return Fail(Error, TEXT("Preview playback of the selected audio take is required before approval."));
    }
    const TSharedPtr<FJsonObject>* PropProfile = nullptr;
    if (SourceDescriptor->TryGetObjectField(TEXT("staticProp"), PropProfile))
    {
        if (StableId != Field(*PropProfile, TEXT("stableId")))
            return Fail(Error, TEXT("Promotion stable ID must match the approved harbor-prop contract."));
        FString PropReceiptFile, CaptureHash;
        TSharedPtr<FJsonObject> PropPreview;
        SafeSourcePath(ReceiptPath, PropReceiptFile);
        if (!ReadJson(PropReceiptFile + TEXT(".preview.json"), PropPreview) ||
            !HashFile(PropReceiptFile + TEXT(".preview.png"), CaptureHash) ||
            CaptureHash != Field(PropPreview, TEXT("captureSha256")) ||
            !Field(PropPreview, TEXT("scene")).StartsWith(TEXT("HarborProp-v1-1280x720-")))
            return Fail(Error, TEXT("A matching deterministic prop scene capture is required before human promotion approval."));
    }
    FString ReceiptFile;
    SafeSourcePath(ReceiptPath, ReceiptFile);
    TSharedPtr<FJsonObject> PreviewRecord;
    if (!ReadJson(ReceiptFile + TEXT(".preview.json"), PreviewRecord) || Field(PreviewRecord, TEXT("reviewHash")) != Hash)
        return Fail(Error, TEXT("Open the staged preview before approving these bytes."));
    const FString FinalDirectory = FPackageName::LongPackageNameToFilename(Destination);
#if PLATFORM_WINDOWS
    FString Current = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
    TArray<FString> DestinationParts;
    Destination.RightChop(6).ParseIntoArray(DestinationParts, TEXT("/"), true);
    for (const FString& Part : DestinationParts)
    {
        Current /= Part;
        const DWORD Attributes = GetFileAttributesW(*Current);
        if (Attributes != INVALID_FILE_ATTRIBUTES && (Attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            return Fail(Error, TEXT("Production destinations cannot traverse links or junctions."));
    }
#endif

#if PLATFORM_WINDOWS
    // Serialize the same receipt across separate editor processes. The OS releases
    // the handle on crash; durable commit state never depends on this Saved lock.
    const FString LockDirectory = FPaths::ProjectSavedDir() / TEXT("MediaPromotion/Locks");
    IFileManager::Get().MakeDirectory(*LockDirectory, true);
    const FString LockFile = FPaths::ConvertRelativePathToFull(LockDirectory / (Hash + TEXT(".lock")));
    HANDLE PromotionLock = CreateFileW(*LockFile, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (PromotionLock == INVALID_HANDLE_VALUE)
        return Fail(Error, TEXT("Another editor is promoting this receipt; verify its destination and retry."));
    ON_SCOPE_EXIT { CloseHandle(PromotionLock); };
#endif
    const FString ExistingDecisionFile = ReceiptFile + TEXT(".promotion.json");
    if (IFileManager::Get().FileExists(*ExistingDecisionFile))
    {
        TSharedPtr<FJsonObject> Previous;
        if (!ReadJson(ExistingDecisionFile, Previous) || Field(Previous, TEXT("destination")) != Destination ||
            Field(Previous, TEXT("reviewHash")) != Hash || Field(Previous, TEXT("stableId")) != StableId)
            return Fail(Error, TEXT("A different promotion decision already exists; inspect its recorded destination."));
        if (IFileManager::Get().DirectoryExists(*FinalDirectory))
            return VerifyPromotion(ReceiptPath, Error);
        // A process died before the atomic move. Archive the uncommitted decision.
        const FString Aborted = ExistingDecisionFile + TEXT(".aborted-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        if (!IFileManager::Get().Move(*Aborted, *ExistingDecisionFile, false, false, false, true))
            return Fail(Error, TEXT("Could not archive interrupted pre-commit decision."));
    }

    if (IFileManager::Get().DirectoryExists(*FinalDirectory))
        return Fail(Error, TEXT("Destination already exists. Silent overwrite is prohibited; choose a new feature revision."));
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> Existing;
    Registry.GetAssetsByPath(FName(*Destination), Existing, true);
    if (!Existing.IsEmpty()) return Fail(Error, TEXT("Destination has existing registered assets; inspect referencers and choose a new revision."));
    const FString Working = FPaths::ProjectSavedDir() / TEXT("MediaPromotion") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    IFileManager::Get().MakeDirectory(*Working, true);
    TArray<UObject*> Copies;
    bool Committed = false;
    ON_SCOPE_EXIT
    {
        if (!Committed) Discard(Copies);
        IFileManager::Get().DeleteDirectory(*Working, false, true);
    };
    FScopedTransaction Transaction(NSLOCTEXT("HansaMedia", "Promote", "Promote approved Hansa media bundle"));
    TMap<UObject*, UObject*> Replacements;
    for (int32 Index = 0; Index < Staged.Num(); ++Index)
    {
        // Hansa-owned names, independent of provider filenames and task IDs.
        const FString Name = AudioProfile
            ? TEXT("SW_") + StableId.Replace(TEXT("."), TEXT("_"))
            : FString::Printf(TEXT("Asset_%03d"), Index + 1);
        const FString PackageName = Destination / Name;
        if (FindPackage(nullptr, *PackageName)) { Transaction.Cancel(); return Fail(Error, TEXT("Destination package is already loaded.")); }
        UPackage* Package = CreatePackage(*PackageName);
        Package->SetFlags(RF_Transactional);
        UObject* Copy = StaticDuplicateObject(Staged[Index], Package, FName(*Name));
        if (!Copy) { Transaction.Cancel(); return Fail(Error, TEXT("Could not duplicate staging asset.")); }
        Copy->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
        Copies.Add(Copy);
        Replacements.Add(Staged[Index], Copy);
    }
    TArray<TSharedPtr<FJsonValue>> Promoted;
    TSharedRef<FJsonObject> Decision = MakeShared<FJsonObject>();
    Decision->SetNumberField(TEXT("schemaVersion"), 1);
    Decision->SetStringField(TEXT("state"), TEXT("Approved"));
    Decision->SetStringField(TEXT("reviewHash"), Hash);
    Decision->SetStringField(TEXT("reviewer"), Reviewer);
    Decision->SetStringField(TEXT("outputRights"), RightsStatement);
    Decision->SetStringField(TEXT("stableId"), StableId);
    Decision->SetStringField(TEXT("destination"), Destination);
    Decision->SetStringField(TEXT("receiptPath"), ReceiptPath);
    Decision->SetStringField(TEXT("approvedAt"), FDateTime::UtcNow().ToIso8601());
    for (UObject* Copy : Copies)
    {
        FArchiveReplaceObjectRef<UObject> Replace(Copy, Replacements, EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
        // Relative importer source filenames were based on the staging package.
        // Rebase them against the new package without changing the retained source.
        TArray<UObject*> Subobjects;
        GetObjectsWithOuter(Copy, Subobjects, true);
        for (UObject* Subobject : Subobjects)
            if (UAssetImportData* ImportData = Cast<UAssetImportData>(Subobject))
                ImportData->UpdateFilenameOnly(OriginalSource);
        FMetaData* Metadata = &Copy->GetPackage()->GetMetaData();
        Metadata->SetValue(Copy, TEXT("Hansa.StableMediaId"), *StableId);
        Metadata->SetValue(Copy, TEXT("Hansa.SourceReceipt"), *ReceiptPath);
        Metadata->SetValue(Copy, TEXT("Hansa.SourceSha256"), *Field(SourceDescriptor, TEXT("sha256")));
        Metadata->SetValue(Copy, TEXT("Hansa.ManifestSha256"), *Field(SourceDescriptor, TEXT("manifestHash")));
        Metadata->SetValue(Copy, TEXT("Hansa.ReviewHash"), *Hash);
        Metadata->SetValue(Copy, TEXT("Hansa.Reviewer"), *Reviewer);
        Metadata->SetValue(Copy, TEXT("Hansa.OutputRights"), *RightsStatement);
        if (AudioProfile)
        {
            Metadata->SetValue(Copy, TEXT("Hansa.SpeakerId"), *Field(*AudioProfile, TEXT("speakerId")));
            Metadata->SetValue(Copy, TEXT("Hansa.Subtitle"), *Field(*AudioProfile, TEXT("subtitle")));
            Metadata->SetValue(Copy, TEXT("Hansa.AudioContract"), TEXT("AudioTake-v1"));
        }
        FMediaReferenceAudit ReferenceAudit(Copy);
        if (!ReferenceAudit.Forbidden.IsEmpty()) { Transaction.Cancel(); Error = FString::Join(ReferenceAudit.Forbidden, TEXT("\n")); return false; }
        if (!Validate(Copy, Error)) { Transaction.Cancel(); return false; }
        const FString File = Working / (Copy->GetName() + TEXT(".uasset"));
        if (!SaveAsset(Copy, File)) { Transaction.Cancel(); return Fail(Error, TEXT("Package save failed; no production files were committed.")); }
        FString PackageHash;
        if (!HashFile(File, PackageHash)) { Transaction.Cancel(); return Fail(Error, TEXT("Package hash failed.")); }
        TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("objectPath"), Copy->GetPathName());
        Item->SetStringField(TEXT("sha256"), PackageHash);
        Promoted.Add(MakeShared<FJsonValueObject>(Item));
    }
    Decision->SetArrayField(TEXT("assets"), Promoted);
    // Journal is retained outside Saved before commit. A crash after the directory
    // move is reconciled by verifying these exact final package hashes.
    const FString DecisionFile = ReceiptFile + TEXT(".promotion.json");
    if (IFileManager::Get().FileExists(*DecisionFile))
    { Transaction.Cancel(); return Fail(Error, TEXT("A promotion decision already exists; inspect its destination before retrying.")); }
    if (!WriteJson(DecisionFile, Decision)) { Transaction.Cancel(); return Fail(Error, TEXT("Could not retain promotion decision.")); }
    if (bFailBeforeCommitForTests)
    {
        IFileManager::Get().Delete(*DecisionFile);
        Transaction.Cancel();
        return Fail(Error, TEXT("Injected pre-commit failure; production is unchanged."));
    }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(FinalDirectory), true);
    // Same-volume directory rename is the single visibility/commit point.
    if (!IFileManager::Get().Move(*FinalDirectory, *Working, false, true, false, true))
    {
        IFileManager::Get().Delete(*DecisionFile);
        Transaction.Cancel();
        return Fail(Error, TEXT("Atomic bundle commit failed; production is unchanged."));
    }
    Committed = true;
    for (UObject* Copy : Copies) FAssetRegistryModule::AssetCreated(Copy);
    Registry.ScanPathsSynchronous({Destination}, true);
    return VerifyPromotion(ReceiptPath, Error);
}

static FAutoConsoleCommand VerifyCommand(TEXT("Hansa.Media.Verify"), TEXT("Retained import receipt; verify commit and reconcile its terminal marker."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        FString Error;
        if (Args.Num() != 1 || !FHansaStagedMedia::VerifyPromotion(Args[0], Error))
        { UE_LOG(LogTemp, Error, TEXT("Media verification failed: %s"), *Error); }
        else { UE_LOG(LogTemp, Display, TEXT("Promoted media hashes and provenance verified.")); }
    }));

// Existing Unreal console + native asset previews: no separate visual design.
static FAutoConsoleCommand StageCommand(TEXT("Hansa.Media.Stage"), TEXT("Retained source.json project-relative path."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        FString Receipt, Error;
        if (Args.Num() != 1 || !FHansaStagedMedia::Stage(Args[0], Receipt, Error))
        { UE_LOG(LogTemp, Error, TEXT("Media staging failed: %s"), *Error); }
        else { UE_LOG(LogTemp, Display, TEXT("Staged receipt: %s"), *Receipt); }
    }));
static FAutoConsoleCommand PreviewCommand(TEXT("Hansa.Media.Preview"), TEXT("Import receipt project-relative path."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        FString Hash, Error;
        if (Args.Num() != 1 || !FHansaStagedMedia::Preview(Args[0], Hash, Error))
        { UE_LOG(LogTemp, Error, TEXT("Media preview failed: %s"), *Error); }
        else { UE_LOG(LogTemp, Display, TEXT("Reviewed bundle hash: %s"), *Hash); }
    }));
static FAutoConsoleCommand PromoteCommand(TEXT("Hansa.Media.Promote"), TEXT("receipt destination stableId reviewer rightsStatement reviewHash APPROVE"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        FString Error;
        if (Args.Num() != 7 || !FHansaStagedMedia::Promote(Args[0], Args[1], Args[2], Args[3], Args[4], Args[5], Args[6] == TEXT("APPROVE"), Error))
        { UE_LOG(LogTemp, Error, TEXT("Media promotion failed: %s"), *Error); }
        else { UE_LOG(LogTemp, Display, TEXT("Approved media bundle committed.")); }
    }));
}
