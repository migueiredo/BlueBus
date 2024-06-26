/*
 * File: mid.c
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     Implement the MID UI Mode handler
 */
#include "mid.h"
static MIDContext_t Context;

void MIDInit(BT_t *bt, IBus_t *ibus)
{
    Context.bt = bt;
    Context.ibus = ibus;
    Context.btDeviceIndex = 0;
    Context.mode = MID_MODE_OFF;
    Context.displayUpdate = MID_DISPLAY_NONE;
    Context.mainDisplay = UtilsDisplayValueInit("", MID_DISPLAY_STATUS_OFF);
    Context.tempDisplay = UtilsDisplayValueInit("", MID_DISPLAY_STATUS_OFF);
    Context.modeChangeStatus = MID_MODE_CHANGE_OFF;
    Context.menuContext = MenuSingleLineInit(ibus, bt, &MIDDisplayUpdateText, &Context);
    EventRegisterCallback(
        BT_EVENT_DEVICE_LINK_CONNECTED,
        &MIDBTDeviceConnected,
        &Context
    );
    EventRegisterCallback(
        BT_EVENT_DEVICE_LINK_DISCONNECTED,
        &MIDBTDeviceDisconnected,
        &Context
    );
    EventRegisterCallback(
        BT_EVENT_METADATA_UPDATE,
        &MIDBTMetadataUpdate,
        &Context
    );
    EventRegisterCallback(
        BT_EVENT_PLAYBACK_STATUS_CHANGE,
        &MIDBTPlaybackStatus,
        &Context
    );
    EventRegisterCallback(
        IBUS_EVENT_CDStatusRequest,
        &MIDIBusCDChangerStatus,
        &Context
    );
    EventRegisterCallback(
        IBUS_EVENT_IKEIgnitionStatus,
        &MIDIBusIgnitionStatus,
        &Context
    );
    EventRegisterCallback(
        IBUS_EVENT_MIDButtonPress,
        &MIDIBusMIDButtonPress,
        &Context
    );
    EventRegisterCallback(
        IBUS_EVENT_RADMIDDisplayText,
        &MIDIIBusRADMIDDisplayUpdate,
        &Context
    );
    EventRegisterCallback(
        IBUS_EVENT_MIDModeChange,
        &MIDIBusMIDModeChange,
        &Context
    );
    TimerRegisterScheduledTask(
        &MIDTimerMenuWrite,
        &Context,
        MID_TIMER_MENU_WRITE_INT
    );
    Context.displayUpdateTaskId = TimerRegisterScheduledTask(
        &MIDTimerDisplay,
        &Context,
        MID_TIMER_DISPLAY_INT
    );
}

/**
 * MIDDestroy()
 *     Description:
 *         Unregister all event handlers, scheduled tasks and clear the context
 *     Params:
 *         void
 *     Returns:
 *         void
 */
void MIDDestroy()
{
    EventUnregisterCallback(
        BT_EVENT_DEVICE_LINK_CONNECTED,
        &MIDBTDeviceConnected
    );
    EventUnregisterCallback(
        BT_EVENT_DEVICE_LINK_DISCONNECTED,
        &MIDBTDeviceDisconnected
    );
    EventUnregisterCallback(
        BT_EVENT_METADATA_UPDATE,
        &MIDBTMetadataUpdate
    );
    EventUnregisterCallback(
        BT_EVENT_PLAYBACK_STATUS_CHANGE,
        &MIDBTPlaybackStatus
    );
    EventUnregisterCallback(
        IBUS_EVENT_CDStatusRequest,
        &MIDIBusCDChangerStatus
    );
    EventUnregisterCallback(
        IBUS_EVENT_IKEIgnitionStatus,
        &MIDIBusIgnitionStatus
    );
    EventUnregisterCallback(
        IBUS_EVENT_MIDButtonPress,
        &MIDIBusMIDButtonPress
    );
    EventUnregisterCallback(
        IBUS_EVENT_RADMIDDisplayText,
        &MIDIIBusRADMIDDisplayUpdate
    );
    EventUnregisterCallback(
        IBUS_EVENT_MIDModeChange,
        &MIDIBusMIDModeChange
    );
    TimerUnregisterScheduledTask(&MIDTimerMenuWrite);
    TimerUnregisterScheduledTask(&MIDTimerDisplay);
    memset(&Context, 0, sizeof(MIDContext_t));
}

