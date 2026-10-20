/* Original SP28/088C, SUB1130A alpha classification. */
static unsigned source_answer_keyboard_kind(unsigned module,unsigned pc){
    if(module==0x42)return gibson_keyboard_kind(pc);
    if(module==0x55 && pc==0x88c)return NAME_INPUT_ANSWER;
    if(module==0x54 && pc==0x5a7)return NAME_INPUT_ANSWER;
    return ~0u;
}
