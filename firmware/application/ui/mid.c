/*
 * File: mid.c
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     Implement the MID UI Mode handler
 */
#include "mid.h"
#include <stdio.h>
#include <string.h>
#include "menu/menu_singleline.h"
#include "../lib/bt/bt_common.h"
#include "../lib/bt.h"
#include "../lib/config.h"
#include "../lib/event.h"
#include "../lib/ibus.h"
#include "../lib/timer.h"
#include "../lib/utils.h"
static MIDContext_t Context;

void MIDInit(BT_t *bt, IBus_t *ibus)
{
    memset(&Context, 0, sizeof(MIDContext_t));
    Context.bt = bt;
    Context.ibus = ibus;
    Context.mode = MID_MODE_OFF;
    Context.displayUpdate = MID_DISPLAY_NONE;
    Context.mainDisplay = UtilsDisplayValueInit("Bluetooth", MID_DISPLAY_STATUS_OFF);
    Context.tempDisplay = UtilsDisplayValueInit("", MID_DISPLAY_STATUS_OFF);
    Context.modeChangeStatus = MID_MODE_CHANGE_OFF;
    MenuSingleLineInit(&Context.menuContext, ibus, bt);
    memset(Context.mainText, 0x00, 16);
    EventRegisterCallback(
        UI_EVENT_MAIN_DISPLAY_UPDATE,
        &MIDUIDisplayUpdateText,
        &Context
    );
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
        IBUS_EVENT_CD_STATUS_REQUEST,
        &MIDIBusCDChangerStatus,
        &Context
    );
    EventRegisterCallback(
        IBUS_EVENT_IKE_IGNITION_STATUS,
        &MIDIBusIgnitionStatus,
        &Context
    );
    EventRegisterCallback(
        IBUS_EVENT_MID_BUTTON_PRESS,
        &MIDIBusMIDButtonPress,
        &Context
    );
    EventRegisterCallback(
        IBUS_EVENT_RAD_MID_DISPLAY_TEXT,
        &MIDIIBusRADMIDDisplayUpdate,
        &Context
    );
    EventRegisterCallback(
        IBUS_EVENT_MID_MODE_CHANGE,
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
        IBUS_EVENT_CD_STATUS_REQUEST,
        &MIDIBusCDChangerStatus
    );
    EventUnregisterCallback(
        IBUS_EVENT_IKE_IGNITION_STATUS,
        &MIDIBusIgnitionStatus
    );
    EventUnregisterCallback(
        IBUS_EVENT_MID_BUTTON_PRESS,
        &MIDIBusMIDButtonPress
    );
    EventUnregisterCallback(
        IBUS_EVENT_RAD_MID_DISPLAY_TEXT,
        &MIDIIBusRADMIDDisplayUpdate
    );
    EventUnregisterCallback(
        IBUS_EVENT_MID_MODE_CHANGE,
        &MIDIBusMIDModeChange
    );
    TimerUnregisterScheduledTask(&MIDTimerMenuWrite);
    TimerUnregisterScheduledTask(&MIDTimerDisplay);
    MenuSingleLineDestory();
    memset(&Context, 0, sizeof(MIDContext_t));
}

static void MIDSetMainDisplayText(
    MIDContext_t *context,
    const char *str,
    int8_t timeout
) {
    char text[UTILS_DISPLAY_TEXT_SIZE] = {0};
    if (strlen(context->mainText) != 0) {
        snprintf(
            text,
            UTILS_DISPLAY_TEXT_SIZE,
            "%s %s",
            context->mainText,
            str
        );
    } else {
        snprintf(
            text,
            UTILS_DISPLAY_TEXT_SIZE,
            "%s",
            str
        );
    }
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
    if (strlen(context->mainText) != 0) {
        snprintf(
            text,
            UTILS_DISPLAY_TEXT_SIZE,
            "%s %s",
            context->mainText,
            str
        );
    } else {
        snprintf(
            text,
            UTILS_DISPLAY_TEXT_SIZE,
            "%s",
            str
        );
    }
    UtilsStrncpy(context->tempDisplay.text, text, UTILS_DISPLAY_TEXT_SIZE);
    context->tempDisplay.length = strlen(context->tempDisplay.text);
    context->tempDisplay.index = 0;
    context->tempDisplay.status = MID_DISPLAY_STATUS_NEW;
    // Unlike the main display, we need to set the timeout beforehand, that way
    // the timer knows how many iterations to display the text for.
    context->tempDisplay.timeout = timeout;
    TimerTriggerScheduledTask(context->displayUpdateTaskId);
}

