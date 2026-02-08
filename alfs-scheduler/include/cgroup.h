/**
 * ALFS - Cgroup Management Interface
 */

#ifndef CGROUP_H
#define CGROUP_H

#include "alfs.h"

/**
 * Create a new cgroup
 * @param cgroup_id Cgroup identifier
 * @param cpu_shares CPU shares (weight relative to other cgroups)
 * @param cpu_quota_us CPU quota in microseconds per period (-1 for unlimited)
 * @param cpu_period_us CPU period in microseconds
 * @param cpu_mask Array of allowed CPU IDs
 * @param cpu_mask_count Number of CPUs in mask
 * @return Pointer to new Cgroup or NULL on failure
 */
Cgroup *cgroup_create(const char *cgroup_id, int cpu_shares, 
                      int cpu_quota_us, int cpu_period_us,
                      const int *cpu_mask, int cpu_mask_count);

/**
 * Destroy a cgroup and free memory
 * @param cgroup Cgroup to destroy
 */
void cgroup_destroy(Cgroup *cgroup);

/**
 * Modify cgroup parameters
 * @param cgroup Target cgroup
 * @param cpu_shares New CPU shares (or -1 to keep current)
 * @param cpu_quota_us New CPU quota (or -2 to keep current, -1 for unlimited)
 * @param cpu_period_us New CPU period (or -2 to keep current)
 * @param cpu_mask New CPU mask (or NULL to keep current)
 * @param cpu_mask_count Number of CPUs in new mask
 * @return 0 on success, -1 on failure
 */
int cgroup_modify(Cgroup *cgroup, int cpu_shares, int cpu_quota_us,
                  int cpu_period_us,
                  const int *cpu_mask, int cpu_mask_count);

/**
 * Check if cgroup allows a specific CPU
 * @param cgroup Cgroup to check
 * @param cpu_id CPU ID to check
 * @return true if CPU is allowed, false otherwise
 */
bool cgroup_allows_cpu(const Cgroup *cgroup, int cpu_id);

/**
 * Check if cgroup has remaining quota
 * @param cgroup Cgroup to check
 * @param vtime Current virtual time
 * @return true if quota available or unlimited, false if throttled
 */
bool cgroup_has_quota(const Cgroup *cgroup, int vtime);

/**
 * Account CPU usage for cgroup
 * @param cgroup Target cgroup
 * @param runtime Runtime to account
 */
void cgroup_account_runtime(Cgroup *cgroup, double runtime);

/**
 * Reset quota tracking at start of new period
 * @param cgroup Target cgroup
 * @param vtime Current virtual time
 */
void cgroup_reset_period(Cgroup *cgroup, int vtime);

#endif /* CGROUP_H */
