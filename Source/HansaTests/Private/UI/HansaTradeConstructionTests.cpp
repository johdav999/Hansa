#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HansaTradeSpecializationTestSupport.h"
#include "UI/HansaTradeConstruction.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "Placement/HansaRostockPlacement.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeConstructionBrowserTest,"Hansa.UI.TradeMap.Construction.Browser",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeConstructionBrowserTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareSpecialization(*Host,Error)){AddError(Error);return false;}
 const auto P=Host->BuildProjection();const auto& R=*Host->GetEconomicRegistry();
 auto View=Hansa::UI::BuildTradeConstruction(P.Value,R,Host->GetHouseId(),TEXT("City.Rostock"),0,NAME_None);
 TestEqual(TEXT("One owned lease"),View.Plots.Num(),1);
 TestEqual(TEXT("Policy-compatible definitions only"),View.Options.Num(),3);
 TestTrue(TEXT("Office cannot claim quarter construction"),View.Options.ContainsByPredicate([](const auto& O){return !O.bPermitted&&O.Reason.ToString().Contains(TEXT("Merchant quarter"));}));
 TestTrue(TEXT("Bounds are inclusive and space is not promised placeable"),View.Plots[0].Summary.ToString().Contains(TEXT("144"))&&View.Plots[0].Summary.ToString().Contains(TEXT("fragmented")));
 TestTrue(TEXT("Material costs expose available station inventory"),View.Options[0].Detail.ToString().Contains(TEXT("available excludes reservations")));
 auto Rival=Hansa::UI::BuildTradeConstruction(P.Value,R,Host->GetRivalHouseId(),TEXT("City.Rostock"),1,TEXT("Building.Market"));
 TestTrue(TEXT("Rival cannot browse owned plots or stock"),Rival.Plots.IsEmpty()&&Rival.Options.IsEmpty());
 auto Unknown=Hansa::UI::BuildTradeConstruction(P.Value,R,FHansaHouseId(),TEXT("City.Rostock"),1,NAME_None);
 TestTrue(TEXT("Unknown viewer is unavailable, not zero"),Unknown.Plots.IsEmpty()&&Unknown.Status.ToString().Contains(TEXT("unavailable")));
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());M->InitializeDefaults();M->BindRuntime(Host.Get());M->ApplyProjection(P.Value,R);M->Open();M->SelectCityIntent(TEXT("City.Rostock"));
 auto UI=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get());
 TestTrue(TEXT("Expansion is a native tab"),UI->ActivateSemanticId(TEXT("TradeMap.Navigate.Construction")));
 TestTrue(TEXT("Can select exact lease"),UI->ActivateSemanticId(TEXT("TradeMap.Construction.Plot.1")));
 TestTrue(TEXT("Can inspect locked building"),UI->ActivateSemanticId(TEXT("TradeMap.Construction.Building.Warehouse")));
 const auto Widget=UI->ResolveSemanticWidget(TEXT("TradeMap.Construction.Building.Warehouse"));
 M->ApplyProjection(P.Value,R);
 TestTrue(TEXT("Refresh retains card identity"),Widget==UI->ResolveSemanticWidget(TEXT("TradeMap.Construction.Building.Warehouse")));
 TestEqual(TEXT("Refresh retains building choice"),M->GetConstructionPresentation().SelectedBuilding,FName(TEXT("Building.Warehouse")));
 TestFalse(TEXT("Forged building rejected"),M->ConstructionIntent(TEXT("Building.Residence.Laborer")));
 TestFalse(TEXT("Forged lease rejected"),M->ConstructionIntent(TEXT("Plot.999")));
 TestFalse(TEXT("Disconnected confirmation never mutates"),M->ConstructionIntent(TEXT("Confirm")));
 TestEqual(TEXT("Browsing and invalid actions conserve all state"),Host->BuildProjection().Value.GetFingerprint().Value,P.Value.GetFingerprint().Value);
 for(FIntPoint Size:{FIntPoint(1280,720),FIntPoint(1920,1080)}){UI->SetPresentationSize(Size);UI->ActivateSemanticId(TEXT("TradeMap.Navigate.Construction"));TestTrue(TEXT("Construction choices are in controller order"),UI->GetControllerFocusOrder().Contains(TEXT("TradeMap.Construction.Building.Warehouse")));}
 TestTrue(TEXT("City switch accepted"),M->SelectCityIntent(TEXT("City.Lubeck")));
 TestTrue(TEXT("City switch clears stale foreign choice"),M->GetConstructionPresentation().Plots.IsEmpty()&&M->GetConstructionPresentation().SelectedBuilding.IsNone());
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeConstructionReportsTest,"Hansa.UI.TradeMap.Construction.Reports",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeConstructionReportsTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareSpecialization(*Host,Error,[](auto& Init){
  auto Extra=Init.LeasedPlots[0];Extra.Id=FHansaLeasedPlotId::TryCreate(2).Value;Extra.BoundsMin={24,8};Extra.BoundsMax={31,19};Extra.PermittedBuildingCategories={TEXT("Storage")};Init.LeasedPlots.Add(Extra);
 })){AddError(Error);return false;}
 const auto Projection=Host->BuildProjection();const auto& Registry=*Host->GetEconomicRegistry();
 const auto Local=Hansa::UI::BuildTradeConstruction(Projection.Value,Registry,Host->GetHouseId(),TEXT("City.Rostock"),2,TEXT("Building.Warehouse"));
 TestEqual(TEXT("Secondary leases are included"),Local.Plots.Num(),2);
 TestEqual(TEXT("Selected lease controls allowed definitions"),Local.Options.Num(),2);
 TestFalse(TEXT("Storage-only plot excludes commercial Dock"),Local.Options.ContainsByPredicate([](const auto& O){return O.Id==TEXT("Building.Dock");}));
 TestFalse(TEXT("Authored site ID is not player-facing"),Local.Plots[0].Summary.ToString().Contains(TEXT("TradeStationSite.")));
 const auto Missing=Hansa::UI::BuildTradeConstruction(Projection.Value,Registry,Host->GetHouseId(),TEXT("City.Rostock"),999,TEXT("Building.Warehouse"));
 TestEqual(TEXT("Lost lease never silently selects another"),Missing.SelectedLease,uint64(0));
 TestTrue(TEXT("Lost lease clears options and explains recovery"),Missing.Options.IsEmpty()&&Missing.Status.ToString().Contains(TEXT("no longer")));
 FHansaMultiplayerAuthority Authority;if(!Authority.Initialize(*Host))return false;FHansaClientInterest Interest;
 if(!Authority.RegisterAdmittedClient({913,FHansaParticipantId::TryCreate(1913).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)||
 !Authority.RegisterAdmittedClient({914,FHansaParticipantId::TryCreate(1914).Value,Host->GetRivalHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
 FHansaClientProjectionSnapshot Wire,Rival;
 if(!Authority.BuildProjection(913,0,true,Wire,Error)||!Authority.BuildProjection(914,0,true,Rival,Error)){AddError(Error);return false;}
 TestFalse(TEXT("Owner receives presence reports"),Wire.Presences.IsEmpty());
 for(const auto& Presence:Rival.Presences)for(const auto& Report:Presence.ConstructionReports)TestTrue(TEXT("Rival receives no private leases or materials"),Report.Plots.IsEmpty()&&Report.Options.IsEmpty());
 TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Writer,&Wire,nullptr);
 FHansaClientProjectionSnapshot Copy;FMemoryReader Reader(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Reader,&Copy,nullptr);
 TestFalse(TEXT("Construction reports survive wire serialization"),Reader.IsError());
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->ApplyRemoteEstablishment(Copy);Model->Open();Model->SelectCityIntent(TEXT("City.Rostock"));
 auto UI=SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get());UI->ActivateSemanticId(TEXT("TradeMap.Navigate.Construction"));
 TestTrue(TEXT("Remote secondary plot selectable"),UI->ActivateSemanticId(TEXT("TradeMap.Construction.Plot.2")));
 TestTrue(TEXT("Remote building selectable"),UI->ActivateSemanticId(TEXT("TradeMap.Construction.Building.Warehouse")));
 TestEqual(TEXT("Remote report matches local costs and permissions"),Model->GetConstructionPresentation().Key(),Local.Key());
 const auto Card=UI->ResolveSemanticWidget(TEXT("TradeMap.Construction.Building.Warehouse"));Model->ApplyRemoteEstablishment(Copy);
 TestTrue(TEXT("Remote refresh retains card identity"),Card==UI->ResolveSemanticWidget(TEXT("TradeMap.Construction.Building.Warehouse")));
 Model->VisitRequested=[&](FName City){Model->CloseIntent();return City==TEXT("City.Rostock");};
 TestFalse(TEXT("Remote world visit is explicitly unavailable"),Model->ConstructionIntent(TEXT("Visit")));
 TestFalse(TEXT("Remote visit explains unsupported world session"),Model->RemoteActionReason(TEXT("TradeMap.Construction.Visit")).IsEmpty());
 for(auto& Presence:Copy.Presences)for(auto& Report:Presence.ConstructionReports)Report.Plots.RemoveAll([](const auto& Plot){return Plot.Id==2;});
 for(auto& Presence:Copy.Presences)Presence.ConstructionReports.RemoveAll([](const auto& Report){return Report.SelectedLease==2;});
 ++Copy.Revision;Model->ApplyRemoteEstablishment(Copy);
 TestTrue(TEXT("Removed remote lease invalidates retained choice"),Model->GetConstructionPresentation().Options.IsEmpty());
 TestFalse(TEXT("Removed remote plot cannot be selected"),Model->ConstructionIntent(TEXT("Plot.2")));
 Model->ApplyRemoteEstablishment(Rival);
 TestTrue(TEXT("Viewer change clears private selection"),Model->GetConstructionPresentation().Plots.IsEmpty()&&Model->GetConstructionPresentation().SelectedBuilding.IsNone());
 TestEqual(TEXT("Browsing preserves authority"),Host->BuildProjection().Value.GetFingerprint().Value,Projection.Value.GetFingerprint().Value);
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeConstructionPermissionsTest,"Hansa.UI.TradeMap.Construction.Permissions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeConstructionPermissionsTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)){AddError(Error);return false;}
 const auto* Registry=Host->GetEconomicRegistry();
 const auto* Quarter=Registry->FindPresenceStage(TEXT("PresenceStage.MerchantQuarter"));
 if(!TestNotNull(TEXT("Authored quarter exists"),Quarter))return false;
 for(int32 State=0;State<3;++State)
 {
  if(!Hansa::Tests::PrepareSpecialization(*Host,Error,[&](auto& Init){
   for(auto& Presence:Init.ForeignPresences)if(Presence.HouseId==Host->GetHouseId()&&Presence.CityId.ToString()==TEXT("City.Rostock")){Presence.CurrentStageId=Quarter->StableId;Presence.GrantedCapabilityIds=Quarter->GrantedCapabilityIds;}
   if(State==1)for(auto& Lease:Init.LeasedPlots)Lease.bActive=false;
   if(State==2)for(auto& House:Init.Houses)if(House.Id==Host->GetHouseId())House.Money=FHansaMoney::FromRaw(0);
  })){AddError(Error);return false;}
  const auto P=Host->BuildProjection();const auto V=Hansa::UI::BuildTradeConstruction(P.Value,*Registry,Host->GetHouseId(),TEXT("City.Rostock"),1,NAME_None);
  if(State==0)
  {
   TestTrue(TEXT("Active quarter has permitted choices"),V.Options.ContainsByPredicate([](const auto& O){return O.bPermitted;}));
   auto Restricted=*Registry;auto Stages=Registry->GetPresenceStages();
   for(auto& Stage:Stages)if(Stage.StableId==Quarter->StableId)Stage.PermittedBuildingCategories={TEXT("Commercial")};
   Restricted.SetPresenceDefinitions(Registry->GetPresenceCapabilities(),MoveTemp(Stages),Registry->GetCityTradePolicies());
   const auto Limited=Hansa::UI::BuildTradeConstruction(P.Value,Restricted,Host->GetHouseId(),TEXT("City.Rostock"),1,NAME_None);
   const auto* Warehouse=Limited.Options.FindByPredicate([](const auto& O){return O.Id==TEXT("Building.Warehouse");});
   TestTrue(TEXT("Stage category denial overrides the lease category"),Warehouse&&!Warehouse->bPermitted&&Warehouse->Reason.ToString().Contains(TEXT("category")));
  }
  if(State==1)TestFalse(TEXT("Suspended leases grant no building choices"),V.Options.ContainsByPredicate([](const auto& O){return O.bPermitted;}));
  if(State==2)TestFalse(TEXT("Empty treasury is not affordable"),V.Options.ContainsByPredicate([](const auto& O){return O.bAffordable;}));
  TestEqual(TEXT("Read-only permission review conserves state"),Host->BuildProjection().Value.GetFingerprint().Value,P.Value.GetFingerprint().Value);
 }
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeConstructionWorldTest,"Hansa.UI.TradeMap.Construction.WorldPlacement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeConstructionWorldTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)){AddError(Error);return false;}
 const auto* Q=Host->GetEconomicRegistry()->FindPresenceStage(TEXT("PresenceStage.MerchantQuarter"));
 if(!Hansa::Tests::PrepareSpecialization(*Host,Error,[&](auto& Init){for(auto& P:Init.ForeignPresences)if(P.HouseId==Host->GetHouseId()&&P.CityId.ToString()==TEXT("City.Rostock")){P.CurrentStageId=Q->StableId;P.GrantedCapabilityIds=Q->GrantedCapabilityIds;}})){AddError(Error);return false;}
 const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
 const auto* Map=Host->FindPlacementMap(City);if(!TestNotNull(TEXT("Rostock authority map"),Map))return false;
 TestTrue(TEXT("Versioned canonical quarter"),RostockPlacement::IsCanonicalMap(*Map));
 for(const auto C:Map->PublicRoadCells){TestTrue(TEXT("Public road cell is protected"),Map->Cells.FindByPredicate([&](const auto& V){return V.Coordinate==C;})->bBlocked);TestTrue(TEXT("World/cell roundtrip"),RostockPlacement::WorldToCell(RostockPlacement::CellCenter(C.X,C.Y))==C);}
 TStrongObjectPtr<UHansaBuildMenuPresentationModel> Build(NewObject<UHansaBuildMenuPresentationModel>());
 if(!Build->InitializeForLubeck(nullptr,Host.Get(),Error)){AddError(Error);return false;}
 if(!TestTrue(TEXT("Selected lease enters world placement"),Build->BeginLeasedPlacement(TEXT("City.Rostock"),1,TEXT("Building.Warehouse"))))return false;
 TestFalse(TEXT("Home-only buildings cannot leak into foreign placement"),Build->SelectBuilding(TEXT("Building.Road")));
 TestTrue(TEXT("Preview vacant plot beside municipal road"),Build->TargetGridCell(9,8));
 if(!TestTrue(*Build->GetSnapshot().ValidationCause.ToString(),Build->GetSnapshot().bCanConfirm))return false;
 const auto Before=Host->BuildProjection().Value.GetFingerprint().Value;
 Build->TargetGridCell(8,9);TestFalse(TEXT("Civic street cannot be built over"),Build->ConfirmIntent());
 Build->TargetGridCell(19,19);TestFalse(TEXT("Cross-boundary placement rejected"),Build->ConfirmIntent());
 TestEqual(TEXT("Rejected actions spend nothing"),Before,Host->BuildProjection().Value.GetFingerprint().Value);
 Build->TargetGridCell(9,8);if(!TestTrue(TEXT("Authority commits foreign building"),Build->ConfirmIntent()))return false;
 const auto After=Host->BuildProjection();
 TestTrue(TEXT("Committed occupancy belongs to Rostock"),After.Value.GetPlacements().ContainsByPredicate([&](const auto& P){return P.Spec.CityId==City&&P.Spec.Anchor==FHansaGridCoordinate{9,8};}));
 Build->TargetGridCell(9,8);TestFalse(TEXT("Occupied footprint rejected"),Build->ConfirmIntent());
 TArray<uint8> Bytes;TestTrue(TEXT("Placed foreign building saves"),Host->CaptureSaveBytes(Bytes,TEXT("Rostock"),TEXT("2026-09-23T00:00:00Z")).IsSuccess());
 TestTrue(TEXT("Placed foreign building reloads"),Host->RestoreSaveBytes(Bytes).IsSuccess());
 TestEqual(TEXT("Placement survives exact roundtrip"),After.Value.GetFingerprint().Value,Host->BuildProjection().Value.GetFingerprint().Value);
 Build->SetPlacementCity(TEXT("City.Lubeck"));TestEqual(TEXT("Returning home clears foreign context"),Build->GetPlacementCity(),FName(TEXT("City.Lubeck")));
 return !HasAnyErrors();
}
#endif
