#include "World/HansaGameMode.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaStrategyPlayerController.h"

AHansaGameMode::AHansaGameMode()
{
	DefaultPawnClass = AHansaStrategyCameraPawn::StaticClass();
	PlayerControllerClass = AHansaStrategyPlayerController::StaticClass();
	bStartPlayersAsSpectators = false;
}

void AHansaGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	EnsureLubeckWorldComposition();
}

AActor* AHansaGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHansaLubeckAutomationStart> It(World); It; ++It)
		{
			return *It;
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AHansaGameMode::EnsureLubeckWorldComposition()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	AHansaLubeckWorldFoundation* Foundation = nullptr;
	for (TActorIterator<AHansaLubeckWorldFoundation> It(World); It; ++It)
	{
		Foundation = *It;
		break;
	}

	if (Foundation == nullptr)
	{
		FActorSpawnParameters Parameters;
		Parameters.Name = TEXT("LubeckWorldFoundation");
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>(
			AHansaLubeckWorldFoundation::StaticClass(), FTransform::Identity, Parameters);
	}

	bool bHasProjectionManager = false;
	for (TActorIterator<AHansaPlacementProjectionManager> It(World); It; ++It)
	{
		bHasProjectionManager = true;
		break;
	}
	if (!bHasProjectionManager)
	{
		FActorSpawnParameters ProjectionParameters;
		ProjectionParameters.Name = TEXT("LubeckPlacementProjectionManager");
		ProjectionParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<AHansaPlacementProjectionManager>(
			AHansaPlacementProjectionManager::StaticClass(), FTransform::Identity, ProjectionParameters);
	}

	for (TActorIterator<AHansaLubeckAutomationStart> It(World); It; ++It)
	{
		return;
	}

	FActorSpawnParameters StartParameters;
	StartParameters.Name = TEXT("LubeckAutomationStart");
	StartParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform StartTransform = Foundation != nullptr ? Foundation->GetAutomationStartTransform() :
		Hansa::Game::LubeckMap::AutomationStartTransform();
	World->SpawnActor<AHansaLubeckAutomationStart>(
		AHansaLubeckAutomationStart::StaticClass(), StartTransform, StartParameters);
}
