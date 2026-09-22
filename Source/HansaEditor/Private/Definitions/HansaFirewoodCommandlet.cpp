#include "Definitions/HansaFirewoodCommandlet.h"
#include "Algo/Reverse.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "HansaFirewoodPromotion.inl"

UHansaFirewoodCommandlet::UHansaFirewoodCommandlet() { IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true; }

int32 UHansaFirewoodCommandlet::Main(const FString& Params)
{
	IAssetRegistry& Assets = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Assets.ScanPathsSynchronous({TEXT("/Game/Hansa/Core")}, true);
	TArray<FAssetData> Rows; Assets.GetAssetsByPath(TEXT("/Game/Hansa/Core"), Rows, true, false);
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> Draft;
	TArray<const UHansaDefinitionBase*> Baseline;
	for (const auto& Row : Rows) if (const auto* D = Cast<UHansaDefinitionBase>(Row.GetAsset()))
	{
		Baseline.Add(D); Draft.Emplace(DuplicateObject<UHansaDefinitionBase>(D, GetTransientPackage()));
	}
	if(FParse::Param(*Params,TEXT("PromoteApproved")))return PromoteFirewood(false,Assets,Baseline);
	if(FParse::Param(*Params,TEXT("VerifyApproved")))return PromoteFirewood(true,Assets,Baseline);
	const auto Original = FHansaEconomicDefinitionCompiler::Compile(Baseline);
	if (!Original.IsValid() || Original.Registry.GetRegistryHash() != FHansaLubeckScenarioInitializer::MvpRegistryHash) return 1;
    // A separate process verifies serialized text identity and post-load hashes.
    // Transient draft compilation alone cannot pin newly saved FText definitions.
    if (Params.Contains(TEXT("VerifyStaged")))
    {
        Assets.ScanPathsSynchronous({TEXT("/Game/Hansa/Generated/Staging/Firewood")},true);
        TArray<FAssetData> StagedRows;Assets.GetAssetsByPath(TEXT("/Game/Hansa/Generated/Staging/Firewood"),StagedRows,true,false);
        TArray<const UHansaDefinitionBase*> Loaded;
        for(const auto& Row:StagedRows)if(const auto* D=Cast<UHansaDefinitionBase>(Row.GetAsset()))Loaded.Add(D);
        const auto Verified=FHansaEconomicDefinitionCompiler::Compile(Loaded);
        if(!Verified.IsValid() || Loaded.Num()!=Baseline.Num()+4)return 7;
        Algo::Reverse(Loaded);const auto Reverse=FHansaEconomicDefinitionCompiler::Compile(Loaded);
        if(!Reverse.IsValid() || Reverse.Registry.GetRegistryHash()!=Verified.Registry.GetRegistryHash())return 8;
        Loaded.Sort([](const auto& A,const auto& B){return A.StableDefinitionId<B.StableDefinitionId;});
        auto Manifest=MakeShared<FJsonObject>();Manifest->SetStringField(TEXT("status"),TEXT("draft-reload-verified-awaiting-review"));
        Manifest->SetStringField(TEXT("pendingPresentation"),TEXT("/Game/Hansa/Generated/Staging/FirewoodModel/Meshes/SM_WoodcutterYard"));
        Manifest->SetStringField(TEXT("baseRegistryHash"),FString::Printf(TEXT("%016llX"),Original.Registry.GetRegistryHash()));
        Manifest->SetStringField(TEXT("registryHash"),FString::Printf(TEXT("%016llX"),Verified.Registry.GetRegistryHash()));
        TArray<TSharedPtr<FJsonValue>> Entries;
        for(const auto* D:Loaded)
        {
            auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("stableId"),D->StableDefinitionId);E->SetStringField(TEXT("asset"),D->GetPathName());
            E->SetStringField(TEXT("hash"),FString::Printf(TEXT("%016llX"),D->ComputeDeterministicContentHash()));Entries.Add(MakeShared<FJsonValueObject>(E));
        }
        Manifest->SetArrayField(TEXT("definitions"),Entries);FString Json;FJsonSerializer::Serialize(Manifest,TJsonWriterFactory<>::Create(&Json));
        return FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectDir()/TEXT("Docs/Development/FirewoodCandidate.json")))?0:9;
    }
	auto Find = [&](const TCHAR* Id) -> UHansaDefinitionBase* {
		for (const auto& D : Draft) if (D->StableDefinitionId == Id) return D.Get(); return nullptr;
	};
	auto Clone = [&](const TCHAR* SourceId, const TCHAR* Id, const TCHAR* Label) -> UHansaDefinitionBase* {
		const auto* Source = Find(SourceId); if (!Source) return nullptr;
		auto* D = DuplicateObject<UHansaDefinitionBase>(Source, GetTransientPackage());
		D->StableDefinitionId = Id; D->DisplayName = FText::FromString(Label);
		D->LocalizationKey = FName(*(FString(TEXT("Game.")) + Id + TEXT(".Name")));
		D->AuthoredRevision = 1; Draft.Emplace(D); return D;
	};
	auto* Good = Cast<UHansaGoodDefinition>(Clone(TEXT("Good.Timber"), TEXT("Good.Firewood"), TEXT("Firewood")));
	auto* Recipe = Cast<UHansaRecipeDefinition>(Clone(TEXT("Recipe.MakeBarrels"), TEXT("Recipe.SplitFirewood"), TEXT("Split firewood")));
	auto* Yard = Cast<UHansaBuildingDefinition>(Clone(TEXT("Building.Cooperage"), TEXT("Building.WoodcutterYard"), TEXT("Woodcutter's yard")));
	auto* Heating = Cast<UHansaNeedDefinition>(Clone(TEXT("Need.Bread"), TEXT("Need.Heating"), TEXT("Heating")));
	if (!Good || !Recipe || !Yard || !Heating) return 2;
	auto Amount = [](const TCHAR* Id, int64 Quantity) { FHansaGoodAmount A; A.GoodId = Id; A.QuantityMilliUnits = Quantity; return A; };
	Good->BaseValueMilliMarks = 900; Good->SpoilageBasisPointsPerDay = 0; Good->Icon.Reset();
	Recipe->Inputs = {Amount(TEXT("Good.Timber"), 6'000)};
	Recipe->Outputs = {Amount(TEXT("Good.Firewood"), 5'400)};
	Recipe->CycleTicks = 100; Recipe->LaborerWorkforce = 2; Recipe->ArtisanWorkforce = 0;
	Yard->RecipeIds = {TEXT("Recipe.SplitFirewood")}; Yard->LaborerWorkforce = 2; Yard->ArtisanWorkforce = 0;
	Yard->ConstructionCosts = {Amount(TEXT("Good.Timber"), 6000), Amount(TEXT("Good.Planks"), 2000), Amount(TEXT("Good.Tools"), 500)};
	Yard->ConstructionCostPfennig = 1800; Yard->FootprintWidthCells = 3; Yard->FootprintHeightCells = 3;
	Yard->StorageCapacityMilliUnits = 120'000; Yard->ConstructionChainOutputGoodId = TEXT("Good.Firewood");
	Yard->ConstructionChainStage = 1; Yard->ConstructionChainStageCount = 1;
    Yard->RequiredConstructionTechnologyId.Reset(); Yard->bShowInConstructionMenu = true;
    Yard->ConstructionPresentationPurpose = FText::FromString(TEXT("Split delivered timber into fuel for homes and workshops."));
	Yard->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))); // Explicit economy-test placeholder only; never final presentation.
	Yard->PresentationActorClass.Reset();
	Heating->GoodId = TEXT("Good.Firewood"); Heating->bSeasonal = true; Heating->Alternatives.Reset();
	for (const auto& D : Draft)
	{
		bool Changed = false;
		if (auto* R = Cast<UHansaRecipeDefinition>(D.Get()))
		{
			int64 Fuel = R->StableDefinitionId == TEXT("Recipe.BakeBread") ? 50 :
				R->StableDefinitionId == TEXT("Recipe.MaltGrain") ? 100 : R->StableDefinitionId == TEXT("Recipe.BrewBeer") ? 200 : 0;
			if (Fuel) { R->Inputs.Add(Amount(TEXT("Good.Firewood"), Fuel)); Changed = true; }
		}
		if (auto* Tier = Cast<UHansaPopulationTierDefinition>(D.Get()))
		{
			FHansaPopulationTierNeed N; N.NeedId = TEXT("Need.Heating"); N.ConsumptionMilliUnitsPerResidentPerTick = 1;
			N.ImportanceBasisPoints = 4000; Tier->Needs.Add(N); Changed = true;
			// Keep existing relative summer weights; winter heating forms 28.6% of total.
		}
		if (auto* Market = Cast<UHansaCityMarketProfileDefinition>(D.Get()))
		{
			FHansaMarketGoodProfile P; P.GoodId = TEXT("Good.Firewood"); P.InitialStockMilliUnits = 60000;
			P.DesiredReserveMilliUnits = 60000; P.InitialPriceMilliMarks = 900;
			if (Market->bMarketOnly) { P.BackgroundProductionMilliUnitsPerUpdate = 1250; P.BackgroundCitizenDemandMilliUnitsPerUpdate = 750; }
			Market->Goods.Add(P); Changed = true;
		}
		if (Changed) ++D->AuthoredRevision;
		D->RefreshContentHash();
	}
	TArray<const UHansaDefinitionBase*> Definitions; for (const auto& D : Draft) Definitions.Add(D.Get());
	const auto Compiled = FHansaEconomicDefinitionCompiler::Compile(Definitions);
	for (const auto& Issue : Compiled.Issues) UE_LOG(LogTemp, Warning, TEXT("%s: %s"), *Issue.PropertyPath, *Issue.Cause.ToString());
	if (!Compiled.IsValid()) return 3;
    Algo::Reverse(Definitions);
    const auto Reversed = FHansaEconomicDefinitionCompiler::Compile(Definitions);
    if (!Reversed.IsValid() || Reversed.Registry.GetRegistryHash() != Compiled.Registry.GetRegistryHash()) return 6;
	auto Manifest = MakeShared<FJsonObject>();
	Manifest->SetStringField(TEXT("status"), TEXT("draft-awaiting-review"));
    Manifest->SetStringField(TEXT("pendingPresentation"), TEXT("/Game/Hansa/Generated/Staging/FirewoodModel/Meshes/SM_WoodcutterYard"));
	Manifest->SetStringField(TEXT("baseRegistryHash"), FString::Printf(TEXT("%016llX"), Original.Registry.GetRegistryHash()));
	Manifest->SetStringField(TEXT("registryHash"), FString::Printf(TEXT("%016llX"), Compiled.Registry.GetRegistryHash()));
	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const auto& D : Draft)
	{
		const FString Name = TEXT("DA_") + D->StableDefinitionId.Replace(TEXT("."), TEXT("_"));
		const FString PackageName = TEXT("/Game/Hansa/Generated/Staging/Firewood/") + Name;
		UPackage* Package = CreatePackage(*PackageName);
		auto* Asset = DuplicateObject<UHansaDefinitionBase>(D.Get(), Package, *Name); Asset->SetFlags(RF_Public | RF_Standalone);
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
		const FString File = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (!UPackage::SavePackage(Package, Asset, *File, Args)) return 4;
		auto Entry = MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("stableId"), D->StableDefinitionId);
		Entry->SetStringField(TEXT("asset"), PackageName + TEXT(".") + Name);
		Entry->SetStringField(TEXT("hash"), FString::Printf(TEXT("%016llX"), D->ComputeDeterministicContentHash()));
		Entries.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Manifest->SetArrayField(TEXT("definitions"), Entries);
	FString Json; FJsonSerializer::Serialize(Manifest, TJsonWriterFactory<>::Create(&Json));
	return FFileHelper::SaveStringToFile(Json, *(FPaths::ProjectDir() / TEXT("Docs/Development/FirewoodCandidate.json"))) ? 0 : 5;
}

