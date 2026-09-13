#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "World/HansaLubeckPlacementGrid.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaWorldStyleAnchorContractTest,
	"Hansa.Content.WorldStyleAnchors.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaWorldStyleAnchorContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString ManifestPath = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Tests"), TEXT("Golden"), TEXT("enhanced_mvp_world_style_anchors_v1.json"));
	FString ManifestText;
	if (!TestTrue(TEXT("P07 world-style contract is checked in"), FFileHelper::LoadFileToString(ManifestText, *ManifestPath)))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if (!TestTrue(TEXT("P07 world-style contract is valid JSON"),
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ManifestText), Root) && Root.IsValid()))
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Scale = Root->GetObjectField(TEXT("worldScale"));
	TestEqual(TEXT("Contract grid matches runtime grid"), Scale->GetNumberField(TEXT("gridCellCm")),
		static_cast<double>(Hansa::Game::LubeckPlacementGrid::CellSize));
	TestFalse(TEXT("Runtime auto-fit is explicitly forbidden"), Scale->GetBoolField(TEXT("runtimeAutoFitAllowed")));
	const TArray<TSharedPtr<FJsonValue>>& RuntimeScale = Scale->GetArrayField(TEXT("authoredRuntimeScale"));
	TestEqual(TEXT("Authored scale contract has three axes"), RuntimeScale.Num(), 3);
	for (const TSharedPtr<FJsonValue>& Axis : RuntimeScale)
	{
		TestEqual(TEXT("Every authored presentation axis is identity"), Axis->AsNumber(), 1.0);
	}

	const TArray<TSharedPtr<FJsonValue>>& Candidates = Root->GetArrayField(TEXT("candidates"));
	const TSet<FString> RequiredFamilies = { TEXT("merchant-house"), TEXT("bakery"), TEXT("mill"), TEXT("road"), TEXT("harbor") };
	TSet<FString> ObservedFamilies;
	for (const TSharedPtr<FJsonValue>& CandidateValue : Candidates)
	{
		const TSharedPtr<FJsonObject> Candidate = CandidateValue->AsObject();
		const FString Family = Candidate->GetStringField(TEXT("family"));
		ObservedFamilies.Add(Family);
		const FString Disposition = Candidate->GetStringField(TEXT("disposition"));
		TestTrue(*FString::Printf(TEXT("%s has an auditable disposition"), *Family),
			Disposition == TEXT("production-ready") || Disposition == TEXT("requires-revision") || Disposition == TEXT("rejected"));
		const FString CanonicalPath = Candidate->GetStringField(TEXT("intendedCanonicalPath"));
		TestTrue(*FString::Printf(TEXT("%s has a project canonical target"), *Family), CanonicalPath.StartsWith(TEXT("/Game/")));
		TestFalse(*FString::Printf(TEXT("%s canonical target is not staging"), *Family),
			CanonicalPath.StartsWith(TEXT("/Game/Hansa/Generated/Staging/")));
		TestFalse(*FString::Printf(TEXT("%s canonical target is not an engine primitive"), *Family),
			CanonicalPath.StartsWith(TEXT("/Engine/BasicShapes/")));
		if (Disposition == TEXT("production-ready"))
		{
			const FString CurrentReference = Candidate->GetStringField(TEXT("currentReference"));
			TestFalse(*FString::Printf(TEXT("%s production reference is not staging"), *Family),
				CurrentReference.StartsWith(TEXT("/Game/Hansa/Generated/Staging/")));
			TestFalse(*FString::Printf(TEXT("%s production reference is not an engine primitive"), *Family),
				CurrentReference.StartsWith(TEXT("/Engine/BasicShapes/")));
			TestNotEqual(*FString::Printf(TEXT("%s production asset has editable source"), *Family),
				Candidate->GetStringField(TEXT("sourceBlend")), FString(TEXT("none")));
		}
	}
	TestEqual(TEXT("Contract covers every required anchor family"), ObservedFamilies.Num(), RequiredFamilies.Num());
	for (const FString& Family : RequiredFamilies)
	{
		TestTrue(*FString::Printf(TEXT("Contract covers %s"), *Family), ObservedFamilies.Contains(Family));
	}

	const FString ProjectionSourcePath = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Source"), TEXT("Hansa"), TEXT("Private"), TEXT("World"), TEXT("HansaBuildingWorldProjection.cpp"));
	FString ProjectionSource;
	TestTrue(TEXT("World projection source is available to the contract test"),
		FFileHelper::LoadFileToString(ProjectionSource, *ProjectionSourcePath));
	TestFalse(TEXT("Building projection no longer computes a runtime MeshScale fit"),
		ProjectionSource.Contains(TEXT("double MeshScale")));
	TestTrue(TEXT("Authored meshes are presented at identity scale"),
		ProjectionSource.Contains(TEXT("? FVector::OneVector")));
	TestTrue(TEXT("Authored actor presentations are presented at identity scale"),
		ProjectionSource.Contains(TEXT("BuildingPresentation->SetRelativeScale3D(FVector::OneVector)")));
	TestTrue(TEXT("Authored road previews are presented at identity scale"),
		ProjectionSource.Contains(TEXT("Piece->SetRelativeScale3D(FVector::OneVector)")));
	return !HasAnyErrors();
}

#endif
