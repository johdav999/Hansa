#include "Endpoint/HansaAutomationNamedPipeEndpoint.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HansaAutomationModule.h"
#include "Gameplay/HansaProductionFixtureService.h"
#include "Gameplay/HansaStrategicAutomationFixture.h"
#include "Gameplay/HansaPlacementAutomationFixture.h"
#include "Multiplayer/HansaMultiplayerAutomationService.h"
#include "CoreGlobals.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "Protocol/HansaAutomationProtocol.h"
#include "Screenshot/HansaNativeScreenshotService.h"
#include "SemanticUI/HansaSemanticUiRegistry.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Session/HansaAutomationSessionService.h"
#include "Synchronization/HansaAutomationWaitService.h"
#include "UI/HansaAutomationProofScreen.h"
#include "UI/HansaPlacementAutomationScreen.h"
#include "UI/HansaRouteDeliveryAutomationScreen.h"
#include "UI/HansaStrategicAutomationScreen.h"
#include "World/HansaRuntimeSimulationHost.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace Hansa::Automation
{
	namespace
	{
		constexpr uint32 WireSchemaVersion = 1;
		constexpr uint32 MaximumFrameBytes = 64 * 1024;
		constexpr int32 MaximumFramesPerTick = 8;
		constexpr int32 FrameHeaderBytes = 4;

		int64 MonotonicMilliseconds()
		{
			return static_cast<int64>(FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64()));
		}

		bool IsBoundedWireIdentifier(const FString& Value)
		{
			if (Value.IsEmpty() || Value.Len() > 64)
			{
				return false;
			}
			for (const TCHAR Character : Value)
			{
				if (!(FChar::IsAlnum(Character) || Character == TEXT('.') || Character == TEXT('_') ||
					Character == TEXT(':') || Character == TEXT('-')))
				{
					return false;
				}
			}
			return true;
		}

		TSharedRef<FJsonObject> MakeProtocolVersion(const FHansaAutomationProtocolVersion& Version)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetNumberField(TEXT("major"), Version.Major);
			Json->SetNumberField(TEXT("minor"), Version.Minor);
			return Json;
		}

		TSharedRef<FJsonObject> MakeSessionSnapshot(const FHansaAutomationSessionSnapshot& Snapshot)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetStringField(TEXT("sessionId"), Snapshot.SessionId);
			Json->SetStringField(TEXT("controllerId"), Snapshot.ControllerId);
			Json->SetObjectField(TEXT("protocolVersion"), MakeProtocolVersion(Snapshot.ProtocolVersion));
			Json->SetStringField(TEXT("permission"), LexToString(Snapshot.Permission));
			Json->SetNumberField(TEXT("openedAtMonotonicMs"), Snapshot.OpenedAtMonotonicMilliseconds);

			TArray<TSharedPtr<FJsonValue>> Capabilities;
			for (const EHansaAutomationCapability Capability : Snapshot.GrantedCapabilities)
			{
				Capabilities.Add(MakeShared<FJsonValueString>(LexToString(Capability)));
			}
			Json->SetArrayField(TEXT("grantedCapabilities"), MoveTemp(Capabilities));
			return Json;
		}

		TSharedRef<FJsonObject> MakeCapabilityManifest(const FHansaAutomationCapabilityManifest& Manifest)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetObjectField(TEXT("protocolVersion"), MakeProtocolVersion(Manifest.ProtocolVersion));
			Json->SetStringField(TEXT("maximumPermission"), LexToString(Manifest.MaximumPermission));

			TArray<TSharedPtr<FJsonValue>> Capabilities;
			for (const FHansaAutomationCapabilityDescriptor& Descriptor : Manifest.Capabilities)
			{
				TSharedRef<FJsonObject> Capability = MakeShared<FJsonObject>();
				Capability->SetStringField(TEXT("name"), Descriptor.StableName);
				Capability->SetStringField(TEXT("minimumPermission"), LexToString(Descriptor.MinimumPermission));
				Capability->SetBoolField(TEXT("mutating"), Descriptor.bMutating);
				Capabilities.Add(MakeShared<FJsonValueObject>(Capability));
			}
			Json->SetArrayField(TEXT("capabilities"), MoveTemp(Capabilities));
			return Json;
		}

		FString SerializeResponse(const TSharedRef<FJsonObject>& Response)
		{
			FString Json;
			const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
			FJsonSerializer::Serialize(Response, Writer);
			return Json;
		}

		FString MakeSuccessResponse(const FString& RequestId, const TSharedRef<FJsonObject>& Payload)
		{
			TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetNumberField(TEXT("schemaVersion"), WireSchemaVersion);
			Response->SetStringField(TEXT("requestId"), RequestId);
			Response->SetBoolField(TEXT("ok"), true);
			Response->SetObjectField(TEXT("payload"), Payload);
			return SerializeResponse(Response);
		}

		FString MakeErrorResponse(const FString& RequestId, const FHansaAutomationError& Error)
		{
			TSharedRef<FJsonObject> ErrorJson = MakeShared<FJsonObject>();
			ErrorJson->SetStringField(TEXT("code"), LexToString(Error.Code));
			ErrorJson->SetStringField(TEXT("correlationId"), Error.CorrelationId.IsEmpty() ? RequestId : Error.CorrelationId);
			ErrorJson->SetStringField(TEXT("message"), Error.Message);
			ErrorJson->SetStringField(TEXT("remedy"), Error.Remedy);
			ErrorJson->SetBoolField(TEXT("retryable"), Error.bRetryable);

			TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetNumberField(TEXT("schemaVersion"), WireSchemaVersion);
			Response->SetStringField(TEXT("requestId"), RequestId);
			Response->SetBoolField(TEXT("ok"), false);
			Response->SetObjectField(TEXT("error"), ErrorJson);
			return SerializeResponse(Response);
		}

		FHansaAutomationError InvalidRequest(const FString& RequestId, const TCHAR* Message, const TCHAR* Remedy)
		{
			return FHansaAutomationError {
				EHansaAutomationErrorCode::InvalidRequest,
				RequestId,
				Message,
				Remedy,
				false
			};
		}

		bool TryGetIntegralField(const TSharedRef<FJsonObject>& Json, const TCHAR* Field, int64& OutValue)
		{
			double Number = 0.0;
			if (!Json->TryGetNumberField(Field, Number) ||
				!FMath::IsFinite(Number) ||
				Number < static_cast<double>(MIN_int32) ||
				Number > static_cast<double>(MAX_int32) ||
				!FMath::IsNearlyEqual(Number, FMath::RoundToDouble(Number)))
			{
				return false;
			}
			OutValue = static_cast<int64>(Number);
			return true;
		}

		TSharedRef<FJsonObject> MakeConstructionCost(
			const Hansa::Simulation::FHansaConstructionCostProjection& Cost)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetStringField(TEXT("houseId"), Cost.HouseId.ToDebugString());
			Json->SetStringField(TEXT("cityId"), Cost.CityId.ToString());
			Json->SetStringField(TEXT("buildingDefinitionId"), Cost.BuildingDefinitionId.ToString());
			Json->SetNumberField(TEXT("requiredCurrencyPfennig"), Cost.RequiredCurrency.GetRawValue());
			Json->SetNumberField(TEXT("availableCurrencyPfennig"), Cost.AvailableCurrency.GetRawValue());
			Json->SetNumberField(TEXT("missingCurrencyPfennig"), Cost.MissingCurrency.GetRawValue());
			Json->SetBoolField(TEXT("affordable"), Cost.IsAffordable());
			TArray<TSharedPtr<FJsonValue>> Resources;
			for (const Hansa::Simulation::FHansaConstructionResourceCostProjection& Resource : Cost.Resources)
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("goodId"), Resource.GoodId.ToString());
				Item->SetNumberField(TEXT("requiredMilliUnits"), Resource.Required.GetRawValue());
				Item->SetNumberField(TEXT("availableMilliUnits"), Resource.Available.GetRawValue());
				Item->SetNumberField(TEXT("missingMilliUnits"), Resource.Missing.GetRawValue());
				Resources.Add(MakeShared<FJsonValueObject>(Item));
			}
			Json->SetArrayField(TEXT("resources"), MoveTemp(Resources));
			return Json;
		}

		TSharedRef<FJsonObject> MakeConstruction(
			const Hansa::Simulation::FHansaConstructionProjection& Construction)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetNumberField(TEXT("buildingId"), static_cast<double>(Construction.BuildingId.GetValue()));
			Json->SetStringField(TEXT("cityId"), Construction.CityId.ToString());
			Json->SetStringField(TEXT("buildingDefinitionId"), Construction.BuildingDefinitionId.ToString());
			Json->SetStringField(TEXT("state"), Hansa::Simulation::LexToString(Construction.State));
			Json->SetNumberField(TEXT("startedTick"), static_cast<double>(Construction.StartedTick.GetValue()));
			Json->SetNumberField(TEXT("elapsedTicks"), Construction.ElapsedTicks);
			Json->SetNumberField(TEXT("totalTicks"), Construction.TotalTicks);
			Json->SetNumberField(TEXT("progressPartsPerMillion"), Construction.Progress.GetPartsPerMillion());
			Json->SetNumberField(TEXT("paidCurrencyPfennig"), Construction.PaidCurrency.GetRawValue());
			Json->SetNumberField(TEXT("cancellationCurrencyRefundPfennig"),
				Construction.CancellationCurrencyRefund.GetRawValue());
			return Json;
		}

		TSharedRef<FJsonObject> MakeIntegratedSummary(const FHansaPlacementAutomationFixture& Fixture)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetStringField(TEXT("fixtureId"), FHansaPlacementAutomationFixture::IntegratedFixtureId);
			Json->SetNumberField(TEXT("tick"), Fixture.GetSimulationTick());
			const auto Projection = Fixture.BuildProjection();
			if (!Projection)
			{
				Json->SetBoolField(TEXT("available"), false);
				return Json;
			}
			Json->SetBoolField(TEXT("available"), true);
			Json->SetStringField(TEXT("stateHash"), FString::Printf(TEXT("%016llX"),
				static_cast<unsigned long long>(Projection.Value.GetFingerprint().Value)));
			const FHansaIntegratedLubeckCheckpointState& Checkpoints = Fixture.GetIntegratedCheckpoints();
			int64 CityBreadStock = 0;
			for (const auto& Inventory : Projection.Value.GetInventories())
			{
				if (Inventory.Id.GetValue() != 3) continue;
				for (const auto& Stock : Inventory.Stocks)
				{
					if (Stock.GoodId.ToString() == TEXT("Good.Bread")) CityBreadStock = Stock.Stock.GetRawValue();
				}
			}
			Json->SetBoolField(TEXT("constructionCompleted"), Checkpoints.bConstructionCompleted);
			Json->SetBoolField(TEXT("inventoryMoved"), Checkpoints.bInventoryMoved);
			Json->SetBoolField(TEXT("productionCompleted"), Checkpoints.bProductionCompleted);
			Json->SetBoolField(TEXT("populationGrown"), Checkpoints.bPopulationGrown);
			Json->SetBoolField(TEXT("breadConsumed"), Checkpoints.bBreadConsumed);
			Json->SetNumberField(TEXT("completedDeliveries"), Checkpoints.CompletedDeliveries);
			Json->SetStringField(TEXT("completedProductionCycles"), FString::Printf(TEXT("%llu"),
				static_cast<unsigned long long>(Checkpoints.CompletedProductionCycles)));
			Json->SetNumberField(TEXT("residents"), Checkpoints.Residents);
			Json->SetNumberField(TEXT("breadConsumedLastTickMilliUnits"),
				static_cast<double>(Checkpoints.BreadConsumedLastTickMilliUnits));
			Json->SetNumberField(TEXT("breadConsumedTotalMilliUnits"),
				static_cast<double>(Checkpoints.BreadConsumedTotalMilliUnits));
			Json->SetNumberField(TEXT("cityBreadStockMilliUnits"), CityBreadStock);
			Json->SetNumberField(TEXT("logisticsRequestCount"), Projection.Value.GetLogisticsRequests().Num());
			Json->SetNumberField(TEXT("logisticsJobCount"), Projection.Value.GetLogisticsJobs().Num());
			return Json;
		}

		bool IntegratedPredicateMatches(const TSharedRef<FJsonObject>& Summary, const FString& Predicate)
		{
			if (Predicate == TEXT("integrated.construction_completed")) return Summary->GetBoolField(TEXT("constructionCompleted"));
			if (Predicate == TEXT("integrated.inventory_moved")) return Summary->GetBoolField(TEXT("inventoryMoved"));
			if (Predicate == TEXT("integrated.production_completed")) return Summary->GetBoolField(TEXT("productionCompleted"));
			if (Predicate == TEXT("integrated.population_grown")) return Summary->GetBoolField(TEXT("populationGrown"));
			if (Predicate == TEXT("integrated.bread_consumed")) return Summary->GetBoolField(TEXT("breadConsumed"));
			return false;
		}

		TSharedRef<FJsonObject> MakeSemanticNode(const FHansaSemanticNode& Node)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetStringField(TEXT("id"), Node.Id);
			Json->SetStringField(TEXT("role"), LexToString(Node.Role));
			Json->SetStringField(TEXT("label"), Node.Label);
			Json->SetStringField(TEXT("parentId"), Node.ParentId);
			TSharedRef<FJsonObject> Bounds = MakeShared<FJsonObject>();
			Bounds->SetNumberField(TEXT("x"), Node.Bounds.X);
			Bounds->SetNumberField(TEXT("y"), Node.Bounds.Y);
			Bounds->SetNumberField(TEXT("width"), Node.Bounds.Width);
			Bounds->SetNumberField(TEXT("height"), Node.Bounds.Height);
			Json->SetObjectField(TEXT("bounds"), Bounds);
			TSharedRef<FJsonObject> State = MakeShared<FJsonObject>();
			State->SetBoolField(TEXT("visible"), Node.State.bVisible);
			State->SetBoolField(TEXT("enabled"), Node.State.bEnabled);
			State->SetBoolField(TEXT("focused"), Node.State.bFocused);
			State->SetBoolField(TEXT("selected"), Node.State.bSelected);
			State->SetBoolField(TEXT("loading"), Node.State.bLoading);
			State->SetBoolField(TEXT("warning"), Node.State.bWarning);
			State->SetBoolField(TEXT("error"), Node.State.bError);
			State->SetStringField(TEXT("valueType"), Node.State.ValueType);
			State->SetStringField(TEXT("value"), Node.State.Value);
			Json->SetObjectField(TEXT("state"), State);
			TArray<TSharedPtr<FJsonValue>> Children;
			for (const FString& ChildId : Node.ChildIds)
			{
				Children.Add(MakeShared<FJsonValueString>(ChildId));
			}
			Json->SetArrayField(TEXT("children"), MoveTemp(Children));
			TArray<TSharedPtr<FJsonValue>> Actions;
			for (const EHansaSemanticAction Action : Node.Actions)
			{
				Actions.Add(MakeShared<FJsonValueString>(LexToString(Action)));
			}
			Json->SetArrayField(TEXT("actions"), MoveTemp(Actions));
			return Json;
		}

		TSharedRef<FJsonObject> MakeSemanticSnapshot(const FHansaSemanticUiRegistry& Registry)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetNumberField(TEXT("schemaVersion"), 1);
			Json->SetNumberField(TEXT("revision"), static_cast<double>(Registry.GetRevision()));
			TArray<TSharedPtr<FJsonValue>> Nodes;
			for (const FHansaSemanticNode& Node : Registry.FindNodes())
			{
				Nodes.Add(MakeShared<FJsonValueObject>(MakeSemanticNode(Node)));
			}
			Json->SetArrayField(TEXT("nodes"), MoveTemp(Nodes));
			return Json;
		}

		FHansaAutomationError MakeEndpointError(
			const EHansaAutomationErrorCode Code,
			const FString& RequestId,
			const TCHAR* Message,
			const TCHAR* Remedy,
			const bool bRetryable = false)
		{
			return FHansaAutomationError { Code, RequestId, Message, Remedy, bRetryable };
		}

		FString GetCurrentMapName()
		{
			if (GEngine != nullptr)
			{
				for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
				{
					if (const UWorld* World = WorldContext.World())
					{
						return World->GetMapName();
					}
				}
			}
			return TEXT("Unavailable");
		}

		FString MakeRouteEvidenceSnapshot(const Hansa::Simulation::FHansaProductionFixture& Fixture)
		{
			FString Json = FString::Printf(TEXT("{\"stateHash\":\"%016llX\",\"eventCount\":%d,\"events\":["),
				static_cast<unsigned long long>(Fixture.BuildStateHashes().GetOverallHash()), Fixture.GetEvents().Num());
			bool bFirst = true;
			for (const Hansa::Simulation::FHansaDomainEvent& Event : Fixture.GetEvents())
			{
				if (!Event.GetRouteId().IsValid()) continue;
				if (!bFirst) Json += TEXT(",");
				bFirst = false;
				Json += FString::Printf(TEXT("{\"sequence\":\"%llu\",\"tick\":%lld,\"type\":\"%s\",\"cityId\":\"%s\",\"value\":%lld}"),
					static_cast<unsigned long long>(Event.GetGlobalSequence()),
					static_cast<long long>(Event.GetTick().GetValue()), Hansa::Simulation::LexToString(Event.GetType()),
					*Event.GetCityId().ToString(), static_cast<long long>(Event.GetValue()));
			}
			Json += TEXT("]}");
			return Json;
		}
	}

	FHansaAutomationNamedPipeEndpoint::FHansaAutomationNamedPipeEndpoint(
		FString InPipeName,
		FHansaAutomationSessionService& InSessionService)
		: PipeName(MoveTemp(InPipeName))
		, SessionService(InSessionService)
	{
		SemanticRegistry = MakeUnique<FHansaSemanticUiRegistry>();
		WaitService = MakeUnique<FHansaAutomationWaitService>(*SemanticRegistry);
		ScreenshotService = MakeUnique<FHansaNativeScreenshotService>();
		ProofScreenHost = MakeUnique<FHansaAutomationProofScreenHost>(*SemanticRegistry);
		ProductionFixtureService = MakeUnique<FHansaProductionFixtureService>();
		PlacementFixture = MakeUnique<FHansaPlacementAutomationFixture>();
		StrategicFixture = MakeUnique<FHansaStrategicAutomationFixture>();
		MultiplayerService = MakeUnique<FHansaMultiplayerAutomationService>();
	}

	FHansaAutomationNamedPipeEndpoint::~FHansaAutomationNamedPipeEndpoint()
	{
		Stop();
	}

	bool FHansaAutomationNamedPipeEndpoint::Start()
	{
		if (IsRunning())
		{
			return true;
		}
		if (!CreateServerPipe())
		{
			return false;
		}
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FHansaAutomationNamedPipeEndpoint::Tick),
			0.02f);
		return true;
	}

	void FHansaAutomationNamedPipeEndpoint::Stop()
	{
		if (TickHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
			TickHandle.Reset();
		}
		ResetConnection();
	}

	bool FHansaAutomationNamedPipeEndpoint::Tick(const float DeltaTime)
	{
		(void)DeltaTime;
		if (bStrategicFixtureActive && StrategicScreenHost.IsValid()) StrategicScreenHost->SynchronizeSemantics();
		else if (bRouteDeliveryFixtureActive && RouteDeliveryScreenHost.IsValid()) RouteDeliveryScreenHost->SynchronizeSemantics();
		else if (bPlacementFixtureActive && PlacementScreenHost.IsValid()) PlacementScreenHost->SynchronizeSemantics();
		else if (ProofScreenHost.IsValid()) ProofScreenHost->SynchronizeSemantics();
		WaitService->Tick(MonotonicMilliseconds());
		if (!IsRunning() && !CreateServerPipe())
		{
			return true;
		}
		if (!bClientConnected && !AcceptClient())
		{
			return true;
		}
		if (bClientConnected && !PumpClient())
		{
			ResetConnection();
		}
		return true;
	}

	bool FHansaAutomationNamedPipeEndpoint::CreateServerPipe()
	{
#if PLATFORM_WINDOWS
		const FString FullPipeName = FString::Printf(TEXT("\\\\.\\pipe\\%s"), *PipeName);
		const HANDLE Handle = CreateNamedPipeW(
			*FullPipeName,
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
			1,
			MaximumFrameBytes + FrameHeaderBytes,
			MaximumFrameBytes + FrameHeaderBytes,
			0,
			nullptr);
		if (Handle == INVALID_HANDLE_VALUE)
		{
			UE_LOG(LogHansaAutomation, Error, TEXT("Failed to create the explicitly requested local automation pipe (Win32 error %lu)."), GetLastError());
			return false;
		}
		PipeHandle = Handle;
		bClientConnected = false;
		ReceiveBuffer.Reset();
		return true;
#else
		UE_LOG(LogHansaAutomation, Error, TEXT("The S02-P04 named-pipe endpoint is available only on Windows."));
		return false;
#endif
	}

	bool FHansaAutomationNamedPipeEndpoint::AcceptClient()
	{
#if PLATFORM_WINDOWS
		const HANDLE Handle = static_cast<HANDLE>(PipeHandle);
		if (ConnectNamedPipe(Handle, nullptr) != 0 || GetLastError() == ERROR_PIPE_CONNECTED)
		{
			bClientConnected = true;
			UE_LOG(LogHansaAutomation, Display, TEXT("An authenticated automation controller transport connected."));
			return true;
		}
		const DWORD Error = GetLastError();
		if (Error == ERROR_PIPE_LISTENING || Error == ERROR_NO_DATA)
		{
			return false;
		}
		UE_LOG(LogHansaAutomation, Warning, TEXT("Automation pipe accept failed (Win32 error %lu); recreating the endpoint."), Error);
		ResetConnection();
#endif
		return false;
	}

	bool FHansaAutomationNamedPipeEndpoint::PumpClient()
	{
#if PLATFORM_WINDOWS
		const HANDLE Handle = static_cast<HANDLE>(PipeHandle);
		DWORD AvailableBytes = 0;
		if (PeekNamedPipe(Handle, nullptr, 0, nullptr, &AvailableBytes, nullptr) == 0)
		{
			return false;
		}
		if (AvailableBytes > 0)
		{
			const int32 OldSize = ReceiveBuffer.Num();
			const uint32 ReadCapacity = FMath::Min<uint32>(AvailableBytes, MaximumFrameBytes + FrameHeaderBytes);
			ReceiveBuffer.AddUninitialized(static_cast<int32>(ReadCapacity));
			DWORD BytesRead = 0;
			if (ReadFile(Handle, ReceiveBuffer.GetData() + OldSize, ReadCapacity, &BytesRead, nullptr) == 0)
			{
				ReceiveBuffer.SetNum(OldSize, EAllowShrinking::No);
				return false;
			}
			ReceiveBuffer.SetNum(OldSize + static_cast<int32>(BytesRead), EAllowShrinking::No);
		}

		for (int32 FrameIndex = 0; FrameIndex < MaximumFramesPerTick; ++FrameIndex)
		{
			if (ReceiveBuffer.Num() < FrameHeaderBytes)
			{
				break;
			}
			const uint32 PayloadBytes =
				static_cast<uint32>(ReceiveBuffer[0]) |
				(static_cast<uint32>(ReceiveBuffer[1]) << 8) |
				(static_cast<uint32>(ReceiveBuffer[2]) << 16) |
				(static_cast<uint32>(ReceiveBuffer[3]) << 24);
			if (PayloadBytes == 0 || PayloadBytes > MaximumFrameBytes)
			{
				UE_LOG(LogHansaAutomation, Warning, TEXT("Rejected an invalid or oversized automation frame."));
				return false;
			}
			if (ReceiveBuffer.Num() < FrameHeaderBytes + static_cast<int32>(PayloadBytes))
			{
				break;
			}

			TArray<uint8> Utf8Payload;
			Utf8Payload.Append(ReceiveBuffer.GetData() + FrameHeaderBytes, static_cast<int32>(PayloadBytes));
			Utf8Payload.Add(0);
			const FString RequestJson = UTF8_TO_TCHAR(reinterpret_cast<const char*>(Utf8Payload.GetData()));
			ReceiveBuffer.RemoveAt(0, FrameHeaderBytes + static_cast<int32>(PayloadBytes), EAllowShrinking::No);
			const TOptional<FString> Response = Dispatch(RequestJson);
			if (Response.IsSet() && !WriteResponse(Response.GetValue()))
			{
				return false;
			}
		}
		return ReceiveBuffer.Num() <= static_cast<int32>(MaximumFrameBytes + FrameHeaderBytes);
#else
		return false;
#endif
	}

	bool FHansaAutomationNamedPipeEndpoint::WriteResponse(const FString& ResponseJson)
	{
#if PLATFORM_WINDOWS
		FTCHARToUTF8 Utf8(*ResponseJson);
		const uint32 PayloadBytes = static_cast<uint32>(Utf8.Length());
		if (PayloadBytes == 0 || PayloadBytes > MaximumFrameBytes)
		{
			return false;
		}
		TArray<uint8> Frame;
		Frame.Reserve(FrameHeaderBytes + static_cast<int32>(PayloadBytes));
		Frame.Add(static_cast<uint8>(PayloadBytes & 0xff));
		Frame.Add(static_cast<uint8>((PayloadBytes >> 8) & 0xff));
		Frame.Add(static_cast<uint8>((PayloadBytes >> 16) & 0xff));
		Frame.Add(static_cast<uint8>((PayloadBytes >> 24) & 0xff));
		Frame.Append(reinterpret_cast<const uint8*>(Utf8.Get()), static_cast<int32>(PayloadBytes));

		DWORD BytesWritten = 0;
		return WriteFile(
			static_cast<HANDLE>(PipeHandle),
			Frame.GetData(),
			static_cast<DWORD>(Frame.Num()),
			&BytesWritten,
			nullptr) != 0 && BytesWritten == static_cast<DWORD>(Frame.Num());
#else
		return false;
#endif
	}

	TOptional<FString> FHansaAutomationNamedPipeEndpoint::Dispatch(const FString& RequestJson)
	{
		TSharedPtr<FJsonObject> Request;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RequestJson), Request) || !Request.IsValid())
		{
			return MakeErrorResponse(TEXT("invalid"), InvalidRequest(
				TEXT("invalid"),
				TEXT("The automation frame does not contain a valid JSON object."),
				TEXT("Send one UTF-8 JSON request object inside the length-prefixed frame.")));
		}

		FString RequestId;
		Request->TryGetStringField(TEXT("requestId"), RequestId);
		int64 SchemaVersion = 0;
		int64 TimeoutMilliseconds = 0;
		FString Operation;
		if (!IsBoundedWireIdentifier(RequestId) ||
			!TryGetIntegralField(Request.ToSharedRef(), TEXT("schemaVersion"), SchemaVersion) ||
			SchemaVersion != WireSchemaVersion ||
			!TryGetIntegralField(Request.ToSharedRef(), TEXT("timeoutMs"), TimeoutMilliseconds) ||
			!Request->TryGetStringField(TEXT("operation"), Operation))
		{
			return MakeErrorResponse(RequestId.IsEmpty() ? TEXT("invalid") : RequestId, InvalidRequest(
				RequestId,
				TEXT("The automation envelope is missing a valid schemaVersion, requestId, timeoutMs, or operation."),
				TEXT("Use schemaVersion 1 and the checked-in HansaMcp wire schema.")));
		}

		const FHansaAutomationRequestContext Context {
			RequestId,
			MonotonicMilliseconds(),
			TimeoutMilliseconds
		};
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		if (Request->HasTypedField<EJson::Object>(TEXT("payload")))
		{
			Payload = Request->GetObjectField(TEXT("payload"));
		}

		if (Operation == TEXT("ping"))
		{
			TSharedRef<FJsonObject> Pong = MakeShared<FJsonObject>();
			Pong->SetBoolField(TEXT("pong"), true);
			Pong->SetNumberField(TEXT("processId"), FPlatformProcess::GetCurrentProcessId());
			Pong->SetObjectField(TEXT("protocolVersion"), MakeProtocolVersion(SessionService.GetProtocolVersion()));
			return MakeSuccessResponse(RequestId, Pong);
		}

		if (Operation == TEXT("capabilities_get"))
		{
			const FHansaAutomationCapabilityResult Result = SessionService.DiscoverCapabilities(Context);
			return Result
				? MakeSuccessResponse(RequestId, MakeCapabilityManifest(Result.GetPayload()))
				: MakeErrorResponse(RequestId, Result.GetError());
		}

		FString SessionId;
		FString ControllerId;
		Request->TryGetStringField(TEXT("sessionId"), SessionId);
		Request->TryGetStringField(TEXT("controllerId"), ControllerId);

		if (Operation == TEXT("session_start"))
		{
			if (!Payload->HasTypedField<EJson::Object>(TEXT("protocolVersion")))
			{
				return MakeErrorResponse(RequestId, InvalidRequest(
					RequestId,
					TEXT("session_start requires a protocolVersion object."),
					TEXT("Discover capabilities and pass its major/minor protocol version.")));
			}
			const TSharedPtr<FJsonObject> Version = Payload->GetObjectField(TEXT("protocolVersion"));
			int64 Major = 0;
			int64 Minor = 0;
			FString Token;
			FString PermissionText;
			if (!TryGetIntegralField(Version.ToSharedRef(), TEXT("major"), Major) ||
				!TryGetIntegralField(Version.ToSharedRef(), TEXT("minor"), Minor) ||
				!Payload->TryGetStringField(TEXT("authenticationToken"), Token) ||
				!Payload->TryGetStringField(TEXT("requestedPermission"), PermissionText))
			{
				return MakeErrorResponse(RequestId, InvalidRequest(
					RequestId,
					TEXT("session_start has invalid protocol, authentication, or permission fields."),
					TEXT("Use the checked-in session_start schema.")));
			}

			EHansaAutomationPermissionLevel Permission = EHansaAutomationPermissionLevel::None;
			if (!TryParsePermissionLevel(PermissionText, Permission))
			{
				return MakeErrorResponse(RequestId, InvalidRequest(
					RequestId,
					TEXT("session_start requested an unknown permission level."),
					TEXT("Request ReadOnly, ControlledActions, or FixtureControl.")));
			}

			FHansaAutomationOpenSessionRequest OpenRequest;
			OpenRequest.ProtocolVersion = {
				static_cast<int32>(Major),
				static_cast<int32>(Minor)
			};
			OpenRequest.ControllerId = ControllerId;
			OpenRequest.AuthenticationToken = Token;
			OpenRequest.RequestedPermission = Permission;
			const TArray<TSharedPtr<FJsonValue>>* RequiredCapabilities = nullptr;
			if (Payload->TryGetArrayField(TEXT("requiredCapabilities"), RequiredCapabilities))
			{
				for (const TSharedPtr<FJsonValue>& CapabilityValue : *RequiredCapabilities)
				{
					FString CapabilityText;
					EHansaAutomationCapability Capability;
					if (!CapabilityValue.IsValid() || !CapabilityValue->TryGetString(CapabilityText) ||
						!TryParseCapability(CapabilityText, Capability))
					{
						return MakeErrorResponse(RequestId, InvalidRequest(
							RequestId,
							TEXT("session_start contains an unknown required capability."),
							TEXT("Request only stable names returned by capabilities_get.")));
					}
					OpenRequest.RequiredCapabilities.AddUnique(Capability);
				}
			}

			const FHansaAutomationOpenSessionResult Result = SessionService.OpenSession(OpenRequest, Context);
			return Result
				? MakeSuccessResponse(RequestId, MakeSessionSnapshot(Result.GetPayload()))
				: MakeErrorResponse(RequestId, Result.GetError());
		}

		if (Operation == TEXT("session_get"))
		{
			const FHansaAutomationSessionResult Result = SessionService.GetSession(SessionId, ControllerId, Context);
			return Result
				? MakeSuccessResponse(RequestId, MakeSessionSnapshot(Result.GetPayload()))
				: MakeErrorResponse(RequestId, Result.GetError());
		}

		if (Operation == TEXT("session_stop"))
		{
			const FHansaAutomationOperationResult Result = SessionService.CloseSession(SessionId, ControllerId, Context);
			if (!Result)
			{
				return MakeErrorResponse(RequestId, Result.GetError());
			}
			TSharedRef<FJsonObject> Closed = MakeShared<FJsonObject>();
			Closed->SetBoolField(TEXT("closed"), true);
			return MakeSuccessResponse(RequestId, Closed);
		}

		if (Operation == TEXT("health"))
		{
			const FHansaAutomationOperationResult Result = SessionService.AuthorizeOperation(
				SessionId,
				ControllerId,
				EHansaAutomationOperation::HealthGet,
				Context);
			if (!Result)
			{
				return MakeErrorResponse(RequestId, Result.GetError());
			}
			TSharedRef<FJsonObject> Health = MakeShared<FJsonObject>();
			Health->SetStringField(TEXT("status"), TEXT("healthy"));
			Health->SetBoolField(TEXT("sessionActive"), SessionService.HasActiveSession());
			Health->SetNumberField(TEXT("processId"), FPlatformProcess::GetCurrentProcessId());
			return MakeSuccessResponse(RequestId, Health);
		}

		if (Operation == TEXT("fixture_list"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::GameplayQuery, Context);
			if (!Authorized) return MakeErrorResponse(RequestId, Authorized.GetError());
			TSharedRef<FJsonObject> Listed = ProductionFixtureService->ListFixtures();
			TArray<TSharedPtr<FJsonValue>> Fixtures = Listed->GetArrayField(TEXT("fixtures"));
			const bool bGoldenEvidenceProfile = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::EvidenceRead, Context).IsSuccess();
			if (bGoldenEvidenceProfile)
			{
				for (const TSharedPtr<FJsonValue>& Value : Fixtures)
				{
					const TSharedPtr<FJsonObject> Descriptor = Value.IsValid() ? Value->AsObject() : nullptr;
					if (!Descriptor.IsValid() || Descriptor->GetStringField(TEXT("fixtureId")) !=
						FHansaStrategicAutomationFixture::GoldenFixtureId) continue;
					const uint64 ReviewedHash = FHansaLubeckScenarioInitializer::MvpRegistryHash;
					Descriptor->SetNumberField(TEXT("fixtureVersion"), FHansaStrategicAutomationFixture::GoldenFixtureVersion);
					Descriptor->SetStringField(TEXT("registryHash"), FString::Printf(TEXT("%016llX"),
						static_cast<unsigned long long>(ReviewedHash)));
					Descriptor->SetStringField(TEXT("fixtureHash"), FString::Printf(TEXT("%016llX"),
						static_cast<unsigned long long>(ReviewedHash ^ FHansaStrategicAutomationFixture::SeedAlpha ^
							FHansaStrategicAutomationFixture::GoldenFixtureVersion)));
					Descriptor->SetStringField(TEXT("campaignSeed"), FString::Printf(TEXT("%016llX"),
						static_cast<unsigned long long>(FHansaStrategicAutomationFixture::SeedAlpha)));
					Descriptor->SetStringField(TEXT("purpose"),
						TEXT("S14-P01 playable-runtime golden path: reset, semantic diagnosis, normal commands, save/load, victory, and synchronized evidence"));
					break;
				}
			}
			TSharedRef<FJsonObject> Placement = MakeShared<FJsonObject>();
			Placement->SetStringField(TEXT("fixtureId"), FHansaPlacementAutomationFixture::StableFixtureId);
			Placement->SetNumberField(TEXT("fixtureVersion"), FHansaPlacementAutomationFixture::FixtureVersion);
			Placement->SetStringField(TEXT("registryHash"), FString::Printf(TEXT("%016llX"),
				static_cast<unsigned long long>(FHansaPlacementAutomationFixture::RegistryHash)));
			Placement->SetStringField(TEXT("purpose"), TEXT("Empty Lübeck road and building placement semantic flow"));
			Fixtures.Add(MakeShared<FJsonValueObject>(Placement));
			TSharedRef<FJsonObject> Integrated = MakeShared<FJsonObject>();
			Integrated->SetStringField(TEXT("fixtureId"), FHansaPlacementAutomationFixture::IntegratedFixtureId);
			Integrated->SetNumberField(TEXT("fixtureVersion"), FHansaPlacementAutomationFixture::IntegratedFixtureVersion);
			Integrated->SetStringField(TEXT("registryHash"), FString::Printf(TEXT("%016llX"),
				static_cast<unsigned long long>(FHansaPlacementAutomationFixture::IntegratedRegistryHash)));
			Integrated->SetStringField(TEXT("purpose"), TEXT("Integrated Lübeck construction, logistics, production, and population world slice"));
			Fixtures.Add(MakeShared<FJsonValueObject>(Integrated));
			StrategicFixture->AppendFixtureDescriptors(Fixtures);
			MultiplayerService->AppendFixtureDescriptor(Fixtures);
			Listed->SetArrayField(TEXT("fixtures"), MoveTemp(Fixtures));
			return MakeSuccessResponse(RequestId, Listed);
		}

		if (Operation == TEXT("fixture_reset"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::FixtureReset, Context);
			if (!Authorized) return MakeErrorResponse(RequestId, Authorized.GetError());
			if (ActiveFixtureId.IsEmpty()) return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
				TEXT("fixture_reset requires an active allowlisted fixture."),
				TEXT("Call fixture_load before fixture_reset.")));
			Payload->SetStringField(TEXT("fixtureId"), ActiveFixtureId);
			Operation = TEXT("fixture_load");
		}

		if (Operation == TEXT("fixture_load"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::FixtureLoad, Context);
			if (!Authorized)
			{
				return MakeErrorResponse(RequestId, Authorized.GetError());
			}
			FString FixtureId;
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
			FString Error;
			if (!Payload->TryGetStringField(TEXT("fixtureId"), FixtureId))
			{
				return MakeErrorResponse(RequestId, InvalidRequest(RequestId, TEXT("fixture_load requires fixtureId."), TEXT("Call fixture_list, then load the exact stable fixtureId.")));
			}
			MultiplayerService->DeactivateFixture();
			const bool bGoldenEvidenceProfile = FixtureId == FHansaStrategicAutomationFixture::GoldenFixtureId &&
				SessionService.AuthorizeOperation(SessionId, ControllerId,
					EHansaAutomationOperation::EvidenceRead, Context).IsSuccess();
			if (FixtureId == FHansaMultiplayerAutomationService::FixtureId)
			{
				PlacementScreenHost.Reset();
				RouteDeliveryScreenHost.Reset();
				StrategicScreenHost.Reset();
				bPlacementFixtureActive = false;
				bRouteDeliveryFixtureActive = false;
				bStrategicFixtureActive = false;
				SemanticRegistry->Reset();
				ProofScreenHost = MakeUnique<FHansaAutomationProofScreenHost>(*SemanticRegistry);
				if (!MultiplayerService->ActivateFixture(Result, Error))
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error,
						TEXT("Launch every proof process with -HansaAuthorityFixture.")));
				}
			}
			else if (FixtureId == FHansaPlacementAutomationFixture::StableFixtureId ||
				FixtureId == FHansaPlacementAutomationFixture::IntegratedFixtureId)
			{
				PlacementScreenHost.Reset();
				RouteDeliveryScreenHost.Reset();
				StrategicScreenHost.Reset();
				bStrategicFixtureActive = false;
				bRouteDeliveryFixtureActive = false;
				ProofScreenHost.Reset();
				const bool bFixtureLoaded = FixtureId == FHansaPlacementAutomationFixture::IntegratedFixtureId
					? PlacementFixture->LoadIntegrated(*SemanticRegistry, Error)
					: PlacementFixture->Load(*SemanticRegistry, Error);
				if (!bFixtureLoaded)
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Reload the exact placement fixture identifier.")));
				}
				bPlacementFixtureActive = true;
				PlacementScreenHost = MakeUnique<FHansaPlacementAutomationScreenHost>(*PlacementFixture, *SemanticRegistry);
				Result->SetBoolField(TEXT("loaded"), true);
				Result->SetStringField(TEXT("fixtureId"), FixtureId);
				Result->SetNumberField(TEXT("fixtureVersion"), PlacementFixture->IsIntegrated()
					? FHansaPlacementAutomationFixture::IntegratedFixtureVersion
					: FHansaPlacementAutomationFixture::FixtureVersion);
				Result->SetNumberField(TEXT("tick"), PlacementFixture->GetSimulationTick());
				Result->SetNumberField(TEXT("placedBuildingCount"), PlacementFixture->GetPlacedBuildingCount());
				Result->SetNumberField(TEXT("semanticRevision"), static_cast<double>(SemanticRegistry->GetRevision()));
			}
			else if (FHansaStrategicAutomationFixture::IsStrategicFixtureId(FixtureId) &&
				(FixtureId != FHansaStrategicAutomationFixture::GoldenFixtureId || bGoldenEvidenceProfile))
			{
				PlacementScreenHost.Reset();
				RouteDeliveryScreenHost.Reset();
				ProofScreenHost.Reset();
				bPlacementFixtureActive = false;
				bRouteDeliveryFixtureActive = false;
				SemanticRegistry->Reset();
				if (!StrategicFixture->Load(FixtureId, Result, Error))
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error,
						TEXT("Reload the exact strategic fixture seed identifier.")));
				}
				bStrategicFixtureActive = true;
				StrategicScreenHost = MakeUnique<FHansaStrategicAutomationScreenHost>(*StrategicFixture, *SemanticRegistry);
			}
			else
			{
				PlacementScreenHost.Reset();
				bPlacementFixtureActive = false;
				StrategicScreenHost.Reset();
				bStrategicFixtureActive = false;
				RouteDeliveryScreenHost.Reset();
				bRouteDeliveryFixtureActive = false;
				SemanticRegistry->Reset();
				if (!ProductionFixtureService->Load(FixtureId, Result, Error))
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Call fixture_list, then load the exact stable fixtureId.")));
				}
				bRouteDeliveryFixtureActive = FixtureId == Hansa::Simulation::FHansaProductionFixture::RouteDeliveryFixtureId;
				if (bRouteDeliveryFixtureActive)
				{
					ProofScreenHost.Reset();
					RouteDeliveryScreenHost = MakeUnique<FHansaRouteDeliveryAutomationScreenHost>(*ProductionFixtureService, *SemanticRegistry);
				}
				else
				{
					ProofScreenHost = MakeUnique<FHansaAutomationProofScreenHost>(*SemanticRegistry);
				}
			}
			ActiveFixtureId = FixtureId;
			CapturedScreenshotPaths.Reset();
			CapturedScreenshotHashes.Reset();
			return MakeSuccessResponse(RequestId, Result);
		}

		if (Operation == TEXT("save_list"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::GameplayQuery, Context);
			if (!Authorized) return MakeErrorResponse(RequestId, Authorized.GetError());
			if (!bStrategicFixtureActive) return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
				TEXT("Save slots require an active strategic fixture."), TEXT("Load save_roundtrip_v1 first.")));
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>(); FString Error;
			return StrategicFixture->ListSaveSlots(Result, Error) ? MakeSuccessResponse(RequestId, Result)
				: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Reload save_roundtrip_v1.")));
		}

		if (Operation == TEXT("save_create") || Operation == TEXT("save_load"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::ControlledCommand, Context);
			if (!Authorized) return MakeErrorResponse(RequestId, Authorized.GetError());
			FString SlotId;
			if (!bStrategicFixtureActive || !Payload->TryGetStringField(TEXT("slotId"), SlotId) ||
				(SlotId != TEXT("manual") && SlotId != TEXT("autosave")))
				return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
					TEXT("The save operation requires save_roundtrip_v1 and slotId manual or autosave."),
					TEXT("Load save_roundtrip_v1 and use an allowlisted slot ID.")));
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>(); FString Error;
			const bool bOk = Operation == TEXT("save_create")
				? StrategicFixture->SaveSlot(SlotId, Result, Error)
				: StrategicFixture->LoadSlot(SlotId, Result, Error);
			return bOk ? MakeSuccessResponse(RequestId, Result)
				: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error,
					Operation == TEXT("save_create") ? TEXT("Retry the controlled slot save.") : TEXT("Create the slot, then retry load.")));
		}

		if (Operation == TEXT("save_assert_roundtrip") || Operation == TEXT("save_wait_for"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::GameplayQuery, Context);
			if (!Authorized) return MakeErrorResponse(RequestId, Authorized.GetError());
			if (!bStrategicFixtureActive) return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
				TEXT("Round-trip verification requires an active save fixture."), TEXT("Load save_roundtrip_v1 first.")));
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>(); FString Error;
			if (Operation == TEXT("save_wait_for"))
			{
				FString Condition;
				if (!Payload->TryGetStringField(TEXT("condition"), Condition))
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId, TEXT("save_wait_for requires condition."), TEXT("Use slot_exists or roundtrip_verified.")));
				if (Condition == TEXT("slot_exists"))
				{
					FString SlotId; if (!Payload->TryGetStringField(TEXT("slotId"), SlotId))
						return MakeErrorResponse(RequestId, InvalidRequest(RequestId, TEXT("slot_exists requires slotId."), TEXT("Use manual or autosave.")));
					if (!StrategicFixture->ListSaveSlots(Result, Error)) return MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Reload the fixture.")));
					bool bExists = false; for (const TSharedPtr<FJsonValue>& Value : Result->GetArrayField(TEXT("slots")))
					{ const TSharedPtr<FJsonObject> Slot = Value->AsObject(); if (Slot && Slot->GetStringField(TEXT("slotId")) == SlotId) bExists = Slot->GetBoolField(TEXT("exists")); }
					if (!bExists) return MakeErrorResponse(RequestId, InvalidRequest(RequestId, TEXT("The requested save slot does not exist yet."), TEXT("Call save_create for that slot.")));
					Result->SetBoolField(TEXT("matched"), true); Result->SetStringField(TEXT("condition"), Condition); return MakeSuccessResponse(RequestId, Result);
				}
				if (Condition != TEXT("roundtrip_verified"))
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId, TEXT("Save wait condition is not allowlisted."), TEXT("Use slot_exists or roundtrip_verified.")));
			}
			return StrategicFixture->AssertRoundTrip(Result, Error) ? MakeSuccessResponse(RequestId, Result)
				: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Create and load the same controlled slot before asserting.")));
		}
		if (Operation == TEXT("gameplay_query"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::GameplayQuery, Context);
			if (!Authorized)
			{
				return MakeErrorResponse(RequestId, Authorized.GetError());
			}
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
			FString Error;
			if (MultiplayerService->IsFixtureActive())
			{
				return MultiplayerService->Query(RequestId, Payload.ToSharedRef(), Result, Error)
					? MakeSuccessResponse(RequestId, Result)
					: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error,
						TEXT("Use multiplayer.status after the launched process is ready.")));
			}
			if (bPlacementFixtureActive)
			{
				FString Query;
				if (!Payload->TryGetStringField(TEXT("query"), Query))
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
						TEXT("gameplay_query requires an allowlisted query name."),
						TEXT("Use integrated.summary or an allowlisted construction query for the active fixture.")));
				}
				if (Query == TEXT("integrated.summary") && PlacementFixture->IsIntegrated())
				{
					return MakeSuccessResponse(RequestId, MakeIntegratedSummary(*PlacementFixture));
				}
				if (Query == TEXT("construction.list"))
				{
					TArray<TSharedPtr<FJsonValue>> Values;
					const auto Projection = PlacementFixture->BuildProjection();
					if (!Projection)
					{
						return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
							TEXT("The construction projection is unavailable."), TEXT("Reload the placement fixture.")));
					}
					for (const auto& Construction : Projection.Value.GetConstructions())
					{
						Values.Add(MakeShared<FJsonValueObject>(MakeConstruction(Construction)));
					}
					Result->SetArrayField(TEXT("constructions"), MoveTemp(Values));
					return MakeSuccessResponse(RequestId, Result);
				}
				if (Query == TEXT("construction.get"))
				{
					int64 BuildingValue = 0;
					const auto BuildingId = TryGetIntegralField(Payload.ToSharedRef(), TEXT("buildingId"), BuildingValue) && BuildingValue > 0
						? Hansa::Simulation::FHansaBuildingId::TryCreate(static_cast<uint64>(BuildingValue))
						: Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaBuildingId>::Failure(
							Hansa::Simulation::EHansaValueError::InvalidFormat);
					const auto Construction = BuildingId ? PlacementFixture->QueryConstruction(BuildingId.Value)
						: TOptional<Hansa::Simulation::FHansaConstructionProjection>();
					if (!Construction.IsSet())
					{
						return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
							TEXT("construction.get requires an existing positive buildingId."),
							TEXT("Call construction.list and use an exact buildingId.")));
					}
					Result->SetObjectField(TEXT("construction"), MakeConstruction(Construction.GetValue()));
					return MakeSuccessResponse(RequestId, Result);
				}
				if (Query == TEXT("construction.cost"))
				{
					FString DefinitionText;
					const auto DefinitionId = Payload->TryGetStringField(TEXT("buildingDefinitionId"), DefinitionText)
						? Hansa::Simulation::FHansaBuildingTypeId::TryParse(DefinitionText)
						: Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaBuildingTypeId>::Failure(
							Hansa::Simulation::EHansaValueError::InvalidFormat);
					if (!DefinitionId)
					{
						return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
							TEXT("construction.cost requires a canonical buildingDefinitionId."),
							TEXT("Use Building.Road or Building.Warehouse in this fixture.")));
					}
					Result->SetObjectField(TEXT("cost"), MakeConstructionCost(
						PlacementFixture->QueryConstructionCost(DefinitionId.Value)));
					return MakeSuccessResponse(RequestId, Result);
				}
				return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
					TEXT("Construction query is not allowlisted."),
					TEXT("Use construction.list, construction.get, or construction.cost.")));
			}
			if (bStrategicFixtureActive)
			{
				return StrategicFixture->Query(Payload.ToSharedRef(), Result, Error)
					? MakeSuccessResponse(RequestId, Result)
					: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Use only the documented strategic query names.")));
			}
			return ProductionFixtureService->Query(Payload.ToSharedRef(), Result, Error)
				? MakeSuccessResponse(RequestId, Result)
				: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Use only the documented allowlisted query names and parameters.")));
		}

		if (Operation == TEXT("gameplay_command"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::ControlledCommand, Context);
			if (!Authorized)
			{
				return MakeErrorResponse(RequestId, Authorized.GetError());
			}
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
			FString Error;
			if (MultiplayerService->IsFixtureActive())
			{
				return MultiplayerService->Command(RequestId, Payload.ToSharedRef(), Result, Error)
					? MakeSuccessResponse(RequestId, Result)
					: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error,
						TEXT("Submit an allowlisted intent from a connected client process.")));
			}
			if (bPlacementFixtureActive)
			{
				FString Command;
				if (!Payload->TryGetStringField(TEXT("command"), Command) ||
					(Command != TEXT("construction.cancel") && Command != TEXT("building.remove")))
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
						TEXT("Placement fixture commands allow construction.cancel or building.remove."),
						TEXT("Query construction state before choosing the lifecycle action.")));
				}
				int64 BuildingValue = 0;
				if (!TryGetIntegralField(Payload.ToSharedRef(), TEXT("buildingId"), BuildingValue) || BuildingValue <= 0 ||
					!PlacementFixture->GetLastPlacedBuildingId().IsValid() ||
					PlacementFixture->GetLastPlacedBuildingId().GetValue() != static_cast<uint64>(BuildingValue))
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
						TEXT("The lifecycle action requires the exact current buildingId."),
						TEXT("Call construction.list and pass its positive buildingId.")));
				}
				const bool bSucceeded = Command == TEXT("construction.cancel")
					? PlacementFixture->CancelLastConstructionIntent()
					: PlacementFixture->RemoveLastBuildingIntent();
				if (!bSucceeded)
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
						TEXT("The authoritative construction command was rejected."),
						TEXT("Cancel only unfinished work; remove only completed dependency-free buildings.")));
				}
				Result->SetStringField(TEXT("command"), Command);
				Result->SetBoolField(TEXT("accepted"), true);
				Result->SetNumberField(TEXT("tick"), PlacementFixture->GetSimulationTick());
				Result->SetNumberField(TEXT("placedBuildingCount"), PlacementFixture->GetPlacedBuildingCount());
				return MakeSuccessResponse(RequestId, Result);
			}
			if (bStrategicFixtureActive)
			{
				return StrategicFixture->Command(Payload.ToSharedRef(), Result, Error)
					? MakeSuccessResponse(RequestId, Result)
					: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Use an allowlisted strategic gameplay command.")));
			}
			return ProductionFixtureService->Command(Payload.ToSharedRef(), Result, Error)
				? MakeSuccessResponse(RequestId, Result)
				: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Use an allowlisted controlled gameplay command.")));
		}

		if (Operation == TEXT("gameplay_assert"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::WaitFor, Context);
			if (!Authorized)
			{
				return MakeErrorResponse(RequestId, Authorized.GetError());
			}
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
			FString Error;
			if (bPlacementFixtureActive && PlacementFixture->IsIntegrated())
			{
				if (!Payload->HasTypedField<EJson::Object>(TEXT("predicate")))
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
						TEXT("gameplay_assert requires an integrated predicate."),
						TEXT("Use construction.completed, inventory.moved, production.completed, population.grown, or bread.consumed.")));
				}
				const TSharedRef<FJsonObject> PredicateObject = Payload->GetObjectField(TEXT("predicate")).ToSharedRef();
				FString Predicate;
				if (!PredicateObject->TryGetStringField(TEXT("kind"), Predicate))
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
						TEXT("The integrated predicate requires kind."),
						TEXT("Use an allowlisted integrated.* predicate kind.")));
				}
				const TSharedRef<FJsonObject> Summary = MakeIntegratedSummary(*PlacementFixture);
				const bool bMatched = IntegratedPredicateMatches(Summary, Predicate);
				Result->SetObjectField(TEXT("predicate"), PredicateObject);
				Result->SetBoolField(TEXT("matched"), bMatched);
				Result->SetObjectField(TEXT("summary"), Summary);
				return bMatched ? MakeSuccessResponse(RequestId, Result)
					: MakeErrorResponse(RequestId, InvalidRequest(RequestId,
						TEXT("The integrated gameplay assertion did not match."),
						TEXT("Advance or run-until the authoritative fixture, then inspect integrated.summary.")));
			}
			if (bStrategicFixtureActive)
			{
				return StrategicFixture->AssertPredicate(Payload.ToSharedRef(), Result, Error)
					? MakeSuccessResponse(RequestId, Result)
					: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Use an allowlisted strategic assertion.")));
			}
			return ProductionFixtureService->AssertPredicate(Payload.ToSharedRef(), Result, Error)
				? MakeSuccessResponse(RequestId, Result)
				: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Use an allowlisted gameplay predicate.")));
		}

		if (Operation == TEXT("simulation_step") || Operation == TEXT("simulation_run"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::ControlledCommand, Context);
			if (!Authorized)
			{
				return MakeErrorResponse(RequestId, Authorized.GetError());
			}
			int64 TickCount = Operation == TEXT("simulation_step") ? 1 : 0;
			if (Operation == TEXT("simulation_run") && !TryGetIntegralField(Payload.ToSharedRef(), TEXT("tickCount"), TickCount))
			{
				return MakeErrorResponse(RequestId, InvalidRequest(RequestId, TEXT("simulation_run requires integral tickCount."), TEXT("Use 1 through 10000 ticks.")));
			}
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
			FString Error;
			if (MultiplayerService->IsFixtureActive())
			{
				return MultiplayerService->Step(RequestId, static_cast<int32>(TickCount), Result, Error)
					? MakeSuccessResponse(RequestId, Result)
					: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error,
						TEXT("Advance the paused fixture only from its authority process.")));
			}
			if (bPlacementFixtureActive)
			{
				if (!PlacementFixture->AdvanceTicks(static_cast<int32>(TickCount)))
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
						TEXT("The construction simulation step was rejected."), TEXT("Use 1 through 10000 ticks.")));
				}
				Result->SetNumberField(TEXT("ticksAdvanced"), static_cast<double>(TickCount));
				Result->SetNumberField(TEXT("tick"), PlacementFixture->GetSimulationTick());
				Result->SetNumberField(TEXT("placedBuildingCount"), PlacementFixture->GetPlacedBuildingCount());
				return MakeSuccessResponse(RequestId, Result);
			}
			if (bStrategicFixtureActive)
			{
				return StrategicFixture->Step(static_cast<int32>(TickCount), Result, Error)
					? MakeSuccessResponse(RequestId, Result)
					: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Advance the strategic fixture with a bounded tick count.")));
			}
			return ProductionFixtureService->Step(static_cast<int32>(TickCount), Result, Error)
				? MakeSuccessResponse(RequestId, Result)
				: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Load the fixture and use a bounded positive tick count.")));
		}

		if (Operation == TEXT("simulation_run_until"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::ControlledCommand, Context);
			if (!Authorized)
			{
				return MakeErrorResponse(RequestId, Authorized.GetError());
			}
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
			FString Error;
			if (bPlacementFixtureActive && PlacementFixture->IsIntegrated())
			{
				int64 MaximumTicks = 0;
				if (!Payload->HasTypedField<EJson::Object>(TEXT("predicate")) ||
					!TryGetIntegralField(Payload.ToSharedRef(), TEXT("maximumTicks"), MaximumTicks) ||
					MaximumTicks <= 0 || MaximumTicks > 10'000)
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
						TEXT("Integrated run-until requires an allowlisted predicate and maximumTicks from 1 through 10000."),
						TEXT("Use construction.completed, inventory.moved, production.completed, population.grown, or bread.consumed.")));
				}
				const TSharedRef<FJsonObject> PredicateObject = Payload->GetObjectField(TEXT("predicate")).ToSharedRef();
				FString Predicate;
				if (!PredicateObject->TryGetStringField(TEXT("kind"), Predicate))
				{
					return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
						TEXT("The integrated predicate requires kind."),
						TEXT("Use an allowlisted integrated.* predicate kind.")));
				}
				for (int64 Index = 0; Index <= MaximumTicks; ++Index)
				{
					const TSharedRef<FJsonObject> Summary = MakeIntegratedSummary(*PlacementFixture);
					if (IntegratedPredicateMatches(Summary, Predicate))
					{
						Result->SetObjectField(TEXT("predicate"), PredicateObject);
						Result->SetBoolField(TEXT("matched"), true);
						Result->SetNumberField(TEXT("ticksAdvanced"), Index);
						Result->SetObjectField(TEXT("summary"), Summary);
						return MakeSuccessResponse(RequestId, Result);
					}
					if (Index < MaximumTicks && !PlacementFixture->AdvanceTicks(1)) break;
				}
				return MakeErrorResponse(RequestId, MakeEndpointError(
					EHansaAutomationErrorCode::TimedOut, RequestId,
					TEXT("The integrated gameplay predicate did not match within maximumTicks."),
					TEXT("Inspect integrated.summary and its causal construction, logistics, production, and population fields."), true));
			}
			if (bStrategicFixtureActive)
			{
				return StrategicFixture->RunUntil(Payload.ToSharedRef(), Result, Error)
					? MakeSuccessResponse(RequestId, Result)
					: MakeErrorResponse(RequestId, MakeEndpointError(EHansaAutomationErrorCode::TimedOut, RequestId,
						*Error, TEXT("Inspect strategic.evidence and the failed checkpoint."), true));
			}
			return ProductionFixtureService->RunUntil(Payload.ToSharedRef(), Result, Error)
				? MakeSuccessResponse(RequestId, Result)
				: MakeErrorResponse(RequestId, InvalidRequest(RequestId, *Error, TEXT("Use an allowlisted predicate and maximumTicks from 1 through 10000.")));
		}

		const bool bSemanticRead = Operation == TEXT("semantic_find") || Operation == TEXT("semantic_state");
		const bool bSemanticAction = Operation == TEXT("semantic_activate") || Operation == TEXT("semantic_focus");
		if (bSemanticRead || bSemanticAction)
		{
			const EHansaAutomationOperation AuthorizedOperation = bSemanticAction
				? EHansaAutomationOperation::SemanticUiAction
				: EHansaAutomationOperation::SemanticUiRead;
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, AuthorizedOperation, Context);
			if (!Authorized)
			{
				return MakeErrorResponse(RequestId, Authorized.GetError());
			}
			FString SemanticId;
			if (!Payload->TryGetStringField(TEXT("semanticId"), SemanticId) || !IsValidSemanticId(SemanticId))
			{
				return MakeErrorResponse(RequestId, InvalidRequest(
					RequestId,
					TEXT("A valid stable semanticId is required."),
					TEXT("Use a namespaced semantic ID returned by semantic_find.")));
			}
			const bool bScreenReady = bStrategicFixtureActive
				? StrategicScreenHost.IsValid() && StrategicScreenHost->EnsureScreen()
				: bRouteDeliveryFixtureActive
					? RouteDeliveryScreenHost.IsValid() && RouteDeliveryScreenHost->EnsureScreen()
				: bPlacementFixtureActive
					? PlacementScreenHost.IsValid() && PlacementScreenHost->EnsureScreen()
					: ProofScreenHost.IsValid() && ProofScreenHost->EnsureScreen();
			if (!bScreenReady)
			{
				return MakeErrorResponse(RequestId, MakeEndpointError(
					EHansaAutomationErrorCode::CaptureUnavailable,
					RequestId,
					TEXT("The native automation proof screen is unavailable."),
					TEXT("Run an explicitly enabled Development game with Slate initialized."),
					true));
			}
			if (bStrategicFixtureActive) StrategicScreenHost->SynchronizeSemantics();
			else if (bRouteDeliveryFixtureActive) RouteDeliveryScreenHost->SynchronizeSemantics();
			else if (bPlacementFixtureActive) PlacementScreenHost->SynchronizeSemantics();
			else ProofScreenHost->SynchronizeSemantics();
			const FHansaSemanticNode* Node = SemanticRegistry->FindNode(SemanticId);
			if (Node == nullptr)
			{
				return MakeErrorResponse(RequestId, MakeEndpointError(
					EHansaAutomationErrorCode::SemanticNodeNotFound,
					RequestId,
					TEXT("The semantic node was not found."),
					TEXT("Use a stable ID present in the current semantic snapshot.")));
			}
			if (bSemanticAction)
			{
				const EHansaSemanticAction Action = Operation == TEXT("semantic_activate")
					? EHansaSemanticAction::Activate
					: EHansaSemanticAction::Focus;
				const FHansaSemanticActionResult ActionResult = SemanticRegistry->Invoke(SemanticId, Action);
				if (!ActionResult.IsSuccess())
				{
					return MakeErrorResponse(RequestId, MakeEndpointError(
						EHansaAutomationErrorCode::SemanticActionUnsupported,
						RequestId,
						TEXT("The semantic node cannot perform the requested action."),
						TEXT("Inspect the node's actions array and choose an advertised action.")));
				}
				if (bStrategicFixtureActive) StrategicScreenHost->SynchronizeSemantics();
				else if (bRouteDeliveryFixtureActive) RouteDeliveryScreenHost->SynchronizeSemantics();
				else if (bPlacementFixtureActive) PlacementScreenHost->SynchronizeSemantics();
				else ProofScreenHost->SynchronizeSemantics();
				Node = SemanticRegistry->FindNode(SemanticId);
			}
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
			Result->SetNumberField(TEXT("revision"), static_cast<double>(SemanticRegistry->GetRevision()));
			Result->SetObjectField(TEXT("node"), MakeSemanticNode(*Node));
			return MakeSuccessResponse(RequestId, Result);
		}

		if (Operation == TEXT("wait_for"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::WaitFor, Context);
			if (!Authorized)
			{
				return MakeErrorResponse(RequestId, Authorized.GetError());
			}
			FString SemanticId;
			FString PropertyText;
			bool bExpected = true;
			if (!Payload->TryGetStringField(TEXT("semanticId"), SemanticId) ||
				!Payload->TryGetStringField(TEXT("property"), PropertyText) ||
				!Payload->TryGetBoolField(TEXT("expected"), bExpected) ||
				!IsValidSemanticId(SemanticId))
			{
				return MakeErrorResponse(RequestId, InvalidRequest(
					RequestId,
					TEXT("wait_for requires semanticId, property and expected fields."),
					TEXT("Use an observable semantic boolean property and a bounded request timeout.")));
			}
			EHansaSemanticProperty Property;
			if (!TryParseSemanticProperty(PropertyText, Property))
			{
				return MakeErrorResponse(RequestId, InvalidRequest(
					RequestId,
					TEXT("wait_for requested an unknown observable property."),
					TEXT("Use exists, visible, enabled, focused, selected, loading, warning, or error.")));
			}
			if (bStrategicFixtureActive && StrategicScreenHost.IsValid())
			{
				StrategicScreenHost->EnsureScreen();
				StrategicScreenHost->SynchronizeSemantics();
			}
			else if (bRouteDeliveryFixtureActive && RouteDeliveryScreenHost.IsValid())
			{
				RouteDeliveryScreenHost->EnsureScreen();
				RouteDeliveryScreenHost->SynchronizeSemantics();
			}
			else if (bPlacementFixtureActive && PlacementScreenHost.IsValid())
			{
				PlacementScreenHost->EnsureScreen();
				PlacementScreenHost->SynchronizeSemantics();
			}
			else if (ProofScreenHost.IsValid())
			{
				ProofScreenHost->EnsureScreen();
				ProofScreenHost->SynchronizeSemantics();
			}
			const FHansaSemanticPredicate Predicate { SemanticId, Property, bExpected };
			const int64 Deadline = Context.EnqueuedAtMonotonicMilliseconds + Context.TimeoutMilliseconds;
			const bool bStarted = WaitService->BeginWait(
				RequestId,
				Predicate,
				Deadline,
				[this, RequestId](const FHansaSemanticWaitResult& WaitResult)
				{
					if (WaitResult.bTimedOut)
					{
						WriteResponse(MakeErrorResponse(RequestId, MakeEndpointError(
							EHansaAutomationErrorCode::TimedOut,
							RequestId,
							TEXT("The observable semantic predicate did not match before its deadline."),
							TEXT("Inspect semantic_state, correct the predicate, or retry with a bounded timeout."),
							true)));
						return;
					}
					TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
					Result->SetBoolField(TEXT("matched"), true);
					Result->SetStringField(TEXT("semanticId"), WaitResult.Predicate.SemanticId);
					Result->SetStringField(TEXT("property"), LexToString(WaitResult.Predicate.Property));
					Result->SetBoolField(TEXT("expected"), WaitResult.Predicate.bExpected);
					Result->SetNumberField(TEXT("revision"), static_cast<double>(WaitResult.ObservedRevision));
					WriteResponse(MakeSuccessResponse(RequestId, Result));
				});
			if (!bStarted)
			{
				return MakeErrorResponse(RequestId, InvalidRequest(
					RequestId,
					TEXT("The wait request could not be registered."),
					TEXT("Use a unique request and a valid observable semantic predicate.")));
			}
			return TOptional<FString>();
		}

		if (Operation == TEXT("logs_get"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::EvidenceRead, Context);
			if (!Authorized) return MakeErrorResponse(RequestId, Authorized.GetError());
			if (!bStrategicFixtureActive) return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
				TEXT("logs_get requires an active strategic fixture."),
				TEXT("Load lubeck_grain_shortage_v1 before requesting synchronized logs.")));
			int64 MaximumEntries = 256;
			if (Payload->HasField(TEXT("maximumEntries")) &&
				(!TryGetIntegralField(Payload.ToSharedRef(), TEXT("maximumEntries"), MaximumEntries) ||
				 MaximumEntries < 1 || MaximumEntries > 512))
			{
				return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
					TEXT("logs_get maximumEntries must be an integer from 1 through 512."),
					TEXT("Use a bounded structured log request.")));
			}
			return MakeSuccessResponse(RequestId, StrategicFixture->MakeLogSnapshot(static_cast<int32>(MaximumEntries)));
		}

		if (Operation == TEXT("evidence_bundle_create"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::EvidenceWrite, Context);
			if (!Authorized) return MakeErrorResponse(RequestId, Authorized.GetError());
			FString BundleId, TestId, McpProtocolVersion;
			if (!bStrategicFixtureActive ||
				StrategicFixture->GetFixtureId() != FHansaStrategicAutomationFixture::GoldenFixtureId ||
				!Payload->TryGetStringField(TEXT("bundleId"), BundleId) || !IsBoundedWireIdentifier(BundleId) ||
				!Payload->TryGetStringField(TEXT("testId"), TestId) || TestId != TEXT("s14-p01-mvp-golden") ||
				!Payload->TryGetStringField(TEXT("mcpProtocolVersion"), McpProtocolVersion) ||
				!IsBoundedWireIdentifier(McpProtocolVersion) || McpProtocolVersion.Len() > 32)
			{
				return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
					TEXT("evidence_bundle_create requires the active S14 golden fixture and bounded protocol/test identifiers."),
					TEXT("Load lubeck_grain_shortage_v1 and use testId s14-p01-mvp-golden.")));
			}

			TArray<TSharedPtr<FJsonValue>> Assertions;
			bool bCallerAssertionsPassed = true;
			const TArray<TSharedPtr<FJsonValue>>* RequestedAssertions = nullptr;
			if (Payload->TryGetArrayField(TEXT("assertions"), RequestedAssertions))
			{
				if (RequestedAssertions->Num() > 128) return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
					TEXT("The evidence assertion list exceeds 128 entries."), TEXT("Submit only bounded golden checkpoints.")));
				for (const TSharedPtr<FJsonValue>& Value : *RequestedAssertions)
				{
					const TSharedPtr<FJsonObject>* AssertionObject = nullptr;
					FString AssertionId; bool bPassed = false;
					if (!Value.IsValid() || !Value->TryGetObject(AssertionObject) || !AssertionObject->IsValid() ||
						!(*AssertionObject)->TryGetStringField(TEXT("id"), AssertionId) ||
						!IsBoundedWireIdentifier(AssertionId) ||
						!(*AssertionObject)->TryGetBoolField(TEXT("passed"), bPassed))
					{
						return MakeErrorResponse(RequestId, InvalidRequest(RequestId,
							TEXT("Each evidence assertion requires a bounded id and boolean passed field."),
							TEXT("Use the checked-in evidence_bundle_create schema.")));
					}
					bCallerAssertionsPassed &= bPassed;
					Assertions.Add(MakeShared<FJsonValueObject>(*AssertionObject));
				}
			}

			if (StrategicScreenHost.IsValid())
			{
				StrategicScreenHost->EnsureScreen();
				StrategicScreenHost->SynchronizeSemantics();
			}
			const TCHAR* RequiredSemantics[] = {
				TEXT("BuildMenu.Status.BreadChain"), TEXT("Market.Status.GrainDiagnosed"),
				TEXT("TradeRoute.Editor.Status.Delivered"), TEXT("Research.Status.MarketReports"),
				TEXT("HUD.Status.MerchantAI"), TEXT("SaveLoad.Status.RoundTrip"),
				TEXT("Scenario.Status.Victory") };
			bool bStructuralAssertionsPassed = true;
			for (const TCHAR* Id : RequiredSemantics)
			{
				const FHansaSemanticNode* Node = SemanticRegistry->FindNode(Id);
				bStructuralAssertionsPassed &= Node != nullptr && Node->State.bSelected;
			}
			const bool bHas720 = CapturedScreenshotPaths.ContainsByPredicate([](const FString& Path)
				{ return Path.Contains(TEXT("1280x720")); });
			const bool bHas1080 = CapturedScreenshotPaths.ContainsByPredicate([](const FString& Path)
				{ return Path.Contains(TEXT("1920x1080")); });
			const bool bComplete = bCallerAssertionsPassed && bStructuralAssertionsPassed &&
				StrategicFixture->HasVerifiedRoundTrip() && bHas720 && bHas1080;

			const TSharedRef<FJsonObject> QuerySnapshot = StrategicFixture->MakeEvidenceSnapshot();
			const TSharedRef<FJsonObject> SemanticSnapshot = MakeSemanticSnapshot(*SemanticRegistry);
			const TSharedRef<FJsonObject> LogSnapshot = StrategicFixture->MakeLogSnapshot();
			TSharedRef<FJsonObject> AssertionSnapshot = MakeShared<FJsonObject>();
			AssertionSnapshot->SetBoolField(TEXT("complete"), bComplete);
			AssertionSnapshot->SetBoolField(TEXT("callerAssertionsPassed"), bCallerAssertionsPassed);
			AssertionSnapshot->SetBoolField(TEXT("structuralAssertionsPassed"), bStructuralAssertionsPassed);
			AssertionSnapshot->SetBoolField(TEXT("native1280x720Captured"), bHas720);
			AssertionSnapshot->SetBoolField(TEXT("native1920x1080Captured"), bHas1080);
			AssertionSnapshot->SetArrayField(TEXT("assertions"), MoveTemp(Assertions));

			TSharedRef<FJsonObject> Protocols = MakeShared<FJsonObject>();
			Protocols->SetStringField(TEXT("mcp"), McpProtocolVersion);
			Protocols->SetNumberField(TEXT("wireSchema"), WireSchemaVersion);
			Protocols->SetObjectField(TEXT("automation"), MakeProtocolVersion(SessionService.GetProtocolVersion()));
			TSharedRef<FJsonObject> StateHashes = MakeShared<FJsonObject>();
			StateHashes->SetStringField(TEXT("initial"), QuerySnapshot->GetStringField(TEXT("initialStateHash")));
			StateHashes->SetStringField(TEXT("final"), QuerySnapshot->GetStringField(TEXT("stateHash")));
			StateHashes->SetStringField(TEXT("projection"), QuerySnapshot->GetStringField(TEXT("projectionDigest")));
			const TSharedPtr<FJsonObject> RoundTrip = QuerySnapshot->GetObjectField(TEXT("saveRoundTrip"));
			StateHashes->SetStringField(TEXT("savedAuthoritative"), RoundTrip->GetStringField(TEXT("savedAuthoritativeHash")));
			StateHashes->SetStringField(TEXT("savedProjection"), RoundTrip->GetStringField(TEXT("savedProjectionDigest")));
			TArray<TSharedPtr<FJsonValue>> Screenshots;
			for (int32 Index = 0; Index < CapturedScreenshotPaths.Num(); ++Index)
			{
				TSharedRef<FJsonObject> Screenshot = MakeShared<FJsonObject>();
				Screenshot->SetStringField(TEXT("path"), CapturedScreenshotPaths[Index]);
				Screenshot->SetStringField(TEXT("sha1"), CapturedScreenshotHashes.IsValidIndex(Index) ? CapturedScreenshotHashes[Index] : FString());
				Screenshots.Add(MakeShared<FJsonValueObject>(Screenshot));
			}

			TSharedRef<FJsonObject> Bundle = MakeShared<FJsonObject>();
			Bundle->SetNumberField(TEXT("evidenceSchemaVersion"), 1);
			Bundle->SetStringField(TEXT("testId"), TestId);
			Bundle->SetStringField(TEXT("bundleId"), BundleId);
			Bundle->SetBoolField(TEXT("complete"), bComplete);
			Bundle->SetObjectField(TEXT("protocols"), Protocols);
			Bundle->SetStringField(TEXT("fixtureId"), StrategicFixture->GetFixtureId());
			Bundle->SetNumberField(TEXT("fixtureVersion"), StrategicFixture->GetFixtureVersion());
			Bundle->SetStringField(TEXT("contentHash"), QuerySnapshot->GetStringField(TEXT("contentHash")));
			Bundle->SetStringField(TEXT("fixtureHash"), QuerySnapshot->GetStringField(TEXT("fixtureHash")));
			Bundle->SetStringField(TEXT("seed"), QuerySnapshot->GetStringField(TEXT("campaignSeed")));
			Bundle->SetObjectField(TEXT("stateHashes"), StateHashes);
			Bundle->SetArrayField(TEXT("screenshots"), MoveTemp(Screenshots));

			const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestEvidence"), TEXT("Automation"),
				TEXT("S14P01"), BundleId);
			IFileManager::Get().MakeDirectory(*Root, true);
			auto SaveJson = [&Root](const TCHAR* Filename, const TSharedRef<FJsonObject>& Json)
			{
				return FFileHelper::SaveStringToFile(SerializeResponse(Json), *FPaths::Combine(Root, Filename),
					FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			};
			if (!SaveJson(TEXT("bundle.json"), Bundle) ||
				!SaveJson(TEXT("query-snapshot.json"), QuerySnapshot) ||
				!SaveJson(TEXT("semantic-ui.json"), SemanticSnapshot) ||
				!SaveJson(TEXT("logs.json"), LogSnapshot) ||
				!SaveJson(TEXT("assertions.json"), AssertionSnapshot))
			{
				return MakeErrorResponse(RequestId, MakeEndpointError(EHansaAutomationErrorCode::EvidenceWriteFailed,
					RequestId, TEXT("The synchronized S14 evidence bundle could not be persisted."),
					TEXT("Verify write access beneath Saved/TestEvidence/Automation/S14P01.")));
			}
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
			Result->SetBoolField(TEXT("complete"), bComplete);
			Result->SetStringField(TEXT("bundleId"), BundleId);
			Result->SetStringField(TEXT("testId"), TestId);
			Result->SetStringField(TEXT("rootPath"), Root);
			Result->SetStringField(TEXT("manifestPath"), FPaths::Combine(Root, TEXT("bundle.json")));
			Result->SetStringField(TEXT("querySnapshotPath"), FPaths::Combine(Root, TEXT("query-snapshot.json")));
			Result->SetStringField(TEXT("semanticSnapshotPath"), FPaths::Combine(Root, TEXT("semantic-ui.json")));
			Result->SetStringField(TEXT("logSnapshotPath"), FPaths::Combine(Root, TEXT("logs.json")));
			Result->SetStringField(TEXT("assertionPath"), FPaths::Combine(Root, TEXT("assertions.json")));
			return MakeSuccessResponse(RequestId, Result);
		}

		if (Operation == TEXT("screenshot_capture"))
		{
			const FHansaAutomationOperationResult Authorized = SessionService.AuthorizeOperation(
				SessionId, ControllerId, EHansaAutomationOperation::ScreenshotCapture, Context);
			if (!Authorized)
			{
				return MakeErrorResponse(RequestId, Authorized.GetError());
			}
			int64 Width = 0;
			int64 Height = 0;
			FString BundleId = RequestId;
			Payload->TryGetStringField(TEXT("bundleId"), BundleId);
			if (!TryGetIntegralField(Payload.ToSharedRef(), TEXT("width"), Width) ||
				!TryGetIntegralField(Payload.ToSharedRef(), TEXT("height"), Height))
			{
				return MakeErrorResponse(RequestId, InvalidRequest(
					RequestId,
					TEXT("screenshot_capture requires integral width and height fields."),
					TEXT("Request exactly 1280x720 or 1920x1080.")));
			}
			const FIntPoint Size(static_cast<int32>(Width), static_cast<int32>(Height));
			if (!FHansaNativeScreenshotService::IsSupportedSize(Size))
			{
				return MakeErrorResponse(RequestId, MakeEndpointError(
					EHansaAutomationErrorCode::InvalidCaptureSize,
					RequestId,
					TEXT("The requested screenshot size is not supported."),
					TEXT("Request exactly 1280x720 or 1920x1080.")));
			}
			const bool bScreenReady = bStrategicFixtureActive
				? StrategicScreenHost.IsValid() && StrategicScreenHost->EnsureScreen(Size)
				: bRouteDeliveryFixtureActive
					? RouteDeliveryScreenHost.IsValid() && RouteDeliveryScreenHost->EnsureScreen(Size)
				: bPlacementFixtureActive
					? PlacementScreenHost.IsValid() && PlacementScreenHost->EnsureScreen(Size)
					: ProofScreenHost.IsValid() && ProofScreenHost->EnsureScreen(Size);
			if (!bScreenReady)
			{
				return MakeErrorResponse(RequestId, MakeEndpointError(
					EHansaAutomationErrorCode::CaptureUnavailable,
					RequestId,
					TEXT("The native Slate capture surface is unavailable."),
					TEXT("Run an explicitly enabled Development game with Slate initialized."),
					true));
			}
			if (bStrategicFixtureActive) StrategicScreenHost->SynchronizeSemantics();
			else if (bRouteDeliveryFixtureActive) RouteDeliveryScreenHost->SynchronizeSemantics();
			else if (bPlacementFixtureActive) PlacementScreenHost->SynchronizeSemantics();
			else ProofScreenHost->SynchronizeSemantics();
			FHansaScreenshotContext ScreenshotContext;
			ScreenshotContext.BundleId = BundleId;
			ScreenshotContext.MapName = GetCurrentMapName();
			if (MultiplayerService->IsFixtureActive())
			{
				ScreenshotContext.EvidenceSuiteId = TEXT("S11P04");
				ScreenshotContext.FixtureId = FHansaMultiplayerAutomationService::FixtureId;
				ScreenshotContext.ScreenId = TEXT("Multiplayer.AuthorityProof");
				ScreenshotContext.FlowId = TEXT("two-player-authority-v1");
				FString EvidenceError;
				const TSharedRef<FJsonObject> Evidence =
					MultiplayerService->MakeEvidenceSnapshot(RequestId, EvidenceError);
				ScreenshotContext.SimulationTick =
					static_cast<int64>(Evidence->GetNumberField(TEXT("serverTick")));
				ScreenshotContext.QuerySnapshotJson = SerializeResponse(Evidence);
				ScreenshotContext.FixtureMetadataJson =
					TEXT("{\"schemaVersion\":1,\"fixtureId\":\"two_player_authority_v1\",\"seed\":\"0x533131503034\",\"postCaptureResized\":false}");
				ScreenshotContext.StructuralAssertions = {
					TEXT("fixture.two_player_authority_v1=true"),
					TEXT("projection.ready=true"),
					TEXT("projection.authoritativeHash.present=true"),
					TEXT("projection.projectionDigest.present=true") };
				FString Hash;
				FString Digest;
				ScreenshotContext.bStructuralAssertionsPassed =
					EvidenceError.IsEmpty() && Evidence->GetBoolField(TEXT("ready")) &&
					Evidence->TryGetStringField(TEXT("authoritativeHash"), Hash) && !Hash.IsEmpty() &&
					Evidence->TryGetStringField(TEXT("projectionDigest"), Digest) && !Digest.IsEmpty();
			}
			else if (bStrategicFixtureActive)
			{
				const bool bGolden = StrategicFixture->GetFixtureId() == FHansaStrategicAutomationFixture::GoldenFixtureId;
				ScreenshotContext.EvidenceSuiteId = bGolden ? TEXT("S14P01") : TEXT("S10P04");
				ScreenshotContext.FixtureId = StrategicFixture->GetFixtureId();
				ScreenshotContext.ScreenId = bGolden ? TEXT("HUD.Root") : TEXT("Strategic.Root");
				ScreenshotContext.FlowId = bGolden ? TEXT("s14-p01-mvp-golden") : TEXT("strategic-vertical-slice-v1");
				ScreenshotContext.SimulationTick = StrategicFixture->GetHost()->GetSimulationTick();
				ScreenshotContext.QuerySnapshotJson = SerializeResponse(StrategicFixture->MakeEvidenceSnapshot());
				const FHansaSemanticNode* VictoryNode = SemanticRegistry->FindNode(
					bGolden ? TEXT("Scenario.Status.Victory") : TEXT("Strategic.Status.Victory"));
				const bool bFinalCapture = VictoryNode != nullptr && VictoryNode->State.bSelected;
				TArray<const TCHAR*> Required;
				if (bGolden && !bFinalCapture)
				{
					ScreenshotContext.ScreenId = TEXT("Market.Good.Grain");
					TSharedRef<FJsonObject> DiagnosisRequest = MakeShared<FJsonObject>();
					DiagnosisRequest->SetStringField(TEXT("query"), TEXT("market.diagnosis"));
					DiagnosisRequest->SetStringField(TEXT("cityId"), TEXT("City.Lubeck"));
					DiagnosisRequest->SetStringField(TEXT("goodId"), TEXT("Good.Grain"));
					TSharedRef<FJsonObject> Diagnosis = MakeShared<FJsonObject>();
					FString DiagnosisError;
					const bool bDiagnosisAvailable = StrategicFixture->Query(
						DiagnosisRequest, Diagnosis, DiagnosisError);
					if (bDiagnosisAvailable)
					{
						ScreenshotContext.QuerySnapshotJson = SerializeResponse(Diagnosis);
					}
					ScreenshotContext.StructuralAssertions = {
						TEXT("semantic.Market.Good.Grain.warning=true"),
						TEXT("semantic.Market.Status.GrainDiagnosed.selected=true"),
						TEXT("query.marketDiagnosis.stockBelowReserve=true") };
					const FHansaSemanticNode* Grain = SemanticRegistry->FindNode(TEXT("Market.Good.Grain"));
					const FHansaSemanticNode* Diagnosed = SemanticRegistry->FindNode(TEXT("Market.Status.GrainDiagnosed"));
					ScreenshotContext.bStructuralAssertionsPassed = bDiagnosisAvailable &&
						Diagnosis->GetBoolField(TEXT("shortageDiagnosed")) &&
						Grain != nullptr && Grain->State.bWarning &&
						Diagnosed != nullptr && Diagnosed->State.bSelected;
				}
				else
				{
					ScreenshotContext.StructuralAssertions = bGolden
						? TArray<FString> {
							TEXT("semantic.BuildMenu.Status.BreadChain.selected=true"),
							TEXT("semantic.Market.Status.GrainDiagnosed.selected=true"),
							TEXT("semantic.TradeRoute.Editor.Status.Delivered.selected=true"),
							TEXT("semantic.Research.Status.MarketReports.selected=true"),
							TEXT("semantic.HUD.Status.MerchantAI.selected=true"),
							TEXT("semantic.SaveLoad.Status.RoundTrip.selected=true"),
							TEXT("semantic.Scenario.Status.Victory.selected=true") }
						: TArray<FString> {
							TEXT("semantic.Strategic.Status.Building.selected=true"),
							TEXT("semantic.Strategic.Status.Shortage.selected=true"),
							TEXT("semantic.Strategic.Status.RouteRecovery.selected=true"),
							TEXT("semantic.Strategic.Status.Research.selected=true"),
							TEXT("semantic.Strategic.Status.AI.selected=true"),
							TEXT("semantic.Strategic.Status.Victory.selected=true") };
					Required = bGolden
						? TArray<const TCHAR*> {TEXT("BuildMenu.Status.BreadChain"), TEXT("Market.Status.GrainDiagnosed"),
							TEXT("TradeRoute.Editor.Status.Delivered"), TEXT("Research.Status.MarketReports"),
							TEXT("HUD.Status.MerchantAI"), TEXT("SaveLoad.Status.RoundTrip"), TEXT("Scenario.Status.Victory")}
						: TArray<const TCHAR*> {TEXT("Strategic.Status.Building"), TEXT("Strategic.Status.Shortage"),
							TEXT("Strategic.Status.RouteRecovery"), TEXT("Strategic.Status.Research"),
							TEXT("Strategic.Status.AI"), TEXT("Strategic.Status.Victory")};
					ScreenshotContext.bStructuralAssertionsPassed = true;
					for (const TCHAR* Id : Required)
					{
						const FHansaSemanticNode* Node = SemanticRegistry->FindNode(Id);
						ScreenshotContext.bStructuralAssertionsPassed &= Node != nullptr && Node->State.bSelected;
					}
				}
			}
			else if (bRouteDeliveryFixtureActive)
			{
				const Hansa::Simulation::FHansaProductionFixture* Fixture = ProductionFixtureService->GetFixture();
				ScreenshotContext.EvidenceSuiteId = TEXT("S09P04");
				ScreenshotContext.FixtureId = Hansa::Simulation::FHansaProductionFixture::RouteDeliveryFixtureId;
				ScreenshotContext.ScreenId = SemanticRegistry->FindNode(TEXT("RouteDelivery.Market"))->State.bVisible
					? TEXT("RouteDelivery.Market") : TEXT("RouteDelivery.RouteEditor");
				ScreenshotContext.FlowId = TEXT("route-delivery-v1");
				if (Fixture != nullptr)
				{
					const auto Projection = Fixture->BuildProjection();
					ScreenshotContext.SimulationTick = Projection ? Projection.Value.GetClock().GetTick().GetValue() : 0;
					ScreenshotContext.QuerySnapshotJson = MakeRouteEvidenceSnapshot(*Fixture);
				}
				ScreenshotContext.StructuralAssertions = { TEXT("fixture.route_delivery_v1=true"),
					TEXT("semantic.RouteDelivery.Status.Delivered.selected=true"), TEXT("evidence.stateHashSynchronized=true"),
					TEXT("evidence.eventsSynchronized=true") };
				const FHansaSemanticNode* Delivered = SemanticRegistry->FindNode(TEXT("RouteDelivery.Status.Delivered"));
				ScreenshotContext.bStructuralAssertionsPassed = Delivered != nullptr && Delivered->State.bSelected && Fixture != nullptr;
			}
			else if (bPlacementFixtureActive)
			{
				ScreenshotContext.EvidenceSuiteId = PlacementFixture->IsIntegrated() ? TEXT("S06P04") : TEXT("S07P03");
				ScreenshotContext.FixtureId = PlacementFixture->GetFixtureId();
				ScreenshotContext.ScreenId = TEXT("BuildMode.Screen");
				ScreenshotContext.FlowId = PlacementFixture->IsIntegrated()
					? TEXT("integrated-lubeck-city-loop-v1") : TEXT("empty-lubeck-road-warehouse-v1");
				ScreenshotContext.SimulationTick = PlacementFixture->GetSimulationTick();
				ScreenshotContext.StructuralAssertions = PlacementFixture->IsIntegrated()
					? TArray<FString> {
						TEXT("fixture.loaded=true"), TEXT("semantic.BuildMode.Camera.exists=true"),
						TEXT("semantic.BuildMode.Integrated.Construction.selected=true"),
						TEXT("semantic.BuildMode.Integrated.Logistics.selected=true"),
						TEXT("semantic.BuildMode.Integrated.Production.selected=true"),
						TEXT("semantic.BuildMode.Integrated.Population.selected=true"),
						TEXT("semantic.BuildMode.Integrated.Bread.selected=true") }
					: TArray<FString> {
						TEXT("fixture.loaded=true"),
						FString::Printf(TEXT("authoritative.placedBuildingCount=%d"), PlacementFixture->GetPlacedBuildingCount()),
						TEXT("semantic.BuildMode.Camera.exists=true"),
						TEXT("semantic.BuildMode.Placement.Validation.exists=true"),
						TEXT("semantic.BuildMode.Result.Building.selected=true") };
				const FHansaSemanticNode* CameraNode = SemanticRegistry->FindNode(TEXT("BuildMode.Camera"));
				const FHansaSemanticNode* ValidationNode = SemanticRegistry->FindNode(TEXT("BuildMode.Placement.Validation"));
				const FHansaSemanticNode* ResultNode = SemanticRegistry->FindNode(TEXT("BuildMode.Result.Building"));
				const FHansaSemanticNode* IntegratedConstruction = SemanticRegistry->FindNode(TEXT("BuildMode.Integrated.Construction"));
				const FHansaSemanticNode* IntegratedLogistics = SemanticRegistry->FindNode(TEXT("BuildMode.Integrated.Logistics"));
				const FHansaSemanticNode* IntegratedProduction = SemanticRegistry->FindNode(TEXT("BuildMode.Integrated.Production"));
				const FHansaSemanticNode* IntegratedPopulation = SemanticRegistry->FindNode(TEXT("BuildMode.Integrated.Population"));
				const FHansaSemanticNode* IntegratedBread = SemanticRegistry->FindNode(TEXT("BuildMode.Integrated.Bread"));
				ScreenshotContext.bStructuralAssertionsPassed = PlacementFixture->IsIntegrated()
					? CameraNode != nullptr && IntegratedConstruction != nullptr && IntegratedConstruction->State.bSelected &&
						IntegratedLogistics != nullptr && IntegratedLogistics->State.bSelected &&
						IntegratedProduction != nullptr && IntegratedProduction->State.bSelected &&
						IntegratedPopulation != nullptr && IntegratedPopulation->State.bSelected &&
						IntegratedBread != nullptr && IntegratedBread->State.bSelected
					: PlacementFixture->GetPlacedBuildingCount() == 2 && CameraNode != nullptr && ValidationNode != nullptr &&
						ResultNode != nullptr && ResultNode->State.bSelected;
			}
			ScreenshotContext.UiRevision = SemanticRegistry->GetRevision();
			ScreenshotContext.FrameNumber = GFrameCounter;
			ScreenshotContext.SemanticSnapshotJson = SerializeResponse(MakeSemanticSnapshot(*SemanticRegistry));
			const FHansaScreenshotResult Capture = ScreenshotService->Capture(
				Size,
				ScreenshotContext,
				[this](const FIntPoint& NativeSize, TArray<FColor>& Pixels)
				{
					return bStrategicFixtureActive
						? StrategicScreenHost->CaptureNative(NativeSize, Pixels)
						: bRouteDeliveryFixtureActive
							? RouteDeliveryScreenHost->CaptureNative(NativeSize, Pixels)
						: bPlacementFixtureActive
							? PlacementScreenHost->CaptureNative(NativeSize, Pixels)
							: ProofScreenHost->CaptureNative(NativeSize, Pixels);
				});
			if (!Capture.IsSuccess())
			{
				const EHansaAutomationErrorCode Code = Capture.Error == EHansaScreenshotError::InvalidSize
					? EHansaAutomationErrorCode::InvalidCaptureSize
					: Capture.Error == EHansaScreenshotError::EvidenceWriteFailed
						? EHansaAutomationErrorCode::EvidenceWriteFailed
						: EHansaAutomationErrorCode::CaptureUnavailable;
				return MakeErrorResponse(RequestId, MakeEndpointError(
					Code,
					RequestId,
					TEXT("Native screenshot capture or evidence persistence failed."),
					TEXT("Verify Slate rendering and write access beneath the reported Saved/TestEvidence/Automation suite."),
					Code == EHansaAutomationErrorCode::CaptureUnavailable));
			}
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
			Result->SetNumberField(TEXT("width"), Capture.Size.X);
			Result->SetNumberField(TEXT("height"), Capture.Size.Y);
			Result->SetBoolField(TEXT("postCaptureResized"), false);
			Result->SetStringField(TEXT("screenshotPath"), Capture.ScreenshotPath);
			Result->SetStringField(TEXT("metadataPath"), Capture.MetadataPath);
			Result->SetStringField(TEXT("semanticSnapshotPath"), Capture.SemanticSnapshotPath);
			if (!Capture.QuerySnapshotPath.IsEmpty()) Result->SetStringField(TEXT("querySnapshotPath"), Capture.QuerySnapshotPath);
			Result->SetStringField(TEXT("contentSha1"), Capture.ContentSha1);
			Result->SetNumberField(TEXT("revision"), static_cast<double>(SemanticRegistry->GetRevision()));
			CapturedScreenshotPaths.Add(Capture.ScreenshotPath);
			CapturedScreenshotHashes.Add(Capture.ContentSha1);
			return MakeSuccessResponse(RequestId, Result);
		}

		return MakeErrorResponse(RequestId, InvalidRequest(
			RequestId,
			TEXT("The requested operation is not part of the Hansa automation wire surface."),
			TEXT("Use a listed session, fixture, gameplay-query, simulation, semantic UI, wait, health, or screenshot operation.")));
	}

	void FHansaAutomationNamedPipeEndpoint::ResetConnection()
	{
#if PLATFORM_WINDOWS
		if (PipeHandle != nullptr)
		{
			const HANDLE Handle = static_cast<HANDLE>(PipeHandle);
			if (bClientConnected)
			{
				DisconnectNamedPipe(Handle);
			}
			CloseHandle(Handle);
		}
#endif
		PipeHandle = nullptr;
		bClientConnected = false;
		ReceiveBuffer.Reset();
		WaitService->CancelAll();
	}
}
