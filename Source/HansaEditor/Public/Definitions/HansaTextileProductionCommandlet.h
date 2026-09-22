#pragma once

#include "Commandlets/Commandlet.h"
#include "HansaTextileProductionCommandlet.generated.h"

UCLASS()
class HANSAEDITOR_API UHansaTextileProductionCommandlet final : public UCommandlet
{
	GENERATED_BODY()
public:
	UHansaTextileProductionCommandlet();
	virtual int32 Main(const FString& Params) override;
};
