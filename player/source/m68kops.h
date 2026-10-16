/* routine to skip one 68K bus refresh cycle if instruction processing time is longer than refresh period (128 CPU cycles on Mega Drive / Genesis) */
/* this fixes instructions timing test ROM (test_inst_speed.bin) when 68K bus refresh delay is emulated */
#define SKIP_BUS_REFRESH() \
  if (m68k.cycles >= m68k.refresh_cycles) \
    m68ki_cpu.refresh_cycles += (128*MUL);

/* ======================================================================== */
/* ============== CYCLE-ACCURATE DIV/MUL EXECUTION ======================== */
/* ======================================================================== */

INLINE void UseDivuCycles(uint32 dst, uint32 src)
{
  int i;

  /* minimum cycle time */
  uint mcycles = 76 * MUL;

  /* 16-bit divisor */
  src <<= 16;

  /* 16-bit dividend */
  for (i=0; i<15; i++)
  {
    /* check if carry bit set  */
    if ((sint32) dst < 0)
    {
      /* shift dividend and apply divisor */
      dst <<= 1;
      dst -= src;
    }
    else
    {
      /* shift dividend and add two cycles */
      dst <<= 1;
      mcycles += (4 * MUL);

      if (dst >= src)
      {
        /* apply divisor and remove one cycle */
        dst -= src;
        mcycles -= 2 * MUL;
      }
    }
  }

  USE_CYCLES(mcycles);
  SKIP_BUS_REFRESH();
}

INLINE void UseDivsCycles(sint32 dst, sint16 src)
{
  /* minimum cycle time */
  uint mcycles = 12 * MUL;

  /* negative dividend */
  if (dst < 0) mcycles += 2 * MUL;

  if ((abs(dst) >> 16) < abs(src))
  {
    int i;

    /* absolute quotient */
    uint32 quotient = abs(dst) / abs(src);

    /* add default cycle time */
    mcycles += (110 * MUL);

    /* positive divisor */
    if (src >= 0)
    {
      /* check dividend sign */
      if (dst >= 0) mcycles -= 2 * MUL;
      else mcycles += 2 * MUL;
    }

    /* check higher 15-bits of quotient */
    for (i=0; i<15; i++)
    {
      quotient >>= 1;
      if (!(quotient & 1)) mcycles += 2 * MUL;
    }
  }
  else
  {
    /* absolute overflow */
    mcycles += (4 * MUL);
  }

  USE_CYCLES(mcycles);
  SKIP_BUS_REFRESH();
}

INLINE void UseMuluCycles(uint16 src)
{
  /* minimum cycle time */
  uint mcycles = 38 * MUL;

  /* count number of bits set to 1 */
  while (src)
  {
    if (src & 1) mcycles += (2 * MUL);
    src >>= 1;
  }

  /* 38 + 2*N */
  USE_CYCLES(mcycles);
}

INLINE void UseMulsCycles(sint16 src)
{
  /* minimum cycle time */
  uint mcycles = 38 * MUL;

  /* detect 01 or 10 patterns */
  sint32 tmp = src << 1;
  tmp = (tmp ^ src) & 0xFFFF;

  /* count number of bits set to 1 */
  while (tmp)
  {
    if (tmp & 1) mcycles += (2 * MUL);
    tmp >>= 1;
  }

  /* 38 + 2*N */
  USE_CYCLES(mcycles);
}


/* ======================================================================== */
/* ========================= INSTRUCTION HANDLERS ========================= */
/* ======================================================================== */


static void m68k_op_1010(void)
{
  m68ki_exception_1010();
}


static void m68k_op_1111(void)
{
  m68ki_exception_1111();
}


static void m68k_op_abcd_8_rr(void)
{
  uint* r_dst = &DX;
  uint src = DY;
  uint dst = *r_dst;
  uint res = LOW_NIBBLE(src) + LOW_NIBBLE(dst) + XFLAG_AS_1();
  uint corf = 0;

  if(res > 9)
    corf = 6;
  res += HIGH_NIBBLE(src) + HIGH_NIBBLE(dst);
  FLAG_V = ~res; /* Undefined V behavior */

  res += corf;
  FLAG_X = FLAG_C = (res > 0x9f) << 8;
  if(FLAG_C)
    res -= 0xa0;

  FLAG_V &= res; /* Undefined V behavior part II */
  FLAG_N = NFLAG_8(res); /* Undefined N behavior */

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
}


