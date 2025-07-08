#define HELP_TEXT \
"Pygmy REV B\n(c) Matteo Golin, 2025\n\nThis shell allows you to configure th" \
"e Pygmy flight computer with new settings\nbefore a flight. Make sure that o" \
"nce you've made changes, you save them to the\nPygmy's on-board memory using" \
" the 'save' command. Once saved, you can start the\nflight computer with you" \
"r new settings by sending the 'reboot' command.\n\nBASIC COMMANDS:\n    \n  " \
"  help        Display this help text.\n    reboot      Reboot the Pygmy.\n  " \
"  save        Save newly configured settings to the Pygmy's EEPROM (memory)." \
"\n    current     Print the currently saved settings.\n    modified    Print" \
" the modified settings.\n    copy        Copy all log files to FAT32 partiti" \
"on for user access.\n\nRADIO CONFIGURATION COMMANDS:\n\n    callsign    Set " \
"the user call sign for signing packets.\n    frequency   Set the operating f" \
"requency of the radio module in Hz.\n    bandwidth   Set the operating bandw" \
"idth of the radio module in kHz.\n    preamble    Set the preamble length fo" \
"r radio packets, in bytes.\n    spread      Set the spread factor of the rad" \
"io.\n    mod         Set the modulation mode of the radio. Either 'lora' or " \
"'fsk'.\n    txpower     Set the radio transmit power in dBm.\n\nIMU CONFIGUR" \
"ATION COMMANDS:\n\n    xl_fsr      Set the full scale range of the accelerom" \
"eter, in Gs.\n    gyro_fsr    Set the full scale range of the gyroscope in d" \
"egrees per second.\n    xl_off      Set the calibration offsets of the accel" \
"erometer in m/s^2.\n                Offsets are subtracted from readings.\n " \
"   gyro_off    Set the calibration offsets of the accelerometer in m/s^2.\n " \
"               Offsets are subtracted from readings.\n"
