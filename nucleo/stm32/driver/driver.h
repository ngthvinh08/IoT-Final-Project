#include "stdint.h"
// Uart drivers
void uart_send (uint8_t *msg, uint16_t len);
void uart_receive (char *buffer, uint16_t size);