static void MIDSetMainDisplayText(
    MIDContext_t *context,
    const char *str,
    int8_t timeout
) {
    char text[UTILS_DISPLAY_TEXT_SIZE] = {0};
    snprintf(
        text,
        UTILS_DISPLAY_TEXT_SIZE,
        "%s %s",
        context->mainText,
        str
    );
    memset(context->mainDisplay.text, 0, UTILS_DISPLAY_TEXT_SIZE);
    UtilsStrncpy(context->mainDisplay.text, text, UTILS_DISPLAY_TEXT_SIZE);
    context->mainDisplay.length = strlen(context->mainDisplay.text);
    context->mainDisplay.index = 0;
    TimerTriggerScheduledTask(context->displayUpdateTaskId);
    context->mainDisplay.timeout = timeout;
}

static void MIDSetTempDisplayText(
    MIDContext_t *context,
    char *str,
    int8_t timeout
) {
    char text[UTILS_DISPLAY_TEXT_SIZE] = {0};
    snprintf(
        text,
        UTILS_DISPLAY_TEXT_SIZE,
        "%s %s",
        context->mainText,
        str
    );
    UtilsStrncpy(context->tempDisplay.text, text, UTILS_DISPLAY_TEXT_SIZE);
    context->tempDisplay.length = strlen(context->tempDisplay.text);
    context->tempDisplay.index = 0;
    context->tempDisplay.status = MID_DISPLAY_STATUS_NEW;
    // Unlike the main display, we need to set the timeout beforehand, that way
    // the timer knows how many iterations to display the text for.
    context->tempDisplay.timeout = timeout;
    TimerTriggerScheduledTask(context->displayUpdateTaskId);
}

// Menu Creation
static void MIDShowNextDevice(MIDContext_t *context, uint8_t direction)
{
    if (context->bt->pairedDevicesCount == 0) {
        MIDSetMainDisplayText(context, "No Devices Available", 0);
    } else {
        if (direction == MID_BUTTON_NEXT_VAL) {
            if (context->btDeviceIndex < context->bt->pairedDevicesCount - 1) {
                context->btDeviceIndex++;
            } else {
                context->btDeviceIndex = 0;
            }
        } else {
            if (context->btDeviceIndex == 0) {
                context->btDeviceIndex = context->bt->pairedDevicesCount - 1;
            } else {
                context->btDeviceIndex--;
            }
        }
        BTPairedDevice_t *dev = &context->bt->pairedDevices[context->btDeviceIndex];
        char text[16];
        strncpy(text, dev->deviceName, 15);
        text[15] = '\0';
        // Add a space and asterisks to the end of the device name
        // if it's the currently selected device
        if (memcmp(dev->macId, context->bt->activeDevice.macId, BT_LEN_MAC_ID) == 0) {
            uint8_t startIdx = strlen(text);
            if (startIdx > 13) {
                startIdx = 13;
            }
            text[startIdx++] = 0x20;
            text[startIdx++] = 0x2A;
            text[startIdx++] = 0;
        }
        MIDSetMainDisplayText(context, text, 0);
    }
}


static void MIDMenuDevices(MIDContext_t *context)
{
    context->mode = MID_MODE_DEVICES;
    memset(context->mainText, 0, sizeof(context->mainText));
    context->btDeviceIndex = MID_PAIRING_DEVICE_NONE;
    MIDShowNextDevice(context, 0);
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_ONE_L, "Conn");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_ONE_R, "ect ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TWO_L, " <  ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TWO_R, "  > ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TRE_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TRE_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FOR_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FOR_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FIV_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FIV_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_SIX_L, " Ret");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_SIX_R, "urn ");
}

static void MIDMenuMain(MIDContext_t *context)
{
    context->mode = MID_MODE_ACTIVE;
    strncpy(context->mainText, "Bluetooth", 10);
    MIDSetMainDisplayText(context, "", 0);
    MIDBTMetadataUpdate((void *) context, 0x00);

    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_ONE_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_ONE_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TWO_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TWO_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TRE_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TRE_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FOR_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FOR_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FIV_L, "Sett");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FIV_R, "ings");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_SIX_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_SIX_R, "    ");

    if (context->bt->playbackStatus == BT_AVRCP_STATUS_PLAYING) {
        IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_L, "|| P");
        IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_R, "ause");
    } else {
        IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_L, "|> P");
        IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_R, "lay ");
    }
}

