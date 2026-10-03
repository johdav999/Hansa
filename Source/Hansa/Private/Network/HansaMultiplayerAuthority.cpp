#include "Network/HansaMultiplayerAuthority.h"
#include "UI/HansaTradeRemoteProjection.h"

#include "Commands/HansaGameplayCommand.h"
#include "Events/HansaDomainEvent.h"
#include "HAL/PlatformTime.h"
#include "Inventory/HansaInventory.h"
#include "Logistics/HansaLocalLogistics.h"
#include "Placement/HansaPlacement.h"
#include "Population/HansaPopulation.h"
#include "Production/HansaProduction.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Scenario/HansaScenario.h"
#include "Serialization/MemoryWriter.h"
#include "Trade/HansaTrade.h"
#include "World/HansaRuntimeSimulationHost.h"

#include <type_traits>

using namespace Hansa::Simulation;

namespace
{
	constexpr uint64 FnvOffset = 14695981039346656037ULL;
	constexpr uint64 FnvPrime = 1099511628211ULL;

	void HashByte(uint64& Hash, const uint8 Value)
	{
		Hash ^= Value;
		Hash *= FnvPrime;
	}

	template <typename TValue>
	void HashInteger(uint64& Hash, const TValue Value)
	{
		using TUnsigned = std::make_unsigned_t<TValue>;
		const TUnsigned Unsigned = static_cast<TUnsigned>(Value);
		for (uint32 Index = 0; Index < sizeof(TUnsigned); ++Index)
		{
			HashByte(Hash, static_cast<uint8>((Unsigned >> (Index * 8)) & 0xff));
		}
	}

	void HashString(uint64& Hash, const FString& Value)
	{
		FTCHARToUTF8 Converted(*Value);
		HashInteger(Hash, Converted.Length());
		for (int32 Index = 0; Index < Converted.Length(); ++Index)
		{
			HashByte(Hash, static_cast<uint8>(Converted.Get()[Index]));
		}
	}

	FString HexHash(const uint64 Value)
	{
		return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Value));
	}

	uint64 ProjectionDigest(const FHansaClientProjectionSnapshot& Projection)
	{
		uint64 Hash = FnvOffset;
		HashInteger(Hash, Projection.SchemaVersion);
		HashInteger(Hash, Projection.ServerTick);
		HashInteger(Hash, Projection.OwnerHouseId);
		HashInteger(Hash, Projection.OwnerMoneyPfennig);
        {TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaReplicatedTradeWorkspace::StaticStruct()->SerializeItem(Writer,const_cast<FHansaReplicatedTradeWorkspace*>(&Projection.TradeWorkspace),nullptr);for(uint8 B:Bytes)HashByte(Hash,B);}
        for(const auto& O:Projection.Recoveries){TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaTradeRecovery::StaticStruct()->SerializeItem(Writer,const_cast<FHansaTradeRecovery*>(&O),nullptr);for(uint8 B:Bytes)HashByte(Hash,B);}
        for(const auto& O:Projection.VisitingTrade){TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaVisitingTradeOffer::StaticStruct()->SerializeItem(Writer,const_cast<FHansaVisitingTradeOffer*>(&O),nullptr);for(uint8 B:Bytes)HashByte(Hash,B);}
		HashString(Hash, Projection.ScenarioOutcome);
		HashString(Hash, Projection.WinningVictoryId);
		for (const FHansaReplicatedPlacement& Item : Projection.Placements)
		{
			HashInteger(Hash, Item.BuildingId); HashInteger(Hash, Item.OwnerHouseId);
			HashString(Hash, Item.CityId); HashString(Hash, Item.BuildingDefinitionId);
			HashInteger(Hash, Item.AnchorX); HashInteger(Hash, Item.AnchorY);
			HashString(Hash, Item.Status); HashInteger(Hash, Item.ProgressPartsPerMillion);
		}
		for (const FHansaReplicatedMarket& Item : Projection.Markets)
		{
			HashString(Hash, Item.CityId); HashString(Hash, Item.GoodId);
			HashInteger(Hash, Item.StockMilliUnits); HashInteger(Hash, Item.DesiredReserveMilliUnits);
			HashInteger(Hash, Item.CurrentPriceMilliMarks); HashInteger(Hash, Item.ReportAgeTicks);
		}
		for (const FHansaReplicatedRoute& Item : Projection.Routes)
		{
			HashInteger(Hash, Item.RouteId); HashInteger(Hash, Item.OwnerHouseId);
            HashString(Hash, Item.PlanKey); HashString(Hash, Item.Label);
			HashInteger(Hash, Item.VehicleId); HashString(Hash, Item.Mode); HashString(Hash, Item.Lifecycle);
			HashString(Hash, Item.CurrentCityId); HashInteger(Hash, Item.RemainingTravelTicks);
			HashInteger(Hash, static_cast<uint8>(Item.bCargoVisible)); HashInteger(Hash, Item.CargoMilliUnits);
		}
        for(const auto& Vehicle:Projection.Vehicles){HashInteger(Hash,Vehicle.VehicleId);HashInteger(Hash,uint8(Vehicle.bPrivateDetailsVisible));for(const auto& Slot:Vehicle.CargoSlots){HashString(Hash,Slot.GoodId);HashInteger(Hash,Slot.QuantityMilliUnits);}}
		HashInteger(Hash, Projection.Research.HouseId);
		HashInteger(Hash, Projection.Research.AvailableResearchPoints);
		HashString(Hash, Projection.Research.ActiveTechnologyId);
		HashInteger(Hash, Projection.Research.ProgressTicks);
		for (const FString& Id : Projection.Research.CompletedTechnologyIds) HashString(Hash, Id);
		for (const FHansaReplicatedVictoryObjective& Item : Projection.VictoryObjectives)
		{
			HashString(Hash, Item.VictoryId); HashString(Hash, Item.ObjectiveId);
			HashInteger(Hash, Item.CurrentValue); HashInteger(Hash, Item.TargetValue);
			HashInteger(Hash, static_cast<uint8>(Item.bMet));
		}
        for(const auto& View:Projection.StationOrders){
            HashString(Hash,View.City.ToString());HashInteger(Hash,View.StationId);
            HashInteger(Hash,View.CapacityMilliUnits);HashInteger(Hash,View.MaximumCapMilliUnits);
            HashInteger(Hash,View.MaximumBudgetPfennig);HashInteger(Hash,static_cast<uint8>(View.bOperational));
            for(const auto& Good:View.GoodIds)HashString(Hash,Good);
            for(const auto& Order:View.Orders){
                HashInteger(Hash,Order.Id);HashString(Hash,Order.GoodId);HashInteger(Hash,Order.Side);
                HashInteger(Hash,Order.TargetMilliUnits);HashInteger(Hash,Order.CapMilliUnits);
                HashInteger(Hash,Order.BudgetPfennig);HashInteger(Hash,Order.SpentPfennig);
                HashInteger(Hash,Order.NextUpdateTick);HashInteger(Hash,static_cast<uint8>(Order.bPaused));
                HashInteger(Hash,static_cast<uint8>(Order.bCancelled));
                for(const auto& Event:Order.History){
                    HashInteger(Hash,Event.Tick);HashInteger(Hash,Event.AppliedMilliUnits);
                    HashInteger(Hash,Event.MoneyDelta);HashInteger(Hash,Event.Outcome);HashInteger(Hash,Event.Blocker);
                }
            }
        }
        for(const auto& V:Projection.Presences){HashString(Hash,V.City.ToString());HashString(Hash,V.CurrentStage);HashString(Hash,V.Specialization.Key());for(const auto& C:V.ConstructionReports)HashString(Hash,C.Key());HashString(Hash,V.NextStageId);HashString(Hash,V.Status);HashInteger(Hash,V.UpgradeStatus);HashInteger(Hash,static_cast<uint8>(V.bOfficeVisual));HashInteger(Hash,V.CompletionTick);HashString(Hash,V.ConstructionDelivery);HashString(Hash,V.History);for(const auto& R:V.Requirements){HashString(Hash,R.Id);HashInteger(Hash,R.Current);HashInteger(Hash,R.Required);HashInteger(Hash,static_cast<uint8>(R.bMet));}for(const auto& C:V.Sources){HashString(Hash,C.Id);HashString(Hash,C.Detail.ToString());HashInteger(Hash,static_cast<uint8>(C.bEligible));}}
		for (const FHansaReplicatedEvent& Item : Projection.Events)
		{
			HashInteger(Hash, Item.GlobalSequence); HashInteger(Hash, Item.Tick); HashString(Hash, Item.Type);
			HashInteger(Hash, Item.IssuingHouseId); HashInteger(Hash, Item.BuildingId);
			HashInteger(Hash, Item.RouteId); HashString(Hash, Item.CityId);
			HashString(Hash, Item.GoodId); HashString(Hash, Item.TechnologyId);
		}
		return Hash;
	}

	void SerializeProjectionForDiagnostics(const FHansaClientProjectionSnapshot& Projection,
		TArray<uint8>& OutBytes)
	{
		FHansaClientProjectionSnapshot Copy = Projection;
		Copy.AuthoritativeHash.Reset();
		Copy.ProjectionDigest.Reset();
		Copy.AuthorizedViewDigest.Reset();
		Copy.SerializedBytes = 0;
		Copy.BuildMicroseconds = 0;
		FMemoryWriter Writer(OutBytes, true);
		FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Writer, &Copy, nullptr);
	}

	template <typename TItem, typename TKey>
	void MakeDeltaCollection(const TArray<TItem>& Previous, TArray<TItem>& Current,
		const TCHAR* Collection, TKey KeyOf, TArray<FHansaProjectionRemoval>& OutRemoved)
	{
		TMap<FString, const TItem*> PreviousByKey;
		for (const TItem& Item : Previous) PreviousByKey.Add(KeyOf(Item), &Item);
		TSet<FString> CurrentKeys;
		for (const TItem& Item : Current) CurrentKeys.Add(KeyOf(Item));
		for (const TItem& Item : Previous)
		{
			const FString Key = KeyOf(Item);
			if (!CurrentKeys.Contains(Key))
			{
				FHansaProjectionRemoval& Removal = OutRemoved.AddDefaulted_GetRef();
				Removal.Collection = Collection;
				Removal.StableId = Key;
			}
		}
		Current.RemoveAll([&PreviousByKey, &KeyOf](const TItem& Item)
		{
			const TItem* const* Prior = PreviousByKey.Find(KeyOf(Item));
			return Prior != nullptr && TItem::StaticStruct()->CompareScriptStruct(*Prior, &Item, 0);
		});
	}

	FHansaClientCommandFeedback Reject(const FHansaClientCommandIntent& Intent,
		const EHansaClientCommandRejection Rejection, const FString& Message, const FString& Remedy,
		const UHansaRuntimeSimulationHost* Host)
	{
		FHansaClientCommandFeedback Result;
		Result.ClientSequence = Intent.ClientSequence;
		Result.ClientNonce = Intent.ClientNonce;
		Result.Rejection = Rejection;
		Result.State = EHansaClientCommandState::Rejected;
		Result.Message = Message;
		Result.Remedy = Remedy;
		Result.ServerTick = Host != nullptr ? Host->GetSimulationTick() : 0;
		return Result;
	}

	bool EventIsResearch(const EHansaDomainEventType Type)
	{
		return Type == EHansaDomainEventType::ResearchQueued || Type == EHansaDomainEventType::ResearchCompleted;
	}

	bool EventIsPrivateCargo(const EHansaDomainEventType Type)
	{
		return Type == EHansaDomainEventType::RouteCargoTransferred ||
			Type == EHansaDomainEventType::RouteCargoMissed;
	}

	bool ConvertRouteStops(const TArray<FHansaClientRouteStopIntent>& Source,
		TArray<FHansaRouteStop>& OutStops)
	{
		if (Source.Num() < 2 || Source.Num() > FHansaClientCommandIntent::MaximumRouteStopCount) return false;
		OutStops.Reset(Source.Num());
		for (const FHansaClientRouteStopIntent& SourceStop : Source)
		{
			const auto CityId = FHansaCityDefinitionId::TryParse(SourceStop.CityId);
			if (!CityId || SourceStop.CityId.Len() > 128 ||
				SourceStop.Actions.Num() > FHansaClientCommandIntent::MaximumActionsPerStop) return false;
			FHansaRouteStop& Stop = OutStops.AddDefaulted_GetRef();
			Stop.CityId = CityId.Value;
			for (const FHansaClientRouteActionIntent& SourceAction : SourceStop.Actions)
			{
				const auto GoodId = FHansaGoodId::TryParse(SourceAction.GoodId);
				if (!GoodId || SourceAction.GoodId.Len() > 128 || SourceAction.Kind > 5 ||
					SourceAction.QuantityMilliUnits <= 0 || SourceAction.QuantityMilliUnits > 1'000'000'000'000LL ||
					SourceAction.MinimumSourceReserveMilliUnits < 0 ||
					SourceAction.MinimumSourceReserveMilliUnits > 1'000'000'000'000LL) return false;
				FHansaRouteCargoAction& Action = Stop.Actions.AddDefaulted_GetRef();
				Action.Kind = static_cast<EHansaRouteCargoActionKind>(SourceAction.Kind);
				Action.Condition = EHansaRouteCargoCondition::Always;
				Action.GoodId = GoodId.Value;
				Action.QuantityLimit = FHansaQuantity::FromRaw(SourceAction.QuantityMilliUnits);
				Action.CargoSlotIndex = SourceAction.CargoSlotIndex;
				if (Action.CargoSlotIndex < INDEX_NONE || Action.CargoSlotIndex >= 3) return false;
				Action.MinimumSourceReserve =
					FHansaQuantity::FromRaw(SourceAction.MinimumSourceReserveMilliUnits);
			}
		}
		return true;
	}
}

