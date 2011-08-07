/*******************************************************************************
 * This file is part of OpenWSN, the Open Wireless Sensor Network Platform.
 *
 * Copyright (C) 2005-2010 zhangwei(TongJi University)
 *
 * OpenWSN is a free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 or (at your option) any later version.
 *
 * OpenWSN is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * For non-opensource or commercial applications, please choose commercial license.
 * Refer to OpenWSN site http://code.google.com/p/openwsn/ for more detail.
 *
 * For other questions, you can contact the author through email openwsn#gmail.com
 * or the mailing address: Dr. Wei Zhang, Dept. of Control, Dianxin Hall, TongJi
 * University, 4800 Caoan Road, Shanghai, China. Zip: 201804
 *
 ******************************************************************************/

#include "svc_configall.h"
#include "svc_foundation.h"
#ifdef CONFIG_DYNA_MEMORY
  #include <stdlib.h>
#endif
#include "../rtl/rtl_iobuf.h"
#include "../rtl/rtl_slipfilter.h"
#include "../rtl/rtl_frame.h"
#include "../hal/opennode2010/hal_uart.h"
#include "svc_sio_acceptor.h"

static void _sac_txbuf_to_device( TiSioAcceptor * sac );
static void _sac_device_to_rxbuf( TiSioAcceptor * sac );

/*
char txbuf_block[IOBUF_HOPESIZE(CONFIG_SIOACCEPTOR_TXBUF_CAPACITY)];
char rxbuf_block[IOBUF_HOPESIZE(CONFIG_SIOACCEPTOR_RXBUF_CAPACITY)];
#ifdef SIO_ACCEPTOR_SLIP_ENABLE
char tmpbuf_block[IOBUF_HOPESIZE(CONFIG_SIOACCEPTOR_TMPBUF_CAPACITY)];
char mtmpbuf_block[ IOBUF_HOPESIZE(CONFIG_SIOACCEPTOR_TXBUF_CAPACITY)];
#endif
*/



#ifdef CONFIG_DYNA_MEMORY
TiSioAcceptor * sac_create( TiSioAcceptor * sac, TiUartAdapter * uart )
{
	TiSioAcceptor * sac;
	uint32 memsize = SIO_ACCEPTOR_MEMSIZE(0);
	
	sac = (TiSioAcceptor *)malloc( memsize );
	if (sac != NULL)
	{
		sac = sac_open( sac, memsize, uart );
	}
	return sac;
}
#endif

#ifdef CONFIG_DYNA_MEMORY
void sac_free( TiSioAcceptor * sac )
{
	if (sac != NULL)
	{
		sac_close(sac);
		sac_free(sac);
	}
}
#endif

TiSioAcceptor * sac_construct( char * buf, uint16 size )//todo for testing
{
    hal_assert( sizeof(TiSioAcceptor) <= size );
    memset( buf, 0x00, size );
    return (TiSioAcceptor *)buf;
}

TiSioAcceptor * sac_open( TiSioAcceptor * sac, TiSlipFilter *slip, TiUartAdapter * uart )//TiSioAcceptor * sac_open( TiSioAcceptor * sac, uint32 memsize,TiSlipFilter *slip, TiUartAdapter * uart )
{
	//rtl_assert( memsize <= SIO_ACCEPTOR_MEMSIZE(0) );
	
	if (sac != NULL)
	{
		//memset( sac, 0x00, memsize );
		sac->state = 0;
		sac->device = uart;
		//sac->txbuf = iobuf_construct(( void *)(&txbuf_block), IOBUF_HOPESIZE(CONFIG_SIOACCEPTOR_TXBUF_CAPACITY) );
		//sac->rxbuf = iobuf_construct( (void *)(&rxbuf_block), IOBUF_HOPESIZE(CONFIG_SIOACCEPTOR_RXBUF_CAPACITY) );
		#ifdef SIO_ACCEPTOR_SLIP_ENABLE
		sac->rx_accepted = false;
        sac->slipfilter = slip;
		//sac->tmpbuf = iobuf_construct( (void *)(&tmpbuf_block), IOBUF_HOPESIZE(CONFIG_SIOACCEPTOR_TMPBUF_CAPACITY) );
		#endif
	}

    return sac;

}

void sac_close( TiSioAcceptor * sac )
{
	sac = sac;
	return;
}

/**
 * Send a frame through serial communication through the TiSioAcceptor object. 
 */
uintx sac_send( TiSioAcceptor * sac, TiFrame * buf, uintx option )
{
	uintx count=0;
	#ifdef SIO_ACCEPTOR_SLIP_ENABLE
	TiIoBuf * tmpbuf;
	#endif

    hal_assert( sac != NULL );

	/* @attention
	 * @warning
	 * The following assumes io->txbuf can accept all data inputed from parameter "buf". 
	 * If not, then the frame data will be truncated.
	 */
	#ifdef SIO_ACCEPTOR_SLIP_ENABLE
	if (iobuf_empty(sac->txbuf))
	{
		//tmpbuf = iobuf_construct((void *)(&mtmpbuf_block), IOBUF_HOPESIZE(CONFIG_SIOACCEPTOR_TXBUF_CAPACITY) );
		iobuf_write(sac->tmpbuf, frame_startptr(buf), frame_length(buf));
		count = slip_filter_tx_handler( sac->slipfilter, sac->tmpbuf, sac->txbuf );
		iobuf_clear(sac->tmpbuf);
	}
	#endif

	#ifndef SIO_ACCEPTOR_SLIP_ENABLE

	if (iobuf_empty(sac->txbuf))
	{
		count = iobuf_write(sac->txbuf, frame_startptr(buf), frame_length(buf));
	}
    
	#endif

	sac_evolve(sac,NULL);

	return count;
}

