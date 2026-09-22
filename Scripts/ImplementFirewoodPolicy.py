from pathlib import Path
exec((Path(__file__).parent / 'ImplementFirewood.py').read_text().split('def compiled(t):')[0])
edit('Source/HansaSimulation/Public/Definitions/HansaEconomicRegistry.h',lambda t: replace(t,'int32 FixedSeason = -1;','int32 FixedSeason = -1;\n\t\tint32 DefaultReserveDays = 3;'))
edit('Source/Hansa/Public/Definitions/HansaPopulationDefinitions.h',lambda t: replace(t,'\tint32 FixedSeason = -1;','''\tint32 FixedSeason = -1;

\tUPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Need|Heating", meta = (
\t\tDisplayName = "Default household reserve days", ToolTip = "Default protected household heating stock in days; excludes industrial demand.",
\t\tClampMin = "0", ClampMax = "90", HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
\t\tHansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Range", HansaUnit = "GameDay", HansaMin = "0", HansaMax = "90"))
\tint32 DefaultReserveDays = 3;'''))
edit('Source/Hansa/Private/Definitions/HansaPopulationDefinitions.cpp',lambda t: t.replace('if (SeasonDays < 1','if (DefaultReserveDays < 0 || DefaultReserveDays > 90 || SeasonDays < 1',1).replace('if (bSeasonal || SeasonDays','if (DefaultReserveDays != 3 || bSeasonal || SeasonDays',1).replace('for (int32 Factor : SeasonMultipliers)', 'InOutCanonicalData += FString::Printf(TEXT("reserveDays=%d;"), DefaultReserveDays);\n\t\tfor (int32 Factor : SeasonMultipliers)',1))
edit('Source/Hansa/Private/Definitions/HansaEconomicDefinitionCompiler.cpp',lambda t:replace(t,'Need->FixedSeason, Need->SeasonMultipliers','Need->FixedSeason, Need->DefaultReserveDays, Need->SeasonMultipliers'))
def command_h(t):
    t=replace(t,'\tenum class EHansaGameplayCommandType : uint8','''\tstruct FHansaSetHeatingReserveCommand
\t{
\t\tFHansaBuildingId MarketBuildingId;
\t\tint32 ReserveDays = 3;
\t\tbool bReleaseProtection = false;
\t};

\tenum class EHansaGameplayCommandType : uint8''')
    t=replace(t,'\t\tQueueResearch\n','\t\tQueueResearch,\n\t\tSetHeatingReserve\n')
    t=replace(t,'\t\t[[nodiscard]] uint64 ComputeStableFingerprint() const;','''\t\tstatic FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaSetHeatingReserveCommand& Payload);
\t\t[[nodiscard]] const FHansaSetHeatingReserveCommand& GetSetHeatingReserve() const;
\t\t[[nodiscard]] uint64 ComputeStableFingerprint() const;''')
    return replace(t,'\t\tFHansaQueueResearchCommand QueueResearch;','\t\tFHansaQueueResearchCommand QueueResearch;\n\t\tFHansaSetHeatingReserveCommand SetHeatingReserve;')
