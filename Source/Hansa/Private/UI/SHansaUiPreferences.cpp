#include "UI/SHansaUiPreferences.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HansaUiPreferences"
namespace Hansa::UI {
void SHansaUiPreferences::Construct(const FArguments& Args){
 Preferences=Args._Preferences;Changed=Args._OnChanged;
 HeadingStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2,false);
 HeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading2,Preferences));
 BodyStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body,false);
 BodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
 auto Action=[&](const TCHAR* Id,FText Label,int32 Setting,bool Enabled=true){
  auto Button=SNew(SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).Compact(true)
   .Label(Label).State(Enabled?EUiState::Default:EUiState::Disabled).OnClicked(this,&SHansaUiPreferences::Change,Setting);
  Controls.Add(Id,Button);return Button;
 };
 auto ToggleLabel=[](FText Name,bool On){return FText::Format(LOCTEXT("Toggle","{0}: {1}"),Name,On?LOCTEXT("On","On"):LOCTEXT("Off","Off"));};
 ChildSlot[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(STextBlock).Text(LOCTEXT("Interface","Interface")).TextStyle(&HeadingStyle)]
  +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1)[Action(TEXT("SaveLoad.Preferences.Contrast"),ToggleLabel(LOCTEXT("Contrast","High contrast"),Preferences.bHighContrast),0)]
   +SHorizontalBox::Slot().FillWidth(1).Padding(8,0)[Action(TEXT("SaveLoad.Preferences.Text"),ToggleLabel(LOCTEXT("Text","Large text"),Preferences.bLargeText),1)]
   +SHorizontalBox::Slot().FillWidth(1)[Action(TEXT("SaveLoad.Preferences.Motion"),ToggleLabel(LOCTEXT("Motion","Reduced motion"),Preferences.bReducedMotion),2)]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,16,0)[SNew(STextBlock).Text(LOCTEXT("Scale","UI scale")).TextStyle(&BodyStyle)]
   +SHorizontalBox::Slot().AutoWidth()[Action(TEXT("SaveLoad.Preferences.ScaleDown"),LOCTEXT("Smaller","−"),3,Preferences.UiScale>.8001f)]
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16,0)[SNew(STextBlock).Text(FText::AsPercent(Preferences.UiScale)).TextStyle(&BodyStyle)]
   +SHorizontalBox::Slot().AutoWidth()[Action(TEXT("SaveLoad.Preferences.ScaleUp"),LOCTEXT("Larger","+"),4,Preferences.UiScale<1.3999f)]
   +SHorizontalBox::Slot().AutoWidth().Padding(16,0)[Action(TEXT("SaveLoad.Preferences.Reset"),LOCTEXT("Reset","Reset"),5)]]];
}
bool SHansaUiPreferences::ActivateControl(const FString& Id){
 const auto* Control=Controls.Find(Id);if(!Control||!(*Control)->IsEnabled())return false;
 const TCHAR* Ids[]={TEXT("SaveLoad.Preferences.Contrast"),TEXT("SaveLoad.Preferences.Text"),TEXT("SaveLoad.Preferences.Motion"),TEXT("SaveLoad.Preferences.ScaleDown"),TEXT("SaveLoad.Preferences.ScaleUp"),TEXT("SaveLoad.Preferences.Reset")};
 for(int32 I=0;I<6;++I)if(Id==Ids[I])return Change(I).IsEventHandled();return false;
}
FReply SHansaUiPreferences::Change(int32 Setting){
 switch(Setting){
  case 0:Preferences.bHighContrast=!Preferences.bHighContrast;break;
  case 1:Preferences.bLargeText=!Preferences.bLargeText;break;
  case 2:Preferences.bReducedMotion=!Preferences.bReducedMotion;break;
  case 3:Preferences.UiScale=FMath::Clamp(Preferences.UiScale-.1f,.8f,1.4f);break;
  case 4:Preferences.UiScale=FMath::Clamp(Preferences.UiScale+.1f,.8f,1.4f);break;
  default:Preferences={};break;
 }
 Changed.ExecuteIfBound(Preferences);return FReply::Handled();
}
}
#undef LOCTEXT_NAMESPACE
