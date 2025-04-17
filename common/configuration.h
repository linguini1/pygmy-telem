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

#ifndef CONFIG_PYGMY_TELEM_CONFIGFILE
#define CONFIG_PYGMY_TELEM_CONFIGFILE "/eeprom"
#endif

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Radio configuration parameters */

struct radio_config_s
{
  char callsign[CONFIG_PYGMY_CALLSIGN_LEN + 1]; /* Call sign and '\0' */
  uint32_t frequency; /* Operating frequency in Hz */
  uint32_t bandwidth; /* Operating bandwidth in kHz */
  uint16_t prlen;     /* Packet preamble length */
  uint8_t spread;     /* Spread factor */
  uint8_t mod;        /* Modulation type */
  float txpower;      /* Transmission power in dBm */
};

/* IMU configuration parameters */

struct imu_config_s
{
  uint8_t xl_fsr;        /* FSR of the accelerometer in g */
  uint16_t gyro_fsr;     /* FSR of the gyroscope in dps */
  float xl_offsets[3];   /* Calibration offsets for the accelerometer */
  float gyro_offsets[3]; /* Calibration offsets for the gyroscope */
};

/* Telemetry system configuration. Must be kept at a consistent size since
 * configuration is read from external storage at startup.
 */

struct configuration_s
{
  struct radio_config_s radio; /* Radio parameters */
  struct imu_config_s imu;     /* IMU parameters */
};

#endif /* _PYGMY_CONFIG_H_ */
