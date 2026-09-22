#pragma once
#include "Commandlets/Commandlet.h"
#include "HansaCompoundPreviewCommandlet.generated.h"
/** Explicit authoring-only fixture command. Not called by ordinary validation/CI. */
UCLASS()
class UHansaCompoundPreviewCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UHansaCompoundPreviewCommandlet();
 virtual int32 Main(const FString& Params) override;
};
