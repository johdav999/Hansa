#include "Screenshot/HansaNativeScreenshotService.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace Hansa::Automation
{
	namespace
	{
		bool IsSafeBundleId(const FString& Value)
		{
			if (Value.IsEmpty() || Value.Len() > 64)
			{
				return false;
			}
			for (const TCHAR Character : Value)
			{
				if (!(FChar::IsAlnum(Character) || Character == TEXT('-') || Character == TEXT('_')))
				{
					return false;
				}
			}
			return true;
		}

		FString SerializeJson(const TSharedRef<FJsonObject>& Object)
		{
			FString Result;
			const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
			FJsonSerializer::Serialize(Object, Writer);
			return Result;
		}
	}

	bool FHansaNativeScreenshotService::IsSupportedSize(const FIntPoint& Size)
	{
		return Size == FIntPoint(1280, 720) || Size == FIntPoint(1920, 1080);
	}

	FHansaScreenshotResult FHansaNativeScreenshotService::Capture(
		const FIntPoint& RequestedSize,
		const FHansaScreenshotContext& Context,
		FNativeCapture NativeCapture) const
	{
		FHansaScreenshotResult Result;
		Result.Size = RequestedSize;
		if (!IsSupportedSize(RequestedSize))
		{
			Result.Error = EHansaScreenshotError::InvalidSize;
			return Result;
		}
		if (!IsSafeBundleId(Context.BundleId))
		{
			Result.Error = EHansaScreenshotError::InvalidBundleId;
			return Result;
		}
		if (!IsSafeBundleId(Context.EvidenceSuiteId))
		{
			Result.Error = EHansaScreenshotError::InvalidBundleId;
			return Result;
		}
		if (!NativeCapture)
		{
			Result.Error = EHansaScreenshotError::CaptureUnavailable;
			return Result;
		}

		TArray<FColor> Pixels;
		if (!NativeCapture(RequestedSize, Pixels))
		{
			Result.Error = EHansaScreenshotError::CaptureUnavailable;
			return Result;
		}
		const int64 ExpectedPixels = static_cast<int64>(RequestedSize.X) * RequestedSize.Y;
		if (Pixels.Num() != ExpectedPixels)
		{
			Result.Error = EHansaScreenshotError::UnexpectedPixelCount;
			return Result;
		}

		const FString BundleRoot = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("TestEvidence"), TEXT("Automation"), Context.EvidenceSuiteId, Context.BundleId);
		if (!IFileManager::Get().MakeDirectory(*BundleRoot, true))
		{
			Result.Error = EHansaScreenshotError::EvidenceWriteFailed;
			return Result;
		}
		Result.ScreenshotPath = FPaths::Combine(
			BundleRoot,
			FString::Printf(TEXT("screenshot-%dx%d.png"), RequestedSize.X, RequestedSize.Y));
		Result.MetadataPath = FPaths::Combine(BundleRoot, TEXT("metadata.json"));
		Result.SemanticSnapshotPath = FPaths::Combine(BundleRoot, TEXT("semantic-ui.json"));

		TArray64<uint8> PngBytes;
		FImageUtils::PNGCompressImageArray(
			RequestedSize.X,
			RequestedSize.Y,
			TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()),
			PngBytes);
		if (PngBytes.IsEmpty() || !FFileHelper::SaveArrayToFile(PngBytes, *Result.ScreenshotPath))
		{
			Result.Error = EHansaScreenshotError::EvidenceWriteFailed;
			return Result;
		}

		FSHAHash Hash;
		FSHA1::HashBuffer(PngBytes.GetData(), static_cast<uint32>(PngBytes.Num()), Hash.Hash);
		Result.ContentSha1 = Hash.ToString();

		TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
		Metadata->SetNumberField(TEXT("schemaVersion"), 1);
		Metadata->SetStringField(TEXT("capturedAtUtc"), FDateTime::UtcNow().ToIso8601());
		Metadata->SetStringField(TEXT("captureMethod"), Context.CaptureMethod);
		Metadata->SetBoolField(TEXT("postCaptureResized"), false);
		Metadata->SetStringField(TEXT("fixtureId"), Context.FixtureId);
		Metadata->SetStringField(TEXT("map"), Context.MapName);
		Metadata->SetStringField(TEXT("screenId"), Context.ScreenId);
		Metadata->SetNumberField(TEXT("width"), RequestedSize.X);
		Metadata->SetNumberField(TEXT("height"), RequestedSize.Y);
		Metadata->SetNumberField(TEXT("uiScale"), Context.UiScale);
		Metadata->SetNumberField(TEXT("uiRevision"), static_cast<double>(Context.UiRevision));
		Metadata->SetNumberField(TEXT("simulationTick"), static_cast<double>(Context.SimulationTick));
		Metadata->SetNumberField(TEXT("frame"), static_cast<double>(Context.FrameNumber));
		Metadata->SetStringField(TEXT("contentSha1"), Result.ContentSha1);
		Metadata->SetStringField(TEXT("screenshot"), FPaths::GetCleanFilename(Result.ScreenshotPath));
		Metadata->SetStringField(TEXT("semanticSnapshot"), FPaths::GetCleanFilename(Result.SemanticSnapshotPath));
		Metadata->SetStringField(TEXT("flowId"), Context.FlowId);
		TArray<TSharedPtr<FJsonValue>> Assertions;
		for (const FString& Assertion : Context.StructuralAssertions)
		{
			Assertions.Add(MakeShared<FJsonValueString>(Assertion));
		}
		Metadata->SetNumberField(TEXT("structuralAssertionCount"), Assertions.Num());
		Metadata->SetBoolField(TEXT("structuralAssertionsPassed"), Context.bStructuralAssertionsPassed);
		Metadata->SetArrayField(TEXT("structuralAssertions"), MoveTemp(Assertions));

		if (!FFileHelper::SaveStringToFile(SerializeJson(Metadata), *Result.MetadataPath) ||
			!FFileHelper::SaveStringToFile(Context.SemanticSnapshotJson, *Result.SemanticSnapshotPath))
		{
			Result.Error = EHansaScreenshotError::EvidenceWriteFailed;
		}
		return Result;
	}
}
