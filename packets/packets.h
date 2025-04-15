#ifndef _PYGMY_PACKET_H_
#define _PYGMY_PACKET_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <string.h>

#include <uORB/uORB.h>

#include "../common/configuration.h"

/****************************************************************************
 * Pre-processor definitions
 ****************************************************************************/

#define PACKED __attribute__((packed))

/* Maximum packet length in bytes */

#ifndef CONFIG_PYGMY_PACKET_MAXLEN
#define CONFIG_PYGMY_PACKET_MAXLEN 255
#endif

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Header sent with all packets */

struct packet_hdr_s
{
  char callsign[CONFIG_PYGMY_CALLSIGN_LEN]; /* Call sign */
  uint8_t num;                              /* Rolling counter */
} PACKED;

/* Packet representation. */

struct packet_s
{
  uint8_t *contents; /* Packet raw contents buffer */
  size_t len;        /* Packet length in bytes */
};

/* Packet time field (time since boot in milliseconds) */

typedef uint32_t pkt_time_t;

/* Packet types */

typedef enum
{
  PACKET_PRESS = 0x0, /* Pressure in Pa */
  PACKET_TEMP = 0x1,  /* Temperature in millidegrees C */
  PACKET_ALT = 0x2,   /* Altitude in centimetres */
  PACKET_COORD = 0x3, /* Coordinates in 0.1 micro degrees (10^-7) */
  PACKET_ACCEL = 0x4, /* Linear acceleration in cm/s^2 */
  PACKET_GYRO = 0x5,  /* Angular velocity in 0.1dps */
  PACKET_MAG = 0x6,   /* Magnetic field in 0.1 uTesla */
} pkt_kind_e;

/* Coordinate packet */

typedef struct
{
  pkt_time_t time; /* Mission time */
  int32_t lat;     /* Latitude in 0.1 micro degrees */
  int32_t lon;     /* Longitude in 0.1 micro degrees */
} PACKED coord_p;

/* Pressure packet */

typedef struct
{
  pkt_time_t time; /* Mission time */
  int32_t press;   /* Pressure in Pa */
} PACKED press_p;

/* Temperature packet */

typedef struct
{
  pkt_time_t time; /* Mission time */
  int32_t temp;    /* Temperature in millidegrees C */
} PACKED temp_p;

/* Altitude packet */

typedef struct
{
  pkt_time_t time; /* Mission time */
  int32_t alt;     /* Altitude in centimetres */
} PACKED alt_p;

/* Acceleration packet */

typedef struct
{
  pkt_time_t time; /* Mission time */
  int16_t x;       /* Acceleration in x in cm/s^2 */
  int16_t y;       /* Acceleration in y in cm/s^2 */
  int16_t z;       /* Acceleration in z in cm/s^2 */
} PACKED accel_p;

/* Gyroscope packet */

typedef struct
{
  pkt_time_t time; /* Mission time */
  int16_t x;       /* Angular velocity in x in 0.1dps */
  int16_t y;       /* Angular velocity in y in 0.1dps */
  int16_t z;       /* Angular velocity in z in 0.1dps */
} PACKED gyro_p;

/* Magnetometer packet */

typedef struct
{
  pkt_time_t time; /* Mission time */
  int16_t x;       /* Magnetic field in x in 0.1uT */
  int16_t y;       /* Magnetic field in y in 0.1uT */
  int16_t z;       /* Magnetic field in z in 0.1uT */
} PACKED mag_p;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void packet_init(struct packet_s *pkt, void *buf);
void packet_reset(struct packet_s *pkt);
void packet_header_init(struct packet_hdr_s *hdr, char *callsign,
                        uint8_t num);
int packet_push(struct packet_s *pkt, const void *buf, size_t nbytes);
int packet_push_block(struct packet_s *pkt, const uint8_t kind,
                      const void *block, size_t nbytes);

void block_init_pressure(press_p *blk, struct sensor_baro *data);
void block_init_temp(temp_p *blk, struct sensor_baro *data);
void block_init_alt(alt_p *blk, struct sensor_baro *data);
void block_init_accel(accel_p *blk, struct sensor_accel *data);
void block_init_gyro(gyro_p *blk, struct sensor_gyro *data);
void block_init_mag(mag_p *blk, struct sensor_mag *data);

#endif /* _PYGMY_PACKET_H_ */
