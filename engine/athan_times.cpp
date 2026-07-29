// نقل حرفي لـathan_times.py — أي تغيير هنا لازم يعدّي على اختبار المطابقة ضد البايثون
#include "athan_times.h"
#include <cwchar>

namespace athan {

static const double D2R = 3.14159265358979323846 / 180.0;
static const double R2D = 180.0 / 3.14159265358979323846;

static double dsin(double d)  { return std::sin(d * D2R); }
static double dcos(double d)  { return std::cos(d * D2R); }
static double dtan(double d)  { return std::tan(d * D2R); }
static double dasin(double x) { return std::asin(x) * R2D; }
static double dacos(double x) { return std::acos(x) * R2D; }
static double datan2(double y, double x) { return std::atan2(y, x) * R2D; }
static double dacot(double x) { return std::atan2(1.0, x) * R2D; }

// NaN بتعدّي زي ما هي (زي البايثون) — الفجر/العشاء ممكن ما يحصلوش في العروض العالية
static double fix(double a, double b) {
    if (a != a) return a;
    a = a - b * std::floor(a / b);
    return a < 0 ? a + b : a;
}
static double fixangle(double a) { return fix(a, 360.0); }
static double fixhour(double a)  { return fix(a, 24.0); }

const std::map<std::string, Method>& methods() {
    static const std::map<std::string, Method> M = {
        {"Egypt",     {L"الهيئة المصرية العامة للمساحة", L"Egyptian General Authority of Survey", 19.5, 17.5, 0,   0}},
        {"Makkah",    {L"أم القرى — مكة المكرمة",        L"Umm al-Qura (Makkah)",                 18.5, 0,    90,  0}},
        {"MWL",       {L"رابطة العالم الإسلامي",          L"Muslim World League",                  18.0, 17.0, 0,   0}},
        {"ISNA",      {L"أمريكا الشمالية (ISNA)",         L"ISNA (North America)",                 15.0, 15.0, 0,   0}},
        {"Karachi",   {L"جامعة العلوم الإسلامية — كراتشي",L"Univ. of Islamic Sciences, Karachi",   18.0, 18.0, 0,   0}},
        {"Dubai",     {L"الإمارات — دبي",                 L"UAE (Dubai)",                          18.2, 18.2, 0,   0}},
        {"Qatar",     {L"قطر",                            L"Qatar",                                18.0, 0,    90,  0}},
        {"Kuwait",    {L"الكويت",                         L"Kuwait",                               18.0, 17.5, 0,   0}},
        {"Turkey",    {L"تركيا — ديانت",                  L"Türkiye (Diyanet)",                    18.0, 17.0, 0,   0}},
        {"Singapore", {L"سنغافورة وماليزيا وإندونيسيا",   L"Singapore / Malaysia / Indonesia",     20.0, 18.0, 0,   0}},
        {"Morocco",   {L"المغرب",                         L"Morocco",                              19.0, 17.0, 0,   0}},
        {"France",    {L"فرنسا (UOIF)",                   L"France (UOIF)",                        12.0, 12.0, 0,   0}},
        {"Russia",    {L"روسيا",                          L"Russia",                               16.0, 15.0, 0,   0}},
        {"Tehran",    {L"طهران",                          L"Tehran",                               17.7, 14.0, 0,   4.5}},
        {"Jafari",    {L"الجعفري",                        L"Jafari",                               16.0, 14.0, 0,   4.0}},
        {"Oman",      {L"سلطنة عُمان",                    L"Oman",                                 18.0, 18.0, 0,   0}},
        {"Jordan",    {L"الأردن وفلسطين",                 L"Jordan & Palestine",                   18.0, 18.0, 0,   0}},
        {"Syria",     {L"سوريا",                          L"Syria",                                18.5, 17.5, 0,   0}},
        {"Tunisia",   {L"تونس",                           L"Tunisia",                              18.0, 18.0, 0,   0}},
        {"Algeria",   {L"الجزائر",                        L"Algeria",                              18.0, 17.0, 0,   0}},
    };
    return M;
}

const char* NAMES[COUNT] = {"fajr", "sunrise", "dhuhr", "asr", "maghrib", "isha"};

static double julian(int y, int m, int d) {
    if (m <= 2) { y -= 1; m += 12; }
    double a = std::floor(y / 100.0);
    double b = 2 - a + std::floor(a / 4.0);
    return std::floor(365.25 * (y + 4716)) + std::floor(30.6001 * (m + 1)) + d + b - 1524.5;
}

static void sunPosition(double jd, double& decl, double& eqt) {
    double dd = jd - 2451545.0;
    double g  = fixangle(357.529 + 0.98560028 * dd);
    double q  = fixangle(280.459 + 0.98564736 * dd);
    double ll = fixangle(q + 1.915 * dsin(g) + 0.020 * dsin(2 * g));
    double e  = 23.439 - 0.00000036 * dd;
    double ra = fixhour(datan2(dcos(e) * dsin(ll), dcos(ll)) / 15.0);
    decl = dasin(dsin(e) * dsin(ll));
    eqt  = q / 15.0 - ra;
}

PrayerTimes::PrayerTimes(double lat, double lng, double tz,
                         const std::string& method, int asrFactor)
    : lat_(lat), lng_(lng), tz_(tz), asr_(asrFactor) {
    auto it = methods().find(method);
    m_ = (it != methods().end()) ? it->second : methods().at("Egypt");
}

double PrayerTimes::midDay(double jd, double t) const {
    double decl, eqt;
    sunPosition(jd + t, decl, eqt);
    return fixhour(12.0 - eqt);
}

double PrayerTimes::sunAngleTime(double jd, double angle, double t, bool ccw) const {
    double decl, eqt;
    sunPosition(jd + t, decl, eqt);
    double noon = midDay(jd, t);
    double x = (-dsin(angle) - dsin(decl) * dsin(lat_)) / (dcos(decl) * dcos(lat_));
    if (x < -1.0 || x > 1.0) return std::nan("");
    double v = dacos(x) / 15.0;
    return noon + (ccw ? -v : v);
}

double PrayerTimes::asrTime(double jd, double t) const {
    double decl, eqt;
    sunPosition(jd + t, decl, eqt);
    double angle = -dacot(asr_ + dtan(std::fabs(lat_ - decl)));
    return sunAngleTime(jd, angle, t, false);
}

static double timeDiff(double a, double b) { return fixhour(b - a); }

double PrayerTimes::nightPortion(double angle, double night) const {
    return angle / 60.0 * night;    // AngleBased — نفس افتراضي النسخة المرجعية
}

Times PrayerTimes::compute(int y, int mo, int d) const {
    double jd = julian(y, mo, d) - lng_ / (15.0 * 24.0);

    double fajr = 5, sunrise = 6, dhuhr = 12, asr = 13,
           sunset = 18, maghrib = 18, isha = 18;

    for (int i = 0; i < 3; i++) {
        fajr    = sunAngleTime(jd, m_.fajr, fajr / 24.0, true);
        sunrise = sunAngleTime(jd, 0.833, sunrise / 24.0, true);
        dhuhr   = midDay(jd, dhuhr / 24.0);
        asr     = asrTime(jd, asr / 24.0);
        sunset  = sunAngleTime(jd, 0.833, sunset / 24.0, false);
        maghrib = sunset;
        if (m_.ishaMin > 0) isha = sunset + m_.ishaMin / 60.0;
        else                isha = sunAngleTime(jd, m_.isha, isha / 24.0, false);
    }

    if (m_.maghribAng > 0)
        maghrib = sunAngleTime(jd, m_.maghribAng, sunset / 24.0, false);

    // معالجة العروض العالية (AngleBased) — مطابقة لـ_adjust_high_lats
    {
        double night = timeDiff(sunset, sunrise);
        {
            double portion = nightPortion(m_.fajr, night);
            double diff = timeDiff(fajr, sunrise);
            if (std::isnan(fajr) || diff > portion) fajr = sunrise - portion;
        }
        if (m_.ishaMin <= 0) {
            double portion = nightPortion(m_.isha, night);
            double diff = timeDiff(sunset, isha);
            if (std::isnan(isha) || diff > portion) isha = sunset + portion;
        }
    }

    Times out;
    double raw[COUNT] = {fajr, sunrise, dhuhr, asr, maghrib, isha};
    for (int i = 0; i < COUNT; i++) {
        double v = raw[i] + tz_ - lng_ / 15.0;
        out.h[i] = fixhour(v);
    }
    return out;
}

std::wstring Times::hhmm(int i) const {
    if (std::isnan(h[i])) return L"--:--";
    int total = (int)std::llround(h[i] * 60.0);
    wchar_t buf[8];
    swprintf(buf, 8, L"%02d:%02d", (total / 60) % 24, total % 60);
    return buf;
}

int Times::minuteOfDay(int i) const {
    if (std::isnan(h[i])) return -1;
    return (int)std::llround(h[i] * 60.0) % 1440;
}

} // namespace athan