edit('Source/HansaSimulation/Public/Commands/HansaGameplayCommand.h',command_h)
def command_cpp(t):
    t=replace(t,'\t\tcase EHansaGameplayCommandType::QueueResearch: return TEXT("QueueResearch");','\t\tcase EHansaGameplayCommandType::QueueResearch: return TEXT("QueueResearch");\n\t\tcase EHansaGameplayCommandType::SetHeatingReserve: return TEXT("SetHeatingReserve");')
    marker='\tuint64 FHansaGameplayCommand::ComputeStableFingerprint() const'
    methods='''\tFHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& Header, const FHansaSetHeatingReserveCommand& Payload)
\t{
\t\tFHansaGameplayCommand Command; Command.Header = Header;
\t\tCommand.Type = EHansaGameplayCommandType::SetHeatingReserve; Command.SetHeatingReserve = Payload; return Command;
\t}
\tconst FHansaSetHeatingReserveCommand& FHansaGameplayCommand::GetSetHeatingReserve() const
\t{
\t\tcheck(Type == EHansaGameplayCommandType::SetHeatingReserve); return SetHeatingReserve;
\t}

'''
    t=replace(t,marker,methods+marker)
    marker='\t\tcase EHansaGameplayCommandType::QueueResearch:\n'
    return replace(t,marker,'''\t\tcase EHansaGameplayCommandType::SetHeatingReserve:
\t\t\tAddUInt64(Hash, SetHeatingReserve.MarketBuildingId.GetValue());
\t\t\tAddUInt32(Hash, SetHeatingReserve.MarketBuildingId.GetGeneration());
\t\t\tAddUInt32(Hash, SetHeatingReserve.ReserveDays);
\t\t\tAddByte(Hash, SetHeatingReserve.bReleaseProtection ? 1 : 0);
\t\t\tbreak;
'''+marker)
edit('Source/HansaSimulation/Private/Commands/HansaGameplayCommand.cpp',command_cpp)
def pipeline(t):
    t='#include "Population/HansaHeating.h"\n'+t
    marker='\t\t\tcase EHansaGameplayCommandType::QueueResearch:\n'
    code='''\t\t\tcase EHansaGameplayCommandType::SetHeatingReserve:
\t\t\t{
\t\t\t\tconst auto& Payload = Command.GetSetHeatingReserve();
\t\t\t\tconst auto* Registry = Definitions.GetEconomicRegistry();
\t\t\t\tconst auto* Building = Candidate.Buildings.FindByPredicate([&](const auto& V) { return V.Id == Payload.MarketBuildingId; });
\t\t\t\tconst auto* Placement = Candidate.Placement.FindPlacement(Payload.MarketBuildingId);
\t\t\t\tconst auto* Definition = Building && Registry ? Registry->FindBuilding(Building->DefinitionId.ToString()) : nullptr;
\t\t\t\tif (!Building || Building->OwnerId != Header.Authority.IssuingHouseId || !Placement || !Definition ||
\t\t\t\t\t!Definition->bProvidesMarketAccess || Building->ConstructionState != EHansaConstructionState::Completed ||
\t\t\t\t\tPayload.ReserveDays < 0 || Payload.ReserveDays > 90)
\t\t\t\t\treturn MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
\t\t\t\t// A shared city policy requires uncontested civic ownership in this MVP.
\t\t\t\tfor (const auto& Other : Candidate.Buildings)
\t\t\t\t{
\t\t\t\t\tconst auto* OtherPlacement = Candidate.Placement.FindPlacement(Other.Id);
\t\t\t\t\tconst auto* OtherDefinition = Registry->FindBuilding(Other.DefinitionId.ToString());
\t\t\t\t\tif (OtherPlacement && OtherPlacement->Spec.CityId == Placement->Spec.CityId && OtherDefinition &&
\t\t\t\t\t\tOtherDefinition->bProvidesMarketAccess && Other.OwnerId != Header.Authority.IssuingHouseId)
\t\t\t\t\t\treturn MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
\t\t\t\t}
\t\t\t\tauto* City = Candidate.Cities.FindByPredicate([&](const auto& V) { return V.DefinitionId == Placement->Spec.CityId; });
\t\t\t\tif (!City) return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
\t\t\t\tCity->HeatingReserveDays = Payload.ReserveDays; City->bReleaseHeatingReserve = Payload.bReleaseProtection;
\t\t\t\tFHansaHeating::RefreshProtection(Candidate.InventoryLedger, Candidate.Cities, Candidate.PopulationCohorts, *Registry, Candidate.Clock);
\t\t\t\tEvent.Type = EHansaDomainEventType::HeatingReserveChanged;
\t\t\t\tEvent.BuildingId = Payload.MarketBuildingId; Event.Value = Payload.ReserveDays;
\t\t\t\tbreak;
\t\t\t}
'''
    t=replace(t,marker,code+marker)
    marker='\t\t\tTransientCache.RecordPhase(Phase);'
    t=replace(t,marker,marker+'''
\t\t\tif (const auto* Registry = Definitions.GetEconomicRegistry())
\t\t\t\tFHansaHeating::RefreshProtection(Candidate.InventoryLedger, Candidate.Cities, Candidate.PopulationCohorts, *Registry, ClockAfter.Value);''')
    marker='FHansaSimulationState Candidate = State;'
    t=replace(t,marker,marker+'''
\t\tif (const auto* Registry = Definitions.GetEconomicRegistry())
\t\t\tFHansaHeating::RefreshProtection(Candidate.InventoryLedger, Candidate.Cities, Candidate.PopulationCohorts, *Registry, Candidate.Clock);''')
    # Policy commands affect the Cities subsystem, even when no construction occurs.
    marker='\t\tTransientCache.BeginStep(TickBefore, AuthoritativeEntityCount);'
    t=replace(t,marker,'\t\tCandidate.InvalidateAllStateHashCaches();\n'+marker)
    return t
edit('Source/HansaSimulation/Private/Systems/HansaSimulationPipeline.cpp',pipeline)
print('Heating policy command and phase refresh implemented.')
