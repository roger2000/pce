/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:     src/ibmpc/hgc.c                                            *
 * Created:       2003-08-19 by Hampa Hug <hampa@hampa.ch>                   *
 * Last modified: 2003-08-31 by Hampa Hug <hampa@hampa.ch>                   *
 * Copyright:     (C) 2003 by Hampa Hug <hampa@hampa.ch>                     *
 *****************************************************************************/

/*****************************************************************************
 * This program is free software. You can redistribute it and / or modify it *
 * under the terms of the GNU General Public License version 2 as  published *
 * by the Free Software Foundation.                                          *
 *                                                                           *
 * This program is distributed in the hope  that  it  will  be  useful,  but *
 * WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  General *
 * Public License for more details.                                          *
 *****************************************************************************/

/* $Id: hgc.c,v 1.8 2003/08/31 01:38:11 hampa Exp $ */


#include <stdio.h>

#include "pce.h"


static
unsigned char coltab[16] = {
 0x00, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
};

static unsigned hgclines1[348];
static unsigned hgclines2[348];


video_t *hgc_new (terminal_t *trm, ini_sct_t *sct)
{
  unsigned      i, n;
  unsigned char r, g, b;
  unsigned      iobase, membase, memsize;
  hgc_t         *hgc;

  hgc = (hgc_t *) malloc (sizeof (hgc_t));
  if (hgc == NULL) {
    return (NULL);
  }

  pce_video_init (&hgc->vid);

  hgc->vid.ext = hgc;
  hgc->vid.del = (pce_video_del_f) &hgc_del;
  hgc->vid.get_mem = (pce_video_get_mem_f) &hgc_get_mem;
  hgc->vid.get_reg = (pce_video_get_reg_f) &hgc_get_reg;
  hgc->vid.prt_state = (pce_video_prt_state_f) &hgc_prt_state;
  hgc->vid.screenshot = (pce_video_screenshot_f) &hgc_screenshot;

  for (i = 0; i < 16; i++) {
    hgc->crtc_reg[i] = 0;
  }

  ini_get_ulng (sct, "color7", &hgc->rgb_fg, 0xe89050);
  ini_get_ulng (sct, "color15", &hgc->rgb_hi, 0xfff0c8);

  ini_get_uint (sct, "io", &iobase, 0x3b4);
  ini_get_uint (sct, "membase", &membase, 0xb0000);
  ini_get_uint (sct, "memsize", &memsize, 65536);

  memsize = (memsize < 32768) ? 32768 : 65536;

  pce_log (MSG_INF, "video:\tHGC io=0x%04x membase=0x%05x memsize=0x%05x\n",
    iobase, membase, memsize
  );

  hgc->mem = mem_blk_new (membase, memsize, 1);
  hgc->mem->ext = hgc;
  hgc->mem->set_uint8 = (seta_uint8_f) &hgc_mem_set_uint8;
  hgc->mem->set_uint16 = (seta_uint16_f) &hgc_mem_set_uint16;
  mem_blk_init (hgc->mem, 0x00);

  hgc->reg = mem_blk_new (iobase, 16, 1);
  hgc->reg->ext = hgc;
  hgc->reg->set_uint8 = (seta_uint8_f) &hgc_reg_set_uint8;
  hgc->reg->set_uint16 = (seta_uint16_f) &hgc_reg_set_uint16;
  hgc->reg->get_uint8 = (geta_uint8_f) &hgc_reg_get_uint8;
  hgc->reg->get_uint16 = (geta_uint16_f) &hgc_reg_get_uint16;
  mem_blk_init (hgc->reg, 0x00);

  hgc->trm = trm;

  hgc->crtc_pos = 0;
  hgc->crtc_ofs = 0;

  hgc->enable_page1 = 0;
  hgc->enable_graph = 0;

  hgc->crs_on = 1;

  hgc->mode = 0;

  r = (hgc->rgb_fg >> 16) & 0xff;
  g = (hgc->rgb_fg >> 8) & 0xff;
  b = hgc->rgb_fg & 0xff;
  trm_set_map (trm, 7, r | (r << 8), g | (g << 8), b | (b << 8));

  r = (hgc->rgb_hi >> 16) & 0xff;
  g = (hgc->rgb_hi >> 8) & 0xff;
  b = hgc->rgb_hi & 0xff;
  trm_set_map (trm, 15, r | (r << 8), g | (g << 8), b | (b << 8));

  trm_set_size (trm, TERM_MODE_TEXT, 80, 25);

  n = 0;
  for (i = 0; i < 348; i++) {
    n += 540;
    hgclines1[i] = n / 348;
    hgclines2[i] = (i > 0) ? (hgclines2[i - 1] + hgclines1[i - 1]) : 0;
    n = n % 348;
  }

  return (&hgc->vid);
}

