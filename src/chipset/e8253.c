/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:     src/chipset/e8253.c                                        *
 * Created:       2001-05-04 by Hampa Hug <hampa@hampa.ch>                   *
 * Last modified: 2003-04-26 by Hampa Hug <hampa@hampa.ch>                   *
 * Copyright:     (C) 2001-2003 by Hampa Hug <hampa@hampa.ch>                *
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

/* $Id: e8253.c,v 1.3 2003/04/26 23:35:24 hampa Exp $ */


#include <stdlib.h>
#include <stdio.h>

#include "e8253.h"


static void cnt_new_val_0 (e8253_counter_t *cnt);
static void cnt_dec_val_0 (e8253_counter_t *cnt, unsigned n);
static void cnt_set_gate_0 (e8253_counter_t *cnt, unsigned char val);
static void cnt_new_val_2 (e8253_counter_t *cnt);
static void cnt_dec_val_2 (e8253_counter_t *cnt, unsigned n);
static void cnt_set_gate_2 (e8253_counter_t *cnt, unsigned char val);
static void cnt_new_val_3 (e8253_counter_t *cnt);
static void cnt_dec_val_3 (e8253_counter_t *cnt, unsigned n);
static void cnt_set_gate_3 (e8253_counter_t *cnt, unsigned char val);


struct cnt_mode_t {
  void (*new_val) (e8253_counter_t *cnt);
  void (*dec_val) (e8253_counter_t *cnt, unsigned n);
  void (*set_gate) (e8253_counter_t *cnt, unsigned char val);
};

static struct cnt_mode_t tab_mode[6] = {
  { &cnt_new_val_0, &cnt_dec_val_0, &cnt_set_gate_0 },
  { NULL, NULL, NULL },
  { &cnt_new_val_2, &cnt_dec_val_2, &cnt_set_gate_2 },
  { &cnt_new_val_3, &cnt_dec_val_3, &cnt_set_gate_3 }
};


static
void cnt_set_out (e8253_counter_t *cnt, unsigned char val)
{
  cnt->out_val = val;

  if (cnt->out != NULL) {
    cnt->out (cnt->out_ext, val);
  }
}

static
void cnt_new_val_0 (e8253_counter_t *cnt)
{
  cnt_set_out (cnt, 0);

  cnt->val = ((unsigned short) cnt->cr[1] << 8) + cnt->cr[0];

  if (cnt->gate) {
    cnt->counting = 1;
  }
}

static
void cnt_dec_val_0 (e8253_counter_t *cnt, unsigned n)
{
  while (n > 0) {
    if (cnt->gate && !cnt->out_val) {
      cnt->val = (cnt->val - 1) & 0xffff;

      if (cnt->val == 0) {
        cnt_set_out (cnt, 1);
      }
    }

    n -= 1;
  }
}

static
void cnt_set_gate_0 (e8253_counter_t *cnt, unsigned char val)
{
}

static
void cnt_new_val_2 (e8253_counter_t *cnt)
{
  cnt_set_out (cnt, 1);

  cnt->val = ((unsigned short) cnt->cr[1] << 8) + cnt->cr[0];

  if (cnt->gate) {
    cnt->counting = 1;
  }
}

static
void cnt_dec_val_2 (e8253_counter_t *cnt, unsigned n)
{
  while (n > 0) {
    if (n < cnt->val) {
      cnt->val -= n;
      n = 0;
    }
    else {
      if (cnt->val > 1) {
        n -= (cnt->val - 1);
        cnt->val = 1;
      }
      else {
        cnt->val = (cnt->val - 1) & 0xffff;
        n -= 1;
      }
    }

    if (cnt->val == 1) {
      cnt_set_out (cnt, 0);
    }
    else if (cnt->val == 0) {
      cnt_new_val_2 (cnt);
    }
  }
}

