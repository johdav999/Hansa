#include "UI/HansaFrontendPresentationModel.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#define LOCTEXT_NAMESPACE "HansaFrontend"
void UHansaFrontendPresentationModel::Initialize(bool Ready){Snapshot={};Snapshot.bReady=Ready;Changed.Broadcast();}
void UHansaFrontendPresentationModel::RefreshSlots(const TArray<FHansaSaveSlotMetadata>& Slots){
 Snapshot.bCanContinue=false;FString Latest;
 for(const auto& S:Slots)if(S.bCanLoad&&(!Snapshot.bCanContinue||S.SavedUtc>Latest)){Snapshot.bCanContinue=true;Snapshot.ContinueSlot=S.SlotId;Latest=S.SavedUtc;}
 Changed.Broadcast();
}
void UHansaFrontendPresentationModel::Begin(FName Action){Snapshot.PendingAction=Action;Snapshot.Page=EHansaFrontendPage::Loading;Snapshot.Message=Action==TEXT("NewGame")?LOCTEXT("Starting","Preparing your merchant house…"):LOCTEXT("Loading","Restoring your saved game…");Changed.Broadcast();if(Intent)Intent(Action);}
bool UHansaFrontendPresentationModel::Request(FName Action){
 if(Snapshot.Page==EHansaFrontendPage::Loading||Snapshot.Page==EHansaFrontendPage::Confirmation)return false;
 if(Action==TEXT("NewGame")){if(!Snapshot.bReady)return false;if(!Snapshot.bHasSession){Begin(Action);return true;}}
 else if(Action==TEXT("Continue")){if(!Snapshot.bCanContinue||!Snapshot.bReady)return false;Begin(Action);return true;}
 else if(Action==TEXT("Load")){if(Intent)Intent(Action);return true;}
 else if(Action==TEXT("Settings")){OpenSettings();return true;}
 else if(Action==TEXT("Credits")){ReturnPage=Snapshot.Page;Snapshot.Page=EHansaFrontendPage::Credits;Changed.Broadcast();return true;}
 else if(Action!=TEXT("Quit")&&Action!=TEXT("ReturnTitle")&&Action!=TEXT("Display"))return false;
 ConfirmationReturnPage=Snapshot.Page;Snapshot.PendingAction=Action;Snapshot.Page=EHansaFrontendPage::Confirmation;
 Snapshot.Message=Action==TEXT("Display")?LOCTEXT("KeepDisplay","Keep this display mode? It reverts automatically after 15 seconds."):Action==TEXT("Quit")?LOCTEXT("Quit","Quit Hansa? Unsaved progress will be lost."):Action==TEXT("ReturnTitle")?LOCTEXT("ReturnTitle","Return to the title screen? Unsaved progress will be lost. Your saved games remain available."):LOCTEXT("NewGame","Start a new game? Unsaved progress will be lost. Your saved games remain available.");
 Changed.Broadcast();if(Action==TEXT("Display")&&Intent)Intent(TEXT("PreviewDisplay"));return true;
}
void UHansaFrontendPresentationModel::Confirm(){if(Snapshot.Page!=EHansaFrontendPage::Confirmation)return;const FName Action=Snapshot.PendingAction;if(Action==TEXT("NewGame")){Begin(Action);return;}Snapshot.Page=ConfirmationReturnPage;Snapshot.PendingAction=NAME_None;Changed.Broadcast();if(Intent)Intent(Action);}
void UHansaFrontendPresentationModel::Back(){
 if(Snapshot.Page==EHansaFrontendPage::Loading)return;
 if(Snapshot.Page==EHansaFrontendPage::Confirmation){const bool Display=Snapshot.PendingAction==TEXT("Display");Snapshot.Page=ConfirmationReturnPage;Snapshot.PendingAction=NAME_None;if(Display&&Intent)Intent(TEXT("RevertDisplay"));}
 else if(Snapshot.Page==EHansaFrontendPage::Settings||Snapshot.Page==EHansaFrontendPage::Credits)Snapshot.Page=ReturnPage;
 else if(Snapshot.Page==EHansaFrontendPage::Error)Snapshot.Page=EHansaFrontendPage::Title;
 else return;
 Snapshot.Message=FText();Changed.Broadcast();
}
void UHansaFrontendPresentationModel::Complete(bool Success,FText Error){Snapshot.PendingAction=NAME_None;if(Success){SessionStarted();return;}Snapshot.Page=EHansaFrontendPage::Error;Snapshot.Message=FText::Format(LOCTEXT("Recovery","{0}\nChoose another compatible save or start a new game. Your save files have not been replaced."),Error);Changed.Broadcast();}
void UHansaFrontendPresentationModel::SessionStarted(){Snapshot.bHasSession=true;Snapshot.Page=EHansaFrontendPage::Hidden;Snapshot.Message=FText();Changed.Broadcast();}
void UHansaFrontendPresentationModel::OpenSettings(){ReturnPage=Snapshot.Page==EHansaFrontendPage::Hidden?EHansaFrontendPage::Hidden:EHansaFrontendPage::Title;Snapshot.Page=EHansaFrontendPage::Settings;Changed.Broadcast();}
void UHansaFrontendPresentationModel::ReturnToTitle(){Snapshot.bHasSession=false;Snapshot.Page=EHansaFrontendPage::Title;Snapshot.PendingAction=NAME_None;Changed.Broadcast();}
void UHansaFrontendPresentationModel::LoadPreferences(const FString& File){PreferenceFile=File;Snapshot.Volume=1.f;Snapshot.CameraSpeed=1.f;Snapshot.bEdgeScroll=true;FConfigFile C;C.Read(File);C.GetFloat(TEXT("Hansa.System"),TEXT("Volume"),Snapshot.Volume);C.GetFloat(TEXT("Hansa.System"),TEXT("CameraSpeed"),Snapshot.CameraSpeed);C.GetBool(TEXT("Hansa.System"),TEXT("EdgeScroll"),Snapshot.bEdgeScroll);Snapshot.Volume=FMath::Clamp(Snapshot.Volume,0.f,1.f);Snapshot.CameraSpeed=FMath::Clamp(Snapshot.CameraSpeed,.5f,2.f);Changed.Broadcast();}
void UHansaFrontendPresentationModel::SavePreferences()const{if(PreferenceFile.IsEmpty())return;FConfigFile C;C.SetString(TEXT("Hansa.System"),TEXT("Volume"),*FString::SanitizeFloat(Snapshot.Volume));C.SetString(TEXT("Hansa.System"),TEXT("CameraSpeed"),*FString::SanitizeFloat(Snapshot.CameraSpeed));C.SetBool(TEXT("Hansa.System"),TEXT("EdgeScroll"),Snapshot.bEdgeScroll);IFileManager::Get().MakeDirectory(*FPaths::GetPath(PreferenceFile),true);C.Write(PreferenceFile);}
void UHansaFrontendPresentationModel::ChangeSetting(FName S){
 if(Snapshot.Page!=EHansaFrontendPage::Settings)return;
 if(S==TEXT("VolumeDown"))Snapshot.Volume=FMath::Clamp(Snapshot.Volume-.1f,0.f,1.f);
 else if(S==TEXT("VolumeUp"))Snapshot.Volume=FMath::Clamp(Snapshot.Volume+.1f,0.f,1.f);
 else if(S==TEXT("CameraDown"))Snapshot.CameraSpeed=FMath::Clamp(Snapshot.CameraSpeed-.1f,.5f,2.f);
 else if(S==TEXT("CameraUp"))Snapshot.CameraSpeed=FMath::Clamp(Snapshot.CameraSpeed+.1f,.5f,2.f);
 else if(S==TEXT("Edge"))Snapshot.bEdgeScroll=!Snapshot.bEdgeScroll;
 else if(S==TEXT("VSync"))Snapshot.bVSync=!Snapshot.bVSync;
 else if(S==TEXT("Window")){Snapshot.bBorderless=!Snapshot.bBorderless;Changed.Broadcast();Request(TEXT("Display"));return;}
 else return;
 SavePreferences();if(Intent)Intent(TEXT("SettingsChanged"));Changed.Broadcast();
}
#undef LOCTEXT_NAMESPACE
