#include "Model/HansaIds.h"

namespace Hansa::Simulation
{
	namespace
	{
		constexpr const TCHAR* RegisteredDomains[] = {
			TEXT("Good"),
			TEXT("Recipe"),
			TEXT("Building"),
			TEXT("Need"),
			TEXT("PopulationTier"),
			TEXT("Vehicle"),
			TEXT("City"),
			TEXT("Region"),
			TEXT("Technology"),
			TEXT("Event"),
			TEXT("Victory"),
			TEXT("Scenario")
		};

		bool IsAsciiLetter(const TCHAR Character)
		{
			return (Character >= TEXT('A') && Character <= TEXT('Z')) ||
				(Character >= TEXT('a') && Character <= TEXT('z'));
		}

		bool IsAsciiDigit(const TCHAR Character)
		{
			return Character >= TEXT('0') && Character <= TEXT('9');
		}

		bool IsCanonicalSegment(const FString& Segment)
		{
			if (Segment.IsEmpty() || Segment[0] < TEXT('A') || Segment[0] > TEXT('Z'))
			{
				return false;
			}

			for (const TCHAR Character : Segment)
			{
				if (!IsAsciiLetter(Character) && !IsAsciiDigit(Character))
				{
					return false;
				}
			}
			return true;
		}
	}

	bool FHansaDefinitionDomains::IsRegistered(const FString& Domain)
	{
		for (const TCHAR* RegisteredDomain : RegisteredDomains)
		{
			if (Domain.Equals(RegisteredDomain, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	THansaValueResult<FHansaDefinitionId> FHansaDefinitionId::TryParse(const FString& Text)
	{
		if (Text.StartsWith(TEXT(".")) || Text.EndsWith(TEXT(".")) || Text.Contains(TEXT("..")))
		{
			return THansaValueResult<FHansaDefinitionId>::Failure(EHansaValueError::InvalidFormat);
		}

		TArray<FString> Segments;
		Text.ParseIntoArray(Segments, TEXT("."), false);
		if (Segments.Num() < 2)
		{
			return THansaValueResult<FHansaDefinitionId>::Failure(EHansaValueError::InvalidFormat);
		}

		for (const FString& Segment : Segments)
		{
			if (!IsCanonicalSegment(Segment))
			{
				return THansaValueResult<FHansaDefinitionId>::Failure(EHansaValueError::InvalidFormat);
			}
		}

		if (!FHansaDefinitionDomains::IsRegistered(Segments[0]))
		{
			return THansaValueResult<FHansaDefinitionId>::Failure(EHansaValueError::UnknownDomain);
		}

		return THansaValueResult<FHansaDefinitionId>::Success(FHansaDefinitionId(Text));
	}

	FString FHansaDefinitionId::GetDomain() const
	{
		FString Domain;
		FString Remainder;
		Canonical.Split(TEXT("."), &Domain, &Remainder, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		return Domain;
	}
}
