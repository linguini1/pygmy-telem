/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#if defined(CONFIG_RP2040_ADC)
#include <nuttx/analog/adc.h>
#endif

#include <uORB/uORB.h>

#include "../common/configuration.h"
#include "../packets/packets.h"
#include "arguments.h"
#include "syncro.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Array length helper */

#define array_len(arr) sizeof(arr) / sizeof((arr)[0])

/* Sensor indexes */

enum sensor_kind
{
#ifdef CONFIG_SENSORS_MS56XX
  SENSOR_BARO,
#endif
#ifdef CONFIG_SENSORS_LSM6DSO32
  SENSOR_ACCEL,
  SENSOR_GYRO,
#endif
#ifdef CONFIG_SENSORS_LIS2MDL
  SENSOR_MAG,
#endif
};

/* Sensor sampling frequencies (Hz) */

#ifndef CONFIG_PYGMY_BARO_FREQ
#define CONFIG_PYGMY_BARO_FREQ 25
#endif

#ifndef CONFIG_PYGMY_ACCEL_FREQ
#define CONFIG_PYGMY_ACCEL_FREQ 50
#endif

#ifndef CONFIG_PYGMY_GYRO_FREQ
#define CONFIG_PYGMY_GYRO_FREQ 50
#endif

#ifndef CONFIG_PYGMY_MAG_FREQ
#define CONFIG_PYGMY_MAG_FREQ 50
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Buffer for packet header construction */

static struct packet_hdr_s pkt_hdr;

/* Packet under construction */

static struct packet_s pkt_a;
static struct packet_s pkt_b;

/* Buffer for constructing packets */

static uint8_t pkt_bufa[CONFIG_PYGMY_PACKET_MAXLEN];
static uint8_t pkt_bufb[CONFIG_PYGMY_PACKET_MAXLEN];

static struct packet_s *pkt_cur = &pkt_a;
static struct packet_s *pkt_prev = &pkt_b;

/* Buffer to store blocks under construction temporarily */

static uint8_t block_buf[32];

/* Buffer to store some sensor data temporarily */

static uint8_t uorb_data[32];

/* uORB sensor polling */

struct pollfd fds[] = {
    [SENSOR_BARO] = {.fd = -1, .events = POLLIN, .revents = 0},
    [SENSOR_ACCEL] = {.fd = -1, .events = POLLIN, .revents = 0},
    [SENSOR_GYRO] = {.fd = -1, .events = POLLIN, .revents = 0},
    [SENSOR_MAG] = {.fd = -1, .events = POLLIN, .revents = 0},
};

/* uORB sensor metadata */

struct orb_metadata const *metas[] = {
    [SENSOR_BARO] = NULL,
    [SENSOR_ACCEL] = NULL,
    [SENSOR_GYRO] = NULL,
    [SENSOR_MAG] = NULL,
};

/* uORB sensor frequencies */

