/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 1999-2021 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General
   Public License along with this library.  If not, see
   <http://www.gnu.org/licenses/>. */

#ifndef _MAILUTILS_LOCKER_H
#define _MAILUTILS_LOCKER_H

#include <mailutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* lock expiry time */
#define MU_LOCKER_DEFAULT_EXPIRE_TIME   (10 * 60)
#define MU_LOCKER_DEFAULT_RETRY_COUNT   10
#define MU_LOCKER_DEFAULT_RETRY_SLEEP   1
#define MU_LOCKER_DEFAULT_EXT_LOCKER    "dotlock"

/* return codes for the external locker */
enum
  {
    MU_DL_EX_OK     = 0, /* success */
    MU_DL_EX_ERROR  = 1, /* failed due to some other error */
    MU_DL_EX_NEXIST = 2, /* unlock requested, but file is not locked */
    MU_DL_EX_EXIST  = 3, /* lock requested, but file is already locked */
    MU_DL_EX_PERM   = 4, /* insufficient permissions */
  };

/* Locker types */
enum
  {
    MU_LOCKER_TYPE_DOTLOCK  = 0, /* Dotlock-style locking.  The default. */
    MU_LOCKER_TYPE_EXTERNAL = 1, /* Use external program to lock the file. */
    MU_LOCKER_TYPE_KERNEL   = 2, /* Use kernel locking (flock,lockf,ioctl) */ 
    MU_LOCKER_TYPE_NULL     = 3, /* No locking at all. */
  };

#define MU_LOCKER_TYPE_DEFAULT MU_LOCKER_TYPE_DOTLOCK  

typedef struct
{
  int flags;
  int type;
  unsigned retry_count;
  unsigned retry_sleep;
  unsigned expire_time;
  char *ext_locker;
} mu_locker_hints_t;

/* Locker hint flags */
#define MU_LOCKER_FLAG_RETRY       0x0001 /* retry_count and retry_sleep are set */
#define MU_LOCKER_FLAG_EXPIRE_TIME 0x0002 /* expire_time is set */
#define MU_LOCKER_FLAG_CHECK_PID   0x0004 /* check if lock owner PID is active */
#define MU_LOCKER_FLAG_EXT_LOCKER  0x0008 /* ext_locker is set */
#define MU_LOCKER_FLAG_TYPE        0x0010 /* type is set */   

#define MU_LOCKER_FLAGS_ALL (\
  MU_LOCKER_FLAG_TYPE | \
  MU_LOCKER_FLAG_RETRY | \
  MU_LOCKER_FLAG_EXPIRE_TIME | \
  MU_LOCKER_FLAG_EXT_LOCKER | \
  MU_LOCKER_FLAG_CHECK_PID )
  
enum mu_locker_mode
{
   mu_lck_shr,   /* Shared (advisory) lock */
   mu_lck_exc,   /* Exclusive lock */
   mu_lck_opt    /* Optional lock = shared, if the locker supports it, no
                    locking otherwise */
}; 

#define MU_LOCKFILE_MODE 0644
  
extern int mu_locker_create_ext (mu_locker_t *, const char *, mu_locker_hints_t *);
extern int mu_locker_modify (mu_locker_t, mu_locker_hints_t *);  
extern void mu_locker_destroy (mu_locker_t *);

extern int mu_locker_lock_mode (mu_locker_t, enum mu_locker_mode);
extern int mu_locker_lock          (mu_locker_t);
extern int mu_locker_touchlock     (mu_locker_t);
extern int mu_locker_unlock        (mu_locker_t);
extern int mu_locker_remove_lock   (mu_locker_t);

extern int mu_locker_get_hints (mu_locker_t lck, mu_locker_hints_t *hints);
  
extern mu_locker_hints_t mu_locker_defaults;

/*
 * Deprecated defines and interfaces.
 */
 
/* Legacy definitions for locker defaults */
#define MU_LOCKER_EXPIRE_TIME        MU_LOCKER_DEFAULT_EXPIRE_TIME
#define MU_LOCKER_RETRIES            MU_LOCKER_DEFAULT_RETRY_COUNT
#define MU_LOCKER_RETRY_SLEEP        MU_LOCKER_DEFAULT_RETRY_SLEEP
#define MU_LOCKER_EXTERNAL_PROGRAM   MU_LOCKER_DEFAULT_EXT_LOCKER

/* Legacy definitions of locker types */
#define MU_LOCKER_DOTLOCK       (MU_LOCKER_TYPE_DOTLOCK << 8)
#define MU_LOCKER_EXTERNAL      (MU_LOCKER_TYPE_EXTERNAL << 8)
#define MU_LOCKER_KERNEL        (MU_LOCKER_TYPE_KERNEL << 8)
#define MU_LOCKER_NULL          (MU_LOCKER_TYPE_NULL << 8)

/* Legacy definitions of locker flags (a.k.a. options). */
#define MU_LOCKER_SIMPLE   0x0000
#define MU_LOCKER_RETRY    MU_LOCKER_FLAG_RETRY
#define MU_LOCKER_TIME     MU_LOCKER_FLAG_EXPIRE_TIME
#define MU_LOCKER_PID      MU_LOCKER_FLAG_CHECK_PID

#define MU_LOCKER_DEFAULT  (MU_LOCKER_DOTLOCK | MU_LOCKER_RETRY)

/* The following was used to pack/unpack flags and locker type: */  
#define MU_LOCKER_TYPE_TO_FLAG(t) ((t) << 8)
#define MU_LOCKER_FLAG_TO_TYPE(f) ((f) >> 8)
#define MU_LOCKER_TYPE_MASK 0xff00
#define MU_LOCKER_OPTION_MASK 0x00ff
  
enum mu_locker_set_mode
  {
    mu_locker_assign,
    mu_locker_set_bit,
    mu_locker_clear_bit
  };

extern int mu_locker_create (mu_locker_t *, const char *, int) MU_DEPRECATED;
  
extern int mu_locker_set_default_flags (int, enum mu_locker_set_mode) MU_DEPRECATED;
extern void mu_locker_set_default_retry_timeout (time_t) MU_DEPRECATED;
extern void mu_locker_set_default_retry_count (size_t) MU_DEPRECATED;
extern void mu_locker_set_default_expire_timeout (time_t) MU_DEPRECATED;
extern int mu_locker_set_default_external_program (char const *) MU_DEPRECATED;

extern int mu_locker_set_flags (mu_locker_t, int) MU_DEPRECATED;
extern int mu_locker_mod_flags (mu_locker_t, int, enum mu_locker_set_mode) MU_DEPRECATED;
extern int mu_locker_set_expire_time (mu_locker_t, int) MU_DEPRECATED;
extern int mu_locker_set_retries (mu_locker_t, int) MU_DEPRECATED;
extern int mu_locker_set_retry_sleep (mu_locker_t, int) MU_DEPRECATED;
extern int mu_locker_set_external (mu_locker_t, const char *) MU_DEPRECATED; 

extern int mu_locker_get_flags (mu_locker_t, int *) MU_DEPRECATED;
extern int mu_locker_get_expire_time (mu_locker_t, int *) MU_DEPRECATED;
extern int mu_locker_get_retries (mu_locker_t, int *) MU_DEPRECATED;
extern int mu_locker_get_retry_sleep (mu_locker_t, int *) MU_DEPRECATED;

#ifdef __cplusplus
}
#endif

#endif

