#pragma once
#include "Commandlets/Commandlet.h"
#include "HansaWorldMapCommandlet.generated.h"

/** Explicit offline authoring of staged campaign geography; no production writes. */
UCLASS()
class HANSAEDITOR_API UHansaWorldMapCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UHansaWorldMapCommandlet();
    virtual int32 Main(const FString& Params) override;
};
