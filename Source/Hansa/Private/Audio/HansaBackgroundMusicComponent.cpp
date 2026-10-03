#include "Audio/HansaBackgroundMusicComponent.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HansaLog.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

void UHansaBackgroundMusicComponent::StartMusic()
{
	if (bRunning) return;
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller || !Controller->IsLocalController() || !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) return;

	// These imported SoundWaves live in a cook-always directory. Source WAVs are not read at runtime.
	static const TCHAR* TrackPaths[] = {
		TEXT("/Game/Hansa/Audio/Music/SW_HansasHarbor1.SW_HansasHarbor1"),
		TEXT("/Game/Hansa/Audio/Music/SW_HansasHarbor2.SW_HansasHarbor2")
	};
	for (const TCHAR* Path : TrackPaths)
	{
		if (USoundBase* Track = LoadObject<USoundBase>(nullptr, Path)) Tracks.Add(Track);
		else UE_LOG(LogHansa, Warning, TEXT("Background music asset missing: %s"), Path);
	}
	if (Tracks.IsEmpty()) return;

	MusicAudio = NewObject<UAudioComponent>(GetOwner(), TEXT("HansaBackgroundMusicAudio"));
	MusicAudio->bAutoActivate = false;
	MusicAudio->bIsUISound = true;
	MusicAudio->bAllowSpatialization = false;
	MusicAudio->OnAudioFinished.AddDynamic(this, &UHansaBackgroundMusicComponent::OnTrackFinished);
	MusicAudio->RegisterComponent();
	bRunning = true;
	PlayNextTrack();
}

void UHansaBackgroundMusicComponent::StopMusic()
{
	bRunning = false;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(NextTrackTimer);
	if (MusicAudio)
	{
		MusicAudio->OnAudioFinished.RemoveDynamic(this, &UHansaBackgroundMusicComponent::OnTrackFinished);
		MusicAudio->Stop();
		MusicAudio->DestroyComponent();
		MusicAudio = nullptr;
	}
	Tracks.Reset();
	PreviousTrackIndex = INDEX_NONE;
}

void UHansaBackgroundMusicComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopMusic();
	Super::EndPlay(EndPlayReason);
}

void UHansaBackgroundMusicComponent::OnTrackFinished()
{
	if (!bRunning || !GetWorld()) return;
	// About one in four transitions is quiet for one to two real minutes.
	if (FMath::FRand() < 0.25f)
	{
		GetWorld()->GetTimerManager().SetTimer(NextTrackTimer, this,
			&UHansaBackgroundMusicComponent::PlayNextTrack, FMath::FRandRange(60.0f, 120.0f), false);
	}
	else PlayNextTrack();
}

void UHansaBackgroundMusicComponent::PlayNextTrack()
{
	if (!bRunning || !MusicAudio || Tracks.IsEmpty()) return;
	int32 Index = FMath::RandRange(0, Tracks.Num() - 1);
	if (Tracks.Num() > 1 && Index == PreviousTrackIndex)
	{
		Index = (Index + FMath::RandRange(1, Tracks.Num() - 1)) % Tracks.Num();
	}
	PreviousTrackIndex = Index;
	MusicAudio->SetSound(Tracks[Index]);
	MusicAudio->Play();
}
