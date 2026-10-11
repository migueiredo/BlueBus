/*
 * File:   eeprom.c
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     EEPROM mechanisms
 */
#include "eeprom.h"
#include "../mappings.h"
#include "sfr_setters.h"
#include "utils.h"

// These values constitute the SOA mode for each SPI module
static const uint8_t SPI_SDO_MODES[] = {7, 10, 23};

// These values constitute the SCK mode for each SPI module
static const uint8_t SPI_SCK_MODES[] = {8, 11, 24};

// Detected EEPROM Address width
static uint8_t EEPROM_ADDRESS_WIDTH = EEPROM_ADDRESS_WIDTH_16;

static void EEPROMIsReady();
static void EEPROMProbeAddressWidth();
static uint8_t EEPROMSend(char);

/**
 * EEPROMInit()
 *     Description:
 *         Initialize the EEPROM on SPI1 with the predefined values
 *     Params:
 *         void
 *     Returns:
 *         void
 */
void EEPROMInit()
{
    uint8_t spiModuleIndex = EEPROM_SPI_MODULE - 1;
    EEPROM_CS_IO_MODE = 0;
    EEPROM_CS_PIN = 1;
    // Disable the Module & associated IRQs
    SetSPIIE(spiModuleIndex, 0);
    SetSPITXIE(spiModuleIndex, 0);
    SetSPIRXIE(spiModuleIndex, 0);
    SPI1CON1L = 0;
    SPI1STATLbits.SPIRBF = 0;
    // Unlock the programmable pin register
    __builtin_write_OSCCONL(OSCCON & 0xBF);
    // Data Input
    _SDI1R = EEPROM_SDI_RPIN;
    // Set the SCK Output
    UtilsSetRPORMode(EEPROM_SCK_RPIN, SPI_SCK_MODES[spiModuleIndex]);
    // Set the SDO Output
    UtilsSetRPORMode(EEPROM_SDO_RPIN, SPI_SDO_MODES[spiModuleIndex]);
    // Lock the programmable pin register
    __builtin_write_OSCCONL(OSCCON & 0x40);
    SPI1BRGL = EEPROM_BRG;
    SPI1STATLbits.SPIROV = 0;
    // Enable Module | Set CKE to active -> idle | Master Enable
    SPI1CON1L = 0b1000000100100000;
    EEPROMProbeAddressWidth();
}

/**
 * EEPROMEnableWrite()
 *     Description:
 *         Perform the necessary actions to set up the EEPROM for writing
 *     Params:
 *         void
 *     Returns:
 *         void
 */
static void EEPROMEnableWrite()
{
    EEPROMIsReady();
    EEPROM_CS_PIN = 0;
    EEPROMSend(EEPROM_COMMAND_WREN);
    EEPROM_CS_PIN = 1;
}

/**
 * EEPROMIsReady()
 *     Description:
 *         Check with the EEPROM to see if it's ready to be written to. If it
 *         is not, this function blocks until it is ready (status 0x00).
 *     Params:
 *         void
 *     Returns:
 *         void
 */
static void EEPROMIsReady()
{
    char status = EEPROM_STATUS_BUSY;
    while (status & EEPROM_STATUS_BUSY) {
        EEPROM_CS_PIN = 0;
        EEPROMSend(EEPROM_COMMAND_RDSR);
        status = EEPROMSend(EEPROM_COMMAND_GET);
        EEPROM_CS_PIN = 1;
    }
}

/**
 * EEPROMProbeAddressWidth()
 *     Description:
 *         EEPROM Address Width detection exists because the unit has used multiple
 *         EEPROM sizes, some that require 24-bit addressing and other that
 *         require 16-bit. This works by writing a FLAG byte to the top of
 *         128kB EEPROM space, then reading it back twice. If we have a part
 *         with 24-bit addressing, when the response will NOP and we will not
 *         receive our FLAG value back.
 *     Params:
 *         void
 *     Returns:
 *         void
 */
static void EEPROMProbeAddressWidth()
{
    EEPROM_ADDRESS_WIDTH = EEPROM_ADDRESS_WIDTH_16;
    // The flag persists across boots, so this write only happens once
    if (EEPROMReadByte(EEPROM_PROBE_ADDRESS) != EEPROM_PROBE_FLAG) {
        EEPROMWriteByte(EEPROM_PROBE_ADDRESS, EEPROM_PROBE_FLAG);
    }
    // Probe twice to protect against line noise
    if (
        EEPROMReadByte(EEPROM_PROBE_ADDRESS) != EEPROM_PROBE_FLAG ||
        EEPROMReadByte(EEPROM_PROBE_ADDRESS) != EEPROM_PROBE_FLAG
    ) {
        EEPROM_ADDRESS_WIDTH = EEPROM_ADDRESS_WIDTH_24;
    }
}

/**
 * EEPROMReadByte()
 *     Description:
 *         Read a byte from the EEPROM at the given address and return it
 *     Params:
 *         uint32_t - The memory address of the byte to retrieve
 *     Returns:
 *         uint8_t - The byte at the given address
 */
uint8_t EEPROMReadByte(uint32_t address)
{
    EEPROMIsReady();
    EEPROM_CS_PIN = 0;
    EEPROMSend(EEPROM_COMMAND_READ);
    if (EEPROM_ADDRESS_WIDTH == EEPROM_ADDRESS_WIDTH_24) {
        EEPROMSend(address >> 16 & 0xFF);
    }
    EEPROMSend(address >> 8 & 0xFF);
    EEPROMSend(address & 0xFF);
    // Cast return of EEPROM send to an 8-bit byte, since the returned register
    // is always 16 bits
    uint8_t data = (uint8_t)((uint8_t )EEPROMSend(EEPROM_COMMAND_GET));
    EEPROM_CS_PIN = 1;
    return data;
}

/**
 * EEPROMSend()
 *     Description:
 *         Write to the SPI buffer and return the received value
 *     Params:
 *         char data - The data to transfer to the EEPROM
 *     Returns:
 *         uint8_t - The 8-bit byte returned from the EEPROM
 */
static uint8_t EEPROMSend(char data)
{
    SPI1BUFL = data;
    while (!SPI1STATLbits.SPIRBF);
    return SPI1BUFL;
}

/**
 * EEPROMWriteByte()
 *     Description:
 *         Check with the EEPROM to see if it's ready to be written to. If it
 *         is not, this function blocks until it is ready (status 0x00).
 *     Params:
 *         uint32_t address - The memory address of the byte to retrieve
 *         uint8_t data - The 8-bit byte to write
 *     Returns:
 *         void
 */
void EEPROMWriteByte(uint32_t address, uint8_t data)
{
    EEPROMEnableWrite();
    EEPROM_CS_PIN = 0;
    EEPROMSend(EEPROM_COMMAND_WRITE);
    if (EEPROM_ADDRESS_WIDTH == EEPROM_ADDRESS_WIDTH_24) {
        EEPROMSend(address >> 16 & 0xFF);
    }
    EEPROMSend(address >> 8 & 0xFF);
    EEPROMSend(address & 0xFF);
    EEPROMSend(data);
    EEPROM_CS_PIN = 1;
}

