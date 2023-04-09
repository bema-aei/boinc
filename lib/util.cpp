// This file is part of BOINC.
// http://boinc.berkeley.edu
// Copyright (C) 2023 University of California
//
// BOINC is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// BOINC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with BOINC.  If not, see <http://www.gnu.org/licenses/>.

#if defined(_WIN32)
#include "boinc_win.h"
#include "str_replace.h"
#include "win_util.h"
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
#define finite   _finite
#define snprintf _snprintf
#endif

#ifndef M_LN2
#define M_LN2      0.693147180559945309417
#endif

#include "boinc_stdio.h"

#ifndef _WIN32
#include "config.h"
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <cmath>
#if HAVE_IEEEFP_H
#include <ieeefp.h>
extern "C" {
    int finite(double);
}
#endif
#endif

#include "error_numbers.h"
#include "common_defs.h"
#include "filesys.h"
#include "base64.h"
#include "mfile.h"
#include "miofile.h"
#include "parse.h"

#include "util.h"

using std::min;
using std::string;
using std::vector;

#define EPOCHFILETIME_SEC (11644473600.)
#define TEN_MILLION 10000000.

#ifdef GCL_SIMULATOR
double simtime;
#endif

// return time of day (seconds since 1970) as a double
//
double dtime() {
#ifdef GCL_SIMULATOR
    return simtime;
#else
#ifdef _WIN32
    LARGE_INTEGER time;
    FILETIME sysTime;
    double t;
    GetSystemTimeAsFileTime(&sysTime);
    time.LowPart = sysTime.dwLowDateTime;
    time.HighPart = sysTime.dwHighDateTime;  // Time is in 100 ns units
    t = (double)time.QuadPart;    // Convert to 1 s units
    t /= TEN_MILLION;                /* In seconds */
    t -= EPOCHFILETIME_SEC;     /* Offset to the Epoch time */
    return t;
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + (tv.tv_usec/1.e6);
#endif
#endif
}

// return time today 0:00 in seconds since 1970 as a double
//
double dday() {
    double now=dtime();
    return (now-fmod(now, SECONDS_PER_DAY));
}

// sleep for a specified number of seconds
//
void boinc_sleep(double seconds) {
#ifdef _WIN32
    ::Sleep((int)(1000*seconds));
#else
    double end_time = dtime() + seconds - 0.01;
    // sleep() and usleep() can be interrupted by SIGALRM,
    // so we may need multiple calls
    //
    while (1) {
        if (seconds >= 1) {
            sleep((unsigned int) seconds);
        } else {
            usleep((int)fmod(seconds*1000000, 1000000));
        }
        seconds = end_time - dtime();
        if (seconds <= 0) break;
    }
#endif
}

void push_unique(string s, vector<string>& v) {
    for (unsigned int i=0; i<v.size();i++) {
        if (s == v[i]) return;
    }
    v.push_back(s);
}

// Update an estimate of "units per day" of something (credit or CPU time).
// The estimate is exponentially averaged with a given half-life
// (i.e. if no new work is done, the average will decline by 50% in this time).
// This function can be called either with new work,
// or with zero work to decay an existing average.
//
// NOTE: if you change this, also change update_average in
// html/inc/credit.inc
//
void update_average(
    double now,
    double work_start_time,       // when new work was started
                                    // (or zero if no new work)
    double work,                    // amount of new work
    double half_life,
    double& avg,                    // average work per day (in and out)
    double& avg_time                // when average was last computed
) {
    if (avg_time) {
        // If an average R already exists, imagine that the new work was done
        // entirely between avg_time and now.
        // That gives a rate R'.
        // Replace R with a weighted average of R and R',
        // weighted so that we get the right half-life if R' == 0.
        //
        // But this blows up if avg_time == now; you get 0*(1/0)
        // So consider the limit as diff->0,
        // using the first-order Taylor expansion of
        // exp(x)=1+x+O(x^2).
        // So to the lowest order in diff:
        // weight = 1 - diff ln(2) / half_life
        // so one has
        // avg += (1-weight)*(work/diff_days)
        // avg += [diff*ln(2)/half_life] * (work*SECONDS_PER_DAY/diff)
        // notice that diff cancels out, leaving
        // avg += [ln(2)/half_life] * work*SECONDS_PER_DAY

        double diff, diff_days, weight;

        diff = now - avg_time;
        if (diff<0) diff=0;

        diff_days = diff/SECONDS_PER_DAY;
        weight = exp(-diff*M_LN2/half_life);

        avg *= weight;

        if ((1.0-weight) > 1.e-6) {
            avg += (1-weight)*(work/diff_days);
        } else {
            avg += M_LN2*work*SECONDS_PER_DAY/half_life;
        }
    } else if (work) {
        // If first time, average is just work/duration
        //
        double dd = (now - work_start_time)/SECONDS_PER_DAY;
        avg = work/dd;
    }
    avg_time = now;
}

void boinc_crash() {
#ifdef _WIN32
    DebugBreak();
#else
    abort();
#endif
}

bool boinc_is_finite(double x) {
#if defined (HPUX_SOURCE)
    return _Isfinite(x);
#elif defined (__APPLE__)
    // finite() is deprecated in OS 10.9
    return std::isfinite(x) != 0;
#else
    return finite(x) != 0;
#endif
}

#define PI2 (2*3.1415926)

// generate normal random numbers using Box-Muller.
// this generates 2 at a time, so cache the other one
//
double rand_normal() {
    static bool cached;
    static double cached_value;
    if (cached) {
        cached = false;
        return cached_value;
    }
    double u1 = drand();
    double u2 = drand();
    double z = sqrt(-2*log(u1));
    cached_value = z*sin(PI2*u2);
    cached = true;
    return z*cos(PI2*u2);
}

// determines the real path and filename of the current process
// not the current working directory
//
int get_real_executable_path(char* path, size_t max_len) {
#if defined(__APPLE__)
    uint32_t size = (uint32_t)max_len;
    if (_NSGetExecutablePath(path, &size)) {
        return ERR_BUFFER_OVERFLOW;
    }
    return BOINC_SUCCESS;
#elif (defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__)) && defined(KERN_PROC_PATHNAME)
#if defined(__DragonFly__) || defined(__FreeBSD__)
    int name[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
#else
    int name[4] = { CTL_KERN, KERN_PROC_ARGS, -1, KERN_PROC_PATHNAME };
#endif
    if (sysctl(name, 4, path, &max_len, NULL, 0)) {
        return errno == ENOMEM ? ERR_BUFFER_OVERFLOW : ERR_PROC_PARSE;
    }
    return BOINC_SUCCESS;
#elif defined(_WIN32)
    DWORD length = GetModuleFileNameA(NULL, path, (DWORD)max_len);
    if (!length) {
        return ERR_PROC_PARSE;
    } else if (length == (DWORD)max_len) {
        return ERR_BUFFER_OVERFLOW;
    }
    return BOINC_SUCCESS;
#else
    const char* links[] = { "/proc/self/exe", "/proc/curproc/exe", "/proc/self/path/a.out", "/proc/curproc/file" };
    for (unsigned int i = 0; i < sizeof(links) / sizeof(links[0]); ++i) {
        ssize_t ret = readlink(links[i], path, max_len - 1);
        if (ret < 0) {
            if (errno != ENOENT) {
                boinc::perror("readlink");
            }
            continue;
        } else if ((size_t)ret == max_len - 1) {
            return ERR_BUFFER_OVERFLOW;
        }
        path[ret] = '\0'; // readlink does not null terminate
        return BOINC_SUCCESS;
    }
    return ERR_NOT_IMPLEMENTED;
#endif
}
