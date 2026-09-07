#include "Network/HansaMultiplayerAuthority.h"

#include "Commands/HansaGameplayCommand.h"
#include "Events/HansaDomainEvent.h"
#include "Placement/HansaPlacement.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Scenario/HansaScenario.h"
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

	FHansaClientCommandFeedback Reject(const FHansaClientCommandIntent& Intent,
		const EHansaClientCommandRejection Rejection, const FString& Message, const FString& Remedy,
		const UHansaRuntimeSimulationHost* Host)
	{
		FHansaClientCommandFeedback Result;
		Result.ClientSequence = Intent.ClientSequence;
		Result.ClientNonce = Intent.ClientNonce;
		Result.Rejection = Rejection;
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

	bool FHansaMultiplayerAuthority::RegisterClient(const uint64 PrincipalId, const FHansaHouseId HouseId,
		const FHansaClientInterest& Interest, FString& OutError)
	{
		UHansaRuntimeSimulationHost* RuntimeHost = Host.Get();
		if (RuntimeHost == nullptr || !RuntimeHost->IsReady() || PrincipalId == 0 || !HouseId.IsValid())
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
		FClientState State;
		State.HouseId = HouseId;
		State.Interest = MoveTemp(EffectiveInterest);
		Clients.Add(PrincipalId, MoveTemp(State));
		return true;
	}

	void FHansaMultiplayerAuthority::UnregisterClient(const uint64 PrincipalId)
	{
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
			const auto CityId = FHansaCityDefinitionId::TryParse(Intent.CityId);
			const auto BuildingId = FHansaBuildingTypeId::TryParse(Intent.BuildingDefinitionId);
			bPayloadValid = CityId && BuildingId && Intent.Rotation <= static_cast<uint8>(EHansaGridRotation::West) &&
				FMath::Abs(Intent.AnchorX) <= 4096 && FMath::Abs(Intent.AnchorY) <= 4096 &&
				Intent.CityId.Len() <= 128 && Intent.BuildingDefinitionId.Len() <= 128;
			if (bPayloadValid)
			{
				FHansaPlacementSpec Spec;
				Spec.CityId = CityId.Value;
				Spec.BuildingDefinitionId = BuildingId.Value;
				Spec.Anchor = { Intent.AnchorX, Intent.AnchorY };
				Spec.Rotation = static_cast<EHansaGridRotation>(Intent.Rotation);
				Gateway = RuntimeHost->PlaceBuildingsForAuthority(Authority, MakeArrayView(&Spec, 1));
			}
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
		OutProjection = {};
		OutProjection.SchemaVersion = FHansaClientProjectionSnapshot::CurrentSchemaVersion;
		OutProjection.bFullRefresh = bForceFullRefresh || Client->LastProjectionRevision == 0 ||
			ClientKnownRevision != Client->LastProjectionRevision;
		OutProjection.Revision = ++Client->LastProjectionRevision;
		OutProjection.ServerTick = Source.GetClock().GetTick().GetValue();
		OutProjection.AuthoritativeHash = HexHash(Source.GetFingerprint().Value);
		OutProjection.OwnerHouseId = static_cast<int64>(Client->HouseId.GetValue());

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
		}
		for (const FHansaRouteProjection& Item : Source.GetRoutes())
		{
			const FHansaVehicleProjection* Vehicle = Source.GetVehicles().FindByPredicate(
				[&Item](const FHansaVehicleProjection& Candidate) { return Candidate.Id == Item.VehicleId; });
			const FString CurrentCity = Vehicle != nullptr ? Vehicle->CurrentCityId.ToString() : FString();
			const bool bOwner = Item.OwnerId == Client->HouseId;
			if (!bOwner && !IsInterestedInCity(*Client, CurrentCity)) continue;
			FHansaReplicatedRoute& Dest = OutProjection.Routes.AddDefaulted_GetRef();
			Dest.RouteId = static_cast<int64>(Item.Id.GetValue());
			Dest.OwnerHouseId = static_cast<int64>(Item.OwnerId.GetValue());
			Dest.VehicleId = static_cast<int64>(Item.VehicleId.GetValue());
			Dest.Mode = LexToString(Item.Mode);
			Dest.Lifecycle = LexToString(Item.Lifecycle);
			Dest.CurrentCityId = CurrentCity;
			Dest.RemainingTravelTicks = Item.RemainingTravelTicks;
			Dest.bCargoVisible = bOwner;
			Dest.CargoMilliUnits = bOwner && Vehicle != nullptr ? Vehicle->Cargo.GetRawValue() : 0;
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
		OutProjection.ProjectionDigest = HexHash(ProjectionDigest(OutProjection));
		return true;
	}
}
