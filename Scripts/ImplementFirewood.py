"""Task-local guarded edits; retain pre-task source bytes in Saved for review."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
BACKUP = ROOT / 'Saved/GenerationJobs/Firewood_20260916/baseline'
def edit(name, fn):
    path = ROOT / name
    before = path.read_bytes()
    backup = BACKUP / name
    if not backup.exists():
        backup.parent.mkdir(parents=True, exist_ok=True)
        backup.write_bytes(before)
    text = before.decode('utf-8-sig').replace('\r\n','\n')
    after = fn(text)
    path.write_text(after, encoding='utf-8', newline='\n')
def replace(text, old, new, count=1):
    assert text.count(old) == count, (old[:100], text.count(old), count)
    return text.replace(old,new)
def compiled(t):
    marker='\tstruct HANSASIMULATION_API FHansaCompiledPopulationTierNeed final'
    a,b=t.split(marker)
    i=a.rfind('\t\tuint64 ContentHash = 0;')
    a=a[:i]+a[i:].replace('\t\tuint64 ContentHash = 0;', '\t\tuint64 ContentHash = 0;\n\t\tbool bSeasonal = false;\n\t\tint32 SeasonDays = 90;\n\t\tint32 FixedSeason = -1;\n\t\tTArray<int32> SeasonMultipliers = { 0, 4000, 10000, 4000 };',1)
    return a+marker+b
edit('Source/HansaSimulation/Public/Definitions/HansaEconomicRegistry.h',compiled)
def need_header(t):
    fields=''
    for typ,name,default,desc,validation,extra in [
        ('bool','bSeasonal','false','Scale good consumption by the deterministic calendar; zero demand is excluded from satisfaction.','Boolean',''),
        ('int32','SeasonDays','90','Length of each season in game days. Calendar begins in summer.','Range','ClampMin = "1", ClampMax = "365", HansaMin = "1", HansaMax = "365", HansaUnit = "GameDay",'),
        ('int32','FixedSeason','-1','Minus one follows the calendar; 0 summer, 1 autumn, 2 winter, 3 spring fixes the season for scenarios.','Range','ClampMin = "-1", ClampMax = "3", HansaMin = "-1", HansaMax = "3", HansaUnit = "SeasonIndex",'),
        ('TArray<int32>','SeasonMultipliers','{ 0, 4000, 10000, 4000 }','Exactly four demand multipliers: summer, autumn, winter, spring. Each is 0 to 10000 basis points.','SeasonMultipliers','HansaUnit = "BasisPoint",')]:
        fields+=f'\n\tUPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Need|Season", meta = (\n\t\tDisplayName = "{name}", ToolTip = "{desc}", {extra}\n\t\tHansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",\n\t\tHansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "{validation}"))\n\t{typ} {name} = {default};\n'
    return t.replace('\tFString GoodId;','\tFString GoodId;\n'+fields,1)
edit('Source/Hansa/Public/Definitions/HansaPopulationDefinitions.h',need_header)
def need_cpp(t):
    marker='\tif (!HasPopulationDomain(StableDefinitionId, TEXT("Need")))'
    check='''\tif (SeasonDays < 1 || SeasonDays > 365 || FixedSeason < -1 || FixedSeason > 3 ||
		SeasonMultipliers.Num() != 4 || SeasonMultipliers.ContainsByPredicate([](int32 V) { return V < 0 || V > 10000; }) ||
		(bSeasonal && Kind != EHansaNeedKind::Good))
	{
		AddPopulationIssue(OutIssues, TEXT("HSA-NEED-SEASON"), TEXT("SeasonMultipliers"),
			NSLOCTEXT("HansaPopulationDefinition", "SeasonInvalid", "Seasonal need settings are invalid."),
			NSLOCTEXT("HansaPopulationDefinition", "SeasonRemedy", "Use a good need, 1-365 days, fixed season -1 to 3, and four multipliers from 0 to 10000."));
	}
'''
    t=replace(t,marker,check+marker)
    marker='\tInOutCanonicalData += FString::Printf(TEXT("kind=%d\\ngood=%s\\n"), static_cast<int32>(Kind), *GoodId);'
    return replace(t,marker,marker+'''
	// Default nonseasonal definitions retain their historical fingerprints.
	if (bSeasonal || SeasonDays != 90 || FixedSeason != -1 || SeasonMultipliers != TArray<int32>({0,4000,10000,4000}))
	{
		InOutCanonicalData += FString::Printf(TEXT("seasonal=%d;days=%d;fixed=%d;"), bSeasonal, SeasonDays, FixedSeason);
		for (int32 Factor : SeasonMultipliers) InOutCanonicalData += FString::Printf(TEXT("%d,"), Factor);
	}
''')
edit('Source/Hansa/Private/Definitions/HansaPopulationDefinitions.cpp',need_cpp)
edit('Source/Hansa/Private/Definitions/HansaEconomicDefinitionCompiler.cpp',lambda t:replace(t,'Need->GoodId, ContentHash });','Need->GoodId, ContentHash, Need->bSeasonal, Need->SeasonDays, Need->FixedSeason, Need->SeasonMultipliers });'))
def population(t):
    t=t.replace('#include "Population/HansaPopulation.h"','#include "Population/HansaPopulation.h"\n#include "Population/HansaSeasonalNeeds.h"',1)
    import re
    t,n=re.subn(r'const int64 RequiredRaw = static_cast<int64>\(Cohort.Residents\) \*\s*Requirement.ConsumptionMilliUnitsPerResidentPerTick;', 'const int64 RequiredRaw = FHansaSeasonalNeeds::Demand(*NeedDefinition, Requirement.ConsumptionMilliUnitsPerResidentPerTick, Cohort.Residents, Tick, MinutesPerTick);',t)
    assert n==2,n
    marker='\t\t\t\tif (NeedDefinition == nullptr || !NeedId) continue;'
    t=replace(t,marker,marker+'\n\t\t\t\tif (FHansaSeasonalNeeds::Multiplier(*NeedDefinition, Tick, MinutesPerTick) == 0) continue;')
    t=replace(t,'const int64 ProspectiveRaw = Requirement.ConsumptionMilliUnitsPerResidentPerTick;', 'const int64 ProspectiveRaw = FMath::Max<int64>(1, FHansaSeasonalNeeds::Demand(*NeedDefinition, Requirement.ConsumptionMilliUnitsPerResidentPerTick, 1, Tick, MinutesPerTick));')
    return t
edit('Source/HansaSimulation/Private/Population/HansaPopulation.cpp',population)
print('Seasonal need schema, compiler and consumption edits applied.')
