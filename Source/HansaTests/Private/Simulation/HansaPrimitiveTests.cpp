#include "Math/HansaDeterministicRandom.h"
#include "Math/HansaFixedPoint.h"
#include "Math/NumericLimits.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"
#include "Save/HansaPrimitiveSerialization.h"

#include <type_traits>

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::Primitives
{
	using namespace Hansa::Simulation;

	static_assert(!std::is_same_v<FHansaGoodId, FHansaRecipeId>);
	static_assert(!std::is_same_v<FHansaHouseId, FHansaBuildingId>);
	static_assert(!std::is_convertible_v<FHansaGoodId, FHansaRecipeId>);
	static_assert(!std::is_convertible_v<FHansaHouseId, FHansaBuildingId>);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPrimitiveIdentifierTest,
	"Hansa.Simulation.Primitives.Identifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPrimitiveIdentifierTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;

	const THansaValueResult<FHansaDefinitionId> Generic = FHansaDefinitionId::TryParse(TEXT("Building.Bakery.Small"));
	TestTrue(TEXT("A registered canonical definition ID parses"), Generic.IsSuccess());
	if (Generic)
	{
		TestEqual(TEXT("The canonical definition text is retained"), Generic.Value.ToString(), FString(TEXT("Building.Bakery.Small")));
		TestEqual(TEXT("The definition domain is explicit"), Generic.Value.GetDomain(), FString(TEXT("Building")));
	}

	const THansaValueResult<FHansaGoodId> Grain = FHansaGoodId::TryParse(TEXT("Good.Grain"));
	const THansaValueResult<FHansaGoodId> Beer = FHansaGoodId::TryParse(TEXT("Good.Beer"));
	TestTrue(TEXT("Typed good IDs parse their own domain"), Grain.IsSuccess() && Beer.IsSuccess());
	TestTrue(TEXT("Typed IDs reject a different registered domain"),
		FHansaGoodId::TryParse(TEXT("Recipe.Bread")).Error == EHansaValueError::WrongDomain);
	TestTrue(TEXT("Unknown domains are rejected"),
		FHansaDefinitionId::TryParse(TEXT("Provider.External42")).Error == EHansaValueError::UnknownDomain);
	TestTrue(TEXT("Empty path segments are rejected"),
		FHansaDefinitionId::TryParse(TEXT("Good..Grain")).Error == EHansaValueError::InvalidFormat);
	TestTrue(TEXT("Non-PascalCase segments are rejected"),
		FHansaDefinitionId::TryParse(TEXT("Good.grain")).Error == EHansaValueError::InvalidFormat);
	TestTrue(TEXT("Asset-like paths are rejected"),
		FHansaDefinitionId::TryParse(TEXT("Good./Game/Grain")).Error == EHansaValueError::InvalidFormat);

	if (Grain && Beer)
	{
		TArray<FHansaGoodId> Ordered = { Grain.Value, Beer.Value };
		Ordered.Sort();
		TestEqual(TEXT("Definition ordering is canonical and case-sensitive"), Ordered[0].ToString(), FString(TEXT("Good.Beer")));
		TestEqual(TEXT("Definition debug text is the canonical ID"), Grain.Value.ToString(), FString(TEXT("Good.Grain")));
	}

	TestTrue(TEXT("Runtime entity zero remains invalid"),
		FHansaRouteId::TryCreate(0).Error == EHansaValueError::InvalidZero);
	const THansaValueResult<FHansaRouteId> RouteGenerationOne = FHansaRouteId::TryCreate(9, 1);
	const THansaValueResult<FHansaRouteId> RouteGenerationTwo = FHansaRouteId::TryCreate(9, 2);
	const THansaValueResult<FHansaRouteId> LaterRoute = FHansaRouteId::TryCreate(10, 0);
	TestTrue(TEXT("Runtime entity IDs with nonzero values are valid"),
		RouteGenerationOne && RouteGenerationTwo && LaterRoute);
	if (RouteGenerationOne && RouteGenerationTwo && LaterRoute)
	{
		TestTrue(TEXT("Entity order compares value before generation"),
			RouteGenerationOne.Value < RouteGenerationTwo.Value && RouteGenerationTwo.Value < LaterRoute.Value);
		TestEqual(TEXT("Entity debug text contains type, value and generation"),
			RouteGenerationTwo.Value.ToDebugString(), FString(TEXT("Route#9@2")));
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPrimitiveCheckedArithmeticTest,
	"Hansa.Simulation.Primitives.CheckedArithmetic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPrimitiveCheckedArithmeticTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;

	const FHansaMoney MaximumMoney = FHansaMoney::FromRaw(TNumericLimits<int64>::Max());
	const FHansaMoney MinimumMoney = FHansaMoney::FromRaw(TNumericLimits<int64>::Lowest());
	TestTrue(TEXT("Addition reports positive overflow"),
		FHansaMoney::TryAdd(MaximumMoney, FHansaMoney::FromRaw(1)).Error == EHansaValueError::Overflow);
	TestTrue(TEXT("Subtraction reports negative overflow"),
		FHansaMoney::TrySubtract(MinimumMoney, FHansaMoney::FromRaw(1)).Error == EHansaValueError::Overflow);

	const FHansaRate Half = FHansaRate::FromPartsPerMillion(500'000);
	const THansaValueResult<FHansaMoney> PositiveHalf = FHansaMoney::FromRaw(3).TryScale(Half);
	const THansaValueResult<FHansaMoney> NegativeHalf = FHansaMoney::FromRaw(-3).TryScale(Half);
	TestTrue(TEXT("Half-away-from-zero scaling succeeds"), PositiveHalf && NegativeHalf);
	if (PositiveHalf && NegativeHalf)
	{
		TestEqual(TEXT("Positive halves round away from zero"), PositiveHalf.Value.GetRawValue(), int64(2));
		TestEqual(TEXT("Negative halves round away from zero"), NegativeHalf.Value.GetRawValue(), int64(-2));
	}

	const THansaValueResult<int64> FloorNegative = FHansaCheckedIntegerMath::TryMultiplyDivide(
		-3, 1, 2, EHansaRoundingMode::Floor);
	const THansaValueResult<int64> CeilingNegative = FHansaCheckedIntegerMath::TryMultiplyDivide(
		-3, 1, 2, EHansaRoundingMode::Ceiling);
	TestTrue(TEXT("Directed rounding succeeds"), FloorNegative && CeilingNegative);
	if (FloorNegative && CeilingNegative)
	{
		TestEqual(TEXT("Floor rounds a negative result down"), FloorNegative.Value, int64(-2));
		TestEqual(TEXT("Ceiling rounds a negative result up"), CeilingNegative.Value, int64(-1));
	}

	const THansaValueResult<int64> LargeExact = FHansaCheckedIntegerMath::TryMultiplyDivide(
		TNumericLimits<int64>::Max(),
		TNumericLimits<int64>::Max(),
		TNumericLimits<int64>::Max(),
		EHansaRoundingMode::TowardZero);
	TestTrue(TEXT("A 128-bit intermediate avoids false overflow"), LargeExact.IsSuccess());
	if (LargeExact)
	{
		TestEqual(TEXT("Large exact multiply/divide keeps its value"), LargeExact.Value, TNumericLimits<int64>::Max());
	}
	TestTrue(TEXT("A final result outside int64 reports overflow"),
		FHansaCheckedIntegerMath::TryMultiplyDivide(
			TNumericLimits<int64>::Max(), 2, 1, EHansaRoundingMode::TowardZero).Error == EHansaValueError::Overflow);
	const THansaValueResult<int64> WideDivision = FHansaCheckedIntegerMath::TryMultiplyDivide(
		TNumericLimits<int64>::Max(), 2, 3, EHansaRoundingMode::TowardZero);
	TestTrue(TEXT("Wide products divide deterministically without intermediate overflow"), WideDivision.IsSuccess());
	if (WideDivision)
	{
		TestEqual(TEXT("Wide quotient is exact toward zero"), WideDivision.Value, int64(6'148'914'691'236'517'204LL));
	}
	const THansaValueResult<int64> MinimumCancellation = FHansaCheckedIntegerMath::TryMultiplyDivide(
		TNumericLimits<int64>::Lowest(), 1, TNumericLimits<int64>::Lowest(), EHansaRoundingMode::TowardZero);
	TestTrue(TEXT("INT64_MIN divisor magnitude is handled without signed negation"), MinimumCancellation.IsSuccess());
	if (MinimumCancellation)
	{
		TestEqual(TEXT("Equal minimum magnitudes cancel"), MinimumCancellation.Value, int64(1));
	}
	TestTrue(TEXT("Negating INT64_MIN through multiply/divide reports overflow"),
		FHansaCheckedIntegerMath::TryMultiplyDivide(
			TNumericLimits<int64>::Lowest(), -1, 1, EHansaRoundingMode::TowardZero).Error == EHansaValueError::Overflow);
	TestTrue(TEXT("Division by zero is structured"),
		FHansaCheckedIntegerMath::TryMultiplyDivide(1, 1, 0, EHansaRoundingMode::TowardZero).Error == EHansaValueError::DivisionByZero);

	const THansaValueResult<FHansaRate> OneThird = FHansaRate::TryRatio(1, 3);
	TestTrue(TEXT("Integer ratios produce fixed-point rates"), OneThird.IsSuccess());
	if (OneThird)
	{
		TestEqual(TEXT("One third rounds deterministically in ppm"), OneThird.Value.GetPartsPerMillion(), int64(333'333));
	}
	TestTrue(TEXT("Normalized rates reject negative values"),
		FHansaRate::TryMakeNormalized(-1).Error == EHansaValueError::OutOfRange);
	TestTrue(TEXT("Normalized rates reject values above one"),
		FHansaRate::TryMakeNormalized(FHansaRate::Scale + 1).Error == EHansaValueError::OutOfRange);
	TestEqual(TEXT("Money debug text records its unit"),
		FHansaMoney::FromRaw(125).ToDebugString(), FString(TEXT("Money[pfennig=125]")));
	TestEqual(TEXT("Quantity debug text records milli-units"),
		FHansaQuantity::FromRaw(-250).ToDebugString(), FString(TEXT("Quantity[milli-unit=-250]")));

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPrimitiveClockTest,
	"Hansa.Simulation.Primitives.Clock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPrimitiveClockTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;

	TestTrue(TEXT("Negative ticks are rejected"),
		FHansaSimulationTick::TryCreate(-1).Error == EHansaValueError::NegativeNotAllowed);
	TestTrue(TEXT("Negative durations are rejected"),
		FHansaSimulationDuration::TryCreate(-1).Error == EHansaValueError::NegativeNotAllowed);
	TestTrue(TEXT("Version zero is invalid"),
		FHansaSimulationVersion::TryCreate(0).Error == EHansaValueError::InvalidZero);

	const THansaValueResult<FHansaSimulationVersion> Version = FHansaSimulationVersion::TryCreate(
		FHansaSimulationClock::CurrentSimulationVersion);
	const THansaValueResult<FHansaSimulationTick> Tick = FHansaSimulationTick::TryCreate(47);
	TestTrue(TEXT("Current version and positive tick parse"), Version && Tick);
	if (!Version || !Tick)
	{
		return false;
	}

	const THansaValueResult<FHansaSimulationClock> Clock = FHansaSimulationClock::TryCreate(Version.Value, Tick.Value);
	TestTrue(TEXT("The current one-hour clock is valid"), Clock.IsSuccess());
	if (Clock)
	{
		const THansaValueResult<FHansaCalendarProjection> Calendar = Clock.Value.TryProjectCalendar();
		TestTrue(TEXT("Calendar projection succeeds"), Calendar.IsSuccess());
		if (Calendar)
		{
			TestEqual(TEXT("Forty-seven hours is day one"), Calendar.Value.ElapsedDays, int64(1));
			TestEqual(TEXT("Forty-seven hours projects to 23:00"), Calendar.Value.HourOfDay, uint8(23));
			TestEqual(TEXT("Hourly ticks have zero projected minutes"), Calendar.Value.MinuteOfHour, uint8(0));
		}

		const THansaValueResult<FHansaSimulationDuration> OneTick = FHansaSimulationDuration::TryCreate(1);
		const THansaValueResult<FHansaSimulationClock> Advanced = Clock.Value.TryAdvance(OneTick.Value);
		TestTrue(TEXT("Advancing by an integer duration succeeds"), Advanced.IsSuccess());
		if (Advanced)
		{
			TestEqual(TEXT("Clock advancement changes only the authoritative tick"), Advanced.Value.GetTick().GetValue(), int64(48));
		}
		TestEqual(TEXT("Clock debug evidence is explicit"),
			Clock.Value.ToDebugString(), FString(TEXT("Clock[simulationVersion=1;tick=47;minutesPerTick=60]")));
	}

	const THansaValueResult<FHansaSimulationVersion> FutureVersion = FHansaSimulationVersion::TryCreate(2);
	TestTrue(TEXT("Unsupported clock versions fail explicitly"),
		FHansaSimulationClock::TryCreate(FutureVersion.Value, Tick.Value).Error == EHansaValueError::UnsupportedVersion);
	TestTrue(TEXT("A zero tick duration is rejected"),
		FHansaSimulationClock::TryCreate(Version.Value, Tick.Value, 0).Error == EHansaValueError::OutOfRange);

	const THansaValueResult<FHansaSimulationTick> MaximumTick = FHansaSimulationTick::TryCreate(TNumericLimits<int64>::Max());
	const THansaValueResult<FHansaSimulationClock> MaximumClock = FHansaSimulationClock::TryCreate(Version.Value, MaximumTick.Value);
	const THansaValueResult<FHansaSimulationDuration> OneTick = FHansaSimulationDuration::TryCreate(1);
	TestTrue(TEXT("Clock advancement reports overflow"),
		MaximumClock.Value.TryAdvance(OneTick.Value).Error == EHansaValueError::Overflow);

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPrimitiveRandomStreamTest,
	"Hansa.Simulation.Primitives.RandomStreams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPrimitiveRandomStreamTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;

	const THansaValueResult<FHansaRandomStream> FirstResult = FHansaRandomStream::TryCreate(0x123456789abcdef0ULL, TEXT("Market.Price"));
	const THansaValueResult<FHansaRandomStream> SecondResult = FHansaRandomStream::TryCreate(0x123456789abcdef0ULL, TEXT("Market.Price"));
	const THansaValueResult<FHansaRandomStream> OtherResult = FHansaRandomStream::TryCreate(0x123456789abcdef0ULL, TEXT("Market.Arrival"));
	TestTrue(TEXT("Canonical named streams can be created"), FirstResult && SecondResult && OtherResult);
	TestTrue(TEXT("Malformed stream names are rejected"),
		FHansaRandomStream::TryCreate(1, TEXT("Market..Price")).Error == EHansaValueError::InvalidFormat);
	if (!FirstResult || !SecondResult || !OtherResult)
	{
		return false;
	}

	FHansaRandomStream First = FirstResult.Value;
	FHansaRandomStream Second = SecondResult.Value;
	FHansaRandomStream Other = OtherResult.Value;
	bool bAllEqual = true;
	for (int32 Index = 0; Index < 256; ++Index)
	{
		bAllEqual &= First.NextUInt64() == Second.NextUInt64();
	}
	TestTrue(TEXT("Equal seed and name produce identical long sequences"), bAllEqual);
	TestTrue(TEXT("Different named streams do not share the same sequence"), FirstResult.Value.GetState() != OtherResult.Value.GetState());

	const THansaValueResult<FHansaRandomStream> KnownResult = FHansaRandomStream::TryRestore(
		TEXT("Test.Known"), EHansaRandomAlgorithm::SplitMix64V1, 0, 0);
	FHansaRandomStream Known = KnownResult.Value;
	TestTrue(TEXT("SplitMix64 V1 has a locked first output"), Known.NextUInt64() == 0xe220a8397b1dcdafULL);
	TestTrue(TEXT("Bound zero is rejected"), Known.TryNextBounded(0).Error == EHansaValueError::OutOfRange);
	bool bBounded = true;
	for (int32 Index = 0; Index < 100; ++Index)
	{
		const THansaValueResult<uint32> Value = Known.TryNextBounded(7);
		bBounded &= Value && Value.Value < 7;
	}
	TestTrue(TEXT("Bounded draws remain in range"), bBounded);
	TestTrue(TEXT("Draw count is serialized state, not hidden state"), Known.GetDrawCount() >= 101);

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPrimitiveSerializationTest,
	"Hansa.Simulation.Primitives.Serialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPrimitiveSerializationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;

	const FHansaGoodId Good = FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
	const FHansaRouteId Route = FHansaRouteId::TryCreate(42, 3).Value;
	const FHansaMoney Money = FHansaMoney::FromRaw(-123456789);
	const FHansaQuantity Quantity = FHansaQuantity::FromRaw(9'876'543'210LL);
	const FHansaRate Rate = FHansaRate::FromPartsPerMillion(1'250'000);
	const FHansaSimulationVersion Version = FHansaSimulationVersion::TryCreate(1).Value;
	const FHansaSimulationTick Tick = FHansaSimulationTick::TryCreate(1234).Value;
	const FHansaSimulationDuration Duration = FHansaSimulationDuration::TryCreate(72).Value;
	const FHansaSimulationClock Clock = FHansaSimulationClock::TryCreate(Version, Tick).Value;
	FHansaRandomStream Random = FHansaRandomStream::TryCreate(987654321, TEXT("Production.Output")).Value;
	Random.NextUInt64();
	Random.NextUInt64();

	FHansaPrimitiveWriter Writer;
	TestTrue(TEXT("Typed definition ID serializes"), Writer.WriteDefinitionId(Good));
	TestTrue(TEXT("Typed entity ID serializes"), Writer.WriteEntityId(Route));
	TestTrue(TEXT("Money serializes"), Writer.WriteMoney(Money));
	TestTrue(TEXT("Quantity serializes"), Writer.WriteQuantity(Quantity));
	TestTrue(TEXT("Rate serializes"), Writer.WriteRate(Rate));
	TestTrue(TEXT("Simulation version serializes"), Writer.WriteSimulationVersion(Version));
	TestTrue(TEXT("Simulation tick serializes"), Writer.WriteSimulationTick(Tick));
	TestTrue(TEXT("Simulation duration serializes"), Writer.WriteSimulationDuration(Duration));
	TestTrue(TEXT("Clock serializes"), Writer.WriteClock(Clock));
	TestTrue(TEXT("Random stream serializes"), Writer.WriteRandomStream(Random));
	TestTrue(TEXT("Writer remains valid"), Writer.IsValid());
	TestTrue(TEXT("The explicit byte envelope has its HPR1 magic"),
		Writer.GetBytes().Num() >= 6 &&
		Writer.GetBytes()[0] == 'H' && Writer.GetBytes()[1] == 'P' &&
		Writer.GetBytes()[2] == 'R' && Writer.GetBytes()[3] == '1');

	FHansaGoodId ReadGood;
	FHansaRouteId ReadRoute;
	FHansaMoney ReadMoney;
	FHansaQuantity ReadQuantity;
	FHansaRate ReadRate;
	FHansaSimulationVersion ReadVersion;
	FHansaSimulationTick ReadTick;
	FHansaSimulationDuration ReadDuration;
	FHansaSimulationClock ReadClock;
	FHansaRandomStream ReadRandom;
	FHansaPrimitiveReader Reader(Writer.GetBytes());
	TestTrue(TEXT("Reader recognizes the current format"), Reader.IsValid());
	TestTrue(TEXT("Typed definition ID deserializes"), Reader.ReadDefinitionId(ReadGood));
	TestTrue(TEXT("Typed entity ID deserializes"), Reader.ReadEntityId(ReadRoute));
	TestTrue(TEXT("Money deserializes"), Reader.ReadMoney(ReadMoney));
	TestTrue(TEXT("Quantity deserializes"), Reader.ReadQuantity(ReadQuantity));
	TestTrue(TEXT("Rate deserializes"), Reader.ReadRate(ReadRate));
	TestTrue(TEXT("Simulation version deserializes"), Reader.ReadSimulationVersion(ReadVersion));
	TestTrue(TEXT("Simulation tick deserializes"), Reader.ReadSimulationTick(ReadTick));
	TestTrue(TEXT("Simulation duration deserializes"), Reader.ReadSimulationDuration(ReadDuration));
	TestTrue(TEXT("Clock deserializes"), Reader.ReadClock(ReadClock));
	TestTrue(TEXT("Random stream deserializes"), Reader.ReadRandomStream(ReadRandom));
	TestTrue(TEXT("Reader consumed exactly one complete envelope"), Reader.Finish());
	TestTrue(TEXT("Definition ID round trip is exact"), ReadGood == Good);
	TestTrue(TEXT("Entity ID round trip is exact"), ReadRoute == Route);
	TestTrue(TEXT("Money round trip is exact"), ReadMoney == Money);
	TestTrue(TEXT("Quantity round trip is exact"), ReadQuantity == Quantity);
	TestTrue(TEXT("Rate round trip is exact"), ReadRate == Rate);
	TestTrue(TEXT("Simulation version round trip is exact"), ReadVersion == Version);
	TestTrue(TEXT("Simulation tick round trip is exact"), ReadTick == Tick);
	TestTrue(TEXT("Simulation duration round trip is exact"), ReadDuration == Duration);
	TestTrue(TEXT("Clock round trip is exact"), ReadClock == Clock);
	TestTrue(TEXT("Random stream state round trip is exact"), ReadRandom == Random);
	TestTrue(TEXT("Restored random streams continue with the identical next output"), ReadRandom.NextUInt64() == Random.NextUInt64());

	TArray<uint8> FutureFormat = Writer.GetBytes();
	FutureFormat[4] = 2;
	FutureFormat[5] = 0;
	FHansaPrimitiveReader FutureReader(FutureFormat);
	TestTrue(TEXT("Unknown serialization versions fail explicitly"),
		FutureReader.GetError() == EHansaValueError::UnsupportedVersion);

	FHansaPrimitiveWriter WrongTypeWriter;
	WrongTypeWriter.WriteMoney(Money);
	FHansaPrimitiveReader WrongTypeReader(WrongTypeWriter.GetBytes());
	FHansaQuantity WrongTypeQuantity;
	TestFalse(TEXT("Type tags prevent reading money as quantity"), WrongTypeReader.ReadQuantity(WrongTypeQuantity));
	TestTrue(TEXT("Wrong primitive type is structured"), WrongTypeReader.GetError() == EHansaValueError::UnexpectedType);

	TArray<uint8> Truncated = Writer.GetBytes();
	Truncated.RemoveAt(Truncated.Num() - 1);
	FHansaPrimitiveReader TruncatedReader(Truncated);
	TruncatedReader.ReadDefinitionId(ReadGood);
	TruncatedReader.ReadEntityId(ReadRoute);
	TruncatedReader.ReadMoney(ReadMoney);
	TruncatedReader.ReadQuantity(ReadQuantity);
	TruncatedReader.ReadRate(ReadRate);
	TruncatedReader.ReadSimulationVersion(ReadVersion);
	TruncatedReader.ReadSimulationTick(ReadTick);
	TruncatedReader.ReadSimulationDuration(ReadDuration);
	TruncatedReader.ReadClock(ReadClock);
	TestFalse(TEXT("Truncated random state does not deserialize"), TruncatedReader.ReadRandomStream(ReadRandom));
	TestTrue(TEXT("Truncation is structured"), TruncatedReader.GetError() == EHansaValueError::TruncatedData);

	TArray<uint8> Trailing = WrongTypeWriter.GetBytes();
	Trailing.Add(0xff);
	FHansaPrimitiveReader TrailingReader(Trailing);
	TrailingReader.ReadMoney(ReadMoney);
	TestFalse(TEXT("Trailing bytes are rejected at the explicit envelope boundary"), TrailingReader.Finish());
	TestTrue(TEXT("Trailing data is structured"), TrailingReader.GetError() == EHansaValueError::TrailingData);

	return !HasAnyErrors();
}

#endif
