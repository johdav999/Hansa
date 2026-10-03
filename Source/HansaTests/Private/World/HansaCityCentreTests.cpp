#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/HansaCityCentrePresentation.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCityCentreLayout,"Hansa.World.CityCentre.LayoutAndIndustry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCityCentreLayout::RunTest(const FString&)
{
 Hansa::Simulation::FHansaEconomicRegistry Registry;FString Error;
 if(!FHansaLubeckScenarioInitializer::TryLoadMvpRegistry(Registry,Error)){AddError(Error);return false;}
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);auto* Centre=World->SpawnActor<AHansaCityCentrePresentation>();
 TestTrue(TEXT("Authored profile valid and approved assets resolve"),Centre->ValidateLayout(Error));
 const auto* Profile=Registry.FindCityMarket(TEXT("City.Rostock"));if(!TestNotNull(TEXT("Rostock public production profile"),Profile)){World->DestroyWorld(false);return false;}
 TArray<FBox> Buildings;int32 Homes=0,Industries=0;
 for(const auto& S:Centre->Slots){
  TestTrue(*FString::Printf(TEXT("Profile supports %s"),*S.Id.ToString()),Centre->IsEnabled(S,Profile));
  if(S.Id.ToString().StartsWith(TEXT("Home.")))++Homes;
  if(S.bSelectable&&!S.ProductionChain.IsNone())++Industries;
  if(!S.bSelectable||!S.Mesh)continue;
  FBox B=S.Mesh->GetBoundingBox().TransformBy(FTransform(FRotator(0,S.Yaw,0),S.Location));
  TestTrue(TEXT("Municipal building avoids entire station lease"),B.Max.X<-1500||B.Min.X>3300||B.Max.Y<-5000||B.Min.Y>-200);
  for(const auto& Existing:Buildings)TestFalse(TEXT("Municipal buildings do not overlap"),B.IntersectXY(Existing));Buildings.Add(B);
 }
 TestEqual(TEXT("Twelve homes"),Homes,12);TestEqual(TEXT("Four native industries"),Industries,4);
 auto Disabled=*Profile;for(auto& Binding:Disabled.IndustryBindings)Binding.bEnabled=false;
 for(const auto& S:Centre->Slots)TestEqual(TEXT("Disabled industries disappear; civic scenery remains"),Centre->IsEnabled(S,&Disabled),S.ProductionChain.IsNone());
 Centre->ApplyCity(Registry);TArray<UStaticMeshComponent*> Before;Centre->GetComponents(Before);Centre->ApplyCity(Registry);
 TArray<UStaticMeshComponent*> After;Centre->GetComponents(After);TestEqual(TEXT("Repeated refresh never duplicates geometry"),After.Num(),Before.Num());
 FHitResult Hit;const FVector Market=Centre->GetMarketLocation();
 TestTrue(TEXT("Market selectable"),World->LineTraceSingleByChannel(Hit,Market+FVector(0,0,2000),Market,ECC_Visibility));
 const auto* Selected=Centre->FindSlot(Hit.GetComponent());TestTrue(TEXT("Stable selection identity"),Selected&&Selected->Id==TEXT("Market"));
 const auto Duplicate=Centre->Slots[0];Centre->Slots.Add(Duplicate);TestFalse(TEXT("Duplicate IDs rejected"),Centre->ValidateLayout(Error));Centre->Slots.Pop();
 Centre->LayoutVersion=2;TestFalse(TEXT("Unsupported layout migration rejected"),Centre->ValidateLayout(Error));
 World->DestroyWorld(false);return true;
}
#endif