static void MIDMenuSettings(MIDContext_t *context)
{
    context->mode = MID_MODE_SETTINGS;
    memset(context->mainText, 0, sizeof(context->mainText));
    MenuSingleLineSettings(&context->menuContext);
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_ONE_L, "Edit");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_ONE_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TWO_L, " <  ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TWO_R, "  > ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TRE_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TRE_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FOR_L, "Devi");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FOR_R, "ces ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FIV_L, "Pair");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FIV_R, "ing ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_SIX_L, " Ret");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_SIX_R, "urn ");
}

/**
 * MIDDisplayUpdateText()
 *     Description:
 *         Handle updates from the menu driver
 *     Params:
 *         void *ctx - A void pointer to the MIDContext_t struct
 *         char *text - The text to update
 *         int8_t timeout - The timeout for the text
 *         uint8_t updateType - If we are updating the temp or main text
 *     Returns:
 *         void
 */
void MIDDisplayUpdateText(void *ctx, char *text, int8_t timeout, uint8_t updateType)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    if (updateType == MENU_SINGLELINE_DISPLAY_UPDATE_MAIN) {
        MIDSetMainDisplayText(context, text, timeout);
    } else if (updateType == MENU_SINGLELINE_DISPLAY_UPDATE_TEMP) {
        MIDSetTempDisplayText(context, text, timeout);
    }
}

void MIDBTDeviceConnected(void *ctx, unsigned char *tmp)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    if (context->mode == MID_MODE_ACTIVE) {
        memset(context->mainText, 0, sizeof(context->mainText));
        MIDSetMainDisplayText(context, "Bluetooth connected :)", 0);
    }
}

void MIDBTDeviceDisconnected(void *ctx, unsigned char *tmp)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    if (context->mode == MID_MODE_ACTIVE) {
        memset(context->mainText, 0, sizeof(context->mainText));
        MIDSetMainDisplayText(context, "Bluetooth disconnected", 0);
    }
}

void MIDBTMetadataUpdate(void *ctx, unsigned char *tmp)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    if (context->mode == MID_MODE_ACTIVE &&
        strlen(context->bt->title) > 0 &&
        ConfigGetSetting(CONFIG_SETTING_METADATA_MODE) != MID_SETTING_METADATA_MODE_OFF)
    {
        memset(context->mainText, 0, sizeof(context->mainText));
        char text[UTILS_DISPLAY_TEXT_SIZE] = {0};

        uint8_t mid_button = MID_BUTTON_ONE_L;

        char artist[4];

        for (int i = 0; i < 32; i+=4)
        {
            for (int j = 0; j < 4; j++)
            {
                if (strlen(context->bt->artist+i+j) > 0)
                {
                    artist[j] = context->bt->artist[i+j];
                }
                else
                {
                    artist[j] = ' ';
                }
            }
            IBusCommandMIDMenuWriteSingle(context->ibus, mid_button, artist);
            mid_button++;
        }

        snprintf(text, UTILS_DISPLAY_TEXT_SIZE, "%s", context->bt->title);

        MIDSetMainDisplayText(context, text, 3000 / MID_DISPLAY_SCROLL_SPEED);
        TimerTriggerScheduledTask(context->displayUpdateTaskId);
    }
}

void MIDBTPlaybackStatus(void *ctx, unsigned char *tmp)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    if (context->mode == MID_MODE_ACTIVE) {
        if (context->bt->playbackStatus == BT_AVRCP_STATUS_PLAYING) {
            IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_L, "|| P");
            IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_R, "ause");
        } else {
            IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_L, "|> P");
            IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_R, "lay ");
        }
        BTCommandGetMetadata(context->bt);
    }
}