static void MIDDisplayOBC(MIDContext_t *context)
{
    context->mode = MID_MODE_OBC;
    memset(context->mainText, 0, sizeof(context->mainText));
    MenuSingleLineOBC(&context->menuContext);
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_ONE_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_ONE_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TWO_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TWO_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TRE_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TRE_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FOR_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FOR_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FIV_L, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FIV_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_SIX_L, " Ret");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_SIX_R, "urn ");
}

static void MIDMenuDevices(MIDContext_t *context)
{
    context->mode = MID_MODE_DEVICES;
    memset(context->mainText, 0, sizeof(context->mainText));
    MenuSingleLineDevices(&context->menuContext, 0);
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
    context->displayMetadata = MID_DISPLAY_METADATA_ON;
    memset(context->mainText, 0x00, 16);
    MIDSetMainDisplayText(context, "Bluetooth", 0);

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

    if (context->bt->playbackStatus == BT_AVRCP_STATUS_PLAYING) {
        IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_L, "|| P");
        IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_R, "ause");
    } else {
        IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_L, "|> P");
        IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_R, "lay ");
    }
    // Write metadata last so the artist overwrites the blanked cells
    MIDBTMetadataUpdate((void *) context, 0x00);
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
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TRE_L, "OBC ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_TRE_R, "    ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FOR_L, "Devi");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FOR_R, "ces ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FIV_L, "Pair");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_FIV_R, "ing ");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_SIX_L, " Ret");
    IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_SIX_R, "urn ");
}

/**
 * MIDUIDisplayUpdateText()
 *     Description:
 *         Handle updates from the menu driver
 *     Params:
 *         void *ctx - A void pointer to the MIDContext_t struct
 *         uint8_t *data - The data struct disguised as a pointer
 *     Returns:
 *         void
 */
void MIDUIDisplayUpdateText(void *ctx, uint8_t *data)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    MenuSingleLineDisplayUpdate_t *update = (MenuSingleLineDisplayUpdate_t *) data;
    if (update->type == MENU_SINGLELINE_DISPLAY_UPDATE_MAIN) {
        MIDSetMainDisplayText(context, update->text, update->timeout);
    } else if (update->type == MENU_SINGLELINE_DISPLAY_UPDATE_TEMP) {
        MIDSetTempDisplayText(context, update->text, update->timeout);
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
    if (
        context->mode != MID_MODE_ACTIVE ||
        context->displayMetadata != MID_DISPLAY_METADATA_ON ||
        strlen(context->bt->title) == 0 ||
        ConfigGetSetting(CONFIG_SETTING_METADATA_MODE) == MENU_SINGLELINE_SETTING_METADATA_MODE_OFF
    ) {
        return;
    }
    memset(context->mainText, 0, sizeof(context->mainText));
    // Spread the artist across the first eight menu cells, four chars each
    char artist[IBUS_MID_MENU_MAX_CHARS + 1] = {0};
    size_t artistLength = strlen(context->bt->artist);
    uint8_t midButton = MID_BUTTON_ONE_L;
    uint8_t i;
    for (i = 0; i < 32; i += IBUS_MID_MENU_MAX_CHARS) {
        uint8_t j;
        for (j = 0; j < IBUS_MID_MENU_MAX_CHARS; j++) {
            if (i + j < artistLength) {
                artist[j] = context->bt->artist[i + j];
            } else {
                artist[j] = ' ';
            }
        }
        IBusCommandMIDMenuWriteSingle(context->ibus, midButton, artist);
        midButton++;
    }
    MIDSetMainDisplayText(context, context->bt->title, 3000 / MID_DISPLAY_SCROLL_SPEED);
    TimerTriggerScheduledTask(context->displayUpdateTaskId);
}

