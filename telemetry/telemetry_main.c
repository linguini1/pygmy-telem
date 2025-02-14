/****************************************************************************
 * pygmy-telem/telemetry/telemetry_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../common/configuration.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_PYGMY_TELEM_CONFIGFILE
#define CONFIG_PYGMY_TELEM_CONFIGFILE "/eeprom"
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static pthread_t radio_pid;
static pthread_t log_pid;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void *log_thread(void *arg);
void *radio_thread(void *arg);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * telemetry_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{

  int err;
  int configfile;
  ssize_t b_read;
  struct configuration_s config;

  /* Read configuration data */

  configfile = open(CONFIG_PYGMY_TELEM_CONFIGFILE, O_RDONLY);
  if (configfile < 0)
    {
      err = errno;
      fprintf(stderr, "Couldn't open configuration file: %d\n", err);
      return EXIT_FAILURE;
    }

  b_read = read(configfile, &config, sizeof(config));
  if (b_read < 0)
    {
      err = errno;
      fprintf(stderr, "Couldn't read configuration file: %d\n", err);
      close(configfile);
      return EXIT_FAILURE;
    }
  else if (b_read < sizeof(config))
    {
      fprintf(stderr, "Couldn't read complete configuration file: %d\n", err);
      close(configfile);
      return EXIT_FAILURE;
    }

  close(configfile);

  /* Configure sensors and perform setup */

  // TODO: for now, just fill configuration with some valid parameters since
  // EEPROM is full of junk
  config.radio.mod = 0;
  config.radio.prlen = 6;
  config.radio.frequency = 902000000;
  config.radio.spread = 7;
  config.radio.txpower = 18.0f;
  config.radio.bandwidth = 125;
  memcpy(config.radio.callsign, "VA3INI", sizeof("VA3INI") - 1);

  // TODO

  /* Start logging thread. */

  err = pthread_create(&log_pid, NULL, log_thread, NULL);
  if (err < 0)
    {
      fprintf(stderr, "Failed to start logging thread %d\n", err);
    }

  /* Start radio broadcast thread */

  err = pthread_create(&radio_pid, NULL, radio_thread, &config.radio);
  if (err < 0)
    {
      fprintf(stderr, "Failed to start radio thread: %d\n", err);
    }

  pthread_join(log_pid, (void *)&err);
  if (err)
    {
      fprintf(stderr, "Logging thread exited with error: %d\n", err);
    }

  pthread_join(radio_pid, (void *)&err);
  if (err)
    {
      fprintf(stderr, "Radio thread exited with error: %d\n", err);
    }

  return EXIT_SUCCESS;
}