void MIDIBusCDChangerStatus(void *ctx, unsigned char *pkt)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    unsigned char requestedCommand = pkt[4];
    if (requestedCommand == IBUS_CDC_CMD_STOP_PLAYING) {
        if (context->mode != MID_MODE_DISPLAY_OFF &&
            context->mode != MID_MODE_OFF
        ) {
            IBusCommandMIDSetMode(context->ibus, IBUS_DEVICE_TEL, 0x00);
        }
        // Stop Playing
        context->mode = MID_MODE_OFF;
        context->modeChangeStatus = MID_MODE_CHANGE_OFF;
    } else if (requestedCommand == IBUS_CDC_CMD_START_PLAYING) {
        context->modeChangeStatus = MID_MODE_CHANGE_OFF;
        // Start Playing
        if (context->mode == MID_MODE_OFF) {
            IBusCommandMIDSetMode(context->ibus, IBUS_DEVICE_TEL, 0x02);
        }
    } else if (requestedCommand == IBUS_CDC_CMD_CD_CHANGE &&
               context->mode == MID_MODE_DISPLAY_OFF
    ) {
        IBusCommandMIDSetMode(context->ibus, IBUS_DEVICE_TEL, 0x02);
    } else if (requestedCommand == IBUS_CDC_CMD_GET_STATUS) {
        // Reset the UI if the user has chosen not to
        // continue with the mode change
        if (context->mode == MID_MODE_DISPLAY_OFF &&
            context->modeChangeStatus == MID_MODE_CHANGE_RELEASE
        ) {
            context->modeChangeStatus = MID_MODE_CHANGE_OFF;
            IBusCommandMIDSetMode(context->ibus, IBUS_DEVICE_TEL, 0x02);
        }
    }
}

/**
 * MIDIBusIgnitionStatus()
 *     Description:
 *         Ensure we drop the TEL UI when the igintion is turned off
 *         if the display is still active
 *     Params:
 *         void *ctx - A void pointer to the MIDContext_t struct
 *         uint8_t *pkt - A pointer to the ignition status
 *     Returns:
 *         void
 */
void MIDIBusIgnitionStatus(void *ctx, uint8_t *pkt)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    if (pkt[0] == IBUS_IGNITION_OFF &&
        context->mode != MID_MODE_DISPLAY_OFF &&
        context->mode != MID_MODE_OFF
    ) {
        IBusCommandMIDSetMode(context->ibus, IBUS_DEVICE_TEL, 0x00);
        context->mode = MID_MODE_OFF;
        context->modeChangeStatus = MID_MODE_CHANGE_OFF;
    }
}

void MIDIBusMIDButtonPress(void *ctx, unsigned char *pkt)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    unsigned char btnPressed = pkt[6];

    if (context->mode == MID_MODE_ACTIVE) {

        if (btnPressed == MID_BUTTON_PLAY_L || btnPressed == MID_BUTTON_PLAY_R)
        {
            if (context->bt->playbackStatus == BT_AVRCP_STATUS_PLAYING) {
                BTCommandPause(context->bt);
                IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_L, "|> P");
                IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_R, "lay ");
            } else {
                BTCommandPlay(context->bt);
                IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_L, "|| P");
                IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_R, "ause");
            }
        } else if (btnPressed == MID_BUTTON_SETTINGS_L ||
                   btnPressed == MID_BUTTON_SETTINGS_R
        ) {
            context->mode = MID_MODE_SETTINGS_NEW;
        }
    }
    else if (context->mode == MID_MODE_SETTINGS)
    {
        if (btnPressed == MID_BUTTON_RETURN_L || btnPressed == MID_BUTTON_RETURN_R)
        {
            context->mode = MID_MODE_ACTIVE_NEW;
        }
        else if (btnPressed == MID_BUTTON_EDIT_SAVE)
        {
            if (context->menuContext.settingMode == MENU_SINGLELINE_SETTING_MODE_SCROLL_SETTINGS)
            {
                IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_EDIT_SAVE, "Save");
            }
            else
            {
                IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_EDIT_SAVE, "Edit");
            }

            MenuSingleLineSettingsEditSave(&context->menuContext);
        }
        else if (btnPressed == MID_BUTTON_PREV_VAL)
        {
            MenuSingleLineSettingsScroll(&context->menuContext, 0x01);
        }
        else if (btnPressed == MID_BUTTON_PREV_VAL || btnPressed == MID_BUTTON_NEXT_VAL)
        {
            MenuSingleLineSettingsScroll(&context->menuContext, 0x00);
        }
        else if (btnPressed == MID_BUTTON_DEVICES_L || btnPressed == MID_BUTTON_DEVICES_R)
        {
            context->mode = MID_MODE_DEVICES_NEW;
        } 
        else if (btnPressed == MID_BUTTON_PAIR_L || btnPressed == MID_BUTTON_PAIR_R)
        {
            // Toggle the discoverable state
            uint8_t state;
            int8_t timeout = 1500 / MID_DISPLAY_SCROLL_SPEED;
            if (context->bt->discoverable == BT_STATE_ON) {
                MIDSetTempDisplayText(context, "Pairing mode off", timeout);
                state = BT_STATE_OFF;
            } else {
                MIDSetTempDisplayText(context, "Pairing mode on", timeout);
                state = BT_STATE_ON;
                if (context->bt->activeDevice.deviceId != 0) {
                    // To pair a new device, we must disconnect the active one
                    EventTriggerCallback(UIEvent_CloseConnection, 0x00);
                }
            }
            BTCommandSetDiscoverable(context->bt, state);
        }
    }
    else if (context->mode == MID_MODE_DEVICES)
    {
        if (btnPressed == MID_BUTTON_RETURN_L || btnPressed == MID_BUTTON_RETURN_R)
        {
            context->mode = MID_MODE_SETTINGS_NEW;
        }
        else if (btnPressed == MID_BUTTON_CONNECT_L || btnPressed == MID_BUTTON_CONNECT_R)
        {
            if (context->bt->pairedDevicesCount > 0)
            {
                // Connect to device
                BTPairedDevice_t *dev = &context->bt->pairedDevices[context->btDeviceIndex];
                if (memcmp(dev->macId, context->bt->activeDevice.macId, BT_LEN_MAC_ID) != 0 &&
                    dev != 0
                ) {
                    // Trigger device selection event
                    EventTriggerCallback(
                        UIEvent_InitiateConnection,
                        (uint8_t *)&context->btDeviceIndex
                    );
                }
            }
        }
        else if (btnPressed == MID_BUTTON_PREV_VAL || btnPressed == MID_BUTTON_NEXT_VAL)
        {
            MIDShowNextDevice(context, btnPressed);
        }
    }

    // Handle Next and Previous
    if (context->ibus->cdChangerFunction != IBUS_CDC_FUNC_NOT_PLAYING) {
        if (btnPressed == IBus_MID_BTN_TEL_RIGHT_RELEASE) {
            BTCommandPlaybackTrackNext(context->bt);
        } else if (btnPressed == IBus_MID_BTN_TEL_LEFT_RELEASE) {
            BTCommandPlaybackTrackPrevious(context->bt);
        }
    }
}

