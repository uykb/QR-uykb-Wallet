#pragma once

#include "lvgl.h"

LV_FONT_DECLARE(font_china_16);

typedef enum {
    LANG_EN = 0,
    LANG_CN = 1,
} lang_id_t;

typedef enum {
    // Wizard
    STR_LANGUAGE = 0,
    STR_SECURITY,
    STR_MNEMONIC_TYPE,
    STR_ENTER_MNEMONIC,
    STR_ENTER_PASSCODE,
    STR_NEXT,
    STR_12_WORDS,
    STR_24_WORDS,
    STR_ENTER,
    STR_SECURITY_TEXT,
    STR_WELCOME_TEXT,
    STR_TOAST_SECURE_BOOT,
    STR_INIT_WALLET_FAILED,
    // Home
    STR_WALLET,
    STR_SCANNER,
    STR_SETTINGS,
    STR_LOCK_NOW,
    STR_LOCK,
    STR_ERASE_ALL_DATA,
    STR_ERASE_WALLET,
    STR_ERASE_WARNING,
    STR_BACKLIGHT,
    STR_ABOUT,
    STR_GITHUB,
    STR_VERSION_FORMAT,
    STR_ERASE_CONFIRM_TITLE,
    STR_ERASE_CONFIRM_TEXT,
    STR_ERASE,
    STR_CANCEL,
    STR_HOMEPAGE,
    STR_SET_MAX_ATTEMPTS,
    STR_NETWORK_NOT_IMPLEMENTED,
    STR_AFTER_N_TIMES,
    // Pin
    STR_ENTER_NEW_PIN,
    STR_RE_ENTER_PIN,
    STR_VERIFY_PIN,
    STR_PASSCODE_NOT_MATCH,
    // Mnemonic
    STR_MNEMONIC_PHRASE,
    STR_CONFIRM,
    STR_RETRY,
    // Decoder
    STR_TRANSACTION,
    STR_TX_NOT_IMPLEMENTED,
    STR_SIGN,
    STR_LOAD_WALLET_FAILED,
    // Connect
    STR_CONNECT_VIA_QR,
    // Sign
    STR_SIGNATURE,
    STR_SCAN_TO_SEND,
    // Panic
    STR_PANIC_TITLE,
    STR_REBOOT,
    // Master page
    STR_COUNT,
} str_id_t;

void lang_init(void);
void lang_set(lang_id_t lang);
lang_id_t lang_get(void);
const char *lang_str(str_id_t id);
const lv_font_t *lang_font(void);
