#include "Terrain/HansaCityTerrainToolset.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GeoReferencingSystem.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "LandscapeSubsystem.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	constexpr int32 Side = 2017;
	const TCHAR* DraftMap = TEXT("/Game/Hansa/Generated/Staging/RostockTerrain_P31_20260908/L_Rostock_Survey_WP");
	const TCHAR* SourceRoot = TEXT("SourceArt/Terrain/Rostock/Survey_20260908/NativeEncoding_v2/");
	const TCHAR* SourceName = TEXT("rostock-survey--2017x2017--2m.r16");
	const TCHAR* HeightHash = TEXT("879a56a00c635f26e783bc0158db45dbfb06caa35e694752d24f14ef6d1d99b6");
	const TCHAR* ManifestHash = TEXT("28ef2f8f3b1a9055adccce77f3a0ca7e73e6ebb154f809a590139d41b1876539");

	FString Json(const TSharedRef<FJsonObject>& Object)
	{
		FString Text;
		FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text));
		return Text;
	}

	FString Failure(const FString& Reason)
	{
		const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("ok"), false);
		Result->SetStringField(TEXT("error"), Reason);
		Result->SetBoolField(TEXT("productionAccepted"), false);
		return Json(Result);
	}

	bool Sha256(const TArray<uint8>& Bytes, FString& Hex)
	{
#if PLATFORM_WINDOWS
		BCRYPT_ALG_HANDLE Algorithm = nullptr;
		if (BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
		uint8 Digest[32] = {};
		const NTSTATUS Status = BCryptHash(Algorithm, nullptr, 0, const_cast<PUCHAR>(Bytes.GetData()),
			static_cast<ULONG>(Bytes.Num()), Digest, UE_ARRAY_COUNT(Digest));
		BCryptCloseAlgorithmProvider(Algorithm, 0);
		if (Status < 0) return false;
		Hex = BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
		return true;
#else
		return false;
#endif
	}

	TArray<UPackage*> DirtyPackages()
	{
		TArray<UPackage*> Maps, Content;
		UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Maps);
		UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);
		for (UPackage* Package : Content) Maps.AddUnique(Package);
		return Maps;
	}

	bool ReadPinnedSource(TArray<uint8>& Heights)
	{
		const FString Root = FPaths::ProjectDir() / SourceRoot;
		TArray<uint8> Manifest;
		FString Digest;
		// Check sizes before allocation. The manifest is deliberately pinned: new surveys need explicit review.
		if (IFileManager::Get().FileSize(*(Root / TEXT("terrain-manifest.json"))) > 65536 ||
			IFileManager::Get().FileSize(*(Root / SourceName)) != Side * Side * 2) return false;
		return FFileHelper::LoadFileToArray(Manifest, *(Root / TEXT("terrain-manifest.json"))) &&
			Sha256(Manifest, Digest) && Digest == ManifestHash &&
			FFileHelper::LoadFileToArray(Heights, *(Root / SourceName)) &&
			Sha256(Heights, Digest) && Digest == HeightHash;
	}
}

FString UHansaCityTerrainToolset::InspectAuthoringContext()
{
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("projectFile"), FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
	Result->SetBoolField(TEXT("pieRunning"), GEditor && GEditor->PlayWorld);
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	Result->SetStringField(TEXT("map"), World ? World->GetOutermost()->GetName() : TEXT(""));
	TArray<TSharedPtr<FJsonValue>> Dirty;
	for (const UPackage* Package : DirtyPackages()) Dirty.Add(MakeShared<FJsonValueString>(Package->GetName()));
	Result->SetArrayField(TEXT("dirtyPackages"), Dirty);
	return Json(Result);
}