/**
 * MIDIIBusRADMIDDisplayUpdate()
 *     Description:
 *         Handle the RAD writing to the MID.
 *     Params:
 *         void *context - A void pointer to the MIDContext_t struct
 *         unsigned char *pkt - The IBus packet
 *     Returns:
 *         void
 */
void MIDIIBusRADMIDDisplayUpdate(void *ctx, unsigned char *pkt)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    unsigned char watermark = pkt[pkt[IBUS_PKT_LEN]];
    if (watermark != IBUS_RAD_MAIN_AREA_WATERMARK) {
        if ((context->mode == MID_MODE_ACTIVE ||
             context->mode == MID_MODE_DISPLAY_OFF) &&
            context->modeChangeStatus == MID_MODE_CHANGE_OFF
        ) {
            IBusCommandMIDDisplayRADTitleText(context->ibus, "");
        }
    }
}

/**
 * MIDIBusMIDModeChange()
 *     Description:
 *         
 *     Params:
 *         void *context - A void pointer to the MIDContext_t struct
 *         unsigned char *pkt - The IBus packet
 *     Returns:
 *         void
 */
void MIDIBusMIDModeChange(void *ctx, unsigned char *pkt)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    if (pkt[IBUS_PKT_DB2] == IBUS_MID_UI_TEL_OPEN) {
        if (pkt[IBUS_PKT_DB1] == IBUS_MID_MODE_REQUEST_TYPE_PHYSICAL) {
            if (ConfigGetSetting(CONFIG_SETTING_SELF_PLAY) == CONFIG_SETTING_ON) {
                IBusCommandRADCDCRequest(context->ibus, IBUS_CDC_CMD_START_PLAYING);
            }
        } else {
            context->mode = MID_MODE_ACTIVE_NEW;
        }
    } else if (pkt[IBUS_PKT_DB2] != IBUS_MID_UI_TEL_CLOSE) {
        if (pkt[IBUS_PKT_DB2] == 0x00) {
            if (context->mode != MID_MODE_DISPLAY_OFF &&
                context->mode != MID_MODE_OFF
            ) {
                IBusCommandMIDSetMode(context->ibus, IBUS_DEVICE_TEL, 0x02);
            }
        } else if (context->mode != MID_MODE_OFF) {
            context->mode = MID_MODE_DISPLAY_OFF;
        }
        if (pkt[IBUS_PKT_DB2] == IBUS_MID_UI_RADIO_OPEN &&
            context->modeChangeStatus == MID_MODE_CHANGE_PRESS
        ) {
            IBusCommandMIDButtonPress(context->ibus, IBUS_DEVICE_RAD, MID_BUTTON_MODE_PRESS);
            context->modeChangeStatus = MID_MODE_CHANGE_RELEASE;
        }
    } else {
        // This should be 0x8F, which is "close TEL UI"
        if (ConfigGetSetting(CONFIG_SETTING_SELF_PLAY) == CONFIG_SETTING_ON) {
            IBusCommandRADCDCRequest(context->ibus, IBUS_CDC_CMD_STOP_PLAYING);
        }
    }
}

