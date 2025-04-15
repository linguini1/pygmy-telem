#ifndef _PYGMY_ARGUMENTS_H_
#define _PYGMY_ARGUMENTS_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../common/configuration.h"
#include "syncro.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define args_syncro(args) ((struct thread_args_t *)((args)))->syncro
#define args_config(args) ((struct thread_args_t *)((args)))->config

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct thread_args_t
{
  syncro_t *syncro;               /* Synchronization object */
  struct configuration_s *config; /* Configuration object */
};

#endif // _PYGMY_ARGUMENTS_H_
