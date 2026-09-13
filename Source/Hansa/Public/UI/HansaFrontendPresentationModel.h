#pragma once
#include "CoreMinimal.h"
#include "Save/HansaSaveSubsystem.h"
#include "UObject/Object.h"
#include "HansaFrontendPresentationModel.generated.h"
UENUM()
enum class EHansaFrontendPage:uint8 { Hidden, Title, Settings, Credits, Confirmation, Loading, Error };
struct HANSA_API FHansaFrontendSnapshot {
 EHansaFrontendPage Page=EHansaFrontendPage::Title;
 bool bHasSession=false,bReady=false,bCanContinue=false;
 EHansaSaveSlotId ContinueSlot=EHansaSaveSlotId::Manual;
 FText Message;
 FName PendingAction;
 float Volume=1.f,CameraSpeed=1.f;
 bool bEdgeScroll=true,bVSync=false,bBorderless=false;
};
/** Player-facing session intents. No fixture selector, console or provider access. */
UCLASS()
class HANSA_API UHansaFrontendPresentationModel final:public UObject {
 GENERATED_BODY()
public:
 void Initialize(bool bReady);
 void RefreshSlots(const TArray<FHansaSaveSlotMetadata>& Slots);
 void SetIntent(TFunction<void(FName)> In){Intent=MoveTemp(In);}
 bool Request(FName Action);
 void Confirm();void Back();void Complete(bool Success,FText Error=FText());
 void SessionStarted();void OpenSettings();void ReturnToTitle();
 void LoadPreferences(const FString& File);void SavePreferences()const;
 void ChangeSetting(FName Setting);
 void SetDisplayState(bool VSync,bool Borderless){Snapshot.bVSync=VSync;Snapshot.bBorderless=Borderless;}
 const FHansaFrontendSnapshot& GetSnapshot()const{return Snapshot;}
 FSimpleMulticastDelegate& OnChanged(){return Changed;}
private:
 void Begin(FName Action);
 FHansaFrontendSnapshot Snapshot;
 EHansaFrontendPage ReturnPage=EHansaFrontendPage::Title,ConfirmationReturnPage=EHansaFrontendPage::Title;
 FString PreferenceFile;
 TFunction<void(FName)> Intent;
 FSimpleMulticastDelegate Changed;
};
