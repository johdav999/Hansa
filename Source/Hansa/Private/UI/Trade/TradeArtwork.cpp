#include "TradeArtwork.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Misc/Paths.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"
namespace Hansa::UI {
const FSlateBrush* TradeIconArtwork(const TCHAR* Name,int32 PixelSize) {
 static TMap<FString,TSharedPtr<FSlateDynamicImageBrush>> Icons;
 const TArray<int32> Sizes=FString(Name)==TEXT("MerchantOffice")?TArray<int32>{80,112}:FString(Name)==TEXT("GrainSack")?TArray<int32>{48,64,80}:TArray<int32>{14,20,28,40};
 int32 Density=Sizes.Last();for(int32 Candidate:Sizes)if(PixelSize<=Candidate){Density=Candidate;break;}
 const FString Key=FString::Printf(TEXT("%s--%d.png"),Name,Density);auto& Brush=Icons.FindOrAdd(Key);
 if(!Brush)Brush=MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/TradeWorkspace/Icons")/Key)),FVector2D(Density,Density));
 return Brush.Get();
}

const FSlateBrush* TradeArtwork(const TCHAR* Name, bool Tiled) {
 struct FAsset { TStrongObjectPtr<UTexture2D> Texture; FSlateBrush Brush; };
 static TMap<FString,TSharedPtr<FAsset>> Assets;
 const FString Key=FString(Name)+(Tiled?TEXT("Tile"):TEXT("Image"));
 auto& Asset=Assets.FindOrAdd(Key);
 if(!Asset) {
  Asset=MakeShared<FAsset>();
  const FString Path=FString::Printf(TEXT("/Game/Hansa/UI/TradeWorkspace/T_UI_TradeWorkspace_%s_Default.T_UI_TradeWorkspace_%s_Default"),Name,Name);
  Asset->Texture.Reset(LoadObject<UTexture2D>(nullptr,*Path));
  if(Asset->Texture.IsValid()) {
   Asset->Brush.SetResourceObject(Asset->Texture.Get());Asset->Brush.ImageSize=FVector2D(Asset->Texture->GetSizeX(),Asset->Texture->GetSizeY());
   Asset->Brush.DrawAs=ESlateBrushDrawType::Image;Asset->Brush.Tiling=Tiled?ESlateBrushTileType::Both:ESlateBrushTileType::NoTile;
  }
 }
 return &Asset->Brush;
}
}
