from pathlib import Path
exec(Path('Scripts/ImplementFirewood.py').read_text().split('def compiled(t):')[0])
# Clamp transport requests to actual unprotected surplus instead of repeatedly failing an oversized reservation.
edit('Source/HansaSimulation/Private/Logistics/HansaLocalLogisticsInternal.cpp',lambda t:replace(t,'FMath::Min(SourceStock->Available.GetRawValue(), DestinationFree)', 'FMath::Min(FMath::Max<int64>(0, SourceStock->Available.GetRawValue() - InventoryView.QueryProtectedRaw(Request.SourceInventoryId, Request.GoodId)), DestinationFree)'))
edit('Source/HansaSimulation/Private/Trade/HansaTradeInternal.cpp',lambda t:t.replace('Stock->Available.GetRawValue()', 'FMath::Max<int64>(0, Stock->Available.GetRawValue() - Inventories.CreateReadOnlyAccess().QueryProtectedRaw(InventoryId, Action.GoodId))') if False else t)
# Public typed read model; policy mutations still use the gateway.
edit('Source/HansaSimulation/Public/Queries/HansaSimulationReadOnly.h',lambda t:t.replace('#pragma once','#pragma once\n#include "Population/HansaHeating.h"',1).replace('\t\t[[nodiscard]] TOptional<FHansaCityPopulationProjection> QueryCityPopulation', '\t\t[[nodiscard]] FHansaHeatingProjection QueryHeating(FHansaCityDefinitionId CityId) const;\n\t\t[[nodiscard]] TOptional<FHansaCityPopulationProjection> QueryCityPopulation',1))
edit('Source/HansaSimulation/Private/Queries/HansaSimulationReadOnly.cpp',lambda t:replace(t,'\tTOptional<FHansaCityPopulationProjection> FHansaSimulationReadOnlyAccess::QueryCityPopulation(','''\tFHansaHeatingProjection FHansaSimulationReadOnlyAccess::QueryHeating(FHansaCityDefinitionId CityId) const
\t{
\t\tconst auto* Registry = Definitions->GetEconomicRegistry();
\t\treturn Registry ? FHansaHeating::Project(CityId, State->InventoryLedger.CreateReadOnlyAccess(), State->Cities,
\t\t\tState->PopulationCohorts, *Registry, State->Clock) : FHansaHeatingProjection();
\t}

\tTOptional<FHansaCityPopulationProjection> FHansaSimulationReadOnlyAccess::QueryCityPopulation('''))
edit('Source/Hansa/Public/World/HansaRuntimeSimulationHost.h',lambda t:t.replace('#pragma once','#pragma once\n#include "Population/HansaHeating.h"',1).replace('\t[[nodiscard]] int64 GetSimulationTick() const;', '''\t[[nodiscard]] int64 GetSimulationTick() const;
\t[[nodiscard]] Hansa::Simulation::FHansaHeatingProjection QueryHeating() const;
\tHansa::Simulation::FHansaCommandGatewayResult SetHeatingReserve(Hansa::Simulation::FHansaBuildingId MarketId, int32 Days, bool bOverride);''',1))
edit('Source/Hansa/Private/World/HansaRuntimeSimulationHost.cpp',lambda t:replace(t,'FHansaCommandGatewayResult UHansaRuntimeSimulationHost::QueueResearch(const FString& TechnologyId)', '''FHansaHeatingProjection UHansaRuntimeSimulationHost::QueryHeating() const
{
\treturn IsReady() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).QueryHeating(Runtime->CityId) : FHansaHeatingProjection();
}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::SetHeatingReserve(FHansaBuildingId MarketId, int32 Days, bool bOverride)
{
\tif (!IsReady()) return FHansaCommandGatewayResult();
\tauto Result = ExecuteRuntimeCommand(*Runtime, FHansaSetHeatingReserveCommand{MarketId, Days, bOverride});
\tif (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
\treturn Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::QueueResearch(const FString& TechnologyId)'''))
def inspector(t):
    marker='\tif (Residence != nullptr && Definition != nullptr && !Definition->UpgradeTargetBuildingId.IsEmpty())'
    code='''\tif (Definition && Definition->bProvidesMarketAccess && Registry->FindNeed(TEXT("Need.Heating")))
\t{
\t\tconst auto H = Host->QueryHeating();
\t\tconst FText Summary = FText::Format(LOCTEXT("HeatingLedger", "Firewood: {0} in household pools; {1} committed. Households {2}/day; protected target {3}; surplus {4}. Winter target {5}."),
\t\t\tFText::AsNumber(H.StockRaw / 1000.), FText::AsNumber(H.CommittedRaw / 1000.), FText::AsNumber(H.HouseholdDailyRaw / 1000.),
\t\t\tFText::AsNumber(H.ProtectedRaw / 1000.), FText::AsNumber(H.SurplusRaw / 1000.), FText::AsNumber(H.WinterDailyRaw * H.ReserveDays / 1000.));
\t\tSnapshot.Actions.Add(Action(TEXT("Inspector.Heating.Decrease"), LOCTEXT("HeatingLess", "Reduce heating reserve by one day"), Summary));
\t\tSnapshot.Actions.Last().bEnabled = H.ReserveDays > 0;
\t\tSnapshot.Actions.Add(Action(TEXT("Inspector.Heating.Increase"), FText::Format(LOCTEXT("HeatingMore", "Heating reserve: {0} days (+1)"), FText::AsNumber(H.ReserveDays)), Summary));
\t\tSnapshot.Actions.Last().bEnabled = H.ReserveDays < 90;
\t\tSnapshot.Actions.Add(Action(TEXT("Inspector.Heating.Override"), H.bOverride ? LOCTEXT("HeatingProtect", "Restore household fuel protection") : LOCTEXT("HeatingRelease", "Release household fuel reserve"), Summary));
\t\tSnapshot.Actions.Last().bSelected = H.bOverride;
\t}
'''
    t=replace(t,marker,code+marker)
    marker='bool UHansaInspectorPresentationModel::ActivateAction(const FName SemanticId)\n{'
    return replace(t,marker,marker+'''
\tif (SemanticId.ToString().StartsWith(TEXT("Inspector.Heating.")))
\t{
\t\tauto* Host = RuntimeHost.Get();
\t\tconst auto Id = Hansa::Simulation::FHansaBuildingId::TryCreate(Snapshot.BuildingValue);
\t\tif (!Host || !Id || !IsActionEnabled(SemanticId)) return false;
\t\tconst auto H = Host->QueryHeating();
\t\tint32 Days = H.ReserveDays; bool Override = H.bOverride;
\t\tif (SemanticId == TEXT("Inspector.Heating.Increase")) ++Days;
\t\telse if (SemanticId == TEXT("Inspector.Heating.Decrease")) --Days;
\t\telse if (SemanticId == TEXT("Inspector.Heating.Override")) Override = !Override;
\t\telse return false;
\t\tconst auto Result = Host->SetHeatingReserve(Id.Value, Days, Override);
\t\tSnapshot.LastActionResult = Result.IsSuccess() ? LOCTEXT("HeatingPolicySaved", "Household fuel policy updated") : GatewayFailure(Result);
\t\treturn Result.IsSuccess();
\t}
''')
edit('Source/Hansa/Private/UI/HansaInspectorPresentationModel.cpp',inspector)
# Only invalidate city hash when a policy command changes it.
edit('Source/HansaSimulation/Private/Systems/HansaSimulationPipeline.cpp',lambda t:t.replace('\t\tCandidate.InvalidateAllStateHashCaches();\n','',1).replace('\t\t\tcase EHansaGameplayCommandType::CreateTestEntity:\n\t\t\tcase EHansaGameplayCommandType::CancelTestEntity:', '\t\t\tcase EHansaGameplayCommandType::SetHeatingReserve:\n\t\t\t\tDirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Cities);\n\t\t\t\tbreak;\n\t\t\tcase EHansaGameplayCommandType::CreateTestEntity:\n\t\t\tcase EHansaGameplayCommandType::CancelTestEntity:',1))
print('Heating read model and existing inspector controls connected.')
