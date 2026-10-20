/* Original SUB1130A classification of SP09 answer tables. */
static unsigned gibson_keyboard_kind(unsigned pc){switch(pc){
case 0x0445:return NAME_INPUT_NUMBER;
case 0x0549:return NAME_INPUT_ANSWER;
case 0x05fd:return NAME_INPUT_ANSWER;
case 0x0690:return NAME_INPUT_ANSWER;
default:return ~0u;}}
