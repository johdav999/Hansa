#include "Save/HansaPrimitiveSerialization.h"

#include "Math/NumericLimits.h"

namespace Hansa::Simulation
{
	namespace
	{
		constexpr uint32 PrimitiveMagic = 0x31525048U; // ASCII "HPR1" in little-endian byte order.
		constexpr uint8 GenericDefinitionIdTag = 9;
	}

	FHansaPrimitiveWriter::FHansaPrimitiveWriter()
	{
		WriteUInt32(PrimitiveMagic);
		WriteUInt16(CurrentFormatVersion);
	}

	bool FHansaPrimitiveWriter::WriteDefinitionId(const FHansaDefinitionId& Value)
	{
		if (!Value.IsValid())
		{
			SetError(EHansaValueError::InvalidFormat);
			return false;
		}
		WriteUInt8(GenericDefinitionIdTag);
		return WriteAsciiString(Value.ToString());
	}

	bool FHansaPrimitiveWriter::WriteMoney(const FHansaMoney Value)
	{
		WriteUInt8(FHansaMoney::GetSerializationTag());
		WriteInt64(Value.GetRawValue());
		return IsValid();
	}

	bool FHansaPrimitiveWriter::WriteQuantity(const FHansaQuantity Value)
	{
		WriteUInt8(FHansaQuantity::GetSerializationTag());
		WriteInt64(Value.GetRawValue());
		return IsValid();
	}

	bool FHansaPrimitiveWriter::WriteRate(const FHansaRate Value)
	{
		WriteUInt8(FHansaRate::SerializationTag);
		WriteInt64(Value.GetPartsPerMillion());
		return IsValid();
	}

	bool FHansaPrimitiveWriter::WriteSimulationVersion(const FHansaSimulationVersion Value)
	{
		if (!Value.IsValid())
		{
			SetError(EHansaValueError::InvalidZero);
			return false;
		}
		WriteUInt8(FHansaSimulationVersion::SerializationTag);
		WriteUInt32(Value.GetValue());
		return IsValid();
	}

	bool FHansaPrimitiveWriter::WriteSimulationTick(const FHansaSimulationTick Value)
	{
		WriteUInt8(FHansaSimulationTick::SerializationTag);
		WriteInt64(Value.GetValue());
		return IsValid();
	}

	bool FHansaPrimitiveWriter::WriteSimulationDuration(const FHansaSimulationDuration Value)
	{
		WriteUInt8(FHansaSimulationDuration::SerializationTag);
		WriteInt64(Value.GetTicks());
		return IsValid();
	}

	bool FHansaPrimitiveWriter::WriteClock(const FHansaSimulationClock& Value)
	{
		if (!Value.GetVersion().IsValid())
		{
			SetError(EHansaValueError::InvalidZero);
			return false;
		}
		WriteUInt8(FHansaSimulationClock::SerializationTag);
		WriteUInt32(Value.GetVersion().GetValue());
		WriteInt64(Value.GetTick().GetValue());
		WriteUInt16(Value.GetMinutesPerTick());
		return IsValid();
	}

	bool FHansaPrimitiveWriter::WriteRandomStream(const FHansaRandomStream& Value)
	{
		if (Value.GetName().IsEmpty())
		{
			SetError(EHansaValueError::InvalidFormat);
			return false;
		}
		WriteUInt8(FHansaRandomStream::SerializationTag);
		if (!WriteAsciiString(Value.GetName()))
		{
			return false;
		}
		WriteUInt8(static_cast<uint8>(Value.GetAlgorithm()));
		WriteUInt64(Value.GetState());
		WriteUInt64(Value.GetDrawCount());
		return IsValid();
	}

	void FHansaPrimitiveWriter::SetError(const EHansaValueError InError)
	{
		if (Error == EHansaValueError::None)
		{
			Error = InError;
		}
	}

	bool FHansaPrimitiveWriter::WriteAsciiString(const FString& Value)
	{
		if (Value.Len() < 0 || static_cast<uint64>(Value.Len()) > TNumericLimits<uint32>::Max())
		{
			SetError(EHansaValueError::OutOfRange);
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (Character > 0x7f)
			{
				SetError(EHansaValueError::InvalidFormat);
				return false;
			}
		}

		WriteUInt32(static_cast<uint32>(Value.Len()));
		for (const TCHAR Character : Value)
		{
			WriteUInt8(static_cast<uint8>(Character));
		}
		return true;
	}

	void FHansaPrimitiveWriter::WriteUInt8(const uint8 Value)
	{
		Bytes.Add(Value);
	}

	void FHansaPrimitiveWriter::WriteUInt16(const uint16 Value)
	{
		for (uint32 ByteIndex = 0; ByteIndex < 2; ++ByteIndex)
		{
			Bytes.Add(static_cast<uint8>(Value >> (ByteIndex * 8)));
		}
	}

