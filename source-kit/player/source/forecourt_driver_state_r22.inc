/* Portable source-computed driver boundary state. Included in adapter.c.
 * CPU callbacks, memory-map pointers and exception jump buffers are not data.
 * Every addressable page is retained as canonical bus bytes, with zero runs.
 */
typedef struct {
 uint8_t *out;const uint8_t *in;unsigned at,size;int ok,apply;unsigned required_events,required_drums;
} driver_state_io;
static unsigned driver_word(driver_state_io *io,unsigned value){
 if(io->at>io->size || io->size-io->at<4){io->ok=0;return 0;}
 if(io->in){const uint8_t *p=io->in+io->at;value=p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
 if(io->out)for(unsigned i=0;i<4;i++)io->out[io->at+i]=(uint8_t)(value>>(8*i));
 io->at+=4;return value;
}
#define DRIVER_FIELD(x) do{unsigned v=driver_word(io,(unsigned)(x));if(io->apply)(x)=v;}while(0)
static unsigned driver_state_fields(driver_state_io *io){
 DRIVER_FIELD(m68k.poll.pc);DRIVER_FIELD(m68k.poll.cycle);DRIVER_FIELD(m68k.poll.detected);
 DRIVER_FIELD(m68k.cycles);DRIVER_FIELD(m68k.refresh_cycles);DRIVER_FIELD(m68k.cycle_end);
 for(unsigned i=0;i<16;i++)DRIVER_FIELD(m68k.dar[i]);
 DRIVER_FIELD(m68k.pc);DRIVER_FIELD(m68k.prev_pc);
 for(unsigned i=0;i<8;i++){DRIVER_FIELD(m68k.prev_dr[i]);DRIVER_FIELD(m68k.prev_ar[i]);}
 for(unsigned i=0;i<5;i++)DRIVER_FIELD(m68k.sp[i]);
 DRIVER_FIELD(m68k.ir);DRIVER_FIELD(m68k.t1_flag);DRIVER_FIELD(m68k.s_flag);
 DRIVER_FIELD(m68k.x_flag);DRIVER_FIELD(m68k.n_flag);DRIVER_FIELD(m68k.not_z_flag);
 DRIVER_FIELD(m68k.v_flag);DRIVER_FIELD(m68k.c_flag);DRIVER_FIELD(m68k.int_mask);
 DRIVER_FIELD(m68k.int_level);DRIVER_FIELD(m68k.stopped);
 DRIVER_FIELD(m68k.pref_addr);DRIVER_FIELD(m68k.pref_data);
 DRIVER_FIELD(m68k.instr_mode);DRIVER_FIELD(m68k.run_mode);DRIVER_FIELD(m68k.aerr_enabled);
 DRIVER_FIELD(m68k.aerr_address);DRIVER_FIELD(m68k.aerr_write_mode);DRIVER_FIELD(m68k.aerr_fc);
 DRIVER_FIELD(m68k.tracing);DRIVER_FIELD(m68k.address_space);
 DRIVER_FIELD(frame);DRIVER_FIELD(busy_until);DRIVER_FIELD(status_reads);DRIVER_FIELD(busy_reads);
 DRIVER_FIELD(instructions);DRIVER_FIELD(refresh_enabled);
 return io->ok;
}
#undef DRIVER_FIELD
static int driver_bytes(driver_state_io *io,uint8_t *data,unsigned bytes){
 /* RLE consists of a zero skip followed by a nonzero literal run. Word-
  * swapped core storage is explicitly converted to/from bus byte order. */
 unsigned at=0;
 while(at<bytes){
  unsigned zero=0,literal=0;
  if(!io->in){
   while(at+zero<bytes && !data[(at+zero)^1])zero++;
   while(at+zero+literal<bytes){
    if(data[(at+zero+literal)^1]){literal++;continue;}
    unsigned gap=0;
    while(at+zero+literal+gap<bytes && !data[(at+zero+literal+gap)^1])gap++;
    if(gap>=16)break;
    literal+=gap;
   }
  }
  zero=driver_word(io,zero);literal=driver_word(io,literal);
  if(!io->ok || zero>bytes-at || literal>bytes-at-zero || !(zero|literal) ||
     io->at>io->size || literal>io->size-io->at)return 0;
  if(io->apply)for(unsigned i=0;i<zero;i++)data[(at+i)^1]=0;
  at+=zero;
  for(unsigned i=0;i<literal;i++){
   if(io->out)io->out[io->at+i]=data[(at+i)^1];
   if(io->apply)data[(at+i)^1]=io->in[io->at+i];
  }
  at+=literal;io->at+=literal;
 }
 return 1;
}
static int driver_payload(driver_state_io *io){
 if(!driver_state_fields(io) || !driver_bytes(io,memory,4u<<16) ||
    !driver_bytes(io,io_a1,sizeof(io_a1)) || !driver_bytes(io,io_c0,sizeof(io_c0)))return 0;
 unsigned event_count=driver_word(io,count),drums_count=driver_word(io,drum_count);
 if(!io->ok || event_count>4096 || drums_count>256)return 0;
 io->required_events=event_count;io->required_drums=drums_count;
 if(io->apply && (event_count>capacity || drums_count>drum_capacity))return 0;
 for(unsigned i=0;i<event_count;i++){
  probe_event e={0};if(!io->in)e=events[i];
  e.frame=driver_word(io,e.frame);e.master=driver_word(io,e.master);
  unsigned port=driver_word(io,e.port),data=driver_word(io,e.data);
  if(port>4 || data>255)return 0;
  e.port=port;e.data=data;if(io->apply)events[i]=e;
 }
 for(unsigned i=0;i<drums_count;i++){
  probe_drum d={0};if(!io->in)d=drums[i];
  d.frame=driver_word(io,d.frame);d.address=driver_word(io,d.address);
  d.size=driver_word(io,d.size);d.value=driver_word(io,d.value);
  if(d.address<0xa1201a || d.address>0xa1201d || d.size!=1 || d.value>255)return 0;
  if(io->apply)drums[i]=d;
 }
 if(io->apply){count=event_count;drum_count=drums_count;failure=0;bad_address=0;}
 return io->ok;
}
unsigned fm_driver_export(void *data,unsigned size){
 if(!memory || !data || failure || bad_address || m68k.stopped || m68k.pc!=0x1fffc)return 0;
 driver_state_io io={data,NULL,0,size,1,0,0,0};
 driver_word(&io,0x44534d46);driver_word(&io,1);
 return driver_payload(&io)?io.at:0;
}
static int driver_validate_state(const void *data,unsigned size,driver_state_io *io){
 if(!data || !memory)return 0;
 *io=(driver_state_io){NULL,data,0,size,1,0,0,0};
 return driver_word(io,0)==0x44534d46 && driver_word(io,0)==1 &&
    driver_payload(io) && io->at==size;
}
int fm_driver_prepare_state(const void *data,unsigned size,unsigned *event_total){
 driver_state_io io;
 if(!driver_validate_state(data,size,&io))return 0;
 /* Allocate before changing any machine state. Retain all bounded events. */
 unsigned needed=512,drums_needed=32;
 while(needed<io.required_events)needed*=2;
 while(drums_needed<io.required_drums)drums_needed*=2;
 if(capacity<needed){void *p=realloc(events,needed*sizeof(*events));if(!p)return 0;events=p;capacity=needed;}
 if(drum_capacity<drums_needed){void *p=realloc(drums,drums_needed*sizeof(*drums));if(!p)return 0;drums=p;drum_capacity=drums_needed;}
 if(event_total)*event_total=io.required_events;
 return 1;
}
int fm_driver_import(const void *data,unsigned size,int apply){
 driver_state_io io;
 if(!driver_validate_state(data,size,&io))return 0;
 if(!apply)return 1;
 if(!fm_driver_prepare_state(data,size,NULL))return 0;
 io.at=8;io.apply=1;
 return driver_payload(&io) && io.at==size;
}

unsigned fm_driver_capacity_bytes(void){return capacity*sizeof(*events)+drum_capacity*sizeof(*drums);}
