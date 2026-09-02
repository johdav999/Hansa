#include "Model/HansaValueResult.h"

namespace Hansa::Simulation
{
	const TCHAR* LexToString(const EHansaValueError Error)
	{
		switch (Error)
		{
		case EHansaValueError::None: return TEXT("None");
		case EHansaValueError::InvalidFormat: return TEXT("InvalidFormat");
		case EHansaValueError::UnknownDomain: return TEXT("UnknownDomain");
		case EHansaValueError::WrongDomain: return TEXT("WrongDomain");
		case EHansaValueError::InvalidZero: return TEXT("InvalidZero");
		case EHansaValueError::NegativeNotAllowed: return TEXT("NegativeNotAllowed");
		case EHansaValueError::Overflow: return TEXT("Overflow");
		case EHansaValueError::DivisionByZero: return TEXT("DivisionByZero");
		case EHansaValueError::OutOfRange: return TEXT("OutOfRange");
		case EHansaValueError::UnsupportedVersion: return TEXT("UnsupportedVersion");
		case EHansaValueError::UnexpectedType: return TEXT("UnexpectedType");
		case EHansaValueError::TruncatedData: return TEXT("TruncatedData");
		case EHansaValueError::TrailingData: return TEXT("TrailingData");
		default: return TEXT("UnknownError");
		}
	}
}
