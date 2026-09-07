#pragma once
#include "Commandlets/Commandlet.h"
#include "HansaMediaAuditCommandlet.generated.h"

UCLASS()
class UHansaMediaAuditCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    UHansaMediaAuditCommandlet();
    virtual int32 Main(const FString& Params) override;
};