namespace Hansa::Multiplayer
{
	bool FHansaMultiplayerAuthority::Initialize(UHansaRuntimeSimulationHost& InHost)
	{
		if (!InHost.IsReady()) return false;
		Host = &InHost;
		Clients.Reset();
		return true;
	}

	bool FHansaMultiplayerAuthority::ValidateInterest(const FHansaClientInterest& Interest, FString& OutError) const
	{
		if (Interest.HistoryOffset < 0 || Interest.HistoryPageSize < 0 ||
			Interest.HistoryPageSize > FHansaClientInterest::MaximumHistoryPageSize)
		{
			OutError = TEXT("Market history requests must use a non-negative offset and at most 64 entries.");
			return false;
		}
		if (Interest.CityIds.Num() > FHansaClientInterest::MaximumCityCount)
		{
			OutError = TEXT("A client may observe at most four MVP cities.");
			return false;
		}
		TSet<FString> Unique;
		for (const FString& CityText : Interest.CityIds)
		{
			const auto CityId = FHansaCityDefinitionId::TryParse(CityText);
			if (!CityId || Unique.Contains(CityId.Value.ToString()))
			{
				OutError = TEXT("Client city interests must be unique valid City.* stable IDs.");
				return false;
			}
			const UHansaRuntimeSimulationHost* RuntimeHost = Host.Get();
			const auto Projection = RuntimeHost != nullptr ? RuntimeHost->BuildProjection() :
				THansaValueResult<FHansaSimulationProjection>::Failure(EHansaValueError::InvalidZero);
			if (!Projection || !Projection.Value.GetMarkets().ContainsByPredicate(
				[&CityId](const FHansaCityMarketProjection& Market) { return Market.CityId == CityId.Value; }))
			{
				OutError = TEXT("Client city interests must identify a city in the active scenario.");
				return false;
			}
			Unique.Add(CityId.Value.ToString());
		}
		return true;
	}

	bool FHansaMultiplayerAuthority::RegisterAdmittedClient(const FHansaAdmissionGrant& Admission,
		const FHansaClientInterest& Interest, FString& OutError)
	{
		const uint64 PrincipalId = Admission.PrincipalId;
		const FHansaHouseId HouseId = Admission.HouseId;
		UHansaRuntimeSimulationHost* RuntimeHost = Host.Get();
		if (RuntimeHost == nullptr || !RuntimeHost->IsReady() || PrincipalId == 0 ||
			!Admission.ParticipantId.IsValid() || !HouseId.IsValid())
		{
			OutError = TEXT("The authority host, principal, or house is invalid.");
			return false;
		}
		if (Clients.Contains(PrincipalId))
		{
			OutError = TEXT("The principal is already registered.");
			return false;
		}
		const auto Projection = RuntimeHost->BuildProjection();
		if (!Projection || !Projection.Value.GetHouses().ContainsByPredicate(
			[HouseId](const FHansaHouseProjection& House) { return House.Id == HouseId; }))
		{
			OutError = TEXT("The requested house does not exist in the authoritative campaign.");
			return false;
		}
		FHansaClientInterest EffectiveInterest = Interest;
		if (EffectiveInterest.CityIds.IsEmpty())
		{
			EffectiveInterest.CityIds.Add(RuntimeHost->GetCityId().ToString());
		}
		if (!ValidateInterest(EffectiveInterest, OutError)) return false;
		if (!RuntimeHost->ClaimHouseForHuman(HouseId, Admission.ParticipantId, OutError)) return false;
		FClientState State;
		State.ParticipantId = Admission.ParticipantId;
		State.HouseId = HouseId;
		State.Interest = MoveTemp(EffectiveInterest);
		Clients.Add(PrincipalId, MoveTemp(State));
		return true;
	}

	void FHansaMultiplayerAuthority::UnregisterClient(const uint64 PrincipalId)
	{
		if (const FClientState* Client = Clients.Find(PrincipalId))
		{
			if (UHansaRuntimeSimulationHost* RuntimeHost = Host.Get())
			{
				FString Ignored;
				RuntimeHost->ReleaseHumanHouse(Client->ParticipantId, Ignored);
			}
		}
		Clients.Remove(PrincipalId);
	}

	bool FHansaMultiplayerAuthority::SetClientInterest(const uint64 PrincipalId,
		const FHansaClientInterest& Interest, FString& OutError)
	{
		FClientState* Client = Clients.Find(PrincipalId);
		if (Client == nullptr)
		{
			OutError = TEXT("The principal is not registered.");
			return false;
		}
		if (!ValidateInterest(Interest, OutError)) return false;
		Client->Interest = Interest;
		Client->LastProjectionRevision = 0;
		Client->LastDeliveredEventSequence = 0;
		return true;
	}

	bool FHansaMultiplayerAuthority::SetAuthorizedReportHouses(const uint64 PrincipalId,
		const TConstArrayView<FHansaHouseId> HouseIds, FString& OutError)
	{
		FClientState* Client = Clients.Find(PrincipalId);
		if (Client == nullptr)
		{
			OutError = TEXT("The principal is not registered.");
			return false;
		}
		if (HouseIds.Num() > 7)
		{
			OutError = TEXT("A participant may receive at most seven additional authorized house reports.");
			return false;
		}
		TSet<FHansaHouseId> Validated;
		for (const FHansaHouseId HouseId : HouseIds)
		{
			if (!HouseId.IsValid() || HouseId == Client->HouseId || Validated.Contains(HouseId))
			{
				OutError = TEXT("Authorized report houses must be unique, valid, and different from the owner house.");
				return false;
			}
			Validated.Add(HouseId);
		}
		Client->AuthorizedReportHouses = MoveTemp(Validated);
		return true;
	}

	bool FHansaMultiplayerAuthority::IsRegistered(const uint64 PrincipalId) const
	{
		return Clients.Contains(PrincipalId);
	}

	bool FHansaMultiplayerAuthority::IsHouseRegistered(const FHansaHouseId HouseId) const
	{
		for (const TPair<uint64, FClientState>& Pair : Clients)
		{
			if (Pair.Value.HouseId == HouseId) return true;
		}
		return false;
	}

	uint64 FHansaMultiplayerAuthority::GetExpectedClientSequence(const uint64 PrincipalId) const
	{
		const FClientState* Client = Clients.Find(PrincipalId);
		return Client != nullptr ? Client->ExpectedClientSequence : 0;
	}

