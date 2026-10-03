#include "UI/HansaTradeRemoteProjection.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/SecureHash.h"
#include "UObject/UnrealType.h"
namespace {
// Wire reports contain resolved text, not the recursive formatting histories of native widgets.
// Preserve the native presentation tree locally; this also avoids repeating format arguments in every RPC.
void FlattenReportText(void* Data,const UStruct* Type){
 for(TFieldIterator<FProperty> It(Type);It;++It){
  auto* P=*It;void* Value=P->ContainerPtrToValuePtr<void>(Data);
  if(auto* Text=CastField<FTextProperty>(P))Text->SetPropertyValue(Value,FText::AsCultureInvariant(Text->GetPropertyValue(Value).ToString()));
  else if(auto* Struct=CastField<FStructProperty>(P))FlattenReportText(Value,Struct->Struct);
  else if(auto* Array=CastField<FArrayProperty>(P)){FScriptArrayHelper Values(Array,Value);if(auto* Inner=CastField<FStructProperty>(Array->Inner))for(int32 I=0;I<Values.Num();++I)FlattenReportText(Values.GetRawPtr(I),Inner->Struct);}
 }
}
}

FString Hansa::UI::TradeRoutePlanKey(int64 RouteId,TConstArrayView<FHansaClientRouteStopIntent> Stops)
{
 FString Key=LexToString(RouteId);
 for(const auto& S:Stops){Key+=TEXT("|")+S.CityId;for(const auto& A:S.Actions)Key+=FString::Printf(TEXT("|%d:%s:%lld:%lld:%d"),A.Kind,*A.GoodId,A.QuantityMilliUnits,A.MinimumSourceReserveMilliUnits,A.CargoSlotIndex);}
 const FTCHARToUTF8 Utf8(*Key);FSHAHash Hash;FSHA1::HashBuffer(Utf8.Get(),Utf8.Length(),Hash.Hash);return Hash.ToString();
}

FHansaReplicatedTradeWorkspace Hansa::UI::BuildRemoteTradeWorkspace(
 UHansaRuntimeSimulationHost& Host,const Hansa::Simulation::FHansaSimulationProjection& Source,
 const FHansaClientProjectionSnapshot& Authorized)
{
 using namespace Hansa::Simulation;
 FHansaReplicatedTradeWorkspace Out;
 const auto Viewer=FHansaHouseId::TryCreate(uint64(Authorized.OwnerHouseId));
 if(!Viewer||!Host.GetEconomicRegistry())return Out;
 // Reuse the native presentation builders with an explicit viewer and no runtime binding.
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());
 M->InitializeDefaults();M->SetViewerHouse(Viewer.Value);M->ApplyProjection(Source,*Host.GetEconomicRegistry(),true);
 Out.Cities=M->GetSnapshot().Cities;
 for(auto& C:Out.Cities){
  const auto* Report=Authorized.Markets.FindByPredicate([&](const auto& R){return R.CityId==C.StableId.ToString();});
  C.bUnknown=!Report||Report->CurrentPriceMilliMarks<=0;C.bStale=Report&&Report->bStale;C.ReportAgeTicks=Report?Report->ReportAgeTicks:0;
  C.Information=FText::FromString(C.bUnknown?TEXT("No authorized report"):FString::Printf(TEXT("Authorized report · %lld ticks old"),C.ReportAgeTicks));
 }
 for(const auto& C:Out.Cities)Out.Inspectors.Add(BuildTradeCityInspector(C.StableId,&C,Source,*Host.GetEconomicRegistry(),Viewer.Value));
 for(auto R:M->GetSnapshot().Routes){
  if(!Authorized.Routes.ContainsByPredicate([&](const auto& V){return V.RouteId==R.RouteValue;}))continue;
  if(R.bOwnedByPlayer){const FString Label=Host.GetRouteLabel(uint64(R.RouteValue));if(!Label.IsEmpty())R.Label=FText::FromString(Label);}
  Out.Routes.Add(MoveTemp(R));
 }
 auto CopyDirectory=[&](TArray<FHansaTradeDirectoryEntry>& Dest){
  for(auto D:M->GetSnapshot().Directory){
   if(!Authorized.Vehicles.ContainsByPredicate([&](const auto& V){return V.VehicleId==D.VehicleValue;}))continue;
   if(!D.bOwned){D.Alert=FText();D.Goods.Reset();D.bAttention=false;D.bPresence=false;D.bAvailable=false;D.Detail=FText::FromString(TEXT("Other house · cargo, orders and exact voyage progress are private."));}
   if(D.RouteValue&&D.bOwned){const FString Label=Host.GetRouteLabel(uint64(D.RouteValue));if(!Label.IsEmpty()&&!M->GetSnapshot().bFleetView)D.Label=FText::FromString(Label);}
   Dest.Add(MoveTemp(D));
  }
 };
 CopyDirectory(Out.RouteDirectory);M->DirectoryIntent(TEXT("Fleet"));CopyDirectory(Out.FleetDirectory);
 for(const auto& V:Source.GetVehicles())if(V.OwnerId==Viewer.Value){
  const auto* R=Source.GetRoutes().FindByPredicate([&](const auto& X){return X.VehicleId==V.Id&&X.OwnerId==Viewer.Value&&X.Lifecycle!=EHansaRouteLifecycleState::Cancelled;});
  auto Schedule=BuildTradeSchedule(Source,*Host.GetEconomicRegistry(),R?int64(R->Id.GetValue()):0,int64(V.Id.GetValue()),Viewer.Value.GetValue());
  Schedule.PrepareForReplication();Out.Schedules.Add(MoveTemp(Schedule));
 }
 for(const auto& Good:Host.GetEconomicRegistry()->GetGoods())Out.GoodIds.Add(Good.StableId);
 FlattenReportText(&Out,FHansaReplicatedTradeWorkspace::StaticStruct());
 return Out;
}
