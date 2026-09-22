#pragma once
#include "Misc/ConfigCacheIni.h"

// Keep tests independent of the local playtest setting; never persist overrides.
struct FScopedHansaArtisanConstructionOverride
{
 bool bPrevious = false;
 bool bExisted = false;
 bool bPreviousPlots = true;
 bool bPlotsExisted = false;
 explicit FScopedHansaArtisanConstructionOverride(bool bEnabled)
 {
  bPlotsExisted = GConfig->GetBool(TEXT("Hansa.Housing"), TEXT("UseArtisanPlots"), bPreviousPlots, GEngineIni);
  GConfig->SetBool(TEXT("Hansa.Housing"), TEXT("UseArtisanPlots"), false, GEngineIni);
  bExisted = GConfig->GetBool(TEXT("Hansa.ConstructionTesting"), TEXT("AllowDirectArtisanResidence"), bPrevious, GEngineIni);
  GConfig->SetBool(TEXT("Hansa.ConstructionTesting"), TEXT("AllowDirectArtisanResidence"), bEnabled, GEngineIni);
 }
 ~FScopedHansaArtisanConstructionOverride()
 {
  if (bPlotsExisted) GConfig->SetBool(TEXT("Hansa.Housing"), TEXT("UseArtisanPlots"), bPreviousPlots, GEngineIni);
  else GConfig->RemoveKey(TEXT("Hansa.Housing"), TEXT("UseArtisanPlots"), GEngineIni);
  if (bExisted) GConfig->SetBool(TEXT("Hansa.ConstructionTesting"), TEXT("AllowDirectArtisanResidence"), bPrevious, GEngineIni);
  else GConfig->RemoveKey(TEXT("Hansa.ConstructionTesting"), TEXT("AllowDirectArtisanResidence"), GEngineIni);
 }
};
