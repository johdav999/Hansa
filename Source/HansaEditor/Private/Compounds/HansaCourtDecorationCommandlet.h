#pragma once
#include "Commandlets/Commandlet.h"
#include "HansaCourtDecorationCommandlet.generated.h"
/** Validates a court art revision and exports its catalogue diff before optional apply. */
UCLASS()
class UHansaCourtDecorationCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UHansaCourtDecorationCommandlet();
 virtual int32 Main(const FString& Params) override;
};