// This file is part of BOINC.
// http://boinc.berkeley.edu
// Copyright (C) 2020 University of California
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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include "hostinfo.h"   // for P_FEATURES_SIZE
#include "filesys.h"    // for boinc_fopen()
#include "file_names.h" // for EMULATED_CPU_INFO_FILENAME

#ifdef __APPLE__
static void get_cpu_info_mac() {
    char buf[16384];
    size_t len;
    char brand_string[256];
    char features[P_FEATURES_SIZE];
    char *p;
    char *sep=" ";
    int family, stepping, model, feature;
    char host_p_model[16384];
    char host_p_vendor[16384];

    len = sizeof(brand_string);
    sysctlbyname("machdep.cpu.brand_string", brand_string, &len, NULL, 0);

#if defined(__i386__) || defined(__x86_64__)

    // on an Apple M1 chip the cpu.vendor is broken, family, model and stepping don't exist
    if (!strncmp(brand_string, "Apple M", strlen("Apple M"))) {

        strcpy(host_p_vendor, "Apple");
        strncpy(host_p_model, brand_string, sizeof(host_p_model));

    } else {

        len = sizeof(host_p_vendor);
        sysctlbyname("machdep.cpu.vendor", host_p_vendor, &len, NULL, 0);

	len = sizeof(family);
	sysctlbyname("machdep.cpu.family", &family, &len, NULL, 0);

	len = sizeof(model);
	sysctlbyname("machdep.cpu.model", &model, &len, NULL, 0);

	len = sizeof(stepping);
	sysctlbyname("machdep.cpu.stepping", &stepping, &len, NULL, 0);

        snprintf(
            host_p_model, sizeof(host_p_model),
            "%s [x86 Family %d Model %d Stepping %d]",
            brand_string, family, model, stepping
        );
    }

    len = sizeof(features);
    sysctlbyname("machdep.cpu.features", features, &len, NULL, 0);

#else // defined(__i386__) || defined(__x86_64__)

    strcpy(host_p_vendor, "Apple");
    strncpy(host_p_model, brand_string, sizeof(host_p_model));

    features[0] = '\0';
    len = sizeof(feature);

    sysctlbyname("hw.optional.amx_version", &feature, &len, NULL, 0);
    snprintf(features, sizeof(features), "amx_version_%d", feature);

    sysctlbyname("hw.optional.arm64", &feature, &len, NULL, 0);
    if (feature) strncat(features, " arm64", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.armv8_1_atomics", &feature, &len, NULL, 0);
    if (feature) strncat(features, " armv8_1_atomics", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.armv8_2_fhm", &feature, &len, NULL, 0);
    if (feature) strncat(features, " armv8_2_fhm", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.armv8_2_sha3", &feature, &len, NULL, 0);
    if (feature) strncat(features, " armv8_2_sha3", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.armv8_2_sha512", &feature, &len, NULL, 0);
    if (feature) strncat(features, " armv8_2_sha512", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.armv8_crc32", &feature, &len, NULL, 0);
    if (feature) strncat(features, " armv8_crc32", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.floatingpoint", &feature, &len, NULL, 0);
    if (feature) strncat(features, " floatingpoint", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.neon", &feature, &len, NULL, 0);
    if (feature) strncat(features, " neon", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.neon_fp16", &feature, &len, NULL, 0);
    if (feature) strncat(features, " neon_fp16", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.neon_hpfp", &feature, &len, NULL, 0);
    if (feature) strncat(features, " neon_hpfp", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.ucnormal_mem", &feature, &len, NULL, 0);
    if (feature) strncat(features, " ucnormal_mem", sizeof(features) - strlen(features) -1);

#endif // defined(__i386__) || defined(__x86_64__)

    fprintf(stdout,"host.brand_string: %s\n", brand_string);
    fprintf(stdout,"host_p_vendor: %s\n", host_p_vendor);
    fprintf(stdout,"host_p_model: %s\n", host_p_model);
    fprintf(stdout,"host.cpu_features: %s\n", features);
}
#endif

int main () {
#if 1
    size_t len;
    char features[P_FEATURES_SIZE];
    FILE*fp;

    len = sizeof(features);
    sysctlbyname("machdep.cpu.features", features, &len, NULL, 0);
    if (fp = boinc_fopen(EMULATED_CPU_INFO_FILENAME, "w")) {
        fprintf(fp," %s\n", features);
        fclose(fp);
    }
    return 0;
#else
    get_cpu_info_mac();
#endif
}