	void FHansaPrimitiveWriter::WriteUInt32(const uint32 Value)
	{
		for (uint32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
		{
			Bytes.Add(static_cast<uint8>(Value >> (ByteIndex * 8)));
		}
	}

	void FHansaPrimitiveWriter::WriteUInt64(const uint64 Value)
	{
		for (uint32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
		{
			Bytes.Add(static_cast<uint8>(Value >> (ByteIndex * 8)));
		}
	}

	void FHansaPrimitiveWriter::WriteInt64(const int64 Value)
	{
		WriteUInt64(static_cast<uint64>(Value));
	}

	FHansaPrimitiveReader::FHansaPrimitiveReader(const TConstArrayView<uint8> InBytes)
		: Bytes(InBytes)
	{
		uint32 Magic = 0;
		uint16 Version = 0;
		if (!ReadUInt32(Magic) || !ReadUInt16(Version))
		{
			return;
		}
		if (Magic != PrimitiveMagic)
		{
			SetError(EHansaValueError::InvalidFormat);
			return;
		}
		if (Version != FHansaPrimitiveWriter::CurrentFormatVersion)
		{
			SetError(EHansaValueError::UnsupportedVersion);
		}
	}

	bool FHansaPrimitiveReader::ReadDefinitionId(FHansaDefinitionId& OutValue)
	{
		if (!ExpectType(GenericDefinitionIdTag))
		{
			return false;
		}
		FString Text;
		if (!ReadAsciiString(Text))
		{
			return false;
		}
		const THansaValueResult<FHansaDefinitionId> Parsed = FHansaDefinitionId::TryParse(Text);
		if (!Parsed)
		{
			SetError(Parsed.Error);
			return false;
		}
		OutValue = Parsed.Value;
		return true;
	}

	bool FHansaPrimitiveReader::ReadMoney(FHansaMoney& OutValue)
	{
		int64 Value = 0;
		if (!ExpectType(FHansaMoney::GetSerializationTag()) || !ReadInt64(Value))
		{
			return false;
		}
		OutValue = FHansaMoney::FromRaw(Value);
		return true;
	}

	bool FHansaPrimitiveReader::ReadQuantity(FHansaQuantity& OutValue)
	{
		int64 Value = 0;
		if (!ExpectType(FHansaQuantity::GetSerializationTag()) || !ReadInt64(Value))
		{
			return false;
		}
		OutValue = FHansaQuantity::FromRaw(Value);
		return true;
	}

	bool FHansaPrimitiveReader::ReadRate(FHansaRate& OutValue)
	{
		int64 Value = 0;
		if (!ExpectType(FHansaRate::SerializationTag) || !ReadInt64(Value))
		{
			return false;
		}
		OutValue = FHansaRate::FromPartsPerMillion(Value);
		return true;
	}

	bool FHansaPrimitiveReader::ReadSimulationVersion(FHansaSimulationVersion& OutValue)
	{
		uint32 Value = 0;
		if (!ExpectType(FHansaSimulationVersion::SerializationTag) || !ReadUInt32(Value))
		{
			return false;
		}
		const THansaValueResult<FHansaSimulationVersion> Parsed = FHansaSimulationVersion::TryCreate(Value);
		if (!Parsed)
		{
			SetError(Parsed.Error);
			return false;
		}
		OutValue = Parsed.Value;
		return true;
	}

	bool FHansaPrimitiveReader::ReadSimulationTick(FHansaSimulationTick& OutValue)
	{
		int64 Value = 0;
		if (!ExpectType(FHansaSimulationTick::SerializationTag) || !ReadInt64(Value))
		{
			return false;
		}
		const THansaValueResult<FHansaSimulationTick> Parsed = FHansaSimulationTick::TryCreate(Value);
		if (!Parsed)
		{
			SetError(Parsed.Error);
			return false;
		}
		OutValue = Parsed.Value;
		return true;
	}

	bool FHansaPrimitiveReader::ReadSimulationDuration(FHansaSimulationDuration& OutValue)
	{
		int64 Value = 0;
		if (!ExpectType(FHansaSimulationDuration::SerializationTag) || !ReadInt64(Value))
		{
			return false;
		}
		const THansaValueResult<FHansaSimulationDuration> Parsed = FHansaSimulationDuration::TryCreate(Value);
		if (!Parsed)
		{
			SetError(Parsed.Error);
			return false;
		}
		OutValue = Parsed.Value;
		return true;
	}

	bool FHansaPrimitiveReader::ReadClock(FHansaSimulationClock& OutValue)
	{
		uint32 VersionValue = 0;
		int64 TickValue = 0;
		uint16 MinutesPerTick = 0;
		if (!ExpectType(FHansaSimulationClock::SerializationTag) ||
			!ReadUInt32(VersionValue) ||
			!ReadInt64(TickValue) ||
			!ReadUInt16(MinutesPerTick))
		{
			return false;
		}

		const THansaValueResult<FHansaSimulationVersion> Version = FHansaSimulationVersion::TryCreate(VersionValue);
		const THansaValueResult<FHansaSimulationTick> Tick = FHansaSimulationTick::TryCreate(TickValue);
		if (!Version || !Tick)
		{
			SetError(!Version ? Version.Error : Tick.Error);
			return false;
		}
		const THansaValueResult<FHansaSimulationClock> Clock = FHansaSimulationClock::TryCreate(Version.Value, Tick.Value, MinutesPerTick);
		if (!Clock)
		{
			SetError(Clock.Error);
			return false;
		}
		OutValue = Clock.Value;
		return true;
	}

	bool FHansaPrimitiveReader::ReadRandomStream(FHansaRandomStream& OutValue)
	{
		if (!ExpectType(FHansaRandomStream::SerializationTag))
		{
			return false;
		}
		FString Name;
		uint8 AlgorithmValue = 0;
		uint64 State = 0;
		uint64 DrawCount = 0;
		if (!ReadAsciiString(Name) || !ReadUInt8(AlgorithmValue) || !ReadUInt64(State) || !ReadUInt64(DrawCount))
		{
			return false;
		}

		const THansaValueResult<FHansaRandomStream> Stream = FHansaRandomStream::TryRestore(
			Name,
			static_cast<EHansaRandomAlgorithm>(AlgorithmValue),
			State,
			DrawCount);
		if (!Stream)
		{
			SetError(Stream.Error);
			return false;
		}
		OutValue = Stream.Value;
		return true;
	}

	bool FHansaPrimitiveReader::Finish()
	{
		if (!IsValid())
		{
			return false;
		}
		if (Offset != Bytes.Num())
		{
			SetError(EHansaValueError::TrailingData);
			return false;
		}
		return true;
	}

	void FHansaPrimitiveReader::SetError(const EHansaValueError InError)
	{
		if (Error == EHansaValueError::None)
		{
			Error = InError;
		}
	}

	bool FHansaPrimitiveReader::ExpectType(const uint8 ExpectedType)
	{
		uint8 ActualType = 0;
		if (!ReadUInt8(ActualType))
		{
			return false;
		}
		if (ActualType != ExpectedType)
		{
			SetError(EHansaValueError::UnexpectedType);
			return false;
		}
		return true;
	}

	bool FHansaPrimitiveReader::ReadAsciiString(FString& OutValue)
	{
		uint32 Length = 0;
		if (!ReadUInt32(Length))
		{
			return false;
		}
		if (Length > static_cast<uint32>(Bytes.Num() - Offset))
		{
			SetError(EHansaValueError::TruncatedData);
			return false;
		}

		OutValue.Reset(static_cast<int32>(Length));
		for (uint32 Index = 0; Index < Length; ++Index)
		{
			const uint8 Character = Bytes[Offset++];
			if (Character > 0x7f)
			{
				SetError(EHansaValueError::InvalidFormat);
				return false;
			}
			OutValue.AppendChar(static_cast<TCHAR>(Character));
		}
		return true;
	}

	bool FHansaPrimitiveReader::ReadUInt8(uint8& OutValue)
	{
		if (!IsValid() || Offset >= Bytes.Num())
		{
			SetError(EHansaValueError::TruncatedData);
			return false;
		}
		OutValue = Bytes[Offset++];
		return true;
	}

	bool FHansaPrimitiveReader::ReadUInt16(uint16& OutValue)
	{
		if (!IsValid() || Bytes.Num() - Offset < 2)
		{
			SetError(EHansaValueError::TruncatedData);
			return false;
		}
		OutValue = 0;
		for (uint32 ByteIndex = 0; ByteIndex < 2; ++ByteIndex)
		{
			OutValue |= static_cast<uint16>(Bytes[Offset++]) << (ByteIndex * 8);
		}
		return true;
	}

	bool FHansaPrimitiveReader::ReadUInt32(uint32& OutValue)
	{
		if (!IsValid() || Bytes.Num() - Offset < 4)
		{
			SetError(EHansaValueError::TruncatedData);
			return false;
		}
		OutValue = 0;
		for (uint32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
		{
			OutValue |= static_cast<uint32>(Bytes[Offset++]) << (ByteIndex * 8);
		}
		return true;
	}

	bool FHansaPrimitiveReader::ReadUInt64(uint64& OutValue)
	{
		if (!IsValid() || Bytes.Num() - Offset < 8)
		{
			SetError(EHansaValueError::TruncatedData);
			return false;
		}
		OutValue = 0;
		for (uint32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
		{
			OutValue |= static_cast<uint64>(Bytes[Offset++]) << (ByteIndex * 8);
		}
		return true;
	}

	bool FHansaPrimitiveReader::ReadInt64(int64& OutValue)
	{
		uint64 UnsignedValue = 0;
		if (!ReadUInt64(UnsignedValue))
		{
			return false;
		}
		constexpr uint64 NegativeLimit = 1ULL << 63;
		if (UnsignedValue < NegativeLimit)
		{
			OutValue = static_cast<int64>(UnsignedValue);
		}
		else
		{
			const uint64 Magnitude = (~UnsignedValue) + 1;
			OutValue = Magnitude == NegativeLimit
				? TNumericLimits<int64>::Lowest()
				: -static_cast<int64>(Magnitude);
		}
		return true;
	}
}
