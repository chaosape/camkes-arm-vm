/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <platsupport/serial.h>
#include <camkes.h>

#define MUTEX_OP(X) assert(X==0)
#define SEM_OP(X) assert(X==0)
#define INTR_OP(X) assert(X==0)
#define ENTRY //fprintf(stderr,"Enter %s\n",__FUNCTION__);
#define EXIT //fprintf(stderr,"Exit %s\n",__FUNCTION__);

#define BAUD_RATE 115200
//#define BAUD_RATE 57600

void hexDump(char *desc, void *addr, int len) 
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf("  %s\n", buff);

            // Output the offset.
            printf("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf(" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
            buff[i % 16] = '.';
        } else {
            buff[i % 16] = pc[i];
        }

        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf("  %s\n", buff);
}

static ps_chardevice_t serial_device;

#define WRITE_PKTBUF_SIZE 1024
static UARTPacket writepktbuf[WRITE_PKTBUF_SIZE];
static uint32_t writepktbuffront = 0;
static uint32_t writepktbuflength = 0;
static bool can_write = true;

static void write_callback(ps_chardevice_t* device,
                           enum chardev_status stat,
                           size_t bytes_transferred,
                           void* token) {
  bool send = false;
  UARTPacket pkt;
  UARTPacket * pktp = (UARTPacket*)token;
  ENTRY;
  if(bytes_transferred < pktp->buf_len) {
    uint32_t remaining = pktp->buf_len - bytes_transferred;
    printf("UARTSHIM: Did not complete last send, %u bytes remain.\n",remaining);
    pkt = *pktp;
    memcpy(pktp->buf,pkt.buf+bytes_transferred,remaining);
    pktp->buf_len = remaining;
    assert(ps_cdev_write(&serial_device,
			 writepktbuf[writepktbuffront].buf,
			 writepktbuf[writepktbuffront].buf_len,
			 write_callback,
			 pktp) == 0);
    return;
  }
  
  free(token);
  MUTEX_OP(write_pktbuf_mutex_lock());
  /*printf("UARTSHIM: dequeue (front=%u,length=%u,packet size=%u).\n"
	 ,writepktbuffront
	 ,writepktbuflength
	 ,writepktbuf[writepktbuffront].buf_len);*/

  writepktbuffront = (writepktbuffront + 1) % WRITE_PKTBUF_SIZE;
  writepktbuflength--;
  pkt = writepktbuf[writepktbuffront];
  send = writepktbuflength > 0;
  MUTEX_OP(write_pktbuf_mutex_unlock());


  if(send) {
    //for(int i = 0; i < 100000000; i++) {}
    pktp = (UARTPacket*)malloc(sizeof(UARTPacket));
    *pktp = pkt;
    //hexDump("Sending:\n",
    //	    pktp->buf,
    //	    pktp->buf_len);
    assert(ps_cdev_write(&serial_device,
			 pktp->buf,
			 pktp->buf_len,
			 write_callback,
			 pktp) == 0);
  }
  EXIT;
}


bool incoming_uartwrite(const UARTPacket * packet) {
  bool send = false;
  bool result = true;
  uint32_t idx;
  UARTPacket pkt;
  
  ENTRY;
  MUTEX_OP(write_pktbuf_mutex_lock());
  if(writepktbuflength < WRITE_PKTBUF_SIZE) {
    idx = (writepktbuffront+writepktbuflength) % WRITE_PKTBUF_SIZE;
    writepktbuf[idx] = *packet;
    pkt = *packet;
    send = writepktbuflength == 0;
    writepktbuflength++;
    //printf("UARTSHIM: enqueue (front=%u,length=%u).\n",writepktbuffront,writepktbuflength); 
  } else {
    result = false;
  }
  MUTEX_OP(write_pktbuf_mutex_unlock());
  if(send) {
    UARTPacket * pktp = (UARTPacket*)malloc(sizeof(UARTPacket));
    *pktp = pkt;
    MUTEX_OP(device_lock());
    //hexDump("Sending:\n",
    //	    pktp->buf,
    //	    pktp->buf_len);
    assert(ps_cdev_write(&serial_device,
			 pktp->buf,
			 pktp->buf_len,
			 write_callback,
			 pktp) == 0);
    MUTEX_OP(device_unlock());  
  }
  EXIT;
  return result;
}



void pre_init(void)
{
    ps_io_ops_t uart_shim_io_ops;
    if (camkes_io_ops(&uart_shim_io_ops) != 0) {
        printf("UART Shim: Failed to get IO-ops.\n");
        return;
    }

    clkcar_uart_clk_init(NV_UARTB_ASYNC);
    ps_chardevice_t *result = ps_cdev_init(NV_UARTB_ASYNC, &uart_shim_io_ops, &serial_device);

    if (result == NULL) {
        printf("UART Shim: Failed to initialize UART B.\n");
        return;
    }

    serial_device.flags &= ~SERIAL_AUTO_CR;
    serial_configure(&serial_device, BAUD_RATE, 8, PARITY_NONE, 1);
    
    printf("UARTSHIM: initialized.\n");
}


static UARTPacket pkt;

static void read_callback(ps_chardevice_t *device,
			  enum chardev_status stat,
			  size_t bytes_transfered,
			  void *token) {
  pkt.buf_len = bytes_transfered;
  outgoing_uartwrite(&pkt);
  ps_cdev_read(&serial_device, pkt.buf,255, read_callback, NULL);
}

void
interrupt_handle()
{
  /* VERY IMPORTANT: This interrupt and subsequent call to ps_cdev_handle_irq 
     causes callbacks to be serviced. */
  MUTEX_OP(device_lock());
  ps_cdev_handle_irq(&serial_device, 0);
  MUTEX_OP(device_unlock());
  INTR_OP(interrupt_acknowledge());
}


int run(void)
{
  MUTEX_OP(device_lock());
  ps_cdev_read(&serial_device, pkt.buf,255, read_callback, NULL);
  MUTEX_OP(device_unlock());
    
  return 0;
}

