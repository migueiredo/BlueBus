/*
 * File:   log.c
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     Implementation of logging mechanisms that we can use throughout the project
 */
#include "log.h"
#include "uart.h"
#include <stdarg.h>
#include <stdio.h>
#include "../mappings.h"
#include "config.h"
#include "event.h"
#include "timer.h"

/**
 * LogCheckChunk()
 *     Description:
 *        Return the current string offset OR return the max length minus one
 *        if this write will go beyond the end of the array
 *     Params:
 *         int16_t offset - The current string offset
 *         int16_n n - The amount of bytes written
 *         int16_t max - The size of the destination string array
 *     Returns:
 *         int16_t - The next offset
 */
static inline int16_t LogCheckChunk(int16_t offset, int16_t n, int16_t max)
{
    if (n > 0) {
        offset += n;
    }
    return (offset >= max) ? max - 1 : offset;
}


/**
 * LogMessage()
 *     Description:
 *         Send a message over the system UART, for the given log level.
 *         Implicitly adds CRLF
 *     Params:
 *         const char *type
 *         const char *data
 *     Returns:
 *         void
 */
void LogMessage(const char *type, const char *data)
{
    UART_t *debugger = UARTGetModuleHandler(SYSTEM_UART_MODULE);
    if (debugger != 0) {
        char output[LOG_MESSAGE_SIZE] = {0};
        long long unsigned int ts = (long long unsigned int) TimerGetMillis();
        snprintf(output, LOG_MESSAGE_SIZE - 1 , "[%llu] %s: %s\r\n", ts, type, data);
        EventTriggerCallback(UI_EVENT_CLI_WRITE_BEFORE, 0);
        UARTSendString(debugger, output);
        EventTriggerCallback(UI_EVENT_CLI_WRITE_COMPLETE, 0);
    }
}

/**
 * LogRaw()
 *     Description:
 *         Sends the given data over to the debug UART.
 *     Params:
 *         const char *format - The string format
 *         va_args ...
 *     Returns:
 *         void
 */
void LogRaw(const char *format, ...)
{
    UART_t *debugger = UARTGetModuleHandler(SYSTEM_UART_MODULE);
    if (debugger != 0) {
        char buffer[LOG_MESSAGE_SIZE] = {0};
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, LOG_MESSAGE_SIZE - 1, format, args);
        va_end(args);
        EventTriggerCallback(UI_EVENT_CLI_WRITE_BEFORE, 0);
        UARTSendString(debugger, buffer);
        EventTriggerCallback(UI_EVENT_CLI_WRITE_COMPLETE, 0);
    }
}

/**
 * LogDebug()
 *     Description:
 *         Send a debug message over the system UART
 *     Params:
 *         uint8_t source - The source system
 *         const char *format
 *         va_args ...
 *     Returns:
 *         void
 */
void LogDebug(uint8_t source, const char *format, ...)
{
    unsigned char canLog = ConfigGetLog(source);
    if (canLog != 0) {
        char buffer[LOG_MESSAGE_SIZE] = {0};
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, LOG_MESSAGE_SIZE - 1, format, args);
        va_end(args);
        LogMessage("DEBUG", buffer);
    }
}

/**
 * LogDebugByteArray()
 *     Description:
 *         Sends a debug message containing an array to the system UART
 *     Params:
 *         uint8_t source - The source system
 *         uint8_t *data - Pointer to the byte array
 *         uint16_t length - Number of bytes to dump
 *         const char *suffix - Text appended after hex bytes, or NULL
 *         const char *format - Printf-style format for the label
 *         va_args ...
 *     Returns:
 *         void
 */
void LogDebugByteArray(
    uint8_t source,
    uint8_t *data,
    uint16_t length,
    const char *suffix,
    const char *format,
    ...
) {
    UART_t *debugger = UARTGetModuleHandler(SYSTEM_UART_MODULE);
    uint8_t canLog = ConfigGetLog(source);
    if (debugger == 0 || canLog == 0) {
        return;
    }
    char buffer[LOG_MESSAGE_SIZE] = {0};
    long long unsigned int ts = (long long unsigned int)TimerGetMillis();
    int16_t offset = LogCheckChunk(
        0,
        snprintf(buffer, LOG_MESSAGE_SIZE, "[%llu] ", ts),
        LOG_MESSAGE_SIZE
    );
    va_list args;
    va_start(args, format);
    offset = LogCheckChunk(
        offset,
        vsnprintf(buffer + offset, LOG_MESSAGE_SIZE - offset, format, args),
        LOG_MESSAGE_SIZE
    );
    va_end(args);
    offset = LogCheckChunk(
        offset,
        snprintf(buffer + offset, LOG_MESSAGE_SIZE - offset, ": "),
        LOG_MESSAGE_SIZE
    );
    uint16_t i = 0;
    for (i = 0; i < length && offset < LOG_MESSAGE_SIZE - LOG_HEX_BYTE_SIZE; i++) {
        offset += snprintf(buffer + offset, LOG_MESSAGE_SIZE - offset, "%02X ", data[i]);
    }
    if (suffix != 0) {
        offset = LogCheckChunk(
            offset,
            snprintf(buffer + offset, LOG_MESSAGE_SIZE - offset, "%s", suffix),
            LOG_MESSAGE_SIZE
        );
    }
    snprintf(buffer + offset, LOG_MESSAGE_SIZE - offset, "\r\n");
    EventTriggerCallback(UI_EVENT_CLI_WRITE_BEFORE, 0);
    UARTSendString(debugger, buffer);
    EventTriggerCallback(UI_EVENT_CLI_WRITE_COMPLETE, 0);
}

/**
 * LogError()
 *     Description:
 *         Send an error message over the system UART
 *         va_args ...
 *     Params:
 *         const char *format
 *         va_args ...
 *     Returns:
 *         void
 */
void LogError(const char *format, ...)
{
    char buffer[LOG_MESSAGE_SIZE] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, LOG_MESSAGE_SIZE - 1, format, args);
    va_end(args);
    LogMessage("ERROR", buffer);
}

/**
 * LogInfo()
 *     Description:
 *         Send an info message over the system UART
 *         va_args ...
 *     Params:
 *         uint8_t source - The source system
 *         const char *format
 *         va_args ...
 *     Returns:
 *         void
 */
void LogInfo(uint8_t source, const char *format, ...)
{
    unsigned char canLog = ConfigGetLog(source);
    if (canLog != 0) {
        char buffer[LOG_MESSAGE_SIZE] = {0};
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, LOG_MESSAGE_SIZE - 1, format, args);
        va_end(args);
        LogMessage("INFO", buffer);
    }
}

/**
 * LogWarning()
 *     Description:
 *         Send a warning message over the system UART
 *     Params:
 *         const char *format
 *         va_args ...
 *     Returns:
 *         void
 */
void LogWarning(const char *format, ...)
{
    char buffer[LOG_MESSAGE_SIZE] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, LOG_MESSAGE_SIZE - 1, format, args);
    va_end(args);
    LogMessage("WARNING", buffer);
}