void hgc_del (hgc_t *hgc)
{
  if (hgc != NULL) {
    mem_blk_del (hgc->mem);
    mem_blk_del (hgc->reg);
    free (hgc);
  }
}

void hgc_clock (hgc_t *hgc)
{
}

void hgc_prt_state (hgc_t *hgc, FILE *fp)
{
  unsigned i;
  unsigned x, y;

  if (hgc->crtc_pos < hgc->crtc_ofs) {
    x = 0;
    y = 0;
  }
  else {
    x = (hgc->crtc_pos - hgc->crtc_ofs) % 80;
    y = (hgc->crtc_pos - hgc->crtc_ofs) / 80;
  }

  fprintf (fp, "HGC: MODE=%u  PAGE=%04X  OFS=%04X  POS=%04X[%u/%u]  CRS=%s\n",
    hgc->mode, hgc->page_ofs, hgc->crtc_ofs, hgc->crtc_pos, x, y,
    (hgc->crs_on) ? "ON" : "OFF"
  );

  fprintf (fp, "REG: 3B4=%02X  3B5=%02X  3B8=%02X  3BA=%02X  3BF=%02X\n",
    hgc->reg->data[0], hgc->reg->data[1], hgc->reg->data[4],
    hgc->reg->data[6], hgc->reg->data[11]
  );

  fprintf (fp, "CRTC=[%02X", hgc->crtc_reg[0]);
  for (i = 1; i < 18; i++) {
    if ((i & 7) == 0) {
      fputs ("-", fp);
    }
    else {
      fputs (" ", fp);
    }
    fprintf (fp, "%02X", hgc->crtc_reg[i]);
  }
  fputs ("]\n", fp);

  fflush (fp);
}

mem_blk_t *hgc_get_mem (hgc_t *hgc)
{
  return (hgc->mem);
}

mem_blk_t *hgc_get_reg (hgc_t *hgc)
{
  return (hgc->reg);
}


/*****************************************************************************
 * mode 0 (text 80 * 25 * 2)
 *****************************************************************************/

static
int hgc_mode0_screenshot (hgc_t *hgc, FILE *fp)
{
  unsigned i;
  unsigned x, y;

  i = (hgc->page_ofs + (hgc->crtc_ofs << 1)) & 0x7fff;

  for (y = 0; y < 25; y++) {
    for (x = 0; x < 80; x++) {
      fputc (hgc->mem->data[i], fp);
      i = (i + 2) & 0x7fff;
    }

    fputs ("\n", fp);
  }

  return (0);
}

static
void hgc_mode0_update (hgc_t *hgc)
{
  unsigned i;
  unsigned x, y;
  unsigned fg, bg;

  i = (hgc->page_ofs + (hgc->crtc_ofs << 1)) & 0x7fff;

  for (y = 0; y < 25; y++) {
    for (x = 0; x < 80; x++) {
      fg = hgc->mem->data[i + 1] & 0x0f;
      bg = (hgc->mem->data[i + 1] & 0xf0) >> 4;

      fg = coltab[fg];
      bg = coltab[bg];

      trm_set_col (hgc->trm, fg, bg);
      trm_set_chr (hgc->trm, x, y, hgc->mem->data[i]);

      i = (i + 2) & 0x7fff;
    }
  }
}

