/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <pthread.h>
#include <stdbool.h>

#include "../packets/packets.h"
#include "syncro.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: syncro_init
 *
 * Description:
 *   Initializes the synchronization monitor.
 *
 * Parameters:
 *   syncro - The monitor the be initialized
 *
 * Return: 0 on success, errno error code on failure (mutex/cond init)
 *
 ****************************************************************************/

int syncro_init(syncro_t *syncro)
{
  int err;

  syncro->logged = false;
  syncro->transmitted = false;
  syncro->pkt = NULL;

  err = pthread_mutex_init(&syncro->lock, NULL);
  if (err) return err;

  err = pthread_cond_init(&syncro->is_new, NULL);
  return err;
}

/****************************************************************************
 * Name: syncro_publish
 *
 * Description:
 *   Publishes a new packet to subscribing threads.
 *
 * Parameters:
 *   syncro - The monitor object
 *   pkt - The packet to be published
 *
 * Return: 0 on success, errno error code on failure (mutex/cond init)
 *
 ****************************************************************************/

int syncro_publish(syncro_t *syncro, struct packet_s *pkt)
{
  int err;

  /* Exclusive access */

  err = pthread_mutex_lock(&syncro->lock);
  if (err) return err;

  syncro->pkt = pkt;           /* Store new packet */
  syncro->logged = false;      /* New packet not yet logged */
  syncro->transmitted = false; /* New packet not yet transmitted */
  pthread_cond_broadcast(&syncro->is_new); /* Signal change to listeners */

  /* Release access */

  pthread_mutex_unlock(&syncro->lock);
  return err;
}

/****************************************************************************
 * Name: syncro_get_unlogged
 *
 * Description:
 *   Waits for a packet that hasn't been logged and obtains a pointer to it
 *
 * Parameters:
 *   syncro - The monitor object
 *   pkt - Where to store the pointer to the new packet
 *
 * Return: 0 on success, errno error code on failure (mutex lock)
 *
 ****************************************************************************/

int syncro_get_unlogged(syncro_t *syncro, struct packet_s **pkt)
{
  int err;

  /* Exclusive access */

  err = pthread_mutex_lock(&syncro->lock);
  if (err) return err;

  /* Wait for change in packet status */

  while (!syncro->logged)
    {
      pthread_cond_wait(&syncro->is_new, &syncro->lock);
    }

  /* A new packet that hasn't been logged yet was acquired, return it to the
   * user under the lock still */

  *pkt = syncro->pkt;

  return 0;
}

/****************************************************************************
 * Name: syncro_get_untransmitted
 *
 * Description:
 *   Waits for a packet that hasn't been transmitted and obtains a pointer to
 *it
 *
 * Parameters:
 *   syncro - The monitor object
 *   pkt - Where to store the pointer to the new packet
 *
 * Return: 0 on success, errno error code on failure (mutex lock)
 *
 ****************************************************************************/

int syncro_get_untransmitted(syncro_t *syncro, struct packet_s **pkt)
{
  int err;

  /* Exclusive access */

  err = pthread_mutex_lock(&syncro->lock);
  if (err) return err;

  /* Wait for change in packet status */

  while (!syncro->transmitted)
    {
      pthread_cond_wait(&syncro->is_new, &syncro->lock);
    }

  /* A new packet that hasn't been logged yet was acquired, return it to the
   * user under the lock still */

  *pkt = syncro->pkt;

  return 0;
}

/****************************************************************************
 * Name: syncro_mark_logged
 *
 * Description:
 *   Mark the currently active packet as logged
 *
 * Parameters:
 *   syncro - The monitor object
 *
 * Return: 0 on success, errno error code on failure (mutex unlock)
 *
 ****************************************************************************/

int syncro_mark_logged(syncro_t *syncro)
{
  syncro->logged = true;
  return pthread_mutex_unlock(&syncro->lock);
}

/****************************************************************************
 * Name: syncro_mark_transmitted
 *
 * Description:
 *   Mark the currently active packet as transmitted
 *
 * Parameters:
 *   syncro - The monitor object
 *
 * Return: 0 on success, errno error code on failure (mutex unlock)
 *
 ****************************************************************************/

int syncro_mark_transmitted(syncro_t *syncro)
{
  syncro->transmitted = true;
  return pthread_mutex_unlock(&syncro->lock);
}
