#ifndef KERNEL_ERROR_H
#define KERNEL_ERROR_H

#include <kernel/types.h>

/* Error codes */
#define E_SUCCESS       0
#define E_GENERIC      -1
#define E_NOMEM        -2   /* Out of memory */
#define E_INVAL        -3   /* Invalid argument */
#define E_NOENT        -4   /* No such entry */
#define E_PERM         -5   /* Permission denied */
#define E_BUSY         -6   /* Resource busy */
#define E_EXIST        -7   /* Already exists */
#define E_NOTDIR       -8   /* Not a directory */
#define E_ISDIR        -9   /* Is a directory */
#define E_NOSPACE      -10  /* No space left */
#define E_RANGE        -11  /* Out of range */
#define E_IO           -12  /* I/O error */
#define E_BADF         -13  /* Bad file descriptor */
#define E_CHILD        -14  /* No child processes */
#define E_AGAIN        -15  /* Try again */
#define E_NODEV        -16  /* No such device */
#define E_NOTIMPL      -17  /* Not implemented */
#define E_OVERFLOW     -18  /* Value overflow */
#define E_DEADLK       -19  /* Deadlock */
#define E_CORRUPT      -20  /* Data corruption */

/* Error tracking */
void error_set_last(int error_code);
int error_get_last(void);
const char *error_to_string(int error_code);

/* Error reporting */
void error_report(const char *subsystem, int error_code, const char *message);

#endif
