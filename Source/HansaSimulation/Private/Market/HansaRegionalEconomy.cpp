#include "Market/HansaRegionalEconomy.h"
#include "Market/HansaRegionalEconomyInternal.h"

namespace Hansa::Simulation
{
	namespace
	{
		const FHansaCompiledCityMarketProfileDefinition* ProfileFor(const FHansaEconomicRegistry& Registry,const FHansaCityDefinitionId City)
		{
			return Registry.FindCityMarket(City.ToString());
		}

		FHansaInventoryId InventoryFor(const TArray<FHansaCityMarketState>& Markets,const FHansaCityDefinitionId City)
		{
			for(const auto& Market:Markets) if(Market.CityId==City&&!Market.InventoryIds.IsEmpty()) return Market.InventoryIds[0];
			return {};
		}

		int64 Available(const FHansaInventoryLedger& Inventories,const FHansaInventoryId Inventory,const FHansaGoodId Good)
		{
			const auto Stock=Inventories.CreateReadOnlyAccess().QueryStock(Inventory,Good);
			return Stock.IsSet()?Stock->Available.GetRawValue():0;
		}

		FHansaCityMarketState* MarketFor(TArray<FHansaCityMarketState>& Markets,const FHansaCityDefinitionId City,const FHansaGoodId Good)
		{
			return Markets.FindByPredicate([&](const auto& M){return M.CityId==City&&M.GoodId==Good;});
		}

		bool ParseGood(const FString& StableId,FHansaGoodId& Out)
		{
			const auto Parsed=FHansaGoodId::TryParse(StableId); if(!Parsed)return false;Out=Parsed.Value;return true;
		}

		bool Deposit(FHansaInventoryLedger& Inventories,const FHansaInventoryId Inventory,const FHansaGoodId Good,const int64 Quantity,const FName Source,const FHansaSimulationTick Tick)
		{
			if(Quantity<=0)return true;
			return Inventories.TryTransfer(FHansaInventoryEndpoint::Source(Source),FHansaInventoryEndpoint::Inventory(Inventory),Good,FHansaQuantity::FromRaw(Quantity),Tick,Inventories.CreateReadOnlyAccess().GetLastMovementSequence()+1).IsSuccess();
		}

		bool Withdraw(FHansaInventoryLedger& Inventories,const FHansaInventoryId Inventory,const FHansaGoodId Good,const int64 Quantity,const FName Sink,const FHansaSimulationTick Tick)
		{
			if(Quantity<=0)return true;
			return Inventories.TryTransfer(FHansaInventoryEndpoint::Inventory(Inventory),FHansaInventoryEndpoint::Sink(Sink),Good,FHansaQuantity::FromRaw(Quantity),Tick,Inventories.CreateReadOnlyAccess().GetLastMovementSequence()+1).IsSuccess();
		}

		int64 InputTarget(const FHansaCompiledCityMarketProfileDefinition& City,const FHansaEconomicRegistry& Registry,const FString& GoodId)
		{
			int64 Target=0;
			for(const auto& Binding:City.IndustryBindings) if(Binding.bEnabled)
			{
				const auto* Chain=Registry.FindProductionChain(Binding.ProductionChainId); if(!Chain)continue;
				for(const auto& Stage:Chain->Stages) if(Binding.EnabledStageKeys.Contains(Stage.StageKey))
					if(const auto* Recipe=Registry.FindRecipe(Stage.RecipeId)) for(const auto& Input:Recipe->Inputs) if(Input.GoodId==GoodId)
						Target=FMath::Max(Target,Binding.InputReserveMilliUnits+Input.QuantityMilliUnits*Binding.CyclesPerMarketUpdate);
			}
			return Target;
		}
	}

