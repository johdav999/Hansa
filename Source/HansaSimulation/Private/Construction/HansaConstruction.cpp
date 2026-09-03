#include "Construction/HansaConstruction.h"

namespace Hansa::Simulation
{
	const TCHAR* LexToString(const EHansaConstructionState State)
	{
		switch (State)
		{
		case EHansaConstructionState::UnderConstruction: return TEXT("UnderConstruction");
		case EHansaConstructionState::Completed: return TEXT("Completed");
		default: return TEXT("UnknownConstructionState");
		}
	}

	bool FHansaConstructionCostProjection::IsAffordable() const
	{
		if (MissingCurrency.GetRawValue() != 0)
		{
			return false;
		}
		for (const FHansaConstructionResourceCostProjection& Resource : Resources)
		{
			if (Resource.Missing.GetRawValue() != 0)
			{
				return false;
			}
		}
		return true;
	}
}
