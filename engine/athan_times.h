// AdhanBox for Windows — محرّك مواقيت الصلاة (منقول حرفيًا من athan_times.py المتحقق منه)
// حساب فلكي محلي بالكامل — بدون إنترنت. خوارزمية PrayTimes المعيارية.
#pragma once
#include <string>
#include <map>
#include <cmath>

namespace athan {

// 'ishaMin' لو أكبر من صفر = دقايق بعد المغرب بدل الزاوية (أم القرى/قطر)
struct Method {
    const wchar_t* labelAr;
    const wchar_t* labelEn;
    double fajr;        // زاوية الفجر تحت الأفق
    double isha;        // زاوية العشاء (مُهملة لو ishaMin > 0)
    double ishaMin;     // دقايق بعد المغرب
    double maghribAng;  // زاوية مغرب خاصة (طهران/جعفري) — 0 = غروب عادي
};

// نفس ترتيب ومفاتيح النسخة المرجعية
const std::map<std::string, Method>& methods();

enum PrayerIdx { FAJR = 0, SUNRISE, DHUHR, ASR, MAGHRIB, ISHA, COUNT };
extern const char* NAMES[COUNT];

struct Times {
    double h[COUNT];            // بالساعات المحلية؛ NAN = مش بيحصل
    bool valid(int i) const { return !std::isnan(h[i]); }
    // "HH:MM" بالتقريب للدقيقة الأقرب — مطابق لـtimes_hhmm في البايثون
    std::wstring hhmm(int i) const;
    int minuteOfDay(int i) const;   // -1 لو NAN
};

class PrayerTimes {
public:
    PrayerTimes(double lat, double lng, double tzOffsetHours,
                const std::string& method = "Egypt", int asrFactor = 1);
    // y/m/d بالتقويم الميلادي المحلي
    Times compute(int y, int m, int d) const;

private:
    double lat_, lng_, tz_;
    Method m_;
    int asr_;
    double midDay(double jd, double t) const;
    double sunAngleTime(double jd, double angle, double t, bool ccw) const;
    double asrTime(double jd, double t) const;
    double nightPortion(double angle, double night) const;
};

} // namespace athan
