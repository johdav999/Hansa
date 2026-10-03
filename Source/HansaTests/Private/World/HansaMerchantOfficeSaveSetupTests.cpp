#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"
#include "World/HansaRuntimeSimulationHost.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMerchantOfficeNewGame,
 "Hansa.Integration.TradePresence.MerchantOfficeTestStartup",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaMerchantOfficeNewGame::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 bool bEnabled=false;
 if(GConfig)GConfig->GetBool(TEXT("Hansa.MerchantOfficeTest"),TEXT("bEnableAtNewGame"),bEnabled,GGameIni);
 if(!bEnabled){AddInfo(TEXT("Merchant Office startup fixture disabled in DefaultGame.ini."));return true;}
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
 FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Host->StartNewGame(Error)){AddError(Error);return false;}
 const auto Projection=Host->BuildProjection();
 const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
 const auto* Presence=Projection?Projection.Value.GetForeignPresences().FindByPredicate([&](const auto& P){return P.CityId==City&&P.HouseId==Host->GetHouseId();}):nullptr;
 const auto* Station=Projection&&Presence?Projection.Value.GetTradeStations().FindByPredicate([&](const auto& S){return S.Station.Id==Presence->StationId;}):nullptr;
 if(!TestNotNull(TEXT("New Game grants an active placed Rostock trade station"),Station)||
  !TestNotNull(TEXT("New Game shows Rostock presence"),Presence))return false;
 TestEqual(TEXT("Office remains to be earned through player action"),Presence->CurrentStageId,FString(TEXT("PresenceStage.TradeStation")));
 TestEqual(TEXT("Station is operational"),Station->Station.Status,EHansaTradeStationStatus::Active);
 TestTrue(TEXT("Office requirements and funding are available"),Presence->NextStages.ContainsByPredicate([](const auto& S){return S.StageId==TEXT("PresenceStage.MerchantOffice")&&S.bProgressRequirementsMet&&S.bFundingAvailable;}));
 return !HasAnyErrors();
}