static
void cnt_set_gate_2 (e8253_counter_t *cnt, unsigned char val)
{
  if (val) {
    if ((cnt->gate == 0) && (cnt->cr_wr == 0)) {
      cnt_new_val_2 (cnt);
      cnt->counting = 1;
    }
  }
  else {
    cnt->counting = 0;
  }
}

static
void cnt_new_val_3 (e8253_counter_t *cnt)
{
  if (!cnt->counting) {
    cnt_set_out (cnt, 1);

    cnt->val = ((unsigned short) cnt->cr[1] << 8) + cnt->cr[0];
    cnt->val &= 0xfffe;

    if (cnt->gate) {
      cnt->counting = 1;
    }
  }
}

static
void cnt_dec_val_3 (e8253_counter_t *cnt, unsigned n)
{
  while (n > 0) {
    if (cnt->val == 0) {
      cnt->val = ((unsigned short) cnt->cr[1] << 8) + cnt->cr[0];
      cnt->val = (cnt->val - 2) & 0xfffe;
      n -= 1;
    }
    else if ((n << 1) <= cnt->val) {
      cnt->val -= n << 1;
      n = 0;
    }
    else {
      n -= cnt->val >> 1;
      cnt->val = 0;
    }

    if (cnt->val == 0) {
      cnt_set_out (cnt, !cnt->out_val);
    }
  }
}

static
void cnt_set_gate_3 (e8253_counter_t *cnt, unsigned char val)
{
  if (val) {
    if ((cnt->gate == 0) && (cnt->cr_wr == 0)) {
      cnt_new_val_3 (cnt);
      cnt->counting = 1;
    }
  }
  else {
    cnt->counting = 0;
  }
}


static
void cnt_new_val (e8253_counter_t *cnt)
{
  if (cnt->mode < 6) {
    if (tab_mode[cnt->mode].new_val != NULL) {
      tab_mode[cnt->mode].new_val (cnt);
    }
  }
}

static
void cnt_dec_val (e8253_counter_t *cnt, unsigned n)
{
  if (cnt->mode < 6) {
    if (tab_mode[cnt->mode].dec_val != NULL) {
      tab_mode[cnt->mode].dec_val (cnt, n);
    }
  }
}

unsigned char e8253_cnt_get_uint8 (e8253_counter_t *cnt)
{
  if (cnt->ol_rd & 1) {
    cnt->ol_rd &= ~1;
    return (cnt->ol[0]);
  }

  if (cnt->ol_rd & 2) {
    cnt->ol_rd &= ~2;
    return (cnt->ol[1]);
  }

  if (cnt->cnt_rd == 0) {
    cnt->cnt_rd = cnt->rw;
  }

  if (cnt->cnt_rd & 1) {
    cnt->cnt_rd &= ~1;
    return (cnt->val & 0xff);
  }

  cnt->cnt_rd &= ~2;

  return ((cnt->val >> 8) & 0xff);
}

void e8253_cnt_set_uint8 (e8253_counter_t *cnt, unsigned char val)
{
  if (cnt->cr_wr == 0) {
    cnt->cr_wr = cnt->rw;
  }

  if (cnt->cr_wr & 1) {
    cnt->cr_wr &= ~1;
    cnt->cr[0] = val;
  }
  else if (cnt->cr_wr & 2) {
    cnt->cr_wr &= ~2;
    cnt->cr[1] = val;
  }

  if (cnt->cr_wr == 0) {
    cnt_new_val (cnt);
  }
}

unsigned char e8253_cnt_get_out (e8253_counter_t *cnt)
{
  return (cnt->out_val);
}

void e8253_cnt_set_gate (e8253_counter_t *cnt, unsigned char val)
{
  if (tab_mode[cnt->mode].set_gate != NULL) {
    tab_mode[cnt->mode].set_gate (cnt, val);
  }

  cnt->gate = val;
}

static
void cnt_latch (e8253_counter_t *cnt)
{
  if (cnt->ol_rd) {
    return;
  }

  cnt->ol_rd = cnt->rw;

  cnt->ol[0] = cnt->val & 0xff;
  cnt->ol[1] = (cnt->val >> 8) & 0xff;
}

