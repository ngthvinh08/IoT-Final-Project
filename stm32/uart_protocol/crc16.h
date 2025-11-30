#ifndef __CRC16_H
#define __CRC16_H

#include <stdint.h>
#include <stddef.h>

/**
 * ============================================================================
 * CRC16-CCITT Calculation
 * ============================================================================
 * 
 * CRC Parameters:
 * - Polynomial: 0x1021
 * - Initial Value (Seed): 0xFFFF
 * - Input: Length field (2 bytes) + Data field (8 bytes)
 * - Output: 2 bytes (little-endian)
 */

/**
 * @brief Calculate CRC16-CCITT checksum
 * 
 * @param data      Pointer to data buffer
 * @param length    Length of data in bytes
 * @return          CRC16 value (16-bit)
 * 
 * CRC16 được tính trên: CRC = CRC16(Length[0:2] + Data[0:8])
 */
uint16_t crc16_calculate(const uint8_t* data, size_t length);

#endif /* __CRC16_H */
