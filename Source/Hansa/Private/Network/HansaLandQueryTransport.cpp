#include "Network/HansaLandQueryTransport.h"
#include "Misc/Compression.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

using namespace Hansa::Simulation;
namespace
{
constexpr int32 MaxSide = 32, MaxLeases = 16, MaxCategories = 16;

bool BoundedString(FArchive& Ar, FString& Value)
{
    uint16 Length = Ar.IsSaving() ? Value.Len() : 0;
    if (Ar.IsSaving() && Value.Len() > 128) return false;
    Ar << Length;
    if (Ar.IsError() || Length > 128) return false;
    ANSICHAR Bytes[129] = {};
    if (Ar.IsSaving())
        for (int32 I=0; I<Length; ++I)
        {
            if (Value[I] < 32 || Value[I] > 126) return false;
            Bytes[I] = ANSICHAR(Value[I]);
        }
    Ar.Serialize(Bytes, Length);
    if (Ar.IsError()) return false;
    if (Ar.IsLoading())
    {
        for (int32 I=0; I<Length; ++I) if (Bytes[I] < 32 || Bytes[I] > 126) return false;
        Value = ANSI_TO_TCHAR(Bytes);
    }
    return true;
}
template<typename T> bool EntityId(FArchive& Ar, T& Id)
{
    uint64 Value = Id.GetValue(); uint32 Generation = Id.GetGeneration();
    Ar << Value << Generation;
    if (Ar.IsError() || (Value == 0 && Generation != 0)) return false;
    if (Ar.IsLoading())
    {
        if (Value == 0) Id = T();
        else { const auto Parsed = T::TryCreate(Value,Generation); if (!Parsed) return false; Id=Parsed.Value; }
    }
    return true;
}
bool Body(FArchive& Ar, FHansaLandQueryReply& Reply)
{
    uint8 Version = 2, Accepted = Reply.bAccepted ? 1 : 0;
    auto& Q = Reply.Request;
    FString City = Q.City.ToString();
    Ar << Version << Q.RequestId << Q.Slot;
    if (Version != 2 || !BoundedString(Ar,City)) return false;
    if (Ar.IsLoading()) Q.City=FName(*City);
    Ar << Q.Min.X << Q.Min.Y << Q.Max.X << Q.Max.Y << Accepted;
    if (Ar.IsError() || !Q.IsValid() || Accepted > 1) return false;
    Reply.bAccepted=Accepted != 0;
    if (!Reply.bAccepted) return true;
    auto& R=Reply.Result;
    R.CityId=FHansaCityDefinitionId::TryParse(City).Value;
    if(Q.Slot<5) { R.BoundsMin={Q.Min.X,Q.Min.Y}; R.BoundsMax={Q.Max.X,Q.Max.Y}; }
    if(Q.Slot>=5) Ar << R.BoundsMin.X << R.BoundsMin.Y << R.BoundsMax.X << R.BoundsMax.Y << R.SurveyPages;
    if (!EntityId(Ar,R.ViewerHouseId)) return false;
    Ar << R.StateRevision;
    int32 Count=(Q.Max.X-Q.Min.X+1)*(Q.Max.Y-Q.Min.Y+1);
    if(Q.Slot>=5) { Count=R.Cells.Num();Ar << Count; if(Count<0 || Count>256 || R.SurveyPages<1 || R.SurveyPages>256) return false; }
    if (Ar.IsLoading()) R.Cells.SetNum(Count);
    if (R.Cells.Num()!=Count) return false;
    int32 Index=0;
    for (Index=0;Index<Count;++Index)
    {
        auto& C=R.Cells[Index];
        if(Q.Slot<5) C.Coordinate={Q.Min.X+Index/(Q.Max.Y-Q.Min.Y+1),Q.Min.Y+Index%(Q.Max.Y-Q.Min.Y+1)};
        if(Q.Slot>=5) Ar << C.Coordinate.X << C.Coordinate.Y << C.RunLength << C.RegionId;
        if (!EntityId(Ar,C.RecordedOwnerId) || !EntityId(Ar,C.ViewerLeaseId) || !EntityId(Ar,C.OccupyingBuildingId)) return false;
        uint8 Terrain=uint8(C.Terrain), Access=uint8(C.Access), Reason=uint8(C.Reason);
        uint8 Flags=(C.bSurveyKnown?1:0)|(C.bProtected?2:0);
        Ar << Terrain << Access << Reason << Flags;
        if (Ar.IsError() || Terrain>uint8(EHansaPlacementTerrain::Water) || Access>uint8(EHansaLandAccess::Denied) ||
            Reason>uint8(EHansaLandAccessReason::ForeignPresenceInsufficient) || Flags>3) return false;
        C.Terrain=EHansaPlacementTerrain(Terrain); C.Access=EHansaLandAccess(Access); C.Reason=EHansaLandAccessReason(Reason);
        C.bSurveyKnown=(Flags&1)!=0; C.bProtected=(Flags&2)!=0;
    }
    uint16 Leases=R.ViewerLeases.Num(); Ar << Leases;
    if (Ar.IsError() || Leases>MaxLeases) return false;
    if (Ar.IsLoading()) R.ViewerLeases.SetNum(Leases);
    for (auto& L:R.ViewerLeases)
    {
        if (!EntityId(Ar,L.Id)) return false;
        Ar << L.BoundsMin.X << L.BoundsMin.Y << L.BoundsMax.X << L.BoundsMax.Y;
        uint8 Active=L.bActive?1:0, Categories=L.PermittedBuildingCategories.Num(); Ar << Active << Categories;
        if (Ar.IsError() || Active>1 || Categories>MaxCategories) return false;
        L.bActive=Active!=0;
        if (Ar.IsLoading()) L.PermittedBuildingCategories.SetNum(Categories);
        for (auto& Category:L.PermittedBuildingCategories) if (!BoundedString(Ar,Category)) return false;
    }
    return !Ar.IsError() && Reply.IsValid();
}
}

