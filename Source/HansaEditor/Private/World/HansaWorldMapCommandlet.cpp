#include "World/HansaWorldMapCommandlet.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "WaterBodyCustomActor.h"
#include "WaterBodyCustomComponent.h"
#include "WaterBodyRiverActor.h"
#include "WaterBodyRiverComponent.h"
#include "WaterSplineComponent.h"
#include "WaterSplineMetadata.h"
#include "UObject/UnrealType.h"
#include "WaterZoneActor.h"
#include "GeoReferencingSystem.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshDescription.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartition.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerStart.h"
#include "World/HansaGameMode.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayerInstancePrivate.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/SavePackage.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const FString Root = TEXT("/Game/Hansa/Generated/Staging/HansaWorld_20260918");
const FString Map = Root / TEXT("L_HansaWorld_WP");
const FString ReferenceMap = TEXT("/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP");
FString Source() { return FPaths::ProjectDir() / TEXT("SourceArt/Terrain/HansaWorld/Prototype_20260918"); }
bool SaveAsset(UObject* Asset)
{
    FAssetRegistryModule::AssetCreated(Asset);
    Asset->MarkPackageDirty();
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(Asset->GetOutermost(),Asset,
        *FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
TSharedPtr<FJsonObject> ReadManifest()
{
    FString Text; TSharedPtr<FJsonObject> Result;
    if (!FFileHelper::LoadFileToString(Text,*(Source()/TEXT("terrain-manifest.json")))) return nullptr;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Result)) return nullptr;
    return Result;
}
void Report(const TSharedRef<FJsonObject>& Result)
{
    FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("GenerationJobs/HansaWorld_20260918");
    IFileManager::Get().MakeDirectory(*Directory,true);
    FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("unreal-build-report.json")));
    UE_LOG(LogTemp,Display,TEXT("HansaWorld: %s"),*Text);
}
TArray<uint16> ReadHeights(const TCHAR* Name,int32 Count)
{
    TArray<uint8> Bytes;TArray<uint16> Data;
    if (FFileHelper::LoadFileToArray(Bytes,*(Source()/Name)) && Bytes.Num()==Count*2)
    {Data.SetNumUninitialized(Count);FMemory::Memcpy(Data.GetData(),Bytes.GetData(),Bytes.Num());}
    return Data;
}
bool IsLighting(AActor* Actor)
{
    return Actor->FindComponentByClass<ULightComponent>() || Actor->FindComponentByClass<USkyLightComponent>() ||
        Actor->FindComponentByClass<USkyAtmosphereComponent>() || Actor->FindComponentByClass<UExponentialHeightFogComponent>() ||
        Actor->FindComponentByClass<UVolumetricCloudComponent>() || Actor->FindComponentByClass<UPostProcessComponent>() ||
        Actor->IsA<APostProcessVolume>();
}
UStaticMesh* MakeWaterMesh(const FString& Name,const TArray<FVector3f>& Positions,const TArray<int32>& Indices)
{
    FMeshDescription Description;FStaticMeshAttributes Attributes(Description);Attributes.Register();
    auto VertexPositions=Attributes.GetVertexPositions();auto Normals=Attributes.GetVertexInstanceNormals();
    auto UVs=Attributes.GetVertexInstanceUVs();UVs.SetNumChannels(1);
    const FPolygonGroupID Group=Description.CreatePolygonGroup();
    TArray<FVertexID> Vertices;
    for (const FVector3f& Position:Positions){const FVertexID Vertex=Description.CreateVertex();VertexPositions[Vertex]=Position;Vertices.Add(Vertex);}
    for(int32 I=0;I<Indices.Num();I+=3)
    {
        TArray<FVertexInstanceID> Corners;
        for(int32 J=0;J<3;++J){const int32 Index=Indices[I+J];const FVertexInstanceID Corner=Description.CreateVertexInstance(Vertices[Index]);
            Normals[Corner]=FVector3f(0,0,1);UVs.Set(Corner,0,FVector2f(Positions[Index].X/400,Positions[Index].Y/400));Corners.Add(Corner);}
        Description.CreatePolygon(Group,Corners);
    }
    UStaticMesh* Mesh=NewObject<UStaticMesh>(CreatePackage(*(Root/TEXT("Water")/Name)),*Name,RF_Public|RF_Standalone);
    Mesh->GetStaticMaterials().Add(FStaticMaterial());Mesh->SetNumSourceModels(1);
    Mesh->CreateMeshDescription(0,MoveTemp(Description));Mesh->CommitMeshDescription(0);Mesh->Build(false);Mesh->PostEditChange();
    return SaveAsset(Mesh)?Mesh:nullptr;
}
void Quad(TArray<FVector3f>& P,TArray<int32>& I,float X,float Y,float W,float H,float Z)
{
    int32 N=P.Num();P.Append({FVector3f(X,Y,Z),FVector3f(X+W,Y,Z),FVector3f(X+W,Y+H,Z),FVector3f(X,Y+H,Z)});
    // Unreal front faces are clockwise from above.
    I.Append({N,N+2,N+1,N,N+3,N+2});
}
void WaterActor(UWorld* World,UStaticMesh* Mesh,UMaterialInterface* Material,const FString& Label)
{
    AWaterBodyCustom* Actor=World->SpawnActor<AWaterBodyCustom>();Actor->SetActorLabel(Label);Actor->SetFolderPath(TEXT("Water"));
    Actor->Tags.Add(TEXT("HansaWorld.WaterWinding.v3"));
    Actor->GetWaterBodyComponent()->SetWaterMaterial(Material);Actor->GetWaterBodyComponent()->SetWaterMeshOverride(Mesh);
    Actor->GetWaterBodyComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Actor->GetWaterBodyComponent()->SetCanEverAffectNavigation(false);Actor->PostEditChange();
    FOnWaterBodyChangedParams Changed;Changed.bShapeOrPositionChanged=true;Actor->GetWaterBodyComponent()->UpdateAll(Changed);
}
}

