#pragma once
#include "Commandlets/Commandlet.h"
#include "HansaArtisanPlotsCommandlet.generated.h"
UCLASS()
class UHansaArtisanPlotsCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UHansaArtisanPlotsCommandlet();
 virtual int32 Main(const FString& Params) override;
};
