#ifndef __UARTPACKET_H__
#define __UARTPACKET_H__

typedef
struct UARTPacket_struct { 
  uint8_t buf[255]  ; 
  int32_t buf_len  ; 
} UARTPacket ; 

#endif /* __UARTPACKET_H__ */