	void FHansaRegionalEconomyExecutor::AdvanceMarketUpdate(TArray<FHansaRemoteIndustryState>& Industries,
		TArray<FHansaRegionalShipmentState>& Shipments,uint64& NextShipmentSequence,TArray<FHansaCityMarketState>& Markets,
		FHansaInventoryLedger& Inventories,const FHansaEconomicRegistry& Registry,const FHansaSimulationTick Tick)
	{
		// Delivery is a distinct later update. Failed capacity leaves cargo in transit for a later deterministic retry.
		for(int32 Index=Shipments.Num()-1;Index>=0;--Index)
		{
			auto& Shipment=Shipments[Index]; if(Shipment.DeliveryTick.GetValue()>Tick.GetValue())continue;
			const FHansaInventoryId Destination=InventoryFor(Markets,Shipment.DestinationCityId);
			const FName Source(*FString::Printf(TEXT("RegionalExchange.%llu.Delivery"),static_cast<unsigned long long>(Shipment.Sequence)));
			if(Destination.IsValid()&&Deposit(Inventories,Destination,Shipment.GoodId,Shipment.DeliverableQuantity.GetRawValue(),Source,Tick)) Shipments.RemoveAt(Index);
		}

		TMap<FString,int64> UsedSourceCapacity;
		for(const auto& City:Registry.GetCityMarkets())
		{
			if(!City.bMarketOnly||City.StableId==TEXT("City.Lubeck")||City.RegionId.IsEmpty()||City.IndustryBindings.IsEmpty())continue;
			const auto CityIdResult=FHansaCityDefinitionId::TryParse(City.StableId); if(!CityIdResult)continue;
			const FHansaCityDefinitionId CityId=CityIdResult.Value; const FHansaInventoryId Inventory=InventoryFor(Markets,CityId); if(!Inventory.IsValid())continue;
			const auto* Region=Registry.FindRegion(City.RegionId); if(!Region)continue;
			for(const auto& Binding:City.IndustryBindings)
			{
				const auto* Chain=Registry.FindProductionChain(Binding.ProductionChainId); if(!Chain)continue;
				for(const auto& Stage:Chain->Stages) if(Binding.EnabledStageKeys.Contains(Stage.StageKey))
				{
					auto* State=Industries.FindByPredicate([&](const auto& S){return S.CityId==CityId&&S.ProductionChainId==Binding.ProductionChainId&&S.StageKey==Stage.StageKey;});
					if(!State){FHansaRemoteIndustryState New;New.CityId=CityId;New.ProductionChainId=Binding.ProductionChainId;New.StageKey=Stage.StageKey;New.RecipeId=Stage.RecipeId;Industries.Add(MoveTemp(New));State=&Industries.Last();}
					State->LastUpdateTick=Tick;State->LastProduced=FHansaQuantity();State->BlockingGoodId={};State->BlockingRequired=FHansaQuantity();State->BlockingAvailable=FHansaQuantity();
					if(!Binding.bEnabled){State->Blocker=EHansaRemoteIndustryBlocker::Disabled;continue;}
					const auto* Recipe=Registry.FindRecipe(Stage.RecipeId);if(!Recipe){State->Blocker=EHansaRemoteIndustryBlocker::Disabled;continue;}
					int32 Cycles=Binding.CyclesPerMarketUpdate; State->Blocker=EHansaRemoteIndustryBlocker::None;
					for(const auto& Input:Recipe->Inputs)
					{
						FHansaGoodId Good;if(!ParseGood(Input.GoodId,Good)){Cycles=0;break;}
						const int64 Have=Available(Inventories,Inventory,Good);const int64 Per=Input.QuantityMilliUnits;const int32 Possible=Per>0?static_cast<int32>(FMath::Min<int64>(Cycles,Have/Per)):Cycles;
						if(Possible<Cycles){Cycles=Possible;State->Blocker=EHansaRemoteIndustryBlocker::MissingInput;State->BlockingGoodId=Good;State->BlockingRequired=FHansaQuantity::FromRaw(Per*Binding.CyclesPerMarketUpdate);State->BlockingAvailable=FHansaQuantity::FromRaw(Have);}
					}
					for(const auto& Output:Recipe->Outputs)
					{
						FHansaGoodId Good;if(!ParseGood(Output.GoodId,Good)){Cycles=0;break;}
						const int64 Ceiling=Binding.OutputReserveMilliUnits;const int64 Have=Available(Inventories,Inventory,Good);const int64 Per=FMath::Max<int64>(1,Output.QuantityMilliUnits*Binding.EfficiencyBasisPoints/10000);
						const int32 Possible=Ceiling>0?static_cast<int32>(FMath::Min<int64>(Cycles,FMath::Max<int64>(0,Ceiling-Have)/Per)):Cycles;
						if(Possible<Cycles){Cycles=Possible;State->Blocker=EHansaRemoteIndustryBlocker::OutputReserveReached;}
						if(Recipe->bDeclaredSource)
						{
							const auto* Endowment=Region->ResourceEndowments.FindByPredicate([&](const auto& E){return E.GoodId==Output.GoodId;});const int64 Capacity=Endowment&&Endowment->Endowment>=2?Endowment->SourceCapacityMilliUnitsPerUpdate:0;const FString Key=Region->StableId+TEXT("|")+Output.GoodId;const int64 Remaining=FMath::Max<int64>(0,Capacity-UsedSourceCapacity.FindRef(Key));const int32 SourceCycles=static_cast<int32>(FMath::Min<int64>(Cycles,Remaining/Per));if(SourceCycles<Cycles){Cycles=SourceCycles;State->Blocker=EHansaRemoteIndustryBlocker::SourceCapacityReached;}
						}
					}
					if(Cycles>0)
					{
						int64 OutputPerCycle=0;for(const auto& Output:Recipe->Outputs)OutputPerCycle+=Output.QuantityMilliUnits*Binding.EfficiencyBasisPoints/10000;
						const auto InventoryView=Inventories.CreateReadOnlyAccess().QueryInventory(Inventory);const int32 CapacityCycles=InventoryView.IsSet()&&OutputPerCycle>0?static_cast<int32>(FMath::Min<int64>(Cycles,InventoryView->FreeCapacity.GetRawValue()/OutputPerCycle)):0;
						if(CapacityCycles<Cycles){Cycles=CapacityCycles;State->Blocker=EHansaRemoteIndustryBlocker::OutputReserveReached;}
					}
					if(Cycles<=0)continue;
					const FName Sink(*FString::Printf(TEXT("RemoteIndustry.%s.%s.Input"),*City.StableId,*Stage.StageKey));
					for(const auto& Input:Recipe->Inputs){FHansaGoodId Good;ParseGood(Input.GoodId,Good);Withdraw(Inventories,Inventory,Good,Input.QuantityMilliUnits*Cycles,Sink,Tick);}
					int64 TotalProduced=0;const FName Source(*FString::Printf(TEXT("RemoteIndustry.%s.%s.Output"),*City.StableId,*Stage.StageKey));
					for(const auto& Output:Recipe->Outputs){FHansaGoodId Good;ParseGood(Output.GoodId,Good);const int64 Quantity=Output.QuantityMilliUnits*Cycles*Binding.EfficiencyBasisPoints/10000;if(Deposit(Inventories,Inventory,Good,Quantity,Source,Tick)){TotalProduced+=Quantity;if(Recipe->bDeclaredSource)UsedSourceCapacity.FindOrAdd(Region->StableId+TEXT("|")+Output.GoodId)+=Quantity;}}
					State->CompletedCycles+=Cycles;State->LastProduced=FHansaQuantity::FromRaw(TotalProduced);
				}
			}
		}

		// Stable region/good/city matching. Goods needed by an enabled industry receive priority over market reserve.
		for(const auto& Region:Registry.GetRegions())
		{
			int64 RemainingCapacity=Region.ExchangeCapacityMilliUnitsPerUpdate;
			for(const auto& GoodDefinition:Registry.GetGoods())
			{
				FHansaGoodId Good;if(!ParseGood(GoodDefinition.StableId,Good))continue;
				for(int32 Priority=0;Priority<2;++Priority)
				for(const FString& DestinationName:Region.MemberCityIds)
				{
					if(DestinationName==TEXT("City.Lubeck"))continue;const auto* DestinationProfile=Registry.FindCityMarket(DestinationName);if(!DestinationProfile||!DestinationProfile->bMarketOnly)continue;
					const auto DestinationIdResult=FHansaCityDefinitionId::TryParse(DestinationName);if(!DestinationIdResult)continue;const auto DestinationId=DestinationIdResult.Value;const FHansaInventoryId DestinationInventory=InventoryFor(Markets,DestinationId);if(!DestinationInventory.IsValid())continue;
					auto* DestinationMarket=MarketFor(Markets,DestinationId,Good);const int64 Target=Priority==0?InputTarget(*DestinationProfile,Registry,GoodDefinition.StableId):(DestinationMarket?DestinationMarket->DesiredReserve.GetRawValue():0);if(Target<=0)continue;int64 Incoming=0;for(const auto& Existing:Shipments)if(Existing.DestinationCityId==DestinationId&&Existing.GoodId==Good)Incoming+=Existing.DeliverableQuantity.GetRawValue();int64 Needed=FMath::Max<int64>(0,Target-Available(Inventories,DestinationInventory,Good)-Incoming);
					for(const FString& SourceName:Region.MemberCityIds)
					{
						if(Needed<=0||RemainingCapacity<=0)break;if(SourceName==TEXT("City.Lubeck")||SourceName==DestinationName)continue;const auto* SourceProfile=Registry.FindCityMarket(SourceName);if(!SourceProfile||!SourceProfile->bMarketOnly)continue;const auto SourceIdResult=FHansaCityDefinitionId::TryParse(SourceName);if(!SourceIdResult)continue;const auto SourceId=SourceIdResult.Value;const FHansaInventoryId SourceInventory=InventoryFor(Markets,SourceId);auto* SourceMarket=MarketFor(Markets,SourceId,Good);if(!SourceInventory.IsValid()||!SourceMarket)continue;const int64 Surplus=FMath::Max<int64>(0,Available(Inventories,SourceInventory,Good)-SourceMarket->DesiredReserve.GetRawValue());const int64 Quantity=FMath::Min3(Surplus,Needed,RemainingCapacity);if(Quantity<=0)continue;
						const uint64 Sequence=NextShipmentSequence++;const FName Sink(*FString::Printf(TEXT("RegionalExchange.%llu.Commit"),static_cast<unsigned long long>(Sequence)));if(!Withdraw(Inventories,SourceInventory,Good,Quantity,Sink,Tick))continue;
						FHansaRegionalShipmentState Shipment;Shipment.Sequence=Sequence;Shipment.RegionId=Region.StableId;Shipment.SourceCityId=SourceId;Shipment.DestinationCityId=DestinationId;Shipment.GoodId=Good;Shipment.CommittedQuantity=FHansaQuantity::FromRaw(Quantity);Shipment.DeliverableQuantity=FHansaQuantity::FromRaw(Quantity*(10000-Region.TransportLossBasisPoints)/10000);Shipment.DispatchTick=Tick;Shipment.DeliveryTick=FHansaSimulationTick::TryCreate(Tick.GetValue()+Region.ExchangeDelayUpdates*FMath::Max(1,DestinationProfile->UpdateCadenceTicks)).Value;Shipment.TransportCostMilliMarks=Quantity*Region.TransportCostMilliMarksPerUnit;Shipments.Add(MoveTemp(Shipment));Needed-=Quantity;RemainingCapacity-=Quantity;
					}
				}
			}
		}
		Industries.Sort([](const auto& L,const auto& R){if(L.CityId!=R.CityId)return L.CityId<R.CityId;if(L.ProductionChainId!=R.ProductionChainId)return L.ProductionChainId<R.ProductionChainId;return L.StageKey<R.StageKey;});
		Shipments.Sort([](const auto& L,const auto& R){return L.Sequence<R.Sequence;});
	}
}
