#include "UI/HansaMarketTablePresentationModel.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Math/HansaFixedPoint.h"
#define LOCTEXT_NAMESPACE "HansaVisitingTrade"
using namespace Hansa::Simulation;
namespace {
FText Quantity(int64 V){return V<0?LOCTEXT("Unknown","Unknown"):FText::AsNumber(double(V)/1000.);}
}
void UHansaMarketTablePresentationModel::ApplyVisitingMarket(const TArray<FHansaReplicatedMarket>& Markets,FName City)
{
 const auto Before=Snapshot;
 const auto Parsed=FHansaCityDefinitionId::TryParse(City.ToString());if(!Parsed)return;
 if(CurrentCityId!=Parsed.Value){SpotTradeVehicleId={};SpotTradeResult=FText();}
 CurrentCityId=Parsed.Value;Snapshot.AllRows.Reset();DetailByGood.Reset();
 for(const auto& M:Markets){if(M.CityId!=City.ToString())continue;
  FHansaMarketTableRowPresentation Row;Row.GoodStableId=FName(*M.GoodId);Row.GoodLabel=FText::FromString(M.GoodId.RightChop(5));DescribeVisitingGood(Row);Row.bUnknown=M.CurrentPriceMilliMarks<=0;Row.bStale=M.bStale;Row.bEstimated=true;Row.PriceRaw=M.CurrentPriceMilliMarks;Row.StockRaw=M.StockMilliUnits;Row.ReportAgeTicks=M.ReportAgeTicks;
  Row.Stock=Quantity(M.StockMilliUnits);Row.Reserve=Quantity(M.DesiredReserveMilliUnits);Row.Price=Row.bUnknown?LOCTEXT("Unknown","Unknown"):FText::Format(LOCTEXT("ReportedPrice","{0} milli-marks"),FText::AsNumber(M.CurrentPriceMilliMarks));Row.Demand=Row.Incoming=Row.Trend=Row.Sparkline=LOCTEXT("Unavailable","Unavailable");
  Row.Status=Row.bUnknown?LOCTEXT("UnknownReport","Report unavailable"):Row.bStale?LOCTEXT("Stale","Stale report"):LOCTEXT("Reported","Reported");Row.ReportAge=M.ReportAgeTicks<0?LOCTEXT("UnknownAge","Age unknown"):FText::Format(LOCTEXT("Age","{0} ticks old"),FText::AsNumber(M.ReportAgeTicks));Row.AccessibleLabel=FText::Format(LOCTEXT("Accessible","{0}, {1}, stock {2}, price {3}, {4}"),Row.GoodLabel,Row.Status,Row.Stock,Row.Price,Row.ReportAge);
  FHansaSelectedGoodPresentation D;D.GoodStableId=Row.GoodStableId;D.GoodLabel=Row.GoodLabel;D.bHasSelection=true;D.bHasReport=!Row.bUnknown;D.bStale=Row.bStale;D.bRouteEnabled=!bRemoteVisiting&&!Row.bUnknown;if(bRemoteVisiting)D.RouteDisabledReason=LOCTEXT("RemoteRouteUnavailable","New routes are unavailable in this remote view: the complete route-planning permission report is not provided.");D.LocalPrice=Row.Price;D.Confidence=FText::Format(LOCTEXT("Context","{0} · {1} · {2}"),FText::FromString(City.ToString().RightChop(5)),Row.Status,Row.ReportAge);D.StockVersusReserve=FText::Format(LOCTEXT("StockReserve","{0} / {1} cargo (reported)"),Row.Stock,Row.Reserve);D.BaseValue=D.RecentAverageDifference=D.ReserveDays=D.Production=D.Consumption=D.SupplyBalance=D.CitizenDemand=D.IndustrialDemand=D.IncomingSupply=LOCTEXT("Unavailable","Unavailable");D.ChartSummary=LOCTEXT("HistoryUnavailable","Detailed price history is unavailable in this visiting report.");D.Explanation=LOCTEXT("PublicOnly","This is the city's lawful public report. Rival cargo and finances are private. A quote is an estimate, not a reserved fill.");D.RouteActionLabel=LOCTEXT("CreateRoute","Create route");D.PinActionLabel=LOCTEXT("Pin","Pin price & stock");D.PinDisabledReason=LOCTEXT("PinUnavailable","Visiting report pinning is unavailable.");
  DetailByGood.Add(D.GoodStableId,D);Snapshot.AllRows.Add(Row);
 }
 if(!DetailByGood.Contains(Snapshot.SelectedGoodStableId)&&!Snapshot.AllRows.IsEmpty())Snapshot.SelectedGoodStableId=Snapshot.AllRows[0].GoodStableId;
 RebuildVisibleRows();RebuildSelectedGood();PublishIfChanged(Before);
}
void UHansaMarketTablePresentationModel::ApplyRemoteVisiting(const FHansaClientProjectionSnapshot& P,FName City)
{
 if(P.SchemaVersion!=FHansaClientProjectionSnapshot::CurrentSchemaVersion||P.OwnerHouseId<=0){
  SpotOwner=SpotProjectionRevision=SpotSequence=SpotNonce=0;SpotTradeVehicleId={};SpotTradeResult=FText();bSpotPending=false;bRemoteVisiting=true;VisitingOffers.Reset();ApplyVisitingMarket({},City);return;
 }
 if(SpotOwner==P.OwnerHouseId&&P.Revision>0&&(P.Revision<SpotProjectionRevision||(P.Revision==SpotProjectionRevision&&CurrentCityId.ToString()==City.ToString())))return;
 if(SpotOwner!=P.OwnerHouseId){SpotTradeVehicleId={};SpotTradeResult=FText();bSpotPending=false;SpotSequence=SpotNonce=0;}
 SpotOwner=P.OwnerHouseId;SpotProjectionRevision=P.Revision;bRemoteVisiting=true;VisitingOffers=P.VisitingTrade;ApplyVisitingMarket(P.Markets,City);
}
void UHansaMarketTablePresentationModel::RebuildSpotTrade()
{
 auto& D=Snapshot.SelectedGood;
 D.bSpotTradeVisible=D.bHasSelection&&CurrentCityId.IsValid()&&(bRemoteVisiting||!Runtime.IsValid()||CurrentCityId!=Runtime->GetCityId());
 D.bSpotTradeBuy=bSpotTradeBuy;D.SpotTradeQuantityRaw=SpotTradeQuantityRaw;D.SpotTradeHeading=LOCTEXT("QuayTrade","Quay trade");D.SpotTradeQuantity=FText::Format(LOCTEXT("Cargo","{0} cargo"),Quantity(SpotTradeQuantityRaw));D.SpotTradeResult=SpotTradeResult;
 D.SpotTradeConfirmLabel=bSpotPending?LOCTEXT("Pending","Awaiting server"):bSpotTradeBuy?LOCTEXT("Buy","Confirm purchase"):LOCTEXT("Sell","Confirm sale");D.bSpotTradeCanSubmit=false;D.SpotTradeQuote=FText();D.SpotTradeRemedy=FText();D.SpotTradeReviewedMarketUpdateTick=-1;D.SpotTradeReviewedUnitPrice=0;
 VisitingVehicles.Reset();for(const auto& O:VisitingOffers)if(O.City==CurrentCityId.ToString()&&O.Good==D.GoodStableId.ToString())VisitingVehicles.AddUnique(O.Vehicle);
 VisitingVehicles.Sort();D.bSpotTradePending=bSpotPending;D.bSpotTradeHasShipChoices=!VisitingVehicles.IsEmpty();
 // Auto-select only an unambiguous single owned ship. Multiple choices require an explicit action.
 if(!SpotTradeVehicleId.IsValid()&&VisitingVehicles.Num()==1)SpotTradeVehicleId=FHansaVehicleId::TryCreate(uint64(VisitingVehicles[0])).Value;
 D.SpotTradeVehicleValue=int64(SpotTradeVehicleId.GetValue());
 D.SpotTradeVehicle=SpotTradeVehicleId.IsValid()?FText::Format(LOCTEXT("Ship","Owned Cog {0}"),FText::AsNumber(D.SpotTradeVehicleValue)):VisitingVehicles.IsEmpty()?LOCTEXT("NoShip","No owned Cog in this city"):LOCTEXT("ChooseShip","Choose an owned Cog");
 if(!D.bSpotTradeVisible)return;
 const auto* O=VisitingOffers.FindByPredicate([&](const auto& V){return V.City==CurrentCityId.ToString()&&V.Good==D.GoodStableId.ToString()&&V.Vehicle==D.SpotTradeVehicleValue&&V.bBuy==bSpotTradeBuy;});
 FHansaSpotTradeQuoteProjection Q;
#if WITH_DEV_AUTOMATION_TESTS
 if(SpotTradeQuoteForTesting)Q=SpotTradeQuoteForTesting(bSpotTradeBuy?EHansaSpotTradeSide::BuyFromCity:EHansaSpotTradeSide::SellToCity,FHansaQuantity::FromRaw(SpotTradeQuantityRaw));else
#endif
 if(O){Q.bCanSubmit=O->bCanSubmit;Q.Cause=O->Cause;Q.Remedy=O->Remedy;Q.EstimatedQuantity=FHansaQuantity::FromRaw(FMath::Min(SpotTradeQuantityRaw,O->Bound));Q.ReviewedUnitPriceMilliMarks=O->Price;Q.ReviewedMarketUpdateTick=O->ReportTick;const auto Cash=FHansaCheckedIntegerMath::TryMultiplyDivide(Q.EstimatedQuantity.GetRawValue(),O->Price,1000,bSpotTradeBuy?EHansaRoundingMode::Ceiling:EHansaRoundingMode::TowardZero);Q.EstimatedSettlementMoneyRaw=Cash?(bSpotTradeBuy?-Cash.Value:Cash.Value):0;}
 else{D.SpotTradeRemedy=VisitingVehicles.IsEmpty()?LOCTEXT("Berth","Use Create route below to send an owned Cog to this city, then wait for it to berth. Only a berthed Cog can buy or sell at this quay."):LOCTEXT("Choose","Choose a ship explicitly. A missing or no-longer-owned ship cannot trade.");return;}
 D.bSpotTradeCanSubmit=Q.bCanSubmit&&!bSpotPending;D.SpotTradeReviewedMarketUpdateTick=Q.ReviewedMarketUpdateTick;D.SpotTradeReviewedUnitPrice=Q.ReviewedUnitPriceMilliMarks;
 D.SpotTradeQuote=FText::Format(LOCTEXT("Estimate","ESTIMATE · fill up to {0} of {1} cargo\nUnit {2} milli-marks (friction included) · cash {3} pfennig\nStock, price and funds may change before execution."),Quantity(Q.EstimatedQuantity.GetRawValue()),Quantity(SpotTradeQuantityRaw),FText::AsNumber(Q.ReviewedUnitPriceMilliMarks),FText::AsNumber(Q.EstimatedSettlementMoneyRaw));
 if(O){D.SpotTradeVehicle=FText::Format(LOCTEXT("ShipDetails","Owned Cog {0} · {1}\nCargo of this good {2} · free hold {3} cargo\nTreasury {4} pfennig · reported city stock {5} cargo\nReport {6} ticks old · friction price adjustment {7} milli-marks/unit"),FText::AsNumber(O->Vehicle),FText::FromString(O->bCanSubmit?TEXT("berthed here"):O->Cause),Quantity(O->Cargo),Quantity(O->FreeCapacity),FText::AsNumber(O->Money),Quantity(O->ReportStock),O->ReportAge<0?LOCTEXT("Unknown","Unknown"):FText::AsNumber(O->ReportAge),O->Price<=0?LOCTEXT("Unknown","Unknown"):FText::AsNumber(O->Price-O->ReportPrice));
  if(!O->Receipt.IsEmpty())D.SpotTradeResult=FText::FromString(O->Receipt+(SpotTradeResult.IsEmpty()?FString():TEXT("\n")+SpotTradeResult.ToString()));
 }
 if(Q.ReviewedUnitPriceMilliMarks<=0)D.SpotTradeQuote=LOCTEXT("QuoteUnavailable","Estimate unavailable until a lawful market quote is available.");
 D.SpotTradeRemedy=FText::FromString(Q.Cause+TEXT(" ")+Q.Remedy);
 if(Q.bCanSubmit&&Q.EstimatedQuantity.GetRawValue()<SpotTradeQuantityRaw){D.SpotTradeRemedy=FText::Format(LOCTEXT("Bound","Estimated partial or missed fill: {0}. Confirming records the actual result without exceeding stock, capacity or funds."),!bSpotTradeBuy?LOCTEXT("StockLimit","available ship stock limits this sale"):O&&O->FreeCapacity<SpotTradeQuantityRaw?LOCTEXT("CapacityLimit","ship hold space is limited"):O&&O->ReportStock<SpotTradeQuantityRaw?LOCTEXT("MarketLimit","reported market stock is limited"):LOCTEXT("FundsLimit","available funds limit this purchase"));}
}
bool UHansaMarketTablePresentationModel::CycleSpotTradeVehicleIntent()
{
 if(bSpotPending||VisitingVehicles.IsEmpty())return false;const auto Before=Snapshot;const int32 I=VisitingVehicles.IndexOfByKey(int64(SpotTradeVehicleId.GetValue()));SpotTradeVehicleId=FHansaVehicleId::TryCreate(uint64(VisitingVehicles[(I+1)%VisitingVehicles.Num()])).Value;SpotTradeResult=FText();RebuildSpotTrade();PublishIfChanged(Before);return true;
}
bool UHansaMarketTablePresentationModel::CycleSpotTradeSideIntent()
{
 if(bSpotPending||!Snapshot.SelectedGood.bSpotTradeVisible)return false;const auto Before=Snapshot;bSpotTradeBuy=!bSpotTradeBuy;SpotTradeResult=FText();RebuildSpotTrade();PublishIfChanged(Before);return true;
}
bool UHansaMarketTablePresentationModel::AdjustSpotTradeQuantityIntent(int64 Delta)
{
 if(bSpotPending||!Snapshot.SelectedGood.bSpotTradeVisible)return false;const auto Before=Snapshot;SpotTradeQuantityRaw=FMath::Clamp<int64>(SpotTradeQuantityRaw+FMath::Clamp<int64>(Delta,-1'000'000'000,1'000'000'000),1000,1'000'000'000);SpotTradeResult=FText();RebuildSpotTrade();PublishIfChanged(Before);return true;
}
bool UHansaMarketTablePresentationModel::ConfirmSpotTradeIntent()
{
 const auto D=Snapshot.SelectedGood;if(!D.bSpotTradeCanSubmit||(!Runtime.IsValid()&&!NetworkCommandIntent))return false;
 const auto Good=FHansaGoodId::TryParse(D.GoodStableId.ToString());if(!Good)return false;const auto Before=Snapshot;bool Accepted=false;
 if(NetworkCommandIntent){bSpotPending=true;SpotSequence=SpotNonce=0;SpotTradeResult=LOCTEXT("Waiting","Awaiting authoritative execution; this is not a receipt.");FHansaClientCommandIntent I;I.Type=EHansaClientIntentType::SpotTrade;I.VehicleId=D.SpotTradeVehicleValue;I.CityId=CurrentCityId.ToString();I.GoodId=Good.Value.ToString();I.bSpotTradeBuy=bSpotTradeBuy;I.QuantityMilliUnits=D.SpotTradeQuantityRaw;I.ReviewedMarketUpdateTick=D.SpotTradeReviewedMarketUpdateTick;I.ReviewedUnitPriceMilliMarks=D.SpotTradeReviewedUnitPrice;Accepted=NetworkCommandIntent(I);if(!Accepted){bSpotPending=false;SpotTradeResult=LOCTEXT("SendFailed","Submission failed. Reconnect and review the current quote before retrying.");}}
 else{FHansaSpotTradeCommand C;C.VehicleId=SpotTradeVehicleId;C.CityId=CurrentCityId;C.GoodId=Good.Value;C.Side=bSpotTradeBuy?EHansaSpotTradeSide::BuyFromCity:EHansaSpotTradeSide::SellToCity;C.Quantity=FHansaQuantity::FromRaw(D.SpotTradeQuantityRaw);C.ReviewedMarketUpdateTick=D.SpotTradeReviewedMarketUpdateTick;C.ReviewedUnitPriceMilliMarks=D.SpotTradeReviewedUnitPrice;
 const auto Result=Runtime->ExecuteSpotTrade(C);Accepted=Result.IsSuccess();SpotTradeResult=Accepted?FText():FText::Format(LOCTEXT("Rejected","Rejected: {0}. Nothing was spent. Selection retained; review the updated quote before retrying."),FText::FromString(LexToString(Result.GetError())));VisitingOffers=Runtime->BuildVisitingTradeOffers(Runtime->GetHouseId());}
 RebuildSpotTrade();Snapshot.FocusedSemanticId=TEXT("Market.Detail.SpotTrade.Confirm");PublishIfChanged(Before);return Accepted;
}
void UHansaMarketTablePresentationModel::ReceiveCommandFeedback(const FHansaClientCommandFeedback& F)
{
 if(!bSpotPending)return;
 if(F.State==EHansaClientCommandState::Pending){if(!SpotSequence){SpotSequence=F.ClientSequence;SpotNonce=F.ClientNonce;}return;}
 if(!SpotSequence||SpotSequence!=F.ClientSequence||SpotNonce!=F.ClientNonce)return;
 const auto Before=Snapshot;bSpotPending=false;SpotSequence=SpotNonce=0;SpotTradeResult=FText::FromString(F.Message+TEXT(" ")+F.Remedy);RebuildSpotTrade();Snapshot.FocusedSemanticId=TEXT("Market.Detail.SpotTrade.Confirm");PublishIfChanged(Before);
}
bool UHansaMarketTablePresentationModel::SpotTradeLinkIntent(FName Action)
{
 if(bSpotPending||!Snapshot.SelectedGood.bSpotTradeVisible)return false;
 if(Action==TEXT("Manifest")){const auto* O=VisitingOffers.FindByPredicate([&](const auto& X){return X.Vehicle==int64(SpotTradeVehicleId.GetValue())&&X.City==CurrentCityId.ToString();});if(!O)return false;const auto Before=Snapshot;SpotTradeResult=FText::FromString(TEXT("Owned Cog manifest\n")+O->Manifest);RebuildSpotTrade();PublishIfChanged(Before);return true;}
 if(!VisitingLinkRequested)return false;VisitingLinkRequested(FName(*CurrentCityId.ToString()),Snapshot.SelectedGoodStableId,int64(SpotTradeVehicleId.GetValue()),Action);return true;
}
#undef LOCTEXT_NAMESPACE