void MIDTimerMenuWrite(void *ctx)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    switch (context->mode) {
        case MID_MODE_ACTIVE_NEW:
            MIDMenuMain(context);
            break;
        case MID_MODE_SETTINGS_NEW:
            MIDMenuSettings(context);
            break;
        case MID_MODE_DEVICES_NEW:
            MIDMenuDevices(context);
            break;
    }
}

void MIDTimerDisplay(void *ctx)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    if (context->mode == MID_MODE_OFF ||
        context->mode == MID_MODE_DISPLAY_OFF ||
        context->ibus->ignitionStatus == IBUS_IGNITION_OFF
    ) {
        return;
    }
    // Display the temp text, if there is any
    if (context->tempDisplay.status > MID_DISPLAY_STATUS_OFF) {
        if (context->tempDisplay.timeout == 0) {
            context->tempDisplay.status = MID_DISPLAY_STATUS_OFF;
        } else if (context->tempDisplay.timeout > 0) {
            context->tempDisplay.timeout--;
        } else if (context->tempDisplay.timeout < -1) {
            context->tempDisplay.status = MID_DISPLAY_STATUS_OFF;
        }
        if (context->tempDisplay.status == MID_DISPLAY_STATUS_NEW) {
            IBusCommandMIDDisplayText(
                context->ibus,
                context->tempDisplay.text
            );
            context->tempDisplay.status = MID_DISPLAY_STATUS_ON;
        }
        if (context->mainDisplay.length <= IBus_MID_MAX_CHARS) {
            context->mainDisplay.index = 0;
        }
    } else {
        // Display the main text if there isn't a timeout set
        if (context->mainDisplay.timeout > 0) {
            context->mainDisplay.timeout--;
        } else {
            if (context->mainDisplay.length > IBus_MID_MAX_CHARS) {
                char text[IBus_MID_MAX_CHARS + 1] = {0};
                uint8_t textLength = IBus_MID_MAX_CHARS;
                // If we start with a space, it will be ignored by the display
                // Skipping the space allows us to have "smooth" scrolling
                if (context->mainDisplay.text[context->mainDisplay.index] == 0x20 &&
                    context->mainDisplay.index < context->mainDisplay.length
                ) {
                    context->mainDisplay.index++;
                }
                uint8_t idxEnd = context->mainDisplay.index + textLength;
                // Prevent strncpy() from going out of bounds
                if (idxEnd >= context->mainDisplay.length) {
                    textLength = context->mainDisplay.length - context->mainDisplay.index;
                    idxEnd = context->mainDisplay.index + textLength;
                }
                UtilsStrncpy(
                    text,
                    &context->mainDisplay.text[context->mainDisplay.index],
                    textLength + 1
                );
                IBusCommandMIDDisplayText(context->ibus, text);
                // Pause at the beginning of the text
                if (context->mainDisplay.index == 0) {
                    context->mainDisplay.timeout = 5;
                }
                if (idxEnd >= context->mainDisplay.length) {
                    // Pause at the end of the text
                    context->mainDisplay.timeout = 2;
                    context->mainDisplay.index = 0;
                } else {
                    if (ConfigGetSetting(CONFIG_SETTING_METADATA_MODE) ==
                        MID_SETTING_METADATA_MODE_CHUNK
                    ) {
                        context->mainDisplay.timeout = 2;
                        context->mainDisplay.index += IBus_MID_MAX_CHARS;
                    } else {
                        context->mainDisplay.index++;
                    }
                }
            } else {
                if (context->mainDisplay.index == 0) {
                    IBusCommandMIDDisplayText(
                        context->ibus,
                        context->mainDisplay.text
                    );
                }
                context->mainDisplay.index = 1;
            }
        }
    }
}
