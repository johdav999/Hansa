// Native material treatment of the existing, unchanged grass/loam and road artwork.
// World-centimetre coordinates keep adjacent Landscape components and road cells continuous.
namespace
{
const TCHAR* SurfaceNoise = TEXT(R"HLSL(
struct HansaSurfaceNoise {
 float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
 float sample(float2 p) {
  float2 i=floor(p), f=frac(p); f=f*f*(3.0-2.0*f);
  return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),
              lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);
 }
};
HansaSurfaceNoise n;
)HLSL");
UMaterialExpressionCustom* SurfaceCustom(UMaterial* M,const TCHAR* Marker,ECustomMaterialOutputType Type)
{
 auto* N=FindByDesc<UMaterialExpressionCustom>(M,Marker);
 if(!N){N=Node<UMaterialExpressionCustom>(M);N->Desc=Marker;}
 if(N->Inputs.Num()==1&&N->Inputs[0].InputName.IsNone())N->Inputs.Reset();
 N->OutputType=Type;return N;
}
void SurfaceInput(UMaterialExpressionCustom* N,const TCHAR* Name,UMaterialExpression* E,int32 Output=0)
{
 FCustomInput I;I.InputName=Name;I.Input.Connect(Output,E);N->Inputs.Add(I);
}
void ConfigureBrokenRoadShoulders(UMaterial* M)
{
 auto* Edge=SurfaceCustom(M,TEXT("Hansa broken shoulders v3"),CMOT_Float1);
 if(Edge->Inputs.IsEmpty())
 {
  SurfaceInput(Edge,TEXT("P"),Node<UMaterialExpressionWorldPosition>(M));
  SurfaceInput(Edge,TEXT("Coverage"),Node<UMaterialExpressionVertexColor>(M),4);
  SurfaceInput(Edge,TEXT("Breakup"),Scalar(M,TEXT("ShoulderBreakup"),.7f));
 }
 Edge->Code=FString(SurfaceNoise)+TEXT(R"HLSL(
 float coarse=n.sample(P.xy/73.0), fine=n.sample(P.xy/19.0);
 // Preserve the solid travel lane and zero outer boundary; erode only shoulders.
 float erosion=saturate(Breakup)*(0.18+0.55*coarse+0.27*fine);
 return saturate(Coverage-4.0*Coverage*(1.0-Coverage)*erosion);
 )HLSL");
 // Broad dirt variation remains readable after the fine source texture mips out.
 auto* Albedo=FindByDesc<UMaterialExpressionMultiply>(M,TEXT("Hansa road albedo v2"));
 auto* Earth=SurfaceCustom(M,TEXT("Hansa road earth variation v3"),CMOT_Float3);
 if(Albedo&&Earth->Inputs.IsEmpty())
 {
  FCustomInput Base;Base.InputName=TEXT("Base");Base.Input=Albedo->A;Earth->Inputs.Add(Base);
  SurfaceInput(Earth,TEXT("P"),Edge->Inputs[0].Input.Expression);
  SurfaceInput(Earth,TEXT("Coverage"),Edge->Inputs[1].Input.Expression,4);
 }
 Earth->Code=FString(SurfaceNoise)+TEXT(R"HLSL(
 float patches=n.sample(P.xy/165.0), grain=n.sample(P.xy/31.0);
 float shade=0.76+0.30*patches+0.12*grain;
 float3 earth=Base*float3(.52,.46,.38)*shade;
 return earth*lerp(float3(.81,.86,.70),float3(1,1,1),saturate(Coverage));
 )HLSL");
 if(Albedo)Albedo->A.Connect(0,Earth);
 M->GetExpressionInputForProperty(MP_OpacityMask)->Connect(0,Edge);
 for(UMaterialExpression* E:M->GetExpressions())
  if(auto* Out=Cast<UMaterialExpressionRuntimeVirtualTextureOutput>(E))Out->Opacity.Connect(0,Edge);
}
// Distance is surveyed; band width and moisture are presentation only.
UMaterialExpressionCustom* ConfigureShoreWeights(UMaterial* M,UMaterialExpression* P)
{
 auto* Data=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/Textures/T_Terrain_Lubeck_ShoreBands.T_Terrain_Lubeck_ShoreBands"));
 if(!Data)return nullptr;
 auto* UV=SurfaceCustom(M,TEXT("Hansa shoreline UV v1"),CMOT_Float2);
 if(UV->Inputs.IsEmpty())SurfaceInput(UV,TEXT("P"),P);
 UV->Code=TEXT("return (P.xy+float2(201550.0,201650.0))/403200.0;");
 auto* W=SurfaceCustom(M,TEXT("Hansa shoreline weights v1"),CMOT_Float3);
 if(W->Inputs.IsEmpty())
 {
  auto* Sample=Node<UMaterialExpressionTextureSample>(M);Sample->Texture=Data;Sample->SamplerType=SAMPLERTYPE_LinearColor;Sample->Coordinates.Connect(0,UV);
  SurfaceInput(W,TEXT("Data"),Sample);SurfaceInput(W,TEXT("P"),P);
 }
 if(auto* Sample=Cast<UMaterialExpressionTextureSample>(W->Inputs[0].Input.Expression))Sample->Texture=Data;
 W->Code=FString(SurfaceNoise)+TEXT(R"HLSL(
 float width=lerp(.65,1.4,n.sample(P.xy/1100.0));
 float grain=n.sample(P.xy/95.0)-.5;
 float d=max(0.0,Data.b*3200.0+grain*45.0);
 float h=max(0.0,P.z-Data.g*400.0);
 float reach=1.0-smoothstep(1500.0,1900.0,Data.r*3200.0);
 float damp=(1.0-smoothstep(550.0*width,1400.0*width,d))*(1.0-smoothstep(180.0,350.0,h));
 float silt=(1.0-smoothstep(180.0*width,600.0*width,d))*(1.0-smoothstep(55.0*width,145.0*width,h));
 float wet=(1.0-smoothstep(45.0,175.0*width,d))*(1.0-smoothstep(10.0,45.0*width,h));
 return saturate(float3(damp,silt,wet)*reach);
 )HLSL");
 return W;
}
bool ConfigureGroundVariation(UMaterial* M)
{
 auto* Switch=Cast<UMaterialExpressionVirtualTextureFeatureSwitch>(M->GetExpressionInputForProperty(MP_BaseColor)->Expression);
 auto* Blend=Switch?Cast<UMaterialExpressionLinearInterpolate>(Switch->Yes.Expression):nullptr;
 if(!Switch||!Blend)return false;
 UMaterialExpressionTextureSample* Soil=nullptr;
 for(UMaterialExpression* E:M->GetExpressions())
  if(auto* Sample=Cast<UMaterialExpressionTextureSample>(E);Sample&&Sample->Texture&&Sample->Texture->GetName()==TEXT("T_Terrain_Lubeck_BankLoam"))Soil=Sample;
 if(!Soil)return false;
 auto* Color=SurfaceCustom(M,TEXT("Hansa ground macro variation v3"),CMOT_Float3);
 if(Color->Inputs.IsEmpty())
 {
  FCustomInput Original;Original.InputName=TEXT("Base");Original.Input=Switch->No;Color->Inputs.Add(Original);
  SurfaceInput(Color,TEXT("P"),Node<UMaterialExpressionWorldPosition>(M));
  SurfaceInput(Color,TEXT("Strength"),Scalar(M,TEXT("GroundVariationStrength"),.85f));
  SurfaceInput(Color,TEXT("PatchScale"),Scalar(M,TEXT("GroundPatchScaleCm"),2400.f));
  SurfaceInput(Color,TEXT("Soil"),Soil);
 }
 // P30 rebuilding restores its base input; reattach it without nesting wrappers.
 if(Switch->No.Expression!=Color)Color->Inputs[0].Input=Switch->No;
 auto* Shore=ConfigureShoreWeights(M,Color->Inputs[1].Input.Expression);
 if(Shore&&Color->Inputs.Num()==5)SurfaceInput(Color,TEXT("Shore"),Shore);
 Color->Code=FString(SurfaceNoise)+TEXT(R"HLSL(
 float2 p=P.xy/max(PatchScale,100.0);
 float broad=n.sample(p), mid=n.sample(p*3.17+float2(17.3,41.7));
 float detail=n.sample(p*11.1+float2(63.1,7.2));
 float meadow=smoothstep(0.26,0.73,broad*0.72+mid*0.28);
 float bare=smoothstep(0.57,0.78,mid*0.78+detail*0.22)*(1.0-meadow*0.55);
 float3 grass=Base*lerp(float3(.82,.86,.64),float3(.57,.88,.52),meadow);
 float3 soil=Soil*float3(.96,.92,.83);
 float3 varied=lerp(grass,soil,bare*.7)*(0.88+0.24*broad);
 float3 ground=lerp(Base,varied,saturate(Strength));
 float3 damp=Soil*float3(.70,.67,.57);
 float3 silt=Soil*.8+float3(.075,.060,.040);
 float3 wet=Soil*float3(.34,.33,.29);
 ground=lerp(ground,damp,Shore.r);
 ground=lerp(ground,silt,Shore.g);
 return lerp(ground,wet,Shore.b);
 )HLSL");
 if(!Shore)Color->Code.ReplaceInline(TEXT("Shore."),TEXT("float3(0,0,0)."));
 Switch->No.Connect(0,Color);Blend->A.Connect(0,Color);
 if(Shore)
 {
  auto* RS=Cast<UMaterialExpressionVirtualTextureFeatureSwitch>(M->GetExpressionInputForProperty(MP_Roughness)->Expression);
  auto* RB=RS?Cast<UMaterialExpressionLinearInterpolate>(RS->Yes.Expression):nullptr;
  if(RS&&RB)
  {
   auto* R=SurfaceCustom(M,TEXT("Hansa shoreline roughness v1"),CMOT_Float1);
   if(R->Inputs.IsEmpty())
   {
    FCustomInput I;I.InputName=TEXT("Base");I.Input=RS->No;R->Inputs.Add(I);SurfaceInput(R,TEXT("Shore"),Shore);
   }
   if(RS->No.Expression!=R)R->Inputs[0].Input=RS->No;
   R->Code=TEXT("return lerp(lerp(lerp(Base,.72,Shore.r),.82,Shore.g),.32,Shore.b);");
   RS->No.Connect(0,R);RB->A.Connect(0,R);
  }
 }
 return true;
}
}
