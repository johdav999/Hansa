// Editor-only, explicitly requested migration of the staged campaign's water.
namespace
{
bool ReplaceRiverSeaCorridors(UWorld* World)
{
    const FString Directory=Source()/TEXT("SplineRivers_v1/SeaCorridors_v1");
    FString Text;TSharedPtr<FJsonObject> Manifest;
    if(!FFileHelper::LoadFileToString(Text,*(Directory/TEXT("sea-corridors.json"))) ||
       !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Manifest) ||
       Manifest->GetIntegerField(TEXT("schema_version"))!=1 || Manifest->GetStringField(TEXT("map"))!=Map)return false;
    struct FCut { AWaterBodyCustom* Actor;FString Name;TArray<FVector3f> Positions;TArray<int32> Indices; };
    TArray<FCut> Cuts;TSet<FString> Labels;
    // Validate every exact target and every triangle before changing any actor.
    for(const auto& Value:Manifest->GetArrayField(TEXT("tiles")))
    {
        const auto Entry=Value->AsObject();const FString Label=Entry->GetStringField(TEXT("actor"));
        const FString Name=Entry->GetStringField(TEXT("mesh")),File=Entry->GetStringField(TEXT("file"));
        if(!Label.StartsWith(TEXT("SM_Sea_"))||Label.Len()!=12||Labels.Contains(Label)||
           !Name.StartsWith(TEXT("SM_Sea_RiverCut_v1_"))||File!=Name+TEXT(".f32"))return false;
        Labels.Add(Label);AWaterBodyCustom* Actor=nullptr;
        for(TActorIterator<AWaterBodyCustom> A(World);A;++A)if(A->GetActorLabel()==Label){if(Actor)return false;Actor=*A;}
        if(!Actor||!Actor->GetActorTransform().Equals(FTransform::Identity))return false;
        auto* Old=Actor->GetWaterBodyComponent()->GetWaterMeshOverride();
        if(!Old||!Old->GetPathName().StartsWith(Root/TEXT("Water/SM_Sea_")))return false;
        TArray<uint8> Bytes;
        if(!FFileHelper::LoadFileToArray(Bytes,*(Directory/File))||Bytes.IsEmpty()||Bytes.Num()%36||
           Bytes.Num()/12!=Entry->GetIntegerField(TEXT("vertices")))return false;
        FCut Cut;Cut.Actor=Actor;Cut.Name=Name+TEXT("_")+Entry->GetStringField(TEXT("sha256")).Left(12);
        const float* Data=reinterpret_cast<const float*>(Bytes.GetData());
        for(int32 N=0;N<Bytes.Num()/12;++N)
        {
            const FVector3f P(Data[N*3]*100,Data[N*3+1]*100,Data[N*3+2]*100);
            if(P.ContainsNaN()||P.X<0||P.X>7500001||P.Y<0||P.Y>6093751||P.Z!=0)return false;
            Cut.Positions.Add(P);Cut.Indices.Add(N);
        }
        Cuts.Add(MoveTemp(Cut));
    }
    if(Cuts.IsEmpty()||Cuts.Num()>48)return false;
    for(auto& Cut:Cuts)
    {
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(Root/TEXT("Water")/Cut.Name+TEXT(".")+Cut.Name));
        if(!Mesh)Mesh=MakeWaterMesh(Cut.Name,Cut.Positions,Cut.Indices);
        if(!Mesh)return false;
        Cut.Actor->Modify();auto* Body=Cut.Actor->GetWaterBodyComponent();
        Body->SetWaterMeshOverride(Mesh);
        FOnWaterBodyChangedParams Changed;Changed.bShapeOrPositionChanged=true;Body->UpdateAll(Changed);
        Cut.Actor->Tags.AddUnique(TEXT("HansaWorld.RiverSeaCut.v1"));Cut.Actor->MarkPackageDirty();
    }
    UE_LOG(LogTemp,Display,TEXT("Applied %d river-corridor sea cutouts; original sea assets and lake actors retained."),Cuts.Num());
    return true;
}
bool ReplaceWorldRivers(UWorld* World)
{
    const FString Directory=Source()/TEXT("SplineRivers_v1");
    FString Text;TSharedPtr<FJsonObject> Rivers;
    if(!FFileHelper::LoadFileToString(Text,*(Directory/TEXT("rivers.json"))) ||
       !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Rivers) ||
       Rivers->GetIntegerField(TEXT("schema_version"))!=1 || Rivers->GetStringField(TEXT("map"))!=Map)return false;
    const auto& Entries=Rivers->GetArrayField(TEXT("rivers"));
    if(Entries.IsEmpty() || Entries.Num()>2000)return false;
    TSet<FString> Ids;
    for(const auto& V:Entries)
    {
        const auto R=V->AsObject();const FString Id=R->GetStringField(TEXT("id"));
        if(Ids.Contains(Id)||!Id.StartsWith(TEXT("Water.River.")))return false;Ids.Add(Id);
        const auto& Points=R->GetArrayField(TEXT("points"));if(Points.Num()<2||Points.Num()>10000)return false;
        double LastHeight=DBL_MAX;
        for(const auto& P:Points)
        {
            const auto& A=P->AsArray();if(A.Num()!=8)return false;
            for(const auto& N:A)if(!FMath::IsFinite(N->AsNumber()))return false;
            if(A[2]->AsNumber()>LastHeight+.00001 || A[3]->AsNumber()<.05 || A[3]->AsNumber()>200 || A[4]->AsNumber()<=0)return false;
            LastHeight=A[2]->AsNumber();
        }
    }
    TArray<uint8> LakeBytes,Delta;
    if(!FFileHelper::LoadFileToArray(LakeBytes,*(Directory/TEXT("lake-cells.f32")))||LakeBytes.Num()%12||
       !FFileHelper::LoadFileToArray(Delta,*(Directory/TEXT("riverbed-delta.r16")))||Delta.Num()!=4033*3277*2)return false;
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Hansa/Generated/Staging/Rostock_P31/M_Rostock_Warnow.M_Rostock_Warnow"));
    auto* Affects=FindFProperty<FBoolProperty>(UWaterBodyComponent::StaticClass(),TEXT("bAffectsLandscape"));
    if(!Material||!Affects)return false;
    // WaterEditor's actor-added callback runs before SpawnActor returns. Disable
    // automatic carving on the spawn template, not after the brush has been added.
    TStrongObjectPtr<AWaterBodyRiver> RiverTemplate(DuplicateObject<AWaterBodyRiver>(
        GetMutableDefault<AWaterBodyRiver>(),GetTransientPackage()));
    Affects->SetPropertyValue_InContainer(RiverTemplate->GetWaterBodyComponent(),false);
    TMap<FString,AWaterBodyRiver*> Existing;
    for(TActorIterator<AWaterBodyRiver> A(World);A;++A)
        if(A->Tags.Contains(TEXT("HansaWorld.SplineRiver.v1")))for(const FName Tag:A->Tags)
            if(Tag.ToString().StartsWith(TEXT("Water.River.")))Existing.Add(Tag.ToString(),*A);
    for(const auto& V:Entries)
    {
        const auto R=V->AsObject();const FString Id=R->GetStringField(TEXT("id"));
        AWaterBodyRiver* Actor=Existing.FindRef(Id);
        if(!Actor){FActorSpawnParameters Spawn;Spawn.Template=RiverTemplate.Get();Actor=World->SpawnActor<AWaterBodyRiver>(Spawn);}
        if(!Actor)return false;
        Actor->Modify();Actor->SetActorLabel(Id+TEXT(" — ")+R->GetStringField(TEXT("name")));
        Actor->SetFolderPath(TEXT("Water/Rivers"));Actor->Tags.AddUnique(TEXT("HansaWorld.SplineRiver.v1"));Actor->Tags.AddUnique(FName(*Id));
        Actor->SetIsSpatiallyLoaded(false);
        auto* Body=CastChecked<UWaterBodyRiverComponent>(Actor->GetWaterBodyComponent());
        Affects->SetPropertyValue_InContainer(Body,false);
        Body->SetWaterMaterial(Material);Body->SetLakeAndOceanTransitionMaterials(Material,Material);
        // UE 5.8 PostLoad unconditionally clears river mesh overrides. Use its
        // supported spline-generated static surface instead (one mesh per reach).
        Body->SetWaterMeshOverride(nullptr);Body->SetWaterStaticMeshMaterial(Material);
        Body->SetWaterBodyStaticMeshEnabled(true);
        Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);Body->SetCanEverAffectNavigation(false);
        auto* Spline=Actor->GetWaterSpline();Spline->ClearSplinePoints(false);Spline->SetClosedLoop(false,false);
        const auto& Points=R->GetArrayField(TEXT("points"));
        const auto& First=Points[0]->AsArray();
        const FVector Origin(First[0]->AsNumber()*100,First[1]->AsNumber()*100,First[2]->AsNumber()*100);
        Actor->SetActorLocation(Origin);
        for(int32 N=0;N<Points.Num();++N)
        {
            const auto& P=Points[N]->AsArray();
            const FVector Position(P[0]->AsNumber()*100,P[1]->AsNumber()*100,P[2]->AsNumber()*100);
            const FVector Tangent(P[5]->AsNumber()*100,P[6]->AsNumber()*100,P[7]->AsNumber()*100);
            Spline->AddPoint(FSplinePoint(N,Position-Origin,Tangent,Tangent,FRotator::ZeroRotator,
                FVector(P[3]->AsNumber()*100,P[4]->AsNumber()*100,1),ESplinePointType::CurveCustomTangent),false);
        }
        Spline->UpdateSpline();
        auto* Metadata=Actor->GetWaterSplineMetadata();Metadata->Fixup(Points.Num(),Spline);
        for(int32 N=0;N<Points.Num();++N)
        {
            const auto& P=Points[N]->AsArray();
            Metadata->RiverWidth.Points[N].OutVal=P[3]->AsNumber()*100;
            Metadata->Depth.Points[N].OutVal=P[4]->AsNumber()*100;
            Metadata->WaterVelocityScalar.Points[N].OutVal=50;
            Metadata->RiverWidth.Points[N].InterpMode=CIM_Linear;
            Metadata->Depth.Points[N].InterpMode=CIM_Linear;
        }
        Spline->SynchronizeWaterProperties();Spline->UpdateSpline();
        FOnWaterBodyChangedParams Changed;Changed.bShapeOrPositionChanged=true;Body->UpdateAll(Changed);
        Actor->MarkPackageDirty();
    }
    for(const auto& Pair:Existing)if(!Ids.Contains(Pair.Key))World->DestroyActor(Pair.Value);
    // Preserve old actors for a reversible comparison, but disable both editor and game rendering.
    for(TActorIterator<AWaterBodyCustom> A(World);A;++A)
        if(A->GetActorLabel().StartsWith(TEXT("SM_InlandWater_")))
        {
            A->Modify();A->SetActorHiddenInGame(true);A->SetIsTemporarilyHiddenInEditor(true);
            A->bHiddenEd=true; // Persistent editor hide; temporary hiding is lost on reopen.
            A->GetWaterBodyComponent()->SetVisibility(false,true);
            A->SetFolderPath(TEXT("Water/LegacyDisabled"));A->MarkPackageDirty();
        }
    const auto Manifest=ReadManifest();const float Spacing=Manifest->GetNumberField(TEXT("world_step_m"))*100;
    const float* Data=reinterpret_cast<const float*>(LakeBytes.GetData());TMap<FIntPoint,TArray<int32>> Groups;
    for(int32 N=0;N<LakeBytes.Num()/12;++N)Groups.FindOrAdd(FIntPoint(int(Data[N*3])/256,int(Data[N*3+1])/256)).Add(N);
    for(const auto& Group:Groups)
    {
        const FString Name=FString::Printf(TEXT("SM_LakeOnly_v1_%02d_%02d"),Group.Key.X,Group.Key.Y);
        AWaterBodyCustom* ExistingLake=nullptr;
        for(TActorIterator<AWaterBodyCustom> A(World);A;++A)if(A->GetActorLabel()==Name){ExistingLake=*A;break;}
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(Root/TEXT("Water")/Name+TEXT(".")+Name));
        if(!Mesh)
        {
            TArray<FVector3f>P;TArray<int32>I;
            for(int32 N:Group.Value)Quad(P,I,Data[N*3]*Spacing,Data[N*3+1]*Spacing,Spacing,Spacing,Data[N*3+2]*100);
            Mesh=MakeWaterMesh(Name,P,I);if(!Mesh)return false;
        }
        if(!ExistingLake)WaterActor(World,Mesh,Material,Name);
    }
    for(TActorIterator<ALandscape> L(World);L;++L)
    {
        const FName Name(TEXT("SplineRiver_Hydrology"));int32 Index=L->GetLayerIndex(Name);
        if(Index==INDEX_NONE)Index=L->CreateLayer(Name);
        auto* Layer=L->GetEditLayer(Index);if(!Layer)return false;
        FLandscapeEditDataInterface Edit(L->GetLandscapeInfo(),Layer->GetGuid(),false);
        Edit.SetHeightData(0,0,4032,3276,reinterpret_cast<const uint16*>(Delta.GetData()),4033,true);Edit.Flush();
        L->ForceLayersFullUpdate();L->MarkPackageDirty();
    }
    if(!ReplaceRiverSeaCorridors(World))return false;
    if(!UEditorLoadingAndSavingUtils::SaveMap(World,Map))return false;
    TArray<UPackage*> Dirty,Content,Ours;UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Dirty);UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);
    Dirty.Append(Content);for(auto* P:Dirty)if(P->GetName().Contains(TEXT("HansaWorld_20260918")))Ours.AddUnique(P);
    if(!UEditorLoadingAndSavingUtils::SavePackages(Ours,true))return false;
    UE_LOG(LogTemp,Display,TEXT("Spline river migration saved: %d native rivers. GPU terrain merge/reopen review still required."),Entries.Num());
    return true;
}
}
