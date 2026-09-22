#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "UI/SHansaBuildMenu.h"
#include "UI/HansaConstructionTestSettings.h"
#include "InputCoreTypes.h"
#include "UObject/StrongObjectPtr.h"

using namespace Hansa::Simulation;
using namespace Hansa::UI;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaConstructionTierClassification,
 "Hansa.UI.ConstructionProgression.AuthoredClassification",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaConstructionTierClassification::RunTest(const FString&)
{
 FHansaCompiledBuildingDefinition Home;
 Home.StableId=TEXT("Building.TestHome"); Home.bShowInConstructionMenu=true;
 Home.ConstructionMenuCategory=TEXT("Residences"); Home.ResidentPopulationTierId=TEXT("PopulationTier.Artisan");
 auto Producer=Home; Producer.StableId=TEXT("Building.TestProducer"); Producer.ResidentPopulationTierId.Reset();
 Producer.ConstructionMenuCategory=TEXT("Production"); Producer.ConstructionChainOutputGoodId=TEXT("Good.TestFood");
 Producer.LaborerWorkforce=8; Producer.ArtisanWorkforce=2;
 auto LaborStage=Producer; LaborStage.StableId=TEXT("Building.TestLaborStage"); LaborStage.ArtisanWorkforce=0;
 auto ArtisanChain=Producer; ArtisanChain.StableId=TEXT("Building.TestArtisanChain");
 ArtisanChain.ConstructionChainOutputGoodId=TEXT("Good.TestCraft"); ArtisanChain.LaborerWorkforce=0;
 auto Shared=Producer; Shared.StableId=TEXT("Building.TestSupply"); Shared.ConstructionChainOutputGoodId=TEXT("Good.TestMaterial");
 Shared.LaborerWorkforce=0; Shared.ArtisanWorkforce=0;
 FHansaEconomicRegistry Registry({}, {}, {Home,Producer,LaborStage,ArtisanChain,Shared}, 123);
 TArray<FHansaBuildCardPresentation> Cards; TArray<FHansaBuildChainPresentation> Chains; FString Error;
 if (!TestTrue(TEXT("Authored catalogue builds"), UHansaBuildMenuPresentationModel::BuildCatalogFromDefinitions(Registry,{},Cards,Chains,Error))) return false;
 auto Mask=[&](const TCHAR* Id) { const auto* C=Cards.FindByPredicate([&](const auto& V){return V.StableId==Id;}); return C?C->BrowsingTierMask:-1; };
 TestEqual(TEXT("Residence uses its authored household tier"),Mask(TEXT("Building.TestHome")),2);
 TestEqual(TEXT("A labor-based chain appears only under Day Laborers"),Mask(TEXT("Building.TestProducer")),1);
 TestEqual(TEXT("Every stage stays with the chain''s lowest workforce tier"),Mask(TEXT("Building.TestLaborStage")),1);
 TestEqual(TEXT("A chain without laborers appears under Craftsmen"),Mask(TEXT("Building.TestArtisanChain")),2);
 TestEqual(TEXT("Construction supplies remain shared"),Mask(TEXT("Building.TestSupply")),7);
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaExplicitConstructionTier,
 "Hansa.UI.ConstructionProgression.ExplicitMixedWorkforce",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaExplicitConstructionTier::RunTest(const FString&)
{
 FHansaCompiledBuildingDefinition Burner;
 Burner.StableId=TEXT("Building.CharcoalBurner"); Burner.bShowInConstructionMenu=true;
 Burner.ConstructionMenuCategory=TEXT("Production"); Burner.ConstructionChainOutputGoodId=TEXT("Good.Tools");
 Burner.ConstructionTier=TEXT("DayLaborers"); Burner.LaborerWorkforce=2;
 auto Smith=Burner; Smith.StableId=TEXT("Building.Smithy"); Smith.ConstructionTier=TEXT("Craftsmen");
 Smith.ArtisanWorkforce=3; // Laborer helpers must not change its authored ownership.
 FHansaEconomicRegistry Registry({}, {}, {Burner,Smith}, 123);
 TArray<FHansaBuildCardPresentation> Cards; TArray<FHansaBuildChainPresentation> Chains; FString Error;
 if (!TestTrue(TEXT("Mixed chain catalog builds"),UHansaBuildMenuPresentationModel::BuildCatalogFromDefinitions(Registry,{},Cards,Chains,Error))) return false;
 for(const auto& Card:Cards)
  TestEqual(TEXT("Each card has exactly its own tier"),Card.BrowsingTierMask,Card.StableId==TEXT("Building.Smithy")?2:1);
 TestEqual(TEXT("One shared chain selector"),Chains.Num(),1);
 if(Chains.Num()==1) TestEqual(TEXT("Chain navigation reaches both tiers without duplicate building cards"),Chains[0].BrowsingTierMask,3);
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaConstructionTierJourney,
 "Hansa.UI.ConstructionProgression.SemanticJourney",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaConstructionTierJourney::RunTest(const FString&)
{
 FScopedHansaArtisanConstructionOverride Override(false);
 TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
 FString Error;
 if (!TestTrue(*FString::Printf(TEXT("Runtime catalogue initializes: %s"),*Error),Model->InitializeForLubeck(nullptr,Error))) {AddError(Error);return false;}
 auto Menu=SNew(SHansaBuildMenu).Model(Model.Get());
 TestTrue(TEXT("Open production"),Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Production")));
 TestTrue(TEXT("Labor production chains appear for Day Laborers"),Menu->ResolveSemanticWidget(TEXT("BuildMenu.Chain.Good_Bread"))->GetVisibility().IsVisible());
 TestTrue(TEXT("Switch production browsing to Craftsmen"),Menu->ActivateSemanticId(TEXT("BuildMenu.Tier.Craftsmen")));
 TestFalse(TEXT("Labor production chains are hidden from Craftsmen"),Model->IsChainVisible(TEXT("Good.Bread")));
 if (FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 28)
 {
 TestTrue(TEXT("Normal catalog offers Tools to Craftsmen without a candidate flag"),Model->IsChainVisible(TEXT("Good.Tools")));
 TestTrue(TEXT("Normal catalog offers Shoes to Craftsmen without a candidate flag"),Model->IsChainVisible(TEXT("Good.Shoes")));
 TestFalse(TEXT("Charcoal supply remains with Day Laborers"),Model->IsChainVisible(TEXT("Good.Charcoal")));
 TestTrue(TEXT("Open Tools through native action"),Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Tools")));
 TestTrue(TEXT("Smithy is available in its chain"),Model->IsCardVisible(TEXT("Building.Smithy")));
 TestTrue(TEXT("Open Shoes through native action"),Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Shoes")));
 TestTrue(TEXT("Tannery is available in Shoes"),Model->IsCardVisible(TEXT("Building.Tannery")));
 TestTrue(TEXT("Shoemaker is available in Shoes"),Model->IsCardVisible(TEXT("Building.Shoemaker")));

 }
 TestFalse(TEXT("Hidden labor chain rejects semantic activation"),Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Bread")));
 TestTrue(TEXT("Return to Day Laborers"),Menu->ActivateSemanticId(TEXT("BuildMenu.Tier.DayLaborers")));
 TestTrue(TEXT("Open residences"),Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Residences")));
 TestTrue(TEXT("Day Laborers is the initial tier"),Model->GetSnapshot().SelectedTier==EHansaBuildTier::DayLaborers);
 TestTrue(TEXT("Day laborer residence visible"),Model->IsCardVisible(TEXT("Building.Residence.Laborer")));
 TestFalse(TEXT("Craftsman residence is in its own tab"),Model->IsCardVisible(TEXT("Building.Residence.Artisan")));
 TestTrue(TEXT("Switch through ordinary semantic tier action"),Menu->ActivateSemanticId(TEXT("BuildMenu.Tier.Craftsmen")));
 TestTrue(TEXT("Craftsman home now visible"),Model->IsCardVisible(TEXT("Building.Residence.Artisan")));
 TestFalse(TEXT("Other residence hidden"),Model->IsCardVisible(TEXT("Building.Residence.Laborer")));
 TestFalse(TEXT("Tier browsing does not bypass upgrade-only lock"),Menu->ActivateSemanticId(TEXT("BuildMenu.Card.Building_Residence_Artisan")));
 TestTrue(TEXT("Merchant tier is browsable"),Menu->ActivateSemanticId(TEXT("BuildMenu.Tier.Merchants")));
 auto Empty=Menu->ResolveSemanticWidget(TEXT("BuildMenu.EmptyTier"));
 TestTrue(TEXT("Missing merchant residences have explicit empty state"),Empty.IsValid()&&Empty->GetVisibility().IsVisible());
 TestFalse(TEXT("Hidden cards cannot be activated"),Menu->ActivateSemanticId(TEXT("BuildMenu.Card.Building_Residence_Laborer")));
 TestTrue(TEXT("Category change preserves tier"),Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Roads"))&&Model->GetSnapshot().SelectedTier==EHansaBuildTier::Merchants);
 TestTrue(TEXT("Roads remain shared infrastructure"),Menu->ActivateSemanticId(TEXT("BuildMenu.Card.Building_Road")));
 TestTrue(TEXT("World targeting begins"),Model->TargetGridCell(10,10));
 TestTrue(TEXT("Tier change cancels active preview"),Menu->ActivateSemanticId(TEXT("BuildMenu.Tier.DayLaborers")));
 TestTrue(TEXT("No stale placement can be confirmed"),Model->GetSnapshot().SelectedBuildingId.IsNone()&&!Model->GetSnapshot().bCanConfirm&&Model->GetSnapshot().FootprintCells.IsEmpty());
 TestTrue(TEXT("Controller can focus Craftsmen"),Menu->FocusSemanticId(TEXT("BuildMenu.Tier.Craftsmen")));
 Menu->OnKeyDown(FGeometry(),FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
 TestTrue(TEXT("Controller confirm activates tier, not placement"),Model->GetSnapshot().SelectedTier==EHansaBuildTier::Craftsmen);
 Menu->SetPreferences({true,true,true});
 TestTrue(TEXT("Preferences retain selected tier and focus"),Model->GetSnapshot().SelectedTier==EHansaBuildTier::Craftsmen&&Menu->GetControllerFocusOrder().Contains(TEXT("BuildMenu.Tier.Craftsmen")));
 Model->SetOpen(false);
 TestFalse(TEXT("Closed tier cannot activate"),Menu->ActivateSemanticId(TEXT("BuildMenu.Tier.Merchants")));
 Model->SetConstructionAllowed(false);
 TestFalse(TEXT("Remote city rejects tier intent"),Model->SelectTier(EHansaBuildTier::Merchants));
 TestFalse(TEXT("Invalid enum is rejected"),Model->SelectTier(static_cast<EHansaBuildTier>(255)));
 return !HasAnyErrors();
}
#endif
