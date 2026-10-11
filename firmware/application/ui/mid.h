/*
 * File: mid.h
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     Implement the MID UI Mode handler
 */
#ifndef MID_H
#define MID_H
#include "../lib/ibus.h"
#include "../lib/utils.h"
#include "../lib/bt/bt_common.h"
#include "menu/menu_singleline.h"

#define MID_BUTTON_ONE_L 0x40
#define MID_BUTTON_ONE_R 0x41
#define MID_BUTTON_TWO_L 0x42
#define MID_BUTTON_TWO_R 0x43
#define MID_BUTTON_TRE_L 0x44
#define MID_BUTTON_TRE_R 0x45
#define MID_BUTTON_FOR_L 0x46
#define MID_BUTTON_FOR_R 0x47
#define MID_BUTTON_FIV_L 0x48
#define MID_BUTTON_FIV_R 0x49
#define MID_BUTTON_SIX_L 0x4A
#define MID_BUTTON_SIX_R 0x4B

#define MID_BUTTON_PLAY_L MID_BUTTON_SIX_L
#define MID_BUTTON_PLAY_R MID_BUTTON_SIX_R
#define MID_BUTTON_SETTINGS_L MID_BUTTON_FIV_L
#define MID_BUTTON_SETTINGS_R MID_BUTTON_FIV_R

#define MID_BUTTON_CONNECT_L MID_BUTTON_ONE_L
#define MID_BUTTON_CONNECT_R MID_BUTTON_ONE_R

#define MID_BUTTON_EDIT_SAVE MID_BUTTON_ONE_L
#define MID_BUTTON_PREV_VAL MID_BUTTON_TWO_L
#define MID_BUTTON_NEXT_VAL MID_BUTTON_TWO_R
#define MID_BUTTON_OBC_L MID_BUTTON_TRE_L
#define MID_BUTTON_OBC_R MID_BUTTON_TRE_R
#define MID_BUTTON_DEVICES_L MID_BUTTON_FOR_L
#define MID_BUTTON_DEVICES_R MID_BUTTON_FOR_R
#define MID_BUTTON_PAIR_L MID_BUTTON_FIV_L
#define MID_BUTTON_PAIR_R MID_BUTTON_FIV_R
#define MID_BUTTON_RETURN_L MID_BUTTON_SIX_L
#define MID_BUTTON_RETURN_R MID_BUTTON_SIX_R

// The menu row is laid out by the MID as six two-cell pairs. A 0x06 in a
// multi-cell write moves to the next pair, whose text (up to 8 chars) is
// centered unless padded with spaces.
#define MID_MENU_PAIRS 6
#define MID_MENU_PAIR_CHARS 8
#define MID_MENU_PAIR_SEPARATOR 0x06
#define MID_MENU_PAIR_START_IDX 0x40
// The MID ignores frames of 34 bytes or more, so cap the whole I-Bus
// message at 33: 4 bytes of framing and 4 of menu header leave 25 for text
#define MID_MENU_FRAME_MAX_LENGTH 33
#define MID_MENU_FRAME_MAX_DATA (MID_MENU_FRAME_MAX_LENGTH - 8)
#define MID_MENU_PAIR_EDIT_SAVE 0
#define MID_MENU_PAIR_PLAY 5
#define MID_MENU_ARTIST_PAIRS 4

#define MID_BUTTON_MODE_PRESS 0x0B
#define MID_BUTTON_MODE_RELEASE 0x4B

#define MID_DISPLAY_NONE 0
#define MID_DISPLAY_REWRITE 1
#define MID_DISPLAY_REWRITE_NEXT 2
#define MID_DISPLAY_SCROLL_SPEED 750
#define MID_DISPLAY_STATUS_OFF 0
#define MID_DISPLAY_STATUS_ON 1
#define MID_DISPLAY_STATUS_NEW 2
#define MID_DISPLAY_TEXT_SIZE 24
#define MID_TIMER_DISPLAY_INT 500
#define MID_TIMER_MENU_WRITE_INT 250

#define MID_MODE_OFF 0
#define MID_MODE_DISPLAY_OFF 1
#define MID_MODE_ACTIVE 2
#define MID_MODE_SETTINGS 3
#define MID_MODE_DEVICES 4
#define MID_MODE_ACTIVE_NEW 5
#define MID_MODE_SETTINGS_NEW 6
#define MID_MODE_DEVICES_NEW 7
#define MID_MODE_OBC 8
#define MID_MODE_OBC_NEW 9

#define MID_DISPLAY_METADATA_OFF 0
#define MID_DISPLAY_METADATA_ON 1
#define MID_DISPLAY_OBC 2

#define MID_MODE_CHANGE_OFF 0
#define MID_MODE_CHANGE_PRESS 1
#define MID_MODE_CHANGE_RELEASE 2

/*
 * MIDContext_t
 *  This is a struct to hold the context of the MID UI implementation
 *  BC127_t *bt: A pointer to the Bluetooth struct
 *  IBus_t *ibus: A pointer to the IBus struct
 *  mode: Track the state of the radio to see what we should display to the user.
 *  screenUpdated: The screen has been updated by the radio
 */
typedef struct MIDContext_t {
    BT_t *bt;
    IBus_t *ibus;
    uint8_t mode;
    uint8_t displayMetadata: 2;
    uint8_t displayUpdate;
    uint8_t modeChangeStatus;
    char mainText[16];
    MenuSingleLineContext_t menuContext;
    UtilsAbstractDisplayValue_t mainDisplay;
    UtilsAbstractDisplayValue_t tempDisplay;
    uint8_t displayUpdateTaskId;
} MIDContext_t;
void MIDInit(BT_t *, IBus_t *);
void MIDDestroy();
void MIDUIDisplayUpdateText(void *ctx, uint8_t *data);
void MIDBTDeviceConnected(void *, uint8_t *);
void MIDBTDeviceDisconnected(void *, uint8_t *);
void MIDBTMetadataUpdate(void *, uint8_t *);
void MIDBTPlaybackStatus(void *, uint8_t *);
void MIDIBusCDChangerStatus(void *, uint8_t *);
void MIDIBusIgnitionStatus(void *, uint8_t *);
void MIDIBusMIDButtonPress(void *, uint8_t *);
void MIDIIBusRADMIDDisplayUpdate(void *, uint8_t *);
void MIDIBusMIDModeChange(void *, uint8_t *);
void MIDTimerMenuWrite(void *);
void MIDTimerDisplay(void *);
#endif /* MID_H */