#include "HansaWorldRivers.inl"

UHansaWorldMapCommandlet::UHansaWorldMapCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}

int32 UHansaWorldMapCommandlet::Main(const FString& Params)
{
    const auto Manifest=ReadManifest();
    if(!GEditor || !Manifest || Manifest->GetStringField(TEXT("map"))!=Map){UE_LOG(LogTemp,Error,TEXT("Invalid world manifest/editor"));return 1;}
    const int32 Width=Manifest->GetArrayField(TEXT("vertices"))[0]->AsNumber();
    const int32 Height=Manifest->GetArrayField(TEXT("vertices"))[1]->AsNumber();
    const double Spacing=Manifest->GetNumberField(TEXT("world_step_m"))*100;
    if(Width<127||Height<127||Width>8129||Height>8129||(Width-1)%126||(Height-1)%126||Spacing<=0)return 1;
    const auto Result=MakeShared<FJsonObject>();Result->SetStringField(TEXT("map"),Map);Result->SetBoolField(TEXT("productionApproved"),false);
    if(FPackageName::DoesPackageExist(Map))
    {
        // A completed map is reusable; retries never blindly overwrite partially authored packages.
        if(!FEditorFileUtils::LoadMap(Map,false,true))return 1;
        if(FParse::Param(*Params,TEXT("AuditRiverZones")))
        {
            UWorld* AuditWorld=GEditor->GetEditorWorldContext().World();
            for(TActorIterator<AWaterZone> Z(AuditWorld);Z;++Z)
                UE_LOG(LogTemp,Display,TEXT("RiverZoneAudit zone=%s label=%s"),*Z->GetPathName(),*Z->GetActorLabel());
            for(TActorIterator<AWaterBody> B(AuditWorld);B;++B)
            {auto* C=B->GetWaterBodyComponent();UE_LOG(LogTemp,Display,TEXT("RiverZoneAudit body=%s type=%d tiles=%d zone=%s"),
                *B->GetActorLabel(),int32(C->GetWaterBodyType()),C->AffectsWaterMesh(),*GetPathNameSafe(C->GetWaterZone()));}
            return 0;
        }
        if(FParse::Param(*Params,TEXT("SplineRivers")))
            return ReplaceWorldRivers(GEditor->GetEditorWorldContext().World())?0:1;
        if(FParse::Param(*Params,TEXT("Finalize")))
        {
            UWorld* Existing=GEditor->GetEditorWorldContext().World();
            FWorldPartitionHelpers::FForEachActorWithLoadingParams LP;LP.bKeepReferences=true;
            FWorldPartitionHelpers::FForEachActorWithLoadingResult LR;
            FWorldPartitionHelpers::ForEachActorWithLoading(Existing->GetWorldPartition(),[](const FWorldPartitionActorDescInstance*){return true;},LP,LR);
            // Authoring prototype must be visible on a normal open, without temporary loader handles.
            Existing->GetWorldPartition()->SetEnableStreaming(false);
            Existing->GetWorldSettings()->DefaultGameMode=AHansaGameMode::StaticClass();Existing->GetWorldSettings()->MarkPackageDirty();
            TMap<UMaterialInterface*,UMaterial*> TreeMaterials;
            auto* CustomWaterMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Hansa/Generated/Staging/Rostock_P31/M_Rostock_Warnow.M_Rostock_Warnow"));
            if(!CustomWaterMaterial)return 1;
            for(TActorIterator<AActor> Actor(Existing);Actor;++Actor)
            {
                if(auto* Water=Cast<AWaterBodyCustom>(*Actor))
                {Water->GetWaterBodyComponent()->SetWaterMaterial(CustomWaterMaterial);FOnWaterBodyChangedParams Changed;Changed.bShapeOrPositionChanged=true;Water->GetWaterBodyComponent()->UpdateAll(Changed);Water->MarkPackageDirty();}
                if(auto* Water=Cast<AWaterBodyCustom>(*Actor);Water&&!Water->Tags.Contains(TEXT("HansaWorld.WaterWinding.v3")))
                {
                    UStaticMesh* Mesh=Water->GetWaterBodyComponent()->GetWaterMeshOverride();
                    if(!Mesh||!Mesh->GetPathName().StartsWith(Root))return 1;
                    auto* Description=Mesh->GetMeshDescription(0);if(!Description)return 1;
                    if(Water->Tags.Contains(TEXT("HansaWorld.WaterWinding.v2")))
                        for(const FPolygonID Polygon:Description->Polygons().GetElementIDs())Description->ReversePolygonFacing(Polygon);
                    Mesh->CommitMeshDescription(0);Mesh->Build(false);Mesh->PostEditChange();if(!SaveAsset(Mesh))return 1;
                    Water->Tags.Add(TEXT("HansaWorld.WaterWinding.v3"));Water->MarkPackageDirty();
                }
                if(Actor->GetActorLabel().StartsWith(TEXT("DEV_City")))
                {
                    // These are reference aids for this NeverCook staged map,
                    // including Play/Simulate previews, not editor-only actors.
                    Actor->bIsEditorOnlyActor=false;Actor->SetIsSpatiallyLoaded(false);
                    Actor->SetActorHiddenInGame(false);Actor->SetIsTemporarilyHiddenInEditor(false);
                    for(const UDataLayerInstance* Layer:Actor->GetDataLayerInstances())
                    {
                        GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>()->SetDataLayerVisibility(const_cast<UDataLayerInstance*>(Layer),true);
                        GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>()->SetDataLayerIsLoadedInEditor(const_cast<UDataLayerInstance*>(Layer),true,true);
                    }
                    Actor->MarkPackageDirty();
                }
                TArray<UHierarchicalInstancedStaticMeshComponent*> Components;Actor->GetComponents(Components);
                for(auto* Component:Components)for(int32 Slot=0;Slot<Component->GetNumMaterials();++Slot)
                {
                    UMaterialInterface* Original=Component->GetMaterial(Slot);if(!Original||Original->GetPathName().StartsWith(Root))continue;
                    UMaterial*& Copy=TreeMaterials.FindOrAdd(Original);
                    if(!Copy)
                    {
                        const FString Name=Original->GetName()+TEXT("_WorldInstanced");const FString Path=Root/TEXT("TreeMaterials")/Name;
                        Copy=LoadObject<UMaterial>(nullptr,*(Path+TEXT(".")+Name));
                        if(!Copy)Copy=DuplicateObject<UMaterial>(Original->GetMaterial(),CreatePackage(*Path),*Name);
                        bool Recompile=false;Copy->SetMaterialUsage(Recompile,MATUSAGE_InstancedStaticMeshes);Copy->PostEditChange();if(!SaveAsset(Copy))return 1;
                    }
                    Component->SetMaterial(Slot,Copy);Actor->MarkPackageDirty();
                }
            }
            bool HasStart=false;for(TActorIterator<APlayerStart> It(Existing);It;++It)HasStart=true;
            if(!HasStart)
            {
                const auto City=Manifest->GetArrayField(TEXT("cities"))[0]->AsObject();
                Existing->SpawnActor<APlayerStart>(FVector(City->GetNumberField(TEXT("x_cm")),City->GetNumberField(TEXT("y_cm")),City->GetNumberField(TEXT("z_cm"))+30000),FRotator(-55,-90,0))->SetActorLabel(TEXT("HansaWorld_PreviewStart"));
            }
            if(!UEditorLoadingAndSavingUtils::SaveMap(Existing,Map))return 1;
            TArray<UPackage*> Dirty,Content;UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Dirty);UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);
            TArray<UPackage*> Ours;for(auto* P:Dirty)if(P->GetName().Contains(TEXT("HansaWorld_20260918")))Ours.AddUnique(P);
            for(auto* P:Content)if(P->GetName().Contains(TEXT("HansaWorld_20260918")))Ours.AddUnique(P);
            if(!UEditorLoadingAndSavingUtils::SavePackages(Ours,true))return 1;
            Result->SetStringField(TEXT("status"),TEXT("Staged water winding, tree shader variants, markers and preview setup finalized"));
        }
        else Result->SetStringField(TEXT("status"),TEXT("Existing staged map reopened; authoring skipped"));
        Report(Result);return 0;
    }
    TArray<uint16> Survey=ReadHeights(TEXT("survey-base.r16"),Width*Height);
    TArray<uint16> Hydrology=ReadHeights(TEXT("hydrology-delta.r16"),Width*Height);
    TArray<uint16> Grading=ReadHeights(TEXT("gameplay-delta.r16"),Width*Height);
    if(Survey.Num()!=Width*Height||Hydrology.Num()!=Width*Height||Grading.Num()!=Width*Height)return 1;
    UMaterialInterface* Ground=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/M_Lubeck_Ground.M_Lubeck_Ground"));
    UMaterialInterface* WaterMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Hansa/Generated/Staging/Rostock_P31/M_Rostock_Warnow.M_Rostock_Warnow"));
    UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    if(!Ground||!WaterMaterial||!Cube)return 1;
    TArray<TStrongObjectPtr<AActor>> LightTemplates;
    TArray<FTransform> LightTransforms;
    TArray<TSharedPtr<FJsonValue>> LightingReport;
    if(!FEditorFileUtils::LoadMap(ReferenceMap,false,true))return 1;
    UWorld* Reference=GEditor->GetEditorWorldContext().World();
    FWorldPartitionHelpers::FForEachActorWithLoadingParams LoadParams;LoadParams.bKeepReferences=true;
    FWorldPartitionHelpers::FForEachActorWithLoadingResult LoadedActors;
    FWorldPartitionHelpers::ForEachActorWithLoading(Reference->GetWorldPartition(),[](const FWorldPartitionActorDescInstance*){return true;},LoadParams,LoadedActors);
    for(TActorIterator<AActor> It(Reference);It;++It)if(IsLighting(*It))
    {
        LightTransforms.Add(It->GetActorTransform());
        auto* Duplicate=DuplicateObject<AActor>(*It,GetTransientPackage());Duplicate->SetFlags(RF_Transient);
        LightTemplates.Emplace(Duplicate);
        auto Entry=MakeShared<FJsonObject>();Entry->SetStringField(TEXT("sourceActor"),It->GetPathName());Entry->SetStringField(TEXT("class"),It->GetClass()->GetPathName());
        Entry->SetStringField(TEXT("rotation"),It->GetActorRotation().ToString());
        if(auto* Light=It->FindComponentByClass<ULightComponent>()){Entry->SetNumberField(TEXT("intensity"),Light->Intensity);Entry->SetNumberField(TEXT("temperature"),Light->Temperature);}
        if(auto* Sky=It->FindComponentByClass<USkyLightComponent>())Entry->SetNumberField(TEXT("skyIntensity"),Sky->Intensity);
        LightingReport.Add(MakeShared<FJsonValueObject>(Entry));
    }
    if(LightTemplates.IsEmpty())return 1;
    // Release loaded reference-map handles before opening the new world.
    LoadedActors.ActorReferences.Empty();
    UWorld* World=GEditor->NewMap(true);if(!World||!World->GetWorldPartition())return 1;
    World->GetWorldPartition()->SetEnableStreaming(false);
    for(int32 I=0;I<LightTemplates.Num();++I)
    {
        FActorSpawnParameters Spawn;Spawn.Template=LightTemplates[I].Get();
        AActor* Actor=World->SpawnActor<AActor>(Spawn.Template->GetClass(),LightTransforms[I],Spawn);
        if(!Actor)return 1;Actor->SetActorLabel(LightTemplates[I]->GetActorLabel());Actor->SetFolderPath(TEXT("Environment/Lighting"));
    }
    ALandscape* Landscape=World->SpawnActor<ALandscape>();Landscape->SetActorScale3D(FVector(Spacing,Spacing,400));
    Landscape->SetActorLabel(TEXT("Terrain.HansaWorld"));Landscape->LandscapeMaterial=Ground;
    TMap<FGuid,TArray<uint16>> HL;HL.Add(FGuid(),MoveTemp(Survey));TMap<FGuid,TArray<FLandscapeImportLayerInfo>> ML;ML.Add(FGuid(),{});
    Landscape->Import(FGuid::NewGuid(),0,0,Width-1,Height-1,2,63,HL,nullptr,ML,ELandscapeImportAlphamapType::Additive,TArrayView<const FLandscapeLayer>());
    ULandscapeEditLayerBase* Base=Landscape->GetEditLayer(0);if(!Base)return 1;Base->SetName(TEXT("Survey_Base"),true);Base->SetLocked(true,true);
    Landscape->CreateLayer(TEXT("Historical_Hydrology"));Landscape->CreateLayer(TEXT("Historical_Corrections"));
    auto AddDelta=[&](FName Name,const TArray<uint16>& Data)
    {
        int32 Index=Landscape->CreateLayer(Name);auto* Layer=Landscape->GetEditLayer(Index);if(!Layer)return false;
        FLandscapeEditDataInterface Edit(Landscape->GetLandscapeInfo(),Layer->GetGuid(),false);
        Edit.SetHeightData(0,0,Width-1,Height-1,Data.GetData(),Width,true);Edit.Flush();return true;
    };
    if(!AddDelta(TEXT("Water_Hydrology"),Hydrology)||!AddDelta(TEXT("Gameplay_Grading"),Grading))return 1;
    Landscape->ForceLayersFullUpdate();Landscape->PostEditChange();
    World->GetSubsystem<ULandscapeSubsystem>()->ChangeGridSize(Landscape->GetLandscapeInfo(),4);
    AGeoReferencingSystem* Geo=World->SpawnActor<AGeoReferencingSystem>();Geo->SetActorLabel(TEXT("Geography.SourceEPSG3035_CompressionInManifest"));
    Geo->PlanetShape=EPlanetShape::FlatPlanet;Geo->ProjectedCRS=TEXT("EPSG:3035");Geo->GeographicCRS=TEXT("EPSG:4326");
    Geo->bOriginLocationInProjectedCRS=true;Geo->OriginProjectedCoordinatesEasting=Manifest->GetArrayField(TEXT("projected_bounds_m"))[0]->AsNumber();
    Geo->OriginProjectedCoordinatesNorthing=Manifest->GetArrayField(TEXT("projected_bounds_m"))[3]->AsNumber();Geo->ApplySettings();
    Geo->Tags.Add(TEXT("GameplayCompressed.NotDirectGeoreferencing"));
    auto* MarkerLayer=World->GetWorldDataLayers()->CreateDataLayer<UDataLayerInstancePrivate>();
    GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>()->SetDataLayerShortName(MarkerLayer,TEXT("CityMarkers"));
    for(const auto& Value:Manifest->GetArrayField(TEXT("cities")))
    {
        const auto City=Value->AsObject();const FString Id=City->GetStringField(TEXT("id"));
        const FVector Position(City->GetNumberField(TEXT("x_cm")),City->GetNumberField(TEXT("y_cm")),City->GetNumberField(TEXT("z_cm")));
        auto* Marker=World->SpawnActor<AStaticMeshActor>(Position+FVector(0,0,1000),FRotator::ZeroRotator);
        Marker->GetStaticMeshComponent()->SetStaticMesh(Cube);Marker->SetActorScale3D(FVector(20));
        Marker->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);Marker->GetStaticMeshComponent()->SetCastShadow(false);Marker->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
        Marker->SetActorLabel(TEXT("DEV_CityMarker_")+Id);Marker->Tags.Add(FName(*Id));Marker->bIsEditorOnlyActor=false;Marker->SetIsSpatiallyLoaded(false);Marker->SetActorHiddenInGame(false);Marker->AddDataLayer(MarkerLayer);Marker->SetFolderPath(TEXT("CityMarkers"));
        auto* Label=World->SpawnActor<ATextRenderActor>(Position+FVector(0,0,3500),FRotator(60,90,0));
        Label->GetTextRender()->SetText(FText::FromString(City->GetStringField(TEXT("name"))));Label->GetTextRender()->SetWorldSize(1800);
        Label->GetTextRender()->SetHorizontalAlignment(EHTA_Center);Label->GetTextRender()->SetTextRenderColor(FColor(250,247,239));
        Label->GetTextRender()->SetCollisionEnabled(ECollisionEnabled::NoCollision);Label->GetTextRender()->SetCastShadow(false);
        Label->SetActorLabel(TEXT("DEV_CityLabel_")+Id);Label->Tags.Add(FName(*Id));Label->bIsEditorOnlyActor=false;Label->SetIsSpatiallyLoaded(false);Label->SetActorHiddenInGame(false);Label->AddDataLayer(MarkerLayer);Label->SetFolderPath(TEXT("CityMarkers"));
    }
    // Sea uses project Water rendering over DEM bathymetry; land and island silhouettes occlude it.
    const double WX=(Width-1)*Spacing,WY=(Height-1)*Spacing;
    for(int32 Y=0;Y<6;++Y)for(int32 X=0;X<8;++X)
    {
        TArray<FVector3f>P;TArray<int32>I;Quad(P,I,X*WX/8,Y*WY/6,WX/8,WY/6,0);
        const FString Name=FString::Printf(TEXT("SM_Sea_%02d_%02d"),X,Y);auto* Mesh=MakeWaterMesh(Name,P,I);if(!Mesh)return 1;
        WaterActor(World,Mesh,WaterMaterial,Name);
    }
    TArray<uint8> WaterBytes;if(!FFileHelper::LoadFileToArray(WaterBytes,*(Source()/TEXT("water-cells.f32")))||WaterBytes.Num()%12)return 1;
    const float* WaterData=reinterpret_cast<const float*>(WaterBytes.GetData());
    const int32 Stride=Manifest->GetIntegerField(TEXT("water_cell_stride"));
    TMap<FIntPoint,TArray<int32>> WaterGroups;
    for(int32 N=0;N<WaterBytes.Num()/12;++N)WaterGroups.FindOrAdd(FIntPoint(int(WaterData[N*3])/256,int(WaterData[N*3+1])/256)).Add(N);
    for(const auto& Group:WaterGroups)
    {
        TArray<FVector3f>P;TArray<int32>I;
        for(int32 N:Group.Value)Quad(P,I,WaterData[N*3]*Spacing,WaterData[N*3+1]*Spacing,Spacing*Stride,Spacing*Stride,WaterData[N*3+2]*100);
        const FString Name=FString::Printf(TEXT("SM_InlandWater_%02d_%02d"),Group.Key.X,Group.Key.Y);auto* Mesh=MakeWaterMesh(Name,P,I);if(!Mesh)return 1;
        WaterActor(World,Mesh,WaterMaterial,Name);
    }
    World->SpawnActor<AWaterZone>()->SetActorLabel(TEXT("WaterZone.HansaWorld"));
    TArray<uint8> TreeBytes;if(!FFileHelper::LoadFileToArray(TreeBytes,*(Source()/TEXT("trees.f32")))||TreeBytes.Num()%28)return 1;
    const float* TreeData=reinterpret_cast<const float*>(TreeBytes.GetData());
    const TCHAR* Species[]={TEXT("Alder"),TEXT("Willow"),TEXT("Birch"),TEXT("Oak"),TEXT("Beech")};
    TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Forests;
    for(int32 N=0;N<TreeBytes.Num()/28;++N)
    {
        const float* T=TreeData+N*7;int32 S=int32(T[5]);if(S<0||S>=5)return 1;
        const FString MeshName=FString::Printf(TEXT("SM_%s_%s"),Species[S],T[6]>0?TEXT("Young"):TEXT("Mature"));
        const FString Key=FString::Printf(TEXT("%s_%d_%d"),*MeshName,int(T[0]/500000),int(T[1]/500000));
        auto*& Instances=Forests.FindOrAdd(Key);
        if(!Instances)
        {
            UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/Hansa/Generated/Staging/LubeckTrees_20260916/Meshes/")+MeshName+TEXT(".")+MeshName));if(!Mesh)return 1;
            AActor* Actor=World->SpawnActor<AActor>();Actor->SetActorLabel(TEXT("Forest_")+Key);Actor->SetFolderPath(TEXT("Environment/Trees"));
            Instances=NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor);Actor->AddInstanceComponent(Instances);Actor->SetRootComponent(Instances);
            Instances->SetStaticMesh(Mesh);Instances->SetMobility(EComponentMobility::Static);Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Instances->SetCullDistances(20000,180000);Instances->RegisterComponent();
        }
        Instances->AddInstance(FTransform(FRotator(0,T[3],0),FVector(T[0],T[1],T[2]),FVector(T[4])),true);
    }
    if(!UEditorLoadingAndSavingUtils::SaveMap(World,Map))return 1;
    TArray<UPackage*> Packages,Content;UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Packages);UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);
    for(UPackage* Package:Content)if(Package->GetName().StartsWith(Root))Packages.AddUnique(Package);
    for(UPackage* Package:Packages)if(!Package->GetName().Contains(TEXT("HansaWorld_20260918"))){UE_LOG(LogTemp,Error,TEXT("Refusing unrelated package save: %s"),*Package->GetName());return 1;}
    if(!UEditorLoadingAndSavingUtils::SavePackages(Packages,true))return 1;
    Result->SetStringField(TEXT("status"),TEXT("Staged native World Partition map authored; visual and gameplay validation pending"));
    Result->SetArrayField(TEXT("lightingCopied"),LightingReport);Result->SetStringField(TEXT("terrainMaterial"),Ground->GetPathName());
    Result->SetStringField(TEXT("waterMaterial"),WaterMaterial->GetPathName());Result->SetNumberField(TEXT("cityCount"),Manifest->GetArrayField(TEXT("cities")).Num());
    Result->SetNumberField(TEXT("treeCount"),TreeBytes.Num()/28);Result->SetNumberField(TEXT("landscapeComponents"),((Width-1)/126)*((Height-1)/126));
    Report(Result);return 0;
}