	FHansaClientCommandFeedback FHansaMultiplayerAuthority::SubmitIntent(
		const uint64 PrincipalId, const FHansaClientCommandIntent& Intent)
	{
		UHansaRuntimeSimulationHost* RuntimeHost = Host.Get();
		if (RuntimeHost == nullptr || !RuntimeHost->IsReady())
		{
			return Reject(Intent, EHansaClientCommandRejection::AuthorityUnavailable,
				TEXT("The authoritative simulation is unavailable."),
				TEXT("Wait for the server to finish loading the scenario."), RuntimeHost);
		}
		FClientState* Client = Clients.Find(PrincipalId);
		if (Client == nullptr)
		{
			return Reject(Intent, EHansaClientCommandRejection::ClientNotRegistered,
				TEXT("This connection has no server authority identity."),
				TEXT("Rejoin the session so the server can assign a house."), RuntimeHost);
		}
		if (Intent.ClientSequence < 1 || static_cast<uint64>(Intent.ClientSequence) < Client->ExpectedClientSequence ||
			(Intent.ClientNonce > 0 && Client->SeenNonces.Contains(static_cast<uint64>(Intent.ClientNonce))))
		{
			return Reject(Intent, EHansaClientCommandRejection::DuplicateCommand,
				TEXT("The server rejected a duplicate client command."),
				TEXT("Send each intent once with the next sequence and a new nonce."), RuntimeHost);
		}
		if (static_cast<uint64>(Intent.ClientSequence) != Client->ExpectedClientSequence)
		{
			return Reject(Intent, EHansaClientCommandRejection::CommandOrderInvalid,
				TEXT("The client command arrived out of order."),
				FString::Printf(TEXT("Resubmit using client sequence %llu."),
					static_cast<unsigned long long>(Client->ExpectedClientSequence)), RuntimeHost);
		}
		if (Intent.SchemaVersion != FHansaClientCommandIntent::CurrentSchemaVersion || Intent.ClientNonce <= 0)
		{
			return Reject(Intent, EHansaClientCommandRejection::InvalidPayload,
				TEXT("The client command schema or nonce is invalid."),
				TEXT("Refresh the client projection and submit a current-schema command."), RuntimeHost);
		}
		constexpr int64 MaximumAcceptedTickLag = 64;
		const int64 ServerTick = RuntimeHost->GetSimulationTick();
        const bool bReviewedStation=(Intent.Type==EHansaClientIntentType::ProposeTradeStation||Intent.Type==EHansaClientIntentType::FundTradeStation)&&Intent.ExpectedServerTick>0;
		if (Intent.ExpectedServerTick < 0 || (Intent.ExpectedServerTick > ServerTick&&!bReviewedStation) ||
			(Intent.ExpectedServerTick > 0 && ServerTick - Intent.ExpectedServerTick > MaximumAcceptedTickLag&&!bReviewedStation))
		{
			return Reject(Intent, EHansaClientCommandRejection::StaleProjection,
				TEXT("The selected state is too old for this command."),
				TEXT("Refresh the projection, review the current target, and submit again."), RuntimeHost);
		}


		++Client->ExpectedClientSequence;
		Client->SeenNonces.Add(static_cast<uint64>(Intent.ClientNonce));
		if (Client->SeenNonces.Num() > 256) Client->SeenNonces.Reset();

        // A reviewed station proposal/spend promises exact terms, so its opt-in tick
        // precondition is strict rather than the general 64-tick command tolerance.
        if ((Intent.Type==EHansaClientIntentType::ProposeTradeStation || Intent.Type==EHansaClientIntentType::FundTradeStation) &&
            Intent.ExpectedServerTick>0 && Intent.ExpectedServerTick!=ServerTick)
        {
            return Reject(Intent, EHansaClientCommandRejection::StaleProjection,
                TEXT("The station review changed before confirmation."),
                TEXT("Refresh station access, inventory and treasury, then review the exact transfer again. Nothing was spent."), RuntimeHost);
        }

		const FHansaCommandAuthorityContext Authority {
			Client->HouseId, PrincipalId, EHansaCommandOrigin::MultiplayerRpc };
		FHansaCommandGatewayResult Gateway;
		bool bPayloadValid = true;
		switch (Intent.Type)
		{
		case EHansaClientIntentType::PlaceBuilding:
		{
			TArray<FHansaClientPlacementIntent> Requested = Intent.Placements;
			if (Requested.IsEmpty())
			{
				FHansaClientPlacementIntent& Legacy = Requested.AddDefaulted_GetRef();
				Legacy.CityId = Intent.CityId;
				Legacy.BuildingDefinitionId = Intent.BuildingDefinitionId;
				Legacy.AnchorX = Intent.AnchorX;
				Legacy.AnchorY = Intent.AnchorY;
				Legacy.Rotation = Intent.Rotation;
			}
			bPayloadValid = Requested.Num() <= FHansaClientCommandIntent::MaximumPlacementCount;
			TArray<FHansaPlacementSpec> Specs;
			for (const FHansaClientPlacementIntent& Item : Requested)
			{
				const auto CityId = FHansaCityDefinitionId::TryParse(Item.CityId);
				const auto BuildingTypeId = FHansaBuildingTypeId::TryParse(Item.BuildingDefinitionId);
				bPayloadValid = bPayloadValid && CityId && BuildingTypeId &&
					Item.Rotation <= static_cast<uint8>(EHansaGridRotation::West) &&
					FMath::Abs(Item.AnchorX) <= 4096 && FMath::Abs(Item.AnchorY) <= 4096 &&
					Item.CityId.Len() <= 128 && Item.BuildingDefinitionId.Len() <= 128;
				if (!bPayloadValid) break;
				FHansaPlacementSpec& Spec = Specs.AddDefaulted_GetRef();
				Spec.CityId = CityId.Value;
				Spec.BuildingDefinitionId = BuildingTypeId.Value;
				Spec.Anchor = { Item.AnchorX, Item.AnchorY };
				Spec.Rotation = static_cast<EHansaGridRotation>(Item.Rotation);
			}
			if (bPayloadValid) Gateway = RuntimeHost->PlaceBuildingsForAuthority(Authority, Specs);
			break;
		}
		case EHansaClientIntentType::SetRouteActive:
		{
			const auto RouteId = Intent.RouteId > 0
				? FHansaRouteId::TryCreate(static_cast<uint64>(Intent.RouteId))
				: THansaValueResult<FHansaRouteId>::Failure(EHansaValueError::InvalidZero);
			bPayloadValid = RouteId.IsSuccess();
			if (bPayloadValid)
			{
				Gateway = RuntimeHost->SetRouteActiveForAuthority(Authority, RouteId.Value, Intent.bActive);
			}
			break;
		}
		case EHansaClientIntentType::QueueResearch:
			bPayloadValid = !Intent.TechnologyId.IsEmpty() && Intent.TechnologyId.Len() <= 128 &&
				FHansaTechnologyId::TryParse(Intent.TechnologyId).IsSuccess();
			if (bPayloadValid)
			{
				Gateway = RuntimeHost->QueueResearchForAuthority(Authority, Intent.TechnologyId);
			}
			break;
		case EHansaClientIntentType::CancelConstruction:
		case EHansaClientIntentType::RemoveBuilding:
		case EHansaClientIntentType::UpgradeResidence:
		{
			const auto Id = Intent.BuildingId > 0
				? FHansaBuildingId::TryCreate(static_cast<uint64>(Intent.BuildingId))
				: THansaValueResult<FHansaBuildingId>::Failure(EHansaValueError::InvalidZero);
			bPayloadValid = Id.IsSuccess();
			if (bPayloadValid && Intent.Type == EHansaClientIntentType::CancelConstruction)
				Gateway = RuntimeHost->CancelConstructionForAuthority(Authority, Id.Value);
			else if (bPayloadValid && Intent.Type == EHansaClientIntentType::RemoveBuilding)
			{
				Gateway = RuntimeHost->RemoveBuildingForAuthority(Authority, Id.Value);
				if (Gateway.GetError() == EHansaCommandGatewayError::ConstructionStateInvalid)
					Gateway = RuntimeHost->CancelConstructionForAuthority(Authority, Id.Value);
			}
			else if (bPayloadValid)
				Gateway = RuntimeHost->UpgradeResidenceForAuthority(Authority, Id.Value);
			break;
		}
		case EHansaClientIntentType::SetProductionActive:
		case EHansaClientIntentType::UpgradeProduction:
		{
			const auto Id = Intent.ProductionId > 0
				? FHansaProductionId::TryCreate(static_cast<uint64>(Intent.ProductionId))
				: THansaValueResult<FHansaProductionId>::Failure(EHansaValueError::InvalidZero);
			bPayloadValid = Id.IsSuccess();
			if (bPayloadValid && Intent.Type == EHansaClientIntentType::SetProductionActive)
				Gateway = RuntimeHost->SetProductionActiveForAuthority(Authority, Id.Value, Intent.bActive);
			else if (bPayloadValid)
				Gateway = RuntimeHost->UpgradeProductionForAuthority(Authority, Id.Value);
			break;
		}
		case EHansaClientIntentType::SetProductionMode:
		{
			const auto Id = Intent.ProductionId > 0
				? FHansaProductionId::TryCreate(static_cast<uint64>(Intent.ProductionId))
				: THansaValueResult<FHansaProductionId>::Failure(EHansaValueError::InvalidZero);
			const auto Recipe = FHansaRecipeId::TryParse(Intent.RecipeId);
			bPayloadValid = Id && Recipe && Intent.RecipeId.Len() <= 128;
			if (bPayloadValid) Gateway = RuntimeHost->SetProductionModeForAuthority(
				Authority, Id.Value, Recipe.Value, Intent.bFallback);
			break;
		}
		case EHansaClientIntentType::SetHeatingReserve:
		{
			const auto Id = Intent.BuildingId > 0
				? FHansaBuildingId::TryCreate(static_cast<uint64>(Intent.BuildingId))
				: THansaValueResult<FHansaBuildingId>::Failure(EHansaValueError::InvalidZero);
			bPayloadValid = Id && Intent.ReserveDays >= 0 && Intent.ReserveDays <= 365;
			if (bPayloadValid) Gateway = RuntimeHost->SetHeatingReserveForAuthority(
				Authority, Id.Value, Intent.ReserveDays, Intent.bReleaseProtection);
			break;
		}
		case EHansaClientIntentType::SetHouseholdAvailability:
		{
			const auto Id = Intent.BuildingId > 0
				? FHansaBuildingId::TryCreate(static_cast<uint64>(Intent.BuildingId))
				: THansaValueResult<FHansaBuildingId>::Failure(EHansaValueError::InvalidZero);
			const auto Good = FHansaGoodId::TryParse(Intent.GoodId);
			bPayloadValid = Id && Good && Intent.GoodId.Len() <= 128;
			if (bPayloadValid) Gateway = RuntimeHost->SetHouseholdAvailabilityForAuthority(
				Authority, Id.Value, Good.Value, Intent.bAvailable);
			break;
		}
		case EHansaClientIntentType::CreateRoute:
		{
			const auto Vehicle = Intent.VehicleId > 0
				? FHansaVehicleId::TryCreate(static_cast<uint64>(Intent.VehicleId))
				: THansaValueResult<FHansaVehicleId>::Failure(EHansaValueError::InvalidZero);
			TArray<FHansaRouteStop> Stops;
			bPayloadValid = Vehicle && ConvertRouteStops(Intent.RouteStops, Stops) &&
				!Intent.RouteName.IsEmpty() && Intent.RouteName.Len() <= 48;
			uint64 NewRouteId = 0;
			if (bPayloadValid) Gateway = RuntimeHost->CreateTradeRouteForAuthority(
				Authority, Vehicle.Value, Stops, Intent.RouteName,
				Intent.bReassignStoppedVehicle, NewRouteId);
			break;
		}
		case EHansaClientIntentType::EditRoute:
		{
			const auto Route = Intent.RouteId > 0
				? FHansaRouteId::TryCreate(static_cast<uint64>(Intent.RouteId))
				: THansaValueResult<FHansaRouteId>::Failure(EHansaValueError::InvalidZero);
			TArray<FHansaRouteStop> Stops;
			bPayloadValid = Route && ConvertRouteStops(Intent.RouteStops, Stops);
            if(bPayloadValid&&!Intent.ExpectedRoutePlanKey.IsEmpty()){
             const auto P=RuntimeHost->BuildProjection();const auto* Current=P?P.Value.GetRoutes().FindByPredicate([&](const auto& R){return R.Id==Route.Value&&R.OwnerId==Client->HouseId;}):nullptr;
             TArray<FHansaClientRouteStopIntent> CurrentStops;
             if(Current)for(const auto& S:Current->Stops){auto& D=CurrentStops.AddDefaulted_GetRef();D.CityId=S.CityId.ToString();for(const auto& A:S.Actions)D.Actions.Add({uint8(A.Kind),A.GoodId.ToString(),A.QuantityLimit.GetRawValue(),A.MinimumSourceReserve.GetRawValue(),A.CargoSlotIndex});}
             if(!Current||Hansa::UI::TradeRoutePlanKey(Intent.RouteId,CurrentStops)!=Intent.ExpectedRoutePlanKey)return Reject(Intent,EHansaClientCommandRejection::StaleProjection,TEXT("The route plan changed while you edited it."),TEXT("Your draft was not applied. Discard edits to load the current plan, then edit again."),RuntimeHost);
            }
			if (bPayloadValid) Gateway = RuntimeHost->EditRouteForAuthority(Authority, Route.Value, Stops);
			break;
		}
		case EHansaClientIntentType::CancelRoute:
		{
			const auto Route = Intent.RouteId > 0
				? FHansaRouteId::TryCreate(static_cast<uint64>(Intent.RouteId))
				: THansaValueResult<FHansaRouteId>::Failure(EHansaValueError::InvalidZero);
			bPayloadValid = Route.IsSuccess();
			if (bPayloadValid) Gateway = RuntimeHost->CancelRouteForAuthority(Authority, Route.Value);
			break;
		}
		case EHansaClientIntentType::SpotTrade:
		{
			const auto Vehicle = Intent.VehicleId > 0 ? FHansaVehicleId::TryCreate(static_cast<uint64>(Intent.VehicleId)) : THansaValueResult<FHansaVehicleId>::Failure(EHansaValueError::InvalidZero);
			const auto City = FHansaCityDefinitionId::TryParse(Intent.CityId); const auto Good = FHansaGoodId::TryParse(Intent.GoodId);
			bPayloadValid = Vehicle && City && Good && Intent.QuantityMilliUnits > 0 && Intent.QuantityMilliUnits <= 1'000'000'000LL &&
				Intent.ReviewedMarketUpdateTick >= 0 && Intent.ReviewedUnitPriceMilliMarks > 0;
			if (bPayloadValid)
			{
				FHansaSpotTradeCommand Payload; Payload.VehicleId = Vehicle.Value; Payload.CityId = City.Value; Payload.GoodId = Good.Value;
				Payload.Side = Intent.bSpotTradeBuy ? EHansaSpotTradeSide::BuyFromCity : EHansaSpotTradeSide::SellToCity;
				Payload.Quantity = FHansaQuantity::FromRaw(Intent.QuantityMilliUnits); Payload.ReviewedMarketUpdateTick = Intent.ReviewedMarketUpdateTick;
				Payload.ReviewedUnitPriceMilliMarks = Intent.ReviewedUnitPriceMilliMarks; Gateway = RuntimeHost->ExecuteSpotTradeForAuthority(Authority, Payload);
			}
			break;
		}
		case EHansaClientIntentType::ProposeTradeStation:
		{
			const auto City=FHansaCityDefinitionId::TryParse(Intent.CityId);bPayloadValid=City&& !Intent.TradeStationSiteId.IsEmpty()&&Intent.TradeStationSiteId.Len()<=128;
			if(bPayloadValid){FHansaTradeStationId Station;Gateway=RuntimeHost->ProposeTradeStationForAuthority(Authority,City.Value,Intent.TradeStationSiteId,Station);}
			break;
		}
		case EHansaClientIntentType::FundTradeStation:
		{
			const auto Station=Intent.TradeStationId>0?FHansaTradeStationId::TryCreate(static_cast<uint64>(Intent.TradeStationId)):THansaValueResult<FHansaTradeStationId>::Failure(EHansaValueError::InvalidZero);
			const auto Inventory=Intent.FundingInventoryId>0?FHansaInventoryId::TryCreate(static_cast<uint64>(Intent.FundingInventoryId)):THansaValueResult<FHansaInventoryId>::Failure(EHansaValueError::InvalidZero);
			bPayloadValid=Station&&Inventory&&Intent.ConstructionDeliveryMode<=2;if(bPayloadValid)Gateway=RuntimeHost->FundTradeStationForAuthority(Authority,Station.Value,Inventory.Value,Intent.ConstructionDeliveryMode);break;
		}
        case EHansaClientIntentType::ManageStationOrder:
        {
            const auto Station=Intent.TradeStationId>0?FHansaTradeStationId::TryCreate(static_cast<uint64>(Intent.TradeStationId)):THansaValueResult<FHansaTradeStationId>::Failure(EHansaValueError::InvalidZero);
            const auto Good=FHansaGoodId::TryParse(Intent.GoodId);
            bPayloadValid=Station && Intent.StationOrderId>0 && Intent.StationOrderAction<=4 && Intent.StationOrderSide<=1;
            if(bPayloadValid) {
                FHansaManageStationOrderCommand P; P.StationId=Station.Value; P.OrderId=Intent.StationOrderId; P.Action=static_cast<EHansaStationOrderAction>(Intent.StationOrderAction);
                if(Good)P.Terms.GoodId=Good.Value; P.Terms.Side=static_cast<EHansaStationOrderSide>(Intent.StationOrderSide);
                P.Terms.TargetOrReserveMilliUnits=Intent.StationOrderTarget;P.Terms.CapMilliUnits=Intent.StationOrderCap;P.Terms.TotalBudgetPfennig=Intent.StationOrderBudget;P.Terms.LimitUnitPriceMilliMarks=Intent.StationOrderLimitUnitPriceMilliMarks;P.Terms.ReviewedMarketUpdateTick=Intent.StationOrderReviewedMarketUpdateTick;P.Terms.ReviewedUnitPriceMilliMarks=Intent.StationOrderReviewedUnitPriceMilliMarks;
                Gateway=RuntimeHost->ManageStationOrderForAuthority(Authority,P);
            }
            break;
        }
		case EHansaClientIntentType::RequestPresenceUpgrade:
		case EHansaClientIntentType::FundPresenceUpgrade:
		{
			const auto City=FHansaCityDefinitionId::TryParse(Intent.CityId);const auto Inventory=Intent.FundingInventoryId>0?FHansaInventoryId::TryCreate(static_cast<uint64>(Intent.FundingInventoryId)):THansaValueResult<FHansaInventoryId>::Failure(EHansaValueError::InvalidZero);
			bPayloadValid=City&&!Intent.PresenceStageId.IsEmpty()&&Intent.PresenceStageId.Len()<=128&&(Intent.Type==EHansaClientIntentType::RequestPresenceUpgrade||Inventory);
			if(bPayloadValid){if(Intent.Type==EHansaClientIntentType::RequestPresenceUpgrade)Gateway=RuntimeHost->RequestPresenceUpgradeForAuthority(Authority,{City.Value,Intent.PresenceStageId});else Gateway=RuntimeHost->FundPresenceUpgradeForAuthority(Authority,{City.Value,Intent.PresenceStageId,Inventory.Value});}
			break;
		}
		case EHansaClientIntentType::ApplyPresenceSpecialization:
		{
			const auto City=FHansaCityDefinitionId::TryParse(Intent.CityId);const auto Inventory=Intent.FundingInventoryId>0?FHansaInventoryId::TryCreate(static_cast<uint64>(Intent.FundingInventoryId)):THansaValueResult<FHansaInventoryId>::Failure(EHansaValueError::InvalidZero);
			bPayloadValid=City&&Inventory&&!Intent.PresenceSpecializationId.IsEmpty()&&Intent.PresenceSpecializationId.Len()<=64&&Intent.PresenceSpecializationAction<=1&&Intent.PresenceSpecializationRevision>=0;
			if(bPayloadValid)Gateway=RuntimeHost->ApplyPresenceSpecializationForAuthority(Authority,{City.Value,Intent.PresenceSpecializationId,Inventory.Value,static_cast<EHansaPresenceSpecializationAction>(Intent.PresenceSpecializationAction),Intent.PresenceSpecializationRevision});break;
		}		case EHansaClientIntentType::ManageCityPrivilege:
        case EHansaClientIntentType::FundCityProject:
        case EHansaClientIntentType::TransitionCityAuthority:
        {
            const auto City=FHansaCityDefinitionId::TryParse(Intent.CityId);
            const auto Inventory=FHansaInventoryId::TryCreate(Intent.FundingInventoryId>0?uint64(Intent.FundingInventoryId):0);
            const bool NeedsInventory=Intent.Type==EHansaClientIntentType::FundCityProject||(Intent.Type==EHansaClientIntentType::ManageCityPrivilege&&Intent.DecisionAction==0);
            bPayloadValid=City&&!Intent.DecisionId.IsEmpty()&&Intent.DecisionId.Len()<=128&&Intent.AuthorityRevision>=0&&Intent.DecisionAction<=1&&(!NeedsInventory||Inventory);
            if(bPayloadValid){
                if(Intent.Type==EHansaClientIntentType::ManageCityPrivilege)Gateway=RuntimeHost->ManageCityPrivilegeForAuthority(Authority,{City.Value,Intent.DecisionId,Inventory.Value,{},static_cast<EHansaCityPrivilegeAction>(Intent.DecisionAction),Intent.AuthorityRevision});
                else if(Intent.Type==EHansaClientIntentType::FundCityProject)Gateway=RuntimeHost->FundCityProjectForAuthority(Authority,{City.Value,Intent.DecisionId,Inventory.Value,Intent.AuthorityRevision});
                else Gateway=RuntimeHost->TransitionCityAuthorityForAuthority(Authority,{City.Value,Intent.DecisionId,Intent.AuthorityRevision});
            }
            break;
        }
        case EHansaClientIntentType::CloseTradeStation:
		{
			const auto Station=Intent.TradeStationId>0?FHansaTradeStationId::TryCreate(static_cast<uint64>(Intent.TradeStationId)):THansaValueResult<FHansaTradeStationId>::Failure(EHansaValueError::InvalidZero);
			bPayloadValid=Station.IsSuccess();
            if(bPayloadValid&&!Intent.RecoveryReviewKey.IsEmpty()){
                const auto Views=RuntimeHost->BuildTradeRecovery(Client->HouseId);
                const auto* V=Views.FindByPredicate([&](const auto& X){return X.Station==Intent.TradeStationId;});
                bPayloadValid=V&&V->bCanClose&&V->ReviewKey==Intent.RecoveryReviewKey;
            }
            if(bPayloadValid)Gateway=RuntimeHost->CloseTradeStationForAuthority(Authority,Station.Value);break;
		}
		case EHansaClientIntentType::MoveShip:
		{
			const auto Vehicle = Intent.VehicleId > 0
				? FHansaVehicleId::TryCreate(static_cast<uint64>(Intent.VehicleId))
				: THansaValueResult<FHansaVehicleId>::Failure(EHansaValueError::InvalidZero);
			bPayloadValid = Vehicle && FMath::Abs(Intent.TargetX) <= 4096 &&
				FMath::Abs(Intent.TargetY) <= 4096;
			if (bPayloadValid) Gateway = RuntimeHost->MoveShipForAuthority(
				Authority, Vehicle.Value, { Intent.TargetX, Intent.TargetY });
			break;
		}
		default:
			bPayloadValid = false;
			break;
		}
		if (!bPayloadValid)
		{
			--Client->ExpectedClientSequence;
			Client->SeenNonces.Remove(static_cast<uint64>(Intent.ClientNonce));
			return Reject(Intent, EHansaClientCommandRejection::InvalidPayload,
				TEXT("The server rejected an invalid or oversized command payload."),
				TEXT("Refresh the selected object and submit valid stable IDs and bounded values."), RuntimeHost);
		}
		if (!Gateway)
		{
			const bool bOwnership = Gateway.GetError() == EHansaCommandGatewayError::NotAuthorized;
			FHansaClientCommandFeedback Result = Reject(Intent,
				bOwnership ? EHansaClientCommandRejection::NotAuthorized :
					EHansaClientCommandRejection::GatewayRejected,
				bOwnership ? TEXT("Your house does not own the requested target.") :
					TEXT("The authoritative gameplay rules rejected the command."),
				bOwnership ? TEXT("Select an asset owned by your assigned house.") :
					TEXT("Refresh the projection and correct the highlighted requirement."), RuntimeHost);
			Result.GatewayError = LexToString(Gateway.GetError());
			return Result;
		}

		FHansaClientCommandFeedback Result;
		Result.bAccepted = true;
		Result.State = EHansaClientCommandState::Accepted;
		Result.ClientSequence = Intent.ClientSequence;
		Result.ClientNonce = Intent.ClientNonce;
		Result.GatewayError = LexToString(Gateway.GetError());
		Result.Message = TEXT("The server accepted the command.");
		Result.ServerTick = RuntimeHost->GetSimulationTick();
		Result.AcceptedGlobalSequence = static_cast<int64>(RuntimeHost->GetLastProcessedCommandSequence());
		return Result;
	}