static const uint32_t frequencies[] = {
    [SENSOR_BARO] = CONFIG_PYGMY_BARO_FREQ,
    [SENSOR_ACCEL] = CONFIG_PYGMY_ACCEL_FREQ,
    [SENSOR_GYRO] = CONFIG_PYGMY_GYRO_FREQ,
    [SENSOR_MAG] = CONFIG_PYGMY_MAG_FREQ,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/*
 * Packages some uORB sensor data as a block in the current packet depending
 * which sensor it originated from.
 * WARNING: modifies `pkt_cur`, global within this thread
 *
 * @param sensor The sensor which the data came from
 * @param data The data read from the sensor
 * @param buf The buffer to use to put the block in
 * @return 0 on success, ENOMEM on no more packet space
 */
static int package_uorb(enum sensor_kind sensor, void *data, void *buf)
{
  int err = 0;

  switch (sensor)
    {
    case SENSOR_BARO:
      {
        /* Pressure data */

        block_init_pressure(buf, data);
        err = packet_push_block(pkt_cur, PACKET_PRESS, buf, sizeof(press_p));
        if (err == ENOMEM) break;

        /* Temperature data */

        block_init_temp(buf, data);
        err = packet_push_block(pkt_cur, PACKET_TEMP, buf, sizeof(temp_p));
        if (err == ENOMEM) break;

        /* Altitude data */

        block_init_alt(buf, data);
        err = packet_push_block(pkt_cur, PACKET_ALT, buf, sizeof(alt_p));
        if (err == ENOMEM) break;
      }
    case SENSOR_ACCEL:
      {
        /* Accelerometer data */

        block_init_accel(buf, data);
        err = packet_push_block(pkt_cur, PACKET_ACCEL, buf, sizeof(accel_p));
        break;
      }
    case SENSOR_GYRO:
      {
        /* Gyro data */

        block_init_gyro(buf, data);
        err = packet_push_block(pkt_cur, PACKET_GYRO, buf, sizeof(gyro_p));
        break;
      }
    case SENSOR_MAG:
      {
        /* Magnetometer data */

        block_init_mag(buf, data);
        err = packet_push_block(pkt_cur, PACKET_MAG, buf, sizeof(mag_p));
        break;
      }
    }

  /* GPS data TODO */

  return err;
}

/****************************************************************************
 * Name: to_millivolts
 *
 * Description:
 *   Converts an ADC reading to a millivolts battery reading. Assumes maximum
 *   battery voltage is 4.2V.
 *
 ****************************************************************************/

#if defined(CONFIG_RP2040_ADC)
uint16_t to_millivolts(struct adc_msg_s *reading)
{
  /* NOTE: the resistor divider on the board will read 3.231V when the battery
   * voltage is at 4.2. Hence, for a more accurate reading, we use the
   * constant of 4.3 volts in our calculation. This number would result in a
   * full 3.308V measurement from the ADC, so our calculated battery voltage
   * will be closer to the real 4.2.
   */
  return (4300 * (reading->am_data >> 16)) / (32768);
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: packet_thread
 *
 * Description:
 *   Constructs packets of sensor data for consumption by radio and logging
 *   thread.
 *
 ****************************************************************************/

void *packet_thread(void *arg)
{
  int err;
#if defined(CONFIG_RP2040_ADC)
  int adc;
  struct adc_msg_s voltage;
#endif

  /* Unpack arguments */

  syncro_t *syncro = args_syncro(arg);
  struct configuration_s *config = args_config(arg);

  /* Prepare packet header for constructing packets */

  packet_header_init(&pkt_hdr, config->radio.callsign, 0);

  /* Prepare packets for construction */

  packet_init(&pkt_a, pkt_bufa);
  packet_init(&pkt_b, pkt_bufb);

  /* Get file descriptor to ADC for battery measurements */

#if defined(CONFIG_RP2040_ADC)
  adc = open("/dev/adc0", O_RDONLY);
  if (adc < 0)
    {
      err = errno;
      fprintf(stderr, "Could not open ADC device: %d\n", err);
    }
#endif

    /* Get sensor metadata */

#ifdef CONFIG_SENSORS_MS56XX
  metas[SENSOR_BARO] = orb_get_meta("sensor_baro");
#endif
#ifdef CONFIG_SENSORS_LSM6DSO32
  metas[SENSOR_ACCEL] = orb_get_meta("sensor_accel");
  metas[SENSOR_GYRO] = orb_get_meta("sensor_gyro");
#endif
#ifdef CONFIG_SENSORS_LIS2MDL
  metas[SENSOR_MAG] = orb_get_meta("sensor_mag");
#endif

  /* Subscribe to all sensors */

  for (int i = 0; i < array_len(fds); i++)
    {
      fds[i].fd = orb_subscribe(metas[i]);
      if (fds[i].fd < 0)
        {
          fprintf(stderr, "Failed to subscribe to sensor '%s': %d\n",
                  metas[i]->o_name, errno);
        }
    }

  /* Set sensor frequencies */

  for (int i = 0; i < array_len(frequencies); i++)
    {
      err = orb_set_frequency(fds[i].fd, frequencies[i]);
      if (err < 0)
        {
          fprintf(stderr, "Couldn't set frequency of '%s'\n",
                  metas[i]->o_name);
        }
    }

  /* Create packets while sampling sensors continually. */

  for (;;)
    {
      /* Reset current packet for fresh construction */

      packet_reset(pkt_cur);

      /* Add header to packet */

      err = packet_push(pkt_cur, &pkt_hdr, sizeof(pkt_hdr));
      if (err)
        {
          fprintf(stderr, "Out of packet space!\n");
        }

        /* Construct a packet from sensor data */

        /* Add one battery measurement to every packet */

#if defined(CONFIG_RP2040_ADC)
      ssize_t b_read = read(adc, &voltage, sizeof(voltage));
      if (b_read < 0)
        {
          err = errno;
          fprintf(stderr, "Couldn't read battery voltage: %d", err);
          goto uorb_collection;
        }
      else if (b_read < sizeof(voltage))
        {
          fprintf(stderr, "Couldn't read full battery voltage\n");
          goto uorb_collection;
        }

      /* We read some battery voltage, time to make a packet */

      block_init_volt((void *)block_buf, to_millivolts(&voltage));
      err =
          packet_push_block(pkt_cur, PACKET_VOLT, block_buf, sizeof(volt_p));
      if (err == ENOMEM) break;
#endif

    uorb_collection:
      for (;;)
        {
          /* Poll forever until some data is available */

          err = poll(fds, array_len(fds), -1);

          if (err < 0)
            {
              fprintf(stderr, "Error polling sensors: %d\n", errno);
              continue;
            }

          /* Polling worked and we have some data to package */

          for (int i = 0; i < array_len(fds); i++)
            {
              /* Some data available on this sensor */

              if (fds[i].revents & POLLIN)
                {
                  fds[i].revents = 0; /* Reset events */

                  err = orb_copy(metas[i], fds[i].fd, uorb_data);
                  if (err < 0)
                    {
                      fprintf(stderr, "Error copying uORB data: %d\n", err);
                      continue;
                    }

                  /* Package according to sensor */

                  err = package_uorb(i, uorb_data, block_buf);

                  /* Out of packet space, stop reading this set of poll events
                   */
                  if (err == ENOMEM) break;
                }
            }

          /* We exited the collection loop after polling, if out of memory in
           * this packet then break completely to finish it */

          if (err == ENOMEM) break;
        }

      /* Share this packet with other threads using syncro monitor */

      err = syncro_publish(syncro, pkt_cur);
      if (err)
        {
          fprintf(stderr, "Couldn't publish new packet: %d\n", err);
        }

      /* Swap to already consumed packet buffer for next iteration */

      struct packet_s *tmp = pkt_cur;
      pkt_cur = pkt_prev;
      pkt_prev = tmp;

      /* Update packet sequence number */

      pkt_hdr.num++;
    }

  return 0;
}