static
void hgc_mode0_set_uint8 (hgc_t *hgc, unsigned long addr, unsigned char val)
{
  unsigned      x, y;
  unsigned char c, a;
  unsigned      fg, bg;

  if (hgc->mem->data[addr] == val) {
    return;
  }

  hgc->mem->data[addr] = val;

  if (addr & 1) {
    c = hgc->mem->data[addr - 1];
    a = val;
  }
  else {
    c = val;
    a = hgc->mem->data[addr + 1];
  }

  if (addr < (hgc->crtc_ofs << 1)) {
    return;
  }

  addr -= (hgc->crtc_ofs << 1);

  if (addr >= 4000) {
    return;
  }

  x = (addr >> 1) % 80;
  y = (addr >> 1) / 80;

  fg = coltab[a & 0x0f];
  bg = coltab[(a & 0xf0) >> 4];

  trm_set_col (hgc->trm, fg, bg);
  trm_set_chr (hgc->trm, x, y, c);
}

static
void hgc_mode0_set_uint16 (hgc_t *hgc, unsigned long addr, unsigned short val)
{
  unsigned      x, y;
  unsigned char c, a;
  unsigned      fg, bg;

  if (addr & 1) {
    hgc_mem_set_uint8 (hgc, addr, val & 0xff);

    if (addr < hgc->mem->end) {
      hgc_mem_set_uint8 (hgc, addr + 1, val >> 8);
    }

    return;
  }

  c = val & 0xff;
  a = (val >> 8) & 0xff;

  if ((hgc->mem->data[addr] == c) && (hgc->mem->data[addr + 1] == a)) {
    return;
  }

  hgc->mem->data[addr] = c;
  hgc->mem->data[addr + 1] = a;

  if (addr < (hgc->crtc_ofs << 1)) {
    return;
  }

  addr -= (hgc->crtc_ofs << 1);

  if (addr >= 4000) {
    return;
  }

  x = (addr >> 1) % 80;
  y = (addr >> 1) / 80;

  fg = coltab[a & 0x0f];
  bg = coltab[(a & 0xf0) >> 4];

  trm_set_col (hgc->trm, fg, bg);
  trm_set_chr (hgc->trm, x, y, c);
}


/*****************************************************************************
 * mode 1 (graphics 720 * 348 * 2)
 *****************************************************************************/

static
int hgc_mode1_screenshot (hgc_t *hgc, FILE *fp)
{
  unsigned      x, y, i;
  unsigned      val;
  unsigned char *mem;

  fprintf (fp, "P5\n720 348\n255 ");

  for (y = 0; y < 348; y++) {
    mem = (hgc->page_ofs) ? (hgc->mem->data + 32768) : hgc->mem->data;
    mem += (y & 3) * 8192;
    mem += 90 * (y / 4);

    for (x = 0; x < 90; x++) {
      val = mem[x];

      for (i = 0; i < 8; i++) {
        if (val & (0x80 >> i)) {
          fputc (255, fp);
        }
        else {
          fputc (0, fp);
        }
      }
    }
  }

  return (0);
}

static
void hgc_mode1_update (hgc_t *hgc)
{
  unsigned      x, y, i, j;
  unsigned      val[4];
  unsigned      sx;
  unsigned      sy[4], sn[4];
  unsigned char *mem[4];

  mem[0] = hgc->mem->data;

  if (hgc->page_ofs) {
    mem[0] += 32768;
  }

  mem[1] = mem[0] + 1 * 8192;
  mem[2] = mem[0] + 2 * 8192;
  mem[3] = mem[0] + 3 * 8192;

  for (y = 0; y < 87; y++) {
    for (j = 0; j < 4; j++) {
      sy[j] = hgclines2[4 * y + j];
      sn[j] = hgclines1[4 * y + j];
    }

    sx = 0;

    for (x = 0; x < 90; x++) {
      val[0] = mem[0][x];
      val[1] = mem[1][x];
      val[2] = mem[2][x];
      val[3] = mem[3][x];

      for (i = 0; i < 8; i++) {
        for (j = 0; j < 4; j++) {
          trm_set_col (hgc->trm, (val[j] & 0x80) ? 7 : 0, 0);
          trm_set_pxl (hgc->trm, sx, sy[j]);
          if (sn[j] > 1) {
            trm_set_pxl (hgc->trm, sx, sy[j] + 1);
          }
          val[j] <<= 1;
        }
        sx += 1;
      }
    }

    mem[0] += 90;
    mem[1] += 90;
    mem[2] += 90;
    mem[3] += 90;
  }
}

