/*
 * File:   eeprom.h
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     EEPROM mechanisms
 */
#ifndef EEPROM_H
#define EEPROM_H
#include <xc.h>

/* 16000000 / (2 * (0 + 1)) = 8,000,000 or 8Mhz */
#define EEPROM_BRG 0
// SPI EEPROM Commands
#define EEPROM_COMMAND_GET 0x00
#define EEPROM_COMMAND_WRITE 0x02
#define EEPROM_COMMAND_READ 0x03
#define EEPROM_COMMAND_WRDI 0x04
#define EEPROM_COMMAND_RDSR 0x05
#define EEPROM_COMMAND_WREN 0x06

#define EEPROM_STATUS_BUSY 0x01

// EEPROM Type detection. Set PROBE_FLAG into PROBE_ADDRESS then attempt
// to read it back. This is to allow for more part variety going forward
#define EEPROM_ADDRESS_WIDTH_16 2
#define EEPROM_ADDRESS_WIDTH_24 3
#define EEPROM_PROBE_ADDRESS 0x3FFF
#define EEPROM_PROBE_FLAG 0xBB

void EEPROMInit();
void EEPROMDestroy();
unsigned char EEPROMReadByte(uint32_t);
void EEPROMWriteByte(uint32_t, uint8_t);
#endif /* EEPROM_H */
