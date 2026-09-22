from pathlib import Path
exec((Path(__file__).parent / 'ImplementFirewood.py').read_text().split('def compiled(t):')[0])

def inventory_h(t):
    t=replace(t,'\t\tArithmeticOverflow\n','\t\tArithmeticOverflow,\n\t\tHouseholdReserveProtected\n')
    marker='\tclass HANSASIMULATION_API FHansaInventoryLedger final'
    data='''	/** Derived withdrawal floor for one actual household consumption pool. Not physical stock. */
	struct FHansaHouseholdStockProtection
	{
		FHansaInventoryId InventoryId;
		FHansaGoodId GoodId;
		int64 TargetRaw = 0;
	};

'''
    t=replace(t,marker,data+marker)
    marker='\t\t[[nodiscard]] bool IsValid() const { return bInitialized; }'
    t=replace(t,marker,marker+'''
		void SetHouseholdProtection(TArray<FHansaHouseholdStockProtection> Values) { HouseholdProtection = MoveTemp(Values); }
		[[nodiscard]] int64 ProtectedRaw(FHansaInventoryId InventoryId, FHansaGoodId GoodId) const;
''')
    marker='\t\tvoid AddRecentMovement(FHansaInventoryMovement Movement);'
    t=replace(t,marker,marker+'\n\t\t// Rebuilt from current cohorts/calendar/policy before mutations; excluded from saves/hash.\n\t\tTArray<FHansaHouseholdStockProtection> HouseholdProtection;')
    marker='\t\t[[nodiscard]] int32 GetInventoryCount() const;'
    t=replace(t,marker,marker+'\n\t\t[[nodiscard]] int64 QueryProtectedRaw(FHansaInventoryId InventoryId, FHansaGoodId GoodId) const;')
    return t
edit('Source/HansaSimulation/Public/Inventory/HansaInventory.h',inventory_h)
def inventory_cpp(t):
    t=t.replace('case EHansaInventoryTransactionError::ArithmeticOverflow:', 'case EHansaInventoryTransactionError::HouseholdReserveProtected: return TEXT("HouseholdReserveProtected");\n\t\tcase EHansaInventoryTransactionError::ArithmeticOverflow:',1)
    marker='\tFHansaInventoryTransactionResult FHansaInventoryLedger::TryTransfer('
    methods='''	int64 FHansaInventoryLedger::ProtectedRaw(FHansaInventoryId InventoryId, FHansaGoodId GoodId) const
	{
		const auto* Floor = HouseholdProtection.FindByPredicate([&](const auto& V) { return V.InventoryId == InventoryId && V.GoodId == GoodId; });
		return Floor ? FMath::Max<int64>(0, Floor->TargetRaw) : 0;
	}

	int64 FHansaInventoryReadOnlyAccess::QueryProtectedRaw(FHansaInventoryId InventoryId, FHansaGoodId GoodId) const
	{
		return Ledger ? Ledger->ProtectedRaw(InventoryId, GoodId) : 0;
	}

'''
    t=replace(t,marker,methods+marker)
    marker='''				if (!Available || Available.Value.GetRawValue() < Quantity.GetRawValue())'''
    t=replace(t,marker,'''				const bool bHouseholdConsumption = Destination.Kind == EHansaInventoryEndpointKind::ExplicitSink &&
					Destination.ExternalEndpointId == TEXT("PopulationConsumption");
				if (Available && !bHouseholdConsumption && Available.Value.GetRawValue() >= Quantity.GetRawValue() &&
					Available.Value.GetRawValue() - Quantity.GetRawValue() < ProtectedRaw(Source.InventoryId, GoodId))
					return Failure(EHansaInventoryTransactionError::HouseholdReserveProtected, Sequence, Quantity);
'''+marker)
    marker='''		const THansaValueResult<FHansaQuantity> Available = FHansaQuantity::TrySubtract(Stock.Quantity, Stock.Reserved);
		if (!Available'''
    t=replace(t,marker,'''		const THansaValueResult<FHansaQuantity> Available = FHansaQuantity::TrySubtract(Stock.Quantity, Stock.Reserved);
		if (Available && Available.Value.GetRawValue() >= Quantity.GetRawValue() &&
			Available.Value.GetRawValue() - Quantity.GetRawValue() < ProtectedRaw(InventoryId, GoodId))
			return Failure(EHansaInventoryTransactionError::HouseholdReserveProtected, Sequence, Quantity);
		if (!Available''')
    return t
edit('Source/HansaSimulation/Private/Inventory/HansaInventory.cpp',inventory_cpp)
def city(t):
    return replace(t,'\t\tFHansaQuantity AggregateStock;','''		FHansaQuantity AggregateStock;
		// -1 takes the authored city default. An explicit override persists until changed.
		int32 HeatingReserveDays = -1;
		bool bReleaseHeatingReserve = false;''')
edit('Source/HansaSimulation/Public/Model/HansaSimulationState.h',city)
edit('Source/HansaSimulation/Private/Model/HansaSimulationState.cpp',lambda t:replace(t,'if (City.AggregateStock.GetRawValue() < 0)','if (City.AggregateStock.GetRawValue() < 0 || City.HeatingReserveDays < -1 || City.HeatingReserveDays > 90)'))
edit('Source/HansaSimulation/Private/Save/HansaSaveFields.inl',lambda t:replace(t,'\tValue(V.AggregateStock);','\tValue(V.AggregateStock);\n\tif (FormatVersion >= 8) { Value(V.HeatingReserveDays); Value(V.bReleaseHeatingReserve); }'))
edit('Source/HansaSimulation/Public/Save/HansaSaveEnvelope.h',lambda t:replace(t,'CurrentFormatVersion = 7','CurrentFormatVersion = 8'))
edit('Source/HansaSimulation/Private/Diagnostics/HansaStateHash.cpp',lambda t:replace(t,'Builder.AddInt64(City.AggregateStock.GetRawValue());','''Builder.AddInt64(City.AggregateStock.GetRawValue());
					if (City.HeatingReserveDays != -1 || City.bReleaseHeatingReserve)
					{
						Builder.AddInt64(City.HeatingReserveDays);
						Builder.AddInt64(City.bReleaseHeatingReserve ? 1 : 0);
					}'''))
print('Inventory protection and persisted city policy applied.')