/**
 * Retrieve the first frame pending inside the TiSioAcceptor object.
 */
uintx sac_recv( TiSioAcceptor * sac, TiFrame * buf, uintx option )
{
	uintx count=0;
	
    hal_assert( sac != NULL );
	
	#ifdef SIO_ACCEPTOR_SLIP_ENABLE
	if (sac->rx_accepted)
	{
		hal_assert(iobuf_length(sac->rxbuf) > 0);
		if (iobuf_length(sac->rxbuf) > 0)
		{
			//count = frame_read(buf, iobuf_ptr(sac->rxbuf), iobuf_length(sac->rxbuf));
            count = frame_write(buf, iobuf_ptr(sac->rxbuf), iobuf_length(sac->rxbuf));//Ӧ����write�ɣ�
			iobuf_clear(sac->rxbuf);
		}
		sac->rx_accepted = 0;
	}
	#endif

	#ifndef SIO_ACCEPTOR_SLIP_ENABLE
	if (iobuf_length(sac->rxbuf) > 0)
	{
		//count = frame_read(buf, iobuf_ptr(sac->rxbuf), iobuf_length(sac->rxbuf));
        count = frame_write(buf, iobuf_ptr(sac->rxbuf), iobuf_length(sac->rxbuf));
		iobuf_clear(sac->rxbuf);
	}
	#endif

	sac_evolve(sac,NULL);

	return count;
}

void sac_evolve( TiSioAcceptor * sac, TiEvent * event )
{
	_sac_txbuf_to_device( sac );
	 _sac_device_to_rxbuf( sac );
	return;
}

void _sac_txbuf_to_device( TiSioAcceptor * sac )
{
    uintx count=0;
	
	/* If there're data in io->txbuf, then try to send it through sac->device. */
	if (!iobuf_empty(sac->txbuf))
	{
		//count = io->device->write(iobuf_ptr(io->rxbuf), iobuf_length(io->rxbuf));
        count = uart_write(sac->device, iobuf_ptr(sac->txbuf), iobuf_length(sac->txbuf), 0x00);
		iobuf_popfront(sac->txbuf, count);
	}
}

void _sac_device_to_rxbuf( TiSioAcceptor * sac )
{
    int count;

    hal_assert( sac != NULL );

	/* If io->rxbuf is empty, then try to retrieve data from the device adapter(io->device). */

	/* If framing is enabled, then do framing here */
	#ifdef SIO_ACCEPTOR_SLIP_ENABLE
	if (!iobuf_full(sac->rmpbuf))
	{
		// @attention
		// @todo
		// The following process depends on read(). Attention here read() should be 
		// an asynchronious call, however, i'm not sure how the current read() behaves.

		/* Read some data into sio->tmpbuf first and do framing on this buffer. The frame
		 * found will be placed into io->rxbuf by the framing process.
		 */
        count = uart_read( sac->device, iobuf_endptr(sac->rmpbuf), iobuf_available(sac->rmpbuf), 0x00);
       
		iobuf_adjustlength( sac->rmpbuf, count );
	}

	if ((sac->rx_accepted == 0) && (!iobuf_empty(sac->rmpbuf)))
	{
		/**
         * @attention 
		 * If sac->rxbuf is full, then we have no better idea but to drop the current frame.
		 * This is done by clear the rxbuf and restart the framing process.
		 * However, this should NOT happen if you can guarantee the rxbuf is large
		 * enough and can accept the largest possible frame.
		 */
		if (iobuf_full(sac->rxbuf))
		{
			iobuf_clear(sac->rxbuf);
		}
		/* slip_filter_rx_handler return a positive value means an frame found. The frame
		 * is placed in io->rxbuf 
		 */
		if (slip_filter_rx_handler(sac->slipfilter, sac->rmpbuf, sac->rxbuf) > 0)
		{
			/* set the rx_accept flag to indicate an entire frame is successfully identified
			 * and be placed inside io->rxbuf. */
			sac->rx_accepted = 1;
            iobuf_clear( sac->rmpbuf);//todo for testing
		}
	}
	#endif

	#ifndef SIO_ACCEPTOR_SLIP_ENABLE
	if (!iobuf_full(sac->rxbuf))
	{
		/* Q: whether here should be iobuf_endptr(sac->rxbuf) or iobuf_endptr(sac->rxbuf)+1?
		 * seems iobuf_endptr(sac->rxbuf). */		 
        count = uart_read( sac->device, iobuf_endptr(sac->rxbuf), iobuf_available(sac->rxbuf), 0x00);
		iobuf_adjustlength( sac->rxbuf, count );
	}
	#endif	
}












