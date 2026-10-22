#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Startup-only, before services/apps allocate RAM; no flash or SD access.
 * Reserves a persistent internal-stack worker without allocating from the heap. */
bool fw_dualboot_init(void);
/* Does not write flash. Returns a user-facing reason if this is not a valid pair. */
bool fw_dualboot_check(char* error, size_t error_size);
/* Rechecks SD and the companion, then selects it from an internal-stack task.
 * Success reboots; failure returns false without changing the boot selection. */
bool fw_dualboot_start_bruce(char* error, size_t error_size);
