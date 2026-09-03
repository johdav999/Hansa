#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "SemanticUI/HansaSemanticUiRegistry.h"
#include "Templates/Function.h"

namespace Hansa::Automation
{
	enum class EHansaSemanticProperty : uint8
	{
		Exists = 0,
		Visible,
		Enabled,
		Focused,
		Selected,
		Loading,
		Warning,
		Error
	};

	HANSAAUTOMATION_API const TCHAR* LexToString(EHansaSemanticProperty Property);
	HANSAAUTOMATION_API bool TryParseSemanticProperty(const FString& Text, EHansaSemanticProperty& OutProperty);

	struct HANSAAUTOMATION_API FHansaSemanticPredicate final
	{
		FString SemanticId;
		EHansaSemanticProperty Property = EHansaSemanticProperty::Exists;
		bool bExpected = true;
	};

	struct HANSAAUTOMATION_API FHansaSemanticWaitResult final
	{
		FString WaitId;
		FHansaSemanticPredicate Predicate;
		bool bMatched = false;
		bool bTimedOut = false;
		uint64 ObservedRevision = 0;
	};

	/** Advances pending predicates from caller ticks; never sleeps or blocks the game thread. */
	class HANSAAUTOMATION_API FHansaAutomationWaitService final
	{
	public:
		using FCompletion = TFunction<void(const FHansaSemanticWaitResult&)>;

		explicit FHansaAutomationWaitService(const FHansaSemanticUiRegistry& InRegistry);
		bool BeginWait(
			const FString& WaitId,
			const FHansaSemanticPredicate& Predicate,
			int64 DeadlineMonotonicMilliseconds,
			FCompletion Completion);
		void Tick(int64 NowMonotonicMilliseconds);
		void CancelAll();
		[[nodiscard]] int32 NumPending() const { return Pending.Num(); }
		[[nodiscard]] FHansaSemanticWaitResult Observe(
			const FString& WaitId,
			const FHansaSemanticPredicate& Predicate) const;

	private:
		struct FPendingWait final
		{
			FString WaitId;
			FHansaSemanticPredicate Predicate;
			int64 DeadlineMonotonicMilliseconds = 0;
			FCompletion Completion;
		};

		const FHansaSemanticUiRegistry& Registry;
		TArray<FPendingWait> Pending;
	};
}
