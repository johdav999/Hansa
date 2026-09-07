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

namespace Hansa::UI
{
	SHansaResearchScreen::~SHansaResearchScreen()
	{
		if (UHansaResearchPresentationModel* Pinned = Model.Get()) Pinned->OnChanged().Remove(ChangedHandle);
	}

	void SHansaResearchScreen::Construct(const FArguments& Arguments)
	{
		Model = Arguments._Model;
		PanelBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Decision);
		InnerBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Working);
		PrimaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		HeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1, false);
		BodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false);
		CaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, false);
		ChildSlot[SAssignNew(Root, SBox).WidthOverride(1120.0f).HeightOverride(580.0f)[SNew(SBorder).BorderImage(&PanelBrush).Padding(24)
		[SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
			+SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(TEXT("Research · Commerce / Production / Logistics"))).TextStyle(&HeadingStyle)]
			+SHorizontalBox::Slot().AutoWidth()[SAssignNew(CloseButton,SButton).ButtonStyle(&SecondaryButtonStyle).Text(FText::FromString(TEXT("Close"))).OnClicked(this,&SHansaResearchScreen::HandleClose)]]
		+SVerticalBox::Slot().FillHeight(1).Padding(0,12)[SNew(SHorizontalBox)
			+SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Commerce,SVerticalBox)]
			+SHorizontalBox::Slot().FillWidth(1).Padding(8,0)[SAssignNew(Production,SVerticalBox)]
			+SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Logistics,SVerticalBox)]
			+SHorizontalBox::Slot().FillWidth(0.9f).Padding(12,0,0,0)[SAssignNew(Detail,SVerticalBox)]]
		+SVerticalBox::Slot().AutoHeight()[SAssignNew(Queue,SVerticalBox)]]]];
		Widgets.Add(TEXT("Research.Root"), Root);
		Widgets.Add(TEXT("Research.Close"), CloseButton);
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
		const TCHAR* Heading = Branch == Hansa::Simulation::EHansaResearchBranch::Commerce ? TEXT("COMMERCE") :
			Branch == Hansa::Simulation::EHansaResearchBranch::Production ? TEXT("PRODUCTION") : TEXT("LOGISTICS");
		Box.AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Heading)).TextStyle(&CaptionStyle)];
		for (const FHansaResearchNodePresentation& Node : Snapshot.Nodes)
		{
			if (Node.Branch != Branch) continue;
			const FString SemanticId = TEXT("Research.Node.") + SafeId(Node.StableId);
			const TCHAR* State = Node.State == EHansaResearchNodePresentationState::Completed ? TEXT("✓ Completed") :
				Node.State == EHansaResearchNodePresentationState::Queued ? TEXT("⌛ Researching") :
				Node.State == EHansaResearchNodePresentationState::Available ? TEXT("◇ Available") : TEXT("🔒 Locked");
			TSharedPtr<SButton> Button;
			Box.AddSlot().AutoHeight().Padding(0,5)[SAssignNew(Button,SButton).ButtonStyle(&SecondaryButtonStyle).OnClicked(this,&SHansaResearchScreen::HandleSelect,Node.StableId)
			[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Node.DisplayName).TextStyle(&BodyStyle)]
			+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s · %d points · %d ticks"),State,Node.CostResearchPoints,Node.DurationTicks))).TextStyle(&CaptionStyle)]]];
			Widgets.Add(SemanticId, Button);
		}
	}

	void SHansaResearchScreen::Refresh(const FHansaResearchPresentationSnapshot& Snapshot, uint64)
	{
		if (!Commerce || !Production || !Logistics || !Detail || !Queue) return;
		RebuildBranch(*Commerce, Hansa::Simulation::EHansaResearchBranch::Commerce, Snapshot);
		RebuildBranch(*Production, Hansa::Simulation::EHansaResearchBranch::Production, Snapshot);
		RebuildBranch(*Logistics, Hansa::Simulation::EHansaResearchBranch::Logistics, Snapshot);
		Detail->ClearChildren(); Queue->ClearChildren();
		const FHansaResearchNodePresentation* Selected = Snapshot.Nodes.FindByPredicate([&Snapshot](const auto& Node){return Node.StableId==Snapshot.SelectedTechnologyId;});
		if (Selected)
		{
			Detail->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("Selected technology"))).TextStyle(&CaptionStyle)];
			Detail->AddSlot().AutoHeight().Padding(0,6)[SNew(STextBlock).Text(Selected->DisplayName).TextStyle(&BodyStyle)];
			Detail->AddSlot().AutoHeight()[SNew(STextBlock).Text(Selected->UnlockExplanation).TextStyle(&BodyStyle).AutoWrapText(true)];
			Detail->AddSlot().AutoHeight().Padding(0,6)[SNew(STextBlock).Text(Selected->EffectSummary).TextStyle(&CaptionStyle).AutoWrapText(true)];
			TSharedPtr<SButton> QueueButton;
			Detail->AddSlot().AutoHeight().Padding(0,8)[SAssignNew(QueueButton,SButton).ButtonStyle(&PrimaryButtonStyle).Text(FText::FromString(TEXT("Start research"))).IsEnabled(Selected->State==EHansaResearchNodePresentationState::Available).OnClicked(this,&SHansaResearchScreen::HandleQueue)];
			Widgets.Add(TEXT("Research.Action.Queue"),QueueButton);
		}
		const FHansaResearchNodePresentation* Active = Snapshot.Nodes.FindByPredicate([](const auto& Node){return Node.State==EHansaResearchNodePresentationState::Queued;});
		Queue->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Active ? FString::Printf(TEXT("Queue 1/1 · %s · %d/%d ticks"),*Active->DisplayName.ToString(),Active->ProgressTicks,Active->DurationTicks) : TEXT("Queue 0/1 · No active research"))).TextStyle(&CaptionStyle)];
		SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);
	}

	FReply SHansaResearchScreen::HandleClose(){if(auto* P=Model.Get())P->Close();return FReply::Handled();}
	FReply SHansaResearchScreen::HandleQueue(){if(auto* P=Model.Get())P->RequestQueueSelected();return FReply::Handled();}
	FReply SHansaResearchScreen::HandleSelect(FString Id){if(auto* P=Model.Get())P->SelectTechnology(Id);return FReply::Handled();}

	bool SHansaResearchScreen::ActivateSemanticId(const FString& Id)
	{
		if (Id==TEXT("Research.Close")){HandleClose();return true;}
		if (Id==TEXT("Research.Action.Queue")){return Model.IsValid() && Model->RequestQueueSelected();}
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
		if(FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::Navigation);
		return true;
	}

	TArray<FString> SHansaResearchScreen::GetControllerFocusOrder() const
	{
		TArray<FString> Result={TEXT("Research.Close")};
		if(auto* P=Model.Get()) for(const auto& Node:P->GetSnapshot().Nodes) Result.Add(TEXT("Research.Node.")+SafeId(Node.StableId));
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
		Add(TEXT("Research.Queue"),TEXT("Research.Root"),TEXT("Research queue"),EHansaHudSemanticRole::Status,false,false,TEXT("capacity=1"));
		Add(TEXT("Research.Action.Queue"),TEXT("Research.Root"),TEXT("Start selected research"),EHansaHudSemanticRole::Button,true,true,S.SelectedTechnologyId);
		return Result;
	}
}
