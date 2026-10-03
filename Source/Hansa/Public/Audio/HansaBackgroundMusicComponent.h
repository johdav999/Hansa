#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "HansaBackgroundMusicComponent.generated.h"

class UAudioComponent;
class USoundBase;

/** Local, non-spatial soundtrack player. It never affects authoritative game state. */
UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class HANSA_API UHansaBackgroundMusicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void StartMusic();
	void StopMusic();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnTrackFinished();
	void PlayNextTrack();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicAudio;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> Tracks;

	FTimerHandle NextTrackTimer;
	int32 PreviousTrackIndex = INDEX_NONE;
	bool bRunning = false;
};
