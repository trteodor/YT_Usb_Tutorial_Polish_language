#ifndef _USB_OTG_FIFO_H
#define _USB_OTG_FIFO_H 1

#include <stdint.h>

#define ALL_RX_FIFOS
#define ALL_TX_FIFOS  16

int FlushTxFifo(int);
int FlushRxFifo(void);
void WriteFifo8(int, uint8_t const *, uint32_t);
void WriteFifo32(int, uint32_t const *, uint32_t);
void ReadFifo8(int, uint8_t *, uint32_t);
void ReadFifo32(int, uint32_t *, uint32_t);
uint32_t GetDeviceFreeTxFifoSpace(int);

#endif
