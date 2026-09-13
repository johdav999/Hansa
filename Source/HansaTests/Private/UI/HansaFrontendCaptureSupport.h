#pragma once
#include "UI/HansaFrontendPresentationModel.h"
#include "UI/HansaRootHud.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/SHansaRootHud.h"
/** Existing screen captures enter through the same production frontend as players. */
inline bool HansaWaitForFrontend(AHansaRootHud* Hud){
 auto* M=Hud->GetFrontendPresentationModel();if(!M)return false;
 if(M->GetSnapshot().Page==EHansaFrontendPage::Title){Hud->GetRootWidget()->ActivateSemanticId(TEXT("Frontend.NewGame"));return true;}
 if(M->GetSnapshot().Page==EHansaFrontendPage::Loading)return true;
 if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){const auto Begin=Hud->GetRootWidget()->ResolveSemanticWidget(TEXT("Scenario.Begin"));return !Begin||!Begin->HasKeyboardFocus();}
 return false;
}