static void m68k_op_abcd_8_mm_ax7(void)
{
  uint src = OPER_AY_PD_8();
  uint ea  = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = LOW_NIBBLE(src) + LOW_NIBBLE(dst) + XFLAG_AS_1();
  uint corf = 0;

  if(res > 9)
    corf = 6;
  res += HIGH_NIBBLE(src) + HIGH_NIBBLE(dst);
  FLAG_V = ~res; /* Undefined V behavior */

  res += corf;
  FLAG_X = FLAG_C = (res > 0x9f) << 8;
  if(FLAG_C)
    res -= 0xa0;

  FLAG_V &= res; /* Undefined V behavior part II */
  FLAG_N = NFLAG_8(res); /* Undefined N behavior */

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_abcd_8_mm_ay7(void)
{
  uint src = OPER_A7_PD_8();
  uint ea  = EA_AX_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = LOW_NIBBLE(src) + LOW_NIBBLE(dst) + XFLAG_AS_1();
  uint corf = 0;

  if(res > 9)
    corf = 6;
  res += HIGH_NIBBLE(src) + HIGH_NIBBLE(dst);
  FLAG_V = ~res; /* Undefined V behavior */

  res += corf;
  FLAG_X = FLAG_C = (res > 0x9f) << 8;
  if(FLAG_C)
    res -= 0xa0;

  FLAG_V &= res; /* Undefined V behavior part II */
  FLAG_N = NFLAG_8(res); /* Undefined N behavior */

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_abcd_8_mm_axy7(void)
{
  uint src = OPER_A7_PD_8();
  uint ea  = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = LOW_NIBBLE(src) + LOW_NIBBLE(dst) + XFLAG_AS_1();
  uint corf = 0;

  if(res > 9)
    corf = 6;
  res += HIGH_NIBBLE(src) + HIGH_NIBBLE(dst);
  FLAG_V = ~res; /* Undefined V behavior */

  res += corf;
  FLAG_X = FLAG_C = (res > 0x9f) << 8;
  if(FLAG_C)
    res -= 0xa0;

  FLAG_V &= res; /* Undefined V behavior part II */
  FLAG_N = NFLAG_8(res); /* Undefined N behavior */

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_abcd_8_mm(void)
{
  uint src = OPER_AY_PD_8();
  uint ea  = EA_AX_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = LOW_NIBBLE(src) + LOW_NIBBLE(dst) + XFLAG_AS_1();
  uint corf = 0;

  if(res > 9)
    corf = 6;
  res += HIGH_NIBBLE(src) + HIGH_NIBBLE(dst);
  FLAG_V = ~res; /* Undefined V behavior */

  res += corf;
  FLAG_X = FLAG_C = (res > 0x9f) << 8;
  if(FLAG_C)
    res -= 0xa0;

  FLAG_V &= res; /* Undefined V behavior part II */
  FLAG_N = NFLAG_8(res); /* Undefined N behavior */

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_add_8_er_d(void)
{
  uint* r_dst = &DX;
  uint src = MASK_OUT_ABOVE_8(DY);
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_ai(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_AI_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_pi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PI_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_pi7(void)
{
  uint* r_dst = &DX;
  uint src = OPER_A7_PI_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_pd(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PD_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_pd7(void)
{
  uint* r_dst = &DX;
  uint src = OPER_A7_PD_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_di(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_DI_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_ix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_IX_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_aw(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AW_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_al(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AL_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_pcdi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCDI_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_pcix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCIX_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_8_er_i(void)
{
  uint* r_dst = &DX;
  uint src = OPER_I_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_d(void)
{
  uint* r_dst = &DX;
  uint src = MASK_OUT_ABOVE_16(DY);
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_a(void)
{
  uint* r_dst = &DX;
  uint src = MASK_OUT_ABOVE_16(AY);
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_ai(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_AI_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_pi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PI_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_pd(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PD_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_di(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_DI_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_ix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_IX_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_aw(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AW_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_al(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AL_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_pcdi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCDI_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_pcix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCIX_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_16_er_i(void)
{
  uint* r_dst = &DX;
  uint src = OPER_I_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_add_32_er_d(void)
{
  uint* r_dst = &DX;
  uint src = DY;
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_a(void)
{
  uint* r_dst = &DX;
  uint src = AY;
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_ai(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_AI_32();
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_pi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PI_32();
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_pd(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PD_32();
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_di(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_DI_32();
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_ix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_IX_32();
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_aw(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AW_32();
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_al(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AL_32();
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_pcdi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCDI_32();
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_pcix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCIX_32();
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_add_32_er_i(void)
{
  uint* r_dst = &DX;
  uint src = OPER_I_32();
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_add_8_re_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_di(void)
{
  uint ea = EA_AY_DI_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_aw(void)
{
  uint ea = EA_AW_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_8_re_al(void)
{
  uint ea = EA_AL_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_add_16_re_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_add_16_re_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_add_16_re_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_add_16_re_di(void)
{
  uint ea = EA_AY_DI_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_add_16_re_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_add_16_re_aw(void)
{
  uint ea = EA_AW_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_add_16_re_al(void)
{
  uint ea = EA_AL_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_add_32_re_ai(void)
{
  uint ea = EA_AY_AI_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_add_32_re_pi(void)
{
  uint ea = EA_AY_PI_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_add_32_re_pd(void)
{
  uint ea = EA_AY_PD_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_add_32_re_di(void)
{
  uint ea = EA_AY_DI_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_add_32_re_ix(void)
{
  uint ea = EA_AY_IX_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_add_32_re_aw(void)
{
  uint ea = EA_AW_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_add_32_re_al(void)
{
  uint ea = EA_AL_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_adda_16_d(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + MAKE_INT_16(DY));
}


static void m68k_op_adda_16_a(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + MAKE_INT_16(AY));
}


static void m68k_op_adda_16_ai(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AY_AI_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_pi(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AY_PI_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_pd(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AY_PD_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_di(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AY_DI_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_ix(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AY_IX_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_aw(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AW_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_al(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AL_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_pcdi(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_PCDI_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_pcix(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_PCIX_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_16_i(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_I_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + src);
}


static void m68k_op_adda_32_d(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + DY);
}


static void m68k_op_adda_32_a(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + AY);
}


static void m68k_op_adda_32_ai(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(OPER_AY_AI_32() + *r_dst);
}


static void m68k_op_adda_32_pi(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(OPER_AY_PI_32() + *r_dst);
}


static void m68k_op_adda_32_pd(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(OPER_AY_PD_32() + *r_dst);
}


static void m68k_op_adda_32_di(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(OPER_AY_DI_32() + *r_dst);
}


static void m68k_op_adda_32_ix(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(OPER_AY_IX_32() + *r_dst);
}


static void m68k_op_adda_32_aw(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(OPER_AW_32() + *r_dst);
}


static void m68k_op_adda_32_al(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(OPER_AL_32() + *r_dst);
}


static void m68k_op_adda_32_pcdi(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(OPER_PCDI_32() + *r_dst);
}


static void m68k_op_adda_32_pcix(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(OPER_PCIX_32() + *r_dst);
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_adda_32_i(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(OPER_I_32() + *r_dst);
}


static void m68k_op_addi_8_d(void)
{
  uint* r_dst = &DY;
  uint src = OPER_I_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_addi_8_ai(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_AI_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_pi(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_PI_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_pi7(void)
{
  uint src = OPER_I_8();
  uint ea = EA_A7_PI_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_pd(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_pd7(void)
{
  uint src = OPER_I_8();
  uint ea = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_di(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_DI_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_ix(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_IX_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_aw(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AW_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_8_al(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AL_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addi_16_d(void)
{
  uint* r_dst = &DY;
  uint src = OPER_I_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_addi_16_ai(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_AI_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_16_pi(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_PI_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_16_pd(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_PD_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_16_di(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_DI_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_16_ix(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_IX_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_16_aw(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AW_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_16_al(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AL_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addi_32_d(void)
{
  uint* r_dst = &DY;
  uint src = OPER_I_32();
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_addi_32_ai(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_AI_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addi_32_pi(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_PI_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addi_32_pd(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_PD_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addi_32_di(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_DI_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addi_32_ix(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_IX_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addi_32_aw(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AW_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addi_32_al(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AL_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_addq_8_d(void)
{
  uint* r_dst = &DY;
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;

  /* reset idle loop detection (fixes cases where instruction is used in tight counter incrementing loop) */
  m68ki_cpu.poll.detected = 0;
}


static void m68k_op_addq_8_ai(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_AI_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addq_8_pi(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_PI_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addq_8_pi7(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_A7_PI_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addq_8_pd(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addq_8_pd7(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addq_8_di(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_DI_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addq_8_ix(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_IX_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addq_8_aw(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AW_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addq_8_al(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AL_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_addq_16_d(void)
{
  uint* r_dst = &DY;
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;

  /* reset idle loop detection (fixes cases where instruction is used in tight counter incrementing loop) */
  m68ki_cpu.poll.detected = 0;
}


static void m68k_op_addq_16_a(void)
{
  uint* r_dst = &AY;

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + (((REG_IR >> 9) - 1) & 7) + 1);
}


static void m68k_op_addq_16_ai(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_AI_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addq_16_pi(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_PI_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addq_16_pd(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_PD_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addq_16_di(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_DI_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addq_16_ix(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_IX_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addq_16_aw(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AW_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addq_16_al(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AL_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst;

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_addq_32_d(void)
{
  uint* r_dst = &DY;
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint dst = *r_dst;
  uint res = src + dst;

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;

  /* reset idle loop detection (fixes cases where instruction is used in tight counter incrementing loop) */
  m68ki_cpu.poll.detected = 0;
}


static void m68k_op_addq_32_a(void)
{
  uint* r_dst = &AY;

  *r_dst = MASK_OUT_ABOVE_32(*r_dst + (((REG_IR >> 9) - 1) & 7) + 1);
}


static void m68k_op_addq_32_ai(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_AI_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;


  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addq_32_pi(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_PI_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;


  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addq_32_pd(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_PD_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;


  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addq_32_di(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_DI_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;


  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addq_32_ix(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_IX_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;


  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addq_32_aw(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AW_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;


  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addq_32_al(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AL_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst;


  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_addx_8_rr(void)
{
  uint* r_dst = &DX;
  uint src = MASK_OUT_ABOVE_8(DY);
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src + dst + XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
}


static void m68k_op_addx_16_rr(void)
{
  uint* r_dst = &DX;
  uint src = MASK_OUT_ABOVE_16(DY);
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src + dst + XFLAG_AS_1();

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);

  res = MASK_OUT_ABOVE_16(res);
  FLAG_Z |= res;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
}


static void m68k_op_addx_32_rr(void)
{
  uint* r_dst = &DX;
  uint src = DY;
  uint dst = *r_dst;
  uint res = src + dst + XFLAG_AS_1();

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);

  res = MASK_OUT_ABOVE_32(res);
  FLAG_Z |= res;

  *r_dst = res;
}


static void m68k_op_addx_8_mm_ax7(void)
{
  uint src = OPER_AY_PD_8();
  uint ea  = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst + XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_addx_8_mm_ay7(void)
{
  uint src = OPER_A7_PD_8();
  uint ea  = EA_AX_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst + XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_addx_8_mm_axy7(void)
{
  uint src = OPER_A7_PD_8();
  uint ea  = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst + XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_addx_8_mm(void)
{
  uint src = OPER_AY_PD_8();
  uint ea  = EA_AX_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = src + dst + XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_V = VFLAG_ADD_8(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_8(res);

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_addx_16_mm(void)
{
  uint src = OPER_AY_PD_16();
  uint ea  = EA_AX_PD_16();
  uint dst = m68ki_read_16(ea);
  uint res = src + dst + XFLAG_AS_1();

  FLAG_N = NFLAG_16(res);
  FLAG_V = VFLAG_ADD_16(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_16(res);

  res = MASK_OUT_ABOVE_16(res);
  FLAG_Z |= res;

  m68ki_write_16(ea, res);
}


static void m68k_op_addx_32_mm(void)
{
  uint src = OPER_AY_PD_32();
  uint ea  = EA_AX_PD_32();
  uint dst = m68ki_read_32(ea);
  uint res = src + dst + XFLAG_AS_1();

  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_ADD_32(src, dst, res);
  FLAG_X = FLAG_C = CFLAG_ADD_32(src, dst, res);

  res = MASK_OUT_ABOVE_32(res);
  FLAG_Z |= res;

  m68ki_write_32(ea, res);
}


static void m68k_op_and_8_er_d(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (DY | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_ai(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AY_AI_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_pi(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AY_PI_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_pi7(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_A7_PI_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_pd(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AY_PD_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_pd7(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_A7_PD_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_di(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AY_DI_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_ix(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AY_IX_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_aw(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AW_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_al(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_AL_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_pcdi(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_PCDI_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_pcix(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_PCIX_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_er_i(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DX &= (OPER_I_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_d(void)
{
  FLAG_Z = MASK_OUT_ABOVE_16(DX &= (DY | 0xffff0000));

  FLAG_N = NFLAG_16(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_ai(void)
{
  FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AY_AI_16() | 0xffff0000));

  FLAG_N = NFLAG_16(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_pi(void)
{
  FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AY_PI_16() | 0xffff0000));

  FLAG_N = NFLAG_16(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_pd(void)
{
  FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AY_PD_16() | 0xffff0000));

  FLAG_N = NFLAG_16(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_di(void)
{
  FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AY_DI_16() | 0xffff0000));

  FLAG_N = NFLAG_16(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_ix(void)
{
  FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AY_IX_16() | 0xffff0000));

  FLAG_N = NFLAG_16(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_aw(void)
{
  FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AW_16() | 0xffff0000));

  FLAG_N = NFLAG_16(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_al(void)
{
  FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_AL_16() | 0xffff0000));

  FLAG_N = NFLAG_16(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_pcdi(void)
{
  FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_PCDI_16() | 0xffff0000));

  FLAG_N = NFLAG_16(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_pcix(void)
{
  FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_PCIX_16() | 0xffff0000));

  FLAG_N = NFLAG_16(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_16_er_i(void)
{
  FLAG_Z = MASK_OUT_ABOVE_16(DX &= (OPER_I_16() | 0xffff0000));

  FLAG_N = NFLAG_16(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_d(void)
{
  FLAG_Z = DX &= DY;

  FLAG_N = NFLAG_32(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_ai(void)
{
  FLAG_Z = DX &= OPER_AY_AI_32();

  FLAG_N = NFLAG_32(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_pi(void)
{
  FLAG_Z = DX &= OPER_AY_PI_32();

  FLAG_N = NFLAG_32(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_pd(void)
{
  FLAG_Z = DX &= OPER_AY_PD_32();

  FLAG_N = NFLAG_32(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_di(void)
{
  FLAG_Z = DX &= OPER_AY_DI_32();

  FLAG_N = NFLAG_32(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_ix(void)
{
  FLAG_Z = DX &= OPER_AY_IX_32();

  FLAG_N = NFLAG_32(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_aw(void)
{
  FLAG_Z = DX &= OPER_AW_32();

  FLAG_N = NFLAG_32(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_al(void)
{
  FLAG_Z = DX &= OPER_AL_32();

  FLAG_N = NFLAG_32(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_pcdi(void)
{
  FLAG_Z = DX &= OPER_PCDI_32();

  FLAG_N = NFLAG_32(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_pcix(void)
{
  FLAG_Z = DX &= OPER_PCIX_32();

  FLAG_N = NFLAG_32(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_32_er_i(void)
{
  FLAG_Z = DX &= OPER_I_32();

  FLAG_N = NFLAG_32(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_and_8_re_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint res = DX & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint res = DX & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint res = DX & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint res = DX & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint res = DX & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_di(void)
{
  uint ea = EA_AY_DI_8();
  uint res = DX & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint res = DX & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_aw(void)
{
  uint ea = EA_AW_8();
  uint res = DX & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_8_re_al(void)
{
  uint ea = EA_AL_8();
  uint res = DX & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_and_16_re_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint res = DX & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_and_16_re_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint res = DX & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_and_16_re_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint res = DX & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_and_16_re_di(void)
{
  uint ea = EA_AY_DI_16();
  uint res = DX & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_and_16_re_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint res = DX & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_and_16_re_aw(void)
{
  uint ea = EA_AW_16();
  uint res = DX & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_and_16_re_al(void)
{
  uint ea = EA_AL_16();
  uint res = DX & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_and_32_re_ai(void)
{
  uint ea = EA_AY_AI_32();
  uint res = DX & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_and_32_re_pi(void)
{
  uint ea = EA_AY_PI_32();
  uint res = DX & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_and_32_re_pd(void)
{
  uint ea = EA_AY_PD_32();
  uint res = DX & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_and_32_re_di(void)
{
  uint ea = EA_AY_DI_32();
  uint res = DX & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_and_32_re_ix(void)
{
  uint ea = EA_AY_IX_32();
  uint res = DX & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_and_32_re_aw(void)
{
  uint ea = EA_AW_32();
  uint res = DX & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_and_32_re_al(void)
{
  uint ea = EA_AL_32();
  uint res = DX & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_andi_8_d(void)
{
  FLAG_Z = MASK_OUT_ABOVE_8(DY &= (OPER_I_8() | 0xffffff00));

  FLAG_N = NFLAG_8(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_andi_8_ai(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_AI_8();
  uint res = src & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_pi(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_PI_8();
  uint res = src & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_pi7(void)
{
  uint src = OPER_I_8();
  uint ea = EA_A7_PI_8();
  uint res = src & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_pd(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_PD_8();
  uint res = src & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_pd7(void)
{
  uint src = OPER_I_8();
  uint ea = EA_A7_PD_8();
  uint res = src & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_di(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_DI_8();
  uint res = src & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_ix(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_IX_8();
  uint res = src & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_aw(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AW_8();
  uint res = src & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_andi_8_al(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AL_8();
  uint res = src & m68ki_read_8(ea);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_andi_16_d(void)
{
  FLAG_Z = MASK_OUT_ABOVE_16(DY &= (OPER_I_16() | 0xffff0000));

  FLAG_N = NFLAG_16(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_andi_16_ai(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_AI_16();
  uint res = src & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_andi_16_pi(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_PI_16();
  uint res = src & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_andi_16_pd(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_PD_16();
  uint res = src & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_andi_16_di(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_DI_16();
  uint res = src & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_andi_16_ix(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_IX_16();
  uint res = src & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_andi_16_aw(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AW_16();
  uint res = src & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_andi_16_al(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AL_16();
  uint res = src & m68ki_read_16(ea);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_andi_32_d(void)
{
  FLAG_Z = DY &= (OPER_I_32());

  FLAG_N = NFLAG_32(FLAG_Z);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_andi_32_ai(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_AI_32();
  uint res = src & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_andi_32_pi(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_PI_32();
  uint res = src & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_andi_32_pd(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_PD_32();
  uint res = src & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_andi_32_di(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_DI_32();
  uint res = src & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_andi_32_ix(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_IX_32();
  uint res = src & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_andi_32_aw(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AW_32();
  uint res = src & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_andi_32_al(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AL_32();
  uint res = src & m68ki_read_32(ea);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_andi_16_toc(void)
{
  m68ki_set_ccr(m68ki_get_ccr() & OPER_I_16());
}


static void m68k_op_andi_16_tos(void)
{
  if(FLAG_S)
  {
    uint src = OPER_I_16();
    m68ki_set_sr(m68ki_get_sr() & src);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_asr_8_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src >> shift;

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  if(GET_MSB_8(src))
    res |= m68ki_shift_8_table[shift];

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_X = FLAG_C = src << (9-shift);
}


static void m68k_op_asr_16_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src >> shift;

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  if(GET_MSB_16(src))
    res |= m68ki_shift_16_table[shift];

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_X = FLAG_C = src << (9-shift);
}


static void m68k_op_asr_32_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = *r_dst;
  uint res = src >> shift;

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  if(GET_MSB_32(src))
    res |= m68ki_shift_32_table[shift];

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_X = FLAG_C = src << (9-shift);
}


static void m68k_op_asr_8_r(void)
{
  uint* r_dst = &DY;
  uint shift = DX & 0x3f;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src >> shift;

  if(shift != 0)
  {
    USE_CYCLES(shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift < 8)
    {
      if(GET_MSB_8(src))
        res |= m68ki_shift_8_table[shift];

      *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

      FLAG_X = FLAG_C = src << (9-shift);
      FLAG_N = NFLAG_8(res);
      FLAG_Z = res;
      FLAG_V = VFLAG_CLEAR;
      return;
    }

    if(GET_MSB_8(src))
    {
      *r_dst |= 0xff;
      FLAG_C = CFLAG_SET;
      FLAG_X = XFLAG_SET;
      FLAG_N = NFLAG_SET;
      FLAG_Z = ZFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      return;
    }

    *r_dst &= 0xffffff00;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
    FLAG_Z = ZFLAG_SET;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_8(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_asr_16_r(void)
{
  uint* r_dst = &DY;
  uint shift = DX & 0x3f;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src >> shift;

  if(shift != 0)
  {
    USE_CYCLES(shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift < 16)
    {
      if(GET_MSB_16(src))
        res |= m68ki_shift_16_table[shift];

      *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

      FLAG_C = FLAG_X = (src >> (shift - 1))<<8;
      FLAG_N = NFLAG_16(res);
      FLAG_Z = res;
      FLAG_V = VFLAG_CLEAR;
      return;
    }

    if(GET_MSB_16(src))
    {
      *r_dst |= 0xffff;
      FLAG_C = CFLAG_SET;
      FLAG_X = XFLAG_SET;
      FLAG_N = NFLAG_SET;
      FLAG_Z = ZFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      return;
    }

    *r_dst &= 0xffff0000;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
    FLAG_Z = ZFLAG_SET;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_16(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_asr_32_r(void)
{
  uint* r_dst = &DY;
  uint shift = DX & 0x3f;
  uint src = *r_dst;
  uint res = src >> shift;

  if(shift != 0)
  {
    USE_CYCLES(shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift < 32)
    {
      if(GET_MSB_32(src))
        res |= m68ki_shift_32_table[shift];

      *r_dst = res;

      FLAG_C = FLAG_X = (src >> (shift - 1))<<8;
      FLAG_N = NFLAG_32(res);
      FLAG_Z = res;
      FLAG_V = VFLAG_CLEAR;
      return;
    }

    if(GET_MSB_32(src))
    {
      *r_dst = 0xffffffff;
      FLAG_C = CFLAG_SET;
      FLAG_X = XFLAG_SET;
      FLAG_N = NFLAG_SET;
      FLAG_Z = ZFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      return;
    }

    *r_dst = 0;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
    FLAG_Z = ZFLAG_SET;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_32(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_asr_16_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  if(GET_MSB_16(src))
    res |= 0x8000;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = FLAG_X = src << 8;
}


static void m68k_op_asr_16_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  if(GET_MSB_16(src))
    res |= 0x8000;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = FLAG_X = src << 8;
}


static void m68k_op_asr_16_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  if(GET_MSB_16(src))
    res |= 0x8000;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = FLAG_X = src << 8;
}


static void m68k_op_asr_16_di(void)
{
  uint ea = EA_AY_DI_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  if(GET_MSB_16(src))
    res |= 0x8000;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = FLAG_X = src << 8;
}


static void m68k_op_asr_16_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  if(GET_MSB_16(src))
    res |= 0x8000;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = FLAG_X = src << 8;
}


static void m68k_op_asr_16_aw(void)
{
  uint ea = EA_AW_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  if(GET_MSB_16(src))
    res |= 0x8000;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = FLAG_X = src << 8;
}


static void m68k_op_asr_16_al(void)
{
  uint ea = EA_AL_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  if(GET_MSB_16(src))
    res |= 0x8000;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = FLAG_X = src << 8;
}


static void m68k_op_asl_8_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = MASK_OUT_ABOVE_8(src << shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_X = FLAG_C = src << shift;
  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  src &= m68ki_shift_8_table[shift + 1];
  FLAG_V = (!(src == 0 || (src == m68ki_shift_8_table[shift + 1] && shift < 8)))<<7;
}


static void m68k_op_asl_16_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = MASK_OUT_ABOVE_16(src << shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> (8-shift);
  src &= m68ki_shift_16_table[shift + 1];
  FLAG_V = (!(src == 0 || src == m68ki_shift_16_table[shift + 1]))<<7;
}


static void m68k_op_asl_32_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = *r_dst;
  uint res = MASK_OUT_ABOVE_32(src << shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> (24-shift);
  src &= m68ki_shift_32_table[shift + 1];
  FLAG_V = (!(src == 0 || src == m68ki_shift_32_table[shift + 1]))<<7;
}


static void m68k_op_asl_8_r(void)
{
  uint* r_dst = &DY;
  uint shift = DX & 0x3f;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = MASK_OUT_ABOVE_8(src << shift);

  if(shift != 0)
  {
    USE_CYCLES(shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift < 8)
    {
      *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
      FLAG_X = FLAG_C = src << shift;
      FLAG_N = NFLAG_8(res);
      FLAG_Z = res;
      src &= m68ki_shift_8_table[shift + 1];
      FLAG_V = (!(src == 0 || src == m68ki_shift_8_table[shift + 1]))<<7;
      return;
    }

    *r_dst &= 0xffffff00;
    FLAG_X = FLAG_C = ((shift == 8 ? src & 1 : 0))<<8;
    FLAG_N = NFLAG_CLEAR;
    FLAG_Z = ZFLAG_SET;
    FLAG_V = (!(src == 0))<<7;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_8(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_asl_16_r(void)
{
  uint* r_dst = &DY;
  uint shift = DX & 0x3f;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = MASK_OUT_ABOVE_16(src << shift);

  if(shift != 0)
  {
    USE_CYCLES(shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift < 16)
    {
      *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
      FLAG_X = FLAG_C = (src << shift) >> 8;
      FLAG_N = NFLAG_16(res);
      FLAG_Z = res;
      src &= m68ki_shift_16_table[shift + 1];
      FLAG_V = (!(src == 0 || src == m68ki_shift_16_table[shift + 1]))<<7;
      return;
    }

    *r_dst &= 0xffff0000;
    FLAG_X = FLAG_C = ((shift == 16 ? src & 1 : 0))<<8;
    FLAG_N = NFLAG_CLEAR;
    FLAG_Z = ZFLAG_SET;
    FLAG_V = (!(src == 0))<<7;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_16(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_asl_32_r(void)
{
  uint* r_dst = &DY;
  uint shift = DX & 0x3f;
  uint src = *r_dst;
  uint res = MASK_OUT_ABOVE_32(src << shift);

  if(shift != 0)
  {
    USE_CYCLES(shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift < 32)
    {
      *r_dst = res;
      FLAG_X = FLAG_C = (src >> (32 - shift)) << 8;
      FLAG_N = NFLAG_32(res);
      FLAG_Z = res;
      src &= m68ki_shift_32_table[shift + 1];
      FLAG_V = (!(src == 0 || src == m68ki_shift_32_table[shift + 1]))<<7;
      return;
    }

    *r_dst = 0;
    FLAG_X = FLAG_C = ((shift == 32 ? src & 1 : 0))<<8;
    FLAG_N = NFLAG_CLEAR;
    FLAG_Z = ZFLAG_SET;
    FLAG_V = (!(src == 0))<<7;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_32(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_asl_16_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  src &= 0xc000;
  FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static void m68k_op_asl_16_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  src &= 0xc000;
  FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static void m68k_op_asl_16_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  src &= 0xc000;
  FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static void m68k_op_asl_16_di(void)
{
  uint ea = EA_AY_DI_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  src &= 0xc000;
  FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static void m68k_op_asl_16_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  src &= 0xc000;
  FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static void m68k_op_asl_16_aw(void)
{
  uint ea = EA_AW_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  src &= 0xc000;
  FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static void m68k_op_asl_16_al(void)
{
  uint ea = EA_AL_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  src &= 0xc000;
  FLAG_V = (!(src == 0 || src == 0xc000))<<7;
}


static void m68k_op_bhi_8(void)
{
  if(COND_HI())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bls_8(void)
{
  if(COND_LS())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bcc_8(void)
{
  if(COND_CC())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bcs_8(void)
{
  if(COND_CS())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bne_8(void)
{
  if(COND_NE())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_beq_8(void)
{
  if(COND_EQ())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bvc_8(void)
{
  if(COND_VC())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bvs_8(void)
{
  if(COND_VS())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bpl_8(void)
{
  if(COND_PL())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bmi_8(void)
{
  if(COND_MI())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bge_8(void)
{
  if(COND_GE())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_blt_8(void)
{
  if(COND_LT())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bgt_8(void)
{
  if(COND_GT())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_ble_8(void)
{
  if(COND_LE())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bhi_16(void)
{
  if(COND_HI())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static void m68k_op_bls_16(void)
{
  if(COND_LS())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static void m68k_op_bcc_16(void)
{
  if(COND_CC())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_bcs_16(void)
{
  if(COND_CS())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_bne_16(void)
{
  if(COND_NE())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_beq_16(void)
{
  if(COND_EQ())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static void m68k_op_bvc_16(void)
{
  if(COND_VC())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static void m68k_op_bvs_16(void)
{
  if(COND_VS())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static void m68k_op_bpl_16(void)
{
  if(COND_PL())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static void m68k_op_bmi_16(void)
{
  if(COND_MI())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static void m68k_op_bge_16(void)
{
  if(COND_GE())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static void m68k_op_blt_16(void)
{
  if(COND_LT())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static void m68k_op_bgt_16(void)
{
  if(COND_GT())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static void m68k_op_ble_16(void)
{
  if(COND_LE())
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_BCC_NOTAKE_W);
}


static void m68k_op_bhi_32(void)
{
  if(COND_HI())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bls_32(void)
{
  if(COND_LS())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bcc_32(void)
{
  if(COND_CC())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bcs_32(void)
{
  if(COND_CS())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bne_32(void)
{
  if(COND_NE())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_beq_32(void)
{
  if(COND_EQ())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bvc_32(void)
{
  if(COND_VC())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bvs_32(void)
{
  if(COND_VS())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bpl_32(void)
{
  if(COND_PL())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bmi_32(void)
{
  if(COND_MI())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bge_32(void)
{
  if(COND_GE())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_blt_32(void)
{
  if(COND_LT())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bgt_32(void)
{
  if(COND_GT())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_ble_32(void)
{
  if(COND_LE())
  {
    m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
    return;
  }
  USE_CYCLES(CYC_BCC_NOTAKE_B);
}


static void m68k_op_bchg_32_r_d(void)
{
  uint* r_dst = &DY;
  uint mask = 1 << (DX & 0x1f);

  if (mask & 0xffff0000)
  {
    USE_CYCLES(2 * MUL);
  }

  FLAG_Z = *r_dst & mask;
  *r_dst ^= mask;
}


static void m68k_op_bchg_8_r_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_di(void)
{
  uint ea = EA_AY_DI_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_aw(void)
{
  uint ea = EA_AW_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_r_al(void)
{
  uint ea = EA_AL_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_32_s_d(void)
{
  uint* r_dst = &DY;
  uint mask = 1 << (OPER_I_8() & 0x1f);

  if (mask & 0xffff0000)
  {
    USE_CYCLES(2 * MUL);
  }

  FLAG_Z = *r_dst & mask;
  *r_dst ^= mask;
}


static void m68k_op_bchg_8_s_ai(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_AI_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_pi(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_PI_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_pi7(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_A7_PI_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_pd(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_PD_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_pd7(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_A7_PD_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_di(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_DI_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_ix(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_IX_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_aw(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AW_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bchg_8_s_al(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AL_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src ^ mask);
}


static void m68k_op_bclr_32_r_d(void)
{
  uint* r_dst = &DY;
  uint mask = 1 << (DX & 0x1f);

  if (mask & 0xffff0000)
  {
    USE_CYCLES(2 * MUL);
  }

  FLAG_Z = *r_dst & mask;
  *r_dst &= ~mask;
}


static void m68k_op_bclr_8_r_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_di(void)
{
  uint ea = EA_AY_DI_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_aw(void)
{
  uint ea = EA_AW_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_r_al(void)
{
  uint ea = EA_AL_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_32_s_d(void)
{
  uint* r_dst = &DY;
  uint mask = 1 << (OPER_I_8() & 0x1f);

  if (mask & 0xffff0000)
  {
    USE_CYCLES(2 * MUL);
  }

  FLAG_Z = *r_dst & mask;
  *r_dst &= ~mask;
}


static void m68k_op_bclr_8_s_ai(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_AI_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_pi(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_PI_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_pi7(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_A7_PI_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_pd(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_PD_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_pd7(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_A7_PD_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_di(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_DI_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_ix(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_IX_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bclr_8_s_aw(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AW_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_bclr_8_s_al(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AL_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src & ~mask);
}


static void m68k_op_bra_8(void)
{
  m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_bra_16(void)
{
  uint offset = OPER_I_16();
  REG_PC -= 2;
  m68ki_branch_16(offset);
}


static void m68k_op_bra_32(void)
{
  m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
}


static void m68k_op_bset_32_r_d(void)
{
  uint* r_dst = &DY;
  uint mask = 1 << (DX & 0x1f);

  if (mask & 0xffff0000)
  {
    USE_CYCLES(2 * MUL);
  }

  FLAG_Z = *r_dst & mask;
  *r_dst |= mask;
}


static void m68k_op_bset_8_r_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_di(void)
{
  uint ea = EA_AY_DI_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_aw(void)
{
  uint ea = EA_AW_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_r_al(void)
{
  uint ea = EA_AL_8();
  uint src = m68ki_read_8(ea);
  uint mask = 1 << (DX & 7);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_32_s_d(void)
{
  uint* r_dst = &DY;
  uint mask = 1 << (OPER_I_8() & 0x1f);

  if (mask & 0xffff0000)
  {
    USE_CYCLES(2 * MUL);
  }

  FLAG_Z = *r_dst & mask;
  *r_dst |= mask;
}


static void m68k_op_bset_8_s_ai(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_AI_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_pi(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_PI_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_pi7(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_A7_PI_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_pd(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_PD_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_pd7(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_A7_PD_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_di(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_DI_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_ix(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AY_IX_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_aw(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AW_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bset_8_s_al(void)
{
  uint mask = 1 << (OPER_I_8() & 7);
  uint ea = EA_AL_8();
  uint src = m68ki_read_8(ea);

  FLAG_Z = src & mask;
  m68ki_write_8(ea, src | mask);
}


static void m68k_op_bsr_8(void)
{
  m68ki_push_32(REG_PC);
  m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
}


static void m68k_op_bsr_16(void)
{
  uint offset = OPER_I_16();
  m68ki_push_32(REG_PC);
  REG_PC -= 2;
  m68ki_branch_16(offset);
}


static void m68k_op_bsr_32(void)
{
  m68ki_push_32(REG_PC);
  m68ki_branch_8(MASK_OUT_ABOVE_8(REG_IR));
}


static void m68k_op_btst_32_r_d(void)
{
  FLAG_Z = DY & (1 << (DX & 0x1f));
}


static void m68k_op_btst_8_r_ai(void)
{
  FLAG_Z = OPER_AY_AI_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_pi(void)
{
  FLAG_Z = OPER_AY_PI_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_pi7(void)
{
  FLAG_Z = OPER_A7_PI_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_pd(void)
{
  FLAG_Z = OPER_AY_PD_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_pd7(void)
{
  FLAG_Z = OPER_A7_PD_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_di(void)
{
  FLAG_Z = OPER_AY_DI_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_ix(void)
{
  FLAG_Z = OPER_AY_IX_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_aw(void)
{
  FLAG_Z = OPER_AW_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_al(void)
{
  FLAG_Z = OPER_AL_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_pcdi(void)
{
  FLAG_Z = OPER_PCDI_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_pcix(void)
{
  FLAG_Z = OPER_PCIX_8() & (1 << (DX & 7));
}


static void m68k_op_btst_8_r_i(void)
{
  FLAG_Z = OPER_I_8() & (1 << (DX & 7));
}


static void m68k_op_btst_32_s_d(void)
{
  FLAG_Z = DY & (1 << (OPER_I_8() & 0x1f));
}


static void m68k_op_btst_8_s_ai(void)
{
  uint bit = OPER_I_8() & 7;

  FLAG_Z = OPER_AY_AI_8() & (1 << bit);
}


static void m68k_op_btst_8_s_pi(void)
{
  uint bit = OPER_I_8() & 7;

  FLAG_Z = OPER_AY_PI_8() & (1 << bit);
}


static void m68k_op_btst_8_s_pi7(void)
{
  uint bit = OPER_I_8() & 7;

  FLAG_Z = OPER_A7_PI_8() & (1 << bit);
}


static void m68k_op_btst_8_s_pd(void)
{
  uint bit = OPER_I_8() & 7;

  FLAG_Z = OPER_AY_PD_8() & (1 << bit);
}


static void m68k_op_btst_8_s_pd7(void)
{
  uint bit = OPER_I_8() & 7;

  FLAG_Z = OPER_A7_PD_8() & (1 << bit);
}


static void m68k_op_btst_8_s_di(void)
{
  uint bit = OPER_I_8() & 7;

  FLAG_Z = OPER_AY_DI_8() & (1 << bit);
}


static void m68k_op_btst_8_s_ix(void)
{
  uint bit = OPER_I_8() & 7;

  FLAG_Z = OPER_AY_IX_8() & (1 << bit);
}


static void m68k_op_btst_8_s_aw(void)
{
  uint bit = OPER_I_8() & 7;

  FLAG_Z = OPER_AW_8() & (1 << bit);
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_btst_8_s_al(void)
{
  uint bit = OPER_I_8() & 7;

  FLAG_Z = OPER_AL_8() & (1 << bit);
}


static void m68k_op_btst_8_s_pcdi(void)
{
  uint bit = OPER_I_8() & 7;

  FLAG_Z = OPER_PCDI_8() & (1 << bit);
}


static void m68k_op_btst_8_s_pcix(void)
{
  uint bit = OPER_I_8() & 7;

  FLAG_Z = OPER_PCIX_8() & (1 << bit);
}


static void m68k_op_chk_16_d(void)
{
  sint src = MAKE_INT_16(DX);
  sint bound = MAKE_INT_16(DY);

  FLAG_Z = ZFLAG_16(src); /* Undocumented */
  FLAG_V = VFLAG_CLEAR;   /* Undocumented */
  FLAG_C = CFLAG_CLEAR;   /* Undocumented */

  if(src >= 0 && src <= bound)
  {
    USE_CYCLES(10 * MUL);
    return;
  }
  if(src < 0)
  {
    FLAG_N = 1<<7;
    USE_CYCLES(2 * MUL);
  }
  m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_ai(void)
{
  sint src = MAKE_INT_16(DX);
  sint bound = MAKE_INT_16(OPER_AY_AI_16());

  FLAG_Z = ZFLAG_16(src); /* Undocumented */
  FLAG_V = VFLAG_CLEAR;   /* Undocumented */
  FLAG_C = CFLAG_CLEAR;   /* Undocumented */

  if(src >= 0 && src <= bound)
  {
    USE_CYCLES(10 * MUL);
    return;
  }
  if(src < 0)
  {
    FLAG_N = 1<<7;
    USE_CYCLES(2 * MUL);
  }
  m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_pi(void)
{
  sint src = MAKE_INT_16(DX);
  sint bound = MAKE_INT_16(OPER_AY_PI_16());

  FLAG_Z = ZFLAG_16(src); /* Undocumented */
  FLAG_V = VFLAG_CLEAR;   /* Undocumented */
  FLAG_C = CFLAG_CLEAR;   /* Undocumented */

  if(src >= 0 && src <= bound)
  {
    USE_CYCLES(10 * MUL);
    return;
  }
  if(src < 0)
  {
    FLAG_N = 1<<7;
    USE_CYCLES(2 * MUL);
  }
  m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_pd(void)
{
  sint src = MAKE_INT_16(DX);
  sint bound = MAKE_INT_16(OPER_AY_PD_16());

  FLAG_Z = ZFLAG_16(src); /* Undocumented */
  FLAG_V = VFLAG_CLEAR;   /* Undocumented */
  FLAG_C = CFLAG_CLEAR;   /* Undocumented */

  if(src >= 0 && src <= bound)
  {
    USE_CYCLES(10 * MUL);
    return;
  }
  if(src < 0)
  {
    FLAG_N = 1<<7;
    USE_CYCLES(2 * MUL);
  }
  m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_di(void)
{
  sint src = MAKE_INT_16(DX);
  sint bound = MAKE_INT_16(OPER_AY_DI_16());

  FLAG_Z = ZFLAG_16(src); /* Undocumented */
  FLAG_V = VFLAG_CLEAR;   /* Undocumented */
  FLAG_C = CFLAG_CLEAR;   /* Undocumented */

  if(src >= 0 && src <= bound)
  {
    USE_CYCLES(10 * MUL);
    return;
  }
  if(src < 0)
  {
    FLAG_N = 1<<7;
    USE_CYCLES(2 * MUL);
  }
  m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_ix(void)
{
  sint src = MAKE_INT_16(DX);
  sint bound = MAKE_INT_16(OPER_AY_IX_16());

  FLAG_Z = ZFLAG_16(src); /* Undocumented */
  FLAG_V = VFLAG_CLEAR;   /* Undocumented */
  FLAG_C = CFLAG_CLEAR;   /* Undocumented */

  if(src >= 0 && src <= bound)
  {
    USE_CYCLES(10 * MUL);
    return;
  }
  if(src < 0)
  {
    FLAG_N = 1<<7;
    USE_CYCLES(2 * MUL);
  }
  m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_aw(void)
{
  sint src = MAKE_INT_16(DX);
  sint bound = MAKE_INT_16(OPER_AW_16());

  FLAG_Z = ZFLAG_16(src); /* Undocumented */
  FLAG_V = VFLAG_CLEAR;   /* Undocumented */
  FLAG_C = CFLAG_CLEAR;   /* Undocumented */

  if(src >= 0 && src <= bound)
  {
    USE_CYCLES(10 * MUL);
    return;
  }
  if(src < 0)
  {
    FLAG_N = 1<<7;
    USE_CYCLES(2 * MUL);
  }
  m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_al(void)
{
  sint src = MAKE_INT_16(DX);
  sint bound = MAKE_INT_16(OPER_AL_16());

  FLAG_Z = ZFLAG_16(src); /* Undocumented */
  FLAG_V = VFLAG_CLEAR;   /* Undocumented */
  FLAG_C = CFLAG_CLEAR;   /* Undocumented */

  if(src >= 0 && src <= bound)
  {
    USE_CYCLES(10 * MUL);
    return;
  }
  if(src < 0)
  {
    FLAG_N = 1<<7;
    USE_CYCLES(2 * MUL);
  }
  m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_pcdi(void)
{
  sint src = MAKE_INT_16(DX);
  sint bound = MAKE_INT_16(OPER_PCDI_16());

  FLAG_Z = ZFLAG_16(src); /* Undocumented */
  FLAG_V = VFLAG_CLEAR;   /* Undocumented */
  FLAG_C = CFLAG_CLEAR;   /* Undocumented */

  if(src >= 0 && src <= bound)
  {
    USE_CYCLES(10 * MUL);
    return;
  }
  if(src < 0)
  {
    FLAG_N = 1<<7;
    USE_CYCLES(2 * MUL);
  }
  m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_pcix(void)
{
  sint src = MAKE_INT_16(DX);
  sint bound = MAKE_INT_16(OPER_PCIX_16());

  FLAG_Z = ZFLAG_16(src); /* Undocumented */
  FLAG_V = VFLAG_CLEAR;   /* Undocumented */
  FLAG_C = CFLAG_CLEAR;   /* Undocumented */

  if(src >= 0 && src <= bound)
  {
    USE_CYCLES(10 * MUL);
    return;
  }
  if(src < 0)
  {
    FLAG_N = 1<<7;
    USE_CYCLES(2 * MUL);
  }
  m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_chk_16_i(void)
{
  sint src = MAKE_INT_16(DX);
  sint bound = MAKE_INT_16(OPER_I_16());

  FLAG_Z = ZFLAG_16(src); /* Undocumented */
  FLAG_V = VFLAG_CLEAR;   /* Undocumented */
  FLAG_C = CFLAG_CLEAR;   /* Undocumented */

  if(src >= 0 && src <= bound)
  {
    USE_CYCLES(10 * MUL);
    return;
  }
  if(src < 0)
  {
    FLAG_N = 1<<7;
    USE_CYCLES(2 * MUL);
  }
  m68ki_exception_trap(EXCEPTION_CHK);
}


static void m68k_op_clr_8_d(void)
{
  DY &= 0xffffff00;

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_8_al(void)
{
  m68ki_write_8(EA_AL_8(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_16_d(void)
{
  DY &= 0xffff0000;

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_16_ai(void)
{
  m68ki_write_16(EA_AY_AI_16(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_16_pi(void)
{
  m68ki_write_16(EA_AY_PI_16(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_16_pd(void)
{
  m68ki_write_16(EA_AY_PD_16(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_16_di(void)
{
  m68ki_write_16(EA_AY_DI_16(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_16_ix(void)
{
  m68ki_write_16(EA_AY_IX_16(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_16_aw(void)
{
  m68ki_write_16(EA_AW_16(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_16_al(void)
{
  m68ki_write_16(EA_AL_16(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_32_d(void)
{
  DY = 0;

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_32_ai(void)
{
  m68ki_write_32(EA_AY_AI_32(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_32_pi(void)
{
  m68ki_write_32(EA_AY_PI_32(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_32_pd(void)
{
  m68ki_write_32(EA_AY_PD_32(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_32_di(void)
{
  m68ki_write_32(EA_AY_DI_32(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_32_ix(void)
{
  m68ki_write_32(EA_AY_IX_32(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_32_aw(void)
{
  m68ki_write_32(EA_AW_32(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_clr_32_al(void)
{
  m68ki_write_32(EA_AL_32(), 0);

  FLAG_N = NFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  FLAG_Z = ZFLAG_SET;
}


static void m68k_op_cmp_8_d(void)
{
  uint src = MASK_OUT_ABOVE_8(DY);
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_ai(void)
{
  uint src = OPER_AY_AI_8();
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_pi(void)
{
  uint src = OPER_AY_PI_8();
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_pi7(void)
{
  uint src = OPER_A7_PI_8();
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_pd(void)
{
  uint src = OPER_AY_PD_8();
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_pd7(void)
{
  uint src = OPER_A7_PD_8();
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_di(void)
{
  uint src = OPER_AY_DI_8();
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_ix(void)
{
  uint src = OPER_AY_IX_8();
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_aw(void)
{
  uint src = OPER_AW_8();
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_al(void)
{
  uint src = OPER_AL_8();
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_pcdi(void)
{
  uint src = OPER_PCDI_8();
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_pcix(void)
{
  uint src = OPER_PCIX_8();
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_8_i(void)
{
  uint src = OPER_I_8();
  uint dst = MASK_OUT_ABOVE_8(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmp_16_d(void)
{
  uint src = MASK_OUT_ABOVE_16(DY);
  uint dst = MASK_OUT_ABOVE_16(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_a(void)
{
  uint src = MASK_OUT_ABOVE_16(AY);
  uint dst = MASK_OUT_ABOVE_16(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_ai(void)
{
  uint src = OPER_AY_AI_16();
  uint dst = MASK_OUT_ABOVE_16(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_pi(void)
{
  uint src = OPER_AY_PI_16();
  uint dst = MASK_OUT_ABOVE_16(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_pd(void)
{
  uint src = OPER_AY_PD_16();
  uint dst = MASK_OUT_ABOVE_16(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_di(void)
{
  uint src = OPER_AY_DI_16();
  uint dst = MASK_OUT_ABOVE_16(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_ix(void)
{
  uint src = OPER_AY_IX_16();
  uint dst = MASK_OUT_ABOVE_16(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_aw(void)
{
  uint src = OPER_AW_16();
  uint dst = MASK_OUT_ABOVE_16(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_al(void)
{
  uint src = OPER_AL_16();
  uint dst = MASK_OUT_ABOVE_16(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_pcdi(void)
{
  uint src = OPER_PCDI_16();
  uint dst = MASK_OUT_ABOVE_16(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_pcix(void)
{
  uint src = OPER_PCIX_16();
  uint dst = MASK_OUT_ABOVE_16(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_16_i(void)
{
  uint src = OPER_I_16();
  uint dst = MASK_OUT_ABOVE_16(DX);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmp_32_d(void)
{
  uint src = DY;
  uint dst = DX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_a(void)
{
  uint src = AY;
  uint dst = DX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_ai(void)
{
  uint src = OPER_AY_AI_32();
  uint dst = DX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_pi(void)
{
  uint src = OPER_AY_PI_32();
  uint dst = DX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_pd(void)
{
  uint src = OPER_AY_PD_32();
  uint dst = DX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_di(void)
{
  uint src = OPER_AY_DI_32();
  uint dst = DX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_ix(void)
{
  uint src = OPER_AY_IX_32();
  uint dst = DX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_aw(void)
{
  uint src = OPER_AW_32();
  uint dst = DX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_al(void)
{
  uint src = OPER_AL_32();
  uint dst = DX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_pcdi(void)
{
  uint src = OPER_PCDI_32();
  uint dst = DX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_pcix(void)
{
  uint src = OPER_PCIX_32();
  uint dst = DX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmp_32_i(void)
{
  uint src = OPER_I_32();
  uint dst = DX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_d(void)
{
  uint src = MAKE_INT_16(DY);
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_a(void)
{
  uint src = MAKE_INT_16(AY);
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_ai(void)
{
  uint src = MAKE_INT_16(OPER_AY_AI_16());
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_pi(void)
{
  uint src = MAKE_INT_16(OPER_AY_PI_16());
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_pd(void)
{
  uint src = MAKE_INT_16(OPER_AY_PD_16());
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_di(void)
{
  uint src = MAKE_INT_16(OPER_AY_DI_16());
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_ix(void)
{
  uint src = MAKE_INT_16(OPER_AY_IX_16());
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_aw(void)
{
  uint src = MAKE_INT_16(OPER_AW_16());
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_al(void)
{
  uint src = MAKE_INT_16(OPER_AL_16());
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_pcdi(void)
{
  uint src = MAKE_INT_16(OPER_PCDI_16());
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_pcix(void)
{
  uint src = MAKE_INT_16(OPER_PCIX_16());
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_16_i(void)
{
  uint src = MAKE_INT_16(OPER_I_16());
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_d(void)
{
  uint src = DY;
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_a(void)
{
  uint src = AY;
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_ai(void)
{
  uint src = OPER_AY_AI_32();
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_pi(void)
{
  uint src = OPER_AY_PI_32();
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_pd(void)
{
  uint src = OPER_AY_PD_32();
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_di(void)
{
  uint src = OPER_AY_DI_32();
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_ix(void)
{
  uint src = OPER_AY_IX_32();
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_aw(void)
{
  uint src = OPER_AW_32();
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_al(void)
{
  uint src = OPER_AL_32();
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_pcdi(void)
{
  uint src = OPER_PCDI_32();
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_pcix(void)
{
  uint src = OPER_PCIX_32();
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpa_32_i(void)
{
  uint src = OPER_I_32();
  uint dst = AX;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_cmpi_8_d(void)
{
  uint src = OPER_I_8();
  uint dst = MASK_OUT_ABOVE_8(DY);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_8_ai(void)
{
  uint src = OPER_I_8();
  uint dst = OPER_AY_AI_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_8_pi(void)
{
  uint src = OPER_I_8();
  uint dst = OPER_AY_PI_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_8_pi7(void)
{
  uint src = OPER_I_8();
  uint dst = OPER_A7_PI_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_8_pd(void)
{
  uint src = OPER_I_8();
  uint dst = OPER_AY_PD_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_8_pd7(void)
{
  uint src = OPER_I_8();
  uint dst = OPER_A7_PD_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_8_di(void)
{
  uint src = OPER_I_8();
  uint dst = OPER_AY_DI_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_8_ix(void)
{
  uint src = OPER_I_8();
  uint dst = OPER_AY_IX_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_8_aw(void)
{
  uint src = OPER_I_8();
  uint dst = OPER_AW_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_8_al(void)
{
  uint src = OPER_I_8();
  uint dst = OPER_AL_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpi_16_d(void)
{
  uint src = OPER_I_16();
  uint dst = MASK_OUT_ABOVE_16(DY);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmpi_16_ai(void)
{
  uint src = OPER_I_16();
  uint dst = OPER_AY_AI_16();
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmpi_16_pi(void)
{
  uint src = OPER_I_16();
  uint dst = OPER_AY_PI_16();
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmpi_16_pd(void)
{
  uint src = OPER_I_16();
  uint dst = OPER_AY_PD_16();
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmpi_16_di(void)
{
  uint src = OPER_I_16();
  uint dst = OPER_AY_DI_16();
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmpi_16_ix(void)
{
  uint src = OPER_I_16();
  uint dst = OPER_AY_IX_16();
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmpi_16_aw(void)
{
  uint src = OPER_I_16();
  uint dst = OPER_AW_16();
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_cmpi_16_al(void)
{
  uint src = OPER_I_16();
  uint dst = OPER_AL_16();
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmpi_32_d(void)
{
  uint src = OPER_I_32();
  uint dst = DY;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpi_32_ai(void)
{
  uint src = OPER_I_32();
  uint dst = OPER_AY_AI_32();
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpi_32_pi(void)
{
  uint src = OPER_I_32();
  uint dst = OPER_AY_PI_32();
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpi_32_pd(void)
{
  uint src = OPER_I_32();
  uint dst = OPER_AY_PD_32();
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpi_32_di(void)
{
  uint src = OPER_I_32();
  uint dst = OPER_AY_DI_32();
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpi_32_ix(void)
{
  uint src = OPER_I_32();
  uint dst = OPER_AY_IX_32();
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpi_32_aw(void)
{
  uint src = OPER_I_32();
  uint dst = OPER_AW_32();
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpi_32_al(void)
{
  uint src = OPER_I_32();
  uint dst = OPER_AL_32();
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_cmpm_8_ax7(void)
{
  uint src = OPER_AY_PI_8();
  uint dst = OPER_A7_PI_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpm_8_ay7(void)
{
  uint src = OPER_A7_PI_8();
  uint dst = OPER_AX_PI_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpm_8_axy7(void)
{
  uint src = OPER_A7_PI_8();
  uint dst = OPER_A7_PI_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpm_8(void)
{
  uint src = OPER_AY_PI_8();
  uint dst = OPER_AX_PI_8();
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_C = CFLAG_8(res);
}


static void m68k_op_cmpm_16(void)
{
  uint src = OPER_AY_PI_16();
  uint dst = OPER_AX_PI_16();
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_C = CFLAG_16(res);
}


static void m68k_op_cmpm_32(void)
{
  uint src = OPER_AY_PI_32();
  uint dst = OPER_AX_PI_32();
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_C = CFLAG_SUB_32(src, dst, res);
}


static void m68k_op_dbt_16(void)
{
  REG_PC += 2;
}


static void m68k_op_dbf_16(void)
{
  uint* r_dst = &DY;
  uint res = MASK_OUT_ABOVE_16(*r_dst - 1);
  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  if(res != 0xffff)
  {
    uint offset = OPER_I_16();
    REG_PC -= 2;
    m68ki_branch_16(offset);
    USE_CYCLES(CYC_DBCC_F_NOEXP);

    /* reset idle loop detection */
    m68ki_cpu.poll.detected = 0;
    return;
  }
  REG_PC += 2;
  USE_CYCLES(CYC_DBCC_F_EXP);
}


static void m68k_op_dbhi_16(void)
{
  if(COND_NOT_HI())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dbls_16(void)
{
  if(COND_NOT_LS())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dbcc_16(void)
{
  if(COND_NOT_CC())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dbcs_16(void)
{
  if(COND_NOT_CS())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dbne_16(void)
{
  if(COND_NOT_NE())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dbeq_16(void)
{
  if(COND_NOT_EQ())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dbvc_16(void)
{
  if(COND_NOT_VC())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dbvs_16(void)
{
  if(COND_NOT_VS())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dbpl_16(void)
{
  if(COND_NOT_PL())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dbmi_16(void)
{
  if(COND_NOT_MI())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dbge_16(void)
{
  if(COND_NOT_GE())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dblt_16(void)
{
  if(COND_NOT_LT())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dbgt_16(void)
{
  if(COND_NOT_GT())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_dble_16(void)
{
  if(COND_NOT_LE())
  {
    uint* r_dst = &DY;
    uint res = MASK_OUT_ABOVE_16(*r_dst - 1);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    if(res != 0xffff)
    {
      uint offset = OPER_I_16();
      REG_PC -= 2;
      m68ki_branch_16(offset);
      USE_CYCLES(CYC_DBCC_F_NOEXP);

      /* reset idle loop detection */
      m68ki_cpu.poll.detected = 0;
      return;
    }
    REG_PC += 2;
    USE_CYCLES(CYC_DBCC_F_EXP);
    return;
  }
  REG_PC += 2;
}


static void m68k_op_divs_16_d(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(DY);
  sint quotient;
  sint remainder;

  if(src != 0)
  {
    UseDivsCycles(*r_dst,src);

    if((uint32)*r_dst == 0x80000000 && src == -1)
    {
      FLAG_Z = 0;
      FLAG_N = NFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = 0;
      return;
    }

    quotient = MAKE_INT_32(*r_dst) / src;
    remainder = MAKE_INT_32(*r_dst) % src;

    if(quotient == MAKE_INT_16(quotient))
    {
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_ai(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AY_AI_16());
  sint quotient;
  sint remainder;

  if(src != 0)
  {
    UseDivsCycles(*r_dst,src);

    if((uint32)*r_dst == 0x80000000 && src == -1)
    {
      FLAG_Z = 0;
      FLAG_N = NFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = 0;
      return;
    }

    quotient = MAKE_INT_32(*r_dst) / src;
    remainder = MAKE_INT_32(*r_dst) % src;

    if(quotient == MAKE_INT_16(quotient))
    {
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_pi(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AY_PI_16());
  sint quotient;
  sint remainder;

  if(src != 0)
  {
    UseDivsCycles(*r_dst,src);

    if((uint32)*r_dst == 0x80000000 && src == -1)
    {
      FLAG_Z = 0;
      FLAG_N = NFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = 0;
      return;
    }

    quotient = MAKE_INT_32(*r_dst) / src;
    remainder = MAKE_INT_32(*r_dst) % src;

    if(quotient == MAKE_INT_16(quotient))
    {
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_pd(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AY_PD_16());
  sint quotient;
  sint remainder;

  if(src != 0)
  {
    UseDivsCycles(*r_dst,src);

    if((uint32)*r_dst == 0x80000000 && src == -1)
    {
      FLAG_Z = 0;
      FLAG_N = NFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = 0;
      return;
    }

    quotient = MAKE_INT_32(*r_dst) / src;
    remainder = MAKE_INT_32(*r_dst) % src;

    if(quotient == MAKE_INT_16(quotient))
    {
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_di(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AY_DI_16());
  sint quotient;
  sint remainder;

  if(src != 0)
  {
    UseDivsCycles(*r_dst,src);

    if((uint32)*r_dst == 0x80000000 && src == -1)
    {
      FLAG_Z = 0;
      FLAG_N = NFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = 0;
      return;
    }

    quotient = MAKE_INT_32(*r_dst) / src;
    remainder = MAKE_INT_32(*r_dst) % src;

    if(quotient == MAKE_INT_16(quotient))
    {
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_ix(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AY_IX_16());
  sint quotient;
  sint remainder;

  if(src != 0)
  {
    UseDivsCycles(*r_dst,src);

    if((uint32)*r_dst == 0x80000000 && src == -1)
    {
      FLAG_Z = 0;
      FLAG_N = NFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = 0;
      return;
    }

    quotient = MAKE_INT_32(*r_dst) / src;
    remainder = MAKE_INT_32(*r_dst) % src;

    if(quotient == MAKE_INT_16(quotient))
    {
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_aw(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AW_16());
  sint quotient;
  sint remainder;

  if(src != 0)
  {
    UseDivsCycles(*r_dst,src);

    if((uint32)*r_dst == 0x80000000 && src == -1)
    {
      FLAG_Z = 0;
      FLAG_N = NFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = 0;
      return;
    }

    quotient = MAKE_INT_32(*r_dst) / src;
    remainder = MAKE_INT_32(*r_dst) % src;

    if(quotient == MAKE_INT_16(quotient))
    {
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_al(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AL_16());
  sint quotient;
  sint remainder;

  if(src != 0)
  {
    UseDivsCycles(*r_dst,src);

    if((uint32)*r_dst == 0x80000000 && src == -1)
    {
      FLAG_Z = 0;
      FLAG_N = NFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = 0;
      return;
    }

    quotient = MAKE_INT_32(*r_dst) / src;
    remainder = MAKE_INT_32(*r_dst) % src;

    if(quotient == MAKE_INT_16(quotient))
    {
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_pcdi(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_PCDI_16());
  sint quotient;
  sint remainder;

  if(src != 0)
  {
    UseDivsCycles(*r_dst,src);

    if((uint32)*r_dst == 0x80000000 && src == -1)
    {
      FLAG_Z = 0;
      FLAG_N = NFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = 0;
      return;
    }

    quotient = MAKE_INT_32(*r_dst) / src;
    remainder = MAKE_INT_32(*r_dst) % src;

    if(quotient == MAKE_INT_16(quotient))
    {
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_pcix(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_PCIX_16());
  sint quotient;
  sint remainder;

  if(src != 0)
  {
    UseDivsCycles(*r_dst,src);

    if((uint32)*r_dst == 0x80000000 && src == -1)
    {
      FLAG_Z = 0;
      FLAG_N = NFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = 0;
      return;
    }

    quotient = MAKE_INT_32(*r_dst) / src;
    remainder = MAKE_INT_32(*r_dst) % src;

    if(quotient == MAKE_INT_16(quotient))
    {
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divs_16_i(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_I_16());
  sint quotient;
  sint remainder;

  if(src != 0)
  {
    UseDivsCycles(*r_dst,src);

    if((uint32)*r_dst == 0x80000000 && src == -1)
    {
      FLAG_Z = 0;
      FLAG_N = NFLAG_CLEAR;
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = 0;
      return;
    }

    quotient = MAKE_INT_32(*r_dst) / src;
    remainder = MAKE_INT_32(*r_dst) % src;

    if(quotient == MAKE_INT_16(quotient))
    {
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_d(void)
{
  uint* r_dst = &DX;
  uint src = MASK_OUT_ABOVE_16(DY);

  if(src != 0)
  {
    uint quotient = *r_dst / src;
    uint remainder = *r_dst % src;

    if(quotient < 0x10000)
    {
      UseDivuCycles(*r_dst,src);
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    USE_CYCLES(MUL * 10);
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_ai(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_AI_16();

  if(src != 0)
  {
    uint quotient = *r_dst / src;
    uint remainder = *r_dst % src;

    if(quotient < 0x10000)
    {
      UseDivuCycles(*r_dst,src);
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    USE_CYCLES(MUL * 10);
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_pi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PI_16();

  if(src != 0)
  {
    uint quotient = *r_dst / src;
    uint remainder = *r_dst % src;

    if(quotient < 0x10000)
    {
      UseDivuCycles(*r_dst,src);
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    USE_CYCLES(MUL * 10);
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_pd(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PD_16();

  if(src != 0)
  {
    uint quotient = *r_dst / src;
    uint remainder = *r_dst % src;

    if(quotient < 0x10000)
    {
      UseDivuCycles(*r_dst,src);
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    USE_CYCLES(MUL * 10);
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_di(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_DI_16();

  if(src != 0)
  {
    uint quotient = *r_dst / src;
    uint remainder = *r_dst % src;

    if(quotient < 0x10000)
    {
      UseDivuCycles(*r_dst,src);
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    USE_CYCLES(MUL * 10);
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_ix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_IX_16();

  if(src != 0)
  {
    uint quotient = *r_dst / src;
    uint remainder = *r_dst % src;

    if(quotient < 0x10000)
    {
      UseDivuCycles(*r_dst,src);
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    USE_CYCLES(MUL * 10);
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_aw(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AW_16();

  if(src != 0)
  {
    uint quotient = *r_dst / src;
    uint remainder = *r_dst % src;

    if(quotient < 0x10000)
    {
      UseDivuCycles(*r_dst,src);
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    USE_CYCLES(MUL * 10);
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_al(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AL_16();

  if(src != 0)
  {
    uint quotient = *r_dst / src;
    uint remainder = *r_dst % src;

    if(quotient < 0x10000)
    {
      UseDivuCycles(*r_dst,src);
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    USE_CYCLES(MUL * 10);
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_pcdi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCDI_16();

  if(src != 0)
  {
    uint quotient = *r_dst / src;
    uint remainder = *r_dst % src;

    if(quotient < 0x10000)
    {
      UseDivuCycles(*r_dst,src);
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    USE_CYCLES(MUL * 10);
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_pcix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCIX_16();

  if(src != 0)
  {
    uint quotient = *r_dst / src;
    uint remainder = *r_dst % src;

    if(quotient < 0x10000)
    {
      UseDivuCycles(*r_dst,src);
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    USE_CYCLES(MUL * 10);
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_divu_16_i(void)
{
  uint* r_dst = &DX;
  uint src = OPER_I_16();

  if(src != 0)
  {
    uint quotient = *r_dst / src;
    uint remainder = *r_dst % src;

    if(quotient < 0x10000)
    {
      UseDivuCycles(*r_dst,src);
      FLAG_Z = quotient;
      FLAG_N = NFLAG_16(quotient);
      FLAG_V = VFLAG_CLEAR;
      FLAG_C = CFLAG_CLEAR;
      *r_dst = MASK_OUT_ABOVE_32(MASK_OUT_ABOVE_16(quotient) | (remainder << 16));
      return;
    }
    USE_CYCLES(MUL * 10);
    FLAG_V = VFLAG_SET;
    FLAG_N = NFLAG_SET; /* undocumented behavior (fixes Blood Shot on Genesis) */
    FLAG_C = CFLAG_CLEAR;
    return;
  }
  FLAG_C = CFLAG_CLEAR;
  m68ki_exception_trap(EXCEPTION_ZERO_DIVIDE);
}


static void m68k_op_eor_8_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY ^= MASK_OUT_ABOVE_8(DX));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_di(void)
{
  uint ea = EA_AY_DI_8();
  uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_aw(void)
{
  uint ea = EA_AW_8();
  uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_8_al(void)
{
  uint ea = EA_AL_8();
  uint res = MASK_OUT_ABOVE_8(DX ^ m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_d(void)
{
  uint res = MASK_OUT_ABOVE_16(DY ^= MASK_OUT_ABOVE_16(DX));

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_di(void)
{
  uint ea = EA_AY_DI_16();
  uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_aw(void)
{
  uint ea = EA_AW_16();
  uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_16_al(void)
{
  uint ea = EA_AL_16();
  uint res = MASK_OUT_ABOVE_16(DX ^ m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_d(void)
{
  uint res = DY ^= DX;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_ai(void)
{
  uint ea = EA_AY_AI_32();
  uint res = DX ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_pi(void)
{
  uint ea = EA_AY_PI_32();
  uint res = DX ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_pd(void)
{
  uint ea = EA_AY_PD_32();
  uint res = DX ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_di(void)
{
  uint ea = EA_AY_DI_32();
  uint res = DX ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_ix(void)
{
  uint ea = EA_AY_IX_32();
  uint res = DX ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_aw(void)
{
  uint ea = EA_AW_32();
  uint res = DX ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eor_32_al(void)
{
  uint ea = EA_AL_32();
  uint res = DX ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY ^= OPER_I_8());

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_ai(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_AI_8();
  uint res = src ^ m68ki_read_8(ea);

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_pi(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_PI_8();
  uint res = src ^ m68ki_read_8(ea);

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_pi7(void)
{
  uint src = OPER_I_8();
  uint ea = EA_A7_PI_8();
  uint res = src ^ m68ki_read_8(ea);

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_pd(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_PD_8();
  uint res = src ^ m68ki_read_8(ea);

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_pd7(void)
{
  uint src = OPER_I_8();
  uint ea = EA_A7_PD_8();
  uint res = src ^ m68ki_read_8(ea);

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_di(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_DI_8();
  uint res = src ^ m68ki_read_8(ea);

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_ix(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_IX_8();
  uint res = src ^ m68ki_read_8(ea);

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_aw(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AW_8();
  uint res = src ^ m68ki_read_8(ea);

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_8_al(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AL_8();
  uint res = src ^ m68ki_read_8(ea);

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_d(void)
{
  uint res = MASK_OUT_ABOVE_16(DY ^= OPER_I_16());

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_ai(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_AI_16();
  uint res = src ^ m68ki_read_16(ea);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_pi(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_PI_16();
  uint res = src ^ m68ki_read_16(ea);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_pd(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_PD_16();
  uint res = src ^ m68ki_read_16(ea);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_di(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_DI_16();
  uint res = src ^ m68ki_read_16(ea);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_ix(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_IX_16();
  uint res = src ^ m68ki_read_16(ea);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_aw(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AW_16();
  uint res = src ^ m68ki_read_16(ea);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_al(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AL_16();
  uint res = src ^ m68ki_read_16(ea);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_d(void)
{
  uint res = DY ^= OPER_I_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_ai(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_AI_32();
  uint res = src ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_pi(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_PI_32();
  uint res = src ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_pd(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_PD_32();
  uint res = src ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_di(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_DI_32();
  uint res = src ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_ix(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_IX_32();
  uint res = src ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_aw(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AW_32();
  uint res = src ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_32_al(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AL_32();
  uint res = src ^ m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_eori_16_toc(void)
{
  m68ki_set_ccr(m68ki_get_ccr() ^ OPER_I_16());
}


static void m68k_op_eori_16_tos(void)
{
  if(FLAG_S)
  {
    uint src = OPER_I_16();
    m68ki_set_sr(m68ki_get_sr() ^ src);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_exg_32_dd(void)
{
  uint* reg_a = &DX;
  uint* reg_b = &DY;
  uint tmp = *reg_a;
  *reg_a = *reg_b;
  *reg_b = tmp;
}


static void m68k_op_exg_32_aa(void)
{
  uint* reg_a = &AX;
  uint* reg_b = &AY;
  uint tmp = *reg_a;
  *reg_a = *reg_b;
  *reg_b = tmp;
}


static void m68k_op_exg_32_da(void)
{
  uint* reg_a = &DX;
  uint* reg_b = &AY;
  uint tmp = *reg_a;
  *reg_a = *reg_b;
  *reg_b = tmp;
}


static void m68k_op_ext_16(void)
{
  uint* r_dst = &DY;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | MASK_OUT_ABOVE_8(*r_dst) | (GET_MSB_8(*r_dst) ? 0xff00 : 0);

  FLAG_N = NFLAG_16(*r_dst);
  FLAG_Z = MASK_OUT_ABOVE_16(*r_dst);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_ext_32(void)
{
  uint* r_dst = &DY;

  *r_dst = MASK_OUT_ABOVE_16(*r_dst) | (GET_MSB_16(*r_dst) ? 0xffff0000 : 0);

  FLAG_N = NFLAG_32(*r_dst);
  FLAG_Z = *r_dst;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_illegal(void)
{
  m68ki_exception_illegal();
}


static void m68k_op_jmp_32_ai(void)
{
  m68ki_jump(EA_AY_AI_32());
}


static void m68k_op_jmp_32_di(void)
{
  m68ki_jump(EA_AY_DI_32());
}


static void m68k_op_jmp_32_ix(void)
{
  m68ki_jump(EA_AY_IX_32());
}


static void m68k_op_jmp_32_aw(void)
{
  m68ki_jump(EA_AW_32());
}


static void m68k_op_jmp_32_al(void)
{
  m68ki_jump(EA_AL_32());
}


static void m68k_op_jmp_32_pcdi(void)
{
  m68ki_jump(EA_PCDI_32());
}


static void m68k_op_jmp_32_pcix(void)
{
  m68ki_jump(EA_PCIX_32());
}


static void m68k_op_jsr_32_ai(void)
{
  uint ea = EA_AY_AI_32();
  m68ki_push_32(REG_PC);
  m68ki_jump(ea);
}


static void m68k_op_jsr_32_di(void)
{
  uint ea = EA_AY_DI_32();
  m68ki_push_32(REG_PC);
  m68ki_jump(ea);
}


static void m68k_op_jsr_32_ix(void)
{
  uint ea = EA_AY_IX_32();
  m68ki_push_32(REG_PC);
  m68ki_jump(ea);
}


static void m68k_op_jsr_32_aw(void)
{
  uint ea = EA_AW_32();
  m68ki_push_32(REG_PC);
  m68ki_jump(ea);
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_jsr_32_al(void)
{
  uint ea = EA_AL_32();
  m68ki_push_32(REG_PC);
  m68ki_jump(ea);
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_jsr_32_pcdi(void)
{
  uint ea = EA_PCDI_32();
  m68ki_push_32(REG_PC);
  m68ki_jump(ea);
}


static void m68k_op_jsr_32_pcix(void)
{
  uint ea = EA_PCIX_32();
  m68ki_push_32(REG_PC);
  m68ki_jump(ea);
}


static void m68k_op_lea_32_ai(void)
{
  AX = EA_AY_AI_32();
}


static void m68k_op_lea_32_di(void)
{
  AX = EA_AY_DI_32();
}


static void m68k_op_lea_32_ix(void)
{
  AX = EA_AY_IX_32();
}


static void m68k_op_lea_32_aw(void)
{
  AX = EA_AW_32();
}


static void m68k_op_lea_32_al(void)
{
  AX = EA_AL_32();
}


static void m68k_op_lea_32_pcdi(void)
{
  AX = EA_PCDI_32();
}


static void m68k_op_lea_32_pcix(void)
{
  AX = EA_PCIX_32();
}


static void m68k_op_link_16_a7(void)
{
  REG_A[7] -= 4;
  m68ki_write_32(REG_A[7], REG_A[7]);
  REG_A[7] = MASK_OUT_ABOVE_32(REG_A[7] + MAKE_INT_16(OPER_I_16()));
}


static void m68k_op_link_16(void)
{
  uint* r_dst = &AY;

  m68ki_push_32(*r_dst);
  *r_dst = REG_A[7];
  REG_A[7] = MASK_OUT_ABOVE_32(REG_A[7] + MAKE_INT_16(OPER_I_16()));
}


static void m68k_op_lsr_8_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src >> shift;

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_CLEAR;
  FLAG_Z = res;
  FLAG_X = FLAG_C = src << (9-shift);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src >> shift;

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_CLEAR;
  FLAG_Z = res;
  FLAG_X = FLAG_C = src << (9-shift);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_32_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = *r_dst;
  uint res = src >> shift;

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = res;

  FLAG_N = NFLAG_CLEAR;
  FLAG_Z = res;
  FLAG_X = FLAG_C = src << (9-shift);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_8_r(void)
{
  uint* r_dst = &DY;
  uint shift = DX & 0x3f;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = src >> shift;

  if(shift != 0)
  {
    USE_CYCLES(shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift <= 8)
    {
      *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
      FLAG_X = FLAG_C = src << (9-shift);
      FLAG_N = NFLAG_CLEAR;
      FLAG_Z = res;
      FLAG_V = VFLAG_CLEAR;
      return;
    }

    *r_dst &= 0xffffff00;
    FLAG_X = XFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
    FLAG_Z = ZFLAG_SET;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_8(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_r(void)
{
  uint* r_dst = &DY;
  uint shift = DX & 0x3f;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = src >> shift;

  if(shift != 0)
  {
    USE_CYCLES(shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift <= 16)
    {
      *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
      FLAG_C = FLAG_X = (src >> (shift - 1))<<8;
      FLAG_N = NFLAG_CLEAR;
      FLAG_Z = res;
      FLAG_V = VFLAG_CLEAR;
      return;
    }

    *r_dst &= 0xffff0000;
    FLAG_X = XFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
    FLAG_Z = ZFLAG_SET;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_16(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_32_r(void)
{
  uint* r_dst = &DY;
  uint shift = DX & 0x3f;
  uint src = *r_dst;
  uint res = src >> shift;

  if(shift != 0)
  {
    USE_CYCLES(shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift < 32)
    {
      *r_dst = res;
      FLAG_C = FLAG_X = (src >> (shift - 1))<<8;
      FLAG_N = NFLAG_CLEAR;
      FLAG_Z = res;
      FLAG_V = VFLAG_CLEAR;
      return;
    }

    *r_dst = 0;
    FLAG_X = FLAG_C = (shift == 32 ? GET_MSB_32(src)>>23 : 0);
    FLAG_N = NFLAG_CLEAR;
    FLAG_Z = ZFLAG_SET;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_32(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_CLEAR;
  FLAG_Z = res;
  FLAG_C = FLAG_X = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_CLEAR;
  FLAG_Z = res;
  FLAG_C = FLAG_X = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_CLEAR;
  FLAG_Z = res;
  FLAG_C = FLAG_X = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_di(void)
{
  uint ea = EA_AY_DI_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_CLEAR;
  FLAG_Z = res;
  FLAG_C = FLAG_X = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_CLEAR;
  FLAG_Z = res;
  FLAG_C = FLAG_X = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_aw(void)
{
  uint ea = EA_AW_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_CLEAR;
  FLAG_Z = res;
  FLAG_C = FLAG_X = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsr_16_al(void)
{
  uint ea = EA_AL_16();
  uint src = m68ki_read_16(ea);
  uint res = src >> 1;

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_CLEAR;
  FLAG_Z = res;
  FLAG_C = FLAG_X = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_8_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = MASK_OUT_ABOVE_8(src << shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src << shift;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = MASK_OUT_ABOVE_16(src << shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> (8-shift);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_32_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = *r_dst;
  uint res = MASK_OUT_ABOVE_32(src << shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> (24-shift);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_8_r(void)
{
  uint* r_dst = &DY;
  uint shift = DX & 0x3f;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = MASK_OUT_ABOVE_8(src << shift);

  if(shift != 0)
  {
    USE_CYCLES(shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift <= 8)
    {
      *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
      FLAG_X = FLAG_C = src << shift;
      FLAG_N = NFLAG_8(res);
      FLAG_Z = res;
      FLAG_V = VFLAG_CLEAR;
      return;
    }

    *r_dst &= 0xffffff00;
    FLAG_X = XFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
    FLAG_Z = ZFLAG_SET;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_8(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_r(void)
{
  uint* r_dst = &DY;
  uint shift = DX & 0x3f;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = MASK_OUT_ABOVE_16(src << shift);

  if(shift != 0)
  {
    USE_CYCLES(shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift <= 16)
    {
      *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
      FLAG_X = FLAG_C = (src << shift) >> 8;
      FLAG_N = NFLAG_16(res);
      FLAG_Z = res;
      FLAG_V = VFLAG_CLEAR;
      return;
    }

    *r_dst &= 0xffff0000;
    FLAG_X = XFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
    FLAG_Z = ZFLAG_SET;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_16(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_32_r(void)
{
  uint* r_dst = &DY;
  uint shift = DX & 0x3f;
  uint src = *r_dst;
  uint res = MASK_OUT_ABOVE_32(src << shift);

  if(shift != 0)
  {
    USE_CYCLES(shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift < 32)
    {
      *r_dst = res;
      FLAG_X = FLAG_C = (src >> (32 - shift)) << 8;
      FLAG_N = NFLAG_32(res);
      FLAG_Z = res;
      FLAG_V = VFLAG_CLEAR;
      return;
    }

    *r_dst = 0;
    FLAG_X = FLAG_C = ((shift == 32 ? src & 1 : 0))<<8;
    FLAG_N = NFLAG_CLEAR;
    FLAG_Z = ZFLAG_SET;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_32(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_di(void)
{
  uint ea = EA_AY_DI_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_aw(void)
{
  uint ea = EA_AW_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_lsl_16_al(void)
{
  uint ea = EA_AL_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(src << 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_X = FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_move_8_d_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY);
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_ai(void)
{
  uint res = OPER_AY_AI_8();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_pi(void)
{
  uint res = OPER_AY_PI_8();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_pi7(void)
{
  uint res = OPER_A7_PI_8();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_pd(void)
{
  uint res = OPER_AY_PD_8();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_pd7(void)
{
  uint res = OPER_A7_PD_8();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_move_8_d_di(void)
{
  uint res = OPER_AY_DI_8();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_ix(void)
{
  uint res = OPER_AY_IX_8();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_aw(void)
{
  uint res = OPER_AW_8();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_move_8_d_al(void)
{
  uint res = OPER_AL_8();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_pcdi(void)
{
  uint res = OPER_PCDI_8();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_pcix(void)
{
  uint res = OPER_PCIX_8();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_d_i(void)
{
  uint res = OPER_I_8();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_8_ai_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY);
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ai_ai(void)
{
  uint res = OPER_AY_AI_8();
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ai_pi(void)
{
  uint res = OPER_AY_PI_8();
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ai_pi7(void)
{
  uint res = OPER_A7_PI_8();
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ai_pd(void)
{
  uint res = OPER_AY_PD_8();
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ai_pd7(void)
{
  uint res = OPER_A7_PD_8();
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ai_di(void)
{
  uint res = OPER_AY_DI_8();
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ai_ix(void)
{
  uint res = OPER_AY_IX_8();
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ai_aw(void)
{
  uint res = OPER_AW_8();
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ai_al(void)
{
  uint res = OPER_AL_8();
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ai_pcdi(void)
{
  uint res = OPER_PCDI_8();
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ai_pcix(void)
{
  uint res = OPER_PCIX_8();
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ai_i(void)
{
  uint res = OPER_I_8();
  uint ea = EA_AX_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY);
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY);
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_ai(void)
{
  uint res = OPER_AY_AI_8();
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_pi(void)
{
  uint res = OPER_AY_PI_8();
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_pi7(void)
{
  uint res = OPER_A7_PI_8();
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_pd(void)
{
  uint res = OPER_AY_PD_8();
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_pd7(void)
{
  uint res = OPER_A7_PD_8();
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_di(void)
{
  uint res = OPER_AY_DI_8();
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_ix(void)
{
  uint res = OPER_AY_IX_8();
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_aw(void)
{
  uint res = OPER_AW_8();
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_al(void)
{
  uint res = OPER_AL_8();
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_pcdi(void)
{
  uint res = OPER_PCDI_8();
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_pcix(void)
{
  uint res = OPER_PCIX_8();
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi7_i(void)
{
  uint res = OPER_I_8();
  uint ea = EA_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_ai(void)
{
  uint res = OPER_AY_AI_8();
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_pi(void)
{
  uint res = OPER_AY_PI_8();
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_pi7(void)
{
  uint res = OPER_A7_PI_8();
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_pd(void)
{
  uint res = OPER_AY_PD_8();
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_pd7(void)
{
  uint res = OPER_A7_PD_8();
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_di(void)
{
  uint res = OPER_AY_DI_8();
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_ix(void)
{
  uint res = OPER_AY_IX_8();
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_aw(void)
{
  uint res = OPER_AW_8();
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_al(void)
{
  uint res = OPER_AL_8();
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_pcdi(void)
{
  uint res = OPER_PCDI_8();
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_pcix(void)
{
  uint res = OPER_PCIX_8();
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pi_i(void)
{
  uint res = OPER_I_8();
  uint ea = EA_AX_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY);
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY);
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_ai(void)
{
  uint res = OPER_AY_AI_8();
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_pi(void)
{
  uint res = OPER_AY_PI_8();
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_pi7(void)
{
  uint res = OPER_A7_PI_8();
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_pd(void)
{
  uint res = OPER_AY_PD_8();
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_pd7(void)
{
  uint res = OPER_A7_PD_8();
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_di(void)
{
  uint res = OPER_AY_DI_8();
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_ix(void)
{
  uint res = OPER_AY_IX_8();
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_aw(void)
{
  uint res = OPER_AW_8();
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_al(void)
{
  uint res = OPER_AL_8();
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_pcdi(void)
{
  uint res = OPER_PCDI_8();
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_pcix(void)
{
  uint res = OPER_PCIX_8();
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd7_i(void)
{
  uint res = OPER_I_8();
  uint ea = EA_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_ai(void)
{
  uint res = OPER_AY_AI_8();
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_pi(void)
{
  uint res = OPER_AY_PI_8();
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_pi7(void)
{
  uint res = OPER_A7_PI_8();
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_pd(void)
{
  uint res = OPER_AY_PD_8();
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_pd7(void)
{
  uint res = OPER_A7_PD_8();
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_di(void)
{
  uint res = OPER_AY_DI_8();
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_ix(void)
{
  uint res = OPER_AY_IX_8();
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_aw(void)
{
  uint res = OPER_AW_8();
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_al(void)
{
  uint res = OPER_AL_8();
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_pcdi(void)
{
  uint res = OPER_PCDI_8();
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_pcix(void)
{
  uint res = OPER_PCIX_8();
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_pd_i(void)
{
  uint res = OPER_I_8();
  uint ea = EA_AX_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY);
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_ai(void)
{
  uint res = OPER_AY_AI_8();
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_pi(void)
{
  uint res = OPER_AY_PI_8();
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_pi7(void)
{
  uint res = OPER_A7_PI_8();
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_pd(void)
{
  uint res = OPER_AY_PD_8();
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_pd7(void)
{
  uint res = OPER_A7_PD_8();
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_di(void)
{
  uint res = OPER_AY_DI_8();
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_ix(void)
{
  uint res = OPER_AY_IX_8();
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_aw(void)
{
  uint res = OPER_AW_8();
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_al(void)
{
  uint res = OPER_AL_8();
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_pcdi(void)
{
  uint res = OPER_PCDI_8();
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_pcix(void)
{
  uint res = OPER_PCIX_8();
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_di_i(void)
{
  uint res = OPER_I_8();
  uint ea = EA_AX_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY);
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_ai(void)
{
  uint res = OPER_AY_AI_8();
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_pi(void)
{
  uint res = OPER_AY_PI_8();
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_pi7(void)
{
  uint res = OPER_A7_PI_8();
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_pd(void)
{
  uint res = OPER_AY_PD_8();
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_pd7(void)
{
  uint res = OPER_A7_PD_8();
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_di(void)
{
  uint res = OPER_AY_DI_8();
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_ix(void)
{
  uint res = OPER_AY_IX_8();
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_aw(void)
{
  uint res = OPER_AW_8();
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_al(void)
{
  uint res = OPER_AL_8();
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_pcdi(void)
{
  uint res = OPER_PCDI_8();
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_pcix(void)
{
  uint res = OPER_PCIX_8();
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_ix_i(void)
{
  uint res = OPER_I_8();
  uint ea = EA_AX_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY);
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_ai(void)
{
  uint res = OPER_AY_AI_8();
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_pi(void)
{
  uint res = OPER_AY_PI_8();
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_pi7(void)
{
  uint res = OPER_A7_PI_8();
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_pd(void)
{
  uint res = OPER_AY_PD_8();
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_pd7(void)
{
  uint res = OPER_A7_PD_8();
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_di(void)
{
  uint res = OPER_AY_DI_8();
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_ix(void)
{
  uint res = OPER_AY_IX_8();
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_aw(void)
{
  uint res = OPER_AW_8();
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_al(void)
{
  uint res = OPER_AL_8();
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_pcdi(void)
{
  uint res = OPER_PCDI_8();
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_pcix(void)
{
  uint res = OPER_PCIX_8();
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_aw_i(void)
{
  uint res = OPER_I_8();
  uint ea = EA_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY);
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_ai(void)
{
  uint res = OPER_AY_AI_8();
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_pi(void)
{
  uint res = OPER_AY_PI_8();
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_pi7(void)
{
  uint res = OPER_A7_PI_8();
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_pd(void)
{
  uint res = OPER_AY_PD_8();
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_pd7(void)
{
  uint res = OPER_A7_PD_8();
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_di(void)
{
  uint res = OPER_AY_DI_8();
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_ix(void)
{
  uint res = OPER_AY_IX_8();
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_aw(void)
{
  uint res = OPER_AW_8();
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_al(void)
{
  uint res = OPER_AL_8();
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_pcdi(void)
{
  uint res = OPER_PCDI_8();
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_pcix(void)
{
  uint res = OPER_PCIX_8();
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_8_al_i(void)
{
  uint res = OPER_I_8();
  uint ea = EA_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_8(ea, res);
}


static void m68k_op_move_16_d_d(void)
{
  uint res = MASK_OUT_ABOVE_16(DY);
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_a(void)
{
  uint res = MASK_OUT_ABOVE_16(AY);
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_ai(void)
{
  uint res = OPER_AY_AI_16();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_pi(void)
{
  uint res = OPER_AY_PI_16();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_pd(void)
{
  uint res = OPER_AY_PD_16();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_di(void)
{
  uint res = OPER_AY_DI_16();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_ix(void)
{
  uint res = OPER_AY_IX_16();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_aw(void)
{
  uint res = OPER_AW_16();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_al(void)
{
  uint res = OPER_AL_16();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_pcdi(void)
{
  uint res = OPER_PCDI_16();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_pcix(void)
{
  uint res = OPER_PCIX_16();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_d_i(void)
{
  uint res = OPER_I_16();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_16_ai_d(void)
{
  uint res = MASK_OUT_ABOVE_16(DY);
  uint ea = EA_AX_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ai_a(void)
{
  uint res = MASK_OUT_ABOVE_16(AY);
  uint ea = EA_AX_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ai_ai(void)
{
  uint res = OPER_AY_AI_16();
  uint ea = EA_AX_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ai_pi(void)
{
  uint res = OPER_AY_PI_16();
  uint ea = EA_AX_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ai_pd(void)
{
  uint res = OPER_AY_PD_16();
  uint ea = EA_AX_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ai_di(void)
{
  uint res = OPER_AY_DI_16();
  uint ea = EA_AX_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ai_ix(void)
{
  uint res = OPER_AY_IX_16();
  uint ea = EA_AX_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ai_aw(void)
{
  uint res = OPER_AW_16();
  uint ea = EA_AX_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ai_al(void)
{
  uint res = OPER_AL_16();
  uint ea = EA_AX_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ai_pcdi(void)
{
  uint res = OPER_PCDI_16();
  uint ea = EA_AX_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ai_pcix(void)
{
  uint res = OPER_PCIX_16();
  uint ea = EA_AX_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ai_i(void)
{
  uint res = OPER_I_16();
  uint ea = EA_AX_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pi_d(void)
{
  uint res = MASK_OUT_ABOVE_16(DY);
  uint ea = EA_AX_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pi_a(void)
{
  uint res = MASK_OUT_ABOVE_16(AY);
  uint ea = EA_AX_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pi_ai(void)
{
  uint res = OPER_AY_AI_16();
  uint ea = EA_AX_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pi_pi(void)
{
  uint res = OPER_AY_PI_16();
  uint ea = EA_AX_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pi_pd(void)
{
  uint res = OPER_AY_PD_16();
  uint ea = EA_AX_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pi_di(void)
{
  uint res = OPER_AY_DI_16();
  uint ea = EA_AX_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pi_ix(void)
{
  uint res = OPER_AY_IX_16();
  uint ea = EA_AX_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pi_aw(void)
{
  uint res = OPER_AW_16();
  uint ea = EA_AX_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pi_al(void)
{
  uint res = OPER_AL_16();
  uint ea = EA_AX_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pi_pcdi(void)
{
  uint res = OPER_PCDI_16();
  uint ea = EA_AX_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pi_pcix(void)
{
  uint res = OPER_PCIX_16();
  uint ea = EA_AX_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pi_i(void)
{
  uint res = OPER_I_16();
  uint ea = EA_AX_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pd_d(void)
{
  uint res = MASK_OUT_ABOVE_16(DY);
  uint ea = EA_AX_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pd_a(void)
{
  uint res = MASK_OUT_ABOVE_16(AY);
  uint ea = EA_AX_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pd_ai(void)
{
  uint res = OPER_AY_AI_16();
  uint ea = EA_AX_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pd_pi(void)
{
  uint res = OPER_AY_PI_16();
  uint ea = EA_AX_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pd_pd(void)
{
  uint res = OPER_AY_PD_16();
  uint ea = EA_AX_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pd_di(void)
{
  uint res = OPER_AY_DI_16();
  uint ea = EA_AX_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pd_ix(void)
{
  uint res = OPER_AY_IX_16();
  uint ea = EA_AX_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pd_aw(void)
{
  uint res = OPER_AW_16();
  uint ea = EA_AX_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pd_al(void)
{
  uint res = OPER_AL_16();
  uint ea = EA_AX_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pd_pcdi(void)
{
  uint res = OPER_PCDI_16();
  uint ea = EA_AX_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pd_pcix(void)
{
  uint res = OPER_PCIX_16();
  uint ea = EA_AX_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_pd_i(void)
{
  uint res = OPER_I_16();
  uint ea = EA_AX_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_di_d(void)
{
  uint res = MASK_OUT_ABOVE_16(DY);
  uint ea = EA_AX_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_di_a(void)
{
  uint res = MASK_OUT_ABOVE_16(AY);
  uint ea = EA_AX_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_di_ai(void)
{
  uint res = OPER_AY_AI_16();
  uint ea = EA_AX_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_di_pi(void)
{
  uint res = OPER_AY_PI_16();
  uint ea = EA_AX_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_di_pd(void)
{
  uint res = OPER_AY_PD_16();
  uint ea = EA_AX_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_di_di(void)
{
  uint res = OPER_AY_DI_16();
  uint ea = EA_AX_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_di_ix(void)
{
  uint res = OPER_AY_IX_16();
  uint ea = EA_AX_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_di_aw(void)
{
  uint res = OPER_AW_16();
  uint ea = EA_AX_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_di_al(void)
{
  uint res = OPER_AL_16();
  uint ea = EA_AX_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_di_pcdi(void)
{
  uint res = OPER_PCDI_16();
  uint ea = EA_AX_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_di_pcix(void)
{
  uint res = OPER_PCIX_16();
  uint ea = EA_AX_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_di_i(void)
{
  uint res = OPER_I_16();
  uint ea = EA_AX_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ix_d(void)
{
  uint res = MASK_OUT_ABOVE_16(DY);
  uint ea = EA_AX_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ix_a(void)
{
  uint res = MASK_OUT_ABOVE_16(AY);
  uint ea = EA_AX_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ix_ai(void)
{
  uint res = OPER_AY_AI_16();
  uint ea = EA_AX_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ix_pi(void)
{
  uint res = OPER_AY_PI_16();
  uint ea = EA_AX_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ix_pd(void)
{
  uint res = OPER_AY_PD_16();
  uint ea = EA_AX_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ix_di(void)
{
  uint res = OPER_AY_DI_16();
  uint ea = EA_AX_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ix_ix(void)
{
  uint res = OPER_AY_IX_16();
  uint ea = EA_AX_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ix_aw(void)
{
  uint res = OPER_AW_16();
  uint ea = EA_AX_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ix_al(void)
{
  uint res = OPER_AL_16();
  uint ea = EA_AX_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ix_pcdi(void)
{
  uint res = OPER_PCDI_16();
  uint ea = EA_AX_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ix_pcix(void)
{
  uint res = OPER_PCIX_16();
  uint ea = EA_AX_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_ix_i(void)
{
  uint res = OPER_I_16();
  uint ea = EA_AX_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_aw_d(void)
{
  uint res = MASK_OUT_ABOVE_16(DY);
  uint ea = EA_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_aw_a(void)
{
  uint res = MASK_OUT_ABOVE_16(AY);
  uint ea = EA_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_aw_ai(void)
{
  uint res = OPER_AY_AI_16();
  uint ea = EA_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_aw_pi(void)
{
  uint res = OPER_AY_PI_16();
  uint ea = EA_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_aw_pd(void)
{
  uint res = OPER_AY_PD_16();
  uint ea = EA_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_aw_di(void)
{
  uint res = OPER_AY_DI_16();
  uint ea = EA_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_aw_ix(void)
{
  uint res = OPER_AY_IX_16();
  uint ea = EA_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_aw_aw(void)
{
  uint res = OPER_AW_16();
  uint ea = EA_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_aw_al(void)
{
  uint res = OPER_AL_16();
  uint ea = EA_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_aw_pcdi(void)
{
  uint res = OPER_PCDI_16();
  uint ea = EA_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_aw_pcix(void)
{
  uint res = OPER_PCIX_16();
  uint ea = EA_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_aw_i(void)
{
  uint res = OPER_I_16();
  uint ea = EA_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_al_d(void)
{
  uint res = MASK_OUT_ABOVE_16(DY);
  uint ea = EA_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_al_a(void)
{
  uint res = MASK_OUT_ABOVE_16(AY);
  uint ea = EA_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_al_ai(void)
{
  uint res = OPER_AY_AI_16();
  uint ea = EA_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_al_pi(void)
{
  uint res = OPER_AY_PI_16();
  uint ea = EA_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_al_pd(void)
{
  uint res = OPER_AY_PD_16();
  uint ea = EA_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_al_di(void)
{
  uint res = OPER_AY_DI_16();
  uint ea = EA_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_al_ix(void)
{
  uint res = OPER_AY_IX_16();
  uint ea = EA_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_al_aw(void)
{
  uint res = OPER_AW_16();
  uint ea = EA_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_al_al(void)
{
  uint res = OPER_AL_16();
  uint ea = EA_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_al_pcdi(void)
{
  uint res = OPER_PCDI_16();
  uint ea = EA_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_al_pcix(void)
{
  uint res = OPER_PCIX_16();
  uint ea = EA_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_16_al_i(void)
{
  uint res = OPER_I_16();
  uint ea = EA_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea, res);
}


static void m68k_op_move_32_d_d(void)
{
  uint res = DY;
  uint* r_dst = &DX;

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_a(void)
{
  uint res = AY;
  uint* r_dst = &DX;

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_ai(void)
{
  uint res = OPER_AY_AI_32();
  uint* r_dst = &DX;

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_pi(void)
{
  uint res = OPER_AY_PI_32();
  uint* r_dst = &DX;

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_pd(void)
{
  uint res = OPER_AY_PD_32();
  uint* r_dst = &DX;

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_di(void)
{
  uint res = OPER_AY_DI_32();
  uint* r_dst = &DX;

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_ix(void)
{
  uint res = OPER_AY_IX_32();
  uint* r_dst = &DX;

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_aw(void)
{
  uint res = OPER_AW_32();
  uint* r_dst = &DX;

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_al(void)
{
  uint res = OPER_AL_32();
  uint* r_dst = &DX;

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_pcdi(void)
{
  uint res = OPER_PCDI_32();
  uint* r_dst = &DX;

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_pcix(void)
{
  uint res = OPER_PCIX_32();
  uint* r_dst = &DX;

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_d_i(void)
{
  uint res = OPER_I_32();
  uint* r_dst = &DX;

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_move_32_ai_d(void)
{
  uint res = DY;
  uint ea = EA_AX_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ai_a(void)
{
  uint res = AY;
  uint ea = EA_AX_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ai_ai(void)
{
  uint res = OPER_AY_AI_32();
  uint ea = EA_AX_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ai_pi(void)
{
  uint res = OPER_AY_PI_32();
  uint ea = EA_AX_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ai_pd(void)
{
  uint res = OPER_AY_PD_32();
  uint ea = EA_AX_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ai_di(void)
{
  uint res = OPER_AY_DI_32();
  uint ea = EA_AX_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ai_ix(void)
{
  uint res = OPER_AY_IX_32();
  uint ea = EA_AX_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ai_aw(void)
{
  uint res = OPER_AW_32();
  uint ea = EA_AX_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ai_al(void)
{
  uint res = OPER_AL_32();
  uint ea = EA_AX_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ai_pcdi(void)
{
  uint res = OPER_PCDI_32();
  uint ea = EA_AX_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ai_pcix(void)
{
  uint res = OPER_PCIX_32();
  uint ea = EA_AX_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ai_i(void)
{
  uint res = OPER_I_32();
  uint ea = EA_AX_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pi_d(void)
{
  uint res = DY;
  uint ea = EA_AX_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pi_a(void)
{
  uint res = AY;
  uint ea = EA_AX_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pi_ai(void)
{
  uint res = OPER_AY_AI_32();
  uint ea = EA_AX_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pi_pi(void)
{
  uint res = OPER_AY_PI_32();
  uint ea = EA_AX_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pi_pd(void)
{
  uint res = OPER_AY_PD_32();
  uint ea = EA_AX_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pi_di(void)
{
  uint res = OPER_AY_DI_32();
  uint ea = EA_AX_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pi_ix(void)
{
  uint res = OPER_AY_IX_32();
  uint ea = EA_AX_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pi_aw(void)
{
  uint res = OPER_AW_32();
  uint ea = EA_AX_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pi_al(void)
{
  uint res = OPER_AL_32();
  uint ea = EA_AX_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pi_pcdi(void)
{
  uint res = OPER_PCDI_32();
  uint ea = EA_AX_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pi_pcix(void)
{
  uint res = OPER_PCIX_32();
  uint ea = EA_AX_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pi_i(void)
{
  uint res = OPER_I_32();
  uint ea = EA_AX_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_pd_d(void)
{
  uint res = DY;
  uint ea = EA_AX_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea+2, res & 0xFFFF );
  m68ki_write_16(ea, (res >> 16) & 0xFFFF );
}


static void m68k_op_move_32_pd_a(void)
{
  uint res = AY;
  uint ea = EA_AX_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea+2, res & 0xFFFF );
  m68ki_write_16(ea, (res >> 16) & 0xFFFF );
}


static void m68k_op_move_32_pd_ai(void)
{
  uint res = OPER_AY_AI_32();
  uint ea = EA_AX_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea+2, res & 0xFFFF );
  m68ki_write_16(ea, (res >> 16) & 0xFFFF );
}


static void m68k_op_move_32_pd_pi(void)
{
  uint res = OPER_AY_PI_32();
  uint ea = EA_AX_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea+2, res & 0xFFFF );
  m68ki_write_16(ea, (res >> 16) & 0xFFFF );
}


static void m68k_op_move_32_pd_pd(void)
{
  uint res = OPER_AY_PD_32();
  uint ea = EA_AX_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea+2, res & 0xFFFF );
  m68ki_write_16(ea, (res >> 16) & 0xFFFF );
}


static void m68k_op_move_32_pd_di(void)
{
  uint res = OPER_AY_DI_32();
  uint ea = EA_AX_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea+2, res & 0xFFFF );
  m68ki_write_16(ea, (res >> 16) & 0xFFFF );
}


static void m68k_op_move_32_pd_ix(void)
{
  uint res = OPER_AY_IX_32();
  uint ea = EA_AX_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea+2, res & 0xFFFF );
  m68ki_write_16(ea, (res >> 16) & 0xFFFF );
}


static void m68k_op_move_32_pd_aw(void)
{
  uint res = OPER_AW_32();
  uint ea = EA_AX_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea+2, res & 0xFFFF );
  m68ki_write_16(ea, (res >> 16) & 0xFFFF );
}


static void m68k_op_move_32_pd_al(void)
{
  uint res = OPER_AL_32();
  uint ea = EA_AX_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea+2, res & 0xFFFF );
  m68ki_write_16(ea, (res >> 16) & 0xFFFF );
}


static void m68k_op_move_32_pd_pcdi(void)
{
  uint res = OPER_PCDI_32();
  uint ea = EA_AX_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea+2, res & 0xFFFF );
  m68ki_write_16(ea, (res >> 16) & 0xFFFF );
}


static void m68k_op_move_32_pd_pcix(void)
{
  uint res = OPER_PCIX_32();
  uint ea = EA_AX_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea+2, res & 0xFFFF );
  m68ki_write_16(ea, (res >> 16) & 0xFFFF );
}


static void m68k_op_move_32_pd_i(void)
{
  uint res = OPER_I_32();
  uint ea = EA_AX_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_16(ea+2, res & 0xFFFF );
  m68ki_write_16(ea, (res >> 16) & 0xFFFF );
}


static void m68k_op_move_32_di_d(void)
{
  uint res = DY;
  uint ea = EA_AX_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_di_a(void)
{
  uint res = AY;
  uint ea = EA_AX_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_di_ai(void)
{
  uint res = OPER_AY_AI_32();
  uint ea = EA_AX_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_di_pi(void)
{
  uint res = OPER_AY_PI_32();
  uint ea = EA_AX_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_di_pd(void)
{
  uint res = OPER_AY_PD_32();
  uint ea = EA_AX_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_di_di(void)
{
  uint res = OPER_AY_DI_32();
  uint ea = EA_AX_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_di_ix(void)
{
  uint res = OPER_AY_IX_32();
  uint ea = EA_AX_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_di_aw(void)
{
  uint res = OPER_AW_32();
  uint ea = EA_AX_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_di_al(void)
{
  uint res = OPER_AL_32();
  uint ea = EA_AX_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_di_pcdi(void)
{
  uint res = OPER_PCDI_32();
  uint ea = EA_AX_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_di_pcix(void)
{
  uint res = OPER_PCIX_32();
  uint ea = EA_AX_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_di_i(void)
{
  uint res = OPER_I_32();
  uint ea = EA_AX_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ix_d(void)
{
  uint res = DY;
  uint ea = EA_AX_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ix_a(void)
{
  uint res = AY;
  uint ea = EA_AX_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ix_ai(void)
{
  uint res = OPER_AY_AI_32();
  uint ea = EA_AX_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ix_pi(void)
{
  uint res = OPER_AY_PI_32();
  uint ea = EA_AX_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ix_pd(void)
{
  uint res = OPER_AY_PD_32();
  uint ea = EA_AX_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ix_di(void)
{
  uint res = OPER_AY_DI_32();
  uint ea = EA_AX_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ix_ix(void)
{
  uint res = OPER_AY_IX_32();
  uint ea = EA_AX_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ix_aw(void)
{
  uint res = OPER_AW_32();
  uint ea = EA_AX_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ix_al(void)
{
  uint res = OPER_AL_32();
  uint ea = EA_AX_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ix_pcdi(void)
{
  uint res = OPER_PCDI_32();
  uint ea = EA_AX_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ix_pcix(void)
{
  uint res = OPER_PCIX_32();
  uint ea = EA_AX_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_ix_i(void)
{
  uint res = OPER_I_32();
  uint ea = EA_AX_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_aw_d(void)
{
  uint res = DY;
  uint ea = EA_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_aw_a(void)
{
  uint res = AY;
  uint ea = EA_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_aw_ai(void)
{
  uint res = OPER_AY_AI_32();
  uint ea = EA_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_aw_pi(void)
{
  uint res = OPER_AY_PI_32();
  uint ea = EA_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_aw_pd(void)
{
  uint res = OPER_AY_PD_32();
  uint ea = EA_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_aw_di(void)
{
  uint res = OPER_AY_DI_32();
  uint ea = EA_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_aw_ix(void)
{
  uint res = OPER_AY_IX_32();
  uint ea = EA_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_aw_aw(void)
{
  uint res = OPER_AW_32();
  uint ea = EA_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_aw_al(void)
{
  uint res = OPER_AL_32();
  uint ea = EA_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_aw_pcdi(void)
{
  uint res = OPER_PCDI_32();
  uint ea = EA_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_aw_pcix(void)
{
  uint res = OPER_PCIX_32();
  uint ea = EA_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_aw_i(void)
{
  uint res = OPER_I_32();
  uint ea = EA_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_al_d(void)
{
  uint res = DY;
  uint ea = EA_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_al_a(void)
{
  uint res = AY;
  uint ea = EA_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_al_ai(void)
{
  uint res = OPER_AY_AI_32();
  uint ea = EA_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_al_pi(void)
{
  uint res = OPER_AY_PI_32();
  uint ea = EA_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_al_pd(void)
{
  uint res = OPER_AY_PD_32();
  uint ea = EA_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_al_di(void)
{
  uint res = OPER_AY_DI_32();
  uint ea = EA_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_al_ix(void)
{
  uint res = OPER_AY_IX_32();
  uint ea = EA_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_al_aw(void)
{
  uint res = OPER_AW_32();
  uint ea = EA_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_al_al(void)
{
  uint res = OPER_AL_32();
  uint ea = EA_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_al_pcdi(void)
{
  uint res = OPER_PCDI_32();
  uint ea = EA_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_al_pcix(void)
{
  uint res = OPER_PCIX_32();
  uint ea = EA_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_move_32_al_i(void)
{
  uint res = OPER_I_32();
  uint ea = EA_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  m68ki_write_32(ea, res);
}


static void m68k_op_movea_16_d(void)
{
  AX = MAKE_INT_16(DY);
}


static void m68k_op_movea_16_a(void)
{
  AX = MAKE_INT_16(AY);
}


static void m68k_op_movea_16_ai(void)
{
  AX = MAKE_INT_16(OPER_AY_AI_16());
}


static void m68k_op_movea_16_pi(void)
{
  AX = MAKE_INT_16(OPER_AY_PI_16());
}


static void m68k_op_movea_16_pd(void)
{
  AX = MAKE_INT_16(OPER_AY_PD_16());
}


static void m68k_op_movea_16_di(void)
{
  AX = MAKE_INT_16(OPER_AY_DI_16());
}


static void m68k_op_movea_16_ix(void)
{
  AX = MAKE_INT_16(OPER_AY_IX_16());
}


static void m68k_op_movea_16_aw(void)
{
  AX = MAKE_INT_16(OPER_AW_16());
}


static void m68k_op_movea_16_al(void)
{
  AX = MAKE_INT_16(OPER_AL_16());
}


static void m68k_op_movea_16_pcdi(void)
{
  AX = MAKE_INT_16(OPER_PCDI_16());
}


static void m68k_op_movea_16_pcix(void)
{
  AX = MAKE_INT_16(OPER_PCIX_16());
}


static void m68k_op_movea_16_i(void)
{
  AX = MAKE_INT_16(OPER_I_16());
}


static void m68k_op_movea_32_d(void)
{
  AX = DY;
}


static void m68k_op_movea_32_a(void)
{
  AX = AY;
}


static void m68k_op_movea_32_ai(void)
{
  AX = OPER_AY_AI_32();
}


static void m68k_op_movea_32_pi(void)
{
  AX = OPER_AY_PI_32();
}


static void m68k_op_movea_32_pd(void)
{
  AX = OPER_AY_PD_32();
}


static void m68k_op_movea_32_di(void)
{
  AX = OPER_AY_DI_32();
}


static void m68k_op_movea_32_ix(void)
{
  AX = OPER_AY_IX_32();
}


static void m68k_op_movea_32_aw(void)
{
  AX = OPER_AW_32();
}


static void m68k_op_movea_32_al(void)
{
  AX = OPER_AL_32();
}


static void m68k_op_movea_32_pcdi(void)
{
  AX = OPER_PCDI_32();
}


static void m68k_op_movea_32_pcix(void)
{
  AX = OPER_PCIX_32();
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_movea_32_i(void)
{
  AX = OPER_I_32();
}

static void m68k_op_move_16_toc_d(void)
{
  m68ki_set_ccr(DY);
}


static void m68k_op_move_16_toc_ai(void)
{
  m68ki_set_ccr(OPER_AY_AI_16());
}


static void m68k_op_move_16_toc_pi(void)
{
  m68ki_set_ccr(OPER_AY_PI_16());
}


static void m68k_op_move_16_toc_pd(void)
{
  m68ki_set_ccr(OPER_AY_PD_16());
}


static void m68k_op_move_16_toc_di(void)
{
  m68ki_set_ccr(OPER_AY_DI_16());
}


static void m68k_op_move_16_toc_ix(void)
{
  m68ki_set_ccr(OPER_AY_IX_16());
}


static void m68k_op_move_16_toc_aw(void)
{
  m68ki_set_ccr(OPER_AW_16());
}


static void m68k_op_move_16_toc_al(void)
{
  m68ki_set_ccr(OPER_AL_16());
}


static void m68k_op_move_16_toc_pcdi(void)
{
  m68ki_set_ccr(OPER_PCDI_16());
}


static void m68k_op_move_16_toc_pcix(void)
{
  m68ki_set_ccr(OPER_PCIX_16());
}


static void m68k_op_move_16_toc_i(void)
{
  m68ki_set_ccr(OPER_I_16());
}


static void m68k_op_move_16_frs_d(void)
{
  DY = MASK_OUT_BELOW_16(DY) | m68ki_get_sr();
}


static void m68k_op_move_16_frs_ai(void)
{
  uint ea = EA_AY_AI_16();
  m68ki_write_16(ea, m68ki_get_sr());
}


static void m68k_op_move_16_frs_pi(void)
{
  uint ea = EA_AY_PI_16();
  m68ki_write_16(ea, m68ki_get_sr());
}


static void m68k_op_move_16_frs_pd(void)
{
  uint ea = EA_AY_PD_16();
  m68ki_write_16(ea, m68ki_get_sr());
}


static void m68k_op_move_16_frs_di(void)
{
  uint ea = EA_AY_DI_16();
  m68ki_write_16(ea, m68ki_get_sr());
}


static void m68k_op_move_16_frs_ix(void)
{
  uint ea = EA_AY_IX_16();
  m68ki_write_16(ea, m68ki_get_sr());
}


static void m68k_op_move_16_frs_aw(void)
{
  uint ea = EA_AW_16();
  m68ki_write_16(ea, m68ki_get_sr());
}


static void m68k_op_move_16_frs_al(void)
{
  uint ea = EA_AL_16();
  m68ki_write_16(ea, m68ki_get_sr());
}


static void m68k_op_move_16_tos_d(void)
{
  if(FLAG_S)
  {
    m68ki_set_sr(DY);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_tos_ai(void)
{
  if(FLAG_S)
  {
    uint new_sr = OPER_AY_AI_16();
    m68ki_set_sr(new_sr);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_tos_pi(void)
{
  if(FLAG_S)
  {
    uint new_sr = OPER_AY_PI_16();
    m68ki_set_sr(new_sr);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_tos_pd(void)
{
  if(FLAG_S)
  {
    uint new_sr = OPER_AY_PD_16();
    m68ki_set_sr(new_sr);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_tos_di(void)
{
  if(FLAG_S)
  {
    uint new_sr = OPER_AY_DI_16();
    m68ki_set_sr(new_sr);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_tos_ix(void)
{
  if(FLAG_S)
  {
    uint new_sr = OPER_AY_IX_16();
    m68ki_set_sr(new_sr);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_tos_aw(void)
{
  if(FLAG_S)
  {
    uint new_sr = OPER_AW_16();
    m68ki_set_sr(new_sr);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_tos_al(void)
{
  if(FLAG_S)
  {
    uint new_sr = OPER_AL_16();
    m68ki_set_sr(new_sr);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_tos_pcdi(void)
{
  if(FLAG_S)
  {
    uint new_sr = OPER_PCDI_16();
    m68ki_set_sr(new_sr);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_tos_pcix(void)
{
  if(FLAG_S)
  {
    uint new_sr = OPER_PCIX_16();
    m68ki_set_sr(new_sr);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_move_16_tos_i(void)
{
  if(FLAG_S)
  {
    uint new_sr = OPER_I_16();
    m68ki_set_sr(new_sr);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_move_32_fru(void)
{
  if(FLAG_S)
  {
    AY = REG_USP;
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_move_32_tou(void)
{
  if(FLAG_S)
  {
    REG_USP = AY;
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_movem_16_re_pd(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = AY;
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      ea -= 2;
      m68ki_write_16(ea, MASK_OUT_ABOVE_16(REG_DA[15-i]));
      count++;
    }
  AY = ea;

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_16_re_ai(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AY_AI_16();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      m68ki_write_16(ea, MASK_OUT_ABOVE_16(REG_DA[i]));
      ea += 2;
      count++;
    }

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_16_re_di(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AY_DI_16();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      m68ki_write_16(ea, MASK_OUT_ABOVE_16(REG_DA[i]));
      ea += 2;
      count++;
    }

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_16_re_ix(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AY_IX_16();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      m68ki_write_16(ea, MASK_OUT_ABOVE_16(REG_DA[i]));
      ea += 2;
      count++;
    }

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_16_re_aw(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AW_16();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      m68ki_write_16(ea, MASK_OUT_ABOVE_16(REG_DA[i]));
      ea += 2;
      count++;
    }

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_16_re_al(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AL_16();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      m68ki_write_16(ea, MASK_OUT_ABOVE_16(REG_DA[i]));
      ea += 2;
      count++;
    }

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_32_re_pd(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = AY;
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      ea -= 4;
      m68ki_write_16(ea+2, REG_DA[15-i] & 0xFFFF );
      m68ki_write_16(ea, (REG_DA[15-i] >> 16) & 0xFFFF );
      count++;
    }
  AY = ea;

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_32_re_ai(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AY_AI_32();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      m68ki_write_32(ea, REG_DA[i]);
      ea += 4;
      count++;
    }

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_32_re_di(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AY_DI_32();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      m68ki_write_32(ea, REG_DA[i]);
      ea += 4;
      count++;
    }

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_32_re_ix(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AY_IX_32();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      m68ki_write_32(ea, REG_DA[i]);
      ea += 4;
      count++;
    }

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_32_re_aw(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AW_32();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      m68ki_write_32(ea, REG_DA[i]);
      ea += 4;
      count++;
    }

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_32_re_al(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AL_32();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      m68ki_write_32(ea, REG_DA[i]);
      ea += 4;
      count++;
    }

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_16_er_pi(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = AY;
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_16(ea)));
      ea += 2;
      count++;
    }
  AY = ea;

  /* MOVEM extra read cycle (can have side effect if target hardware is impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_pcdi(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_PCDI_16();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_pcrel_16(ea)));
      ea += 2;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if target hardware is impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_pcix(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_PCIX_16();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_pcrel_16(ea)));
      ea += 2;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_ai(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AY_AI_16();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_16(ea)));
      ea += 2;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_di(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AY_DI_16();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_16(ea)));
      ea += 2;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_ix(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AY_IX_16();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_16(ea)));
      ea += 2;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_aw(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AW_16();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_16(ea)));
      ea += 2;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_16_er_al(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AL_16();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = MAKE_INT_16(MASK_OUT_ABOVE_16(m68ki_read_16(ea)));
      ea += 2;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_W);
}


static void m68k_op_movem_32_er_pi(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = AY;
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = m68ki_read_32(ea);
      ea += 4;
      count++;
    }
  AY = ea;

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_32_er_pcdi(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_PCDI_32();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = m68ki_read_pcrel_32(ea);
      ea += 4;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_32_er_pcix(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_PCIX_32();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = m68ki_read_pcrel_32(ea);
      ea += 4;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_32_er_ai(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AY_AI_32();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = m68ki_read_32(ea);
      ea += 4;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_32_er_di(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AY_DI_32();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = m68ki_read_32(ea);
      ea += 4;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_32_er_ix(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AY_IX_32();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = m68ki_read_32(ea);
      ea += 4;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_32_er_aw(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AW_32();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = m68ki_read_32(ea);
      ea += 4;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movem_32_er_al(void)
{
  uint i = 0;
  uint register_list = OPER_I_16();
  uint ea = EA_AL_32();
  uint count = 0;

  for(; i < 16; i++)
    if(register_list & (1 << i))
    {
      REG_DA[i] = m68ki_read_32(ea);
      ea += 4;
      count++;
    }

  /* MOVEM extra read cycle (can have side effect if extra address is not mapped or mapped to hardware impacted by read access) */
  m68ki_read_16(ea);

  USE_CYCLES(count * CYC_MOVEM_L);
  SKIP_BUS_REFRESH();
}


static void m68k_op_movep_16_re(void)
{
  uint ea = EA_AY_DI_16();
  uint src = DX;

  m68ki_write_8(ea, MASK_OUT_ABOVE_8(src >> 8));
  m68ki_write_8(ea += 2, MASK_OUT_ABOVE_8(src));
}


static void m68k_op_movep_32_re(void)
{
  uint ea = EA_AY_DI_32();
  uint src = DX;

  m68ki_write_8(ea, MASK_OUT_ABOVE_8(src >> 24));
  m68ki_write_8(ea += 2, MASK_OUT_ABOVE_8(src >> 16));
  m68ki_write_8(ea += 2, MASK_OUT_ABOVE_8(src >> 8));
  m68ki_write_8(ea += 2, MASK_OUT_ABOVE_8(src));
}


static void m68k_op_movep_16_er(void)
{
  uint ea = EA_AY_DI_16();
  uint* r_dst = &DX;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | ((m68ki_read_8(ea) << 8) + m68ki_read_8(ea + 2));
}


static void m68k_op_movep_32_er(void)
{
  uint ea = EA_AY_DI_32();

  DX = (m68ki_read_8(ea) << 24) + (m68ki_read_8(ea + 2) << 16)
    + (m68ki_read_8(ea + 4) << 8) + m68ki_read_8(ea + 6);
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_moveq_32(void)
{
  uint res = DX = MAKE_INT_8(MASK_OUT_ABOVE_8(REG_IR));

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_d(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(DY);
  uint res = MASK_OUT_ABOVE_32( src * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

  UseMulsCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_ai(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AY_AI_16());
  uint res = MASK_OUT_ABOVE_32( src * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

  UseMulsCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_pi(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AY_PI_16());
  uint res = MASK_OUT_ABOVE_32( src * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

  UseMulsCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_pd(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AY_PD_16());
  uint res = MASK_OUT_ABOVE_32( src * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

  UseMulsCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_di(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AY_DI_16());
  uint res = MASK_OUT_ABOVE_32( src * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

  UseMulsCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_ix(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AY_IX_16());
  uint res = MASK_OUT_ABOVE_32( src * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

  UseMulsCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_aw(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AW_16());
  uint res = MASK_OUT_ABOVE_32( src * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

  UseMulsCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_al(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_AL_16());
  uint res = MASK_OUT_ABOVE_32( src * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

  UseMulsCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_pcdi(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_PCDI_16());
  uint res = MASK_OUT_ABOVE_32( src * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

  UseMulsCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_pcix(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_PCIX_16());
  uint res = MASK_OUT_ABOVE_32( src * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

  UseMulsCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_muls_16_i(void)
{
  uint* r_dst = &DX;
  sint src = MAKE_INT_16(OPER_I_16());
  uint res = MASK_OUT_ABOVE_32( src * MAKE_INT_16(MASK_OUT_ABOVE_16(*r_dst)));

  UseMulsCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_d(void)
{
  uint* r_dst = &DX;
  uint src = MASK_OUT_ABOVE_16(DY);
  uint res = src * MASK_OUT_ABOVE_16(*r_dst);

  UseMuluCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_ai(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_AI_16();
  uint res = src * MASK_OUT_ABOVE_16(*r_dst);

  UseMuluCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_pi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PI_16();
  uint res = src * MASK_OUT_ABOVE_16(*r_dst);

  UseMuluCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_pd(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PD_16();
  uint res = src * MASK_OUT_ABOVE_16(*r_dst);

  UseMuluCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_di(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_DI_16();
  uint res = src * MASK_OUT_ABOVE_16(*r_dst);

  UseMuluCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_ix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_IX_16();
  uint res = src * MASK_OUT_ABOVE_16(*r_dst);

  UseMuluCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_aw(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AW_16();
  uint res = src * MASK_OUT_ABOVE_16(*r_dst);

  UseMuluCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_al(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AL_16();
  uint res = src * MASK_OUT_ABOVE_16(*r_dst);

  UseMuluCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_pcdi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCDI_16();
  uint res = src * MASK_OUT_ABOVE_16(*r_dst);

  UseMuluCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_pcix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCIX_16();
  uint res = src * MASK_OUT_ABOVE_16(*r_dst);

  UseMuluCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_mulu_16_i(void)
{
  uint* r_dst = &DX;
  uint src = OPER_I_16();
  uint res = src * MASK_OUT_ABOVE_16(*r_dst);

  UseMuluCycles(src);

  *r_dst = res;

  FLAG_Z = res;
  FLAG_N = NFLAG_32(res);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_nbcd_8_d(void)
{
  uint* r_dst = &DY;
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = -dst - XFLAG_AS_1();

  if(res)
  {
    FLAG_V = res; /* Undefined V behavior */

    if(((res|dst) & 0x0f) == 0x0)
      res = (res & 0xf0) + 6;

    res = MASK_OUT_ABOVE_8(res+0x9a);

    FLAG_V &= ~res; /* Undefined V behavior part II */

    *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

    FLAG_Z |= res;
    FLAG_C = CFLAG_SET;
    FLAG_X = XFLAG_SET;
    FLAG_N = NFLAG_8(res);  /* Undefined N behavior */
  }
  else
  {
    FLAG_V = VFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
  }
}


static void m68k_op_nbcd_8_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint dst = m68ki_read_8(ea);
  uint res = -dst - XFLAG_AS_1();

  if(res)
  {
    FLAG_V = res; /* Undefined V behavior */

    if(((res|dst) & 0x0f) == 0x0)
      res = (res & 0xf0) + 6;

    res = MASK_OUT_ABOVE_8(res+0x9a);

    FLAG_V &= ~res; /* Undefined V behavior part II */

    m68ki_write_8(ea, res);

    FLAG_Z |= res;
    FLAG_C = CFLAG_SET;
    FLAG_X = XFLAG_SET;
    FLAG_N = NFLAG_8(res);  /* Undefined N behavior */
  }
  else
  {
    FLAG_V = VFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
  }
}


static void m68k_op_nbcd_8_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint dst = m68ki_read_8(ea);
  uint res = -dst - XFLAG_AS_1();

  if(res)
  {
    FLAG_V = res; /* Undefined V behavior */

    if(((res|dst) & 0x0f) == 0x0)
      res = (res & 0xf0) + 6;

    res = MASK_OUT_ABOVE_8(res+0x9a);

    FLAG_V &= ~res; /* Undefined V behavior part II */

    m68ki_write_8(ea, res);

    FLAG_Z |= res;
    FLAG_C = CFLAG_SET;
    FLAG_X = XFLAG_SET;
    FLAG_N = NFLAG_8(res);  /* Undefined N behavior */
  }
  else
  {
    FLAG_V = VFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
  }
}


static void m68k_op_nbcd_8_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint dst = m68ki_read_8(ea);
  uint res = -dst - XFLAG_AS_1();

  if(res)
  {
    FLAG_V = res; /* Undefined V behavior */

    if(((res|dst) & 0x0f) == 0x0)
      res = (res & 0xf0) + 6;

    res = MASK_OUT_ABOVE_8(res+0x9a);

    FLAG_V &= ~res; /* Undefined V behavior part II */

    m68ki_write_8(ea, res);

    FLAG_Z |= res;
    FLAG_C = CFLAG_SET;
    FLAG_X = XFLAG_SET;
    FLAG_N = NFLAG_8(res);  /* Undefined N behavior */
  }
  else
  {
    FLAG_V = VFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
  }
}


static void m68k_op_nbcd_8_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = -dst - XFLAG_AS_1();

  if(res)
  {
    FLAG_V = res; /* Undefined V behavior */

    if(((res|dst) & 0x0f) == 0x0)
      res = (res & 0xf0) + 6;

    res = MASK_OUT_ABOVE_8(res+0x9a);

    FLAG_V &= ~res; /* Undefined V behavior part II */

    m68ki_write_8(ea, res);

    FLAG_Z |= res;
    FLAG_C = CFLAG_SET;
    FLAG_X = XFLAG_SET;
    FLAG_N = NFLAG_8(res);  /* Undefined N behavior */
  }
  else
  {
    FLAG_V = VFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
  }
}


static void m68k_op_nbcd_8_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = -dst - XFLAG_AS_1();

  if(res)
  {
    FLAG_V = res; /* Undefined V behavior */

    if(((res|dst) & 0x0f) == 0x0)
      res = (res & 0xf0) + 6;

    res = MASK_OUT_ABOVE_8(res+0x9a);

    FLAG_V &= ~res; /* Undefined V behavior part II */

    m68ki_write_8(ea, res);

    FLAG_Z |= res;
    FLAG_C = CFLAG_SET;
    FLAG_X = XFLAG_SET;
    FLAG_N = NFLAG_8(res);  /* Undefined N behavior */
  }
  else
  {
    FLAG_V = VFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
  }
}


static void m68k_op_nbcd_8_di(void)
{
  uint ea = EA_AY_DI_8();
  uint dst = m68ki_read_8(ea);
  uint res = -dst - XFLAG_AS_1();

  if(res)
  {
    FLAG_V = res; /* Undefined V behavior */

    if(((res|dst) & 0x0f) == 0x0)
      res = (res & 0xf0) + 6;

    res = MASK_OUT_ABOVE_8(res+0x9a);

    FLAG_V &= ~res; /* Undefined V behavior part II */

    m68ki_write_8(ea, res);

    FLAG_Z |= res;
    FLAG_C = CFLAG_SET;
    FLAG_X = XFLAG_SET;
    FLAG_N = NFLAG_8(res);  /* Undefined N behavior */
  }
  else
  {
    FLAG_V = VFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
  }
}


static void m68k_op_nbcd_8_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint dst = m68ki_read_8(ea);
  uint res = -dst - XFLAG_AS_1();

  if(res)
  {
    FLAG_V = res; /* Undefined V behavior */

    if(((res|dst) & 0x0f) == 0x0)
      res = (res & 0xf0) + 6;

    res = MASK_OUT_ABOVE_8(res+0x9a);

    FLAG_V &= ~res; /* Undefined V behavior part II */

    m68ki_write_8(ea, res);

    FLAG_Z |= res;
    FLAG_C = CFLAG_SET;
    FLAG_X = XFLAG_SET;
    FLAG_N = NFLAG_8(res);  /* Undefined N behavior */
  }
  else
  {
    FLAG_V = VFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
  }
}


static void m68k_op_nbcd_8_aw(void)
{
  uint ea = EA_AW_8();
  uint dst = m68ki_read_8(ea);
  uint res = -dst - XFLAG_AS_1();

  if(res)
  {
    FLAG_V = res; /* Undefined V behavior */

    if(((res|dst) & 0x0f) == 0x0)
      res = (res & 0xf0) + 6;

    res = MASK_OUT_ABOVE_8(res+0x9a);

    FLAG_V &= ~res; /* Undefined V behavior part II */

    m68ki_write_8(ea, res);

    FLAG_Z |= res;
    FLAG_C = CFLAG_SET;
    FLAG_X = XFLAG_SET;
    FLAG_N = NFLAG_8(res);  /* Undefined N behavior */
  }
  else
  {
    FLAG_V = VFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
  }
}


static void m68k_op_nbcd_8_al(void)
{
  uint ea = EA_AL_8();
  uint dst = m68ki_read_8(ea);
  uint res = -dst - XFLAG_AS_1();

  if(res)
  {
    FLAG_V = res; /* Undefined V behavior */

    if(((res|dst) & 0x0f) == 0x0)
      res = (res & 0xf0) + 6;

    res = MASK_OUT_ABOVE_8(res+0x9a);

    FLAG_V &= ~res; /* Undefined V behavior part II */

    m68ki_write_8(ea, res);

    FLAG_Z |= res;
    FLAG_C = CFLAG_SET;
    FLAG_X = XFLAG_SET;
    FLAG_N = NFLAG_8(res);  /* Undefined N behavior */
  }
  else
  {
    FLAG_V = VFLAG_CLEAR;
    FLAG_C = CFLAG_CLEAR;
    FLAG_X = XFLAG_CLEAR;
    FLAG_N = NFLAG_CLEAR;
  }
}


static void m68k_op_neg_8_d(void)
{
  uint* r_dst = &DY;
  uint res = 0 - MASK_OUT_ABOVE_8(*r_dst);

  FLAG_N = NFLAG_8(res);
  FLAG_C = FLAG_X = CFLAG_8(res);
  FLAG_V = *r_dst & res;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_neg_8_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_8(res);
  FLAG_C = FLAG_X = CFLAG_8(res);
  FLAG_V = src & res;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_8(res);
  FLAG_C = FLAG_X = CFLAG_8(res);
  FLAG_V = src & res;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_8(res);
  FLAG_C = FLAG_X = CFLAG_8(res);
  FLAG_V = src & res;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_8(res);
  FLAG_C = FLAG_X = CFLAG_8(res);
  FLAG_V = src & res;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_8(res);
  FLAG_C = FLAG_X = CFLAG_8(res);
  FLAG_V = src & res;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_di(void)
{
  uint ea = EA_AY_DI_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_8(res);
  FLAG_C = FLAG_X = CFLAG_8(res);
  FLAG_V = src & res;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_8(res);
  FLAG_C = FLAG_X = CFLAG_8(res);
  FLAG_V = src & res;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_aw(void)
{
  uint ea = EA_AW_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_8(res);
  FLAG_C = FLAG_X = CFLAG_8(res);
  FLAG_V = src & res;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_8_al(void)
{
  uint ea = EA_AL_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_8(res);
  FLAG_C = FLAG_X = CFLAG_8(res);
  FLAG_V = src & res;
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_neg_16_d(void)
{
  uint* r_dst = &DY;
  uint res = 0 - MASK_OUT_ABOVE_16(*r_dst);

  FLAG_N = NFLAG_16(res);
  FLAG_C = FLAG_X = CFLAG_16(res);
  FLAG_V = (*r_dst & res)>>8;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_neg_16_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_16(res);
  FLAG_C = FLAG_X = CFLAG_16(res);
  FLAG_V = (src & res)>>8;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_16_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_16(res);
  FLAG_C = FLAG_X = CFLAG_16(res);
  FLAG_V = (src & res)>>8;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_16_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_16(res);
  FLAG_C = FLAG_X = CFLAG_16(res);
  FLAG_V = (src & res)>>8;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_16_di(void)
{
  uint ea = EA_AY_DI_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_16(res);
  FLAG_C = FLAG_X = CFLAG_16(res);
  FLAG_V = (src & res)>>8;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_16_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_16(res);
  FLAG_C = FLAG_X = CFLAG_16(res);
  FLAG_V = (src & res)>>8;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_16_aw(void)
{
  uint ea = EA_AW_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_16(res);
  FLAG_C = FLAG_X = CFLAG_16(res);
  FLAG_V = (src & res)>>8;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_16_al(void)
{
  uint ea = EA_AL_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_16(res);
  FLAG_C = FLAG_X = CFLAG_16(res);
  FLAG_V = (src & res)>>8;
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_neg_32_d(void)
{
  uint* r_dst = &DY;
  uint res = 0 - *r_dst;

  FLAG_N = NFLAG_32(res);
  FLAG_C = FLAG_X = CFLAG_SUB_32(*r_dst, 0, res);
  FLAG_V = (*r_dst & res)>>24;
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_neg_32_ai(void)
{
  uint ea = EA_AY_AI_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_32(res);
  FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_neg_32_pi(void)
{
  uint ea = EA_AY_PI_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_32(res);
  FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_neg_32_pd(void)
{
  uint ea = EA_AY_PD_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_32(res);
  FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_neg_32_di(void)
{
  uint ea = EA_AY_DI_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_32(res);
  FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_neg_32_ix(void)
{
  uint ea = EA_AY_IX_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_32(res);
  FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_neg_32_aw(void)
{
  uint ea = EA_AW_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_32(res);
  FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_neg_32_al(void)
{
  uint ea = EA_AL_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - src;

  FLAG_N = NFLAG_32(res);
  FLAG_C = FLAG_X = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_negx_8_d(void)
{
  uint* r_dst = &DY;
  uint res = 0 - MASK_OUT_ABOVE_8(*r_dst) - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = *r_dst & res;

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
}


static void m68k_op_negx_8_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = src & res;

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = src & res;

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = src & res;

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = src & res;

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = src & res;

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_di(void)
{
  uint ea = EA_AY_DI_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = src & res;

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = src & res;

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_aw(void)
{
  uint ea = EA_AW_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = src & res;

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_negx_8_al(void)
{
  uint ea = EA_AL_8();
  uint src = m68ki_read_8(ea);
  uint res = 0 - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = src & res;

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_negx_16_d(void)
{
  uint* r_dst = &DY;
  uint res = 0 - MASK_OUT_ABOVE_16(*r_dst) - XFLAG_AS_1();

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = (*r_dst & res)>>8;

  res = MASK_OUT_ABOVE_16(res);
  FLAG_Z |= res;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
}


static void m68k_op_negx_16_ai(void)
{
  uint ea  = EA_AY_AI_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = (src & res)>>8;

  res = MASK_OUT_ABOVE_16(res);
  FLAG_Z |= res;

  m68ki_write_16(ea, res);
}


static void m68k_op_negx_16_pi(void)
{
  uint ea  = EA_AY_PI_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = (src & res)>>8;

  res = MASK_OUT_ABOVE_16(res);
  FLAG_Z |= res;

  m68ki_write_16(ea, res);
}


static void m68k_op_negx_16_pd(void)
{
  uint ea  = EA_AY_PD_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = (src & res)>>8;

  res = MASK_OUT_ABOVE_16(res);
  FLAG_Z |= res;

  m68ki_write_16(ea, res);
}


static void m68k_op_negx_16_di(void)
{
  uint ea  = EA_AY_DI_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = (src & res)>>8;

  res = MASK_OUT_ABOVE_16(res);
  FLAG_Z |= res;

  m68ki_write_16(ea, res);
}


static void m68k_op_negx_16_ix(void)
{
  uint ea  = EA_AY_IX_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = (src & res)>>8;

  res = MASK_OUT_ABOVE_16(res);
  FLAG_Z |= res;

  m68ki_write_16(ea, res);
}


static void m68k_op_negx_16_aw(void)
{
  uint ea  = EA_AW_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = (src & res)>>8;

  res = MASK_OUT_ABOVE_16(res);
  FLAG_Z |= res;

  m68ki_write_16(ea, res);
}


static void m68k_op_negx_16_al(void)
{
  uint ea  = EA_AL_16();
  uint src = m68ki_read_16(ea);
  uint res = 0 - MASK_OUT_ABOVE_16(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = (src & res)>>8;

  res = MASK_OUT_ABOVE_16(res);
  FLAG_Z |= res;

  m68ki_write_16(ea, res);
}


static void m68k_op_negx_32_d(void)
{
  uint* r_dst = &DY;
  uint res = 0 - MASK_OUT_ABOVE_32(*r_dst) - XFLAG_AS_1();

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(*r_dst, 0, res);
  FLAG_V = (*r_dst & res)>>24;

  res = MASK_OUT_ABOVE_32(res);
  FLAG_Z |= res;

  *r_dst = res;
}


static void m68k_op_negx_32_ai(void)
{
  uint ea  = EA_AY_AI_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;

  res = MASK_OUT_ABOVE_32(res);
  FLAG_Z |= res;

  m68ki_write_32(ea, res);
}


static void m68k_op_negx_32_pi(void)
{
  uint ea  = EA_AY_PI_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;

  res = MASK_OUT_ABOVE_32(res);
  FLAG_Z |= res;

  m68ki_write_32(ea, res);
}


static void m68k_op_negx_32_pd(void)
{
  uint ea  = EA_AY_PD_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;

  res = MASK_OUT_ABOVE_32(res);
  FLAG_Z |= res;

  m68ki_write_32(ea, res);
}


static void m68k_op_negx_32_di(void)
{
  uint ea  = EA_AY_DI_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;

  res = MASK_OUT_ABOVE_32(res);
  FLAG_Z |= res;

  m68ki_write_32(ea, res);
}


static void m68k_op_negx_32_ix(void)
{
  uint ea  = EA_AY_IX_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;

  res = MASK_OUT_ABOVE_32(res);
  FLAG_Z |= res;

  m68ki_write_32(ea, res);
}


static void m68k_op_negx_32_aw(void)
{
  uint ea  = EA_AW_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;

  res = MASK_OUT_ABOVE_32(res);
  FLAG_Z |= res;

  m68ki_write_32(ea, res);
}


static void m68k_op_negx_32_al(void)
{
  uint ea  = EA_AL_32();
  uint src = m68ki_read_32(ea);
  uint res = 0 - MASK_OUT_ABOVE_32(src) - XFLAG_AS_1();

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, 0, res);
  FLAG_V = (src & res)>>24;

  res = MASK_OUT_ABOVE_32(res);
  FLAG_Z |= res;

  m68ki_write_32(ea, res);
}


static void m68k_op_nop(void)
{
}


static void m68k_op_not_8_d(void)
{
  uint* r_dst = &DY;
  uint res = MASK_OUT_ABOVE_8(~*r_dst);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_di(void)
{
  uint ea = EA_AY_DI_8();
  uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_aw(void)
{
  uint ea = EA_AW_8();
  uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_8_al(void)
{
  uint ea = EA_AL_8();
  uint res = MASK_OUT_ABOVE_8(~m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_d(void)
{
  uint* r_dst = &DY;
  uint res = MASK_OUT_ABOVE_16(~*r_dst);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_di(void)
{
  uint ea = EA_AY_DI_16();
  uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_aw(void)
{
  uint ea = EA_AW_16();
  uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_16_al(void)
{
  uint ea = EA_AL_16();
  uint res = MASK_OUT_ABOVE_16(~m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_d(void)
{
  uint* r_dst = &DY;
  uint res = *r_dst = MASK_OUT_ABOVE_32(~*r_dst);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_ai(void)
{
  uint ea = EA_AY_AI_32();
  uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_pi(void)
{
  uint ea = EA_AY_PI_32();
  uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_pd(void)
{
  uint ea = EA_AY_PD_32();
  uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_di(void)
{
  uint ea = EA_AY_DI_32();
  uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_ix(void)
{
  uint ea = EA_AY_IX_32();
  uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_aw(void)
{
  uint ea = EA_AW_32();
  uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_not_32_al(void)
{
  uint ea = EA_AL_32();
  uint res = MASK_OUT_ABOVE_32(~m68ki_read_32(ea));

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_d(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= MASK_OUT_ABOVE_8(DY)));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_ai(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= OPER_AY_AI_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_pi(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= OPER_AY_PI_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_pi7(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= OPER_A7_PI_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_pd(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= OPER_AY_PD_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_pd7(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= OPER_A7_PD_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_di(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= OPER_AY_DI_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_ix(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= OPER_AY_IX_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_aw(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= OPER_AW_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_al(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= OPER_AL_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_pcdi(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= OPER_PCDI_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_pcix(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= OPER_PCIX_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_er_i(void)
{
  uint res = MASK_OUT_ABOVE_8((DX |= OPER_I_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_d(void)
{
  uint res = MASK_OUT_ABOVE_16((DX |= MASK_OUT_ABOVE_16(DY)));

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_ai(void)
{
  uint res = MASK_OUT_ABOVE_16((DX |= OPER_AY_AI_16()));

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_pi(void)
{
  uint res = MASK_OUT_ABOVE_16((DX |= OPER_AY_PI_16()));

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_pd(void)
{
  uint res = MASK_OUT_ABOVE_16((DX |= OPER_AY_PD_16()));

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_di(void)
{
  uint res = MASK_OUT_ABOVE_16((DX |= OPER_AY_DI_16()));

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_ix(void)
{
  uint res = MASK_OUT_ABOVE_16((DX |= OPER_AY_IX_16()));

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_aw(void)
{
  uint res = MASK_OUT_ABOVE_16((DX |= OPER_AW_16()));

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_al(void)
{
  uint res = MASK_OUT_ABOVE_16((DX |= OPER_AL_16()));

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_pcdi(void)
{
  uint res = MASK_OUT_ABOVE_16((DX |= OPER_PCDI_16()));

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_pcix(void)
{
  uint res = MASK_OUT_ABOVE_16((DX |= OPER_PCIX_16()));

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_er_i(void)
{
  uint res = MASK_OUT_ABOVE_16((DX |= OPER_I_16()));

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_d(void)
{
  uint res = DX |= DY;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_ai(void)
{
  uint res = DX |= OPER_AY_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_pi(void)
{
  uint res = DX |= OPER_AY_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_pd(void)
{
  uint res = DX |= OPER_AY_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_di(void)
{
  uint res = DX |= OPER_AY_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_ix(void)
{
  uint res = DX |= OPER_AY_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_aw(void)
{
  uint res = DX |= OPER_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_al(void)
{
  uint res = DX |= OPER_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_pcdi(void)
{
  uint res = DX |= OPER_PCDI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_pcix(void)
{
  uint res = DX |= OPER_PCIX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_er_i(void)
{
  uint res = DX |= OPER_I_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_di(void)
{
  uint ea = EA_AY_DI_8();
  uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_aw(void)
{
  uint ea = EA_AW_8();
  uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_8_re_al(void)
{
  uint ea = EA_AL_8();
  uint res = MASK_OUT_ABOVE_8(DX | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_di(void)
{
  uint ea = EA_AY_DI_16();
  uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_aw(void)
{
  uint ea = EA_AW_16();
  uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_16_re_al(void)
{
  uint ea = EA_AL_16();
  uint res = MASK_OUT_ABOVE_16(DX | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_ai(void)
{
  uint ea = EA_AY_AI_32();
  uint res = DX | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_pi(void)
{
  uint ea = EA_AY_PI_32();
  uint res = DX | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_pd(void)
{
  uint ea = EA_AY_PD_32();
  uint res = DX | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_di(void)
{
  uint ea = EA_AY_DI_32();
  uint res = DX | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_ix(void)
{
  uint ea = EA_AY_IX_32();
  uint res = DX | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_aw(void)
{
  uint ea = EA_AW_32();
  uint res = DX | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_or_32_re_al(void)
{
  uint ea = EA_AL_32();
  uint res = DX | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_d(void)
{
  uint res = MASK_OUT_ABOVE_8((DY |= OPER_I_8()));

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_ai(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_AI_8();
  uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_pi(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_PI_8();
  uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_pi7(void)
{
  uint src = OPER_I_8();
  uint ea = EA_A7_PI_8();
  uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_pd(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_PD_8();
  uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_pd7(void)
{
  uint src = OPER_I_8();
  uint ea = EA_A7_PD_8();
  uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_di(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_DI_8();
  uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_ix(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_IX_8();
  uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_aw(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AW_8();
  uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_8_al(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AL_8();
  uint res = MASK_OUT_ABOVE_8(src | m68ki_read_8(ea));

  m68ki_write_8(ea, res);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_16_d(void)
{
  uint res = MASK_OUT_ABOVE_16(DY |= OPER_I_16());

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_16_ai(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_AI_16();
  uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_16_pi(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_PI_16();
  uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_16_pd(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_PD_16();
  uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_16_di(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_DI_16();
  uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_16_ix(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_IX_16();
  uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_16_aw(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AW_16();
  uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_16_al(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AL_16();
  uint res = MASK_OUT_ABOVE_16(src | m68ki_read_16(ea));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_32_d(void)
{
  uint res = DY |= OPER_I_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_32_ai(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_AI_32();
  uint res = src | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_32_pi(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_PI_32();
  uint res = src | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_32_pd(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_PD_32();
  uint res = src | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_32_di(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_DI_32();
  uint res = src | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_32_ix(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_IX_32();
  uint res = src | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_32_aw(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AW_32();
  uint res = src | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_32_al(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AL_32();
  uint res = src | m68ki_read_32(ea);

  m68ki_write_32(ea, res);

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ori_16_toc(void)
{
  m68ki_set_ccr(m68ki_get_ccr() | OPER_I_16());
}


static void m68k_op_ori_16_tos(void)
{
  if(FLAG_S)
  {
    uint src = OPER_I_16();
    m68ki_set_sr(m68ki_get_sr() | src);
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_pea_32_ai(void)
{
  uint ea = EA_AY_AI_32();

  m68ki_push_32(ea);
}


static void m68k_op_pea_32_di(void)
{
  uint ea = EA_AY_DI_32();

  m68ki_push_32(ea);
}


static void m68k_op_pea_32_ix(void)
{
  uint ea = EA_AY_IX_32();

  m68ki_push_32(ea);
}


static void m68k_op_pea_32_aw(void)
{
  uint ea = EA_AW_32();

  m68ki_push_32(ea);
}


static void m68k_op_pea_32_al(void)
{
  uint ea = EA_AL_32();

  m68ki_push_32(ea);
}


static void m68k_op_pea_32_pcdi(void)
{
  uint ea = EA_PCDI_32();

  m68ki_push_32(ea);
}


static void m68k_op_pea_32_pcix(void)
{
  uint ea = EA_PCIX_32();

  m68ki_push_32(ea);
}


static void m68k_op_reset(void)
{
  if(FLAG_S)
  {
    m68ki_output_reset()       /* auto-disable (see m68kcpu.h) */
    USE_CYCLES(CYC_RESET);
    m68ki_cpu.refresh_cycles += (128*MUL); /* skip one 68K bus refresh cycle as instruction processing time is longer than refresh period (128 CPU cycles on Mega Drive / Genesis) */
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_ror_8_s(void)
{
  uint* r_dst = &DY;
  uint orig_shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint shift = orig_shift & 7;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = ROR_8(src, shift);

  if(orig_shift != 0)
    USE_CYCLES(orig_shift * CYC_SHIFT);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = src << (9-orig_shift);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = ROR_16(src, shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src << (9-shift);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_32_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint64 src = *r_dst;
  uint res = ROR_32(src, shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = src << (9-shift);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_8_r(void)
{
  uint* r_dst = &DY;
  uint orig_shift = DX & 0x3f;
  uint shift = orig_shift & 7;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = ROR_8(src, shift);

  if(orig_shift != 0)
  {
    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
    FLAG_C = src << (8-((shift-1)&7));
    FLAG_N = NFLAG_8(res);
    FLAG_Z = res;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_8(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_r(void)
{
  uint* r_dst = &DY;
  uint orig_shift = DX & 0x3f;
  uint shift = orig_shift & 15;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = ROR_16(src, shift);

  if(orig_shift != 0)
  {
    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    FLAG_C = (src >> ((shift - 1) & 15)) << 8;
    FLAG_N = NFLAG_16(res);
    FLAG_Z = res;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_16(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_32_r(void)
{
  uint* r_dst = &DY;
  uint orig_shift = DX & 0x3f;
  uint shift = orig_shift & 31;
  uint64 src = *r_dst;
  uint res = ROR_32(src, shift);

  if(orig_shift != 0)
  {
    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    *r_dst = res;
    FLAG_C = (src >> ((shift - 1) & 31)) << 8;
    FLAG_N = NFLAG_32(res);
    FLAG_Z = res;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_32(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_16(src, 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_16(src, 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_16(src, 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_di(void)
{
  uint ea = EA_AY_DI_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_16(src, 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_16(src, 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_aw(void)
{
  uint ea = EA_AW_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_16(src, 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_ror_16_al(void)
{
  uint ea = EA_AL_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_16(src, 1);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src << 8;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_8_s(void)
{
  uint* r_dst = &DY;
  uint orig_shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint shift = orig_shift & 7;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = ROL_8(src, shift);

  if(orig_shift != 0)
    USE_CYCLES(orig_shift * CYC_SHIFT);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_C = src << orig_shift;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = ROL_16(src, shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src >> (8-shift);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_32_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint64 src = *r_dst;
  uint res = ROL_32(src, shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_C = src >> (24-shift);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_8_r(void)
{
  uint* r_dst = &DY;
  uint orig_shift = DX & 0x3f;
  uint shift = orig_shift & 7;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = ROL_8(src, shift);

  if(orig_shift != 0)
  {
    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift != 0)
    {
      *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
      FLAG_C = src << shift;
      FLAG_N = NFLAG_8(res);
      FLAG_Z = res;
      FLAG_V = VFLAG_CLEAR;
      return;
    }
    FLAG_C = (src & 1)<<8;
    FLAG_N = NFLAG_8(src);
    FLAG_Z = src;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_8(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_r(void)
{
  uint* r_dst = &DY;
  uint orig_shift = DX & 0x3f;
  uint shift = orig_shift & 15;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = MASK_OUT_ABOVE_16(ROL_16(src, shift));

  if(orig_shift != 0)
  {
    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    if(shift != 0)
    {
      *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
      FLAG_C = (src << shift) >> 8;
      FLAG_N = NFLAG_16(res);
      FLAG_Z = res;
      FLAG_V = VFLAG_CLEAR;
      return;
    }
    FLAG_C = (src & 1)<<8;
    FLAG_N = NFLAG_16(src);
    FLAG_Z = src;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_16(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_32_r(void)
{
  uint* r_dst = &DY;
  uint orig_shift = DX & 0x3f;
  uint shift = orig_shift & 31;
  uint64 src = *r_dst;
  uint res = ROL_32(src, shift);

  if(orig_shift != 0)
  {
    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    *r_dst = res;

    FLAG_C = (src >> ((32 - shift) & 0x1f)) << 8;
    FLAG_N = NFLAG_32(res);
    FLAG_Z = res;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = CFLAG_CLEAR;
  FLAG_N = NFLAG_32(src);
  FLAG_Z = src;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_di(void)
{
  uint ea = EA_AY_DI_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_aw(void)
{
  uint ea = EA_AW_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rol_16_al(void)
{
  uint ea = EA_AL_16();
  uint src = m68ki_read_16(ea);
  uint res = MASK_OUT_ABOVE_16(ROL_16(src, 1));

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_C = src >> 7;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_8_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = ROR_9(src | (XFLAG_AS_1() << 8), shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  FLAG_C = FLAG_X = res;
  res = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = ROR_17(src | (XFLAG_AS_1() << 16), shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_32_s(void)
{
#if M68K_USE_64_BIT

  uint*  r_dst = &DY;
  uint   shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint64 src   = *r_dst;
  uint64 res   = src | (((uint64)XFLAG_AS_1()) << 32);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  res = ROR_33_64(res, shift);

  FLAG_C = FLAG_X = res >> 24;
  res = MASK_OUT_ABOVE_32(res);

  *r_dst =  res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;

#else

  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = *r_dst;
  uint res = MASK_OUT_ABOVE_32((ROR_33(src, shift) & ~(1 << (32 - shift))) | (XFLAG_AS_1() << (32 - shift)));
  uint new_x_flag = src & (1 << (shift - 1));

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = res;

  FLAG_C = FLAG_X = (new_x_flag != 0)<<8;
  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;

#endif
}


static void m68k_op_roxr_8_r(void)
{
  uint* r_dst = &DY;
  uint orig_shift = DX & 0x3f;

  if(orig_shift != 0)
  {
    uint shift = orig_shift % 9;
    uint src   = MASK_OUT_ABOVE_8(*r_dst);
    uint res   = ROR_9(src | (XFLAG_AS_1() << 8), shift);

    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    FLAG_C = FLAG_X = res;
    res = MASK_OUT_ABOVE_8(res);

    *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
    FLAG_N = NFLAG_8(res);
    FLAG_Z = res;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = FLAG_X;
  FLAG_N = NFLAG_8(*r_dst);
  FLAG_Z = MASK_OUT_ABOVE_8(*r_dst);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_r(void)
{
  uint* r_dst = &DY;
  uint orig_shift = DX & 0x3f;

  if(orig_shift != 0)
  {
    uint shift = orig_shift % 17;
    uint src   = MASK_OUT_ABOVE_16(*r_dst);
    uint res   = ROR_17(src | (XFLAG_AS_1() << 16), shift);

    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    FLAG_C = FLAG_X = res >> 8;
    res = MASK_OUT_ABOVE_16(res);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    FLAG_N = NFLAG_16(res);
    FLAG_Z = res;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = FLAG_X;
  FLAG_N = NFLAG_16(*r_dst);
  FLAG_Z = MASK_OUT_ABOVE_16(*r_dst);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_32_r(void)
{
#if M68K_USE_64_BIT

  uint*  r_dst = &DY;
  uint   orig_shift = DX & 0x3f;

  if(orig_shift != 0)
  {
    uint   shift = orig_shift % 33;
    uint64 src   = *r_dst;
    uint64 res   = src | (((uint64)XFLAG_AS_1()) << 32);

    res = ROR_33_64(res, shift);

    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    FLAG_C = FLAG_X = res >> 24;
    res = MASK_OUT_ABOVE_32(res);

    *r_dst = res;
    FLAG_N = NFLAG_32(res);
    FLAG_Z = res;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = FLAG_X;
  FLAG_N = NFLAG_32(*r_dst);
  FLAG_Z = *r_dst;
  FLAG_V = VFLAG_CLEAR;

#else

  uint* r_dst = &DY;
  uint orig_shift = DX & 0x3f;
  uint shift = orig_shift % 33;
  uint src = *r_dst;
  uint res = MASK_OUT_ABOVE_32((ROR_33(src, shift) & ~(1 << (32 - shift))) | (XFLAG_AS_1() << (32 - shift)));
  uint new_x_flag = src & (1 << (shift - 1));

  if(orig_shift != 0)
  {
    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();
  }

  if(shift != 0)
  {
    *r_dst = res;
    FLAG_X = (new_x_flag != 0)<<8;
  }
  else
    res = src;
  FLAG_C = FLAG_X;
  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;

#endif
}


static void m68k_op_roxr_16_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_di(void)
{
  uint ea = EA_AY_DI_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_aw(void)
{
  uint ea = EA_AW_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxr_16_al(void)
{
  uint ea = EA_AL_16();
  uint src = m68ki_read_16(ea);
  uint res = ROR_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_8_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_8(*r_dst);
  uint res = ROL_9(src | (XFLAG_AS_1() << 8), shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  FLAG_C = FLAG_X = res;
  res = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_s(void)
{
  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = MASK_OUT_ABOVE_16(*r_dst);
  uint res = ROL_17(src | (XFLAG_AS_1() << 16), shift);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_32_s(void)
{
#if M68K_USE_64_BIT

  uint*  r_dst = &DY;
  uint   shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint64 src   = *r_dst;
  uint64 res   = src | (((uint64)XFLAG_AS_1()) << 32);

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  res = ROL_33_64(res, shift);

  FLAG_C = FLAG_X = res >> 24;
  res = MASK_OUT_ABOVE_32(res);

  *r_dst = res;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;

#else

  uint* r_dst = &DY;
  uint shift = (((REG_IR >> 9) - 1) & 7) + 1;
  uint src = *r_dst;
  uint res = MASK_OUT_ABOVE_32((ROL_33(src, shift) & ~(1 << (shift - 1))) | (XFLAG_AS_1() << (shift - 1)));
  uint new_x_flag = src & (1 << (32 - shift));

  if(shift != 0)
    USE_CYCLES(shift * CYC_SHIFT);

  *r_dst = res;

  FLAG_C = FLAG_X = (new_x_flag != 0)<<8;
  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;

#endif
}


static void m68k_op_roxl_8_r(void)
{
  uint* r_dst = &DY;
  uint orig_shift = DX & 0x3f;


  if(orig_shift != 0)
  {
    uint shift = orig_shift % 9;
    uint src   = MASK_OUT_ABOVE_8(*r_dst);
    uint res   = ROL_9(src | (XFLAG_AS_1() << 8), shift);

    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    FLAG_C = FLAG_X = res;
    res = MASK_OUT_ABOVE_8(res);

    *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
    FLAG_N = NFLAG_8(res);
    FLAG_Z = res;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = FLAG_X;
  FLAG_N = NFLAG_8(*r_dst);
  FLAG_Z = MASK_OUT_ABOVE_8(*r_dst);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_r(void)
{
  uint* r_dst = &DY;
  uint orig_shift = DX & 0x3f;

  if(orig_shift != 0)
  {
    uint shift = orig_shift % 17;
    uint src   = MASK_OUT_ABOVE_16(*r_dst);
    uint res   = ROL_17(src | (XFLAG_AS_1() << 16), shift);

    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    FLAG_C = FLAG_X = res >> 8;
    res = MASK_OUT_ABOVE_16(res);

    *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
    FLAG_N = NFLAG_16(res);
    FLAG_Z = res;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = FLAG_X;
  FLAG_N = NFLAG_16(*r_dst);
  FLAG_Z = MASK_OUT_ABOVE_16(*r_dst);
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_32_r(void)
{
#if M68K_USE_64_BIT

  uint*  r_dst = &DY;
  uint   orig_shift = DX & 0x3f;

  if(orig_shift != 0)
  {
    uint   shift = orig_shift % 33;
    uint64 src   = *r_dst;
    uint64 res   = src | (((uint64)XFLAG_AS_1()) << 32);

    res = ROL_33_64(res, shift);

    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();

    FLAG_C = FLAG_X = res >> 24;
    res = MASK_OUT_ABOVE_32(res);

    *r_dst = res;
    FLAG_N = NFLAG_32(res);
    FLAG_Z = res;
    FLAG_V = VFLAG_CLEAR;
    return;
  }

  FLAG_C = FLAG_X;
  FLAG_N = NFLAG_32(*r_dst);
  FLAG_Z = *r_dst;
  FLAG_V = VFLAG_CLEAR;

#else

  uint* r_dst = &DY;
  uint orig_shift = DX & 0x3f;
  uint shift = orig_shift % 33;
  uint src = *r_dst;
  uint res = MASK_OUT_ABOVE_32((ROL_33(src, shift) & ~(1 << (shift - 1))) | (XFLAG_AS_1() << (shift - 1)));
  uint new_x_flag = src & (1 << (32 - shift));

  if(orig_shift != 0)
  {
    USE_CYCLES(orig_shift * CYC_SHIFT);
    SKIP_BUS_REFRESH();
  }

  if(shift != 0)
  {
    *r_dst = res;
    FLAG_X = (new_x_flag != 0)<<8;
  }
  else
    res = src;
  FLAG_C = FLAG_X;
  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;

#endif
}


static void m68k_op_roxl_16_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint src = m68ki_read_16(ea);
  uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint src = m68ki_read_16(ea);
  uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint src = m68ki_read_16(ea);
  uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_di(void)
{
  uint ea = EA_AY_DI_16();
  uint src = m68ki_read_16(ea);
  uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint src = m68ki_read_16(ea);
  uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_aw(void)
{
  uint ea = EA_AW_16();
  uint src = m68ki_read_16(ea);
  uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_roxl_16_al(void)
{
  uint ea = EA_AL_16();
  uint src = m68ki_read_16(ea);
  uint res = ROL_17(src | (XFLAG_AS_1() << 16), 1);

  FLAG_C = FLAG_X = res >> 8;
  res = MASK_OUT_ABOVE_16(res);

  m68ki_write_16(ea, res);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_rte_32(void)
{
  if(FLAG_S)
  {
    uint new_sr;
    uint new_pc;

    new_sr = m68ki_pull_16();
    new_pc = m68ki_pull_32();
    m68ki_jump(new_pc);
    m68ki_set_sr(new_sr);

#if M68K_EMULATE_ADDRESS_ERROR
    CPU_INSTR_MODE = INSTRUCTION_YES;
    CPU_RUN_MODE = RUN_MODE_NORMAL;
#endif

    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_rtr_32(void)
{
  m68ki_set_ccr(m68ki_pull_16());
  m68ki_jump(m68ki_pull_32());
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_rts_32(void)
{
  m68ki_jump(m68ki_pull_32());
}


static void m68k_op_sbcd_8_rr(void)
{
  uint* r_dst = &DX;
  uint src = DY;
  uint dst = *r_dst;
  uint res = LOW_NIBBLE(dst) - LOW_NIBBLE(src) - XFLAG_AS_1();
  uint corf = 0;

  if(res > 0xf)
    corf = 6;

  res += HIGH_NIBBLE(dst) - HIGH_NIBBLE(src);
  FLAG_V = res; /* Undefined V behavior */

  if(res > 0xff)
  {
    res += 0xa0;
    FLAG_X = FLAG_C = CFLAG_SET;
  }
  else if(res < corf)
    FLAG_X = FLAG_C = CFLAG_SET;
  else
    FLAG_X = FLAG_C = 0;

  res = MASK_OUT_ABOVE_8(res-corf);

  FLAG_V &= ~res; /* Undefined V behavior part II */
  FLAG_N = NFLAG_8(res); /* Undefined N behavior */
  FLAG_Z |= res;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
}


static void m68k_op_sbcd_8_mm_ax7(void)
{
  uint src = OPER_AY_PD_8();
  uint ea  = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = LOW_NIBBLE(dst) - LOW_NIBBLE(src) - XFLAG_AS_1();
  uint corf = 0;

  if(res > 0xf)
    corf = 6;
  res += HIGH_NIBBLE(dst) - HIGH_NIBBLE(src);
  FLAG_V = res; /* Undefined V behavior */

  if(res > 0xff)
  {
    res += 0xa0;
    FLAG_X = FLAG_C = CFLAG_SET;
  }
  else if(res < corf)
    FLAG_X = FLAG_C = CFLAG_SET;
  else
    FLAG_X = FLAG_C = 0;

  res = MASK_OUT_ABOVE_8(res-corf);

  FLAG_V &= ~res; /* Undefined V behavior part II */
  FLAG_N = NFLAG_8(res); /* Undefined N behavior */
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_sbcd_8_mm_ay7(void)
{
  uint src = OPER_A7_PD_8();
  uint ea  = EA_AX_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = LOW_NIBBLE(dst) - LOW_NIBBLE(src) - XFLAG_AS_1();
  uint corf = 0;

  if(res > 0xf)
    corf = 6;
  res += HIGH_NIBBLE(dst) - HIGH_NIBBLE(src);
  FLAG_V = res; /* Undefined V behavior */

  if(res > 0xff)
  {
    res += 0xa0;
    FLAG_X = FLAG_C = CFLAG_SET;
  }
  else if(res < corf)
    FLAG_X = FLAG_C = CFLAG_SET;
  else
    FLAG_X = FLAG_C = 0;

  res = MASK_OUT_ABOVE_8(res-corf);

  FLAG_V &= ~res; /* Undefined V behavior part II */
  FLAG_N = NFLAG_8(res); /* Undefined N behavior */
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_sbcd_8_mm_axy7(void)
{
  uint src = OPER_A7_PD_8();
  uint ea  = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = LOW_NIBBLE(dst) - LOW_NIBBLE(src) - XFLAG_AS_1();
  uint corf = 0;

  if(res > 0xf)
    corf = 6;
  res += HIGH_NIBBLE(dst) - HIGH_NIBBLE(src);
  FLAG_V = res; /* Undefined V behavior */

  if(res > 0xff)
  {
    res += 0xa0;
    FLAG_X = FLAG_C = CFLAG_SET;
  }
  else if(res < corf)
    FLAG_X = FLAG_C = CFLAG_SET;
  else
    FLAG_X = FLAG_C = 0;

  res = MASK_OUT_ABOVE_8(res-corf);

  FLAG_V &= ~res; /* Undefined V behavior part II */
  FLAG_N = NFLAG_8(res); /* Undefined N behavior */
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_sbcd_8_mm(void)
{
  uint src = OPER_AY_PD_8();
  uint ea  = EA_AX_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = LOW_NIBBLE(dst) - LOW_NIBBLE(src) - XFLAG_AS_1();
  uint corf = 0;

  if(res > 0xf)
    corf = 6;
  res += HIGH_NIBBLE(dst) - HIGH_NIBBLE(src);
  FLAG_V = res; /* Undefined V behavior */

  if(res > 0xff)
  {
    res += 0xa0;
    FLAG_X = FLAG_C = CFLAG_SET;
  }
  else if(res < corf)
    FLAG_X = FLAG_C = CFLAG_SET;
  else
    FLAG_X = FLAG_C = 0;

  res = MASK_OUT_ABOVE_8(res-corf);

  FLAG_V &= ~res; /* Undefined V behavior part II */
  FLAG_N = NFLAG_8(res); /* Undefined N behavior */
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_st_8_d(void)
{
  DY |= 0xff;
}


static void m68k_op_st_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), 0xff);
}


static void m68k_op_st_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), 0xff);
}


static void m68k_op_st_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), 0xff);
}


static void m68k_op_st_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), 0xff);
}


static void m68k_op_st_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), 0xff);
}


static void m68k_op_st_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), 0xff);
}


static void m68k_op_st_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), 0xff);
}


static void m68k_op_st_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), 0xff);
}


static void m68k_op_st_8_al(void)
{
  m68ki_write_8(EA_AL_8(), 0xff);
}


static void m68k_op_sf_8_d(void)
{
  DY &= 0xffffff00;
}


static void m68k_op_sf_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), 0);
}


static void m68k_op_sf_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), 0);
}


static void m68k_op_sf_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), 0);
}


static void m68k_op_sf_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), 0);
}


static void m68k_op_sf_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), 0);
}


static void m68k_op_sf_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), 0);
}


static void m68k_op_sf_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), 0);
}


static void m68k_op_sf_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), 0);
}


static void m68k_op_sf_8_al(void)
{
  m68ki_write_8(EA_AL_8(), 0);
}


static void m68k_op_shi_8_d(void)
{
  if(COND_HI())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_sls_8_d(void)
{
  if(COND_LS())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_scc_8_d(void)
{
  if(COND_CC())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_scs_8_d(void)
{
  if(COND_CS())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_sne_8_d(void)
{
  if(COND_NE())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_seq_8_d(void)
{
  if(COND_EQ())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_svc_8_d(void)
{
  if(COND_VC())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_svs_8_d(void)
{
  if(COND_VS())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_spl_8_d(void)
{
  if(COND_PL())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_smi_8_d(void)
{
  if(COND_MI())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_sge_8_d(void)
{
  if(COND_GE())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_slt_8_d(void)
{
  if(COND_LT())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_sgt_8_d(void)
{
  if(COND_GT())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_sle_8_d(void)
{
  if(COND_LE())
  {
    DY |= 0xff;
    USE_CYCLES(CYC_SCC_R_TRUE);
    return;
  }
  DY &= 0xffffff00;
}


static void m68k_op_shi_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_shi_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_HI() ? 0xff : 0);
}


static void m68k_op_sls_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_sls_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_LS() ? 0xff : 0);
}


static void m68k_op_scc_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scc_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_CC() ? 0xff : 0);
}


static void m68k_op_scs_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_scs_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_CS() ? 0xff : 0);
}


static void m68k_op_sne_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_sne_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_NE() ? 0xff : 0);
}


static void m68k_op_seq_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_seq_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_EQ() ? 0xff : 0);
}


static void m68k_op_svc_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svc_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_VC() ? 0xff : 0);
}


static void m68k_op_svs_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_svs_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_VS() ? 0xff : 0);
}


static void m68k_op_spl_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_spl_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_PL() ? 0xff : 0);
}


static void m68k_op_smi_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_smi_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_MI() ? 0xff : 0);
}


static void m68k_op_sge_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_sge_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_GE() ? 0xff : 0);
}


static void m68k_op_slt_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_slt_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_LT() ? 0xff : 0);
}


static void m68k_op_sgt_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sgt_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_GT() ? 0xff : 0);
}


static void m68k_op_sle_8_ai(void)
{
  m68ki_write_8(EA_AY_AI_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_pi(void)
{
  m68ki_write_8(EA_AY_PI_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_pi7(void)
{
  m68ki_write_8(EA_A7_PI_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_pd(void)
{
  m68ki_write_8(EA_AY_PD_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_pd7(void)
{
  m68ki_write_8(EA_A7_PD_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_di(void)
{
  m68ki_write_8(EA_AY_DI_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_ix(void)
{
  m68ki_write_8(EA_AY_IX_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_aw(void)
{
  m68ki_write_8(EA_AW_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_sle_8_al(void)
{
  m68ki_write_8(EA_AL_8(), COND_LE() ? 0xff : 0);
}


static void m68k_op_stop(void)
{
  if(FLAG_S)
  {
    uint new_sr = OPER_I_16();
    CPU_STOPPED |= STOP_LEVEL_STOP;
    m68ki_set_sr(new_sr);
    if (CPU_STOPPED)
    {
      SET_CYCLES(m68ki_cpu.cycle_end - 4*MUL); 
    }
    return;
  }
  m68ki_exception_privilege_violation();
}


static void m68k_op_sub_8_er_d(void)
{
  uint* r_dst = &DX;
  uint src = MASK_OUT_ABOVE_8(DY);
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_ai(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_AI_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_pi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PI_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_pi7(void)
{
  uint* r_dst = &DX;
  uint src = OPER_A7_PI_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_pd(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PD_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_pd7(void)
{
  uint* r_dst = &DX;
  uint src = OPER_A7_PD_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_di(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_DI_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_ix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_IX_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_aw(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AW_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_al(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AL_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_pcdi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCDI_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_pcix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCIX_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_8_er_i(void)
{
  uint* r_dst = &DX;
  uint src = OPER_I_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_d(void)
{
  uint* r_dst = &DX;
  uint src = MASK_OUT_ABOVE_16(DY);
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_a(void)
{
  uint* r_dst = &DX;
  uint src = MASK_OUT_ABOVE_16(AY);
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_ai(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_AI_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_pi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PI_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_pd(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PD_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_di(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_DI_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_ix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_IX_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_aw(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AW_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_al(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AL_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_pcdi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCDI_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_pcix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCIX_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_16_er_i(void)
{
  uint* r_dst = &DX;
  uint src = OPER_I_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_sub_32_er_d(void)
{
  uint* r_dst = &DX;
  uint src = DY;
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_a(void)
{
  uint* r_dst = &DX;
  uint src = AY;
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_ai(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_AI_32();
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_pi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PI_32();
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_pd(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_PD_32();
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_di(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_DI_32();
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_ix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AY_IX_32();
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_aw(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AW_32();
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_al(void)
{
  uint* r_dst = &DX;
  uint src = OPER_AL_32();
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_pcdi(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCDI_32();
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_pcix(void)
{
  uint* r_dst = &DX;
  uint src = OPER_PCIX_32();
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_sub_32_er_i(void)
{
  uint* r_dst = &DX;
  uint src = OPER_I_32();
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);

  *r_dst = FLAG_Z;
}


static void m68k_op_sub_8_re_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_di(void)
{
  uint ea = EA_AY_DI_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_aw(void)
{
  uint ea = EA_AW_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_8_re_al(void)
{
  uint ea = EA_AL_8();
  uint src = MASK_OUT_ABOVE_8(DX);
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_sub_16_re_ai(void)
{
  uint ea = EA_AY_AI_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_sub_16_re_pi(void)
{
  uint ea = EA_AY_PI_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_sub_16_re_pd(void)
{
  uint ea = EA_AY_PD_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_sub_16_re_di(void)
{
  uint ea = EA_AY_DI_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_sub_16_re_ix(void)
{
  uint ea = EA_AY_IX_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_sub_16_re_aw(void)
{
  uint ea = EA_AW_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_sub_16_re_al(void)
{
  uint ea = EA_AL_16();
  uint src = MASK_OUT_ABOVE_16(DX);
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_sub_32_re_ai(void)
{
  uint ea = EA_AY_AI_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_sub_32_re_pi(void)
{
  uint ea = EA_AY_PI_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_sub_32_re_pd(void)
{
  uint ea = EA_AY_PD_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_sub_32_re_di(void)
{
  uint ea = EA_AY_DI_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_sub_32_re_ix(void)
{
  uint ea = EA_AY_IX_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_sub_32_re_aw(void)
{
  uint ea = EA_AW_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_sub_32_re_al(void)
{
  uint ea = EA_AL_32();
  uint src = DX;
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_suba_16_d(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - MAKE_INT_16(DY));
}


static void m68k_op_suba_16_a(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - MAKE_INT_16(AY));
}


static void m68k_op_suba_16_ai(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AY_AI_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_pi(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AY_PI_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_pd(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AY_PD_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_di(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AY_DI_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_ix(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AY_IX_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_aw(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AW_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_al(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_AL_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_pcdi(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_PCDI_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_pcix(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_PCIX_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_16_i(void)
{
  uint* r_dst = &AX;
  uint src = MAKE_INT_16(OPER_I_16());

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_d(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - DY);
}


static void m68k_op_suba_32_a(void)
{
  uint* r_dst = &AX;

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - AY);
}


static void m68k_op_suba_32_ai(void)
{
  uint* r_dst = &AX;
  uint src = OPER_AY_AI_32();

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_pi(void)
{
  uint* r_dst = &AX;
  uint src = OPER_AY_PI_32();

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_pd(void)
{
  uint* r_dst = &AX;
  uint src = OPER_AY_PD_32();

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_di(void)
{
  uint* r_dst = &AX;
  uint src = OPER_AY_DI_32();

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_ix(void)
{
  uint* r_dst = &AX;
  uint src = OPER_AY_IX_32();

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_aw(void)
{
  uint* r_dst = &AX;
  uint src = OPER_AW_32();

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_al(void)
{
  uint* r_dst = &AX;
  uint src = OPER_AL_32();

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_pcdi(void)
{
  uint* r_dst = &AX;
  uint src = OPER_PCDI_32();

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_pcix(void)
{
  uint* r_dst = &AX;
  uint src = OPER_PCIX_32();

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_suba_32_i(void)
{
  uint* r_dst = &AX;
  uint src = OPER_I_32();

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - src);
}


static void m68k_op_subi_8_d(void)
{
  uint* r_dst = &DY;
  uint src = OPER_I_8();
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_subi_8_ai(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_AI_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_pi(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_PI_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_pi7(void)
{
  uint src = OPER_I_8();
  uint ea = EA_A7_PI_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_pd(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_pd7(void)
{
  uint src = OPER_I_8();
  uint ea = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_di(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_DI_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_ix(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AY_IX_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_aw(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AW_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_8_al(void)
{
  uint src = OPER_I_8();
  uint ea = EA_AL_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subi_16_d(void)
{
  uint* r_dst = &DY;
  uint src = OPER_I_16();
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_subi_16_ai(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_AI_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_16_pi(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_PI_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_16_pd(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_PD_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_16_di(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_DI_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_16_ix(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AY_IX_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_16_aw(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AW_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_16_al(void)
{
  uint src = OPER_I_16();
  uint ea = EA_AL_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subi_32_d(void)
{
  uint* r_dst = &DY;
  uint src = OPER_I_32();
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  *r_dst = FLAG_Z;
}


static void m68k_op_subi_32_ai(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_AI_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subi_32_pi(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_PI_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subi_32_pd(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_PD_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subi_32_di(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_DI_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subi_32_ix(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AY_IX_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subi_32_aw(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AW_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subi_32_al(void)
{
  uint src = OPER_I_32();
  uint ea = EA_AL_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subq_8_d(void)
{
  uint* r_dst = &DY;
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | FLAG_Z;
}


static void m68k_op_subq_8_ai(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_AI_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subq_8_pi(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_PI_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subq_8_pi7(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_A7_PI_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subq_8_pd(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subq_8_pd7(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subq_8_di(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_DI_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subq_8_ix(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_IX_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subq_8_aw(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AW_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subq_8_al(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AL_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_8(res);
  FLAG_Z = MASK_OUT_ABOVE_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  m68ki_write_8(ea, FLAG_Z);
}


static void m68k_op_subq_16_d(void)
{
  uint* r_dst = &DY;
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | FLAG_Z;
}


static void m68k_op_subq_16_a(void)
{
  uint* r_dst = &AY;

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - ((((REG_IR >> 9) - 1) & 7) + 1));
}


static void m68k_op_subq_16_ai(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_AI_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subq_16_pi(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_PI_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subq_16_pd(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_PD_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subq_16_di(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_DI_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subq_16_ix(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_IX_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subq_16_aw(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AW_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subq_16_al(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AL_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_16(res);
  FLAG_Z = MASK_OUT_ABOVE_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  m68ki_write_16(ea, FLAG_Z);
}


static void m68k_op_subq_32_d(void)
{
  uint* r_dst = &DY;
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint dst = *r_dst;
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  *r_dst = FLAG_Z;
}


static void m68k_op_subq_32_a(void)
{
  uint* r_dst = &AY;

  *r_dst = MASK_OUT_ABOVE_32(*r_dst - ((((REG_IR >> 9) - 1) & 7) + 1));
}


static void m68k_op_subq_32_ai(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_AI_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subq_32_pi(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_PI_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subq_32_pd(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_PD_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subq_32_di(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_DI_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subq_32_ix(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AY_IX_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subq_32_aw(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AW_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subq_32_al(void)
{
  uint src = (((REG_IR >> 9) - 1) & 7) + 1;
  uint ea = EA_AL_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = MASK_OUT_ABOVE_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  m68ki_write_32(ea, FLAG_Z);
}


static void m68k_op_subx_8_rr(void)
{
  uint* r_dst = &DX;
  uint src = MASK_OUT_ABOVE_8(DY);
  uint dst = MASK_OUT_ABOVE_8(*r_dst);
  uint res = dst - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  *r_dst = MASK_OUT_BELOW_8(*r_dst) | res;
}


static void m68k_op_subx_16_rr(void)
{
  uint* r_dst = &DX;
  uint src = MASK_OUT_ABOVE_16(DY);
  uint dst = MASK_OUT_ABOVE_16(*r_dst);
  uint res = dst - src - XFLAG_AS_1();

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  res = MASK_OUT_ABOVE_16(res);
  FLAG_Z |= res;

  *r_dst = MASK_OUT_BELOW_16(*r_dst) | res;
}


static void m68k_op_subx_32_rr(void)
{
  uint* r_dst = &DX;
  uint src = DY;
  uint dst = *r_dst;
  uint res = dst - src - XFLAG_AS_1();

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  res = MASK_OUT_ABOVE_32(res);
  FLAG_Z |= res;

  *r_dst = res;
}


static void m68k_op_subx_8_mm_ax7(void)
{
  uint src = OPER_AY_PD_8();
  uint ea  = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_subx_8_mm_ay7(void)
{
  uint src = OPER_A7_PD_8();
  uint ea  = EA_AX_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_subx_8_mm_axy7(void)
{
  uint src = OPER_A7_PD_8();
  uint ea  = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_subx_8_mm(void)
{
  uint src = OPER_AY_PD_8();
  uint ea  = EA_AX_PD_8();
  uint dst = m68ki_read_8(ea);
  uint res = dst - src - XFLAG_AS_1();

  FLAG_N = NFLAG_8(res);
  FLAG_X = FLAG_C = CFLAG_8(res);
  FLAG_V = VFLAG_SUB_8(src, dst, res);

  res = MASK_OUT_ABOVE_8(res);
  FLAG_Z |= res;

  m68ki_write_8(ea, res);
}


static void m68k_op_subx_16_mm(void)
{
  uint src = OPER_AY_PD_16();
  uint ea  = EA_AX_PD_16();
  uint dst = m68ki_read_16(ea);
  uint res = dst - src - XFLAG_AS_1();

  FLAG_N = NFLAG_16(res);
  FLAG_X = FLAG_C = CFLAG_16(res);
  FLAG_V = VFLAG_SUB_16(src, dst, res);

  res = MASK_OUT_ABOVE_16(res);
  FLAG_Z |= res;

  m68ki_write_16(ea, res);
}


static void m68k_op_subx_32_mm(void)
{
  uint src = OPER_AY_PD_32();
  uint ea  = EA_AX_PD_32();
  uint dst = m68ki_read_32(ea);
  uint res = dst - src - XFLAG_AS_1();

  FLAG_N = NFLAG_32(res);
  FLAG_X = FLAG_C = CFLAG_SUB_32(src, dst, res);
  FLAG_V = VFLAG_SUB_32(src, dst, res);

  res = MASK_OUT_ABOVE_32(res);
  FLAG_Z |= res;

  m68ki_write_32(ea, res);
}


static void m68k_op_swap_32(void)
{
  uint* r_dst = &DY;

  FLAG_Z = MASK_OUT_ABOVE_32(*r_dst<<16);
  *r_dst = (*r_dst>>16) | FLAG_Z;

  FLAG_Z = *r_dst;
  FLAG_N = NFLAG_32(*r_dst);
  FLAG_C = CFLAG_CLEAR;
  FLAG_V = VFLAG_CLEAR;
}


static void m68k_op_tas_8_d(void)
{
  uint* r_dst = &DY;

  FLAG_Z = MASK_OUT_ABOVE_8(*r_dst);
  FLAG_N = NFLAG_8(*r_dst);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
  *r_dst |= 0x80;
}


static void m68k_op_tas_8_ai(void)
{
  uint ea = EA_AY_AI_8();
  uint dst = m68ki_read_8(ea);

  FLAG_Z = dst;
  FLAG_N = NFLAG_8(dst);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  /* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
  if (m68ki_tas_callback()) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_pi(void)
{
  uint ea = EA_AY_PI_8();
  uint dst = m68ki_read_8(ea);

  FLAG_Z = dst;
  FLAG_N = NFLAG_8(dst);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  /* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
  if (m68ki_tas_callback()) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_pi7(void)
{
  uint ea = EA_A7_PI_8();
  uint dst = m68ki_read_8(ea);

  FLAG_Z = dst;
  FLAG_N = NFLAG_8(dst);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  /* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
  if (m68ki_tas_callback()) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_pd(void)
{
  uint ea = EA_AY_PD_8();
  uint dst = m68ki_read_8(ea);

  FLAG_Z = dst;
  FLAG_N = NFLAG_8(dst);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  /* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
  if (m68ki_tas_callback()) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_pd7(void)
{
  uint ea = EA_A7_PD_8();
  uint dst = m68ki_read_8(ea);

  FLAG_Z = dst;
  FLAG_N = NFLAG_8(dst);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  /* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
  if (m68ki_tas_callback()) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_di(void)
{
  uint ea = EA_AY_DI_8();
  uint dst = m68ki_read_8(ea);

  FLAG_Z = dst;
  FLAG_N = NFLAG_8(dst);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  /* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
  if (m68ki_tas_callback()) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_ix(void)
{
  uint ea = EA_AY_IX_8();
  uint dst = m68ki_read_8(ea);

  FLAG_Z = dst;
  FLAG_N = NFLAG_8(dst);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  /* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
  if (m68ki_tas_callback()) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_aw(void)
{
  uint ea = EA_AW_8();
  uint dst = m68ki_read_8(ea);

  FLAG_Z = dst;
  FLAG_N = NFLAG_8(dst);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  /* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
  if (m68ki_tas_callback()) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_tas_8_al(void)
{
  uint ea = EA_AL_8();
  uint dst = m68ki_read_8(ea);

  FLAG_Z = dst;
  FLAG_N = NFLAG_8(dst);
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;

  /* The Genesis/Megadrive games Gargoyles and Ex-Mutants need the TAS writeback
       disabled in order to function properly.  Some Amiga software may also rely
       on this, but only when accessing specific addresses so additional functionality
       will be needed. */
  if (m68ki_tas_callback()) m68ki_write_8(ea, dst | 0x80);
}


static void m68k_op_trap(void)
{
  /* Trap#n stacks exception frame type 0 */
  m68ki_exception_trapN(EXCEPTION_TRAP_BASE + (REG_IR & 0xf));  /* HJB 990403 */
}


static void m68k_op_trapv(void)
{
  if(COND_VC())
  {
    USE_CYCLES (4 * MUL);
    return;
  }
  m68ki_exception_trap(EXCEPTION_TRAPV);  /* HJB 990403 */
}


static void m68k_op_tst_8_d(void)
{
  uint res = MASK_OUT_ABOVE_8(DY);

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_8_ai(void)
{
  uint res = OPER_AY_AI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_8_pi(void)
{
  uint res = OPER_AY_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_8_pi7(void)
{
  uint res = OPER_A7_PI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_8_pd(void)
{
  uint res = OPER_AY_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_8_pd7(void)
{
  uint res = OPER_A7_PD_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


#ifdef __NDS__
__attribute__((section(".itcm"),long_call))
#endif
static void m68k_op_tst_8_di(void)
{
  uint res = OPER_AY_DI_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_8_ix(void)
{
  uint res = OPER_AY_IX_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_8_aw(void)
{
  uint res = OPER_AW_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_8_al(void)
{
  uint res = OPER_AL_8();

  FLAG_N = NFLAG_8(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_16_d(void)
{
  uint res = MASK_OUT_ABOVE_16(DY);

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_16_ai(void)
{
  uint res = OPER_AY_AI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_16_pi(void)
{
  uint res = OPER_AY_PI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_16_pd(void)
{
  uint res = OPER_AY_PD_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_16_di(void)
{
  uint res = OPER_AY_DI_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_16_ix(void)
{
  uint res = OPER_AY_IX_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_16_aw(void)
{
  uint res = OPER_AW_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_16_al(void)
{
  uint res = OPER_AL_16();

  FLAG_N = NFLAG_16(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_32_d(void)
{
  uint res = DY;

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_32_ai(void)
{
  uint res = OPER_AY_AI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_32_pi(void)
{
  uint res = OPER_AY_PI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_32_pd(void)
{
  uint res = OPER_AY_PD_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_32_di(void)
{
  uint res = OPER_AY_DI_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_32_ix(void)
{
  uint res = OPER_AY_IX_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_32_aw(void)
{
  uint res = OPER_AW_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_tst_32_al(void)
{
  uint res = OPER_AL_32();

  FLAG_N = NFLAG_32(res);
  FLAG_Z = res;
  FLAG_V = VFLAG_CLEAR;
  FLAG_C = CFLAG_CLEAR;
}


static void m68k_op_unlk_32_a7(void)
{
  REG_A[7] = m68ki_read_32(REG_A[7]);
}


static void m68k_op_unlk_32(void)
{
  uint* r_dst = &AY;

  REG_A[7] = *r_dst;
  *r_dst = m68ki_pull_32();
}


/* ======================================================================== */
/* ========================= OPCODE TABLE BUILDER ========================= */
/* ======================================================================== */



static const unsigned char compact_pages[256]={0,0,1,0,2,0,3,4,5,0,0,0,6,0,0,0,7,8,9,10,11,12,13,14,15,0,16,0,17,0,18,0,19,0,20,21,0,0,22,0,23,0,24,0,25,0,0,0,26,0,27,28,0,0,29,0,0,0,30,0,31,0,32,0,0,33,34,35,36,37,0,38,39,40,41,42,43,44,45,0,0,46,47,48,49,50,51,0,52,53,0,0,0,0,0,0,54,0,0,55,56,57,58,59,0,0,60,61,0,0,0,0,62,0,63,0,64,0,65,0,66,0,67,0,68,0,0,0,69,70,71,0,0,0,0,0,0,0,72,73,0,74,0,0,75,0,0,76,0,0,0,0,77,0,78,0,79,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,80,0,81,0,82,0,0,0,0,0,0,0,83,0,0,0,84,0,85,0,0,0,0,0,0,0,86,0,87,0,0,0,88,89,0,90,0,91,0,0,92,93,94,95,96,97,0,0,98,99,100,101,102,103,0,104,105,106,0,0,0,0,107,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const unsigned short compact_lookup[][256]={
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,1,2,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,6,0,0,7,0,8,9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10,0,0,0,0,11,12,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{16,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,19,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,20,0,0,0,0,0,0},
{21,0,0,22,0,0,23,0,0,0,0,0,0,0,0,0,0,0,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,25,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,26,0,0,0,0,0,0,0,0,0,0,27,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,28,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,29,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,30,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,31,0,0,0,0,0,0},
{32,33,0,34,0,35,36,0,0,0,0,0,0,0,0,0,37,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,38,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,39,0,0,0,0,0,0,40,0,0,41,0,42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,43,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,44,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,45,0,46,0,0,0,0,0,47,0,0,0,0,0,0,0,0,0,0,48,0,0,0,0,0,0,0,0,0,0,49,0,50,0,0,51,0,52,53,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,54,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,55,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,56,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,57,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,58,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,59,60,61,0,0,62,63,0,0,0,0,0,0,0,0,0,64,0,0,0,0,0,0,0,65,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,66,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,67,0,0,68,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,69,0,70,0,0,71,72,73,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,74,0,0,75,0,0,76,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,77,0,0,0,0,0,0,0,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,79,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,80,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,81,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,82,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,83,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,84,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,85,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,86,0,0,0,0,87,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{88,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,89,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,90,0,0,91,0,92,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,93,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,94,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,95,0,0,0,0,96,0,0,0,0,0,0,0,0,0,97,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,98,0,0,0,0,0,0,0,99,0,0,0,0,100,0,0,0,0,101,102,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{103,0,0,0,0,104,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,105,0,0,106,0,0,0,0,0,107,0,0,0,0,0,0,0,0,0,0,108,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,109,0,110,111,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,112,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,113,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{114,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,115,0,116,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,117,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,118,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,119,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,120,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,121,0,122,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,124,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,125,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,126,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,127,0,0,0,0,0,0,0,0,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,129,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,130,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,131,0,0,132,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,133,0,0,0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,135,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,136,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,0,138,139,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,140,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,141,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,142,0,0,0,0,0,0,0,0,0,0,0,0,143,0,144,0,0,0,145,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,146,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,147,0,0,148,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,149,150,151,0,0,0,152,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,154,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,155,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,156,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,157,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,158,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,159,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{160,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,161,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,162,163,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,164,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,165,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,167,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,168,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,169,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,170,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,171,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,172,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,173,174,175,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,176,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,177,0,0,0,0,0,0},
{178,179,0,0,0,0,180,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,181,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,182,0,0,0,0,0,0,0,183,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,184,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,185,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,186,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,187,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,188,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,189,190,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,191,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,193,194,0,0,195,196,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,197,0,198,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,199,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,200,0,0,0,0,0,0,0,0,0,0,0,0,201,0,0,0,0,0,0,202,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,203,0,0,0,204,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,205,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,206,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,207,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,208,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,209,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,210,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,211,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,212,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,213,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,214,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,215,0,0,0,0,0,0,0,0,0,216,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{217,0,0,0,0,0,218,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,219,0,0,0,0,0,0,0,0,0,0,0,0,0,220,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,221,0,0,0,222,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{223,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{224,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,225,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{226,0,0,0,0,0,227,0,0,0,228,0,0,0,0,0,0,0,0,0,229,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,230,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{231,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,232,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{233,0,0,0,0,0,234,0,0,0,235,0,236,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{237,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,238,0,0,0},
{239,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{240,241,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{242,243,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{244,0,0,245,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{246,0,0,0,247,248,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{249,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,250,0,0,251,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,252,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{253,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{254,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,256,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,257,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,258,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,259,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{260,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,262,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,263,0,264,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,265,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,266,0,0,0},
{0,0,0,0,0,267,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,268,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,269,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,270,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,271,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,272,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,273,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,274,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,275,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,276,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,277,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,278,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,279,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,280,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,281,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,282,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,283,284,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,285,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,286,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,287,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,288,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,289,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,290,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,291,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,292,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,293,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,294,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,295,0,0,0,0,296,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,297,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,298,0,0,0},
{0,0,0,0,0,299,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,300,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,301,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,302,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,303,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,304,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,305,0,0,0,306,0,307,308,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,309,0,0,0,310,311,0,312,0,0,0,0,313,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,314,0,315,0,0,316,317,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,318,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{319,0,0,0,0,0,0,0,320,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,321,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,322,323,0,324,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,325,0,0,0,0,0,0,0,0,0,0,0,0,0,326,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,327,328,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,329,0,0,0,0,0,0,0,0,0,0,0,0,330,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,331,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,332,0,0,0,333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,334,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,335,0,0,0,0,336,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,337,0,0,0,0,0,338,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
typedef struct {void (*run)(void);unsigned cycles;} compact_instruction;
static const compact_instruction compact_instructions[]={ {0,0},
{m68k_op_andi_8_d,56},
{m68k_op_andi_8_d,56},
{m68k_op_andi_8_d,56},
{m68k_op_andi_8_di,140},
{m68k_op_andi_8_al,168},
{m68k_op_andi_16_d,56},
{m68k_op_andi_16_d,56},
{m68k_op_andi_16_d,56},
{m68k_op_andi_16_d,56},
{m68k_op_andi_32_d,112},
{m68k_op_andi_32_d,112},
{m68k_op_andi_32_d,112},
{m68k_op_subi_8_d,56},
{m68k_op_subi_8_d,56},
{m68k_op_subi_8_d,56},
{m68k_op_addi_8_d,56},
{m68k_op_addi_8_d,56},
{m68k_op_addi_16_d,56},
{m68k_op_bclr_8_r_al,140},
{m68k_op_bset_8_r_al,140},
{m68k_op_btst_32_s_d,70},
{m68k_op_btst_32_s_d,70},
{m68k_op_btst_32_s_d,70},
{m68k_op_btst_8_s_ai,84},
{m68k_op_btst_8_s_di,112},
{m68k_op_btst_8_s_al,140},
{m68k_op_bchg_32_s_d,70},
{m68k_op_bclr_8_s_di,140},
{m68k_op_bclr_8_s_al,168},
{m68k_op_bset_8_s_di,140},
{m68k_op_bset_8_s_al,168},
{m68k_op_cmpi_8_d,56},
{m68k_op_cmpi_8_d,56},
{m68k_op_cmpi_8_d,56},
{m68k_op_cmpi_8_d,56},
{m68k_op_cmpi_8_d,56},
{m68k_op_cmpi_8_ai,84},
{m68k_op_cmpi_8_di,112},
{m68k_op_cmpi_8_al,140},
{m68k_op_cmpi_16_d,56},
{m68k_op_cmpi_16_d,56},
{m68k_op_cmpi_16_d,56},
{m68k_op_cmpi_16_al,140},
{m68k_op_move_8_d_d,28},
{m68k_op_move_8_d_ai,56},
{m68k_op_move_8_d_pi,56},
{m68k_op_move_8_d_pi,56},
{m68k_op_move_8_d_di,84},
{m68k_op_move_8_d_ix,98},
{m68k_op_move_8_d_ix,98},
{m68k_op_move_8_d_al,112},
{m68k_op_move_8_d_pcix,98},
{m68k_op_move_8_d_i,56},
{m68k_op_move_8_ai_d,56},
{m68k_op_move_8_ix_d,98},
{m68k_op_move_8_d_d,28},
{m68k_op_move_8_d_pi,56},
{m68k_op_move_8_d_ix,98},
{m68k_op_move_8_di_d,84},
{m68k_op_move_8_di_d,84},
{m68k_op_move_8_di_d,84},
{m68k_op_move_8_di_d,84},
{m68k_op_move_8_di_d,84},
{m68k_op_move_8_di_ai,112},
{m68k_op_move_8_di_pi,112},
{m68k_op_move_8_di_di,140},
{m68k_op_move_8_di_al,168},
{m68k_op_move_8_di_i,112},
{m68k_op_move_8_al_d,112},
{m68k_op_move_8_al_d,112},
{m68k_op_move_8_al_d,112},
{m68k_op_move_8_al_d,112},
{m68k_op_move_8_al_d,112},
{m68k_op_move_8_al_ix,182},
{m68k_op_move_8_al_al,196},
{m68k_op_move_8_al_i,140},
{m68k_op_move_8_d_ai,56},
{m68k_op_move_8_d_pi,56},
{m68k_op_move_8_d_di,84},
{m68k_op_move_8_d_al,112},
{m68k_op_move_8_ai_d,56},
{m68k_op_move_8_di_d,84},
{m68k_op_move_8_d_pi,56},
{m68k_op_move_8_d_i,56},
{m68k_op_move_8_ai_d,56},
{m68k_op_move_8_di_d,84},
{m68k_op_move_8_di_d,84},
{m68k_op_move_8_d_d,28},
{m68k_op_move_8_d_pi,56},
{m68k_op_move_8_d_ix,98},
{m68k_op_move_8_d_al,112},
{m68k_op_move_8_d_pcix,98},
{m68k_op_move_8_ai_d,56},
{m68k_op_move_8_ai_pi,84},
{m68k_op_move_8_d_d,28},
{m68k_op_move_8_d_d,28},
{m68k_op_move_8_d_ai,56},
{m68k_op_move_8_d_di,84},
{m68k_op_move_8_d_ix,98},
{m68k_op_move_8_d_ix,98},
{m68k_op_move_8_d_pcix,98},
{m68k_op_move_8_d_i,56},
{m68k_op_move_8_d_d,28},
{m68k_op_move_8_d_d,28},
{m68k_op_move_8_d_ai,56},
{m68k_op_move_8_d_pi,56},
{m68k_op_move_8_d_pi,56},
{m68k_op_move_8_d_di,84},
{m68k_op_move_8_d_al,112},
{m68k_op_move_8_d_pcix,98},
{m68k_op_move_8_d_i,56},
{m68k_op_move_8_pi_d,56},
{m68k_op_move_8_pi_pi,84},
{m68k_op_move_8_d_d,28},
{m68k_op_move_8_d_al,112},
{m68k_op_move_8_d_pcix,98},
{m68k_op_move_32_d_d,28},
{m68k_op_movea_32_d,28},
{m68k_op_movea_32_di,112},
{m68k_op_move_32_d_pi,84},
{m68k_op_move_32_di_d,112},
{m68k_op_move_32_di_a,112},
{m68k_op_move_32_al_a,140},
{m68k_op_move_32_al_a,140},
{m68k_op_move_32_al_i,196},
{m68k_op_movea_32_a,28},
{m68k_op_movea_32_a,28},
{m68k_op_movea_32_ai,84},
{m68k_op_movea_32_i,84},
{m68k_op_movea_32_ai,84},
{m68k_op_movea_32_al,140},
{m68k_op_movea_32_i,84},
{m68k_op_move_32_d_d,28},
{m68k_op_move_32_d_a,28},
{m68k_op_move_32_d_pi,84},
{m68k_op_movea_32_ai,84},
{m68k_op_movea_32_al,140},
{m68k_op_movea_32_pcix,126},
{m68k_op_movea_32_i,84},
{m68k_op_move_32_ai_a,84},
{m68k_op_move_32_pi_pi,140},
{m68k_op_move_16_d_di,84},
{m68k_op_move_16_d_ix,98},
{m68k_op_move_16_d_aw,84},
{m68k_op_move_16_d_i,56},
{m68k_op_move_16_d_di,84},
{m68k_op_move_16_d_al,112},
{m68k_op_move_16_d_i,56},
{m68k_op_move_16_di_d,84},
{m68k_op_move_16_di_d,84},
{m68k_op_move_16_di_d,84},
{m68k_op_move_16_di_d,84},
{m68k_op_move_16_di_i,112},
{m68k_op_move_16_al_d,112},
{m68k_op_move_16_al_ai,140},
{m68k_op_move_16_al_i,140},
{m68k_op_move_16_d_al,112},
{m68k_op_move_16_d_d,28},
{m68k_op_move_16_d_al,112},
{m68k_op_move_16_d_d,28},
{m68k_op_move_16_d_di,84},
{m68k_op_move_16_d_pcix,98},
{m68k_op_move_16_d_i,56},
{m68k_op_move_16_d_i,56},
{m68k_op_lea_32_al,84},
{m68k_op_clr_8_di,112},
{m68k_op_clr_8_al,140},
{m68k_op_clr_16_di,112},
{m68k_op_lea_32_al,84},
{m68k_op_neg_16_d,28},
{m68k_op_lea_32_al,84},
{m68k_op_lea_32_al,84},
{m68k_op_ext_16,28},
{m68k_op_ext_16,28},
{m68k_op_ext_16,28},
{m68k_op_movem_32_re_pd,56},
{m68k_op_lea_32_al,84},
{m68k_op_tst_8_d,28},
{m68k_op_tst_8_d,28},
{m68k_op_tst_8_d,28},
{m68k_op_tst_8_di,84},
{m68k_op_tst_8_al,112},
{m68k_op_tst_16_d,28},
{m68k_op_lea_32_al,84},
{m68k_op_movem_32_er_pi,84},
{m68k_op_lea_32_al,84},
{m68k_op_rts_32,112},
{m68k_op_jsr_32_ai,112},
{m68k_op_jsr_32_al,140},
{m68k_op_jsr_32_pcdi,126},
{m68k_op_jmp_32_pcdi,70},
{m68k_op_dbf_16,84},
{m68k_op_dbf_16,84},
{m68k_op_dbf_16,84},
{m68k_op_dbf_16,84},
{m68k_op_dbf_16,84},
{m68k_op_addq_8_d,28},
{m68k_op_addq_8_d,28},
{m68k_op_addq_8_di,112},
{m68k_op_addq_8_al,140},
{m68k_op_addq_16_d,28},
{m68k_op_addq_16_a,56},
{m68k_op_addq_32_a,56},
{m68k_op_addq_32_a,56},
{m68k_op_subq_8_d,28},
{m68k_op_subq_8_di,112},
{m68k_op_subq_32_d,56},
{m68k_op_addq_32_a,56},
{m68k_op_subq_32_a,56},
{m68k_op_addq_32_a,56},
{m68k_op_addq_8_d,28},
{m68k_op_addq_16_d,28},
{m68k_op_addq_32_di,168},
{m68k_op_subq_8_d,28},
{m68k_op_subq_32_d,56},
{m68k_op_subq_32_a,56},
{m68k_op_bra_16,70},
{m68k_op_bra_8,70},
{m68k_op_bra_8,70},
{m68k_op_bra_8,70},
{m68k_op_bra_8,70},
{m68k_op_bra_8,70},
{m68k_op_bls_16,70},
{m68k_op_bcc_16,70},
{m68k_op_bcc_8,70},
{m68k_op_bcs_16,70},
{m68k_op_bcs_8,70},
{m68k_op_bcs_8,70},
{m68k_op_bcs_8,70},
{m68k_op_bcs_8,70},
{m68k_op_bne_16,70},
{m68k_op_bne_8,70},
{m68k_op_beq_16,70},
{m68k_op_beq_8,70},
{m68k_op_beq_8,70},
{m68k_op_beq_8,70},
{m68k_op_bpl_16,70},
{m68k_op_bpl_8,70},
{m68k_op_bmi_16,70},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_moveq_32,28},
{m68k_op_or_8_er_al,112},
{m68k_op_divs_16_d,0},
{m68k_op_or_16_er_d,28},
{m68k_op_or_8_er_d,28},
{m68k_op_or_8_re_al,140},
{m68k_op_or_8_re_al,140},
{m68k_op_sub_16_er_d,28},
{m68k_op_sub_16_er_d,28},
{m68k_op_sub_16_er_di,84},
{m68k_op_suba_32_i,112},
{m68k_op_sub_8_er_d,28},
{m68k_op_sub_16_er_d,28},
{m68k_op_sub_16_er_di,84},
{m68k_op_cmp_8_di,84},
{m68k_op_cmp_8_al,112},
{m68k_op_cmp_16_al,112},
{m68k_op_cmp_8_al,112},
{m68k_op_cmp_8_ai,56},
{m68k_op_cmp_8_di,84},
{m68k_op_cmp_8_al,112},
{m68k_op_mulu_16_d,0},
{m68k_op_and_16_er_di,84},
{m68k_op_mulu_16_d,0},
{m68k_op_mulu_16_d,0},
{m68k_op_mulu_16_d,0},
{m68k_op_add_8_er_di,84},
{m68k_op_add_16_er_d,28},
{m68k_op_add_16_er_d,28},
{m68k_op_add_16_er_di,84},
{m68k_op_adda_16_d,56},
{m68k_op_adda_32_d,56},
{m68k_op_add_16_re_di,112},
{m68k_op_adda_32_d,56},
{m68k_op_adda_32_i,112},
{m68k_op_add_16_re_di,112},
{m68k_op_add_8_er_d,28},
{m68k_op_adda_32_d,56},
{m68k_op_adda_32_i,112},
{m68k_op_add_16_er_d,28},
{m68k_op_add_16_er_d,28},
{m68k_op_adda_16_d,56},
{m68k_op_adda_32_i,112},
{m68k_op_add_8_er_d,28},
{m68k_op_add_8_er_di,84},
{m68k_op_add_16_er_d,28},
{m68k_op_add_32_er_ai,98},
{m68k_op_add_8_re_di,112},
{m68k_op_add_16_re_di,112},
{m68k_op_adda_32_d,56},
{m68k_op_adda_32_d,56},
{m68k_op_adda_32_d,56},
{m68k_op_adda_32_d,56},
{m68k_op_asr_16_s,42},
{m68k_op_asr_16_s,42},
{m68k_op_asr_16_s,42},
{m68k_op_lsr_16_s,42},
{m68k_op_lsr_16_s,42},
{m68k_op_asl_16_s,42},
{m68k_op_asl_16_s,42},
{m68k_op_asl_16_s,42},
{m68k_op_asl_16_s,42},
{m68k_op_lsl_32_s,56},
{m68k_op_asr_8_s,42},
{m68k_op_lsr_8_s,42},
{m68k_op_lsr_16_s,42},
{m68k_op_asl_8_s,42},
{m68k_op_asl_8_s,42},
{m68k_op_lsl_8_s,42},
{m68k_op_asr_16_s,42},
{m68k_op_lsr_16_s,42},
{m68k_op_lsl_8_s,42},
{m68k_op_lsl_8_s,42},
{m68k_op_asl_16_s,42},
{m68k_op_lsl_16_s,42},
{m68k_op_lsl_8_s,42},
{m68k_op_asl_16_s,42},
{m68k_op_asl_16_s,42},
{m68k_op_lsr_8_s,42},
{m68k_op_asl_8_s,42},
{m68k_op_asl_8_s,42},
{m68k_op_lsr_16_s,42},
{m68k_op_lsr_16_s,42}};
static unsigned compact_index(unsigned opcode){
 if(opcode>=65536){fm_compact_fault=opcode+1;return 0;}
 unsigned index=compact_lookup[compact_pages[opcode>>8]][opcode&255];
 if(!index)fm_compact_fault=opcode+1;return index;
}
static unsigned compact_cycles(unsigned opcode){return compact_instructions[compact_index(opcode)].cycles;}
static unsigned compact_execute(unsigned opcode){
 unsigned index=compact_index(opcode);if(!index)return 0;
 const compact_instruction *instruction=compact_instructions+index;
 instruction->run();return instruction->cycles;
}
static void compact_dispatch(unsigned opcode){(void)compact_execute(opcode);}
