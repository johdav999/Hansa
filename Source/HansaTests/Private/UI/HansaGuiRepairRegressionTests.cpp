#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION
#include "Misc/AutomationTest.h"
#include "Fixtures/HansaProductionFixture.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaUiComponents.h"
#include "UI/HansaUiStyle.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuiRuntimeStatusRegression,"Hansa.UI.GuiRepair.LiveStatus",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGuiRuntimeStatusRegression::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 auto Created=FHansaProductionFixture::TryCreateGrainShortage();
 if(!TestTrue(TEXT("Production fixture is available"),Created.IsSuccess()))return false;
 auto Fixture=Created.Value;if(!Fixture.Step(5).IsSuccess())return false;
 auto Projection=Fixture.BuildProjection();if(!Projection)return false;
 TStrongObjectPtr<UHansaHudPresentationModel> Model(NewObject<UHansaHudPresentationModel>());Model->InitializeDefaults();
 const auto& P=Projection.Value;const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
 const auto& House=P.GetHouses()[0];
 const auto* Population=P.GetCityPopulations().FindByPredicate([&](const auto& C){return C.CityId==City;});
 if(!TestNotNull(TEXT("City population is projected"),Population))return false;
 Model->ApplyRuntimeStatus(P,City,House.Id);
 const auto& S=Model->GetSnapshot();
 TestTrue(TEXT("HUD population comes from the same runtime projection as City Overview"),S.Population.ToString().Contains(FText::AsNumber(Population->TotalResidents).ToString()));
 TestFalse(TEXT("Demo population is absent"),S.Population.ToString().Contains(TEXT("1,284")));
 TestTrue(TEXT("Unreported income trend is not fabricated"),S.MoneyTrend.IsEmpty());
 TestTrue(TEXT("Calendar displays simulation day and time"),S.DateAndSeason.ToString().Contains(TEXT(":")));
 const uint64 Revision=Model->GetRevision();Model->ApplyRuntimeStatus(P,City,House.Id);
 TestEqual(TEXT("Identical runtime values cause no redundant HUD rebuild"),Model->GetRevision(),Revision);
 Model->ApplyRuntimeStatus(P,FHansaCityDefinitionId(),FHansaHouseId());
 TestTrue(TEXT("Missing house explicitly reports unavailable data"),Model->GetSnapshot().Money.ToString().Contains(TEXT("unavailable")));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuiLockedMiddayClockRegression,"Hansa.UI.GuiRepair.LockedMiddayClock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGuiLockedMiddayClockRegression::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 auto Created=FHansaProductionFixture::TryCreateGrainShortage();
 if(!TestTrue(TEXT("Production fixture is available"),Created.IsSuccess()))return false;
 auto Fixture=Created.Value;if(!Fixture.Step(23).IsSuccess())return false;
 auto Projection=Fixture.BuildProjection();if(!Projection)return false;
 const auto& P=Projection.Value;
 TestEqual(TEXT("Authoritative fixture time still advances beneath presentation"),P.GetCalendar().HourOfDay,uint8(23));
 TStrongObjectPtr<UHansaHudPresentationModel> Model(NewObject<UHansaHudPresentationModel>());Model->InitializeDefaults();
 const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
 Model->ApplyRuntimeStatus(P,City,P.GetHouses()[0].Id);
 const FString Display=Model->GetSnapshot().DateAndSeason.ToString();
 TestTrue(TEXT("HUD preserves the authoritative simulation day"),Display.Contains(FText::AsNumber(P.GetCalendar().ElapsedDays+1).ToString()));
 TestTrue(TEXT("HUD time remains locked to noon"),Display.Contains(TEXT("12:00")));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuiActionGeometryRegression,"Hansa.UI.GuiRepair.ActionGeometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGuiActionGeometryRegression::RunTest(const FString&)
{
 using namespace Hansa::UI;
 for(float Scale:{.8f,1.f,1.4f}){
  FUiPreferences Preferences;Preferences.UiScale=Scale;
  auto Button=SNew(SHansaAction).Preferences(Preferences).Label(FText::FromString(TEXT("Pause")));
  float FirstHeight=0;
  for(EUiState State:{EUiState::Default,EUiState::Selected,EUiState::Disabled,EUiState::Loading}){
   Button->SetState(State,FText::FromString(TEXT("State explanation")));Button->SlatePrepass();
   const float Height=Button->GetDesiredSize().Y;
   if(FirstHeight==0)FirstHeight=Height;
   TestTrue(TEXT("A state change does not reserve another caption row"),FMath::IsNearlyEqual(Height,FirstHeight,.1f));
   TestTrue(TEXT("Target includes padding and remains at least 48 physical pixels"),Height*Scale>=47.9f);
   TestTrue(TEXT("Target is not inflated by double padding"),Height<=48.f/FMath::Min(1.f,Scale)+.1f);
  }
 }
 TestTrue(TEXT("Body point size maps to the brief's 16 native pixels"),FMath::IsNearlyEqual(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Body).Size*96.f/72.f,16.f));
 return !HasAnyErrors();
}
#endif
