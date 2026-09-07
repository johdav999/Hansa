#include "Studio/SHansaResearchGraphPanel.h"

#include "Definitions/HansaDefinitionBase.h"
#include "Definitions/HansaResearchDefinitions.h"
#include "Studio/SHansaAuthoringStudio.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SHansaResearchGraphPanel::Construct(const FArguments&)
{
	ChildSlot[SNew(SVerticalBox)
	+SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Text(FText::FromString(TEXT("Research graph · stable prerequisites and effects"))).Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))]
	+SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1)[SNew(SBorder).Padding(8)[SAssignNew(CommerceLane,SVerticalBox)]]
		+SHorizontalBox::Slot().FillWidth(1).Padding(6,0)[SNew(SBorder).Padding(8)[SAssignNew(ProductionLane,SVerticalBox)]]
		+SHorizontalBox::Slot().FillWidth(1)[SNew(SBorder).Padding(8)[SAssignNew(LogisticsLane,SVerticalBox)]]]
	+SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[SNew(SBorder).Padding(6)[SAssignNew(ValidationRows,SVerticalBox)]]];
}

void SHansaResearchGraphPanel::Refresh(const TArray<TSharedPtr<FHansaDefinitionListItem>>& Definitions)
{
	TArray<Hansa::Simulation::FHansaCompiledTechnologyDefinition> Technologies;
	TSet<FString> KnownIds;
	for (const TSharedPtr<FHansaDefinitionListItem>& Item : Definitions)
	{
		if (!Item.IsValid() || !Item->Definition.IsValid()) continue;
		KnownIds.Add(Item->Definition->StableDefinitionId);
		if (const UHansaTechnologyDefinition* Authored = Cast<UHansaTechnologyDefinition>(Item->Definition.Get()))
		{
			Hansa::Simulation::FHansaCompiledTechnologyDefinition Technology;
			Technology.StableId = Authored->StableDefinitionId;
			Technology.DisplayName = Authored->DisplayName.ToString();
			Technology.Branch = static_cast<Hansa::Simulation::EHansaResearchBranch>(Authored->Branch);
			Technology.PrerequisiteTechnologyIds = Authored->PrerequisiteTechnologyIds;
			Technology.CostResearchPoints = Authored->CostResearchPoints;
			Technology.DurationTicks = Authored->DurationTicks;
			for (const FHansaResearchEffectDefinition& Effect : Authored->Effects)
			{
				Technology.Effects.Add({static_cast<Hansa::Simulation::EHansaResearchEffectKind>(Effect.Kind),Effect.TargetStableId,Effect.Magnitude});
			}
			Technologies.Add(MoveTemp(Technology));
		}
	}
	Technologies.Sort([](const auto& Left,const auto& Right){return Left.StableId<Right.StableId;});
	NodeIds.Reset(); for(const auto& Technology:Technologies) NodeIds.Add(Technology.StableId);
	const TArray<FString> Roots={TEXT("Technology.Commerce.MarketReports"),TEXT("Technology.Production.ImprovedMilling"),TEXT("Technology.Logistics.WarehouseHandling")};
	Diagnostics=Hansa::Simulation::FHansaResearchGraphValidator::Validate(Technologies,Roots,KnownIds);
	RebuildLane(*CommerceLane,Hansa::Simulation::EHansaResearchBranch::Commerce,Technologies);
	RebuildLane(*ProductionLane,Hansa::Simulation::EHansaResearchBranch::Production,Technologies);
	RebuildLane(*LogisticsLane,Hansa::Simulation::EHansaResearchBranch::Logistics,Technologies);
	ValidationRows->ClearChildren();
	ValidationRows->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Validation (%d) · missing nodes / cycles / unreachable content"),Diagnostics.Num())))];
	for(const auto& Diagnostic:Diagnostics)
	{
		ValidationRows->AddSlot().AutoHeight().Padding(0,3)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("⚠ %s · %s · Remedy: %s"),*Diagnostic.TechnologyId,*Diagnostic.Cause,*Diagnostic.Remedy))).AutoWrapText(true)];
	}
	if(Diagnostics.IsEmpty()) ValidationRows->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("✓ Graph valid: all nine nodes reachable and acyclic.")))];
}

void SHansaResearchGraphPanel::RebuildLane(SVerticalBox& Lane,const Hansa::Simulation::EHansaResearchBranch Branch,
	const TArray<Hansa::Simulation::FHansaCompiledTechnologyDefinition>& Technologies)
{
	Lane.ClearChildren(); const TCHAR* Name=Branch==Hansa::Simulation::EHansaResearchBranch::Commerce?TEXT("COMMERCE"):Branch==Hansa::Simulation::EHansaResearchBranch::Production?TEXT("PRODUCTION"):TEXT("LOGISTICS");
	Lane.AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Name)).Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))];
	for(const auto& Technology:Technologies) if(Technology.Branch==Branch)
	{
		Lane.AddSlot().AutoHeight().Padding(0,5)[SNew(SBorder).Padding(7)[SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Technology.DisplayName))]
		+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Technology.StableId)).Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))]
		+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Technology.PrerequisiteTechnologyIds.IsEmpty()?TEXT("Root node"):TEXT("↑ ")+FString::Join(Technology.PrerequisiteTechnologyIds,TEXT(", ")))).AutoWrapText(true)]]];
	}
}

TArray<FString> SHansaResearchGraphPanel::GetControllerFocusOrder() const
{
	TArray<FString> Result; for(const FString& Id:NodeIds){FString Safe=Id;Safe.ReplaceInline(TEXT("."),TEXT("_"));Result.Add(TEXT("Authoring.Research.Node.")+Safe);}return Result;
}

