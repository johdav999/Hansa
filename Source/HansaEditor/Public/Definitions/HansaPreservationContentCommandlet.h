#pragma once
#include "Commandlets/Commandlet.h"
#include "HansaPreservationContentCommandlet.generated.h"
UCLASS()
class UHansaPreservationContentCommandlet final : public UCommandlet
{
 GENERATED_BODY()
public:
 UHansaPreservationContentCommandlet();
 virtual int32 Main(const FString& Params) override;
};