	bool FHansaMultiplayerAuthority::IsInterestedInCity(const FClientState& Client, const FString& CityId) const
	{
		return Client.Interest.CityIds.Contains(CityId);
	}

	bool FHansaMultiplayerAuthority::QueryLand(const uint64 PrincipalId,
		const FHansaCityDefinitionId CityId, const FHansaGridCoordinate BoundsMin,
		const FHansaGridCoordinate BoundsMax, FHansaLandQueryResult& OutResult, FString& OutError, bool bCompactSurvey) const
	{
		OutResult = FHansaLandQueryResult();
		const FClientState* Client = Clients.Find(PrincipalId);
		UHansaRuntimeSimulationHost* RuntimeHost = Host.Get();
		if (Client == nullptr || RuntimeHost == nullptr || !RuntimeHost->IsReady())
		{
			OutError = TEXT("Land query requires an admitted client and a ready authority");
			return false;
		}
		if (!CityId.IsValid() || !IsInterestedInCity(*Client, CityId.ToString()))
		{
			OutError = TEXT("City is outside client interest");
			return false;
		}
		OutResult = RuntimeHost->QueryLand(Client->HouseId, CityId, BoundsMin, BoundsMax, bCompactSurvey);
		if (OutResult.Failure != EHansaLandQueryFailure::None)
		{
			OutError = TEXT("Invalid land query bounds or city");
			return false;
		}
		OutError.Reset();
		return true;
	}

