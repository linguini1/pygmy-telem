#ifndef _PYGMY_SYNCRO_H_
#define _PYGMY_SYNCRO_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <pthread.h>
#include <stdbool.h>

#include "../packets/packets.h"

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Synchronization object for packet to be transmitted and logged */

typedef struct
{
  bool logged;           /* True if this packet has been logged */
  bool transmitted;      /* True if this packet has been transmitted */
  struct packet_s *pkt;  /* The packet being shared */
  pthread_mutex_t lock;  /* Mutex over packet modifications */
  pthread_cond_t is_new; /* Condition that new packet was added */
} syncro_t;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int syncro_init(syncro_t *syncro);
int syncro_publish(syncro_t *syncro, struct packet_s *pkt);
int syncro_get_unlogged(syncro_t *syncro, struct packet_s **pkt);
int syncro_get_untransmitted(syncro_t *syncro, struct packet_s **pkt);
int syncro_mark_logged(syncro_t *syncro);
int syncro_mark_transmitted(syncro_t *syncro);

#endif // _PYGMY_SYNCRO_H_
