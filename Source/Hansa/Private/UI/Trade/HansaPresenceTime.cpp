#include "UI/HansaTradeEstablishment.h"

#define LOCTEXT_NAMESPACE "HansaPresenceCalendar"
namespace Hansa::UI {
FText PresenceDuration(const int64 Steps,const int32 MinutesPerStep) {
 if(MinutesPerStep<=0||Steps<0)return LOCTEXT("Unknown","Unavailable");
 const int64 Minutes=Steps*int64(MinutesPerStep);
 const int64 Days=Minutes/1440,Hours=(Minutes%1440)/60,Rest=Minutes%60;
 if(Days&&Rest)return FText::Format(LOCTEXT("DaysHoursMinutes","{0} d {1} h {2} min"),FText::AsNumber(Days),FText::AsNumber(Hours),FText::AsNumber(Rest));
 if(Days&&Hours)return FText::Format(LOCTEXT("DaysHours","{0} d {1} h"),FText::AsNumber(Days),FText::AsNumber(Hours));
 if(Days)return FText::Format(LOCTEXT("Days","{0} {0}|plural(one=day,other=days)"),Days);
 if(Hours&&Rest)return FText::Format(LOCTEXT("HoursMinutes","{0} h {1} min"),FText::AsNumber(Hours),FText::AsNumber(Rest));
 if(Hours)return FText::Format(LOCTEXT("Hours","{0} {0}|plural(one=hour,other=hours)"),Hours);
 return FText::Format(LOCTEXT("Minutes","{0} min"),FText::AsNumber(Minutes));
}
FText PresenceTimeProgress(const int64 Current,const int64 Required,const int32 MinutesPerStep) {
 if(MinutesPerStep<=0)return LOCTEXT("Unknown","Unavailable");
 const double A=double(Current)*MinutesPerStep,B=double(Required)*MinutesPerStep;
 const double Divisor=B>=1440?1440.:B>=60?60.:1.;
 FNumberFormattingOptions Numbers;Numbers.SetMaximumFractionalDigits(2);
 return FText::Format(LOCTEXT("Progress","{0} / {1} {2}"),FText::AsNumber(A/Divisor,&Numbers),FText::AsNumber(B/Divisor,&Numbers),
  Divisor==1440?LOCTEXT("DayUnit","days"):Divisor==60?LOCTEXT("HourUnit","hours"):LOCTEXT("MinuteUnit","min"));
}
FText PresenceDailyUpkeep(const int64 PerStep,const int32 MinutesPerStep) {
 if(MinutesPerStep<=0)return LOCTEXT("Unknown","Unavailable");
 FNumberFormattingOptions Numbers;Numbers.SetMaximumFractionalDigits(2);
 return FText::Format(LOCTEXT("Daily","{0} pfennig / day"),FText::AsNumber(double(PerStep)*1440./MinutesPerStep,&Numbers));
}
}
#undef LOCTEXT_NAMESPACE
