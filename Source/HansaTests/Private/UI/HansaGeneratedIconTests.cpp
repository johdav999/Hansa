#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UI/HansaUiComponents.h"
using namespace Hansa::UI;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaGeneratedIconCoverage,"Hansa.UI.GeneratedIcons.Coverage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaGeneratedIconCoverage::RunTest(const FString&)
{
    for(int32 I=0;I<int32(EUiGlyph::Count);++I)
        for(int32 Side:{16,20,24,28,32,40,48,56,64,80,96,112,160})
        {
            const auto* Brush=GetGeneratedIconBrush(EUiGlyph(I),Side);
            const FString Path=Brush->GetResourceName().ToString();
            TestTrue(*Path,IFileManager::Get().FileExists(*Path));
            TArray<uint8> Bytes;
            if(FFileHelper::LoadFileToArray(Bytes,*Path)&&Bytes.Num()>25)
            {
                const auto U32=[&](int32 P){return (uint32(Bytes[P])<<24)|(uint32(Bytes[P+1])<<16)|(uint32(Bytes[P+2])<<8)|Bytes[P+3];};
                TestEqual(TEXT("PNG width matches display variant"),U32(16),uint32(Side));
                TestEqual(TEXT("PNG height matches display variant"),U32(20),uint32(Side));
                TestEqual(TEXT("PNG retains RGBA"),Bytes[25],uint8(6));
            }
        }
    TestTrue(TEXT("Unknown goods have explanatory information artwork"),GlyphForGood(TEXT("Good.Unknown"))==EUiGlyph::Information);
    return true;
}
#endif
