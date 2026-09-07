#include "Definitions/HansaTradeDefinitions.h"

#include "Model/HansaIds.h"

namespace
{
	void AddTradeIssue(TArray<FHansaDefinitionValidationIssue>& OutIssues, const FName Code,
		const TCHAR* Path, const FText& Cause, const FText& Remedy)
	{
		OutIssues.Add({ EHansaDefinitionValidationSeverity::Error, Code, Path, Cause, Remedy });
	}

	FString ConnectionKey(const FHansaRouteConnectionDefinition& Connection)
	{
		return Connection.SourceCityId < Connection.DestinationCityId
			? Connection.SourceCityId + TEXT("|") + Connection.DestinationCityId
			: Connection.DestinationCityId + TEXT("|") + Connection.SourceCityId;
	}
}

UHansaVehicleDefinition::UHansaVehicleDefinition()
{
	SchemaVersion = 1;
	DefinitionCategory = TEXT("Trade Vehicles");
}

void UHansaVehicleDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	if (!Hansa::Simulation::FHansaVehicleDefinitionId::TryParse(StableDefinitionId))
	{
		AddTradeIssue(OutIssues, TEXT("HSA-VEHICLE-001"), TEXT("StableDefinitionId"),
			NSLOCTEXT("HansaTradeDefinition", "VehicleDomain", "A vehicle definition requires a Vehicle.* stable identity."),
			NSLOCTEXT("HansaTradeDefinition", "VehicleDomainRemedy", "Assign a canonical Vehicle.* identity."));
	}
	if (CargoCapacityMilliUnits <= 0 || UpkeepPfennigPerTravelTick < 0)
	{
		AddTradeIssue(OutIssues, TEXT("HSA-VEHICLE-002"), TEXT("CargoCapacityMilliUnits"),
			NSLOCTEXT("HansaTradeDefinition", "VehicleRange", "Vehicle capacity must be positive and upkeep cannot be negative."),
			NSLOCTEXT("HansaTradeDefinition", "VehicleRangeRemedy", "Enter a positive capacity and non-negative per-travel-tick upkeep."));
	}
}

void UHansaVehicleDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	InOutCanonicalData += FString::Printf(TEXT("mode=%d\ncapacity=%lld\nupkeep=%lld\n"),
		static_cast<int32>(Mode), static_cast<long long>(CargoCapacityMilliUnits),
		static_cast<long long>(UpkeepPfennigPerTravelTick));
}

UHansaRouteDefinition::UHansaRouteDefinition()
{
	SchemaVersion = 1;
	DefinitionCategory = TEXT("Trade Routes");
}

void UHansaRouteDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	if (!Hansa::Simulation::FHansaRouteDefinitionId::TryParse(StableDefinitionId))
	{
		AddTradeIssue(OutIssues, TEXT("HSA-ROUTE-001"), TEXT("StableDefinitionId"),
			NSLOCTEXT("HansaTradeDefinition", "RouteDomain", "A route definition requires a Route.* stable identity."),
			NSLOCTEXT("HansaTradeDefinition", "RouteDomainRemedy", "Assign a canonical Route.* identity."));
	}
	if (Connections.IsEmpty() || CargoRuleSchemaVersion != 1)
	{
		AddTradeIssue(OutIssues, TEXT("HSA-ROUTE-002"), TEXT("Connections"),
			NSLOCTEXT("HansaTradeDefinition", "RouteEmpty", "A route requires at least one reachable connection and the supported MVP cargo-rule schema."),
			NSLOCTEXT("HansaTradeDefinition", "RouteEmptyRemedy", "Add a city connection and retain cargo-rule schema version 1."));
	}
	TSet<FString> Seen;
	for (const FHansaRouteConnectionDefinition& Connection : Connections)
	{
		if (!Hansa::Simulation::FHansaCityDefinitionId::TryParse(Connection.SourceCityId) ||
			!Hansa::Simulation::FHansaCityDefinitionId::TryParse(Connection.DestinationCityId) ||
			Connection.SourceCityId == Connection.DestinationCityId || Connection.TravelTicks <= 0 ||
			Seen.Contains(ConnectionKey(Connection)))
		{
			AddTradeIssue(OutIssues, TEXT("HSA-ROUTE-003"), TEXT("Connections"),
				NSLOCTEXT("HansaTradeDefinition", "RouteConnection", "A route connection is invalid, self-referential, non-positive, or duplicated."),
				NSLOCTEXT("HansaTradeDefinition", "RouteConnectionRemedy", "Use unique pairs of canonical City.* identities and a positive travel time."));
		}
		Seen.Add(ConnectionKey(Connection));
	}
}

void UHansaRouteDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	InOutCanonicalData += FString::Printf(TEXT("mode=%d\ncargoRuleSchema=%d\n"),
		static_cast<int32>(Mode), CargoRuleSchemaVersion);
	TArray<FHansaRouteConnectionDefinition> Sorted = Connections;
	Sorted.Sort([](const FHansaRouteConnectionDefinition& Left, const FHansaRouteConnectionDefinition& Right)
	{
		return ConnectionKey(Left) < ConnectionKey(Right);
	});
	for (const FHansaRouteConnectionDefinition& Connection : Sorted)
	{
		InOutCanonicalData += FString::Printf(TEXT("connection=%s|%s|%d\n"),
			*Connection.SourceCityId, *Connection.DestinationCityId, Connection.TravelTicks);
	}
}