static
void cnt_set_control (e8253_counter_t *cnt, unsigned char val)
{
  unsigned short rw;

  if ((val & 0xc0) == 0xc0) {
    /* read back in 8254 */
    return;
  }

  rw = (val >> 4) & 3;

  if (rw == 0) {
    cnt_latch (cnt);
    return;
  }

  cnt->sr = val;

  cnt->cr_wr = rw;
  cnt->ol_rd = 0;
  cnt->cnt_rd = rw;

  cnt->counting = 0;

  cnt->rw = rw;
  cnt->mode = (val >> 1) & 7;
  cnt->bcd = (val & 1);
}

void e8253_counter_clock (e8253_counter_t *cnt, unsigned n)
{
  if (cnt->counting) {
    cnt_dec_val (cnt, n);
  }
}

void e8253_counter_init (e8253_counter_t *cnt)
{
  cnt->cr[0] = 0;
  cnt->cr[1] = 0;
  cnt->cr_wr = 0;

  cnt->ol[0] = 0;
  cnt->ol[1] = 0;
  cnt->ol_rd = 0;
  cnt->cnt_rd = 0;

  cnt->rw = 3;
  cnt->mode = 0;
  cnt->bcd = 0;

  cnt->counting = 0;

  cnt->gate = 0;

  cnt->out_ext = NULL;
  cnt->out_val = 0;
  cnt->out = NULL;

  cnt->val = 0;
}


void e8253_init (e8253_t *pit)
{
  e8253_counter_init (&pit->counter[0]);
  e8253_counter_init (&pit->counter[1]);
  e8253_counter_init (&pit->counter[2]);
}

e8253_t *e8253_new (void)
{
  e8253_t *pit;

  pit = (e8253_t *) malloc (sizeof (e8253_t));
  if (pit == NULL) {
    return (NULL);
  }

  e8253_init (pit);

  return (pit);
}

void e8253_free (e8253_t *pit)
{
}

void e8253_del (e8253_t *pit)
{
  if (pit != NULL) {
    e8253_free (pit);
    free (pit);
  }
}

void e8253_set_gate (e8253_t *pit, unsigned cntr, unsigned char val)
{
  if (cntr <= 2) {
    e8253_cnt_set_gate (&pit->counter[cntr], val);
  }
}

void e8253_set_out (e8253_t *pit, unsigned cntr, void *ext, e8253_set_out_f set)
{
  if (cntr <= 2) {
    pit->counter[cntr].out_ext = ext;
    pit->counter[cntr].out = set;
  }
}

unsigned char e8253_get_uint8 (e8253_t *pit, unsigned long addr)
{
  if (addr < 3) {
    return (e8253_cnt_get_uint8 (&pit->counter[addr]));
  }

  return (0xff);
}

unsigned short e8253_get_uint16 (e8253_t *pit, unsigned long addr)
{
  return (e8253_get_uint8 (pit, addr));
}

void e8253_set_uint8 (e8253_t *pit, unsigned long addr, unsigned char val)
{
  unsigned cnt_i;

  if (addr < 3) {
    e8253_cnt_set_uint8 (&pit->counter[addr], val);
  }
  else if (addr == 3) {
    cnt_i = (val >> 6) & 3;

    if (cnt_i < 3) {
      cnt_set_control (&pit->counter[cnt_i], val);
    }
    else {
      /* Read back for 8254 */
    }
  }
}

void e8253_set_uint16 (e8253_t *pit, unsigned long addr, unsigned short val)
{
  e8253_set_uint8 (pit, addr, val);
}

void e8253_clock (e8253_t *pit, unsigned n)
{
  if (pit->counter[0].counting) {
    cnt_dec_val (&pit->counter[0], n);
  }

  if (pit->counter[1].counting) {
    cnt_dec_val (&pit->counter[1], n);
  }

  if (pit->counter[2].counting) {
    cnt_dec_val (&pit->counter[2], n);
  }
}