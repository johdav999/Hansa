#include "Synchronization/HansaAutomationWaitService.h"

namespace Hansa::Automation
{
	const TCHAR* LexToString(const EHansaSemanticProperty Property)
	{
		switch (Property)
		{
		case EHansaSemanticProperty::Exists: return TEXT("exists");
		case EHansaSemanticProperty::Visible: return TEXT("visible");
		case EHansaSemanticProperty::Enabled: return TEXT("enabled");
		case EHansaSemanticProperty::Focused: return TEXT("focused");
		case EHansaSemanticProperty::Selected: return TEXT("selected");
		case EHansaSemanticProperty::Loading: return TEXT("loading");
		case EHansaSemanticProperty::Warning: return TEXT("warning");
		case EHansaSemanticProperty::Error: return TEXT("error");
		default: return TEXT("unknown");
		}
	}

	bool TryParseSemanticProperty(const FString& Text, EHansaSemanticProperty& OutProperty)
	{
		for (const EHansaSemanticProperty Candidate : {
			EHansaSemanticProperty::Exists, EHansaSemanticProperty::Visible,
			EHansaSemanticProperty::Enabled, EHansaSemanticProperty::Focused,
			EHansaSemanticProperty::Selected, EHansaSemanticProperty::Loading,
			EHansaSemanticProperty::Warning, EHansaSemanticProperty::Error })
		{
			if (Text.Equals(LexToString(Candidate), ESearchCase::IgnoreCase))
			{
				OutProperty = Candidate;
				return true;
			}
		}
		return false;
	}

	FHansaAutomationWaitService::FHansaAutomationWaitService(const FHansaSemanticUiRegistry& InRegistry)
		: Registry(InRegistry)
	{
	}

	bool FHansaAutomationWaitService::BeginWait(
		const FString& WaitId,
		const FHansaSemanticPredicate& Predicate,
		const int64 DeadlineMonotonicMilliseconds,
		FCompletion Completion)
	{
		if (WaitId.IsEmpty() || !IsValidSemanticId(Predicate.SemanticId) ||
			DeadlineMonotonicMilliseconds <= 0 || !Completion ||
			Pending.ContainsByPredicate([&WaitId](const FPendingWait& Item) { return Item.WaitId == WaitId; }))
		{
			return false;
		}
		const FHansaSemanticWaitResult Current = Observe(WaitId, Predicate);
		if (Current.bMatched)
		{
			Completion(Current);
			return true;
		}
		Pending.Add(FPendingWait { WaitId, Predicate, DeadlineMonotonicMilliseconds, MoveTemp(Completion) });
		return true;
	}

	void FHansaAutomationWaitService::Tick(const int64 NowMonotonicMilliseconds)
	{
		for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
		{
			FHansaSemanticWaitResult Result = Observe(Pending[Index].WaitId, Pending[Index].Predicate);
			Result.bTimedOut = !Result.bMatched && NowMonotonicMilliseconds >= Pending[Index].DeadlineMonotonicMilliseconds;
			if (Result.bMatched || Result.bTimedOut)
			{
				FCompletion Completion = MoveTemp(Pending[Index].Completion);
				Pending.RemoveAt(Index, 1, EAllowShrinking::No);
				Completion(Result);
			}
		}
	}

	void FHansaAutomationWaitService::CancelAll()
	{
		Pending.Reset();
	}

	FHansaSemanticWaitResult FHansaAutomationWaitService::Observe(
		const FString& WaitId,
		const FHansaSemanticPredicate& Predicate) const
	{
		FHansaSemanticWaitResult Result;
		Result.WaitId = WaitId;
		Result.Predicate = Predicate;
		Result.ObservedRevision = Registry.GetRevision();
		const FHansaSemanticNode* Node = Registry.FindNode(Predicate.SemanticId);
		bool Observed = Node != nullptr;
		if (Node != nullptr)
		{
			switch (Predicate.Property)
			{
			case EHansaSemanticProperty::Exists: Observed = true; break;
			case EHansaSemanticProperty::Visible: Observed = Node->State.bVisible; break;
			case EHansaSemanticProperty::Enabled: Observed = Node->State.bEnabled; break;
			case EHansaSemanticProperty::Focused: Observed = Node->State.bFocused; break;
			case EHansaSemanticProperty::Selected: Observed = Node->State.bSelected; break;
			case EHansaSemanticProperty::Loading: Observed = Node->State.bLoading; break;
			case EHansaSemanticProperty::Warning: Observed = Node->State.bWarning; break;
			case EHansaSemanticProperty::Error: Observed = Node->State.bError; break;
			default: Observed = false; break;
			}
		}
		else if (Predicate.Property != EHansaSemanticProperty::Exists)
		{
			return Result;
		}
		Result.bMatched = Observed == Predicate.bExpected;
		return Result;
	}
}
