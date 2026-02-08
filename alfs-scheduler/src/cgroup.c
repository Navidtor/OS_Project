/**
 * ALFS - Cgroup Management Implementation
 */

#include <stdlib.h>
#include <string.h>
#include "cgroup.h"

Cgroup *cgroup_create(const char *cgroup_id, int cpu_shares, 
                      int cpu_quota_us, int cpu_period_us,
                      const int *cpu_mask, int cpu_mask_count) {
    if (!cgroup_id) {
        return NULL;
    }
    
    Cgroup *cgroup = calloc(1, sizeof(Cgroup));
    if (!cgroup) {
        return NULL;
    }
    
    strncpy(cgroup->cgroup_id, cgroup_id, MAX_CGROUP_ID_LEN - 1);
    cgroup->cgroup_id[MAX_CGROUP_ID_LEN - 1] = '\0';
    
    cgroup->cpu_shares = cpu_shares > 0 ? cpu_shares : DEFAULT_CPU_SHARES;
    cgroup->cpu_quota_us = cpu_quota_us;  /* -1 = unlimited */
    cgroup->cpu_period_us = cpu_period_us > 0 ? cpu_period_us : DEFAULT_CPU_PERIOD_US;
    
    cgroup->quota_used = 0.0;
    cgroup->period_start_vtime = 0;
    
    if (cpu_mask && cpu_mask_count > 0) {
        cgroup->cpu_mask = malloc(sizeof(int) * cpu_mask_count);
        if (!cgroup->cpu_mask) {
            free(cgroup);
            return NULL;
        }
        memcpy(cgroup->cpu_mask, cpu_mask, sizeof(int) * cpu_mask_count);
        cgroup->cpu_mask_count = cpu_mask_count;
    } else {
        cgroup->cpu_mask = NULL;
        cgroup->cpu_mask_count = 0;
    }
    
    return cgroup;
}

void cgroup_destroy(Cgroup *cgroup) {
    if (cgroup) {
        free(cgroup->cpu_mask);
        free(cgroup);
    }
}

int cgroup_modify(Cgroup *cgroup, int cpu_shares, int cpu_quota_us,
                  int cpu_period_us, const int *cpu_mask, int cpu_mask_count) {
    if (!cgroup) {
        return -1;
    }
    
    /* Only modify if value is not sentinel */
    if (cpu_shares > 0) {
        cgroup->cpu_shares = cpu_shares;
    }
    
    /* -2 means "keep current", -1 means "unlimited" */
    if (cpu_quota_us >= -1) {
        cgroup->cpu_quota_us = cpu_quota_us;
    }

    if (cpu_period_us > 0) {
        cgroup->cpu_period_us = cpu_period_us;
    }
    
    if (cpu_mask && cpu_mask_count > 0) {
        free(cgroup->cpu_mask);
        cgroup->cpu_mask = malloc(sizeof(int) * cpu_mask_count);
        if (!cgroup->cpu_mask) {
            cgroup->cpu_mask_count = 0;
            return -1;
        }
        memcpy(cgroup->cpu_mask, cpu_mask, sizeof(int) * cpu_mask_count);
        cgroup->cpu_mask_count = cpu_mask_count;
    }
    
    return 0;
}

bool cgroup_allows_cpu(const Cgroup *cgroup, int cpu_id) {
    if (!cgroup) {
        return true;  /* No cgroup = all CPUs allowed */
    }
    
    /* No mask means all CPUs allowed */
    if (cgroup->cpu_mask_count == 0) {
        return true;
    }
    
    for (int i = 0; i < cgroup->cpu_mask_count; i++) {
        if (cgroup->cpu_mask[i] == cpu_id) {
            return true;
        }
    }
    
    return false;
}

bool cgroup_has_quota(const Cgroup *cgroup, int vtime) {
    (void)vtime;  /* May be used for period tracking */
    
    if (!cgroup) {
        return true;
    }
    
    /* Unlimited quota */
    if (cgroup->cpu_quota_us < 0) {
        return true;
    }
    
    /* Check if quota is exhausted */
    return cgroup->quota_used < (double)cgroup->cpu_quota_us;
}

void cgroup_account_runtime(Cgroup *cgroup, double runtime) {
    if (cgroup && cgroup->cpu_quota_us > 0 && runtime > 0.0) {
        cgroup->quota_used += runtime;
    }
}

void cgroup_reset_period(Cgroup *cgroup, int vtime) {
    if (cgroup) {
        cgroup->quota_used = 0.0;
        cgroup->period_start_vtime = vtime;
    }
}
