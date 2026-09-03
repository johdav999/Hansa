#include "Fixtures/HansaProductionFixture.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION

namespace Hansa::Tests::GrainShortage
{
	using namespace Hansa::Simulation;

	struct FMetric final
	{
		int64 Tick = 0;
		FString StateHash;
		int64 Stock = 0;
		int64 Reserve = 0;
		int64 CitizenDemand = 0;
		int64 IndustrialDemand = 0;
		int64 UnmetDemand = 0;
		int64 Price = 0;
		int64 ReserveMilliDays = 0;
		int32 AlertCount = 0;
		bool bShortage = false;
	};

	FString Hex64(const uint64 Value)
	{
		return FString::Printf(TEXT("%016llX"), static_cast<unsigned long long>(Value));
	}

	FMetric Capture(const FHansaProductionFixture& Fixture)
	{
		FMetric Result;
		const FHansaSimulationReadOnlyAccess ReadOnly = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions());
		const FHansaCityDefinitionId CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
		const FHansaGoodId GoodId = FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
		const FHansaInventoryId InventoryId = FHansaInventoryId::TryCreate(1).Value;
		const TOptional<FHansaCityMarketProjection> Market = ReadOnly.QueryMarket(CityId, GoodId);
		const TOptional<FHansaInventoryStockProjection> Inventory = ReadOnly.GetInventories().QueryStock(InventoryId, GoodId);
		const TOptional<FHansaMarketReserveProjection> Reserve = ReadOnly.QueryMarketReserveDays(CityId, GoodId);
		check(Market.IsSet() && Inventory.IsSet() && Reserve.IsSet());
		Result.Tick = ReadOnly.GetClock().GetTick().GetValue();
		Result.StateHash = Hex64(Fixture.BuildStateHashes().GetOverallHash());
		Result.Stock = Inventory->Stock.GetRawValue();
		Result.Reserve = Market->DesiredReserve.GetRawValue();
		Result.CitizenDemand = Market->CitizenDemand.GetRawValue();
		Result.IndustrialDemand = Market->IndustrialDemand.GetRawValue();
		Result.UnmetDemand = Market->UnmetDemand.GetRawValue();
		Result.Price = Market->CurrentPriceMilliMarks;
		Result.ReserveMilliDays = Reserve->ReserveMilliDays;
		const TArray<FHansaMarketAlertProjection> Alerts = ReadOnly.QueryMarketAlerts(CityId, GoodId);
		Result.AlertCount = Alerts.Num();
		Result.bShortage = Alerts.ContainsByPredicate([](const FHansaMarketAlertProjection& Alert)
		{
			return Alert.Type == EHansaMarketAlertType::Shortage;
		});
		return Result;
	}

	TSharedRef<FJsonObject> MetricJson(const FMetric& Metric)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("tick"), static_cast<double>(Metric.Tick));
		Json->SetStringField(TEXT("stateHash"), Metric.StateHash);
		Json->SetNumberField(TEXT("stockMilliUnits"), static_cast<double>(Metric.Stock));
		Json->SetNumberField(TEXT("desiredReserveMilliUnits"), static_cast<double>(Metric.Reserve));
		Json->SetNumberField(TEXT("citizenDemandMilliUnits"), static_cast<double>(Metric.CitizenDemand));
		Json->SetNumberField(TEXT("industrialDemandMilliUnits"), static_cast<double>(Metric.IndustrialDemand));
		Json->SetNumberField(TEXT("unmetDemandMilliUnits"), static_cast<double>(Metric.UnmetDemand));
		Json->SetNumberField(TEXT("priceMilliMarks"), static_cast<double>(Metric.Price));
		Json->SetNumberField(TEXT("reserveMilliDays"), static_cast<double>(Metric.ReserveMilliDays));
		Json->SetNumberField(TEXT("alertCount"), Metric.AlertCount);
		Json->SetBoolField(TEXT("shortage"), Metric.bShortage);
		return Json;
	}

	FString WriteEvidence(const FMetric& Baseline, const FMetric& Shortage, const FMetric& Recovery,
		const FHansaProductionFixture& Fixture)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("evidenceSchemaVersion"), 1);
		Root->SetStringField(TEXT("fixtureId"), Fixture.GetFixtureId());
		Root->SetNumberField(TEXT("fixtureVersion"), Fixture.GetFixtureVersion());
		Root->SetStringField(TEXT("registryHash"), Hex64(Fixture.GetRegistryHash()));
		Root->SetObjectField(TEXT("baseline"), MetricJson(Baseline));
		Root->SetObjectField(TEXT("shortage"), MetricJson(Shortage));
		Root->SetObjectField(TEXT("recovery"), MetricJson(Recovery));
		TArray<TSharedPtr<FJsonValue>> Events;
		for (const FHansaDomainEvent& Event : Fixture.GetEvents())
		{
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("sequence"), FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Event.GetGlobalSequence())));
			Item->SetNumberField(TEXT("tick"), static_cast<double>(Event.GetTick().GetValue()));
			Item->SetStringField(TEXT("type"), LexToString(Event.GetType()));
			Item->SetNumberField(TEXT("productionId"), static_cast<double>(Event.GetProductionId().GetValue()));
			Item->SetStringField(TEXT("recipeId"), Event.GetRecipeId().ToString());
			Item->SetStringField(TEXT("blocker"), LexToString(Event.GetProductionBlocker()));
			Item->SetNumberField(TEXT("value"), static_cast<double>(Event.GetValue()));
			Events.Add(MakeShared<FJsonValueObject>(Item));
		}
		Root->SetArrayField(TEXT("events"), MoveTemp(Events));
		FString Json;
		FJsonSerializer::Serialize(Root, TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json));
		return Json + TEXT("\n");
	}

	void CompareMetric(FAutomationTestBase& Test, const TCHAR* Phase,
		const TSharedPtr<FJsonObject>& Golden, const FMetric& Actual)
	{
		Test.TestNotNull(FString::Printf(TEXT("%s golden phase exists"), Phase), Golden.Get());
		if (!Golden.IsValid()) return;
		Test.TestEqual(FString::Printf(TEXT("%s tick"), Phase), static_cast<int64>(Golden->GetNumberField(TEXT("tick"))), Actual.Tick);
		Test.TestEqual(FString::Printf(TEXT("%s state hash"), Phase), Golden->GetStringField(TEXT("stateHash")), Actual.StateHash);
		Test.TestEqual(FString::Printf(TEXT("%s stock"), Phase), static_cast<int64>(Golden->GetNumberField(TEXT("stockMilliUnits"))), Actual.Stock);
		Test.TestEqual(FString::Printf(TEXT("%s reserve"), Phase), static_cast<int64>(Golden->GetNumberField(TEXT("desiredReserveMilliUnits"))), Actual.Reserve);
		Test.TestEqual(FString::Printf(TEXT("%s citizen demand"), Phase), static_cast<int64>(Golden->GetNumberField(TEXT("citizenDemandMilliUnits"))), Actual.CitizenDemand);
		Test.TestEqual(FString::Printf(TEXT("%s industrial demand"), Phase), static_cast<int64>(Golden->GetNumberField(TEXT("industrialDemandMilliUnits"))), Actual.IndustrialDemand);
		Test.TestEqual(FString::Printf(TEXT("%s unmet demand"), Phase), static_cast<int64>(Golden->GetNumberField(TEXT("unmetDemandMilliUnits"))), Actual.UnmetDemand);
		Test.TestEqual(FString::Printf(TEXT("%s price"), Phase), static_cast<int64>(Golden->GetNumberField(TEXT("priceMilliMarks"))), Actual.Price);
		Test.TestEqual(FString::Printf(TEXT("%s reserve days"), Phase), static_cast<int64>(Golden->GetNumberField(TEXT("reserveMilliDays"))), Actual.ReserveMilliDays);
		Test.TestEqual(FString::Printf(TEXT("%s alerts"), Phase), static_cast<int32>(Golden->GetNumberField(TEXT("alertCount"))), Actual.AlertCount);
		Test.TestEqual(FString::Printf(TEXT("%s shortage"), Phase), Golden->GetBoolField(TEXT("shortage")), Actual.bShortage);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaGrainShortageFixtureTest,
	"Hansa.Integration.Fixtures.LubeckGrainShortageV1",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaGrainShortageFixtureTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::GrainShortage;
	const auto CreatedA = FHansaProductionFixture::TryCreateGrainShortage();
	const auto CreatedB = FHansaProductionFixture::TryCreateGrainShortage();
	TestTrue(TEXT("Fixture A initializes"), CreatedA.IsSuccess());
	TestTrue(TEXT("Fixture B initializes"), CreatedB.IsSuccess());
	if (!CreatedA || !CreatedB) return false;
	FHansaProductionFixture Fixture = CreatedA.Value;
	FHansaProductionFixture Repeat = CreatedB.Value;
	const FMetric Baseline = Capture(Fixture);
	TestEqual(TEXT("Stable fixture ID"), Fixture.GetFixtureId(), FString(FHansaProductionFixture::GrainShortageFixtureId));
	TestEqual(TEXT("Repeated baseline hash"), Repeat.BuildStateHashes().GetOverallHash(), Fixture.BuildStateHashes().GetOverallHash());

	TestTrue(TEXT("Shortage onset steps through gateway"), Fixture.Step(5).IsSuccess());
	TestTrue(TEXT("Repeated shortage onset steps"), Repeat.Step(5).IsSuccess());
	const FMetric Shortage = Capture(Fixture);
	TestTrue(TEXT("Shortage alert is active"), Shortage.bShortage);
	TestTrue(TEXT("Population contributes citizen demand"), Shortage.CitizenDemand > 0);
	TestTrue(TEXT("Industry contributes demand"), Shortage.IndustrialDemand > 0);
	TestTrue(TEXT("Shortage has unmet demand"), Shortage.UnmetDemand > 0);
	TestTrue(TEXT("Shortage raises grain price"), Shortage.Price > Baseline.Price);

	const FHansaProductionId RecoveryProduction = FHansaProductionId::TryCreate(10).Value;
	TestTrue(TEXT("Recovery production activates through command gateway"), Fixture.SetProductionActive(RecoveryProduction, true).IsSuccess());
	TestTrue(TEXT("Recovery production deactivates through command gateway"), Fixture.SetProductionActive(RecoveryProduction, false).IsSuccess());
	TestTrue(TEXT("Recovery reaches next market cadence"), Fixture.Step(3).IsSuccess());
	TestTrue(TEXT("Repeated recovery activates"), Repeat.SetProductionActive(RecoveryProduction, true).IsSuccess());
	TestTrue(TEXT("Repeated recovery deactivates"), Repeat.SetProductionActive(RecoveryProduction, false).IsSuccess());
	TestTrue(TEXT("Repeated recovery reaches cadence"), Repeat.Step(3).IsSuccess());
	const FMetric Recovery = Capture(Fixture);
	TestFalse(TEXT("Shortage alert clears"), Recovery.bShortage);
	TestTrue(TEXT("Reserve is restored"), Recovery.Stock >= Recovery.Reserve);
	TestTrue(TEXT("Price falls after recovery"), Recovery.Price < Shortage.Price);
	TestEqual(TEXT("Repeated final hash"), Repeat.BuildStateHashes().GetOverallHash(), Fixture.BuildStateHashes().GetOverallHash());
	TestEqual(TEXT("Exactly two controlled commands processed"),
		Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions()).GetProcessedCommandCount(), static_cast<uint64>(2));
	int32 ActivationEventCount = 0;
	for (const FHansaDomainEvent& Event : Fixture.GetEvents())
	{
		if (Event.GetType() == EHansaDomainEventType::ProductionActiveChanged) ++ActivationEventCount;
	}
	TestEqual(TEXT("Two production activation events recorded"), ActivationEventCount, 2);

	const FString Evidence = WriteEvidence(Baseline, Shortage, Recovery, Fixture);
	const FString EvidenceDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestEvidence/Market"));
	IFileManager::Get().MakeDirectory(*EvidenceDirectory, true);
	TestTrue(TEXT("Full structured evidence bundle written"), FFileHelper::SaveStringToFile(Evidence,
		*FPaths::Combine(EvidenceDirectory, TEXT("lubeck_grain_shortage_v1.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	FString GoldenText;
	const FString GoldenPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests/Golden/lubeck_grain_shortage_v1.json"));
	TestTrue(TEXT("Checked-in golden evidence exists"), FFileHelper::LoadFileToString(GoldenText, *GoldenPath));
	TSharedPtr<FJsonObject> Golden;
	TestTrue(TEXT("Golden evidence parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(GoldenText), Golden) && Golden.IsValid());
	if (Golden.IsValid())
	{
		TestEqual(TEXT("Golden fixture ID"), Golden->GetStringField(TEXT("fixtureId")), Fixture.GetFixtureId());
		TestEqual(TEXT("Golden fixture version"), static_cast<uint32>(Golden->GetNumberField(TEXT("fixtureVersion"))), Fixture.GetFixtureVersion());
		TestEqual(TEXT("Golden registry hash"), Golden->GetStringField(TEXT("registryHash")), Hex64(Fixture.GetRegistryHash()));
		CompareMetric(*this, TEXT("baseline"), Golden->GetObjectField(TEXT("baseline")), Baseline);
		CompareMetric(*this, TEXT("shortage"), Golden->GetObjectField(TEXT("shortage")), Shortage);
		CompareMetric(*this, TEXT("recovery"), Golden->GetObjectField(TEXT("recovery")), Recovery);
	}
	return true;
}

#endif