namespace Hansa::Editor::EconomicDefinitions
{
bool ApplyApprovedFirewoodSeed(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Draft)
{
	auto Find = [&](const TCHAR* Id) -> UHansaDefinitionBase* {
		for (const auto& D : Draft) if (D->StableDefinitionId == Id) return D.Get(); return nullptr;
	};
	auto Clone = [&](const TCHAR* SourceId, const TCHAR* Id, const TCHAR* Label) -> UHansaDefinitionBase* {
		const auto* Source = Find(SourceId); if (!Source) return nullptr;
		auto* D = DuplicateObject<UHansaDefinitionBase>(Source, GetTransientPackage());
		D->StableDefinitionId = Id; D->DisplayName = FText::FromString(Label);
		D->LocalizationKey = FName(*(FString(TEXT("Game.")) + Id + TEXT(".Name")));
		D->AuthoredRevision = 1; Draft.Emplace(D); return D;
	};
	auto* Good = Cast<UHansaGoodDefinition>(Clone(TEXT("Good.Timber"), TEXT("Good.Firewood"), TEXT("Firewood")));
	auto* Recipe = Cast<UHansaRecipeDefinition>(Clone(TEXT("Recipe.MakeBarrels"), TEXT("Recipe.SplitFirewood"), TEXT("Split firewood")));
	auto* Yard = Cast<UHansaBuildingDefinition>(Clone(TEXT("Building.Cooperage"), TEXT("Building.WoodcutterYard"), TEXT("Woodcutter's yard")));
	auto* Heating = Cast<UHansaNeedDefinition>(Clone(TEXT("Need.Bread"), TEXT("Need.Heating"), TEXT("Heating")));
	if (!Good || !Recipe || !Yard || !Heating) return false;
	auto Amount = [](const TCHAR* Id, int64 Quantity) { FHansaGoodAmount A; A.GoodId = Id; A.QuantityMilliUnits = Quantity; return A; };
	Good->BaseValueMilliMarks = 900; Good->SpoilageBasisPointsPerDay = 0; Good->Icon.Reset();
	Recipe->Inputs = {Amount(TEXT("Good.Timber"), 6'000)};
	Recipe->Outputs = {Amount(TEXT("Good.Firewood"), 5'400)};
	Recipe->CycleTicks = 100; Recipe->LaborerWorkforce = 2; Recipe->ArtisanWorkforce = 0;
	Yard->RecipeIds = {TEXT("Recipe.SplitFirewood")}; Yard->LaborerWorkforce = 2; Yard->ArtisanWorkforce = 0;
	Yard->ConstructionCosts = {Amount(TEXT("Good.Timber"), 6000), Amount(TEXT("Good.Planks"), 2000), Amount(TEXT("Good.Tools"), 500)};
	Yard->ConstructionCostPfennig = 1800; Yard->FootprintWidthCells = 3; Yard->FootprintHeightCells = 3;
	Yard->StorageCapacityMilliUnits = 120'000; Yard->ConstructionChainOutputGoodId = TEXT("Good.Firewood");
	Yard->ConstructionChainStage = 1; Yard->ConstructionChainStageCount = 1;
    Yard->RequiredConstructionTechnologyId.Reset(); Yard->bShowInConstructionMenu = true;
    Yard->ConstructionPresentationPurpose = FText::FromString(TEXT("Split delivered timber into fuel for homes and workshops."));
	Yard->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Mesh/hansa-woodcutter-yard/Meshes/SM_WoodcutterYard.SM_WoodcutterYard"))); // Reviewed production binding.
	Yard->PresentationActorClass.Reset();
	Heating->SchemaVersion = 2; Heating->GoodId = TEXT("Good.Firewood"); Heating->bSeasonal = true; Heating->Alternatives.Reset();
	for (const auto& D : Draft)
	{
		bool Changed = false;
		if (auto* R = Cast<UHansaRecipeDefinition>(D.Get()))
		{
			int64 Fuel = R->StableDefinitionId == TEXT("Recipe.BakeBread") ? 50 :
				R->StableDefinitionId == TEXT("Recipe.MaltGrain") ? 100 : R->StableDefinitionId == TEXT("Recipe.BrewBeer") ? 200 : 0;
			if (Fuel) { R->Inputs.Add(Amount(TEXT("Good.Firewood"), Fuel)); Changed = true; }
		}
		if (auto* Tier = Cast<UHansaPopulationTierDefinition>(D.Get()))
		{
			FHansaPopulationTierNeed N; N.NeedId = TEXT("Need.Heating"); N.ConsumptionMilliUnitsPerResidentPerTick = 1;
			N.ImportanceBasisPoints = 4000; Tier->Needs.Add(N); Changed = true;
			// Keep existing relative summer weights; winter heating forms 28.6% of total.
		}
		if (auto* Market = Cast<UHansaCityMarketProfileDefinition>(D.Get()))
		{
			FHansaMarketGoodProfile P; P.GoodId = TEXT("Good.Firewood"); P.InitialStockMilliUnits = 60000;
			P.DesiredReserveMilliUnits = 60000; P.InitialPriceMilliMarks = 900;
			if (Market->bMarketOnly) { P.BackgroundProductionMilliUnitsPerUpdate = 1250; P.BackgroundCitizenDemandMilliUnitsPerUpdate = 750; }
			Market->Goods.Add(P); Changed = true;
		}
		if (Changed) ++D->AuthoredRevision;
		D->RefreshContentHash();
	}
	return true;
}
}
