#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#define P_FEATURES_SIZE 16384
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

    sysctlbyname("hw.optional.armv8_1_atomics", buf, &len, NULL, 0);
    if (feature) strncat(features, " armv8_1_atomics", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.armv8_2_fhm", buf, &len, NULL, 0);
    if (feature) strncat(features, " armv8_2_fhm", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.armv8_2_sha3", buf, &len, NULL, 0);
    if (feature) strncat(features, " armv8_2_sha3", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.armv8_2_sha512", buf, &len, NULL, 0);
    if (feature) strncat(features, " armv8_2_sha512", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.armv8_crc32", buf, &len, NULL, 0);
    if (feature) strncat(features, " armv8_crc32", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.floatingpoint", buf, &len, NULL, 0);
    if (feature) strncat(features, " floatingpoint", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.neon", buf, &len, NULL, 0);
    if (feature) strncat(features, " neon", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.neon_fp16", buf, &len, NULL, 0);
    if (feature) strncat(features, " neon_fp16", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.neon_hpfp", buf, &len, NULL, 0);
    if (feature) strncat(features, " neon_hpfp", sizeof(features) - strlen(features) -1);

    sysctlbyname("hw.optional.ucnormal_mem", buf, &len, NULL, 0);
    if (feature) strncat(features, " ucnormal_mem", sizeof(features) - strlen(features) -1);

#endif // defined(__i386__) || defined(__x86_64__)

    fprintf(stdout,"host.brand_string: %s\n", brand_string);
    fprintf(stdout,"host_p_vendor: %s\n", host_p_vendor);
    fprintf(stdout,"host_p_model: %s\n", host_p_model);
    fprintf(stdout,"host.cpu_features: %s\n", features);
}
#endif

int main () {
    get_cpu_info_mac();
#if defined(__arm64__)
    fprintf(stdout,"arm64\n");
#endif
}
