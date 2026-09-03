#pragma once

#include "Containers/Array.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "Misc/Optional.h"
#include "Templates/UniquePtr.h"

namespace Hansa::Automation
{
	class FHansaAutomationSessionService;
	class FHansaSemanticUiRegistry;
	class FHansaAutomationWaitService;
	class FHansaNativeScreenshotService;
	class FHansaAutomationProofScreenHost;
	class FHansaProductionFixtureService;
	class FHansaPlacementAutomationFixture;
	class FHansaPlacementAutomationScreenHost;

	/**
	 * Explicitly enabled, single-controller Windows named-pipe endpoint.
	 * It is polled only while automation is enabled and owns no gameplay state.
	 */
	class FHansaAutomationNamedPipeEndpoint final
	{
	public:
		FHansaAutomationNamedPipeEndpoint(FString InPipeName, FHansaAutomationSessionService& InSessionService);
		~FHansaAutomationNamedPipeEndpoint();

		bool Start();
		void Stop();
		[[nodiscard]] bool IsRunning() const { return PipeHandle != nullptr; }

	private:
		bool Tick(float DeltaTime);
		bool CreateServerPipe();
		bool AcceptClient();
		bool PumpClient();
		bool WriteResponse(const FString& ResponseJson);
		TOptional<FString> Dispatch(const FString& RequestJson);
		void ResetConnection();

		FString PipeName;
		FHansaAutomationSessionService& SessionService;
		TUniquePtr<FHansaSemanticUiRegistry> SemanticRegistry;
		TUniquePtr<FHansaAutomationWaitService> WaitService;
		TUniquePtr<FHansaNativeScreenshotService> ScreenshotService;
		TUniquePtr<FHansaAutomationProofScreenHost> ProofScreenHost;
		TUniquePtr<FHansaProductionFixtureService> ProductionFixtureService;
		TUniquePtr<FHansaPlacementAutomationFixture> PlacementFixture;
		TUniquePtr<FHansaPlacementAutomationScreenHost> PlacementScreenHost;
		bool bPlacementFixtureActive = false;
		void* PipeHandle = nullptr;
		bool bClientConnected = false;
		TArray<uint8> ReceiveBuffer;
		FTSTicker::FDelegateHandle TickHandle;
	};
}