	bool FHansaMultiplayerAuthority::CanReadHousePrivate(const FClientState& Client,
		const FHansaHouseId HouseId) const
	{
		return HouseId == Client.HouseId || Client.AuthorizedReportHouses.Contains(HouseId);
	}

	bool FHansaMultiplayerAuthority::BuildProjection(const uint64 PrincipalId,
		const int64 ClientKnownRevision, const bool bForceFullRefresh,
		FHansaClientProjectionSnapshot& OutProjection, FString& OutError)
	{
		UHansaRuntimeSimulationHost* RuntimeHost = Host.Get();
		FClientState* Client = Clients.Find(PrincipalId);
		if (RuntimeHost == nullptr || !RuntimeHost->IsReady() || Client == nullptr)
		{
			OutError = TEXT("The authority host or client registration is unavailable.");
			return false;
		}
		const auto SourceResult = RuntimeHost->BuildProjection();
		if (!SourceResult)
		{
			OutError = TEXT("The authoritative read model could not be built.");
			return false;
		}
		const FHansaSimulationProjection& Source = SourceResult.Value;
		const uint64 BuildStartCycles = FPlatformTime::Cycles64();
		OutProjection = {};
		OutProjection.SchemaVersion = FHansaClientProjectionSnapshot::CurrentSchemaVersion;
		OutProjection.bFullRefresh = bForceFullRefresh || Client->LastProjectionRevision == 0 ||
			ClientKnownRevision != Client->LastProjectionRevision;
		OutProjection.Revision = ++Client->LastProjectionRevision;
		OutProjection.ServerTick = Source.GetClock().GetTick().GetValue();
		OutProjection.AuthoritativeHash = HexHash(Source.GetFingerprint().Value);
		OutProjection.OwnerHouseId = static_cast<int64>(Client->HouseId.GetValue());
        OutProjection.Recoveries = RuntimeHost->BuildTradeRecovery(Client->HouseId);
        OutProjection.VisitingTrade = RuntimeHost->BuildVisitingTradeOffers(Client->HouseId);
        OutProjection.VisitingTrade.RemoveAll([&](const auto& O){return !IsInterestedInCity(*Client,O.City);});
		auto BuildingOwner = [&Source](const FHansaBuildingId BuildingId)
		{
			const FHansaBuildingWorldProjection* Building = Source.GetBuildingWorldProjections().FindByPredicate(
				[BuildingId](const FHansaBuildingWorldProjection& Item) { return Item.BuildingId == BuildingId; });
			return Building != nullptr ? Building->OwnerId : FHansaHouseId();
		};
		auto VehicleOwner = [&Source](const FHansaVehicleId VehicleId)
		{
			const FHansaVehicleProjection* Vehicle = Source.GetVehicles().FindByPredicate(
				[VehicleId](const FHansaVehicleProjection& Item) { return Item.Id == VehicleId; });
			return Vehicle != nullptr ? Vehicle->OwnerId : FHansaHouseId();
		};
		auto InventoryOwner = [&Source, &BuildingOwner, &VehicleOwner](const FHansaInventoryId InventoryId)
		{
			const FHansaInventoryProjection* Inventory = Source.GetInventories().FindByPredicate(
				[InventoryId](const FHansaInventoryProjection& Item) { return Item.Id == InventoryId; });
			if (Inventory == nullptr) return FHansaHouseId();
			if (Inventory->BuildingId.IsValid()) return BuildingOwner(Inventory->BuildingId);
			if (Inventory->VehicleId.IsValid()) return VehicleOwner(Inventory->VehicleId);
			return FHansaHouseId();
		};

		if (const FHansaHouseProjection* House = Source.GetHouses().FindByPredicate(
			[Client](const FHansaHouseProjection& Item) { return Item.Id == Client->HouseId; }))
		{
			OutProjection.OwnerMoneyPfennig = House->Money.GetRawValue();
		}

		for (const FHansaBuildingWorldProjection& Item : Source.GetBuildingWorldProjections())
		{
			if (!IsInterestedInCity(*Client, Item.Placement.CityId.ToString())) continue;
			FHansaReplicatedPlacement& Dest = OutProjection.Placements.AddDefaulted_GetRef();
			Dest.BuildingId = static_cast<int64>(Item.BuildingId.GetValue());
			Dest.OwnerHouseId = static_cast<int64>(Item.OwnerId.GetValue());
			Dest.CityId = Item.Placement.CityId.ToString();
			Dest.BuildingDefinitionId = Item.Placement.BuildingDefinitionId.ToString();
			Dest.AnchorX = Item.Placement.Anchor.X;
			Dest.AnchorY = Item.Placement.Anchor.Y;
			Dest.Status = LexToString(Item.Status);
			Dest.ProgressPartsPerMillion = Item.ConstructionProgress.GetPartsPerMillion();
		}
		for (const FHansaCityMarketProjection& Item : Source.GetMarkets())
		{
			if (!IsInterestedInCity(*Client, Item.CityId.ToString())) continue;
			FHansaReplicatedMarket& Dest = OutProjection.Markets.AddDefaulted_GetRef();
			Dest.CityId = Item.CityId.ToString();
			Dest.GoodId = Item.GoodId.ToString();
			Dest.StockMilliUnits = Item.CurrentStock.GetRawValue();
			Dest.DesiredReserveMilliUnits = Item.DesiredReserve.GetRawValue();
			Dest.CurrentPriceMilliMarks = Item.CurrentPriceMilliMarks;
			Dest.ReportAgeTicks = Item.ReportAgeTicks;
			Dest.bStale = Item.bIsStale;
			Dest.RecentAveragePriceMilliMarks = Item.RecentAveragePriceMilliMarks;
			Dest.CitizenDemandMilliUnits = Item.CitizenDemand.GetRawValue();
			Dest.IndustrialDemandMilliUnits = Item.IndustrialDemand.GetRawValue();
			Dest.RecentLocalProductionMilliUnits = Item.RecentLocalProduction.GetRawValue();
			Dest.ExpectedIncomingSupplyMilliUnits = Item.ExpectedIncomingSupply.GetRawValue();
			Dest.UnmetDemandMilliUnits = Item.UnmetDemand.GetRawValue();
			Dest.HistoryTotalCount = Item.PriceHistory.Num();
			Dest.HistoryOffset = Client->Interest.HistoryOffset;
			const int32 HistoryEnd = FMath::Max(0, Item.PriceHistory.Num() - Client->Interest.HistoryOffset);
			const int32 HistoryStart = FMath::Max(0, HistoryEnd - Client->Interest.HistoryPageSize);
			for (int32 Index = HistoryStart; Index < HistoryEnd; ++Index)
			{
				Dest.HistoryTicks.Add(Item.PriceHistory[Index].Tick.GetValue());
				Dest.HistoryPriceMilliMarks.Add(Item.PriceHistory[Index].PriceMilliMarks);
			}
			Dest.bHistoryHasMore = HistoryStart > 0;
            // Foreign market UI receives lawful report values, never live hidden stock.
            if(Item.CityId!=RuntimeHost->GetCityId()){
             const auto K=RuntimeHost->QueryKnownMarketPrice(Item.CityId,Item.GoodId,Client->HouseId);const auto S=RuntimeHost->QueryKnownMarketSupply(Item.CityId,Item.GoodId,Client->HouseId);
             Dest.CurrentPriceMilliMarks=K&&K->PriceMilliMarks?K->PriceMilliMarks.GetValue():0;Dest.ReportAgeTicks=K&&K->ReportAgeTicks?K->ReportAgeTicks.GetValue():-1;Dest.bStale=!K||K->InformationState!=EHansaMarketInformationState::Current;
             Dest.StockMilliUnits=S&&S->Stock?S->Stock->GetRawValue():-1;Dest.DesiredReserveMilliUnits=S&&S->DesiredReserve?S->DesiredReserve->GetRawValue():-1;
             Dest.CitizenDemandMilliUnits=Dest.IndustrialDemandMilliUnits=Dest.RecentLocalProductionMilliUnits=Dest.ExpectedIncomingSupplyMilliUnits=Dest.UnmetDemandMilliUnits=0;Dest.HistoryPriceMilliMarks.Reset();Dest.HistoryTicks.Reset();Dest.HistoryTotalCount=0;Dest.bHistoryHasMore=false;
            }
		}
		for (const FHansaRouteProjection& Item : Source.GetRoutes())
		{
			const FHansaVehicleProjection* Vehicle = Source.GetVehicles().FindByPredicate(
				[&Item](const FHansaVehicleProjection& Candidate) { return Candidate.Id == Item.VehicleId; });
			const FString CurrentCity = Vehicle != nullptr ? Vehicle->CurrentCityId.ToString() : FString();
			const bool bPrivate = CanReadHousePrivate(*Client, Item.OwnerId);
			if (!bPrivate && !IsInterestedInCity(*Client, CurrentCity)) continue;
			FHansaReplicatedRoute& Dest = OutProjection.Routes.AddDefaulted_GetRef();
			Dest.RouteId = static_cast<int64>(Item.Id.GetValue());
			Dest.OwnerHouseId = static_cast<int64>(Item.OwnerId.GetValue());
			Dest.VehicleId = static_cast<int64>(Item.VehicleId.GetValue());
			Dest.Mode = LexToString(Item.Mode);
			Dest.Lifecycle = LexToString(Item.Lifecycle);
			Dest.CurrentCityId = CurrentCity;
			Dest.RemainingTravelTicks = bPrivate ? Item.RemainingTravelTicks : 0;
			Dest.bCargoVisible = bPrivate;
			Dest.CargoMilliUnits = bPrivate && Vehicle != nullptr ? Vehicle->Cargo.GetRawValue() : 0;
			Dest.CapacityMilliUnits = bPrivate && Vehicle != nullptr ? Vehicle->Capacity.GetRawValue() : 0;
			Dest.FreeCapacityMilliUnits = bPrivate && Vehicle != nullptr ? Vehicle->FreeCapacity.GetRawValue() : 0;
			Dest.TotalTravelTicks = bPrivate ? Item.TotalTravelTicks : 0;
			Dest.ProgressPartsPerMillion = bPrivate ? Item.Progress.GetPartsPerMillion() : 0;
			Dest.CompletedLegCount = bPrivate ? Item.CompletedLegCount : 0;
            if(Item.OwnerId==Client->HouseId){
             Dest.DefinitionId=Item.RouteDefinitionId.ToString();Dest.Label=RuntimeHost->GetRouteLabel(Item.Id.GetValue());
             for(const auto& Stop:Item.Stops){auto& D=Dest.Stops.AddDefaulted_GetRef();D.CityId=Stop.CityId.ToString();for(const auto& A:Stop.Actions)D.Actions.Add({uint8(A.Kind),A.GoodId.ToString(),A.QuantityLimit.GetRawValue(),A.MinimumSourceReserve.GetRawValue(),A.CargoSlotIndex});}
             Dest.PlanKey=Hansa::UI::TradeRoutePlanKey(Dest.RouteId,Dest.Stops);
            }
		}
		for (const FHansaVehicleProjection& Item : Source.GetVehicles())
		{
			const bool bPrivate = CanReadHousePrivate(*Client, Item.OwnerId);
			if (!bPrivate && !IsInterestedInCity(*Client, Item.CurrentCityId.ToString())) continue;
			FHansaReplicatedVehicle& Dest = OutProjection.Vehicles.AddDefaulted_GetRef();
			Dest.VehicleId = static_cast<int64>(Item.Id.GetValue());
			Dest.OwnerHouseId = static_cast<int64>(Item.OwnerId.GetValue());
			Dest.DefinitionId = Item.DefinitionId.ToString();
			Dest.Mode = LexToString(Item.Mode);
			Dest.CurrentCityId = Item.CurrentCityId.ToString();
			Dest.bPrivateDetailsVisible = bPrivate;
			Dest.CargoMilliUnits = bPrivate ? Item.Cargo.GetRawValue() : 0;
			Dest.CapacityMilliUnits = bPrivate ? Item.Capacity.GetRawValue() : 0;
            if (bPrivate) for (const auto& Inventory : Source.GetInventories()) if (Inventory.Id == Item.CargoInventoryId) for (const auto& Slot : Inventory.CargoSlots) Dest.CargoSlots.Add({Slot.GoodId.ToString(), Slot.Quantity.GetRawValue()});
		}
		for (const FHansaInventoryProjection& Item : Source.GetInventories())
		{
			const FHansaHouseId Owner = InventoryOwner(Item.Id);
			if (!Owner.IsValid() || !CanReadHousePrivate(*Client, Owner) ||
				!IsInterestedInCity(*Client, Item.CityId.ToString())) continue;
			FHansaReplicatedInventory& Dest = OutProjection.Inventories.AddDefaulted_GetRef();
			Dest.InventoryId = static_cast<int64>(Item.Id.GetValue());
			Dest.OwnerHouseId = static_cast<int64>(Owner.GetValue());
			Dest.OwnerKind = FString::FromInt(static_cast<int32>(Item.OwnerKind));
			Dest.CityId = Item.CityId.ToString();
			Dest.BuildingId = static_cast<int64>(Item.BuildingId.GetValue());
			Dest.VehicleId = static_cast<int64>(Item.VehicleId.GetValue());
			Dest.CapacityMilliUnits = Item.Capacity.GetRawValue();
			Dest.UsedCapacityMilliUnits = Item.UsedCapacity.GetRawValue();
			Dest.ReservedMilliUnits = Item.Reserved.GetRawValue();
			for (const FHansaInventoryStockProjection& Stock : Item.Stocks)
			{
				FHansaReplicatedInventoryStock& StockDest = Dest.Stocks.AddDefaulted_GetRef();
				StockDest.GoodId = Stock.GoodId.ToString();
				StockDest.QuantityMilliUnits = Stock.Stock.GetRawValue();
				StockDest.ReservedMilliUnits = Stock.Reserved.GetRawValue();
			}
		}
		for (const FHansaProductionProjection& Item : Source.GetProductions())
		{
			const FHansaHouseId Owner = BuildingOwner(Item.BuildingId);
			if (!CanReadHousePrivate(*Client, Owner) || !IsInterestedInCity(*Client, Item.CityId.ToString())) continue;
			FHansaReplicatedProduction& Dest = OutProjection.Productions.AddDefaulted_GetRef();
			Dest.ProductionId = static_cast<int64>(Item.Id.GetValue());
			Dest.OwnerHouseId = static_cast<int64>(Owner.GetValue());
			Dest.BuildingId = static_cast<int64>(Item.BuildingId.GetValue());
			Dest.CityId = Item.CityId.ToString();
			Dest.RecipeId = Item.RecipeId.ToString();
			Dest.bActive = Item.bActive;
			Dest.ProgressTicks = Item.ProgressTicks;
			Dest.CycleTicks = Item.CycleTicks;
			Dest.CompletedCycles = static_cast<int64>(Item.CompletedCycles);
			Dest.Blocker = LexToString(Item.Blocker);
			Dest.BlockingGoodId = Item.BlockingGoodId.ToString();
			Dest.BlockingRequiredMilliUnits = Item.BlockingRequiredQuantity.GetRawValue();
			Dest.BlockingAvailableMilliUnits = Item.BlockingAvailableQuantity.GetRawValue();
		}
		for (const FHansaPopulationCohortProjection& Item : Source.GetPopulationCohorts())
		{
			const FHansaHouseId Owner = BuildingOwner(Item.ResidenceBuildingId);
			if (!CanReadHousePrivate(*Client, Owner) || !IsInterestedInCity(*Client, Item.CityId.ToString())) continue;
			FHansaReplicatedPopulationCohort& Dest = OutProjection.PopulationCohorts.AddDefaulted_GetRef();
			Dest.CohortId = static_cast<int64>(Item.Id.GetValue());
			Dest.OwnerHouseId = static_cast<int64>(Owner.GetValue());
			Dest.ResidenceBuildingId = static_cast<int64>(Item.ResidenceBuildingId.GetValue());
			Dest.CityId = Item.CityId.ToString();
			Dest.TierId = Item.TierId.ToString();
			Dest.Residents = Item.Residents;
			Dest.ResidenceCapacity = Item.ResidenceCapacity;
			Dest.WorkforceSupply = Item.WorkforceSupply;
			Dest.SatisfactionBasisPoints = Item.SatisfactionBasisPoints;
			for (const FHansaPopulationNeedState& Need : Item.Needs)
			{
				FHansaReplicatedPopulationNeed& NeedDest = Dest.Needs.AddDefaulted_GetRef();
				NeedDest.NeedId = Need.NeedId.ToString();
				NeedDest.SatisfactionBasisPoints = Need.SatisfactionBasisPoints;
				NeedDest.bSatisfied = Need.SatisfactionBasisPoints >= 10000;
			}
		}
		for (const FHansaCityPopulationProjection& Item : Source.GetCityPopulations())
		{
			if (!IsInterestedInCity(*Client, Item.CityId.ToString())) continue;
			FHansaReplicatedCitySummary& Dest = OutProjection.CitySummaries.AddDefaulted_GetRef();
			Dest.CityId = Item.CityId.ToString();
			Dest.TotalResidents = Item.TotalResidents;
			Dest.HousingCapacity = Item.HousingCapacity;
			Dest.LaborerResidents = Item.LaborerResidents;
			Dest.ArtisanResidents = Item.ArtisanResidents;
			Dest.WorkforceAvailable = Item.LaborerWorkforceAvailable + Item.ArtisanWorkforceAvailable;
			Dest.SatisfactionBasisPoints = Item.SatisfactionBasisPoints;
			Dest.StapleReserveMilliDays = Item.StapleReserveMilliDays;
		}
		for (const FHansaLogisticsJobProjection& Item : Source.GetLogisticsJobs())
		{
			const FHansaHouseId Owner = InventoryOwner(Item.SourceInventoryId);
			if (!CanReadHousePrivate(*Client, Owner)) continue;
			const FHansaInventoryProjection* SourceInventory = Source.GetInventories().FindByPredicate(
				[&Item](const FHansaInventoryProjection& Candidate) { return Candidate.Id == Item.SourceInventoryId; });
			const FString CityId = SourceInventory != nullptr ? SourceInventory->CityId.ToString() : FString();
			if (!IsInterestedInCity(*Client, CityId)) continue;
			FHansaReplicatedLogisticsJob& Dest = OutProjection.LogisticsJobs.AddDefaulted_GetRef();
			Dest.JobId = static_cast<int64>(Item.Id.GetValue());
			Dest.OwnerHouseId = static_cast<int64>(Owner.GetValue());
			Dest.CityId = CityId;
			Dest.GoodId = Item.GoodId.ToString();
			Dest.QuantityMilliUnits = Item.Quantity.GetRawValue();
			Dest.CargoMilliUnits = Item.CargoQuantity.GetRawValue();
			Dest.RemainingTravelTicks = Item.RemainingTravelTicks;
			Dest.Status = LexToString(Item.Status);
		}
		if (const FHansaHouseResearchState* Research = Source.GetResearch().FindByPredicate(
			[Client](const FHansaHouseResearchState& Item) { return Item.HouseId == Client->HouseId; }))
		{
			OutProjection.Research.HouseId = static_cast<int64>(Research->HouseId.GetValue());
			OutProjection.Research.AvailableResearchPoints = Research->AvailableResearchPoints;
			OutProjection.Research.ActiveTechnologyId = Research->ActiveTechnologyId;
			OutProjection.Research.ProgressTicks = Research->ProgressTicks;
			OutProjection.Research.CompletedTechnologyIds = Research->CompletedTechnologyIds;
		}
		for (const FHansaHouseResearchState& Research : Source.GetResearch())
		{
			if (!Client->AuthorizedReportHouses.Contains(Research.HouseId)) continue;
			FHansaReplicatedResearch& Dest = OutProjection.AuthorizedResearchReports.AddDefaulted_GetRef();
			Dest.HouseId = static_cast<int64>(Research.HouseId.GetValue());
			Dest.AvailableResearchPoints = Research.AvailableResearchPoints;
			Dest.ActiveTechnologyId = Research.ActiveTechnologyId;
			Dest.ProgressTicks = Research.ProgressTicks;
			Dest.CompletedTechnologyIds = Research.CompletedTechnologyIds;
		}

		if (const FHansaScenarioProgress* Scenario = RuntimeHost->GetScenarioProgress())
		{
			OutProjection.ScenarioOutcome = LexToString(Scenario->Outcome);
			OutProjection.WinningVictoryId = Scenario->WinningVictoryId;
			for (const FHansaVictoryPathProgress& Path : Scenario->VictoryPaths)
			{
				for (const FHansaScenarioObjectiveProgress& Objective : Path.Objectives)
				{
					FHansaReplicatedVictoryObjective& Dest =
						OutProjection.VictoryObjectives.AddDefaulted_GetRef();
					Dest.VictoryId = Path.VictoryId;
					Dest.ObjectiveId = Objective.ObjectiveId;
					Dest.CurrentValue = Objective.CurrentValue;
					Dest.TargetValue = Objective.TargetValue;
					Dest.bMet = Objective.bMet;
				}
			}
		}

		uint64 LastServerEventSequence = Client->LastDeliveredEventSequence;
		for (const FHansaDomainEvent& Event : RuntimeHost->GetEventHistory())
		{
			LastServerEventSequence = FMath::Max(LastServerEventSequence, Event.GetGlobalSequence());
			if (!OutProjection.bFullRefresh && Event.GetGlobalSequence() <= Client->LastDeliveredEventSequence) continue;
			const bool bOwner = Event.GetIssuingHouseId() == Client->HouseId;
			FString City = Event.GetCityId().ToString();
			if (City.IsEmpty()) City = Event.GetPlacement().CityId.ToString();
			if (EventIsResearch(Event.GetType()) && !bOwner) continue;
			if (EventIsPrivateCargo(Event.GetType()) && !bOwner) continue;
			if (!City.IsEmpty() && !bOwner && !IsInterestedInCity(*Client, City)) continue;
			FHansaReplicatedEvent& Dest = OutProjection.Events.AddDefaulted_GetRef();
			Dest.GlobalSequence = static_cast<int64>(Event.GetGlobalSequence());
			Dest.Tick = Event.GetTick().GetValue();
			Dest.Type = LexToString(Event.GetType());
			Dest.IssuingHouseId = static_cast<int64>(Event.GetIssuingHouseId().GetValue());
			Dest.BuildingId = static_cast<int64>(Event.GetBuildingId().GetValue());
			Dest.RouteId = static_cast<int64>(Event.GetRouteId().GetValue());
			Dest.CityId = City;
			Dest.GoodId = Event.GetGoodId().ToString();
			Dest.TechnologyId = Event.GetTechnologyId();
		}
		OutProjection.StartingEventSequence = !OutProjection.Events.IsEmpty()
			? OutProjection.Events[0].GlobalSequence : static_cast<int64>(LastServerEventSequence + 1);
		OutProjection.LastEventSequence = static_cast<int64>(LastServerEventSequence);
		Client->LastDeliveredEventSequence = LastServerEventSequence;
        if(const auto* Registry=RuntimeHost->GetEconomicRegistry())for(const auto& Policy:Registry->GetCityTradePolicies()) {
            const FString& City=Policy.CityId;
            if(!Source.GetForeignPresences().ContainsByPredicate([&](const auto& P){return P.HouseId==Client->HouseId&&P.CityId.ToString()==City;}))continue;
            auto Base=Hansa::UI::BuildTradeEstablishment(Source,*Registry,Client->HouseId,FName(*City),{},{});
            if(!Base.bVisible)continue;
            OutProjection.StationEstablishments.Add(Base);
            if(!Base.StationId)for(const auto& Site:Base.Sites)
                OutProjection.StationEstablishments.Add(Hansa::UI::BuildTradeEstablishment(Source,*Registry,Client->HouseId,FName(*City),Site.Id,{}));
        }
		if(const auto* Registry=RuntimeHost->GetEconomicRegistry())for(const auto& Station:Source.GetTradeStations())if(Station.Station.OwnerId==Client->HouseId){
            const FName City(*Station.Station.CityId.ToString());if(!OutProjection.StationLedgers.ContainsByPredicate([&](const auto& L){return L.City==City;}))OutProjection.StationLedgers.Add(Hansa::UI::BuildTradeLedger(Source,*Registry,Client->HouseId,City));
        }
        if(const auto* Registry=RuntimeHost->GetEconomicRegistry())for(const auto& Station:Source.GetTradeStations())if(Station.Station.OwnerId==Client->HouseId){
            FHansaReplicatedStationOrders View;
            View.City=FName(*Station.Station.CityId.ToString());
            View.StationId=static_cast<int64>(Station.Station.Id.GetValue());
            View.CapacityMilliUnits=Station.StorageCapacity.GetRawValue();
            const auto* Presence=Source.GetForeignPresences().FindByPredicate([&](const auto& X){return X.HouseId==Client->HouseId&&X.CityId==Station.Station.CityId;});
            View.bOperational=Station.Station.Status==EHansaTradeStationStatus::Active&&Presence&&Presence->Status==EHansaForeignPresenceStatus::Active&&Presence->Capabilities.ContainsByPredicate([](const auto& C){return C.CapabilityId==TEXT("PresenceCapability.StationOrders")&&C.bGranted;});
            if(const auto* Policy=Registry->FindCityTradePolicyForCity(Station.Station.CityId.ToString())){
                View.bOperational&=!Policy->DeniedCapabilityIds.Contains(TEXT("PresenceCapability.StationOrders"));
                View.MaximumCapMilliUnits=Policy->MaximumOrderCapMilliUnits;
                View.MaximumBudgetPfennig=Policy->MaximumOrderBudgetPfennig;
            }
            for(const auto& Market:Source.GetMarkets())if(Market.CityId==Station.Station.CityId)
                View.GoodIds.AddUnique(Market.GoodId.ToString());
            View.GoodIds.Sort();
            for(const auto& Order:Station.Station.Orders){
                FHansaReplicatedStationOrder Item;
                Item.Id=static_cast<int64>(Order.Id);Item.GoodId=Order.Terms.GoodId.ToString();
                Item.Side=static_cast<uint8>(Order.Terms.Side);Item.TargetMilliUnits=Order.Terms.TargetOrReserveMilliUnits;
                Item.CapMilliUnits=Order.Terms.CapMilliUnits;Item.BudgetPfennig=Order.Terms.TotalBudgetPfennig;
                Item.LimitUnitPriceMilliMarks=Order.Terms.LimitUnitPriceMilliMarks;
                Item.ReviewedMarketUpdateTick=Order.Terms.ReviewedMarketUpdateTick;
                Item.ReviewedUnitPriceMilliMarks=Order.Terms.ReviewedUnitPriceMilliMarks;
                Item.SpentPfennig=Order.SpentPfennig;Item.NextUpdateTick=Order.NextUpdateTick;
                Item.bPaused=Order.bPaused;Item.bCancelled=Order.bCancelled;
                for(const auto& Event:Order.History){
                    FHansaReplicatedStationOrderExecution E;E.Tick=Event.Tick;
                    E.RequestedMilliUnits=Event.RequestedMilliUnits;E.AppliedMilliUnits=Event.AppliedMilliUnits;
                    E.MoneyDelta=Event.MoneyDelta;E.Outcome=static_cast<uint8>(Event.Outcome);
                    E.Blocker=static_cast<uint8>(Event.Blocker);Item.History.Add(E);
                }
                View.Orders.Add(MoveTemp(Item));
            }
            OutProjection.StationOrders.Add(MoveTemp(View));
        }
        // Presence progression is private to its owning house and replaced in full on each projection.
        if(const auto* Registry=RuntimeHost->GetEconomicRegistry())for(const auto& Presence:Source.GetForeignPresences())if(Presence.HouseId==Client->HouseId){
            FHansaReplicatedPresence View;View.City=FName(*Presence.CityId.ToString());View.Specialization=Hansa::UI::BuildTradeSpecialization(Source,*Registry,Client->HouseId,View.City);View.CurrentStage=Presence.CurrentStageDisplayName;View.Decisions=Hansa::UI::BuildTradeDecisions(Source,*Registry,Client->HouseId,View.City,RuntimeHost->GetAuthorityScenarioId());
            View.bOfficeVisual=Presence.CurrentStageId.Contains(TEXT("MerchantOffice"))||Presence.NextStages.ContainsByPredicate([](const auto& Stage){return Stage.StageId.Contains(TEXT("MerchantOffice"));});
            View.bOfficeBuilt=Presence.Capabilities.ContainsByPredicate([](const auto& Capability){return Capability.CapabilityId==TEXT("PresenceCapability.MerchantOffice")&&Capability.bGranted;});
            View.Status=Presence.Status==EHansaForeignPresenceStatus::Suspended?TEXT("Suspended"):Presence.Status==EHansaForeignPresenceStatus::Revoked?TEXT("Revoked"):TEXT("Active");
            View.UpgradeStatus=static_cast<uint8>(Presence.Upgrade.Status);View.CompletionTick=Presence.Upgrade.CompletionTick.GetValue();
            if(Presence.Upgrade.Status==EHansaPresenceUpgradeStatus::AwaitingMaterials){
                View.ConstructionDelivery=FString::Printf(TEXT("Deliver materials using inventory #%llu in this city. The current station remains operational.\n"),Presence.Upgrade.FundingInventoryId.GetValue());
                if(const auto* Stage=Registry->FindPresenceStage(Presence.Upgrade.TargetStageId))for(const auto& Cost:Stage->UpgradeGoods){const auto* G=Presence.Upgrade.DeliveredGoods.FindByPredicate([&](const auto& X){return X.GoodId.ToString()==Cost.GoodId;});View.ConstructionDelivery+=FString::Printf(TEXT("%s: delivered %.1f / %.1f units\n"),*Cost.GoodId,double(G?G->Quantity.GetRawValue():0)/1000.,double(Cost.QuantityMilliUnits)/1000.);}
            }
            const bool LocalConstruction=Source.GetTradeStations().ContainsByPredicate([&](const auto& S){return S.Station.Id==Presence.StationId&&S.Station.ConstructionSite.bLocalDelivery;});
            const auto* Next=Presence.NextStages.IsEmpty()?nullptr:&Presence.NextStages[0];
            if(Next){
                View.NextStageId=Next->StageId;View.NextStage=Next->DisplayName;View.bProgressMet=Next->bProgressRequirementsMet;
                for(const auto& R:Next->Requirements){auto& V=View.Requirements.AddDefaulted_GetRef();V.Id=R.RequirementId;V.Description=R.Description;V.Current=R.CurrentValue;V.Required=R.RequiredValue;V.bMet=R.bMet;}
                if(const auto* Stage=Registry->FindPresenceStage(Next->StageId)){
                    for(const auto& Id:Stage->GrantedCapabilityIds){const auto* C=Registry->FindPresenceCapability(Id);View.Unlocks+=(C?C->DisplayName:Id)+TEXT("; ");}
                    if(Presence.Upgrade.Status==EHansaPresenceUpgradeStatus::Requested){
                        for(const auto& Inv:Source.GetInventories()){
                            bool Owned=false;FString Label;
                            if(Inv.OwnerKind==EHansaInventoryOwnerKind::TradeStation){Owned=Source.GetTradeStations().ContainsByPredicate([&](const auto& X){return X.Station.InventoryId==Inv.Id&&X.Station.OwnerId==Client->HouseId;});Label=TEXT("Station");}
                            else if(Inv.OwnerKind==EHansaInventoryOwnerKind::Vehicle){Owned=Source.GetVehicles().ContainsByPredicate([&](const auto& X){return X.Id==Inv.VehicleId&&X.OwnerId==Client->HouseId;});Label=TEXT("Ship or vehicle");}
                            else if(Inv.OwnerKind==EHansaInventoryOwnerKind::Building||Inv.OwnerKind==EHansaInventoryOwnerKind::Warehouse){Owned=Source.GetBuildingWorldProjections().ContainsByPredicate([&](const auto& X){return X.BuildingId==Inv.BuildingId&&X.OwnerId==Client->HouseId;});Label=TEXT("Building or warehouse");}
                            if(!Owned)continue;
                            FHansaEstablishmentChoice C;C.Id=LexToString(Inv.Id.GetValue());C.Label=FText::FromString(FString::Printf(TEXT("%s · inventory #%llu"),*Label,Inv.Id.GetValue()));C.bEligible=OutProjection.OwnerMoneyPfennig>=Stage->UpgradeCostPfennig&&Presence.Status==EHansaForeignPresenceStatus::Active;
                            FString Detail=FString::Printf(TEXT("Treasury %lld pfennig · spend %lld · afterward %lld.\n"),OutProjection.OwnerMoneyPfennig,Stage->UpgradeCostPfennig,OutProjection.OwnerMoneyPfennig-Stage->UpgradeCostPfennig);
                            for(const auto& G:Stage->UpgradeGoods){const auto* Stock=Inv.Stocks.FindByPredicate([&](const auto& X){return X.GoodId.ToString()==G.GoodId;});const int64 Available=Stock?Stock->Available.GetRawValue():0;C.bEligible&=LocalConstruction?(Inv.OwnerKind==EHansaInventoryOwnerKind::Vehicle||Inv.CityId==Presence.CityId):Available>=G.QuantityMilliUnits;const auto* Good=Registry->FindGood(G.GoodId);Detail+=FString::Printf(TEXT("%s: need %.1f · available %.1f units.\n"),*(Good?Good->DisplayName:G.GoodId),double(G.QuantityMilliUnits)/1000.,double(Available)/1000.);}
                            if(!C.bEligible)Detail+=TEXT("Insufficient treasury, materials, or access. Replenish this source before review.\n");
                            Detail+=LocalConstruction?TEXT("Pay once on confirmation. Deliver materials in this city to start the upgrade. The current station stays operational."):TEXT("Selected stock and treasury are consumed only after confirmation. Check route and production commitments.");
                            C.Detail=FText::FromString(Detail);C.Transfer=C.Detail;View.Sources.Add(MoveTemp(C));
                        }
                        View.Sources.Sort([](const auto& A,const auto& B){return FCString::Strtoui64(*A.Id,nullptr,10)<FCString::Strtoui64(*B.Id,nullptr,10);});
                    }
                }
            }
            if(LocalConstruction&&Next&&Next->StageId==TEXT("PresenceStage.MerchantOffice")){
                View.Sources=Hansa::UI::BuildMerchantOfficeSources(Source,*Registry,Client->HouseId,View.City);
                if(Presence.Upgrade.Status==EHansaPresenceUpgradeStatus::AwaitingMaterials){
                    const auto* Delivery=View.Sources.FindByPredicate([&](const auto& C){return C.Id==LexToString(Presence.Upgrade.FundingInventoryId.GetValue());});
                    if(Delivery)View.ConstructionDelivery=TEXT("Upgrade paid. ")+Delivery->DeliveryStatus.ToString()+TEXT("\n")+Delivery->Transfer.ToString();
                }
            }
            for(int32 I=FMath::Max(0,Presence.History.Num()-8);I<Presence.History.Num();++I){const auto& E=Presence.History[I];const TCHAR* Kind=E.Kind==EHansaPresenceHistoryKind::UpgradeRequested?TEXT("Review accepted"):E.Kind==EHansaPresenceHistoryKind::UpgradeFunded?TEXT("Funding accepted"):E.Kind==EHansaPresenceHistoryKind::UpgradeCompleted?TEXT("Construction completed"):TEXT("Lawful contribution");View.History+=FString::Printf(TEXT("%s ago · %s%s%s%s\n"),*Hansa::UI::PresenceDuration(FMath::Max<int64>(0,Source.GetClock().GetTick().GetValue()-E.Tick.GetValue()),Source.GetClock().GetMinutesPerTick()).ToString(),Kind,E.QuantityMilliUnits?*FString::Printf(TEXT(" · %.1f units"),double(E.QuantityMilliUnits)/1000.):TEXT(""),E.MoneyPfennig?*FString::Printf(TEXT(" · %lld pfennig"),E.MoneyPfennig):TEXT(""),E.SourceEventSequence?*FString::Printf(TEXT(" · event #%llu"),E.SourceEventSequence):TEXT(""));}
            // Each report is scoped to this owner and a specific lease. Include empty reports
            // so revocation/removal replaces earlier client data rather than retaining it.
            auto Construction=Hansa::UI::BuildTradeConstruction(Source,*Registry,Client->HouseId,View.City,0,NAME_None);
            const auto Plots=Construction.Plots;
            if(Plots.Num()>FHansaClientProjectionSnapshot::MaximumCollectionEntries)
            {
                OutProjection={};OutError=TEXT("The authorized construction report exceeded its lease limit.");return false;
            }
            View.ConstructionReports.Add(MoveTemp(Construction));
            for(const auto& Plot:Plots)
                if(Plot.Id!=View.ConstructionReports[0].SelectedLease)
                {
                    auto Report=Hansa::UI::BuildTradeConstruction(Source,*Registry,Client->HouseId,View.City,Plot.Id,NAME_None);
                    // Common plot geometry travels once; per-plot permissions/costs are separate.
                    Report.Plots.Reset();View.ConstructionReports.Add(MoveTemp(Report));
                }
            OutProjection.Presences.Add(MoveTemp(View));
        }
        OutProjection.TradeWorkspace=Hansa::UI::BuildRemoteTradeWorkspace(*RuntimeHost,Source,OutProjection);
        FHansaClientProjectionSnapshot FullAuthorizedView = OutProjection;
		if (!OutProjection.bFullRefresh && Client->bHasLastFullProjection)
		{
            if(FHansaReplicatedTradeWorkspace::StaticStruct()->CompareScriptStruct(&Client->LastFullProjection.TradeWorkspace,&OutProjection.TradeWorkspace,0)){OutProjection.TradeWorkspace={};OutProjection.bTradeWorkspaceIncluded=false;}
			MakeDeltaCollection(Client->LastFullProjection.Placements, OutProjection.Placements,
				TEXT("placements"), [](const FHansaReplicatedPlacement& V) { return LexToString(V.BuildingId); }, OutProjection.Removed);
			MakeDeltaCollection(Client->LastFullProjection.Markets, OutProjection.Markets,
				TEXT("markets"), [](const FHansaReplicatedMarket& V) { return V.CityId + TEXT("|") + V.GoodId; }, OutProjection.Removed);
			MakeDeltaCollection(Client->LastFullProjection.Routes, OutProjection.Routes,
				TEXT("routes"), [](const FHansaReplicatedRoute& V) { return LexToString(V.RouteId); }, OutProjection.Removed);
			MakeDeltaCollection(Client->LastFullProjection.Inventories, OutProjection.Inventories,
				TEXT("inventories"), [](const FHansaReplicatedInventory& V) { return LexToString(V.InventoryId); }, OutProjection.Removed);
			MakeDeltaCollection(Client->LastFullProjection.Productions, OutProjection.Productions,
				TEXT("productions"), [](const FHansaReplicatedProduction& V) { return LexToString(V.ProductionId); }, OutProjection.Removed);
			MakeDeltaCollection(Client->LastFullProjection.PopulationCohorts, OutProjection.PopulationCohorts,
				TEXT("population"), [](const FHansaReplicatedPopulationCohort& V) { return LexToString(V.CohortId); }, OutProjection.Removed);
			MakeDeltaCollection(Client->LastFullProjection.CitySummaries, OutProjection.CitySummaries,
				TEXT("cities"), [](const FHansaReplicatedCitySummary& V) { return V.CityId; }, OutProjection.Removed);
			MakeDeltaCollection(Client->LastFullProjection.Vehicles, OutProjection.Vehicles,
				TEXT("vehicles"), [](const FHansaReplicatedVehicle& V) { return LexToString(V.VehicleId); }, OutProjection.Removed);
			MakeDeltaCollection(Client->LastFullProjection.LogisticsJobs, OutProjection.LogisticsJobs,
				TEXT("logistics"), [](const FHansaReplicatedLogisticsJob& V) { return LexToString(V.JobId); }, OutProjection.Removed);
			MakeDeltaCollection(Client->LastFullProjection.AuthorizedResearchReports,
				OutProjection.AuthorizedResearchReports, TEXT("reports"),
				[](const FHansaReplicatedResearch& V) { return LexToString(V.HouseId); }, OutProjection.Removed);
		}
		Client->LastFullProjection = MoveTemp(FullAuthorizedView);
		Client->bHasLastFullProjection = true;

		const int32 Maximum = FHansaClientProjectionSnapshot::MaximumCollectionEntries;
		if (OutProjection.Presences.Num() > Maximum || OutProjection.StationOrders.Num() > Maximum || OutProjection.StationLedgers.Num() > Maximum || OutProjection.StationEstablishments.Num() > Maximum || OutProjection.Placements.Num() > Maximum || OutProjection.Markets.Num() > Maximum ||
			OutProjection.Routes.Num() > Maximum || OutProjection.Inventories.Num() > Maximum ||
			OutProjection.Productions.Num() > Maximum || OutProjection.PopulationCohorts.Num() > Maximum ||
			OutProjection.CitySummaries.Num() > Maximum || OutProjection.Vehicles.Num() > Maximum ||
			OutProjection.LogisticsJobs.Num() > Maximum || OutProjection.Events.Num() > Maximum)
		{
			OutProjection = {};
			OutError = TEXT("The authorized projection exceeded a bounded collection limit.");
			return false;
		}
		TArray<uint8> AuthorizedSerialized;
		SerializeProjectionForDiagnostics(Client->LastFullProjection, AuthorizedSerialized);
		uint64 AuthorizedDigest = FnvOffset;
		for (const uint8 Byte : AuthorizedSerialized) HashByte(AuthorizedDigest, Byte);
		OutProjection.AuthorizedViewDigest = HexHash(AuthorizedDigest);
		TArray<uint8> Serialized;
		SerializeProjectionForDiagnostics(OutProjection, Serialized);
		OutProjection.SerializedBytes = Serialized.Num();
		uint64 TransportDigest = FnvOffset;
		for (const uint8 Byte : Serialized) HashByte(TransportDigest, Byte);
		OutProjection.ProjectionDigest = HexHash(TransportDigest);
		OutProjection.BuildMicroseconds = static_cast<int64>(FPlatformTime::ToSeconds64(
			FPlatformTime::Cycles64() - BuildStartCycles) * 1'000'000.0);
		return true;
	}
}
