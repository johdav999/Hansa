#include "UI/SHansaFrontend.h"
#include "UI/HansaUiNavigation.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#define LOCTEXT_NAMESPACE "HansaFrontendScreen"
namespace Hansa::UI {
void SHansaFrontend::Construct(const FArguments& A){
 OnClosed=A._OnClosed;Model=A._Model;Preferences=A._Preferences;PreferencesChanged=A._OnPreferencesChanged;
 Background=GetComponentStyle(EUiSurface::TopBar,EUiState::Default,Preferences).Brush;Panel=GetComponentStyle(EUiSurface::Modal,EUiState::Default,Preferences).Brush;Header=Background;
 ChildSlot[SNew(SBorder).BorderImage(&Background).Padding(16).HAlign(HAlign_Center).VAlign(VAlign_Center)
  .OnMouseButtonDown_Lambda([](const FGeometry&,const FPointerEvent&){return FReply::Handled();})
  [SAssignNew(PanelSize,SBox)[SNew(SBorder).BorderImage(&Panel).Padding(16)
   [SAssignNew(Scroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(Rows,SVerticalBox)]]]]];
 if(auto* M=Model.Get()){Changed=M->OnChanged().AddSP(SharedThis(this),&SHansaFrontend::ScheduleRefresh);Refresh();}else SetVisibility(EVisibility::Collapsed);
}
SHansaFrontend::~SHansaFrontend(){if(auto* M=Model.Get())M->OnChanged().Remove(Changed);}
bool SHansaFrontend::IsOpen()const{return Model.IsValid()&&Model->GetSnapshot().Page!=EHansaFrontendPage::Hidden;}
void SHansaFrontend::SetPresentationSize(FIntPoint S){
 Available=S;const auto Page=Model.IsValid()?Model->GetSnapshot().Page:EHansaFrontendPage::Title;
 const float Width=Page==EHansaFrontendPage::Title?640.f:Page==EHansaFrontendPage::Confirmation?720.f:Page==EHansaFrontendPage::Loading?640.f:Page==EHansaFrontendPage::Credits?800.f:960.f;
 const float Height=Page==EHansaFrontendPage::Title?660.f:Page==EHansaFrontendPage::Confirmation?(Preferences.bLargeText?380.f:320.f):Page==EHansaFrontendPage::Loading?220.f:Page==EHansaFrontendPage::Error?420.f:Page==EHansaFrontendPage::Credits?540.f:700.f;
 PanelSize->SetWidthOverride(FMath::Min(Width,float(S.X-32)));PanelSize->SetHeightOverride(FMath::Min(Height,float(S.Y-32)));
}
void SHansaFrontend::ScheduleRefresh(){SetVisibility(IsOpen()?EVisibility::Visible:EVisibility::Collapsed);if(!IsOpen()){bRefreshQueued=false;if(bWasOpen){bWasOpen=false;OnClosed.ExecuteIfBound();}return;}bWasOpen=true;if(bRefreshQueued)return;bRefreshQueued=true;RegisterActiveTimer(0,FWidgetActiveTimerDelegate::CreateLambda([Weak=TWeakPtr<SHansaFrontend>(SharedThis(this))](double,float){if(auto W=Weak.Pin()){W->bRefreshQueued=false;W->Refresh();}return EActiveTimerReturnType::Stop;}));}
void SHansaFrontend::Refresh(){
 if(!Model.IsValid())return;const auto& S=Model->GetSnapshot();SetVisibility(IsOpen()?EVisibility::Visible:EVisibility::Collapsed);bWasOpen=IsOpen();if(!IsOpen())return;
 SetPresentationSize(Available);const FString OldFocus=Focused;Rows->ClearChildren();Widgets.Reset();Actions.Reset();Labels.Reset();Order.Reset();Interface.Reset();
 auto Text=[&](FText T,EHansaUiTypographyToken Token=EHansaUiTypographyToken::Body){auto W=SNew(STextBlock).Text(T).Font(GetComponentFont(Token,Preferences)).ColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink)).AutoWrapText(true);if(Token==EHansaUiTypographyToken::Heading1&&S.Page==EHansaFrontendPage::Confirmation){W->SetColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk));Rows->AddSlot().AutoHeight().Padding(0,0,0,16)[SNew(SBorder).BorderImage(&Header).Padding(12)[W]];}else Rows->AddSlot().AutoHeight().Padding(0,0,0,12)[W];return W;};
 TSharedPtr<SHorizontalBox> SettingPair;
 auto Action=[&](FString Id,FText Label,TFunction<void()> Invoke,bool Enabled=true,bool Primary=false){auto B=SNew(SHansaAction).Preferences(Preferences).Label(Label).State(Enabled?EUiState::Default:EUiState::Disabled).Kind(Id==TEXT("Frontend.Confirm")&&S.PendingAction!=TEXT("Display")?EHansaUiButtonStyle::Destructive:Primary?EHansaUiButtonStyle::Primary:EHansaUiButtonStyle::Secondary).OnClicked_Lambda([Invoke]{Invoke();return FReply::Handled();});B->SetFocusHandler(FSimpleDelegate::CreateLambda([this,Id]{Focused=Id;}));if(Id.EndsWith(TEXT("VolumeDown"))||Id.EndsWith(TEXT("CameraDown"))||Id==TEXT("Frontend.Cancel")){Rows->AddSlot().AutoHeight().Padding(0,0,0,8)[SAssignNew(SettingPair,SHorizontalBox)];SettingPair->AddSlot().FillWidth(1)[B];}
 else if((Id.EndsWith(TEXT("VolumeUp"))||Id.EndsWith(TEXT("CameraUp"))||Id==TEXT("Frontend.Confirm"))&&SettingPair)SettingPair->AddSlot().FillWidth(1).Padding(8,0,0,0)[B];
 else Rows->AddSlot().AutoHeight().Padding(0,0,0,4)[B];Widgets.Add(Id,B);Labels.Add(Id,Label);Actions.Add(Id,Invoke);Order.Add(Id);};
 auto Request=[this](FName A){if(auto* M=Model.Get())M->Request(A);};
 const auto Page=S.Page;
 Text(Page==EHansaFrontendPage::Title?LOCTEXT("Title","Hansa"):Page==EHansaFrontendPage::Settings?LOCTEXT("Settings","Settings"):Page==EHansaFrontendPage::Credits?LOCTEXT("Credits","Credits"):Page==EHansaFrontendPage::Confirmation?(S.PendingAction==TEXT("Display")?LOCTEXT("ConfirmDisplay","Keep this display mode?"):S.PendingAction==TEXT("ReturnTitle")?LOCTEXT("ConfirmReturn","Return to title?"):S.PendingAction==TEXT("Quit")?LOCTEXT("ConfirmQuit","Quit Hansa?"):LOCTEXT("ConfirmNew","Start a new game?")):Page==EHansaFrontendPage::Loading?LOCTEXT("LoadingTitle","Preparing your city"):LOCTEXT("ErrorTitle","Unable to continue"),EHansaUiTypographyToken::Heading1);
 if(Page==EHansaFrontendPage::Title){
  Text(LOCTEXT("Context","Build a merchant house between Lübeck and Rostock."));
  Action(TEXT("Frontend.Continue"),S.bCanContinue?LOCTEXT("Continue","Continue"):LOCTEXT("NoContinue","Continue — no compatible save"),[Request]{Request(TEXT("Continue"));},S.bCanContinue&&S.bReady);
  Action(TEXT("Frontend.NewGame"),LOCTEXT("NewGame","New game"),[Request]{Request(TEXT("NewGame"));},S.bReady,true);
  Action(TEXT("Frontend.Load"),LOCTEXT("Load","Load game"),[Request]{Request(TEXT("Load"));});
  Action(TEXT("Frontend.Settings"),LOCTEXT("Settings","Settings"),[Request]{Request(TEXT("Settings"));});
  Action(TEXT("Frontend.Credits"),LOCTEXT("Credits","Credits"),[Request]{Request(TEXT("Credits"));});
  Action(TEXT("Frontend.Quit"),LOCTEXT("Quit","Quit"),[Request]{Request(TEXT("Quit"));});
  if(!S.bReady)Text(LOCTEXT("Unavailable","The city could not be prepared. Restart Hansa or restore the installed game content."));
 }else if(Page==EHansaFrontendPage::Settings){
  Text(LOCTEXT("Display","Display"),EHansaUiTypographyToken::Heading2);
  auto Setting=[this](FName N){if(auto* M=Model.Get())M->ChangeSetting(N);};
  Action(TEXT("Frontend.Settings.Window"),S.bBorderless?LOCTEXT("Borderless","Display mode: borderless"):LOCTEXT("Windowed","Display mode: windowed"),[Setting]{Setting(TEXT("Window"));});
  Action(TEXT("Frontend.Settings.VSync"),S.bVSync?LOCTEXT("VSyncOn","VSync: on"):LOCTEXT("VSyncOff","VSync: off"),[Setting]{Setting(TEXT("VSync"));});
  Text(FText::Format(LOCTEXT("Volume","Master volume: {0}"),FText::AsPercent(S.Volume)),EHansaUiTypographyToken::Heading2);
  Action(TEXT("Frontend.Settings.VolumeDown"),LOCTEXT("Quieter","Decrease volume"),[Setting]{Setting(TEXT("VolumeDown"));},S.Volume>.001f);
  Action(TEXT("Frontend.Settings.VolumeUp"),LOCTEXT("Louder","Increase volume"),[Setting]{Setting(TEXT("VolumeUp"));},S.Volume<.999f);
  Text(FText::Format(LOCTEXT("Camera","Camera speed: {0}"),FText::AsPercent(S.CameraSpeed)),EHansaUiTypographyToken::Heading2);
  Action(TEXT("Frontend.Settings.CameraDown"),LOCTEXT("Slower","Decrease camera speed"),[Setting]{Setting(TEXT("CameraDown"));},S.CameraSpeed>.501f);
  Action(TEXT("Frontend.Settings.CameraUp"),LOCTEXT("Faster","Increase camera speed"),[Setting]{Setting(TEXT("CameraUp"));},S.CameraSpeed<1.999f);
  Action(TEXT("Frontend.Settings.Edge"),S.bEdgeScroll?LOCTEXT("EdgeOn","Edge scrolling: on"):LOCTEXT("EdgeOff","Edge scrolling: off"),[Setting]{Setting(TEXT("Edge"));});
  Text(LOCTEXT("InputHelp","Camera: WASD / left stick to pan, wheel / triggers to zoom, Q/E / right stick to rotate. Menu pauses; Escape/B returns. Construction also supports select-and-place controls."));
  Rows->AddSlot().AutoHeight()[SAssignNew(Interface,SHansaUiPreferences).Preferences(Preferences).OnChanged(PreferencesChanged)];
  for(const auto& Pair:Interface->GetControls()){const FString Id=Pair.Key.Replace(TEXT("SaveLoad.Preferences."),TEXT("Frontend.Settings.Interface."));Widgets.Add(Id,Pair.Value);Labels.Add(Id,FText::FromString(Id.RightChop(28)));Order.Add(Id);Actions.Add(Id,[this,Original=Pair.Key]{Interface->ActivateControl(Original);});Pair.Value->SetFocusHandler(FSimpleDelegate::CreateLambda([this,Id]{Focused=Id;}));}
 }else if(Page==EHansaFrontendPage::Credits){
  Text(LOCTEXT("CreditsBody","Hansa\nA Hanseatic city-building and trade game.\n\nBuilt with Unreal Engine.\nInterface fonts: Source Serif 4, Atkinson Hyperlegible and Noto Sans Symbols 2, distributed under the SIL Open Font License.\n\nCredits and legal notices — placeholder\nThe complete contributor list and distribution notices will be finalized for release. Font license files accompany the project fonts."));
 }else{Widgets.Add(TEXT("Frontend.Status"),Text(S.Message));Labels.Add(TEXT("Frontend.Status"),S.Message);}
 if(Page==EHansaFrontendPage::Confirmation){Action(TEXT("Frontend.Cancel"),LOCTEXT("Cancel","Cancel"),[this]{Model->Back();},true,true);Action(TEXT("Frontend.Confirm"),S.PendingAction==TEXT("Display")?LOCTEXT("Keep","Keep display mode"):S.PendingAction==TEXT("ReturnTitle")?LOCTEXT("ReturnAction","Return to title"):S.PendingAction==TEXT("Quit")?LOCTEXT("QuitAction","Quit Hansa"):LOCTEXT("StartAction","Start new game"),[this]{Model->Confirm();});}
 else if(Page!=EHansaFrontendPage::Title&&Page!=EHansaFrontendPage::Loading)Action(TEXT("Frontend.Back"),LOCTEXT("Back","Back"),[this]{Model->Back();});
 const auto FocusOrder=GetControllerFocusOrder();const FString Default=Page==EHansaFrontendPage::Title?(S.bCanContinue?TEXT("Frontend.Continue"):TEXT("Frontend.NewGame")):FocusOrder.IsEmpty()?FString():FocusOrder[0];
 if(!FocusSemanticId(OldFocus))FocusSemanticId(Default);
}
TSharedPtr<SWidget> SHansaFrontend::ResolveSemanticWidget(const FString& Id)const{const auto* W=Widgets.Find(Id);return W?*W:nullptr;}
bool SHansaFrontend::ActivateSemanticId(const FString& Id){if(!IsOpen())return false;const auto W=ResolveSemanticWidget(Id);const auto* A=Actions.Find(Id);if(!W||!W->IsEnabled()||!A)return false;(*A)();return true;}
bool SHansaFrontend::FocusSemanticId(const FString& Id){const auto W=ResolveSemanticWidget(Id);if(!IsOpen()||!W||!W->IsEnabled()||!Actions.Contains(Id))return false;Focused=Id;Scroll->ScrollDescendantIntoView(W,false,EDescendantScrollDestination::IntoView);if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(W,EFocusCause::Navigation);return true;}
TArray<FString> SHansaFrontend::GetControllerFocusOrder()const{TArray<FString> R;if(IsOpen())for(const auto& Id:Order){const auto W=ResolveSemanticWidget(Id);if(W&&W->IsEnabled())R.Add(Id);}return R;}
FReply SHansaFrontend::OnKeyDown(const FGeometry&,const FKeyEvent& E){const auto I=ClassifyNavigationIntent(E);if(I==EHansaUiNavigationIntent::Back){Model->Back();return FReply::Handled();}if(I==EHansaUiNavigationIntent::Next||I==EHansaUiNavigationIntent::Previous){FocusSemanticId(FindWrappedFocusTarget(GetControllerFocusOrder(),Focused,I==EHansaUiNavigationIntent::Next));return FReply::Handled();}if(I==EHansaUiNavigationIntent::Activate)return ActivateSemanticId(Focused)?FReply::Handled():FReply::Unhandled();if(E.GetKey()==EKeys::PageDown||E.GetKey()==EKeys::Gamepad_RightShoulder){Scroll->SetScrollOffset(Scroll->GetScrollOffset()+220);return FReply::Handled();}if(E.GetKey()==EKeys::PageUp||E.GetKey()==EKeys::Gamepad_LeftShoulder){Scroll->SetScrollOffset(FMath::Max(0.f,Scroll->GetScrollOffset()-220));return FReply::Handled();}return FReply::Unhandled();}
TArray<FHansaHudSemanticNode> SHansaFrontend::GetSemanticSnapshot()const{
 TArray<FHansaHudSemanticNode> R;if(!Model.IsValid())return R;FHansaHudSemanticNode Root;Root.Id=TEXT("Frontend.Root");Root.ParentId=TEXT("HUD.Root");Root.Label=TEXT("Hansa");Root.Role=EHansaHudSemanticRole::Screen;Root.State.bVisible=IsOpen();Root.State.ValueType=TEXT("frontend-state");Root.State.Value=FString::Printf(TEXT("page=%d;session=%d;continue=%d;ready=%d"),int32(Model->GetSnapshot().Page),Model->GetSnapshot().bHasSession,Model->GetSnapshot().bCanContinue,Model->GetSnapshot().bReady);R.Add(Root);
 const auto C=Scroll->GetCachedGeometry();const auto Min=C.GetAbsolutePosition(),Max=Min+C.GetAbsoluteSize();
 for(const auto& Pair:Widgets){FHansaHudSemanticNode N;N.Id=Pair.Key;N.ParentId=Root.Id;N.Label=Labels.FindRef(Pair.Key).ToString();N.Role=Actions.Contains(Pair.Key)?EHansaHudSemanticRole::Button:EHansaHudSemanticRole::Status;N.State.bEnabled=Pair.Value->IsEnabled();N.State.bFocused=Pair.Value->HasKeyboardFocus();N.State.ValueType=TEXT("frontend-control");N.State.Value=N.Label;N.bCanActivate=N.bCanFocus=Actions.Contains(Pair.Key)&&N.State.bEnabled;auto G=Pair.Value->GetCachedGeometry();auto A=G.GetAbsolutePosition(),B=A+G.GetAbsoluteSize();A.X=FMath::Max(A.X,Min.X);A.Y=FMath::Max(A.Y,Min.Y);B.X=FMath::Min(B.X,Max.X);B.Y=FMath::Min(B.Y,Max.Y);N.State.bVisible=IsOpen()&&B.X>A.X&&B.Y>A.Y;if(N.State.bVisible)N.Bounds=FIntRect(FMath::RoundToInt(A.X),FMath::RoundToInt(A.Y),FMath::RoundToInt(B.X),FMath::RoundToInt(B.Y));R.Add(MoveTemp(N));}return R;
}
}
#undef LOCTEXT_NAMESPACE
