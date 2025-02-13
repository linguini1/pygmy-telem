#ifndef _PYGMY_CONFIG_H_
#define _PYGMY_CONFIG_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PACKED __attribute__((packed))

/* Length of callsign on packets */

#ifndef CONFIG_PYGMY_CALLSIGN_LEN
#define CONFIG_PYGMY_CALLSIGN_LEN 6
#endif

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Radio configuration parameters */

struct radio_config_s {
  char callsign[CONFIG_PYGMY_CALLSIGN_LEN + 1]; /* Call sign and '\0' */
  uint32_t frequency;                           /* Operating frequency in Hz */
  uint32_t bandwidth;                           /* Operating bandwidth in kHz */
  uint16_t prlen;                               /* Packet preamble length */
  uint8_t spread;                               /* Spread factor */
  uint8_t mod;                                  /* Modulation type */
  float txpower;                                /* Transmission power in dBm */
} PACKED;

/* Telemetry system configuration. Must be kept at a consistent size since
 * configuration is read from external storage at startup.
 */

struct configuration_s {
  struct radio_config_s radio; /* Radio parameters */
} PACKED;

#endif /* _PYGMY_CONFIG_H_ */
