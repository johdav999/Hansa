#include "Generation/HansaAudioTake.h"
#include "Sound/SoundWave.h"
namespace Hansa::Editor::Generation
{
namespace
{
bool Fail(FString& E, const TCHAR* M) { E = M; return false; }
FString Text(const TSharedPtr<FJsonObject>& P, const TCHAR* K) { FString S; if (P) P->TryGetStringField(K, S); return S; }
double Number(const TSharedPtr<FJsonObject>& P, const TCHAR* K) { double N = -1; if (P) P->TryGetNumberField(K, N); return N; }
bool Stable(const FString& S, const FString& Prefix)
{
    if (!S.StartsWith(Prefix + TEXT(".")) || S.Len() > 100) return false;
    TArray<FString> Parts; S.ParseIntoArray(Parts, TEXT("."), false);
    for (const FString& Part : Parts)
    {
        if (Part.IsEmpty()) return false;
        for (TCHAR C : Part) if (!((C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '_')) return false;
    }
    return true;
}
}
bool FHansaAudioTake::ValidateProfile(const TSharedPtr<FJsonObject>& P, FString& E)
{
    bool Loop = true;
    const bool Speech = Text(P, TEXT("kind")) == TEXT("Speech");
    if (!P || P->Values.Num() != 12 || Number(P, TEXT("version")) != 1 ||
        (!Speech && Text(P, TEXT("kind")) != TEXT("SFX")) || !P->TryGetBoolField(TEXT("loop"), Loop) || Loop ||
        !Stable(Text(P, TEXT("stableId")), Speech ? TEXT("Dialogue") : TEXT("SFX")))
        return Fail(E, TEXT("AudioTake v1 requires one non-looping SFX or speech identity."));
    const TSet<FString> Keys{TEXT("version"), TEXT("kind"), TEXT("stableId"), TEXT("variants"),
        TEXT("minimumDurationMs"), TEXT("maximumDurationMs"), TEXT("maximumLeadingSilenceMs"),
        TEXT("maximumTrailingSilenceMs"), TEXT("subtitle"), TEXT("speakerId"), TEXT("language"), TEXT("loop")};
    for (const FString& Key : Keys)
        if (!P->HasField(Key)) return Fail(E, TEXT("Unknown AudioTake profile field."));
    for (const auto& Limit : TArray<TTuple<FString, double, double>>{
        {TEXT("variants"),1,2}, {TEXT("minimumDurationMs"),100,15000}, {TEXT("maximumDurationMs"),100,15000},
        {TEXT("maximumLeadingSilenceMs"),0,500}, {TEXT("maximumTrailingSilenceMs"),0,1000}})
    {
        const double N = Number(P, *Limit.Get<0>());
        if (!FMath::IsFinite(N) || N < Limit.Get<1>() || N > Limit.Get<2>() || N != FMath::FloorToDouble(N))
            return Fail(E, TEXT("AudioTake exceeds a canonical duration, silence or variant limit."));
    }
    if (Number(P, TEXT("minimumDurationMs")) > Number(P, TEXT("maximumDurationMs")))
        return Fail(E, TEXT("Invalid audio duration interval."));
    const FString Subtitle = Text(P, TEXT("subtitle"));
    if (Speech)
    {
        if (Text(P, TEXT("language")) != TEXT("en") || Subtitle.TrimStartAndEnd().IsEmpty() || Subtitle.Len() > 300 ||
            Subtitle.Contains(TEXT("\n")) || Subtitle.Contains(TEXT("\r")) || Subtitle.Contains(TEXT("[")) || Subtitle.Contains(TEXT("]")) ||
            !Stable(Text(P, TEXT("speakerId")), TEXT("Speaker")))
            return Fail(E, TEXT("Speech requires a short English subtitle and stable speaker identity."));
    }
    else if (!Subtitle.IsEmpty() || !Text(P, TEXT("speakerId")).IsEmpty() || !Text(P, TEXT("language")).IsEmpty())
        return Fail(E, TEXT("SFX cannot carry a speaker or subtitle."));
    return true;
}
bool FHansaAudioTake::ValidatePCM(const TArray<uint8>& PCM, uint32 Rate, uint16 Channels, const TSharedPtr<FJsonObject>& P, FString& E)
{
    if (!ValidateProfile(P, E)) return false;
    if (PCM.IsEmpty() || PCM.Num() > 4194304 || (Channels != 1 && Channels != 2) ||
        (Text(P, TEXT("kind")) == TEXT("Speech") && Channels != 1) ||
        (Rate != 24000 && Rate != 44100 && Rate != 48000) || PCM.Num() % (Channels * 2))
        return Fail(E, TEXT("Audio must decode to bounded PCM16 with approved channels and sample rate."));
    const int32 Frames = PCM.Num() / (Channels * 2);
    const double Duration = Frames * 1000.0 / Rate;
    if (Duration < Number(P, TEXT("minimumDurationMs")) || Duration > Number(P, TEXT("maximumDurationMs")))
        return Fail(E, TEXT("Decoded audio duration exceeds the approved range."));
    int32 First = INDEX_NONE, Last = INDEX_NONE;
    for (int32 Frame = 0; Frame < Frames; ++Frame)
    {
        bool Active = false;
        for (int32 Channel = 0; Channel < Channels; ++Channel)
        {
            const int32 Offset = (Frame * Channels + Channel) * 2;
            const int32 Value = FMath::Abs(int32(int16(uint16(PCM[Offset]) | (uint16(PCM[Offset + 1]) << 8))));
            if (Value >= 32760) return Fail(E, TEXT("Audio contains clipped PCM samples."));
            Active |= Value > 104;
        }
        if (Active) { if (First == INDEX_NONE) First = Frame; Last = Frame; }
    }
    if (First == INDEX_NONE) return Fail(E, TEXT("Audio is silent below the -50 dBFS activity threshold."));
    if (First * 1000.0 / Rate > Number(P, TEXT("maximumLeadingSilenceMs")) ||
        (Frames - Last - 1) * 1000.0 / Rate > Number(P, TEXT("maximumTrailingSilenceMs")))
        return Fail(E, TEXT("Audio has excessive leading or trailing silence."));
    return true;
}
bool FHansaAudioTake::Prepare(const TArray<UObject*>& Assets, const TSharedPtr<FJsonObject>& P, FString& E)
{
    if (!ValidateProfile(P, E) || Assets.Num() != 1) return Fail(E, TEXT("Stage one selected audio take at a time."));
    USoundWave* Sound = Cast<USoundWave>(Assets[0]);
    if (!Sound) return Fail(E, TEXT("AudioTake requires one native SoundWave."));
    const bool Speech = Text(P, TEXT("kind")) == TEXT("Speech");
    Sound->bLooping = false;
    Sound->SoundGroup = Speech ? SOUNDGROUP_Voice : SOUNDGROUP_Effects;
    Sound->SetSoundAssetCompressionType(ESoundAssetCompressionType::PCM);
    Sound->Subtitles.Reset();
    if (Speech)
    {
        FSubtitleCue Cue; Cue.Time = 0;
        Cue.Text = FText::ChangeKey(TEXT("Hansa.Dialogue"), Text(P, TEXT("stableId")), FText::FromString(Text(P, TEXT("subtitle"))));
        Sound->Subtitles.Add(Cue);
    }
    Sound->MarkPackageDirty();
    return Validate(Assets, P, E);
}
bool FHansaAudioTake::Validate(const TArray<UObject*>& Assets, const TSharedPtr<FJsonObject>& P, FString& E)
{
    if (!ValidateProfile(P, E) || Assets.Num() != 1) return Fail(E, TEXT("AudioTake requires one selected SoundWave."));
    const USoundWave* Sound = Cast<USoundWave>(Assets[0]);
    const bool Speech = Text(P, TEXT("kind")) == TEXT("Speech");
    if (!Sound || Sound->bLooping || Sound->SoundGroup != (Speech ? SOUNDGROUP_Voice : SOUNDGROUP_Effects) ||
        Sound->GetSoundAssetCompressionType() != ESoundAssetCompressionType::PCM ||
        (Speech ? Sound->Subtitles.Num() != 1 : !Sound->Subtitles.IsEmpty()))
        return Fail(E, TEXT("Audio category, PCM compression, loop or subtitle count changed."));
    if (Speech && (Sound->Subtitles[0].Time != 0 || Sound->Subtitles[0].Text.ToString() != Text(P, TEXT("subtitle"))))
        return Fail(E, TEXT("Speech subtitle differs from the approved source line."));
    if (Speech)
    {
        const auto Key = FTextInspector::GetKey(Sound->Subtitles[0].Text);
        const auto Namespace = FTextInspector::GetNamespace(Sound->Subtitles[0].Text);
        if (!Key.IsSet() || Key.GetValue() != Text(P, TEXT("stableId")) || !Namespace.IsSet() || Namespace.GetValue() != TEXT("Hansa.Dialogue"))
            return Fail(E, TEXT("Speech subtitle lost its stable line identity."));
    }
    TArray<uint8> PCM; uint32 Rate = 0; uint16 Channels = 0;
    if (!Sound->GetImportedSoundWaveData(PCM, Rate, Channels))
        return Fail(E, TEXT("Native SoundWave source did not decode."));
    if (Sound->NumChannels != Channels || Sound->GetImportedSampleRate() != Rate)
        return Fail(E, TEXT("Native audio properties disagree with decoded source."));
    return ValidatePCM(PCM, Rate, Channels, P, E);
}
}
