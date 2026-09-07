#include "Generation/HansaMediaAuditCommandlet.h"
#include "Generation/HansaStagedMedia.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

UHansaMediaAuditCommandlet::UHansaMediaAuditCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}
int32 UHansaMediaAuditCommandlet::Main(const FString& Params)
{
    TArray<FString> Errors;
    const bool Passed = Hansa::Editor::Generation::FHansaStagedMedia::AuditReferences(Errors);
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("schemaVersion"), 1);
    Result->SetBoolField(TEXT("passed"), Passed);
    Result->SetStringField(TEXT("scope"), TEXT("All /Game package hard and soft Asset Registry dependencies"));
    TArray<TSharedPtr<FJsonValue>> Issues;
    for (const FString& Error : Errors)
    {
        UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
        Issues.Add(MakeShared<FJsonValueString>(Error));
    }
    Result->SetArrayField(TEXT("errors"), Issues);
    FString Output;
    if (FParse::Value(*Params, TEXT("Report="), Output))
    {
        FString Text;
        FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Text));
        if (!FFileHelper::SaveStringToFile(Text, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return 2;
    }
    UE_LOG(LogTemp, Display, TEXT("Hansa media reference audit: %s"), Passed ? TEXT("PASS") : TEXT("FAIL"));
    return Passed ? 0 : 1;
}
