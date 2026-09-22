#pragma once
#include "Commandlets/Commandlet.h"
#include "HansaFirewoodCommandlet.generated.h"

/** Stages firewood drafts; explicit PromoteApproved and VerifyApproved switches apply the reviewed expansion. */
UCLASS()
class HANSAEDITOR_API UHansaFirewoodCommandlet final : public UCommandlet
{
	GENERATED_BODY()
public:
	UHansaFirewoodCommandlet();
	virtual int32 Main(const FString& Params) override;
};
