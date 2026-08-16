# -*- coding: utf-8 -*-
"""يقارن ناتج المحرّك C++ بالمحرّك البايثون المرجعي — نفس الحالات بالظبط."""
import subprocess, sys, math
from datetime import date
# مسار محرّك الراسبيري — عدّله أو حطّه في متغيّر البيئة ATHAN_APP
sys.path.insert(0, os.environ.get('ATHAN_APP', os.path.join(os.path.dirname(__file__), '..', '..', 'app')))
from athan_times import PrayerTimes

CITIES = {'Giza':(30.0131,31.2089,3.0),'Makkah':(21.3891,39.8579,3.0),
          'Cairo':(30.0444,31.2357,3.0),'London':(51.5074,-0.1278,1.0),
          'Oslo':(59.9139,10.7522,2.0),'Jakarta':(-6.2088,106.8456,7.0),
          'NewYork':(40.7128,-74.0060,-4.0)}

out = subprocess.run([os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_engine.exe')],
                     capture_output=True, text=True).stdout.strip().splitlines()
NAMES = ['fajr','sunrise','dhuhr','asr','maghrib','isha']
bad = 0
for line in out:
    p = line.split(',')
    city, meth, ds, asr = p[0], p[1], p[2], int(p[3])
    cpp = [int(x) for x in p[4:10]]
    y, m, d = map(int, ds.split('-'))
    lat, lng, tz = CITIES[city]
    py = PrayerTimes(lat, lng, tz, meth, asr).times_for(date(y, m, d))
    for i, k in enumerate(NAMES):
        v = py[k]
        pym = -1 if (isinstance(v, float) and math.isnan(v)) else int(round(v*60)) % 1440
        if pym != cpp[i]:
            bad += 1
            print('MISMATCH %s %s %s asr%d %s: py=%s cpp=%s' % (city, meth, ds, asr, k, pym, cpp[i]))
print('cases: %d rows x 6 prayers = %d comparisons' % (len(out), len(out)*6))
print('RESULT:', 'IDENTICAL' if bad == 0 else '%d MISMATCHES' % bad)