bool FHansaLandQueryRequest::IsValid() const
{
    const int64 Width=int64(Max.X)-Min.X+1, Height=int64(Max.Y)-Min.Y+1;
    return RequestId>0 && Slot<9 && (Slot<5 || (Min==Max && Min.Y==0 && Min.X>=0 && Min.X<256)) && City.ToString().Len()<=128 &&
        FHansaCityDefinitionId::TryParse(City.ToString()).IsSuccess() && Width>0 && Height>0 && Width<=MaxSide && Height<=MaxSide;
}
bool FHansaLandQueryRequest::SameArea(const FHansaLandQueryRequest& Other) const
{ return Slot==Other.Slot && City==Other.City && Min==Other.Min && Max==Other.Max; }
bool FHansaLandQueryReply::IsValid() const
{
    if (!Request.IsValid()) return false;
    if (!bAccepted) return Result.Cells.IsEmpty() && Result.ViewerLeases.IsEmpty();
    if (Result.Failure!=EHansaLandQueryFailure::None || !Result.ViewerHouseId.IsValid() ||
        Result.CityId.ToString()!=Request.City.ToString() || (Request.Slot<5 && (Result.BoundsMin.X!=Request.Min.X || Result.BoundsMin.Y!=Request.Min.Y ||
        Result.BoundsMax.X!=Request.Max.X || Result.BoundsMax.Y!=Request.Max.Y ||
        Result.Cells.Num()!=(Request.Max.X-Request.Min.X+1)*(Request.Max.Y-Request.Min.Y+1))) || Result.ViewerLeases.Num()>MaxLeases) return false;
    if(Request.Slot>=5)
    {
        if(Result.SurveyPages<1 || Result.SurveyPages>256 || Request.Min.X>=Result.SurveyPages || Result.Cells.IsEmpty() || Result.Cells.Num()>256 || (Request.Min.X+1<Result.SurveyPages && Result.Cells.Num()!=256) ||
            Result.BoundsMax.X<Result.BoundsMin.X || Result.BoundsMax.Y<Result.BoundsMin.Y ||
            int64(Result.BoundsMax.X)-Result.BoundsMin.X>=2048 || int64(Result.BoundsMax.Y)-Result.BoundsMin.Y>=2048) return false;
        for(const auto& C:Result.Cells)
            if(C.RunLength<1 || C.RunLength>2048 || C.RegionId<0 || C.RegionId>65536 || C.Coordinate.X<Result.BoundsMin.X || C.Coordinate.X>Result.BoundsMax.X ||
                C.Coordinate.Y<Result.BoundsMin.Y || int64(C.Coordinate.Y)+C.RunLength-1>Result.BoundsMax.Y) return false;
    }
    int32 Index=0;
    if(Request.Slot<5) for (int64 X=Request.Min.X; X<=Request.Max.X; ++X) for (int64 Y=Request.Min.Y; Y<=Request.Max.Y; ++Y)
    {
        const auto& C=Result.Cells[Index++];
        if (C.Coordinate.X!=X || C.Coordinate.Y!=Y) return false;
    }
    for (const auto& L:Result.ViewerLeases)
    {
        if (!L.Id.IsValid() || L.BoundsMax.X<L.BoundsMin.X || L.BoundsMax.Y<L.BoundsMin.Y ||
            L.PermittedBuildingCategories.Num()>MaxCategories) return false;
        for (const auto& Category:L.PermittedBuildingCategories) if (Category.Len()>128) return false;
    }
    return true;
}
bool FHansaLandQueryReply::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
    constexpr uint32 MaxRaw=128*1024, MaxWire=60*1024;
    bOutSuccess=false;
    TArray<uint8> Raw, Wire; uint32 RawSize=0, WireSize=0;
    auto Fail=[&]{if(Ar.IsLoading())*this={};Ar.SetError();return false;};
    if (Ar.IsSaving())
    {
        if (!IsValid()) return Fail();
        FMemoryWriter Writer(Raw,true);
        if (!Body(Writer,*this) || Raw.IsEmpty() || Raw.Num()>MaxRaw) return Fail();
        RawSize=Raw.Num(); int32 Bound=FCompression::CompressMemoryBound(NAME_Zlib,Raw.Num()); Wire.SetNumUninitialized(Bound);
        if (!FCompression::CompressMemory(NAME_Zlib,Wire.GetData(),Bound,Raw.GetData(),Raw.Num()) || Bound>MaxWire) return Fail();
        WireSize=Bound;
    }
    Ar.SerializeIntPacked(RawSize); Ar.SerializeIntPacked(WireSize);
    if (Ar.IsError() || RawSize==0 || RawSize>MaxRaw || WireSize==0 || WireSize>MaxWire) return Fail();
    if (Ar.IsLoading()) Wire.SetNumUninitialized(WireSize);
    Ar.Serialize(Wire.GetData(),WireSize);
    if (Ar.IsError()) return Fail();
    if (Ar.IsLoading())
    {
        Raw.SetNumUninitialized(RawSize);
        if (!FCompression::UncompressMemory(NAME_Zlib,Raw.GetData(),RawSize,Wire.GetData(),WireSize)) return Fail();
        FHansaLandQueryReply Candidate; FMemoryReader Reader(Raw,true);
        if (!Body(Reader,Candidate) || Reader.Tell()!=Raw.Num()) return Fail();
        *this=MoveTemp(Candidate);
    }
    bOutSuccess=true; return true;
}
bool FHansaLandQueryBudget::Consume(double Now)
{
    if (!FMath::IsFinite(Now)) return false;
    if (LastTime>=0) Tokens=FMath::Min(8.0,Tokens+FMath::Max(0.0,Now-LastTime)*16.0);
    LastTime=Now;
    if (Tokens<1.0) return false;
    Tokens-=1.0; return true;
}
void FHansaLandQueryClientCache::Invalidate()
{
    for (auto& Slot:Slots) Slot={};
    // Keep request IDs monotonic across save/load, city and authority changes.
}
EHansaLandViewStatus FHansaLandQueryClientCache::Poll(uint8 Slot, FName City, FIntPoint Min, FIntPoint Max,
    int64 ViewerHouseId, double Now, FHansaLandQueryRequest& OutRequest, FHansaLandQueryResult& OutResult)
{
    OutRequest={}; OutResult={};
    if (ViewerHouseId!=OwnerHouseId) { Invalidate(); OwnerHouseId=ViewerHouseId; }
    FHansaLandQueryRequest Key; Key.RequestId=NextRequestId; Key.Slot=Slot; Key.City=City; Key.Min=Min; Key.Max=Max;
    if (ViewerHouseId<=0 || !Key.IsValid()) return EHansaLandViewStatus::Unavailable;
    auto& Entry=Slots[Slot];
    if (!Entry.Request.SameArea(Key)) { Entry={}; Entry.Request=Key; }
    if (Entry.bPending && Now-Entry.SentAt>=2.0) Entry.bPending=false;
    if (!Entry.bPending && Now>=Entry.NextRequestAt && Budget.Consume(Now))
    {
        Entry.Request=Key; Entry.Request.RequestId=NextRequestId++;
        Entry.bPending=true; Entry.SentAt=Now; Entry.NextRequestAt=Now+.5;
        OutRequest=Entry.Request;
    }
    if (Entry.bReady && Now-Entry.ReceivedAt<=1.5) { OutResult=Entry.Result; return EHansaLandViewStatus::Ready; }
    return Entry.bRejected ? EHansaLandViewStatus::Unavailable : EHansaLandViewStatus::Pending;
}
bool FHansaLandQueryClientCache::Receive(const FHansaLandQueryReply& Reply, int64 ViewerHouseId, double Now)
{
    if (ViewerHouseId<=0 || ViewerHouseId!=OwnerHouseId || !Reply.IsValid()) return false;
    auto& Entry=Slots[Reply.Request.Slot];
    if (!Entry.bPending || Entry.Request.RequestId!=Reply.Request.RequestId || !Entry.Request.SameArea(Reply.Request)) return false;
    if (Reply.bAccepted && Reply.Result.ViewerHouseId.GetValue()!=uint64(ViewerHouseId)) return false;
    Entry.bPending=false; Entry.bRejected=!Reply.bAccepted; Entry.bReady=Reply.bAccepted;
    Entry.Result=Reply.Result; Entry.ReceivedAt=Now;
    return true;
}
