#include "lang.h"
#include "lvgl.h"
#include "kv_fs.h"
#include "esp_log.h"
#include <string.h>

#define TAG_LANG "LANG"
#define KV_KEY_LANG "key_lang_pref"

static lang_id_t current_lang = LANG_EN;

static const char *str_table[][2] = {
    // [EN]                                        [CN]
    [STR_LANGUAGE]           = {"Language",          "语言"},
    [STR_SECURITY]           = {"Security",          "安全"},
    [STR_MNEMONIC_TYPE]      = {"Mnemonic Type",     "助记词类型"},
    [STR_ENTER_MNEMONIC]     = {"Enter Mnemonic",    "输入助记词"},
    [STR_ENTER_PASSCODE]     = {"Enter Passcode",    "输入密码"},
    [STR_NEXT]               = {"Next",              "下一步"},
    [STR_12_WORDS]           = {"12 words",          "12个词"},
    [STR_24_WORDS]           = {"24 words",          "24个词"},
    [STR_ENTER]              = {"Enter",             "进入"},
    [STR_SECURITY_TEXT]      = {"[ Security Notice ]\n\n1. Import-Only Seed Phrases\nThis device lacks an audited true random number generator. Please generate seed phrases on a trusted wallet and import them here.\n\n2. Passcode Protection\nMemorize your device passcode. If lost, your wallet cannot be recovered.\n\n3. Offline Physical Isolation\nTo ensure private keys remain offline, Wi-Fi and Bluetooth are completely disabled.\n\n4. Charging & Connection\nAlways connect directly to a dedicated power adapter, never to an untrusted computer.\n\n5. Verify Before Signing\nAlways double-check recipient address and amounts on screen. Never sign untrusted data.\n\nFollowing these guidelines keeps your assets safe.",
                                  "【安全须知与协议】\n\n1. 仅支持导入助记词\n本设备硬件未配备经过审计的物理真随机数发生器，请使用受信任的钱包生成助记词后导入。\n\n2. 密码保管\n请牢记设备解锁密码。若密码遗失，您将无法访问钱包且无法找回资产。\n\n3. 离线物理隔离\n为保障私钥最高安全，本设备的 Wi-Fi 与蓝牙已在系统固件底层完全禁用。\n\n4. 充电与连接安全\n充电时请直接连接原装电源适配器，切勿连接不受信任的电脑或设备。\n\n5. 签名安全核对\n签署任何交易前，请仔细核对屏幕上的交易详情与地址。切勿签署未知数据。\n\n遵循以上准则，您的数字资产将处于安全保护中。"},
    [STR_WELCOME_TEXT]       = {"\n\n  Welcome to QR Hardware Wallet!\n\n\n  Your account is successfully set up.",
                                  "\n\n  欢迎使用QR硬件钱包！\n\n\n  您的账户已成功设置。"},
    [STR_TOAST_SECURE_BOOT] = {"NOTE:\nSECURE BOOT in current version does not implemented, If the device is lost, your private key may be compromised!",
                                  "注意：\n当前版本未实现安全启动，如果设备丢失，您的私钥可能会泄露！"},
    [STR_INIT_WALLET_FAILED]= {"init_wallet_data failed", "初始化钱包数据失败"},
    [STR_WALLET]            = {"Wallet",             "钱包"},
    [STR_SCANNER]           = {"Scanner",            "扫描"},
    [STR_SETTINGS]          = {"Settings",           "设置"},
    [STR_LOCK_NOW]          = {"Lock Now",           "立即锁定"},
    [STR_LOCK]              = {"Lock",               "锁定"},
    [STR_ERASE_ALL_DATA]    = {"Erase All Data",     "清除所有数据"},
    [STR_ERASE_WALLET]      = {"Erase Wallet",       "清除钱包"},
    [STR_ERASE_WARNING]     = {"To protect your data, all data will be erased if the PIN is entered incorrectly too many times",
                                  "为保护您的数据，密码输入错误次数过多后，所有数据将被清除"},
    [STR_BACKLIGHT]         = {"Backlight",          "背光"},
    [STR_ABOUT]             = {"About",              "关于"},
    [STR_GITHUB]            = {"Github",             "Github"},
    [STR_VERSION_FORMAT]    = {"Release Date: %s",   "发布日期: %s"},
    [STR_ERASE_CONFIRM_TITLE] = {"Erase",            "清除"},
    [STR_ERASE_CONFIRM_TEXT] = {"All data will be erased.", "所有数据将被清除。"},
    [STR_ERASE]             = {"Erase",              "清除"},
    [STR_CANCEL]            = {"Cancel",             "取消"},
    [STR_HOMEPAGE]          = {"Homepage",           "主页"},
    [STR_SET_MAX_ATTEMPTS]  = {"Set maximum attempts to %d times", "设置最大尝试次数为 %d 次"},
    [STR_NETWORK_NOT_IMPLEMENTED] = {"The network %s is not implemented yet.", "网络 %s 尚未实现。"},
    [STR_AFTER_N_TIMES]     = {"After 2 times\nAfter 3 times\nAfter 4 times\nAfter 5 times\nAfter 6 times\nAfter 7 times\nAfter 8 times\nAfter 9 times\nAfter 10 times",
                                  "2次\n3次\n4次\n5次\n6次\n7次\n8次\n9次\n10次"},
    [STR_ENTER_NEW_PIN]     = {"Enter new passcode", "输入新密码"},
    [STR_RE_ENTER_PIN]      = {"Re-Enter passcode",  "再次输入密码"},
    [STR_VERIFY_PIN]        = {"Enter passcode",     "输入密码"},
    [STR_PASSCODE_NOT_MATCH]= {"Passcode not match!", "密码不匹配！"},
    [STR_MNEMONIC_PHRASE]   = {"Mnemonic phrase",    "助记词"},
    [STR_CONFIRM]           = {"Confirm",            "确认"},
    [STR_RETRY]             = {"Retry",              "重试"},
    [STR_TRANSACTION]       = {"Transaction",        "交易"},
    [STR_TX_NOT_IMPLEMENTED]= {"Transaction decode not implemented yet", "交易解码功能尚未实现"},
    [STR_SIGN]              = {"Sign",               "签名"},
    [STR_LOAD_WALLET_FAILED]= {"Can't load wallet data", "无法加载钱包数据"},
    [STR_CONNECT_VIA_QR]    = {"Connect via QR Code", "通过二维码连接"},
    [STR_SIGNATURE]         = {"Signature",          "签名"},
    [STR_SCAN_TO_SEND]      = {"Scan the QR code to send transaction", "扫描二维码发送交易"},
    [STR_PANIC_TITLE]       = {"Panic - can't recover", "严重错误 - 无法恢复"},
    [STR_REBOOT]            = {"Reboot!",            "重启！"},
};

