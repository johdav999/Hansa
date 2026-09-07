#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace Hansa::Editor::Generation
{
/** Editor-owned staging and create-only atomic promotion. No provider calls. */
class FHansaStagedMedia final
{
public:
    static bool Stage(const FString& DescriptorPath, FString& OutReceiptPath, FString& Error);
    static bool Preview(const FString& ReceiptPath, FString& OutReviewHash, FString& Error);
    static bool Promote(const FString& ReceiptPath, const FString& Destination,
        const FString& StableId, const FString& Reviewer, const FString& RightsStatement,
        const FString& ReviewedHash, bool bApprove, FString& Error);
    static bool VerifyPromotion(const FString& ReceiptPath, FString& Error);
    static bool AuditReferences(TArray<FString>& Errors);
    static bool IsForbiddenPackage(const FString& Package);
    static bool IsProductionDestination(const FString& Package);
    static bool HashFile(const FString& Filename, FString& Hash);
    static bool bFailBeforeCommitForTests;
private:
    static bool LoadReview(const FString& ReceiptPath, TSharedPtr<FJsonObject>& Receipt,
        TArray<UObject*>& Assets, FString& ReviewHash, FString& Error);
};
}
