#pragma once
#include "Commandlets/Commandlet.h"
#include "HansaLabourCourtsCommandlet.generated.h"
/** Bounded, opt-in draft import and review settlement. Never promotes content. */
UCLASS()
class UHansaLabourCourtsCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UHansaLabourCourtsCommandlet();
 virtual int32 Main(const FString& Params) override;
};
