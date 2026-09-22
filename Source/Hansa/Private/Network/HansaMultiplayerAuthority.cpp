#include "Network/HansaMultiplayerAuthority.h"

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
			HashInteger(Hash, Item.VehicleId); HashString(Hash, Item.Mode); HashString(Hash, Item.Lifecycle);
			HashString(Hash, Item.CurrentCityId); HashInteger(Hash, Item.RemainingTravelTicks);
			HashInteger(Hash, static_cast<uint8>(Item.bCargoVisible)); HashInteger(Hash, Item.CargoMilliUnits);
		}
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
		if (Intent.ExpectedServerTick < 0 || Intent.ExpectedServerTick > ServerTick ||
			(Intent.ExpectedServerTick > 0 && ServerTick - Intent.ExpectedServerTick > MaximumAcceptedTickLag))
		{
			return Reject(Intent, EHansaClientCommandRejection::StaleProjection,
				TEXT("The selected state is too old for this command."),
				TEXT("Refresh the projection, review the current target, and submit again."), RuntimeHost);
		}

		++Client->ExpectedClientSequence;
		Client->SeenNonces.Add(static_cast<uint64>(Intent.ClientNonce));
		if (Client->SeenNonces.Num() > 256) Client->SeenNonces.Reset();

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
			bPayloadValid=Station&&Inventory;if(bPayloadValid)Gateway=RuntimeHost->FundTradeStationForAuthority(Authority,Station.Value,Inventory.Value);break;
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
		}		case EHansaClientIntentType::CloseTradeStation:
		{
			const auto Station=Intent.TradeStationId>0?FHansaTradeStationId::TryCreate(static_cast<uint64>(Intent.TradeStationId)):THansaValueResult<FHansaTradeStationId>::Failure(EHansaValueError::InvalidZero);
			bPayloadValid=Station.IsSuccess();if(bPayloadValid)Gateway=RuntimeHost->CloseTradeStationForAuthority(Authority,Station.Value);break;
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
			Dest.RemainingTravelTicks = Item.RemainingTravelTicks;
			Dest.bCargoVisible = bPrivate;
			Dest.CargoMilliUnits = bPrivate && Vehicle != nullptr ? Vehicle->Cargo.GetRawValue() : 0;
			Dest.CapacityMilliUnits = bPrivate && Vehicle != nullptr ? Vehicle->Capacity.GetRawValue() : 0;
			Dest.FreeCapacityMilliUnits = bPrivate && Vehicle != nullptr ? Vehicle->FreeCapacity.GetRawValue() : 0;
			Dest.TotalTravelTicks = Item.TotalTravelTicks;
			Dest.ProgressPartsPerMillion = Item.Progress.GetPartsPerMillion();
			Dest.CompletedLegCount = Item.CompletedLegCount;
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
		FHansaClientProjectionSnapshot FullAuthorizedView = OutProjection;
		if (!OutProjection.bFullRefresh && Client->bHasLastFullProjection)
		{
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
		if (OutProjection.Placements.Num() > Maximum || OutProjection.Markets.Num() > Maximum ||
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
