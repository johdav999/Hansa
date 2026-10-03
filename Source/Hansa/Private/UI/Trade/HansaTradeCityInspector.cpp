#include "UI/HansaTradeCityInspector.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Queries/HansaSimulationReadOnly.h"
#define LOCTEXT_NAMESPACE "HansaTradeCityInspector"
using namespace Hansa::Simulation;
const FHansaTradeStationProjection* Hansa::UI::FindTradeCityStation(const FHansaSimulationProjection& Projection,FName City,FHansaHouseId Viewer) {
 if(!Viewer.IsValid())return nullptr;
 const auto* Presence=Projection.GetForeignPresences().FindByPredicate([&](const auto& P){return P.HouseId==Viewer&&P.CityId.ToString()==City.ToString();});
 if(Presence&&Presence->StationId.IsValid())return Projection.GetTradeStations().FindByPredicate([&](const auto& S){return S.Station.OwnerId==Viewer&&S.Station.CityId.ToString()==City.ToString()&&S.Station.Id==Presence->StationId;});
 // Retain the existing closure/recovery surface if no current station is referenced.
 return Projection.GetTradeStations().FindByPredicate([&](const auto& S){return S.Station.OwnerId==Viewer&&S.Station.CityId.ToString()==City.ToString();});
}
bool FHansaTradeCityInspector::operator==(const FHansaTradeCityInspector& B) const {
 return CityId==B.CityId && State==B.State && bStationSupported==B.bStationSupported && PrimarySection==B.PrimarySection &&
 OwnedRouteCount==B.OwnedRouteCount && ApproachingShipCount==B.ApproachingShipCount && StationSiteCount==B.StationSiteCount && bActiveLease==B.bActiveLease && ReportsAccess==B.ReportsAccess && PublicTradeAccess==B.PublicTradeAccess && RoutesAccess==B.RoutesAccess &&
 Identity.EqualTo(B.Identity)&&Overview.EqualTo(B.Overview)&&MarketAccess.EqualTo(B.MarketAccess)&&Presence.EqualTo(B.Presence)&&Routes.EqualTo(B.Routes)&&Expansion.EqualTo(B.Expansion)&&Issue.EqualTo(B.Issue)&&PrimaryAction.EqualTo(B.PrimaryAction);
}
FHansaTradeCityInspector Hansa::UI::BuildTradeCityInspector(FName SelectedCity,const FHansaTradeMapCityPresentation* City,
 const FHansaSimulationProjection& Projection,const FHansaEconomicRegistry& Registry,FHansaHouseId Viewer) {
 FHansaTradeCityInspector Out;Out.CityId=SelectedCity;
 Out.Identity=LOCTEXT("UnknownIdentity","City authority unavailable");
 Out.Overview=LOCTEXT("UnknownCity","City information is unavailable. Select a known city on the map.");
 Out.MarketAccess=LOCTEXT("UnknownAccess","Market access is unknown; obtain an authorized report before planning a purchase.");
 Out.Presence=LOCTEXT("UnknownPresence","Presence unavailable");
 Out.PrimaryAction=LOCTEXT("ReviewReports","Review city reports");
 if(!City)return Out;
 const auto* Policy=Registry.FindCityTradePolicyForCity(SelectedCity.ToString());
 const auto* Presence=Projection.GetForeignPresences().FindByPredicate([&](const auto& P){return Viewer.IsValid()&&P.HouseId==Viewer&&P.CityId.ToString()==SelectedCity.ToString();});
 const auto* Station=FindTradeCityStation(Projection,SelectedCity,Viewer);
 Out.StationSiteCount=Policy&&City->bRendered&&!City->bBuildable?Policy->TradeStationSites.Num():-1;
 Out.bActiveLease=Station&&Station->Lease.bActive;
 Out.Identity=City->bBuildable?LOCTEXT("HomeIdentity","Home / founded city"):LOCTEXT("Autonomous","Autonomous city");
 Out.Presence=!Viewer.IsValid()?LOCTEXT("PrivatePresence","Presence unavailable for this viewer"):Presence?FText::FromString(Presence->CurrentStageDisplayName):LOCTEXT("NoPresence","No established commercial presence");
 Out.Overview=City->CapabilitySummary;
 Out.PrimarySection=TEXT("Presence");Out.PrimaryAction=Station?LOCTEXT("InspectStation","Inspect station"):LOCTEXT("ReviewPresence","Review presence requirements");
 const bool StageAllowed=Policy&&Policy->AllowedStageIds.ContainsByPredicate([&](const FString& Id){const auto* Stage=Registry.FindPresenceStage(Id);return Stage&&Stage->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation"));});
 const bool Denied=Policy&&Policy->DeniedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation"));
 Out.bStationSupported=!City->bBuildable&&City->bRendered&&Policy&&!Policy->TradeStationSites.IsEmpty()&&StageAllowed&&!Denied;
 Out.MarketAccess=City->bBuildable?LOCTEXT("HomeMarket","Home inventory transfers follow local ownership rules; they are not foreign market purchases."):
 !Policy?LOCTEXT("NoPolicy","No city access policy is available. Public trade and station access cannot be promised."):
 Policy->bPublicMarketAccess?LOCTEXT("PublicMarket","Public market access is permitted. Bring an owned ship to berth; purchases and sales recheck access, stock, money and cargo space."):
 LOCTEXT("PublicLocked","Public market access is denied by city policy. Inspect separately granted capabilities; a route alone does not unlock public trade.");
 if(City->bBuildable){Out.State=EHansaTradeCityState::Home;Out.Presence=LOCTEXT("HomePresence","Foreign presence not applicable");Out.Overview=LOCTEXT("HomeOverview","Foreign trade stations do not apply here. Use local inventory, construction and owned-city route transfers under normal permission rules.");Out.PrimarySection=TEXT("Route");Out.PrimaryAction=LOCTEXT("HomeRoutes","Review route transfers");}
 else if(City->bUnknown){Out.State=EHansaTradeCityState::Unknown;Out.Issue=LOCTEXT("UnknownReport","No authorized report for the selected good. Visit an accessible market or wait for a permitted report; unknown stock is not zero.");}
 else if(City->bMarketOnly){Out.State=EHansaTradeCityState::MarketOnly;Out.Overview=LOCTEXT("AbstractCity","Market-only city · visiting commerce only in the current catalog. No rendered world visit, station site or local construction area is available.");Out.PrimarySection=TEXT("Route");Out.PrimaryAction=LOCTEXT("MarketRoutes","Review visiting route");}
 else if(!Policy||Policy->TradeStationSites.IsEmpty()||!City->bRendered){Out.State=EHansaTradeCityState::UnsupportedSite;Out.Issue=LOCTEXT("UnsupportedSite","No supported station site. Continue permitted visiting trade; select a rendered city with an authored site to establish a station.");}
 else if(Denied||!StageAllowed||!Policy->bPublicMarketAccess){Out.State=EHansaTradeCityState::PolicyLocked;Out.Issue=LOCTEXT("PolicyLocked","City policy restricts access or station progression. Review denied capabilities and allowed stages; trading more cannot bypass a policy prohibition.");}
 else Out.State=EHansaTradeCityState::RenderedForeign;
 if(Presence){
  for(const auto& Capability:Presence->Capabilities){
   const int32 Access=Capability.bGranted?1:0;
   if(Capability.CapabilityId==TEXT("PresenceCapability.MarketReports"))Out.ReportsAccess=Access;
   if(Capability.CapabilityId==TEXT("PresenceCapability.PublicMarketTrade"))Out.PublicTradeAccess=Access;
   if(Capability.CapabilityId==TEXT("PresenceCapability.RouteAccess"))Out.RoutesAccess=Access;
  }
  if(Presence->Status!=EHansaForeignPresenceStatus::Active)Out.Issue=LOCTEXT("Suspended","Presence is suspended or revoked. Inspect preserved assets and recovery conditions before resuming operations.");
  for(const auto& Capability:Presence->Capabilities)if(Capability.CapabilityId==TEXT("PresenceCapability.PublicMarketTrade")||Capability.CapabilityId==TEXT("PresenceCapability.MarketReports")||Capability.CapabilityId==TEXT("PresenceCapability.RouteAccess"))Out.MarketAccess=FText::Format(LOCTEXT("CapabilityLine","{0}\n{1}: {2}"),Out.MarketAccess,FText::FromString(Capability.DisplayName),Capability.bGranted?LOCTEXT("Granted","Granted"):LOCTEXT("Locked","Locked"));
 }
 if(Station&&!Station->Blocker.IsEmpty())Out.Issue=FText::FromString(Station->Blocker+TEXT(" ")+Station->NextStep);
 if(Out.Issue.IsEmpty()&&City->bStale)Out.Issue=LOCTEXT("Stale","Report is older. Refresh lawful market information before committing cargo or money.");
 if(Out.bStationSupported&&!Station)Out.Overview=FText::Format(LOCTEXT("Sites","{0}\nStation sites: {1}. Review presence requirements, propose an available site, then fund construction."),Out.Overview,FText::AsNumber(Policy->TradeStationSites.Num()));
 int32 Routes=0,Arrivals=0;
 for(const auto& Route:Projection.GetRoutes())if(Viewer.IsValid()&&Route.OwnerId==Viewer&&Route.Lifecycle!=EHansaRouteLifecycleState::Cancelled){
  if(Route.Stops.ContainsByPredicate([&](const auto& Stop){return Stop.CityId.ToString()==SelectedCity.ToString();}))++Routes;
  if(Route.Lifecycle==EHansaRouteLifecycleState::Traveling&&Route.Stops.IsValidIndex(Route.NextStopIndex)&&Route.Stops[Route.NextStopIndex].CityId.ToString()==SelectedCity.ToString())++Arrivals;
 }
 Out.Routes=Viewer.IsValid()?FText::Format(LOCTEXT("CityRoutes","Your routes: {0} · approaching ships: {1}. Select a route in the directory for its schedule and cargo."),FText::AsNumber(Routes),FText::AsNumber(Arrivals)):LOCTEXT("RoutesPrivate","Routes and arrivals unavailable for this viewer.");
 Out.OwnedRouteCount=Viewer.IsValid()?Routes:-1;Out.ApproachingShipCount=Viewer.IsValid()?Arrivals:-1;
 if(Policy&&City->bRendered&&!City->bBuildable){
  Out.Expansion=Station&&Station->Lease.bActive?
   FText::Format(LOCTEXT("ActiveLease","Active lease: {0}. Permitted categories: {1}. Building still requires normal placement checks."),FText::FromString(Station->Lease.SiteId),FText::FromString(FString::Join(Station->Lease.PermittedBuildingCategories,TEXT(", ")))):
   LOCTEXT("Boundary","No active station lease. City stock, roads and existing buildings remain autonomous; opening a route grants no construction rights.");
  if(Presence&&Presence->bGovernanceAuthority)Out.Expansion=FText::Format(LOCTEXT("Chartered","{0}\nGranted charter: {1}. Existing assets do not automatically transfer."),Out.Expansion,FText::FromString(Presence->GovernanceCharterId));
  else Out.Expansion=FText::Format(LOCTEXT("Governance","{0}\n{1}"),Out.Expansion,Policy->bExceptionalGovernanceAllowed?LOCTEXT("CharterRequired","Governance requires a qualifying scenario charter; trade volume alone is insufficient."):LOCTEXT("NoGovernance","City policy does not permit governance."));
 }
 return Out;
}
#undef LOCTEXT_NAMESPACE