void lang_init(void)
{
    char *val = NULL;
    size_t len = 0;
    if (kv_load(KV_KEY_LANG, &val, &len) == 0 && val != NULL) {
        if (len >= 1) {
            uint8_t l = (uint8_t)val[0];
            if (l == (uint8_t)LANG_CN) {
                current_lang = LANG_CN;
            } else {
                current_lang = LANG_EN;
            }
            ESP_LOGI(TAG_LANG, "Loaded saved language preference: %d", current_lang);
        }
        free(val);
    } else {
        current_lang = LANG_EN;
    }
}

void lang_set(lang_id_t lang)
{
    current_lang = lang;
    char val[2];
    val[0] = (char)lang;
    val[1] = '\0';
    kv_save(KV_KEY_LANG, val, 1);
    ESP_LOGI(TAG_LANG, "Saved language preference: %d", current_lang);

    /* Apply CJK font when Chinese is selected */
    const lv_font_t *f = lang_font();
    lv_obj_t *scr = lv_screen_active();
    if (scr && f) {
        lv_obj_set_style_text_font(scr, f, 0);
    }
}

lang_id_t lang_get(void)
{
    return current_lang;
}

const char *lang_str(str_id_t id)
{
    if (id >= STR_COUNT) return "";
    return str_table[id][current_lang];
}

const lv_font_t *lang_font(void)
{
    if (current_lang == LANG_CN) {
        return &font_china_16;
    }
    return LV_FONT_DEFAULT;
}
