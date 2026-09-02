#pragma once

#include "CoreTypes.h"

namespace Hansa::Simulation
{
	enum class EHansaValueError : uint8
	{
		None = 0,
		InvalidFormat,
		UnknownDomain,
		WrongDomain,
		InvalidZero,
		NegativeNotAllowed,
		Overflow,
		DivisionByZero,
		OutOfRange,
		UnsupportedVersion,
		UnexpectedType,
		TruncatedData,
		TrailingData
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaValueError Error);

	template <typename TValue>
	struct THansaValueResult
	{
		TValue Value{};
		EHansaValueError Error = EHansaValueError::None;

		[[nodiscard]] bool IsSuccess() const
		{
			return Error == EHansaValueError::None;
		}

		explicit operator bool() const
		{
			return IsSuccess();
		}

		static THansaValueResult Success(const TValue& InValue)
		{
			THansaValueResult Result;
			Result.Value = InValue;
			return Result;
		}

		static THansaValueResult Failure(EHansaValueError InError)
		{
			THansaValueResult Result;
			Result.Error = InError;
			return Result;
		}
	};
}
