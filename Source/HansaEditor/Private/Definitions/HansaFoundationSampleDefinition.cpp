#include "Definitions/HansaFoundationSampleDefinition.h"

UHansaFoundationSampleDefinition::UHansaFoundationSampleDefinition()
{
	StableDefinitionId = TEXT("Good.FoundationSample");
	DisplayName = NSLOCTEXT("HansaFoundationSample", "DisplayName", "Foundation sample");
	LocalizationKey = TEXT("Game.Foundation.Sample");
	DefinitionCategory = TEXT("Foundation");
	ContentSet = TEXT("Developer");
	RefreshContentHash();
}

void UHansaFoundationSampleDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	if (SampleValue < 0 || SampleValue > 100)
	{
		OutIssues.Add(FHansaDefinitionValidationIssue {
			EHansaDefinitionValidationSeverity::Error,
			TEXT("HSA-SAMPLE-001"),
			TEXT("SampleValue"),
			NSLOCTEXT("HansaFoundationSample", "OutOfRange", "Sample value is outside its inclusive 0–100 range."),
			NSLOCTEXT("HansaFoundationSample", "OutOfRangeRemedy", "Enter a value from 0 through 100.")
		});
	}
}

void UHansaFoundationSampleDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	InOutCanonicalData += FString::Printf(TEXT("sampleValue=%d\n"), SampleValue);
}
