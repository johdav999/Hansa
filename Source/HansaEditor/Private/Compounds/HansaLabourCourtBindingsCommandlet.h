#pragma once
#include "Commandlets/Commandlet.h"
#include "HansaLabourCourtBindingsCommandlet.generated.h"
/** Creates new residence identities from reviewed, already promoted compounds. Dry run by default. */
UCLASS()
class UHansaLabourCourtBindingsCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UHansaLabourCourtBindingsCommandlet();
 virtual int32 Main(const FString& Params) override;
};
