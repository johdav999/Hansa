#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "HansaGameMode.generated.h"

/** MVP gameplay mode that guarantees the deterministic Lübeck foundation exists in an otherwise blank map. */
UCLASS()
class HANSA_API AHansaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHansaGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

private:
	void EnsureLubeckWorldComposition();
};
