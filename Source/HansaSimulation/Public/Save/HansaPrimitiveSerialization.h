#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Math/HansaDeterministicRandom.h"
#include "Math/HansaFixedPoint.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"

namespace Hansa::Simulation
{
	class HANSASIMULATION_API FHansaPrimitiveWriter final
	{
	public:
		static constexpr uint16 CurrentFormatVersion = 1;

		FHansaPrimitiveWriter();

		bool WriteDefinitionId(const FHansaDefinitionId& Value);

		template <typename TTraits>
		bool WriteDefinitionId(const THansaDefinitionId<TTraits>& Value)
		{
			if (!Value.IsValid())
			{
				SetError(EHansaValueError::InvalidFormat);
				return false;
			}
			WriteUInt8(Value.GetSerializationTag());
			return WriteAsciiString(Value.ToString());
		}

		template <typename TTraits>
		bool WriteEntityId(const THansaEntityId<TTraits>& Value)
		{
			if (!Value.IsValid())
			{
				SetError(EHansaValueError::InvalidZero);
				return false;
			}
			WriteUInt8(Value.GetSerializationTag());
			WriteUInt64(Value.GetValue());
			WriteUInt32(Value.GetGeneration());
			return true;
		}

		bool WriteMoney(FHansaMoney Value);
		bool WriteQuantity(FHansaQuantity Value);
		bool WriteRate(FHansaRate Value);
		bool WriteSimulationVersion(FHansaSimulationVersion Value);
		bool WriteSimulationTick(FHansaSimulationTick Value);
		bool WriteSimulationDuration(FHansaSimulationDuration Value);
		bool WriteClock(const FHansaSimulationClock& Value);
		bool WriteRandomStream(const FHansaRandomStream& Value);

		[[nodiscard]] const TArray<uint8>& GetBytes() const { return Bytes; }
		[[nodiscard]] EHansaValueError GetError() const { return Error; }
		[[nodiscard]] bool IsValid() const { return Error == EHansaValueError::None; }

	private:
		void SetError(EHansaValueError InError);
		bool WriteAsciiString(const FString& Value);
		void WriteUInt8(uint8 Value);
		void WriteUInt16(uint16 Value);
		void WriteUInt32(uint32 Value);
		void WriteUInt64(uint64 Value);
		void WriteInt64(int64 Value);

		TArray<uint8> Bytes;
		EHansaValueError Error = EHansaValueError::None;
	};

	class HANSASIMULATION_API FHansaPrimitiveReader final
	{
	public:
		explicit FHansaPrimitiveReader(TConstArrayView<uint8> Bytes);

		bool ReadDefinitionId(FHansaDefinitionId& OutValue);

		template <typename TTraits>
		bool ReadDefinitionId(THansaDefinitionId<TTraits>& OutValue)
		{
			if (!ExpectType(TTraits::SerializationTag))
			{
				return false;
			}
			FString Text;
			if (!ReadAsciiString(Text))
			{
				return false;
			}
			const THansaValueResult<THansaDefinitionId<TTraits>> Parsed = THansaDefinitionId<TTraits>::TryParse(Text);
			if (!Parsed)
			{
				SetError(Parsed.Error);
				return false;
			}
			OutValue = Parsed.Value;
			return true;
		}

		template <typename TTraits>
		bool ReadEntityId(THansaEntityId<TTraits>& OutValue)
		{
			if (!ExpectType(TTraits::SerializationTag))
			{
				return false;
			}
			uint64 Value = 0;
			uint32 Generation = 0;
			if (!ReadUInt64(Value) || !ReadUInt32(Generation))
			{
				return false;
			}
			const THansaValueResult<THansaEntityId<TTraits>> Parsed = THansaEntityId<TTraits>::TryCreate(Value, Generation);
			if (!Parsed)
			{
				SetError(Parsed.Error);
				return false;
			}
			OutValue = Parsed.Value;
			return true;
		}

		bool ReadMoney(FHansaMoney& OutValue);
		bool ReadQuantity(FHansaQuantity& OutValue);
		bool ReadRate(FHansaRate& OutValue);
		bool ReadSimulationVersion(FHansaSimulationVersion& OutValue);
		bool ReadSimulationTick(FHansaSimulationTick& OutValue);
		bool ReadSimulationDuration(FHansaSimulationDuration& OutValue);
		bool ReadClock(FHansaSimulationClock& OutValue);
		bool ReadRandomStream(FHansaRandomStream& OutValue);

		bool Finish();
		[[nodiscard]] EHansaValueError GetError() const { return Error; }
		[[nodiscard]] bool IsValid() const { return Error == EHansaValueError::None; }

	private:
		void SetError(EHansaValueError InError);
		bool ExpectType(uint8 ExpectedType);
		bool ReadAsciiString(FString& OutValue);
		bool ReadUInt8(uint8& OutValue);
		bool ReadUInt16(uint16& OutValue);
		bool ReadUInt32(uint32& OutValue);
		bool ReadUInt64(uint64& OutValue);
		bool ReadInt64(int64& OutValue);

		TConstArrayView<uint8> Bytes;
		int32 Offset = 0;
		EHansaValueError Error = EHansaValueError::None;
	};
}
