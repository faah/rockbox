/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 * Physical interface of the Philips TEA5767 in iriver H100/H300 series
 *
 * Copyright (C) 2002 by Linus Nielsen Feltzing
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "config.h"
#include "cpu.h"
#include "logf.h"
#include "system.h"

#if (CONFIG_TUNER & TEA5767)

/* cute little functions, atomic read-modify-write */
/* SDA is GPIO1,23 */

#ifdef IRIVER_H300_SERIES

/* SDA is GPIO57 */
#define SDA_OUTINIT and_l(~0x02000000, &GPIO1_OUT)
#define SDA_HI_IN   and_l(~0x02000000, &GPIO1_ENABLE)
#define SDA_LO_OUT   or_l( 0x02000000, &GPIO1_ENABLE)
#define SDA              ( 0x02000000 & GPIO1_READ)

/* SCL is GPIO56 */
#define SCL_INPUT  and_l(~0x01000000, &GPIO1_ENABLE)
#define SCL_OUTPUT  or_l( 0x01000000, &GPIO1_ENABLE)
#define SCL_LO     and_l(~0x01000000, &GPIO1_OUT)
#define SCL_HI      or_l( 0x01000000, &GPIO1_OUT)
#define SCL             ( 0x01000000 & GPIO1_READ)

#else

/* SDA is GPIO55 */
#define SDA_OUTINIT and_l(~0x00800000, &GPIO1_OUT)
#define SDA_HI_IN   and_l(~0x00800000, &GPIO1_ENABLE)
#define SDA_LO_OUT   or_l( 0x00800000, &GPIO1_ENABLE)
#define SDA              ( 0x00800000 & GPIO1_READ)

/* SCL is GPIO3 */
#define SCL_INPUT  and_l(~0x00000008, &GPIO_ENABLE)
#define SCL_OUTPUT  or_l( 0x00000008, &GPIO_ENABLE)
#define SCL_LO     and_l(~0x00000008, &GPIO_OUT)
#define SCL_HI      or_l( 0x00000008, &GPIO_OUT)
#define SCL             ( 0x00000008 & GPIO_READ)
#endif

/* delay loop to achieve 400kHz at 120MHz CPU frequency */
#define DELAY   \
    ({                                \
        int _x_;                      \
        asm volatile (                \
            "move.l #21, %[_x_] \r\n" \
        "1:                     \r\n" \
            "subq.l #1, %[_x_]  \r\n" \
            "bhi.b  1b          \r\n" \
            : [_x_]"=&d"(_x_)         \
        );                            \
    })

static void fmradio_i2c_start(void)
{
    SCL_HI;
    SCL_OUTPUT;
    SDA_HI_IN;
    SDA_OUTINIT;
    DELAY;
    SDA_LO_OUT;
    DELAY;
    SCL_LO;
}

static void fmradio_i2c_stop(void)
{
   SDA_LO_OUT;
   DELAY;
   SCL_HI;
   DELAY;
   SDA_HI_IN;
}

/* Generate ACK or NACK */
static void fmradio_i2c_ack(bool nack)
{
    /* Here's the deal. The slave is slow, and sometimes needs to wait
       before it can receive the acknowledge. Therefore it forces the clock
       low until it is ready. We need to poll the clock line until it goes
       high before we release the ack.

       In their infinite wisdom, iriver didn't pull up the SCL line, so
       we have to drive the SCL high repeatedly to simulate a pullup. */
    
    if (nack)
        SDA_HI_IN;
    else
        SDA_LO_OUT;
    DELAY;

    SCL_HI;
    do
    {
        SCL_OUTPUT;  /* Set the clock to output */
        SCL_INPUT;   /* Set the clock to input */
        DELAY;
    }
    while(!SCL);  /* and wait for the slave to release it */

    SCL_OUTPUT;
    SCL_LO;
}

static int fmradio_i2c_getack(void)
{
    int ret = 1;

    /* Here's the deal. The slave is slow, and sometimes needs to wait
       before it can send the acknowledge. Therefore it forces the clock
       low until it is ready. We need to poll the clock line until it goes
       high before we read the ack.

       In their infinite wisdom, iriver didn't pull up the SCL line, so
       we have to drive the SCL high repeatedly to simulate a pullup. */

    SDA_HI_IN;
    DELAY;

    SCL_HI;          /* set clock to high */
    do
    {
        SCL_OUTPUT;  /* Set the clock to output */
        SCL_INPUT;   /* Set the clock to input */
        DELAY;
    }
    while(!SCL);     /* and wait for the slave to release it */

    if (SDA)
        ret = 0;    /* ack failed */
    
    SCL_OUTPUT;
    SCL_LO;

    return ret;
}

static void fmradio_i2c_outb(unsigned char byte)
{
   int i;

   /* clock out each bit, MSB first */
   for ( i=0x80; i; i>>=1 ) {
      if ( i & byte )
         SDA_HI_IN;
      else
         SDA_LO_OUT;
      DELAY;
      SCL_HI;
      DELAY;
      SCL_LO;
   }
}

static unsigned char fmradio_i2c_inb(void)
{
   int i;
   unsigned char byte = 0;

   SDA_HI_IN;
   /* clock in each bit, MSB first */
   for ( i=0x80; i; i>>=1 ) {
       DELAY;
       SCL_HI;
       DELAY;
       if ( SDA )
           byte |= i;
       SCL_LO;
   }

   return byte;
}

int fmradio_i2c_write(int address, const unsigned char* buf, int count)
{
    int i,x=0;

    fmradio_i2c_start();
    fmradio_i2c_outb(address & 0xfe);
    if (fmradio_i2c_getack())
    {
        for (i=0; i<count; i++)
        {
            fmradio_i2c_outb(buf[i]);
            if (!fmradio_i2c_getack())
            {
                x=-2;
                break;
            }
        }
    }
    else
    {
        logf("fmradio_i2c_write() - no ack\n");
        x=-1;
    }
    fmradio_i2c_stop();
    return x;
}

int fmradio_i2c_read(int address, unsigned char* buf, int count)
{
    int i,x=0;
    
    fmradio_i2c_start();
    fmradio_i2c_outb(address | 1);

    if (fmradio_i2c_getack())
    {
        for (i=count; i>0; i--)
        {
            *buf++ = fmradio_i2c_inb();
            fmradio_i2c_ack(i == 1);
        }
    }
    else
        x=-1;
    fmradio_i2c_stop();
    return x;
}

#endif
