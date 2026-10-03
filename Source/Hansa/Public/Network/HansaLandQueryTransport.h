#pragma once

#include "CoreMinimal.h"
#include "Queries/HansaLandQuery.h"
#include "HansaLandQueryTransport.generated.h"

/** A connection requests geometry, never a viewer/house identity. Nine slots cover
 * legacy exact chunks, a pinned cell and four in-flight compact survey pages. The wire limit is smaller than QueryLand. */
USTRUCT()
struct HANSA_API FHansaLandQueryRequest
{
    GENERATED_BODY()
    UPROPERTY() int64 RequestId = 0;
    UPROPERTY() uint8 Slot = 0;
    UPROPERTY() FName City;
    UPROPERTY() FIntPoint Min = FIntPoint::ZeroValue;
    UPROPERTY() FIntPoint Max = FIntPoint::ZeroValue;
    bool IsValid() const;
    bool SameArea(const FHansaLandQueryRequest& Other) const;
};

/** Owner-only RPC payload. Bounded decoding precedes allocation of cells/leases. */
USTRUCT()
struct HANSA_API FHansaLandQueryReply
{
    GENERATED_BODY()
    FHansaLandQueryRequest Request;
    bool bAccepted = false;
    Hansa::Simulation::FHansaLandQueryResult Result;
    bool IsValid() const;
    bool NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess);
};
template<> struct TStructOpsTypeTraits<FHansaLandQueryReply> : TStructOpsTypeTraitsBase2<FHansaLandQueryReply>
{ enum { WithNetSerializer = true }; };

enum class EHansaLandViewStatus : uint8 { Pending, Ready, Unavailable };

/** Burst of eight, then sixteen queries/second per connection, including invalid requests. */
struct HANSA_API FHansaLandQueryBudget
{
    bool Consume(double Now);
private:
    double LastTime = -1;
    double Tokens = 8;
};

/** Session-only client cache. Request correlation, not simulation revision ordering,
 * rejects obsolete replies (a restored save may move its revision backwards). */
class HANSA_API FHansaLandQueryClientCache
{
public:
    EHansaLandViewStatus Poll(uint8 Slot, FName City, FIntPoint Min, FIntPoint Max,
        int64 ViewerHouseId, double Now, FHansaLandQueryRequest& OutRequest,
        Hansa::Simulation::FHansaLandQueryResult& OutResult);
    bool Receive(const FHansaLandQueryReply& Reply, int64 ViewerHouseId, double Now);
    void Invalidate();
private:
    struct FSlot
    {
        FHansaLandQueryRequest Request;
        Hansa::Simulation::FHansaLandQueryResult Result;
        double SentAt = -10, ReceivedAt = -10, NextRequestAt = 0;
        bool bPending = false, bReady = false, bRejected = false;
    };
    FSlot Slots[9];
    int64 OwnerHouseId = 0, NextRequestId = 1;
    FHansaLandQueryBudget Budget;
};