static
void hgc_mode1_set_uint8 (hgc_t *hgc, unsigned long addr, unsigned char val)
{
  unsigned      i;
  unsigned      x, y, sx, sy;
  unsigned      bank;
  unsigned char old;

  old = hgc->mem->data[addr];

  if (old == val) {
    return;
  }

  hgc->mem->data[addr] = val;

  if (addr > 32767) {
    addr -= 32768;
  }

  bank = addr / 8192;

  x = 8 * ((addr & 8191) % 90);
  y = 4 * ((addr & 8191) / 90) + bank;

  sx = x;
  sy = hgclines2[y];

  for (i = 0; i < 8; i++) {
    if ((old ^ val) & 0x80) {
      trm_set_col (hgc->trm, (val & 0x80) ? 7 : 0, 0);
      trm_set_pxl (hgc->trm, sx, sy);
      if (hgclines1[y] > 1) {
        trm_set_pxl (hgc->trm, sx, sy + 1);
      }
    }

    old <<= 1;
    val <<= 1;
    sx += 1;
  }
}


int hgc_screenshot (hgc_t *hgc, FILE *fp, unsigned mode)
{
  if ((hgc->mode == 1) && ((mode == 2) || (mode == 0))) {
    return (hgc_mode1_screenshot (hgc, fp));
  }
  else if ((hgc->mode == 0) && ((mode == 1) || (mode == 0))) {
    return (hgc_mode0_screenshot (hgc, fp));
  }

  return (1);
}

void hgc_update (hgc_t *hgc)
{
  switch (hgc->mode) {
    case 0:
      hgc_mode0_update (hgc);
      break;

    case 1:
      hgc_mode1_update (hgc);
      break;
  }
}

static
void hgc_set_pos (hgc_t *hgc, unsigned pos)
{
  hgc->crtc_pos = pos;

  if (hgc->mode == 0) {
    if (pos < hgc->crtc_ofs) {
      return;
    }

    pos -= hgc->crtc_ofs;

    if (pos >= 2000) {
      return;
    }

    trm_set_pos (hgc->trm, pos % 80, pos / 80);
  }
}

static
void hgc_set_crs (hgc_t *hgc, unsigned y1, unsigned y2)
{
  if (hgc->mode == 0) {
    if (y1 < y2) {
      y1 = 0;
      y2 = 13;
    }

    y1 = (y1 <= 13) ? (13 - y1) : 0;
    y2 = (y2 <= 13) ? (13 - y2) : 0;

    y1 = (255 * y1 + 6) / 13;
    y2 = (255 * y2 + 6) / 13;

    trm_set_crs (hgc->trm, y1, y2);
  }
}

static
void hgc_set_page_ofs (hgc_t *hgc, unsigned ofs)
{
  if (hgc->crtc_ofs == ofs) {
    return;
  }

  hgc->crtc_ofs = ofs;

  if (hgc->mode == 0) {
    hgc_update (hgc);
  }
}

static
void hgc_set_config (hgc_t *hgc, unsigned char val)
{
  hgc->enable_graph = ((val & 0x01) != 0);
  hgc->enable_page1 = ((val & 0x02) != 0);
}

static
void hgc_set_mode (hgc_t *hgc, unsigned char mode)
{
  unsigned newmode, newofs;

  if (hgc->enable_graph == 0) {
    mode &= ~0x02;
  }

  if ((hgc->enable_page1 == 0) || (mem_blk_get_size (hgc->mem) < 65536)) {
    mode &= ~0x80;
  }

  newmode = (mode & 0x02) ? 1 : 0;
  newofs = (mode & 0x80) ? 32768 : 0;

  if ((newmode == hgc->mode) && (newofs == hgc->page_ofs)) {
    return;
  }

  if (newmode != hgc->mode) {
    hgc->mode = newmode;

    switch (newmode) {
      case 0:
        trm_set_size (hgc->trm, TERM_MODE_TEXT, 80, 25);
        break;

      case 1:
        trm_set_size (hgc->trm, TERM_MODE_GRAPH, 720, 540);
        break;
    }
  }

  if (newofs != hgc->page_ofs) {
    hgc->page_ofs = newofs;
  }

  hgc_update (hgc);
}

