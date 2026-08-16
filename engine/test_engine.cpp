// اختبار مطابقة: بيطبع CSV لنفس الحالات اللي verify.py بيحسبها بالبايثون المرجعي
#include "athan_times.h"
#include <cstdio>

struct Case { const char* name; double lat, lng, tz; };

int main() {
    // مدن متنوعة العروض + عرض عالي (أوسلو) لاختبار مسار AngleBased
    Case cities[] = {
        {"Giza",   30.0131, 31.2089,  3.0},
        {"Makkah", 21.3891, 39.8579,  3.0},
        {"Cairo",  30.0444, 31.2357,  3.0},
        {"London", 51.5074, -0.1278,  1.0},
        {"Oslo",   59.9139, 10.7522,  2.0},
        {"Jakarta",-6.2088, 106.8456, 7.0},
        {"NewYork",40.7128, -74.0060, -4.0},
    };
    const char* meths[] = {"Egypt", "Makkah", "MWL", "ISNA", "Tehran"};
    int dates[][3] = {{2026,1,15},{2026,6,21},{2026,7,29},{2026,12,21}};

    for (auto& c : cities)
      for (auto* m : meths)
        for (auto& d : dates)
          for (int asr = 1; asr <= 2; asr++) {
            athan::PrayerTimes pt(c.lat, c.lng, c.tz, m, asr);
            athan::Times t = pt.compute(d[0], d[1], d[2]);
            printf("%s,%s,%04d-%02d-%02d,%d", c.name, m, d[0], d[1], d[2], asr);
            for (int i = 0; i < athan::COUNT; i++)
                printf(",%d", t.minuteOfDay(i));
            printf("\n");
          }
    return 0;
}
