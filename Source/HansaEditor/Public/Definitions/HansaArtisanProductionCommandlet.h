#pragma once
#include "Commandlets/Commandlet.h"
#include "HansaArtisanProductionCommandlet.generated.h"

UCLASS()
class HANSAEDITOR_API UHansaArtisanProductionCommandlet final : public UCommandlet
{
 GENERATED_BODY()
public:
 UHansaArtisanProductionCommandlet();
 virtual int32 Main(const FString& Params) override;
};
