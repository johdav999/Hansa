#pragma once
#include "Commandlets/Commandlet.h"
#include "HansaRoadMaterialCommandlet.generated.h"

/** Explicit, repeatable material graph amendment; no provider calls or source-art changes. */
UCLASS()
class HANSAEDITOR_API UHansaRoadMaterialCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UHansaRoadMaterialCommandlet();
    virtual int32 Main(const FString& Params) override;
};