// Manual one-shot tool. Ordinary automated suites never alter a player's saves.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMerchantOfficePrepareSaves,
 "Hansa.Development.MerchantOffice.PrepareNamedSaves",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaMerchantOfficePrepareSaves::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 if(!FParse::Param(FCommandLine::Get(),TEXT("HansaPrepareMerchantOfficeSaves")))
 {AddInfo(TEXT("Skipped; pass -HansaPrepareMerchantOfficeSaves for the explicit one-shot save update."));return true;}
 const FString Directory=FPaths::ProjectSavedDir()/TEXT("SaveGames/Hansa");
 TArray<FString> Files;IFileManager::Get().FindFiles(Files,*(Directory/TEXT("*.hansa")),true,false);
 struct FPrepared {FString Name;FString Label;TArray<uint8> Bytes;};
 TArray<FPrepared> Prepared;int32 Test6Count=0,Test7Count=0,AutosaveCount=0;
 for(const FString& File:Files)
 {
  TArray<uint8> Original;if(!FFileHelper::LoadFileToArray(Original,*(Directory/File)))continue;
  FHansaSaveMetadata Metadata;
  const auto MetadataResult=FHansaSaveEnvelope::InspectMetadata(Original,Metadata);
  if(!MetadataResult){AddError(FString::Printf(TEXT("Cannot inspect %s: %s"),*File,*MetadataResult.Message));return false;}
  const FString Label=Metadata.DisplayName.TrimStartAndEnd();
  const bool bAutosave=File.Equals(TEXT("autosave.hansa"),ESearchCase::IgnoreCase);
  const bool bTest6=Label.Equals(TEXT("Test6"),ESearchCase::IgnoreCase)||Label.Equals(TEXT("Test 6"),ESearchCase::IgnoreCase);
  const bool bTest7=Label.Equals(TEXT("Test7"),ESearchCase::IgnoreCase)||Label.Equals(TEXT("Test 7"),ESearchCase::IgnoreCase);
  if(!bAutosave&&!bTest6&&!bTest7)continue;
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
  FString Error;
  if(!Host->InitializeForLubeck(nullptr,Error,EHansaRuntimeScenario::LubeckGrainShortage,0,true)){AddError(Error);return false;}
  const auto Restored=Host->RestoreSaveBytes(Original);
  if(!Restored){AddError(FString::Printf(TEXT("Could not load %s: %s"),*File,*Restored.Message));return false;}
  if(!Host->ApplyMerchantOfficeTestSetup(Error)){AddError(FString::Printf(TEXT("Could not prepare %s: %s"),*File,*Error));return false;}
  FPrepared Entry;Entry.Name=File;Entry.Label=Label;
  const auto Captured=Host->CaptureSaveBytes(Entry.Bytes,Label,FDateTime::UtcNow().ToIso8601());
  if(!Captured){AddError(FString::Printf(TEXT("Could not encode %s: %s"),*File,*Captured.Message));return false;}
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Check(NewObject<UHansaRuntimeSimulationHost>());
  if(!Check->InitializeForLubeck(nullptr,Error,EHansaRuntimeScenario::LubeckGrainShortage,0,true)||!Check->RestoreSaveBytes(Entry.Bytes))
  {AddError(FString::Printf(TEXT("Prepared save cannot reload: %s %s"),*File,*Error));return false;}
  auto Projection=Check->BuildProjection();
  const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
  const auto* Presence=Projection?Projection.Value.GetForeignPresences().FindByPredicate([&](const auto& P){return P.CityId==City&&P.HouseId==Check->GetHouseId();}):nullptr;
  const auto* Station=Projection&&Presence?Projection.Value.GetTradeStations().FindByPredicate([&](const auto& S){return S.Station.Id==Presence->StationId;}):nullptr;
  if(!Presence||Presence->CurrentStageId!=TEXT("PresenceStage.TradeStation")||!Station||
   Station->Station.Status!=EHansaTradeStationStatus::Active||!Station->Station.ConstructionSite.bLocalDelivery)
  {AddError(FString::Printf(TEXT("Prepared %s lacks an active placed Rostock station."),*File));return false;}
  if(!Check->RequestPresenceUpgrade({City,TEXT("PresenceStage.MerchantOffice")}))
  {AddError(FString::Printf(TEXT("Upgrade request rejected in %s."),*File));return false;}
  if(!Check->FundPresenceUpgrade({City,TEXT("PresenceStage.MerchantOffice"),Station->Station.InventoryId}))
  {AddError(FString::Printf(TEXT("Upgrade funding rejected in %s."),*File));return false;}
  if(!Check->AdvanceTicks(4)){AddError(FString::Printf(TEXT("Construction did not advance in %s."),*File));return false;}
  Projection=Check->BuildProjection();
  Presence=Projection?Projection.Value.GetForeignPresences().FindByPredicate([&](const auto& P){return P.CityId==City&&P.HouseId==Check->GetHouseId();}):nullptr;
  if(!Presence||Presence->CurrentStageId!=TEXT("PresenceStage.MerchantOffice")||Presence->Specializations.Num()<3)
  {AddError(FString::Printf(TEXT("Office or specialization choices did not complete in %s."),*File));return false;}
  Prepared.Add(MoveTemp(Entry));
  if(bAutosave)++AutosaveCount;if(bTest6)++Test6Count;if(bTest7)++Test7Count;
 }
 if(AutosaveCount!=1||Test6Count<1||Test7Count<1)
 {AddError(FString::Printf(TEXT("Expected autosave, Test6, Test7; found %d, %d, %d."),AutosaveCount,Test6Count,Test7Count));return false;}
 // Originals were copied and SHA256 checked under Backups/MerchantOffice-2026-10-01 before this test runs.
 for(const auto& Entry:Prepared)
 {
  const FString Path=Directory/Entry.Name,Temp=Path+TEXT(".merchant-office.tmp");
  if(!FFileHelper::SaveArrayToFile(Entry.Bytes,*Temp)||!IFileManager::Get().Move(*Path,*Temp,true,true,false,true))
  {AddError(FString::Printf(TEXT("Atomic save replacement failed: %s"),*Entry.Name));return false;}
  AddInfo(FString::Printf(TEXT("Prepared Merchant Office test save: %s [%s]"),*Entry.Name,*Entry.Label));
 }
 return true;
}
#endif