FString UHansaCityTerrainToolset::ImportRostockSurveyDraft()
{
	if (!GEditor || GEditor->PlayWorld || FString(FApp::GetProjectName()) != TEXT("Hansa"))
		return Failure(TEXT("Requires the Hansa editor outside PIE"));
	if (!DirtyPackages().IsEmpty()) return Failure(TEXT("Save user work first; dirty packages will never be discarded or saved implicitly"));
	if (FPackageName::DoesPackageExist(DraftMap) || FindPackage(nullptr, DraftMap))
		return Failure(TEXT("Destination already exists; inspect it rather than overwriting/retrying"));
	TArray<uint8> Bytes;
	if (!ReadPinnedSource(Bytes)) return Failure(TEXT("Pinned Rostock source preflight failed; no map changed"));
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(Side * Side);
	for (int32 Index = 0; Index < Heights.Num(); ++Index)
		Heights[Index] = static_cast<uint16>(Bytes[Index * 2]) | (static_cast<uint16>(Bytes[Index * 2 + 1]) << 8);

	UWorld* World = GEditor->NewMap(true);
	if (!World || !World->GetWorldPartition()) return Failure(TEXT("Could not create World Partition draft"));
	ALandscape* Landscape = World->SpawnActor<ALandscape>(FVector(-201550, -201650, 3200), FRotator::ZeroRotator);
	if (!Landscape) return Failure(TEXT("Could not create native Landscape"));
	Landscape->SetActorLabel(TEXT("Terrain.Rostock.Survey_Base"));
	Landscape->SetActorRelativeScale3D(FVector(200, 200, 25));
	Landscape->Tags.Add(TEXT("City.Rostock"));
	Landscape->Tags.Add(FName(*FString::Printf(TEXT("Survey.SHA256.%s"), HeightHash)));
	TMap<FGuid, TArray<uint16>> HeightLayers;
	HeightLayers.Add(FGuid(), MoveTemp(Heights));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayers;
	MaterialLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
	const FString HeightPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / SourceRoot / SourceName);
	Landscape->Import(FGuid::NewGuid(), 0, 0, Side-1, Side-1, 2, 63, HeightLayers,
		*HeightPath, MaterialLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	ULandscapeEditLayerBase* Base = Landscape->GetEditLayer(0);
	if (!Base) return Failure(TEXT("Imported draft lacks measured edit layer; retained for diagnosis"));
	Base->SetName(TEXT("Survey_Base"), true);
	Base->SetLocked(true, true);
	Landscape->CreateLayer(TEXT("Historical_Hydrology"));
	Landscape->CreateLayer(TEXT("Historical_Corrections"));
	Landscape->CreateLayer(TEXT("Gameplay_Grading"));
	// Native Water brush layer is created by the later water authoring pass, not a falsely labeled standard layer.
	World->GetSubsystem<ULandscapeSubsystem>()->ChangeGridSize(Landscape->GetLandscapeInfo(), 4);
	AGeoReferencingSystem* Geo = World->SpawnActor<AGeoReferencingSystem>();
	if (!Geo) return Failure(TEXT("Failed to create GeoReferencingSystem"));
	Geo->SetActorLabel(TEXT("GeoReference_Rostock_EPSG25833"));
	Geo->PlanetShape = EPlanetShape::FlatPlanet;
	Geo->ProjectedCRS = TEXT("EPSG:25833");
	Geo->GeographicCRS = TEXT("EPSG:4258");
	Geo->bOriginLocationInProjectedCRS = true;
	Geo->OriginProjectedCoordinatesEasting = 312958;
	Geo->OriginProjectedCoordinatesNorthing = 5997318;
	Geo->OriginProjectedCoordinatesUp = 0;
	Geo->ApplySettings();
	if (!UEditorLoadingAndSavingUtils::SaveMap(World, DraftMap))
		return Failure(TEXT("Draft save failed; do not retry blindly"));
	return InspectRostockSurveyDraft();
}

FString UHansaCityTerrainToolset::InspectRostockSurveyDraft()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || World->GetOutermost()->GetName() != DraftMap)
		return Failure(TEXT("Open the exact staged Rostock survey map first"));
	ALandscape* Landscape = nullptr;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		if (Landscape) return Failure(TEXT("Ambiguous Landscape count"));
		Landscape = *It;
	}
	if (!Landscape || !Landscape->GetLandscapeInfo()) return Failure(TEXT("No loaded native Landscape"));
	const ULandscapeEditLayerBase* Base = Landscape->GetEditLayerConst(FName(TEXT("Survey_Base")));
	if (!Base || !Base->IsLocked()) return Failure(TEXT("Measured base layer missing or unlocked"));
	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (Info->XYtoComponentMap.Num() != 256)
		return Failure(TEXT("Expected all 256 Landscape components loaded; load editor regions before validation"));
	TArray<uint16> Heights;
	Heights.SetNumZeroed(Side * Side);
	FLandscapeEditDataInterface Edit(Info, Base->GetGuid(), false);
	Edit.SetShouldDirtyPackage(false);
	Edit.GetHeightDataFast(0, 0, Side-1, Side-1, Heights.GetData(), Side);
	TArray<uint8> Bytes;
	Bytes.SetNumUninitialized(Heights.Num()*2);
	for (int32 Index = 0; Index < Heights.Num(); ++Index)
	{
		Bytes[Index*2] = Heights[Index] & 255;
		Bytes[Index*2+1] = Heights[Index] >> 8;
	}
	FString Digest;
	if (!Sha256(Bytes, Digest) || Digest != HeightHash) return Failure(TEXT("Native measured-layer readback differs from pinned source"));
	AGeoReferencingSystem* Geo = AGeoReferencingSystem::GetGeoReferencingSystem(World);
	FVector Projected;
	if (!Geo) return Failure(TEXT("Missing georeference"));
	Geo->EngineToProjected(FVector::ZeroVector, Projected);
	if (Geo->PlanetShape != EPlanetShape::FlatPlanet || Geo->ProjectedCRS != TEXT("EPSG:25833") ||
		Geo->GeographicCRS != TEXT("EPSG:4258") || !Geo->bOriginLocationInProjectedCRS ||
		!Projected.Equals(FVector(312958,5997318,0), .001) ||
		!Landscape->GetActorLocation().Equals(FVector(-201550,-201650,3200), .001) ||
		!Landscape->GetActorRotation().IsNearlyZero(.00001) ||
		!Landscape->GetActorScale3D().Equals(FVector(200,200,25), .00001))
		return Failure(TEXT("Georeference / Landscape transform mismatch"));
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("ok"), true);
	Result->SetBoolField(TEXT("productionAccepted"), false);
	Result->SetStringField(TEXT("map"), DraftMap);
	Result->SetStringField(TEXT("measuredLayerSha256"), Digest);
	Result->SetNumberField(TEXT("componentsLoaded"), Info->XYtoComponentMap.Num());
	Result->SetBoolField(TEXT("worldPartition"), World->GetWorldPartition() != nullptr);
	Result->SetStringField(TEXT("status"), TEXT("Native survey draft only; materials, water, historical reconstruction, reopen, collision, streaming and Shipping gates remain"));
	return Json(Result);
}
