#include "Diagnostics/HansaDeterminismEvidence.h"

#include "Misc/DateTime.h"

namespace Hansa::Simulation
{
	namespace
	{
		FString EscapeJson(const FString& Value)
		{
			FString Escaped;
			Escaped.Reserve(Value.Len());
			for (const TCHAR Character : Value)
			{
				switch (Character)
				{
				case TEXT('"'): Escaped += TEXT("\\\""); break;
				case TEXT('\\'): Escaped += TEXT("\\\\"); break;
				case TEXT('\n'): Escaped += TEXT("\\n"); break;
				case TEXT('\r'): Escaped += TEXT("\\r"); break;
				case TEXT('\t'): Escaped += TEXT("\\t"); break;
				default: Escaped.AppendChar(Character); break;
				}
			}
			return Escaped;
		}

		FString Hex64(const uint64 Value)
		{
			return FString::Printf(TEXT("%016llX"), static_cast<unsigned long long>(Value));
		}
	}

	FString FHansaDeterminismEvidenceWriter::WriteRunJson(const FHansaFixtureRunResult& Run)
	{
		FString Json;
		Json += TEXT("{\n");
		Json += FString::Printf(TEXT("  \"evidenceSchemaVersion\": %u,\n"), CurrentEvidenceSchemaVersion);
		Json += FString::Printf(TEXT("  \"generatedUtc\": \"%s\",\n"), *FDateTime::UtcNow().ToIso8601());
		Json += FString::Printf(TEXT("  \"status\": \"%s\",\n"), Run.IsSuccess() ? TEXT("Succeeded") : TEXT("Failed"));
		Json += FString::Printf(TEXT("  \"error\": \"%s\",\n"), LexToString(Run.GetError()));
		Json += FString::Printf(TEXT("  \"gatewayError\": \"%s\",\n"), LexToString(Run.GetGatewayError()));
		Json += FString::Printf(TEXT("  \"requestedTickCount\": %lld,\n"), static_cast<long long>(Run.GetRequestedTickCount()));
		Json += FString::Printf(TEXT("  \"owner\": \"%s\",\n"), *EscapeJson(Run.GetOwner()));

		if (!Run.IsSuccess())
		{
			Json += FString::Printf(TEXT("  \"failedTick\": %lld\n"),
				static_cast<long long>(Run.GetFailedTick().GetValue()));
			Json += TEXT("}\n");
			return Json;
		}

		const FHansaDeterminismTrace& Trace = Run.GetTrace();
		Json += FString::Printf(TEXT("  \"fixtureId\": \"%s\",\n"), *EscapeJson(Trace.GetFixtureId()));
		Json += FString::Printf(TEXT("  \"fixtureSchemaVersion\": %u,\n"), Trace.GetFixtureSchemaVersion());
		Json += FString::Printf(TEXT("  \"hashFormatVersion\": %u,\n"),
			Trace.GetInitialState().GetHashFormatVersion());
		Json += FString::Printf(TEXT("  \"normalizationVersion\": %u,\n"),
			Trace.GetInitialState().GetNormalizationVersion());
		Json += FString::Printf(TEXT("  \"fingerprintVersion\": %u,\n"),
			FHansaSimulationState::DeterminismFingerprintVersion);
		Json += FString::Printf(TEXT("  \"systemPipelineVersion\": %u,\n"),
			Trace.GetInitialState().GetSystemPipelineVersion());
		Json += FString::Printf(TEXT("  \"seed\": \"%s\",\n"), *Hex64(Trace.GetSeed()));
		Json += FString::Printf(TEXT("  \"definitionHash\": \"%s\",\n"), *Hex64(Trace.GetDefinitionHash()));
		Json += FString::Printf(TEXT("  \"initialTick\": %lld,\n"),
			static_cast<long long>(Trace.GetInitialState().GetTick().GetValue()));
		Json += FString::Printf(TEXT("  \"initialStateHash\": \"%s\",\n"),
			*Hex64(Trace.GetInitialState().GetOverallHash()));
		Json += FString::Printf(TEXT("  \"finalStateHash\": \"%s\",\n"),
			*Hex64(Run.GetFinalProjection().GetFingerprint().Value));
		Json += FString::Printf(TEXT("  \"processedCommandCount\": \"%llu\",\n"),
			static_cast<unsigned long long>(Run.GetFinalProjection().GetProcessedCommandCount()));
		Json += TEXT("  \"ticks\": [\n");

		const TConstArrayView<FHansaDeterminismTickRecord> Ticks = Trace.GetTicks();
		for (int32 TickIndex = 0; TickIndex < Ticks.Num(); ++TickIndex)
		{
			const FHansaDeterminismTickRecord& Tick = Ticks[TickIndex];
			Json += TEXT("    {\n");
			Json += FString::Printf(TEXT("      \"processedTick\": %lld,\n"),
				static_cast<long long>(Tick.ProcessedTick.GetValue()));
			Json += FString::Printf(TEXT("      \"pipelineOrderHash\": \"%s\",\n"), *Hex64(Tick.PipelineOrderHash));
			Json += FString::Printf(TEXT("      \"domainEventOrderHash\": \"%s\",\n"), *Hex64(Tick.DomainEventOrderHash));
			Json += FString::Printf(TEXT("      \"overallStateHash\": \"%s\",\n"),
				*Hex64(Tick.StateAfterTick.GetOverallHash()));
			Json += TEXT("      \"subsystems\": {");
			const TConstArrayView<FHansaSubsystemStateHash> Subsystems = Tick.StateAfterTick.GetSubsystems();
			for (int32 SubsystemIndex = 0; SubsystemIndex < Subsystems.Num(); ++SubsystemIndex)
			{
				if (SubsystemIndex > 0)
				{
					Json += TEXT(", ");
				}
				Json += FString::Printf(TEXT("\"%s\": \"%s\""),
					LexToString(Subsystems[SubsystemIndex].Subsystem),
					*Hex64(Subsystems[SubsystemIndex].Value));
			}
			Json += TEXT("}\n    }");
			Json += TickIndex + 1 < Ticks.Num() ? TEXT(",\n") : TEXT("\n");
		}
		Json += TEXT("  ]\n}\n");
		return Json;
	}

	FString FHansaDeterminismEvidenceWriter::WriteComparisonJson(
		const FString& FixtureId,
		const FHansaDeterminismComparison& Comparison)
	{
		return FString::Printf(
			TEXT("{\n  \"evidenceSchemaVersion\": %u,\n  \"fixtureId\": \"%s\",\n  \"equal\": %s,\n")
			TEXT("  \"firstDivergentTick\": %lld,\n  \"kind\": \"%s\",\n  \"subsystem\": \"%s\",\n")
			TEXT("  \"left\": \"%s\",\n  \"right\": \"%s\"\n}\n"),
			CurrentEvidenceSchemaVersion,
			*EscapeJson(FixtureId),
			Comparison.IsEqual() ? TEXT("true") : TEXT("false"),
			static_cast<long long>(Comparison.GetFirstDivergentTick().GetValue()),
			LexToString(Comparison.GetKind()),
			LexToString(Comparison.GetSubsystem()),
			*Hex64(Comparison.GetLeftValue()),
			*Hex64(Comparison.GetRightValue()));
	}
}
