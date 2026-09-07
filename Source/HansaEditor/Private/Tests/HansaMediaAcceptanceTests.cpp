#include "Generation/HansaStagedMedia.h"
#include "Generation/HansaStaticProp.h"
#include "Generation/HansaAudioTake.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundWave.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/MetaData.h"

using namespace Hansa::Editor::Generation;
namespace
{
bool ReadAcceptanceJson(const FString& File, TSharedPtr<FJsonObject>& Json)
{
    FString Text;
    return FFileHelper::LoadFileToString(Text, *File) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json);
}
bool WriteAcceptanceJson(const FString& File, const TSharedRef<FJsonObject>& Json)
{
    FString Text;
    return FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text)) &&
        FFileHelper::SaveStringToFile(Text, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMediaAcceptance, "Hansa.Integration.Authoring.MediaAcceptanceFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaMediaAcceptance::RunTest(const FString&)
{
    const FString Root = FPlatformMisc::GetEnvironmentVariable(TEXT("HANSA_MEDIA_ACCEPTANCE_ROOT"));
    const FString Phase = FPlatformMisc::GetEnvironmentVariable(TEXT("HANSA_MEDIA_ACCEPTANCE_PHASE"));
    if (Root.IsEmpty()) { AddInfo(TEXT("Run Scripts/RunMediaAcceptance.ps1 for the connected worker/editor proof.")); return true; }
    if (!TestTrue(TEXT("Known acceptance phase"), Phase == TEXT("promote") || Phase == TEXT("verify"))) return false;
    TSharedPtr<FJsonObject> Plan;
    if (!TestTrue(TEXT("Read worker plan"), ReadAcceptanceJson(Root / TEXT("media-plan.json"), Plan))) return false;
    if (!TestTrue(TEXT("Only original mocked fixtures may be auto-approved"), Plan->GetBoolField(TEXT("mockOnly")))) return false;
    const FString RunId = Plan->GetStringField(TEXT("runId"));
    FGuid Parsed;
    if (!TestTrue(TEXT("Fresh run identity"), RunId.Len() == 32 && FGuid::ParseExact(RunId, EGuidFormats::Digits, Parsed))) return false;
    const auto& Items = Plan->GetArrayField(TEXT("items"));
    if (!TestEqual(TEXT("One prop and two audio roles"), Items.Num(), 3)) return false;
    const bool bVerify = Phase == TEXT("verify");
    TSharedPtr<FJsonObject> Prior;
    if (bVerify)
    {
        if (!TestTrue(TEXT("Read prior Editor result"), ReadAcceptanceJson(Root / TEXT("editor-promote.json"), Prior))) return false;
        const FString Jobs = TEXT("Saved/GenerationJobs/MediaAcceptance_") + RunId;
        if (!TestFalse(TEXT("Transient worker state is actually deleted"), IFileManager::Get().DirectoryExists(*(FPaths::ProjectDir() / Jobs)))) return false;
    }
    TArray<TSharedPtr<FJsonValue>> Results;
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        const auto Item = Items[Index]->AsObject();
        const FString Kind = Item->GetStringField(TEXT("kind"));
        const bool bProp = Kind == TEXT("Prop");
        if (!TestTrue(TEXT("Expected ordered roles"), Kind == (Index == 0 ? TEXT("Prop") : Index == 1 ? TEXT("SFX") : TEXT("Speech")))) return false;
        const FString Domain = bProp ? TEXT("Meshes") : TEXT("Audio");
        const FString JobId = Item->GetStringField(TEXT("jobId"));
        if (!TestTrue(TEXT("Valid worker identity"), FGuid::Parse(JobId, Parsed))) return false;
        const FString Destination = TEXT("/Game/Hansa/") + Domain + TEXT("/MediaAcceptance_") + RunId + TEXT("_") + Kind;
        if (!TestEqual(TEXT("Isolated test destination"), Item->GetStringField(TEXT("destination")), Destination)) return false;
        const FString Descriptor = Item->GetStringField(TEXT("descriptorPath"));
        if (!TestTrue(TEXT("Bounded retained source"), Descriptor.StartsWith(TEXT("SourceArt/Generated/") + Domain + TEXT("/") + JobId + TEXT("/")) && !Descriptor.Contains(TEXT("..")))) return false;
        TSharedPtr<FJsonObject> Source, Manifest;
        if (!ReadAcceptanceJson(FPaths::ProjectDir() / Descriptor, Source) ||
            !ReadAcceptanceJson(FPaths::ProjectDir() / Source->GetStringField(TEXT("manifestPath")), Manifest)) return false;
        if (!TestTrue(TEXT("Manifest explicitly identifies mathematical mock"), Manifest->GetObjectField(TEXT("provenance"))->GetBoolField(TEXT("deterministicMock")))) return false;
        const FString StableId = Item->GetStringField(TEXT("stableId"));
        FString ReceiptPath, Error, Hash;
        if (!bVerify)
        {
            if (!FHansaStagedMedia::Stage(Descriptor, ReceiptPath, Error)) { AddError(Error); return false; }
            TSharedPtr<FJsonObject> Receipt;
            if (!ReadAcceptanceJson(FPaths::ProjectDir() / ReceiptPath, Receipt)) return false;
            FHansaStagedMedia::HashFile(FPaths::ProjectDir() / ReceiptPath, Hash);
            TestFalse(TEXT("Unapproved retained output cannot promote"), FHansaStagedMedia::Promote(ReceiptPath, Destination, StableId,
                TEXT("Automation"), TEXT("Original mock fixture"), Hash, false, Error));
            // Headless CI has no human reviewer/audio device. This explicit test-only
            // surrogate never calls a production bypass; manual demo uses Preview.
            TSharedRef<FJsonObject> Preview = MakeShared<FJsonObject>();
            Preview->SetStringField(TEXT("reviewHash"), Hash);
            Preview->SetBoolField(TEXT("testOnlyReviewSurrogate"), true);
            if (bProp)
            {
                UStaticMesh* Mesh = nullptr;
                for (const auto& Entry : Receipt->GetArrayField(TEXT("assets")))
                    if (UStaticMesh* Candidate = LoadObject<UStaticMesh>(nullptr, *Entry->AsObject()->GetStringField(TEXT("objectPath")))) Mesh = Candidate;
                if (!TestNotNull(TEXT("Real worker GLB imports as mesh"), Mesh)) return false;
                UTexture2D* Capture = nullptr;
                const FString CaptureFile = FPaths::ProjectDir() / (ReceiptPath + TEXT(".preview.png"));
                if (!FHansaStaticProp::RenderPreview(Mesh, CaptureFile, Capture, Error)) { AddError(Error); return false; }
                FString CaptureHash; FHansaStagedMedia::HashFile(CaptureFile, CaptureHash);
                Preview->SetStringField(TEXT("captureSha256"), CaptureHash);
                Preview->SetStringField(TEXT("scene"), TEXT("HarborProp-v1-1280x720-automation"));
                TestEqual(TEXT("Native capture width"), Capture->Source.GetSizeX(), int64(1280));
                TestEqual(TEXT("Native capture height"), Capture->Source.GetSizeY(), int64(720));
                IFileManager::Get().Copy(*(Root / TEXT("harbor-preview.png")), *CaptureFile);
            }
            else Preview->SetBoolField(TEXT("audioPlaybackStarted"), true);
            if (!WriteAcceptanceJson(FPaths::ProjectDir() / (ReceiptPath + TEXT(".preview.json")), Preview)) return false;
            if (Index == 0)
            {
                FHansaStagedMedia::bFailBeforeCommitForTests = true;
                const bool UnexpectedCommit = FHansaStagedMedia::Promote(ReceiptPath, Destination, StableId,
                    TEXT("Automation"), TEXT("Original mock fixture; test-only approval"), Hash, true, Error);
                FHansaStagedMedia::bFailBeforeCommitForTests = false;
                TestFalse(TEXT("Injected commit failure"), UnexpectedCommit);
                TestFalse(TEXT("No partial production bundle"), IFileManager::Get().DirectoryExists(*FPackageName::LongPackageNameToFilename(Destination)));
            }
            if (!FHansaStagedMedia::Promote(ReceiptPath, Destination, StableId, TEXT("Automation"),
                TEXT("Original mock fixture; test-only approval"), Hash, true, Error)) { AddError(Error); return false; }
        }
        else ReceiptPath = Prior->GetArrayField(TEXT("items"))[Index]->AsObject()->GetStringField(TEXT("receiptPath"));
        if (!FHansaStagedMedia::VerifyPromotion(ReceiptPath, Error)) { AddError(Error); return false; }
        TSharedPtr<FJsonObject> Decision;
        if (!ReadAcceptanceJson(FPaths::ProjectDir() / (ReceiptPath + TEXT(".promotion.json")), Decision)) return false;
        TArray<UObject*> FinalAssets;
        for (const auto& Entry : Decision->GetArrayField(TEXT("assets")))
        {
            const FString ObjectPath = Entry->AsObject()->GetStringField(TEXT("objectPath"));
            if (!TestTrue(TEXT("Final references use only the approved destination"), ObjectPath.StartsWith(Destination + TEXT("/")))) return false;
            UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
            if (!TestNotNull(TEXT("Final package loads after restart"), Asset)) return false;
            FinalAssets.Add(Asset);
            TestEqual(TEXT("Stable media identity survives save/reload"), FString(Asset->GetPackage()->GetMetaData().GetValue(Asset, TEXT("Hansa.StableMediaId"))), StableId);
        }
        const auto Parameters = Manifest->GetObjectField(TEXT("parameters"));
        if (bProp ? !FHansaStaticProp::Validate(FinalAssets, Parameters->GetObjectField(TEXT("staticProp")), Error)
            : !FHansaAudioTake::Validate(FinalAssets, Parameters->GetObjectField(TEXT("audioTake")), Error)) { AddError(Error); return false; }
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("kind"), Kind); Result->SetStringField(TEXT("receiptPath"), ReceiptPath);
        Result->SetStringField(TEXT("stableId"), StableId); Result->SetObjectField(TEXT("decision"), Decision.ToSharedRef());
        Results.Add(MakeShared<FJsonValueObject>(Result));
    }
    TSharedRef<FJsonObject> Evidence = MakeShared<FJsonObject>();
    Evidence->SetStringField(TEXT("status"), HasAnyErrors() ? TEXT("Failed") : TEXT("Succeeded"));
    Evidence->SetBoolField(TEXT("mockOnly"), true); Evidence->SetBoolField(TEXT("humanListeningProven"), false);
    Evidence->SetBoolField(TEXT("freshEditorWithoutWorkerState"), bVerify);
    Evidence->SetArrayField(TEXT("items"), Results);
    return WriteAcceptanceJson(Root / (TEXT("editor-") + Phase + TEXT(".json")), Evidence) && !HasAnyErrors();
}