void hgc_mem_set_uint8 (hgc_t *hgc, unsigned long addr, unsigned char val)
{
  switch (hgc->mode) {
    case 0:
      hgc_mode0_set_uint8 (hgc, addr, val);
      break;

    case 1:
      hgc_mode1_set_uint8 (hgc, addr, val);
      break;
  }
}

void hgc_mem_set_uint16 (hgc_t *hgc, unsigned long addr, unsigned short val)
{
  switch (hgc->mode) {
    case 0:
      hgc_mode0_set_uint16 (hgc, addr, val);
      break;

    case 1:
      hgc_mode1_set_uint8 (hgc, addr, val);
      if ((addr + 1) < hgc->mem->size) {
        hgc_mode1_set_uint8 (hgc, addr + 1, val >> 8);
      }
      break;
  }
}

static
void hgc_crtc_set_reg (hgc_t *hgc, unsigned reg, unsigned char val)
{
  if (reg > 15) {
    return;
  }

  hgc->crtc_reg[reg] = val;

  switch (reg) {
    case 0x0a:
    case 0x0b:
      hgc_set_crs (hgc, hgc->crtc_reg[0x0b], hgc->crtc_reg[0x0a]);
      break;

    case 0x0c:
      hgc_set_page_ofs (hgc, (hgc->crtc_reg[0x0c] << 8) | val);
      break;

    case 0x0d:
      hgc_set_page_ofs (hgc, (hgc->crtc_reg[0x0c] << 8) | val);
      break;

    case 0x0e:
//      hgc_set_pos (hgc, (val << 8) | (hgc->crtc_reg[0x0f] & 0xff));
      break;

    case 0x0f:
      hgc_set_pos (hgc, (hgc->crtc_reg[0x0e] << 8) | val);
      break;
  }
}

static
unsigned char hgc_crtc_get_reg (hgc_t *hgc, unsigned reg)
{
  if (reg > 15) {
    return (0xff);
  }

  return (hgc->crtc_reg[reg]);
}

void hgc_reg_set_uint8 (hgc_t *hgc, unsigned long addr, unsigned char val)
{
  hgc->reg->data[addr] = val;

  switch (addr) {
    case 0x01:
      hgc_crtc_set_reg (hgc, hgc->reg->data[0], val);
      break;

    case 0x04:
      hgc_set_mode (hgc, val);
      break;

    case 0x0b:
      hgc_set_config (hgc, val);
      break;
  }
}

void hgc_reg_set_uint16 (hgc_t *hgc, unsigned long addr, unsigned short val)
{
  hgc_reg_set_uint8 (hgc, addr, val & 0xff);

  if ((addr + 1) < hgc->reg->size) {
    hgc_reg_set_uint8 (hgc, addr + 1, val >> 8);
  }
}

unsigned char hgc_reg_get_uint8 (hgc_t *hgc, unsigned long addr)
{
  static unsigned cnt = 0;

  switch (addr) {
    case 0x00:
      return (hgc->reg->data[0]);

    case 0x01:
      return (hgc_crtc_get_reg (hgc, hgc->reg->data[0]));

    case 0x06:
      cnt += 1;
      if ((cnt & 7) == 0) {
        hgc->reg->data[6] ^= 0x01;
      }
      if (cnt >= 64) {
        cnt = 0;
        hgc->reg->data[6] ^= 0x80;
      }

      return (hgc->reg->data[6]);

    default:
      return (0xff);
  }
}

unsigned short hgc_reg_get_uint16 (hgc_t *hgc, unsigned long addr)
{
  unsigned short ret;

  ret = hgc_reg_get_uint8 (hgc, addr);

  if ((addr + 1) < hgc->reg->size) {
    ret |= hgc_reg_get_uint8 (hgc, addr + 1) << 8;
  }

  return (ret);
}