#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
class USoundWave;
namespace Hansa::Editor::Generation
{
class FHansaAudioTake final
{
public:
    static bool ValidateProfile(const TSharedPtr<FJsonObject>& Profile, FString& Error);
    static bool ValidatePCM(const TArray<uint8>& PCM, uint32 Rate, uint16 Channels, const TSharedPtr<FJsonObject>& Profile, FString& Error);
    static bool Prepare(const TArray<UObject*>& Assets, const TSharedPtr<FJsonObject>& Profile, FString& Error);
    static bool Validate(const TArray<UObject*>& Assets, const TSharedPtr<FJsonObject>& Profile, FString& Error);
};
}
