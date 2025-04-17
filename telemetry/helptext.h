#define HELP_TEXT \
"Pygmy REV B\n(c) Matteo Golin, 2025\n\nThis shell allows you to configure th" \
"e Pygmy flight computer with new settings\nbefore a flight. Make sure that o" \
"nce you've made changes, you save them to the\nPygmy's on-board memory using" \
" the 'save' command. Once saved, you can start the\nflight computer with you" \
"r new settings by sending the 'reboot' command.\n\nBASIC COMMANDS:\n    \n  " \
"  help        Display this help text.\n    reboot      Reboot the Pygmy.\n  " \
"  save        Save newly configured settings to the Pygmy's EEPROM (memory)." \
"\n    current     Print the currently saved settings.\n    modified    Print" \
" the modified settings.\n\nRADIO CONFIGURATION COMMANDS:\n\n    callsign    " \
"Set the user call sign for signing packets.\n    frequency   Set the operati" \
"ng frequency of the radio module in Hz.\n    bandwidth   Set the operating b" \
"andwidth of the radio module in kHz.\n    preamble    Set the preamble lengt" \
"h for radio packets, in bytes.\n    spread      Set the spread factor of the" \
" radio.\n    mod         Set the modulation mode of the radio. Either 'lora'" \
" or 'fsk'.\n    txpower     Set the radio transmit power in dBm.\n"
