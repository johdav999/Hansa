#pragma once
#include "World/HansaRuntimeSimulationHost.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
namespace Hansa::Tests::TradeJourney
{
using namespace Hansa::Simulation;
inline int64 Stock(const FHansaSimulationProjection& P, uint64 Inventory, const FString& Good = TEXT("Good.Bread"))
{
    const auto* I = P.GetInventories().FindByPredicate([&](const auto& X){return X.Id.GetValue()==Inventory;});
    if (!I) return -1;
    const auto* S = I->Stocks.FindByPredicate([&](const auto& X){return X.GoodId.ToString()==Good;});
    return S ? S->Available.GetRawValue() : 0;
}
inline int64 Consumed(const FHansaSimulationProjection& P)
{
    int64 Q=0; for(const auto& C:P.GetPopulationCohorts())for(const auto& N:C.Needs)if(N.GoodId.ToString()==TEXT("Good.Bread"))Q+=N.ConsumedLastTick.GetRawValue();return Q;
}
inline int64 Price(const FHansaSimulationProjection& P,const TCHAR* City)
{
    const auto* M=P.GetMarkets().FindByPredicate([&](const auto& X){return X.CityId.ToString()==City&&X.GoodId.ToString()==TEXT("Good.Bread");});return M?M->CurrentPriceMilliMarks:-1;
}
inline bool UnlockReserveAutomation(UHansaRuntimeSimulationHost* Host)
{
    if (!Host) return false;
    const TCHAR* Technologies[] = {
        TEXT("Technology.Commerce.MarketReports"),
        TEXT("Technology.Commerce.TransactionFriction"),
        TEXT("Technology.Commerce.ReserveAutomation")
    };
    for (const TCHAR* Technology : Technologies)
    {
        if (Host->IsTechnologyCompleted(Technology)) continue;
        if (!Host->QueueResearch(Technology)) return false;
        for (int32 Tick = 0; Tick < 64 && !Host->IsTechnologyCompleted(Technology); ++Tick)
            if (!Host->AdvanceTicks(1)) return false;
        if (!Host->IsTechnologyCompleted(Technology)) return false;
    }
    return true;
}
inline FString Evidence(UHansaRuntimeSimulationHost* Host, uint64 Route)
{
    const auto P=Host->BuildProjection().Value;
    FString S=FString::Printf(TEXT("tick=%lld\nfingerprint=%llu\nevents=%llu\nlubeckBread=%lld\nrostockBread=%lld\ncargoBread=%lld\nbreadConsumed=%lld\nlubeckPrice=%lld\nrostockPrice=%lld\n"),P.GetClock().GetTick().GetValue(),P.GetFingerprint().Value,P.GetPublishedDomainEventCount(),Stock(P,1),Stock(P,4),Stock(P,1001),Consumed(P),Price(P,TEXT("City.Lubeck")),Price(P,TEXT("City.Rostock")));
    const auto* R=P.GetRoutes().FindByPredicate([&](const auto& X){return X.Id.GetValue()==Route;});
    if(R)S+=FString::Printf(TEXT("route=%llu\ntransferTick=%lld\ntransferCity=%s\ntransferGood=%s\ntransferMilli=%lld\ntransferKind=%d\n"),Route,R->LastTransfer.Tick.GetValue(),*R->LastTransfer.CityId.ToString(),*R->LastTransfer.GoodId.ToString(),R->LastTransfer.AppliedQuantity.GetRawValue(),int(R->LastTransfer.Kind));
    for(const auto& E:Host->GetEventHistory())if(E.GetRouteId().GetValue()==Route)
        S+=FString::Printf(TEXT("event=%llu|%lld|%s|%s|%s|%lld\n"),E.GetGlobalSequence(),E.GetTick().GetValue(),LexToString(E.GetType()),*E.GetCityId().ToString(),*E.GetGoodId().ToString(),E.GetValue());
    return S;
}
inline TSharedPtr<FJsonObject> Fixture()
{
    FString Text;TSharedPtr<FJsonObject> J;
    if(FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("Tests/Fixtures/trade_journey_p34_v1.json"))))FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J);return J;
}
}
