#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"

#include "Definitions/HansaArtisanProductionDraft.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaRegionalProductionDraft.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "HAL/PlatformTime.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Save/HansaSaveEnvelope.h"
#include "Systems/HansaSimulationPipeline.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UObject/StrongObjectPtr.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaLubeckScenarioInitializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRegionalProductionCatalogTest,
	"Hansa.Integration.Authoring.RegionalProduction.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRegionalProductionCatalogTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor;
	FString Error;
	auto Definitions=EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	TestTrue(TEXT("Apply accepted artisan content"),ArtisanProduction::ApplyDraft(Definitions,Error));
	TestTrue(TEXT("Apply regional production content"),RegionalProduction::ApplyDraft(Definitions,Error));
	if(!Error.IsEmpty()) AddError(Error);
	TArray<const UHansaDefinitionBase*> Raw;for(const auto& D:Definitions)Raw.Add(D.Get());
	const auto Compiled=FHansaEconomicDefinitionCompiler::Compile(Raw);
	for(const auto& Issue:Compiled.Issues)if(Issue.Severity==EHansaDefinitionValidationSeverity::Error)AddError(Issue.Code.ToString()+TEXT(": ")+Issue.Cause.ToString());
	TestTrue(TEXT("Regional catalog compiles"),Compiled.IsValid());
	TestEqual(TEXT("Five regions"),Compiled.Registry.GetRegions().Num(),5);
	TestEqual(TEXT("Ten reusable production chains"),Compiled.Registry.GetProductionChains().Num(),10);
	TestEqual(TEXT("Thirty city markets"),Compiled.Registry.GetCityMarkets().Num(),30);
	TestEqual(TEXT("Reviewed multi-city presence subset extends the four-city core"),Compiled.Registry.GetCityTradePolicies().Num(),8);
	for(const auto& Region:Compiled.Registry.GetRegions()) TestEqual(*FString::Printf(TEXT("%s has six cities"),*Region.StableId),Region.MemberCityIds.Num(),6);
	for(const auto& City:Compiled.Registry.GetCityMarkets())
	{
		TestNotNull(*FString::Printf(TEXT("%s has a valid region"),*City.StableId),Compiled.Registry.FindRegion(City.RegionId));
		TestTrue(*FString::Printf(TEXT("%s has a bounded subset"),*City.StableId),City.IndustryBindings.Num()>=3&&City.IndustryBindings.Num()<=6);
		int32 Signatures=0;for(const auto& Binding:City.IndustryBindings)Signatures+=Binding.SignatureRank>0?1:0;
		TestTrue(*FString::Printf(TEXT("%s has at most two signatures"),*City.StableId),Signatures<=2);
		for(const auto& Good:City.Goods)TestEqual(*FString::Printf(TEXT("%s %s has no legacy production faucet"),*City.StableId,*Good.GoodId),Good.BackgroundProductionMilliUnitsPerUpdate,int64(0));
	}
	const auto* Rostock=Compiled.Registry.FindCityMarket(TEXT("City.Rostock"));
	TestNotNull(TEXT("Rostock exists"),Rostock);
	if(Rostock)TestTrue(TEXT("Rostock supplies hides, bark and tanning"),Rostock->IndustryBindings.ContainsByPredicate([](const auto& B){return B.ProductionChainId==TEXT("ProductionChain.Shoes")&&B.EnabledStageKeys.Contains(TEXT("SourceRawHides"))&&B.EnabledStageKeys.Contains(TEXT("StripTanningBark"))&&B.EnabledStageKeys.Contains(TEXT("TanLeather"));}));
	const auto* Lubeck=Compiled.Registry.FindCityMarket(TEXT("City.Lubeck"));
	const auto* Wismar=Compiled.Registry.FindCityMarket(TEXT("City.Wismar"));
	TestTrue(TEXT("Lubeck is truthfully classified as rendered and buildable"),Lubeck&&Lubeck->PresentationClass==static_cast<uint8>(EHansaCityPresentationClass::RenderedBuildable)&&!Lubeck->bMarketOnly);
	TestTrue(TEXT("Rostock is truthfully classified as rendered and visitable"),Rostock&&Rostock->PresentationClass==static_cast<uint8>(EHansaCityPresentationClass::RenderedVisitable)&&!Rostock->bMarketOnly);
	TestTrue(TEXT("Wismar is explicitly abstract market-only"),Wismar&&Wismar->PresentationClass==static_cast<uint8>(EHansaCityPresentationClass::MarketOnly)&&Wismar->bMarketOnly);
	for(const FString AbstractCity:{TEXT("City.Wismar"),TEXT("City.Stralsund"),TEXT("City.Danzig"),TEXT("City.Bergen")})
	{
		const auto* Policy=Compiled.Registry.FindCityTradePolicyForCity(AbstractCity);
		TestNotNull(*FString::Printf(TEXT("%s has reviewed visiting-contact policy"),*AbstractCity),Policy);
		if(Policy)
		{
			TestEqual(*FString::Printf(TEXT("%s begins at visiting contact"),*AbstractCity),Policy->InitialStageId,FString(TEXT("PresenceStage.VisitingContact")));
			TestEqual(*FString::Printf(TEXT("%s has no station site without a rendered location"),*AbstractCity),Policy->TradeStationSites.Num(),0);
			TestEqual(*FString::Printf(TEXT("%s has no unreviewed specialization"),*AbstractCity),Policy->Specializations.Num(),0);
			TestEqual(*FString::Printf(TEXT("%s has no unreviewed privilege"),*AbstractCity),Policy->Privileges.Num(),0);
			TestEqual(*FString::Printf(TEXT("%s has no unreviewed city project"),*AbstractCity),Policy->CityProjects.Num(),0);
		}
	}
	const auto* Baltic=Compiled.Registry.GetRoutes().FindByPredicate([](const auto& Route){return Route.StableId==TEXT("Route.BalticSea");});
	TestTrue(TEXT("Baltic route reaches the reviewed abstract markets"),Baltic&&Baltic->Connections.ContainsByPredicate([](const auto& C){return C.DestinationCityId==TEXT("City.Wismar");})&&Baltic->Connections.ContainsByPredicate([](const auto& C){return C.DestinationCityId==TEXT("City.Stralsund");})&&Baltic->Connections.ContainsByPredicate([](const auto& C){return C.DestinationCityId==TEXT("City.Danzig");})&&Baltic->Connections.ContainsByPredicate([](const auto& C){return C.DestinationCityId==TEXT("City.Bergen");}));
	Algo::Reverse(Raw);const auto Reversed=FHansaEconomicDefinitionCompiler::Compile(Raw);
	TestTrue(TEXT("Reversed regional discovery compiles"),Reversed.IsValid());
	TestEqual(TEXT("Registry hash is discovery-order independent"),Compiled.Registry.GetRegistryHash(),Reversed.Registry.GetRegistryHash());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRegionalProductionScaleProfileTest,
	"Hansa.Integration.Authoring.RegionalProduction.ScaleProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRegionalProductionScaleProfileTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor;
	using namespace Hansa::Simulation;
	(void)Parameters;
	FString Error;
	auto Owned=EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	if(!TestTrue(TEXT("Apply accepted artisan content"),ArtisanProduction::ApplyDraft(Owned,Error))||
		!TestTrue(TEXT("Apply regional production content"),RegionalProduction::ApplyDraft(Owned,Error))) {AddError(Error);return false;}
	TArray<const UHansaDefinitionBase*> Raw;for(const auto& D:Owned)Raw.Add(D.Get());
	auto Compiled=FHansaEconomicDefinitionCompiler::Compile(Raw);
	if(!TestTrue(TEXT("Scale catalog compiles"),Compiled.IsValid()))return false;
	const auto Placement=Hansa::Game::LubeckPlacementGrid::TryBuildInitialization(FHansaHouseId::TryCreate(1).Value,Compiled.Registry);
	if(!TestTrue(TEXT("Regional placement initializes"),Placement.IsSuccess()))return false;
	FHansaLubeckScenarioState Scenario;
	if(!TestTrue(TEXT("Thirty-city runtime fixture initializes"),FHansaLubeckScenarioInitializer::TryCreate(EHansaRuntimeScenario::LubeckGrainShortage,Compiled.Registry,Placement.Value,Scenario,Error))) {AddError(Error);return false;}

	const FHansaEconomicRegistry* Registry=Scenario.Definitions.GetEconomicRegistry();
	if(!TestNotNull(TEXT("Scenario retains regional registry"),Registry))return false;
	const auto Projection=Scenario.State.CreateReadOnlyAccess(Scenario.Definitions).BuildProjection();
	if(!TestTrue(TEXT("Regional projection builds"),Projection.IsSuccess()))return false;
	TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
	Model->InitializeDefaults();
	const double ProjectionStart=FPlatformTime::Seconds();
	TestTrue(TEXT("Trade map accepts regional projection"),Model->ApplyProjection(Projection.Value,*Registry));
	const double ProjectionMilliseconds=(FPlatformTime::Seconds()-ProjectionStart)*1000.0;
	TestEqual(TEXT("All thirty market cities are queryable"),Model->GetSnapshot().MatchingCityCount,30);
	TestTrue(TEXT("Map marker materialization is bounded"),Model->GetSnapshot().Cities.Num()<=48);
	TestTrue(TEXT("Route row materialization is bounded"),Model->GetSnapshot().Routes.Num()<=20);
	const int32 InitialMarkerCount=Model->GetSnapshot().Cities.Num();
	const int32 InitialRouteRowCount=Model->GetSnapshot().Routes.Num();
	const double FilterStart=FPlatformTime::Seconds();
	for(int32 Iteration=0;Iteration<200;++Iteration)
	{
		Model->SetCitySearchIntent((Iteration&1)==0?TEXT("Wismar"):TEXT(""));
		Model->CycleCityFilterIntent();
	}
	const double FilterMilliseconds=(FPlatformTime::Seconds()-FilterStart)*1000.0;
	Model->CycleCityFilterIntent(); // Two hundred cycles leave Routes selected; return to All.
	Model->SetCitySearchIntent(TEXT("Wismar"));
	TestEqual(TEXT("Search selects one reviewed abstract city"),Model->GetSnapshot().MatchingCityCount,1);
	TestTrue(TEXT("Wismar advertises market-only and non-visitable truthfully"),Model->GetSnapshot().Cities.Num()==1&&Model->GetSnapshot().Cities[0].bMarketOnly&&!Model->GetSnapshot().Cities[0].bVisitable&&!Model->GetSnapshot().Cities[0].bBuildable);

	FHansaSimulationTransientCache Cache;
	bool bObservedRegionalWork=false;
	const double TickStart=FPlatformTime::Seconds();
	for(int32 Index=0;Index<240;++Index)
	{
		if(!TestTrue(TEXT("Regional deterministic tick succeeds"),FHansaGameplayCommandGateway::ExecuteTick(Scenario.State,Scenario.Definitions,{},Cache).IsSuccess()))return false;
		const auto RuntimeProjection=Scenario.State.CreateReadOnlyAccess(Scenario.Definitions).BuildProjection();
		if(!TestTrue(TEXT("Tick projection builds"),RuntimeProjection.IsSuccess()))return false;
		bObservedRegionalWork|=!RuntimeProjection.Value.GetRemoteIndustries().IsEmpty()||!RuntimeProjection.Value.GetRegionalShipments().IsEmpty();
		for(const auto& Shipment:RuntimeProjection.Value.GetRegionalShipments())
			TestTrue(TEXT("Automatic regional exchange excludes Lubeck at both ends"),Shipment.SourceCityId.ToString()!=TEXT("City.Lubeck")&&Shipment.DestinationCityId.ToString()!=TEXT("City.Lubeck"));
	}
	const double TickMilliseconds=(FPlatformTime::Seconds()-TickStart)*1000.0;
	TestTrue(TEXT("Regional production or transfer work is observable"),bObservedRegionalWork);

	FHansaSaveSnapshot Snapshot;Snapshot.State=Scenario.State;Snapshot.BuildVersion=TEXT("TR-12-ScaleProfile");Snapshot.SavedUtc=TEXT("2026-09-21T00:00:00Z");Snapshot.Players.Add({1,Scenario.HouseId});
	TArray<uint8> Bytes;const double SaveStart=FPlatformTime::Seconds();const auto Saved=FHansaSaveEnvelope::Encode(Snapshot,Scenario.Definitions,Bytes);const double SaveMilliseconds=(FPlatformTime::Seconds()-SaveStart)*1000.0;
	TestTrue(*Saved.Message,Saved.IsSuccess());
	TestTrue(TEXT("Early multi-city save remains bounded below 8 MiB"),Bytes.Num()<8*1024*1024);
	AddInfo(FString::Printf(TEXT("TR-12 profile: 240 ticks %.2f ms; projection %.2f ms; 200 search/filter updates %.2f ms; save %.2f ms / %d bytes; initial markers %d; initial route rows %d."),TickMilliseconds,ProjectionMilliseconds,FilterMilliseconds,SaveMilliseconds,Bytes.Num(),InitialMarkerCount,InitialRouteRowCount));
	return !HasAnyErrors();
}

#endif
