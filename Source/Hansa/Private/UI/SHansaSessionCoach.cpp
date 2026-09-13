#include "UI/SHansaSessionCoach.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#define LOCTEXT_NAMESPACE "HansaSessionCoach"
namespace Hansa::UI {
void SHansaSessionCoach::Construct(const FArguments& Args){
 OnDismissed=Args._OnDismissed;Model=Args._Model;const auto Prefs=Args._Preferences;
 PanelBrush=GetComponentStyle(EUiSurface::Panel,EUiState::Default,Prefs).Brush;
 HeaderBrush=GetComponentStyle(EUiSurface::TopBar,EUiState::Default,Prefs).Brush;
 ChildSlot[SNew(SBox).WidthOverride(400).HeightOverride(Prefs.bLargeText?280:248)
 [SNew(SBorder).BorderImage(&PanelBrush).Padding(12)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&HeaderBrush).Padding(8)[SAssignNew(Title,STextBlock).Font(GetComponentFont(EHansaUiTypographyToken::Heading2,Prefs)).ColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk)).AutoWrapText(true)]]
  +SVerticalBox::Slot().FillHeight(1).Padding(0,8)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Body,STextBlock).Font(GetComponentFont(EHansaUiTypographyToken::Body,Prefs)).ColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink)).AutoWrapText(true)]]
  +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Dismiss,SHansaAction).Preferences(Prefs).Label(LOCTEXT("Dismiss","Dismiss")).OnClicked_Lambda([this]{ActivateSemanticId(TEXT("Session.Help.Dismiss"));return FReply::Handled();})]
   +SHorizontalBox::Slot().FillWidth(1).Padding(8,0,0,0)[SAssignNew(Hide,SHansaAction).Preferences(Prefs).Kind(EHansaUiButtonStyle::Secondary).Label(LOCTEXT("Hide","Hide tips")).OnClicked_Lambda([this]{ActivateSemanticId(TEXT("Session.Help.Hide"));return FReply::Handled();})]]]]];
 if(auto* P=Model.Get()){Changed=P->OnChanged().AddSP(SharedThis(this),&SHansaSessionCoach::Refresh);Refresh(P->GetSnapshot(),P->GetRevision());}
}
SHansaSessionCoach::~SHansaSessionCoach(){if(auto* P=Model.Get())P->OnChanged().Remove(Changed);}
bool SHansaSessionCoach::IsOffered()const{return Model.IsValid()&&Model->GetSnapshot().bCoachVisible&&!Model->GetSnapshot().bOpen;}
void SHansaSessionCoach::Refresh(const FHansaScenarioPresentationSnapshot& S,uint64){SetVisibility(IsOffered()?EVisibility::Visible:EVisibility::Collapsed);Title->SetText(S.HelpTitle);Body->SetText(S.HelpBody);}
bool SHansaSessionCoach::ActivateSemanticId(const FString& Id){if(!IsOffered())return false;auto* P=Model.Get();if(Id==TEXT("Session.Help.Dismiss")){P->DismissHelp();OnDismissed.ExecuteIfBound();return true;}if(Id==TEXT("Session.Help.Hide")){P->ToggleHelp();OnDismissed.ExecuteIfBound();return true;}return false;}
TSharedPtr<SWidget> SHansaSessionCoach::ResolveSemanticWidget(const FString& Id)const{if(Id==TEXT("Session.Help.Dismiss"))return Dismiss;if(Id==TEXT("Session.Help.Hide"))return Hide;return nullptr;}
bool SHansaSessionCoach::FocusSemanticId(const FString& Id){if(!IsOffered())return false;const auto W=ResolveSemanticWidget(Id);if(!W)return false;FSlateApplication::Get().SetKeyboardFocus(W,EFocusCause::Navigation);return true;}
TArray<FHansaHudSemanticNode> SHansaSessionCoach::GetSemanticSnapshot()const{
 TArray<FHansaHudSemanticNode> R;if(!Model.IsValid())return R;const auto& S=Model->GetSnapshot();
 for(const FString Id:{FString(TEXT("Session.Help")),FString(TEXT("Session.Help.Dismiss")),FString(TEXT("Session.Help.Hide"))}){
  FHansaHudSemanticNode N;N.Id=Id;N.ParentId=TEXT("HUD.Root");N.Label=Id==TEXT("Session.Help")?S.HelpTitle.ToString():Id.EndsWith(TEXT("Dismiss"))?LOCTEXT("Dismiss","Dismiss").ToString():LOCTEXT("Hide","Hide tips").ToString();N.Role=Id==TEXT("Session.Help")?EHansaHudSemanticRole::Panel:EHansaHudSemanticRole::Button;
  N.State.bVisible=IsOffered();N.State.bEnabled=IsOffered();N.bCanActivate=N.bCanFocus=Id!=TEXT("Session.Help")&&IsOffered();N.State.ValueType=TEXT("contextual-help");N.State.Value=S.HelpBody.ToString();
  const auto W=ResolveSemanticWidget(Id);const auto G=W?W->GetCachedGeometry():GetCachedGeometry();const auto A=G.GetAbsolutePosition(),B=A+G.GetAbsoluteSize();if(N.State.bVisible)N.Bounds=FIntRect(FMath::RoundToInt(A.X),FMath::RoundToInt(A.Y),FMath::RoundToInt(B.X),FMath::RoundToInt(B.Y));N.State.bFocused=W&&W->HasKeyboardFocus();R.Add(MoveTemp(N));
 }return R;
}
}
#undef LOCTEXT_NAMESPACE