void MIDBTPlaybackStatus(void *ctx, unsigned char *tmp)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    if (context->mode != MID_MODE_ACTIVE) {
        return;
    }
    if (context->bt->playbackStatus == BT_AVRCP_STATUS_PLAYING) {
        IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_L, "|| P");
        IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_R, "ause");
        // Redisplay the metadata on unpause, in case it has not changed
        if (strlen(context->bt->title) > 0) {
            MIDBTMetadataUpdate((void *) context, 0x00);
        }
    } else {
        IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_L, "|> P");
        IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_R, "lay ");
    }
    BTCommandGetMetadata(context->bt);
}

void MIDIBusCDChangerStatus(void *ctx, unsigned char *pkt)
{
    MIDContext_t *context = (MIDContext_t *) ctx;
    unsigned char requestedCommand = pkt[IBUS_PKT_DB1];
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
    } else if (
        requestedCommand == IBUS_CDC_CMD_CD_CHANGE &&
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
    } else if (
        requestedCommand == IBUS_CDC_CMD_CHANGE_TRACK ||
        requestedCommand == IBUS_CDC_CMD_CHANGE_TRACK_BLAUPUNKT
    ) {
        if (pkt[IBUS_PKT_DB2] == 0x00) {
            BTCommandPlaybackTrackNext(context->bt);
        } else {
            BTCommandPlaybackTrackPrevious(context->bt);
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
    unsigned char btnPressed = pkt[IBUS_PKT_DB3];
    if (context->mode == MID_MODE_ACTIVE) {
        if (btnPressed == MID_BUTTON_PLAY_L || btnPressed == MID_BUTTON_PLAY_R) {
            if (context->bt->playbackStatus == BT_AVRCP_STATUS_PLAYING) {
                IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_L, "|> P");
                IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_R, "lay ");
            } else {
                IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_L, "|| P");
                IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_PLAY_R, "ause");
            }
            BTCommandPlaybackToggle(context->bt);
        } else if (
            btnPressed == MID_BUTTON_SETTINGS_L ||
            btnPressed == MID_BUTTON_SETTINGS_R
        ) {
            context->mode = MID_MODE_SETTINGS_NEW;
        }
    } else if (context->mode == MID_MODE_SETTINGS) {
        if (btnPressed == MID_BUTTON_RETURN_L || btnPressed == MID_BUTTON_RETURN_R) {
            context->mode = MID_MODE_ACTIVE_NEW;
        } else if (btnPressed == MID_BUTTON_EDIT_SAVE) {
            if (context->menuContext.settingMode == MENU_SINGLELINE_SETTING_MODE_SCROLL_SETTINGS) {
                IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_EDIT_SAVE, "Save");
            } else {
                IBusCommandMIDMenuWriteSingle(context->ibus, MID_BUTTON_EDIT_SAVE, "Edit");
            }
            MenuSingleLineSettingsEditSave(&context->menuContext);
        } else if (
            btnPressed == MID_BUTTON_PREV_VAL ||
            btnPressed == MID_BUTTON_NEXT_VAL
        ) {
            uint8_t direction = 0x00;
            if (btnPressed == MID_BUTTON_PREV_VAL) {
                direction = 0x01;
            }
            MenuSingleLineSettingsScroll(&context->menuContext, direction);
        } else if (btnPressed == MID_BUTTON_OBC_L || btnPressed == MID_BUTTON_OBC_R) {
            context->displayMetadata = MID_DISPLAY_OBC;
            MenuSingleLineSetUIView(&context->menuContext, MENU_SINGLELINE_VIEW_OBC);
            context->mode = MID_MODE_OBC_NEW;
        } else if (
            btnPressed == MID_BUTTON_DEVICES_L ||
            btnPressed == MID_BUTTON_DEVICES_R
        ) {
            context->mode = MID_MODE_DEVICES_NEW;
        } else if (btnPressed == MID_BUTTON_PAIR_L || btnPressed == MID_BUTTON_PAIR_R) {
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
                    EventTriggerCallback(UI_EVENT_CLOSE_CONNECTION, 0x00);
                }
            }
            BTCommandSetDiscoverable(context->bt, state);
        }
    } else if (context->mode == MID_MODE_DEVICES) {
        if (btnPressed == MID_BUTTON_RETURN_L || btnPressed == MID_BUTTON_RETURN_R) {
            context->mode = MID_MODE_SETTINGS_NEW;
        } else if (btnPressed == MID_BUTTON_CONNECT_L || btnPressed == MID_BUTTON_CONNECT_R) {
            MenuSingleLineDevicesConnect(&context->menuContext);
        } else if (
            btnPressed == MID_BUTTON_PREV_VAL ||
            btnPressed == MID_BUTTON_NEXT_VAL
        ) {
            uint8_t direction = MENU_SINGLELINE_DIRECTION_FORWARD;
            if (btnPressed == MID_BUTTON_PREV_VAL) {
                direction = MENU_SINGLELINE_DIRECTION_BACK;
            }
            MenuSingleLineDevices(&context->menuContext, direction);
        }
    } else if (context->mode == MID_MODE_OBC) {
        if (btnPressed == MID_BUTTON_RETURN_L || btnPressed == MID_BUTTON_RETURN_R) {
            context->displayMetadata = MID_DISPLAY_METADATA_ON;
            MenuSingleLineSetUIView(&context->menuContext, MENU_SINGLELINE_VIEW_METADATA);
            context->mode = MID_MODE_SETTINGS_NEW;
        }
    }
    // Handle Next and Previous
    if (context->ibus->cdChangerFunction != IBUS_CDC_FUNC_NOT_PLAYING) {
        if (btnPressed == IBUS_MID_BTN_TEL_RIGHT_RELEASE) {
            BTCommandPlaybackTrackNext(context->bt);
        } else if (btnPressed == IBUS_MID_BTN_TEL_LEFT_RELEASE) {
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
    if (watermark == IBUS_RAD_MAIN_AREA_WATERMARK) {
        return;
    }
    if (
        (context->mode == MID_MODE_ACTIVE || context->mode == MID_MODE_DISPLAY_OFF) &&
        context->modeChangeStatus == MID_MODE_CHANGE_OFF
    ) {
        IBusCommandMIDDisplayRADTitleText(context->ibus, "");
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
        if (
            pkt[IBUS_PKT_DB2] == IBUS_MID_UI_RADIO_OPEN &&
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
        case MID_MODE_OBC_NEW:
            MIDDisplayOBC(context);
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
        if (context->mainDisplay.length <= IBUS_MID_MAX_CHARS) {
            context->mainDisplay.index = 0;
        }
    } else {
        // Display the main text if there isn't a timeout set
        if (context->mainDisplay.timeout > 0) {
            context->mainDisplay.timeout--;
        } else {
            if (context->mainDisplay.length > IBUS_MID_MAX_CHARS) {
                char text[IBUS_MID_MAX_CHARS + 1] = {0};
                uint8_t textLength = IBUS_MID_MAX_CHARS;
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
                    if (
                        ConfigGetSetting(CONFIG_SETTING_METADATA_MODE) ==
                        MENU_SINGLELINE_SETTING_METADATA_MODE_CHUNK
                    ) {
                        context->mainDisplay.timeout = 2;
                        context->mainDisplay.index += IBUS_MID_MAX_CHARS;
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
