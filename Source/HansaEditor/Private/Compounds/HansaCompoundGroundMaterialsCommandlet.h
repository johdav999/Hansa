#pragma once
#include "Commandlets/Commandlet.h"
#include "HansaCompoundGroundMaterialsCommandlet.generated.h"
UCLASS()
class UHansaCompoundGroundMaterialsCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UHansaCompoundGroundMaterialsCommandlet();
 virtual int32 Main(const FString& Params) override;
};
