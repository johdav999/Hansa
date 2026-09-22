// User-approved promotion. Called only with an explicit commandlet switch.
namespace
{
int32 PromoteFirewood(bool VerifyOnly, IAssetRegistry& Assets, const TArray<const UHansaDefinitionBase*>& Baseline)
{
    constexpr uint64 ReviewedBase=0xD73BFD73C23C2D03ULL;
    auto Compile=FHansaEconomicDefinitionCompiler::Compile(Baseline);
    if(!Compile.IsValid())return 20;
    if(!VerifyOnly)
    {
        if(Compile.Registry.GetRegistryHash()!=ReviewedBase)return 21;
        Assets.ScanPathsSynchronous({TEXT("/Game/Hansa/Generated/Staging/Firewood")},true);
        TArray<FAssetData> Rows;Assets.GetAssetsByPath(TEXT("/Game/Hansa/Generated/Staging/Firewood"),Rows,true,false);
        TArray<const UHansaDefinitionBase*> Draft;
        for(const auto& Row:Rows)if(auto* D=Cast<UHansaDefinitionBase>(Row.GetAsset()))Draft.Add(D);
        const auto Reviewed=FHansaEconomicDefinitionCompiler::Compile(Draft);
        if(!Reviewed.IsValid()||Reviewed.Registry.GetRegistryHash()!=0x8BB8ACD607E70FDBULL)return 22;
        const FString Mesh=TEXT("/Game/Mesh/hansa-woodcutter-yard/Meshes/SM_WoodcutterYard.SM_WoodcutterYard");
        if(!FSoftObjectPath(Mesh).TryLoad())return 23;
        TArray<UHansaDefinitionBase*> Changed;
        for(const auto* D:Draft)
        {
            const auto* Found=Baseline.FindByPredicate([&](const auto* B){return B->StableDefinitionId==D->StableDefinitionId;});
            UHansaDefinitionBase* Target=nullptr;
            if(Found)
            {
                const bool Selected=D->StableDefinitionId==TEXT("Recipe.BakeBread")||D->StableDefinitionId==TEXT("Recipe.MaltGrain")||D->StableDefinitionId==TEXT("Recipe.BrewBeer")||D->IsA<UHansaPopulationTierDefinition>()||D->IsA<UHansaCityMarketProfileDefinition>();
                if(!Selected)continue;
                Target=const_cast<UHansaDefinitionBase*>(*Found);
                if(auto* R=Cast<UHansaRecipeDefinition>(Target))
                {const auto* Fuel=CastChecked<UHansaRecipeDefinition>(D)->Inputs.FindByPredicate([](const auto& A){return A.GoodId==TEXT("Good.Firewood");});if(!Fuel||R->Inputs.ContainsByPredicate([](const auto& A){return A.GoodId==TEXT("Good.Firewood");}))return 24;R->Inputs.Add(*Fuel);}
                else if(auto* T=Cast<UHansaPopulationTierDefinition>(Target))
                {const auto* Heat=CastChecked<UHansaPopulationTierDefinition>(D)->Needs.FindByPredicate([](const auto& A){return A.NeedId==TEXT("Need.Heating");});if(!Heat||T->Needs.ContainsByPredicate([](const auto& A){return A.NeedId==TEXT("Need.Heating");}))return 24;T->Needs.Add(*Heat);}
                else if(auto* M=Cast<UHansaCityMarketProfileDefinition>(Target))
                {const auto* Fuel=CastChecked<UHansaCityMarketProfileDefinition>(D)->Goods.FindByPredicate([](const auto& A){return A.GoodId==TEXT("Good.Firewood");});if(!Fuel||M->Goods.ContainsByPredicate([](const auto& A){return A.GoodId==TEXT("Good.Firewood");}))return 24;M->Goods.Add(*Fuel);}
                else return 24;
                ++Target->AuthoredRevision;Target->RefreshContentHash();
            }
            else
            {
                FString Folder;
                if(D->StableDefinitionId==TEXT("Good.Firewood"))Folder=TEXT("Goods");
                else if(D->StableDefinitionId==TEXT("Recipe.SplitFirewood"))Folder=TEXT("Recipes");
                else if(D->StableDefinitionId==TEXT("Building.WoodcutterYard"))Folder=TEXT("Buildings");
                else if(D->StableDefinitionId==TEXT("Need.Heating"))Folder=TEXT("Needs");
                else return 26;
                const FString Name=TEXT("DA_")+D->StableDefinitionId.Replace(TEXT("."),TEXT("_"));
                const FString Path=TEXT("/Game/Hansa/Core/")+Folder+TEXT("/")+Name;
                if(FPackageName::DoesPackageExist(Path))return 27;
                Target=DuplicateObject<UHansaDefinitionBase>(D,CreatePackage(*Path),*Name);
                Target->SetFlags(RF_Public|RF_Standalone);
                if(Target->StableDefinitionId==TEXT("Building.WoodcutterYard"))Target->PresentationMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Mesh));
                if(Target->StableDefinitionId==TEXT("Need.Heating"))Target->SchemaVersion=2;
                Target->RefreshContentHash();FAssetRegistryModule::AssetCreated(Target);
            }
            Changed.Add(Target);
        }
        if(Changed.Num()!=13)return 28;
        // Backups are outside Content and never part of the cooked catalog.
        for(auto* D:Changed)
        {
            const FString File=FPackageName::LongPackageNameToFilename(D->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension());
            const FString Backup=FPaths::ProjectSavedDir()/TEXT("GenerationJobs/Firewood_20260916/promotion-baseline")/FPaths::GetCleanFilename(File);
            if(IFileManager::Get().FileExists(*File)&&!IFileManager::Get().FileExists(*Backup))
            {IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);if(IFileManager::Get().Copy(*Backup,*File)!=COPY_OK)return 29;}
        }
        for(auto* D:Changed)
        {
            FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
            const FString File=FPackageName::LongPackageNameToFilename(D->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension());
            if(!UPackage::SavePackage(D->GetPackage(),D,*File,Args))return 30;
        }
        UE_LOG(LogTemp,Display,TEXT("FIREWOOD_PROMOTED thirteen approved definitions. Run -VerifyApproved in a fresh process before pinning."));
        return 0;
    }
    if(Baseline.Num()!=106)return 31;
    auto Reverse=Baseline;Algo::Reverse(Reverse);
    if(FHansaEconomicDefinitionCompiler::Compile(Reverse).Registry.GetRegistryHash()!=Compile.Registry.GetRegistryHash())return 32;
    auto Root=MakeShared<FJsonObject>();Root->SetNumberField(TEXT("schemaVersion"),1);Root->SetNumberField(TEXT("catalogVersion"),26);Root->SetNumberField(TEXT("previousCatalogVersion"),25);
    Root->SetStringField(TEXT("previousRegistryHash"),TEXT("D73BFD73C23C2D03"));Root->SetStringField(TEXT("registryHash"),FString::Printf(TEXT("%016llX"),Compile.Registry.GetRegistryHash()));
    Root->SetStringField(TEXT("approval"),TEXT("User approved FirewoodReview.md in conversation, 2026-09-16."));
    Root->SetStringField(TEXT("saveCompatibility"),TEXT("Catalog v25 saves are explicitly incompatible with v26 economics; files are preserved. Current save-format migrations remain available for matching catalogs."));
    TArray<TSharedPtr<FJsonValue>> Entries;
    for(const auto& D:Compile.DefinitionHashes)
    {auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("stableId"),D.StableId);E->SetStringField(TEXT("classPath"),D.DefinitionClassPath);E->SetStringField(TEXT("contentHash"),FString::Printf(TEXT("%016llX"),D.ContentHash));Entries.Add(MakeShared<FJsonValueObject>(E));}
    Root->SetArrayField(TEXT("definitions"),Entries);FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
    if(!FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectDir()/TEXT("Tests/Golden/economic_catalog_v26.json"))))return 33;
    UE_LOG(LogTemp,Display,TEXT("FIREWOOD_ACCEPTED_RELOAD hash=%016llX definitions=%d"),Compile.Registry.GetRegistryHash(),Baseline.Num());return 0;
}
}
