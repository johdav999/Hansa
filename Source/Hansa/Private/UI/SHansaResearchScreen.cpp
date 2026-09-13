#include "UI/SHansaResearchScreen.h"

#include "Framework/Application/SlateApplication.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaUiNavigation.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Algo/Sort.h"

namespace Hansa::UI
{
	SHansaResearchScreen::~SHansaResearchScreen()
	{
		if (UHansaResearchPresentationModel* Pinned = Model.Get()) Pinned->OnChanged().Remove(ChangedHandle);
	}

	void SHansaResearchScreen::SetPresentationSize(FIntPoint Size){bCompact=Size.X<1100;if(Root){Root->SetWidthOverride(FMath::Min(1600.f,float(Size.X)));Root->SetHeightOverride(FMath::Min(720.f,float(Size.Y)));}}
	void SHansaResearchScreen::Construct(const FArguments& Arguments)
	{
		Model = Arguments._Model; Preferences = Arguments._Preferences;
        HeaderBrush=GetComponentStyle(EUiSurface::TopBar,EUiState::Default,Preferences).Brush;
		PanelBrush = GetComponentStyle(EUiSurface::Modal,EUiState::Default,Preferences).Brush;
		InnerBrush = GetComponentStyle(EUiSurface::Panel,EUiState::Default,Preferences).Brush;
		PrimaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		HeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1, false); HeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading1,Preferences));
		BodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false); BodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
		CaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, false); CaptionStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Caption,Preferences));
		ChildSlot[SAssignNew(Root, SBox).WidthOverride(1120.0f).HeightOverride(460.0f)[SNew(SBorder).BorderImage(&PanelBrush).Padding(16)
		[SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&HeaderBrush).Padding(8)[SNew(SHorizontalBox)
			+SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(TEXT("Research"))).TextStyle(&HeadingStyle).ColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk))]
			+SHorizontalBox::Slot().AutoWidth()[SAssignNew(CloseButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Icon).Label(FText::FromString(TEXT("Close"))).OnClicked(this,&SHansaResearchScreen::HandleClose)]]]
		+SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(BranchTabs,SHorizontalBox).Visibility_Lambda([this]{return bCompact?EVisibility::Visible:EVisibility::Collapsed;})]
        +SVerticalBox::Slot().FillHeight(1).Padding(0,12)[SNew(SHorizontalBox)
			+SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([this]{return !bCompact || ActiveBranch==Hansa::Simulation::EHansaResearchBranch::Commerce?1.f:0.f;}))[SNew(SBox).Visibility_Lambda([this]{return !bCompact || ActiveBranch==Hansa::Simulation::EHansaResearchBranch::Commerce?EVisibility::Visible:EVisibility::Collapsed;})[SAssignNew(CommerceScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(Commerce,SVerticalBox)]]]
			+SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([this]{return !bCompact || ActiveBranch==Hansa::Simulation::EHansaResearchBranch::Production?1.f:0.f;})).Padding(4,0)[SNew(SBox).Visibility_Lambda([this]{return !bCompact || ActiveBranch==Hansa::Simulation::EHansaResearchBranch::Production?EVisibility::Visible:EVisibility::Collapsed;})[SAssignNew(ProductionScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(Production,SVerticalBox)]]]
			+SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([this]{return !bCompact || ActiveBranch==Hansa::Simulation::EHansaResearchBranch::Logistics?1.f:0.f;}))[SNew(SBox).Visibility_Lambda([this]{return !bCompact || ActiveBranch==Hansa::Simulation::EHansaResearchBranch::Logistics?EVisibility::Visible:EVisibility::Collapsed;})[SAssignNew(LogisticsScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(Logistics,SVerticalBox)]]]
			+SHorizontalBox::Slot().FillWidth(1.35f).Padding(16,0,0,0)[SNew(SVerticalBox)+SVerticalBox::Slot().FillHeight(1)[SAssignNew(DetailScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(Detail,SVerticalBox)]]+SVerticalBox::Slot().AutoHeight()[SAssignNew(Actions,SVerticalBox)]]]
		+SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&HeaderBrush).Padding(8)[SAssignNew(Queue,SVerticalBox)]]]]];
		for(int32 I=0;I<3;++I) {
            const TCHAR* Names[]={TEXT("Commerce"),TEXT("Production"),TEXT("Logistics")};
            TSharedPtr<SHansaAction> Tab;
            BranchTabs->AddSlot().FillWidth(1).Padding(2,0)[SAssignNew(Tab,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).Label(FText::FromString(Names[I])).OnClicked_Lambda([this,I]{ActiveBranch=static_cast<Hansa::Simulation::EHansaResearchBranch>(I);if(auto* P=Model.Get())for(const auto& N:P->GetSnapshot().Nodes)if(N.Branch==ActiveBranch){const FString Id=N.StableId;P->SelectTechnology(Id);FocusSemanticId(TEXT("Research.Node.")+SafeId(Id));break;}return FReply::Handled();})];
            Widgets.Add(FString::Printf(TEXT("Research.Branch.%d"),I),Tab);
        }
        Widgets.Add(TEXT("Research.Root"), Root);
		Widgets.Add(TEXT("Research.Close"), CloseButton);
        Widgets.Add(TEXT("Research.Queue"),Queue);
        Widgets.Add(TEXT("Research.Detail"),Detail);
		if (UHansaResearchPresentationModel* Pinned = Model.Get())
		{
			ChangedHandle = Pinned->OnChanged().AddSP(SharedThis(this), &SHansaResearchScreen::Refresh);
			Refresh(Pinned->GetSnapshot(), Pinned->GetRevision());
		}
	}

	FString SHansaResearchScreen::SafeId(const FString& StableId) { FString Result=StableId; Result.ReplaceInline(TEXT("."),TEXT("_")); return Result; }

	void SHansaResearchScreen::RebuildBranch(SVerticalBox& Box, const Hansa::Simulation::EHansaResearchBranch Branch,
		const FHansaResearchPresentationSnapshot& Snapshot)
	{
		Box.ClearChildren();
		const TCHAR* Heading = Branch == Hansa::Simulation::EHansaResearchBranch::Commerce ? TEXT("Commerce") :
			Branch == Hansa::Simulation::EHansaResearchBranch::Production ? TEXT("Production") : TEXT("Logistics");
		Box.AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Heading)).TextStyle(&CaptionStyle).AutoWrapText(true)];
		for (const FHansaResearchNodePresentation& Node : Snapshot.Nodes)
		{
			if (Node.Branch != Branch) continue;
			const FString SemanticId = TEXT("Research.Node.") + SafeId(Node.StableId);
            if(!Node.PrerequisiteIds.IsEmpty()) {
                TArray<FString> Names;
                for(const auto& Id:Node.PrerequisiteIds) {const auto* P=Snapshot.Nodes.FindByPredicate([&](const auto& N){return N.StableId==Id;});Names.Add(P?P->DisplayName.ToString():Id);}
                Box.AddSlot().AutoHeight().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(2).HeightOverride(12)[SNew(SBorder).BorderImage(&HeaderBrush).Padding(0)]];
                Box.AddSlot().AutoHeight().Padding(8,2)[SNew(STextBlock).Text(FText::FromString(TEXT("Requires: ")+FString::Join(Names,TEXT(", ")))).TextStyle(&CaptionStyle).AutoWrapText(true)];
            }
			const TCHAR* State = Node.State == EHansaResearchNodePresentationState::Completed ? TEXT("Completed") :
				Node.State == EHansaResearchNodePresentationState::Queued ? TEXT("Researching") :
				Node.State == EHansaResearchNodePresentationState::Available ? TEXT("Available") : TEXT("Locked");
			TSharedPtr<SButton> Button;
			Box.AddSlot().AutoHeight().Padding(0,5)[SAssignNew(Button,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).State(Node.StableId==Snapshot.SelectedTechnologyId?EUiState::Selected:EUiState::Default).OnClicked(this,&SHansaResearchScreen::HandleSelect,Node.StableId)
			[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Node.DisplayName).TextStyle(&BodyStyle).AutoWrapText(true)]
			+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s · %d points · %d ticks"),State,Node.CostResearchPoints,Node.DurationTicks))).TextStyle(&CaptionStyle).AutoWrapText(true)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,5)[SNew(STextBlock).Text(Node.EffectSummary).TextStyle(&CaptionStyle).AutoWrapText(true)]]];
			Widgets.Add(SemanticId, Button);
			StaticCastSharedPtr<SHansaAction>(Button)->SetFocusHandler(FSimpleDelegate::CreateLambda([WeakModel=Model,SemanticId]{if(auto* P=WeakModel.Get())P->SetFocusedSemanticId(FName(*SemanticId));}));
		}
	}

	void SHansaResearchScreen::Refresh(const FHansaResearchPresentationSnapshot& Snapshot, uint64)
	{
		if (!Commerce || !Production || !Logistics || !Detail || !Queue) return;
		SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);
		FString ContentKey = Snapshot.SelectedTechnologyId + FString::FromInt(Snapshot.AvailableResearchPoints) + Snapshot.Feedback.ToString() + FString::FromInt(Snapshot.bLoading) + FString::FromInt(Snapshot.bSubmitting);
		for(const auto& N:Snapshot.Nodes) ContentKey += FString::Printf(TEXT("|%s;%s;%d;%d;%d;%d;%s;%s"),*N.StableId,*N.DisplayName.ToString(),int32(N.State),N.CostResearchPoints,N.DurationTicks,N.ProgressTicks,*N.UnlockExplanation.ToString(),*N.EffectSummary.ToString());
		for(const auto& N:Snapshot.Nodes) ContentKey += N.LockedReason.ToString()+N.AppliedEffectSummary.ToString()+FString::Join(N.PrerequisiteIds,TEXT(";"));
        if(ContentKey == PresentedContentKey) return;
		PresentedContentKey = MoveTemp(ContentKey);
		const auto* OldFocus = Widgets.Find(Snapshot.FocusedSemanticId.ToString());
		const bool RestoreFocus = OldFocus && OldFocus->IsValid() && OldFocus->Pin()->HasKeyboardFocus();
		for(auto It=Widgets.CreateIterator();It;++It) if(It.Key().StartsWith(TEXT("Research.Node."))||It.Key().StartsWith(TEXT("Research.Prerequisite."))||It.Key().StartsWith(TEXT("Research.Effect.")))It.RemoveCurrent();
        Widgets.Remove(TEXT("Research.Action.Queue"));
		RebuildBranch(*Commerce, Hansa::Simulation::EHansaResearchBranch::Commerce, Snapshot);
		RebuildBranch(*Production, Hansa::Simulation::EHansaResearchBranch::Production, Snapshot);
		RebuildBranch(*Logistics, Hansa::Simulation::EHansaResearchBranch::Logistics, Snapshot);
		Detail->ClearChildren(); Queue->ClearChildren(); Actions->ClearChildren();
		const FHansaResearchNodePresentation* Selected = Snapshot.Nodes.FindByPredicate([&Snapshot](const auto& Node){return Node.StableId==Snapshot.SelectedTechnologyId;});
		if (Selected && !Snapshot.bLoading)
        {
            if(bCompact) ActiveBranch=Selected->Branch;
			Detail->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("Selected technology"))).TextStyle(&CaptionStyle).AutoWrapText(true)];
			Detail->AddSlot().AutoHeight().Padding(0,6)[SNew(STextBlock).Text(Selected->DisplayName).TextStyle(&BodyStyle).AutoWrapText(true)];
			Detail->AddSlot().AutoHeight()[SNew(STextBlock).Text(Selected->UnlockExplanation).TextStyle(&BodyStyle).AutoWrapText(true)];
			Detail->AddSlot().AutoHeight().Padding(0,6)[SNew(STextBlock).Text(FText::FromString((Selected->State==EHansaResearchNodePresentationState::Completed?TEXT("Applied effects\n"):TEXT("On completion\n"))+(Selected->State==EHansaResearchNodePresentationState::Completed?Selected->AppliedEffectSummary:Selected->EffectSummary).ToString())).TextStyle(&CaptionStyle).AutoWrapText(true)];
            if(!Selected->LockedReason.IsEmpty()) Actions->AddSlot().AutoHeight().Padding(0,8)[SNew(STextBlock).Text(Selected->LockedReason).TextStyle(&BodyStyle).AutoWrapText(true)];
            for(const auto& Id:Selected->PrerequisiteIds) {
                const auto* Prerequisite=Snapshot.Nodes.FindByPredicate([&](const auto& N){return N.StableId==Id;});
                TSharedPtr<SHansaAction> Button;
                const FString Label=(Prerequisite&&Prerequisite->State==EHansaResearchNodePresentationState::Completed?TEXT(""):TEXT("Requires: "))+(Prerequisite?Prerequisite->DisplayName.ToString():Id);
                Detail->AddSlot().AutoHeight().Padding(0,4)[SAssignNew(Button,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).Label(FText::FromString(Label)).OnClicked(this,&SHansaResearchScreen::HandleSelect,Id)];
                Widgets.Add(TEXT("Research.Prerequisite.")+SafeId(Id),Button);
            }
            for(int32 I=0;I<Selected->Effects.Num();++I) {
                TSharedPtr<SHansaAction> Button;
                FString Target=Selected->Effects[I].TargetStableId;int32 Dot;if(Target.FindLastChar(TEXT('.'),Dot))Target.RightChopInline(Dot+1);
                Detail->AddSlot().AutoHeight().Padding(0,4)[SAssignNew(Button,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).Label(FText::FromString((Target==TEXT("Lubeck")?TEXT("View Lübeck market"):Selected->Effects[I].TargetStableId.StartsWith(TEXT("Recipe."))||Selected->Effects[I].TargetStableId.StartsWith(TEXT("Building."))?TEXT("View affected building"):TEXT("View affected route")))).OnClicked_Lambda([this,I]{if(auto* P=Model.Get())P->RequestEffect(I);return FReply::Handled();})];
                Widgets.Add(FString::Printf(TEXT("Research.Effect.%d"),I),Button);
            }
            TSharedPtr<SButton> QueueButton;
			Actions->AddSlot().AutoHeight().Padding(0,8)[SAssignNew(QueueButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Primary).Label(FText::FromString(Selected->State==EHansaResearchNodePresentationState::Completed?TEXT("Completed"):Selected->State==EHansaResearchNodePresentationState::Queued?TEXT("Researching"):Selected->State==EHansaResearchNodePresentationState::Locked?TEXT("Research locked"):TEXT("Start research"))).State(Selected->State==EHansaResearchNodePresentationState::Available&&!Snapshot.bSubmitting?EUiState::Default:EUiState::Disabled).OnClicked(this,&SHansaResearchScreen::HandleQueue)];
			Widgets.Add(TEXT("Research.Action.Queue"),QueueButton);
            StaticCastSharedPtr<SHansaAction>(QueueButton)->SetFocusHandler(FSimpleDelegate::CreateLambda([WeakModel=Model]{if(auto* P=WeakModel.Get())P->SetFocusedSemanticId(TEXT("Research.Action.Queue"));}));
		}
        for(const auto& Pair:Widgets)if(Pair.Key.StartsWith(TEXT("Research.Prerequisite."))||Pair.Key.StartsWith(TEXT("Research.Effect."))) {
            const FName Id(*Pair.Key);
            StaticCastSharedPtr<SHansaAction>(Pair.Value.Pin())->SetFocusHandler(FSimpleDelegate::CreateLambda([WeakModel=Model,Id]{if(auto* P=WeakModel.Get())P->SetFocusedSemanticId(Id);}));
        }
		if(Snapshot.bLoading) Detail->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("Loading research…"))).TextStyle(&BodyStyle).AutoWrapText(true)];
        if(!Snapshot.Feedback.IsEmpty()) Queue->AddSlot().AutoHeight().Padding(0,4)[SNew(STextBlock).Text(Snapshot.Feedback).TextStyle(&BodyStyle).ColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk)).AutoWrapText(true)];
        Queue->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::Format(NSLOCTEXT("Research","Balance","Research points: {0}"),FText::AsNumber(Snapshot.AvailableResearchPoints))).TextStyle(&BodyStyle).ColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk))];
        const FHansaResearchNodePresentation* Active = Snapshot.Nodes.FindByPredicate([](const auto& Node){return Node.State==EHansaResearchNodePresentationState::Queued;});
		Queue->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Active ? FString::Printf(TEXT("Queue 1/1 · %s · %d/%d ticks"),*Active->DisplayName.ToString(),Active->ProgressTicks,Active->DurationTicks) : TEXT("Queue 0/1 · No active research"))).TextStyle(&CaptionStyle).ColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk)).AutoWrapText(true)];
		if(Active) {
            Queue->AddSlot().AutoHeight().Padding(0,4)[SNew(SBox).HeightOverride(8)[SNew(SProgressBar).FillColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass)).Percent(float(Active->ProgressTicks)/FMath::Max(1,Active->DurationTicks))]];
            Queue->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::Format(NSLOCTEXT("Research","Remaining","{0} simulation ticks remaining • Progress pauses when the game is paused."),FText::AsNumber(FMath::Max(0,Active->DurationTicks-Active->ProgressTicks)))).TextStyle(&CaptionStyle).ColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk)).AutoWrapText(true)];
        }
        SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);
        if(RestoreFocus) FocusSemanticId(Snapshot.FocusedSemanticId.ToString());
	}

	FReply SHansaResearchScreen::HandleClose(){if(auto* P=Model.Get())P->Close();return FReply::Handled();}
	FReply SHansaResearchScreen::HandleQueue(){if(auto* P=Model.Get())if(P->RequestQueueSelected())FocusSemanticId(TEXT("Research.Node.")+SafeId(P->GetSnapshot().SelectedTechnologyId));return FReply::Handled();}
	FReply SHansaResearchScreen::HandleSelect(FString Id){if(auto* P=Model.Get())P->SelectTechnology(Id);return FReply::Handled();}

	bool SHansaResearchScreen::ActivateSemanticId(const FString& Id)
	{
		if (Id==TEXT("Research.Close")){HandleClose();return true;}
		if (Id==TEXT("Research.Action.Queue")){return Model.IsValid() && Model->RequestQueueSelected();}
		if(Id.StartsWith(TEXT("Research.Effect.")))return Model.IsValid()&&Model->RequestEffect(FCString::Atoi(*Id.RightChop(16)));
        if(Id.StartsWith(TEXT("Research.Branch."))&&Model.IsValid()) {
            const FString Suffix=Id.RightChop(16);
            if(Suffix!=TEXT("0")&&Suffix!=TEXT("1")&&Suffix!=TEXT("2"))return false;
            ActiveBranch=static_cast<Hansa::Simulation::EHansaResearchBranch>(FCString::Atoi(*Suffix));
            for(const auto& N:Model->GetSnapshot().Nodes)if(N.Branch==ActiveBranch){const FString TechnologyId=N.StableId;Model->SelectTechnology(TechnologyId);FocusSemanticId(TEXT("Research.Node.")+SafeId(TechnologyId));return true;}
            return false;
        }
        if(Id.StartsWith(TEXT("Research.Prerequisite."))&&Model.IsValid())for(const auto& N:Model->GetSnapshot().Nodes)if(Id==TEXT("Research.Prerequisite.")+SafeId(N.StableId))return Model->SelectTechnology(N.StableId);
        const FString Prefix=TEXT("Research.Node.");
		if(Id.StartsWith(Prefix) && Model.IsValid()) for(const auto& Node:Model->GetSnapshot().Nodes) if(SafeId(Node.StableId)==Id.Mid(Prefix.Len())) return Model->SelectTechnology(Node.StableId);
		return false;
	}

	bool SHansaResearchScreen::FocusSemanticId(const FString& Id)
	{
		const TWeakPtr<SWidget>* Found = Widgets.Find(Id);
		const TSharedPtr<SWidget> Widget = Found != nullptr ? Found->Pin() : nullptr;
		if (!Widget.IsValid() || !Widget->IsEnabled()) return false;
		if(auto* P=Model.Get()) P->SetFocusedSemanticId(FName(*Id));
		TSharedPtr<SScrollBox> Scroll;
        if(Id.StartsWith(TEXT("Research.Effect."))||Id.StartsWith(TEXT("Research.Prerequisite.")))Scroll=DetailScroll;
        else if(auto* P=Model.Get())for(const auto& Node:P->GetSnapshot().Nodes)if(Id==TEXT("Research.Node.")+SafeId(Node.StableId)){
            using Hansa::Simulation::EHansaResearchBranch;
            ActiveBranch=Node.Branch;
            Scroll=Node.Branch==EHansaResearchBranch::Commerce?CommerceScroll:Node.Branch==EHansaResearchBranch::Production?ProductionScroll:LogisticsScroll;
            break;
        }
        if(Scroll)Scroll->ScrollDescendantIntoView(Widget,false,EDescendantScrollDestination::IntoView);
		if(FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::Navigation);
		return true;
	}

	TArray<FString> SHansaResearchScreen::GetControllerFocusOrder() const
	{
		TArray<FString> Result={TEXT("Research.Close")};
		if(auto* P=Model.Get()) for(const auto& Node:P->GetSnapshot().Nodes) Result.Add(TEXT("Research.Node.")+SafeId(Node.StableId));
		for(const auto& Pair:Widgets)if(Pair.Key.StartsWith(TEXT("Research.Prerequisite."))||Pair.Key.StartsWith(TEXT("Research.Effect.")))Result.Add(Pair.Key);
        // Stable suffix order independent of map allocation and rebuilds.
        const int32 ExtrasStart=1+(Model.IsValid()?Model->GetSnapshot().Nodes.Num():0);
        if(Result.Num()>ExtrasStart)Algo::Sort(MakeArrayView(Result.GetData()+ExtrasStart,Result.Num()-ExtrasStart));
        if (const TWeakPtr<SWidget>* QueueWidget = Widgets.Find(TEXT("Research.Action.Queue")); QueueWidget != nullptr)
		{
			if (const TSharedPtr<SWidget> Widget = QueueWidget->Pin(); Widget.IsValid() && Widget->IsEnabled()) Result.Add(TEXT("Research.Action.Queue"));
		}
		return Result;
	}

	FReply SHansaResearchScreen::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		(void)MyGeometry;
		const EHansaUiNavigationIntent Intent = ClassifyNavigationIntent(InKeyEvent);
		if (Intent == EHansaUiNavigationIntent::Back) return HandleClose();
		UHansaResearchPresentationModel* Pinned = Model.Get();
		if (Pinned == nullptr) return FReply::Unhandled();
		if (Intent == EHansaUiNavigationIntent::Activate)
		{
			return ActivateSemanticId(Pinned->GetSnapshot().FocusedSemanticId.ToString()) ? FReply::Handled() : FReply::Unhandled();
		}
		if (Intent == EHansaUiNavigationIntent::Next || Intent == EHansaUiNavigationIntent::Previous)
		{
			const FString Target = FindWrappedFocusTarget(GetControllerFocusOrder(), Pinned->GetSnapshot().FocusedSemanticId.ToString(), Intent == EHansaUiNavigationIntent::Next);
			return !Target.IsEmpty() && FocusSemanticId(Target) ? FReply::Handled() : FReply::Unhandled();
		}
		return FReply::Unhandled();
	}

	TArray<FHansaHudSemanticNode> SHansaResearchScreen::GetSemanticSnapshot() const
	{
		TArray<FHansaHudSemanticNode> Result; const auto* P=Model.Get(); if(!P)return Result; const auto& S=P->GetSnapshot();
		auto Add=[&](FString Id,FString Parent,FString Label,EHansaHudSemanticRole Role,bool Activate,bool Focus,FString Value,bool Selected=false,bool Warning=false)
		{FHansaHudSemanticNode N;N.Id=MoveTemp(Id);N.ParentId=MoveTemp(Parent);N.Label=MoveTemp(Label);N.Role=Role;N.bCanActivate=Activate;N.bCanFocus=Focus;N.State.ValueType=TEXT("research");N.State.Value=MoveTemp(Value);N.State.bSelected=Selected;N.State.bWarning=Warning;N.State.bFocused=S.FocusedSemanticId==FName(*N.Id);N.State.bVisible=S.bOpen;Result.Add(MoveTemp(N));};
		Add(TEXT("Research.Root"),TEXT(""),TEXT("Research"),EHansaHudSemanticRole::Screen,false,false,TEXT("three-branch;queue=1"));
		Add(TEXT("Research.Close"),TEXT("Research.Root"),TEXT("Close research"),EHansaHudSemanticRole::Button,true,true,TEXT("close"));
		for(const auto& Node:S.Nodes){const FString State=Node.State==EHansaResearchNodePresentationState::Completed?TEXT("completed"):Node.State==EHansaResearchNodePresentationState::Queued?TEXT("queued"):Node.State==EHansaResearchNodePresentationState::Available?TEXT("available"):TEXT("locked");Add(TEXT("Research.Node.")+SafeId(Node.StableId),TEXT("Research.Root"),Node.DisplayName.ToString(),EHansaHudSemanticRole::Button,true,true,FString::Printf(TEXT("id=%s;state=%s;cost=%d;duration=%d;missing=%s;effect=%s"),*Node.StableId,*State,Node.CostResearchPoints,Node.DurationTicks,*FString::Join(Node.MissingPrerequisiteIds,TEXT(",")),*Node.EffectSummary.ToString()),S.SelectedTechnologyId==Node.StableId,Node.State==EHansaResearchNodePresentationState::Locked);}
		Add(TEXT("Research.Queue"),TEXT("Research.Root"),TEXT("Research queue"),EHansaHudSemanticRole::Status,false,false,FString::Printf(TEXT("capacity=1;points=%d;loading=%d;submitting=%d;feedback=%s"),S.AvailableResearchPoints,S.bLoading,S.bSubmitting,*S.Feedback.ToString()));
        for(const auto& N:S.Nodes)if(N.State==EHansaResearchNodePresentationState::Queued)Result.Last().State.Value+=FString::Printf(TEXT(";active=%s;progress=%d;duration=%d"),*N.StableId,N.ProgressTicks,N.DurationTicks);
        for(const auto& Pair:Widgets)if(Pair.Key.StartsWith(TEXT("Research.Prerequisite."))||Pair.Key.StartsWith(TEXT("Research.Effect."))||Pair.Key.StartsWith(TEXT("Research.Branch.")))Add(Pair.Key,TEXT("Research.Root"),Pair.Key,EHansaHudSemanticRole::Button,true,true,Pair.Key);
        if(const auto* N=S.Nodes.FindByPredicate([&](const auto& It){return It.StableId==S.SelectedTechnologyId;}))Add(TEXT("Research.Detail"),TEXT("Research.Root"),N->DisplayName.ToString(),EHansaHudSemanticRole::Status,false,false,TEXT("reason=")+N->LockedReason.ToString()+TEXT(";planned=")+N->EffectSummary.ToString()+TEXT(";applied=")+N->AppliedEffectSummary.ToString());
		const bool CanQueue=Widgets.Contains(TEXT("Research.Action.Queue"))&&!S.bLoading&&!S.bSubmitting;
        Add(TEXT("Research.Action.Queue"),TEXT("Research.Root"),TEXT("Start selected research"),EHansaHudSemanticRole::Button,CanQueue,CanQueue,S.SelectedTechnologyId);
        for(auto& N:Result)if(const auto* W=Widgets.Find(N.Id);W&&W->IsValid()){
            const auto Widget=W->Pin();
            N.State.bEnabled=Widget->IsEnabled();N.bCanActivate &= N.State.bEnabled;N.bCanFocus &= N.State.bEnabled;
            const auto G=Widget->GetCachedGeometry();FVector2D Min=G.GetAbsolutePosition(),Max=Min+G.GetAbsoluteSize();
            TSharedPtr<SScrollBox> Clip;
            if(N.Id.StartsWith(TEXT("Research.Node.")))for(const auto& Node:S.Nodes)if(N.Id==TEXT("Research.Node.")+SafeId(Node.StableId)) {
                if(bCompact&&Node.Branch!=ActiveBranch)N.State.bVisible=false;
                using Hansa::Simulation::EHansaResearchBranch;
                Clip=Node.Branch==EHansaResearchBranch::Commerce?CommerceScroll:Node.Branch==EHansaResearchBranch::Production?ProductionScroll:LogisticsScroll;
            }
            if(N.Id.StartsWith(TEXT("Research.Effect."))||N.Id.StartsWith(TEXT("Research.Prerequisite."))||N.Id==TEXT("Research.Detail"))Clip=DetailScroll;
            if(N.Id.StartsWith(TEXT("Research.Branch.")))N.State.bVisible&=bCompact;
            if(Clip){const auto C=Clip->GetCachedGeometry();const auto A=C.GetAbsolutePosition(),B=A+C.GetAbsoluteSize();Min.X=FMath::Max(Min.X,A.X);Min.Y=FMath::Max(Min.Y,A.Y);Max.X=FMath::Min(Max.X,B.X);Max.Y=FMath::Min(Max.Y,B.Y);}
            N.State.bVisible &= Max.X>Min.X&&Max.Y>Min.Y;
            if(N.State.bVisible)N.Bounds=FIntRect(FMath::RoundToInt(Min.X),FMath::RoundToInt(Min.Y),FMath::RoundToInt(Max.X),FMath::RoundToInt(Max.Y));
            if(N.Id==TEXT("Research.Queue")){N.State.bError=!S.Feedback.IsEmpty();}

        }
		return Result;
	}
}
