for (FHansaForeignPresenceState& Presence : Candidate.ForeignPresences)
{
 if (Presence.Upgrade.Status != EHansaPresenceUpgradeStatus::Funded || Candidate.Clock.GetTick().GetValue() < Presence.Upgrade.CompletionTick.GetValue()) continue;
 const auto* Policy=Registry->FindCityTradePolicyForCity(Presence.CityId.ToString());const auto* Target=Registry->FindPresenceStage(Presence.Upgrade.TargetStageId);const auto* Previous=Registry->FindPresenceStage(Presence.CurrentStageId);
 if(!Policy||!Target||!Previous||!Registry->IsValidPresenceTransition(Presence.CityId.ToString(),Presence.CurrentStageId,Target->StableId))return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext);
 const bool bGainsOffice=!Previous->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.MerchantOffice"))&&Target->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.MerchantOffice"));
 if(bGainsOffice&&Policy->MerchantOfficeStorageBonusMilliUnits>0)
 {
  const auto* Station=Candidate.TradeStations.FindByPredicate([&](const auto& V){return V.Id==Presence.StationId&&V.OwnerId==Presence.HouseId;});const auto Inventory=Station?Candidate.InventoryLedger.CreateReadOnlyAccess().QueryInventory(Station->InventoryId):TOptional<FHansaInventoryProjection>();
  const auto NewCapacity=Inventory?FHansaCheckedIntegerMath::TryAdd(Inventory->Capacity.GetRawValue(),Policy->MerchantOfficeStorageBonusMilliUnits):THansaValueResult<int64>::Failure(EHansaValueError::InvalidFormat);
  if(!Station||!NewCapacity||!Candidate.InventoryLedger.TrySetCapacity(Station->InventoryId,FHansaQuantity::FromRaw(NewCapacity.Value)))return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext);
 }
 Presence.CurrentStageId=Target->StableId;Presence.GrantedCapabilityIds=Target->GrantedCapabilityIds;
 for(const FString& Id:Presence.ActiveSpecializationIds)if(const auto* B=Policy->Specializations.FindByPredicate([&](const auto& V){return V.SpecializationId==Id;}))Presence.GrantedCapabilityIds.Append(B->GrantedCapabilityIds);
 Presence.GrantedCapabilityIds.RemoveAll([&](const FString& Id){return Policy->DeniedCapabilityIds.Contains(Id);});Presence.GrantedCapabilityIds.Sort();Presence.LastUpgradeTick=Candidate.Clock.GetTick();
 Presence.History.Add({EHansaPresenceHistoryKind::UpgradeCompleted,Candidate.Clock.GetTick(),++Candidate.PublishedDomainEventCount,Target->StableId,0,Presence.Upgrade.SpentMoneyPfennig});
 FHansaDomainEvent Completion;Completion.Type=EHansaDomainEventType::PresenceUpgradeCompleted;Completion.Tick=Candidate.Clock.GetTick();Completion.GlobalSequence=Candidate.PublishedDomainEventCount;Completion.IssuingHouseId=Presence.HouseId;Completion.CityId=Presence.CityId;Completion.Value=Target->Ordinal;PendingEvents.Add(MoveTemp(Completion));
 Presence.Upgrade=FHansaPresenceUpgradeState();
}for(FHansaForeignPresenceState& Presence:Candidate.ForeignPresences)for(FHansaCityProjectState& Project:Presence.CityProjects)if(Project.Status==EHansaCityProjectStatus::Funded&&Candidate.Clock.GetTick().GetValue()>=Project.CompletionTick.GetValue())
{
 const auto* Policy=Registry->FindCityTradePolicyForCity(Presence.CityId.ToString());const auto* Def=Policy?Policy->CityProjects.FindByPredicate([&](const auto& V){return V.ProjectId==Project.ProjectId;}):nullptr;const auto Good=Def?FHansaGoodId::TryParse(Def->SharedReserveGoodId):THansaValueResult<FHansaGoodId>::Failure(EHansaValueError::InvalidFormat);auto* Market=Good?Candidate.Markets.FindByPredicate([&](const auto& V){return V.CityId==Presence.CityId&&V.GoodId==Good.Value;}):nullptr;if(!Def||!Good||!Market)return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext);const auto Reserve=FHansaCheckedIntegerMath::TryAdd(Market->DesiredReserve.GetRawValue(),Def->SharedReserveBonusMilliUnits);if(!Reserve)return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext);Market->DesiredReserve=FHansaQuantity::FromRaw(Reserve.Value);Project.Status=EHansaCityProjectStatus::Completed;Project.bSharedEffectApplied=true;++Presence.AuthorityRevision;FHansaDomainEvent Completion;Completion.Type=EHansaDomainEventType::CityProjectCompleted;Completion.Tick=Candidate.Clock.GetTick();Completion.GlobalSequence=++Candidate.PublishedDomainEventCount;Completion.IssuingHouseId=Presence.HouseId;Completion.CityId=Presence.CityId;Completion.Value=Def->SharedReserveBonusMilliUnits;PendingEvents.Add(MoveTemp(Completion));
}
