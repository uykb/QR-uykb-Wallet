/*********************
 *      INCLUDES
 *********************/
#include "wallet_db.h"
#include "ui/ui_panic.h"
#include "kv_fs.h"
#include "wallet.h"
#include "ui/ui_loading.h"
#include "esp_log.h"
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>
#include "crc32.h"
#include "app.h"
#include "lang.h"
#include <mbedtls/pkcs5.h>
#include <mbedtls/gcm.h>
#include <mbedtls/platform_util.h>

/*********************
 *      DEFINES
 *********************/
#define DEFAULT_INCORRECT_PIN_COUNT 5
#define LOCK_SCREEN_TIMEOUT_MS (1000 * 60 * 5) // 5m
#define SIGN_PIN_REQUIRED true
#define VERSION 2
#define PBKDF2_ITERATIONS 10000
#define GCM_IV_SIZE 12
#define GCM_TAG_SIZE 16

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG = "wallet_db";
static wallet_data_version_1_t *walletData_cache = NULL;
static char rootPrivateKey_cache[PRIVATE_KEY_SIZE + 1] = {0};
static char temp[64];

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void pin_derive_key_pbkdf2(const char *pinStr, const uint8_t padding[32], uint8_t key[32]);
static uint32_t checksum(wallet_data_version_1_t *walletData);
static void reset_task(void *parameters);
size_t wallet_data_to_bin(wallet_data_version_1_t *walletData, char **hex);
void wallet_data_from_bin(wallet_data_version_1_t *walletData, const char *hex, size_t len);

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void pin_derive_key_pbkdf2(const char *pinStr, const uint8_t padding[32], uint8_t key[32])
{
    /* Salt = pinPadding (32 bytes) + RAINBOW_TABLE_SALT */
    size_t salt_len = 32 + strlen(RAINBOW_TABLE_SALT);
    uint8_t *salt = (uint8_t *)malloc(salt_len);
    if (salt == NULL)
    {
        ESP_LOGE(TAG, "malloc salt failed");
        abort();
    }
    memcpy(salt, padding, 32);
    memcpy(salt + 32, RAINBOW_TABLE_SALT, strlen(RAINBOW_TABLE_SALT));

    /* PBKDF2-HMAC-SHA256 with 10,000 iterations */
    int ret = mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA256,
        (const unsigned char *)pinStr,
        strlen(pinStr),
        salt,
        salt_len,
        PBKDF2_ITERATIONS,
        32,
        key);

    mbedtls_platform_zeroize(salt, salt_len);
    free(salt);

    if (ret != 0)
    {
        ESP_LOGE(TAG, "PBKDF2 failed with code: %d", ret);
        abort();
    }
}

static uint32_t checksum(wallet_data_version_1_t *walletData)
{
    size_t size = sizeof(wallet_data_version_1_t) - sizeof(uint32_t);
    uint8_t *m = (uint8_t *)malloc(size);
    memcpy(m, walletData, size);
    uint32_t c = crc32(0, m, size);
    free(m);
    return c;
}

static void reset_task(void *parameters)
{
    ui_loading_show();
    kv_erase();
    esp_restart();
    vTaskDelete(NULL);
}

size_t wallet_data_to_bin(wallet_data_version_1_t *walletData, char **hex)
{
    size_t size = sizeof(wallet_data_version_1_t);
    *hex = (char *)malloc(size);
    if (*hex == NULL)
    {
        ESP_LOGE(TAG, "malloc failed");
        abort();
    }
    memcpy(*hex, walletData, size);
    return size;
}

void wallet_data_from_bin(wallet_data_version_1_t *walletData, const char *hex, size_t len)
{
    size_t size = sizeof(wallet_data_version_1_t);
    if (len < size)
    {
        ESP_LOGI(TAG, "len < sizeof(wallet_data_version_1_t)");
        memset(walletData, 0, size);
    }
    else
    {
        memcpy(walletData, hex, size);
    }
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
bool wallet_db_load_wallet_data(wallet_data_version_1_t *walletData)
{
    if (walletData_cache != NULL)
    {
        memcpy(walletData, walletData_cache, sizeof(wallet_data_version_1_t));
        return true;
    }

    char *hex = NULL;
    size_t len = 0;
    int ret = kv_load(KV_FS_KEY_WALLET, &hex, &len);
    if (ret != 0)
    {
        return false;
    }

    wallet_data_from_bin(walletData, hex, len);
    if (hex != NULL)
    {
        free(hex);
        hex = NULL;
    }
    if (walletData->initialized == true)
    {
        int checksum_value = checksum(walletData);
        if (walletData->checksum != checksum_value)
        {
            ESP_LOGE(TAG, "Checksum mismatch in wallet data");
            return false;
        }
        else
        {
            if (walletData_cache == NULL)
            {
                walletData_cache = (wallet_data_version_1_t *)malloc(sizeof(wallet_data_version_1_t));
            }
            memcpy(walletData_cache, walletData, sizeof(wallet_data_version_1_t));
            return true;
        }
    }
    return false;
}

void wallet_db_save_wallet_data(wallet_data_version_1_t *walletData)
{
    if (walletData->initialized == false)
    {
        ui_panic("walletData is not initialized", PANIC_REBOOT);
        return;
    }
    walletData->checksum = checksum(walletData);
    char *walletDataStr = NULL;
    size_t len = wallet_data_to_bin(walletData, &walletDataStr);
    if (kv_save(KV_FS_KEY_WALLET, walletDataStr, len) != ESP_OK)
    {
        ui_panic("kv_save failed", PANIC_REBOOT);
        return;
    }
    free(walletDataStr);
    walletDataStr = NULL;
    if (walletData_cache == NULL)
    {
        walletData_cache = (wallet_data_version_1_t *)malloc(sizeof(wallet_data_version_1_t));
    }
    memcpy(walletData_cache, walletData, sizeof(wallet_data_version_1_t));
}

bool wallet_db_init_wallet_data(char *phrase_str, char *pin_str, char **private_key_str)
{
    Wallet wallet = wallet_init_from_mnemonic(phrase_str);
    char *root_private_key = wallet_root_private_key(wallet);
    wallet_data_version_1_t *walletData = (wallet_data_version_1_t *)malloc(sizeof(wallet_data_version_1_t));
    memset(walletData, 0, sizeof(wallet_data_version_1_t));
    walletData->signPinRequired = SIGN_PIN_REQUIRED;
    walletData->language = (uint8_t)lang_get();

    /* Generate 32 bytes random salt and 12 bytes random IV for AES-GCM */
    esp_fill_random(walletData->pinPadding, 32);
    esp_fill_random(walletData->iv, GCM_IV_SIZE);

    uint8_t key[32] = {0};
    pin_derive_key_pbkdf2(pin_str, walletData->pinPadding, key);

    /* Encrypt root private key with AES-256-GCM (Authenticated Encryption) */
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);

    int encrypt_ret = mbedtls_gcm_crypt_and_tag(
        &gcm,
        MBEDTLS_GCM_ENCRYPT,
        PRIVATE_KEY_SIZE,
        walletData->iv,
        GCM_IV_SIZE,
        (const unsigned char *)"QR-WALLET",
        9,
        (const unsigned char *)root_private_key,
        (unsigned char *)walletData->privateKey,
        GCM_TAG_SIZE,
        walletData->tag);

    mbedtls_gcm_free(&gcm);

    if (encrypt_ret != 0)
    {
        mbedtls_platform_zeroize(key, sizeof(key));
        free(root_private_key);
        free(walletData);
        ui_panic("GCM encrypt failed", PANIC_REBOOT);
        return false;
    }

    /* Verification decryption test */
    uint8_t test_decrypt[PRIVATE_KEY_SIZE + 1] = {0};
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);

    int auth_ret = mbedtls_gcm_auth_decrypt(
        &gcm,
        PRIVATE_KEY_SIZE,
        walletData->iv,
        GCM_IV_SIZE,
        (const unsigned char *)"QR-WALLET",
        9,
        walletData->tag,
        GCM_TAG_SIZE,
        (const unsigned char *)walletData->privateKey,
        test_decrypt);

    mbedtls_gcm_free(&gcm);
    mbedtls_platform_zeroize(key, sizeof(key));

    if (auth_ret != 0 || strcmp(root_private_key, (char *)test_decrypt) != 0)
    {
        mbedtls_platform_zeroize(test_decrypt, sizeof(test_decrypt));
        free(root_private_key);
        free(walletData);
        ui_panic("GCM decrypt test failed", PANIC_REBOOT);
        return false;
    }
    mbedtls_platform_zeroize(test_decrypt, sizeof(test_decrypt));

    walletData->initialized = true;
    walletData->version = VERSION;
    walletData->incorrectPinCount = 0;
    walletData->incorrectPinCountMax = DEFAULT_INCORRECT_PIN_COUNT;
    walletData->lockScreenTimeout = LOCK_SCREEN_TIMEOUT_MS;
    walletData->cooldownUntil = 0;
    wallet_db_save_wallet_data(walletData);

    free(walletData);
    wallet_free(wallet);
    *private_key_str = root_private_key;
    return true;
}

void wallet_db_clear_cache()
{
    if (walletData_cache != NULL)
    {
        mbedtls_platform_zeroize(walletData_cache, sizeof(wallet_data_version_1_t));
        free(walletData_cache);
        walletData_cache = NULL;
    }
    mbedtls_platform_zeroize(rootPrivateKey_cache, sizeof(rootPrivateKey_cache));
    memset(temp, 0, sizeof(temp));
}

char *wallet_db_verify_pin(char *pin_str)
{
    if (walletData_cache == NULL)
    {
        wallet_data_version_1_t wd;
        if (!wallet_db_load_wallet_data(&wd))
        {
            ui_panic("walletData is NULL", PANIC_REBOOT);
            return NULL;
        }
    }

    uint32_t now_sec = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    if (walletData_cache->cooldownUntil > now_sec)
    {
        uint32_t remain = walletData_cache->cooldownUntil - now_sec;
        snprintf(temp, sizeof(temp), "Cooldown: wait %ds", (int)remain);
        return temp;
    }

    uint8_t key[32] = {0};
    pin_derive_key_pbkdf2(pin_str, walletData_cache->pinPadding, key);

    uint8_t rootPrivateKey[PRIVATE_KEY_SIZE + 1] = {0};
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);

    int auth_ret = mbedtls_gcm_auth_decrypt(
        &gcm,
        PRIVATE_KEY_SIZE,
        walletData_cache->iv,
        GCM_IV_SIZE,
        (const unsigned char *)"QR-WALLET",
        9,
        walletData_cache->tag,
        GCM_TAG_SIZE,
        (const unsigned char *)walletData_cache->privateKey,
        rootPrivateKey);

    mbedtls_gcm_free(&gcm);
    mbedtls_platform_zeroize(key, sizeof(key));

    if (auth_ret == 0 && strncmp((char *)rootPrivateKey, "xprv", 4) == 0)
    {
        if (walletData_cache->incorrectPinCount > 0)
        {
            walletData_cache->incorrectPinCount = 0;
            walletData_cache->cooldownUntil = 0;
            wallet_db_save_wallet_data(walletData_cache);
        }

        strcpy(rootPrivateKey_cache, (char *)rootPrivateKey);
        mbedtls_platform_zeroize(rootPrivateKey, sizeof(rootPrivateKey));
        return NULL;
    }
    else
    {
        mbedtls_platform_zeroize(rootPrivateKey, sizeof(rootPrivateKey));
        walletData_cache->incorrectPinCount++;

        /* Exponential backoff cooldown (e.g., 3s, 10s, 30s, 60s) */
        uint32_t backoff_delays[] = {0, 3, 10, 30, 60};
        int delay_idx = walletData_cache->incorrectPinCount;
        if (delay_idx > 4) delay_idx = 4;
        walletData_cache->cooldownUntil = now_sec + backoff_delays[delay_idx];

        if (walletData_cache->incorrectPinCount >= walletData_cache->incorrectPinCountMax)
        {
            wallet_db_reset_device();
            ui_panic("Too many incorrect passcode attempts. Device reset.", PANIC_REBOOT);
            return NULL;
        }

        wallet_db_save_wallet_data(walletData_cache);
        return wallet_db_passcode_static_error_msg();
    }
}

char *wallet_db_pop_private_key()
{
    char *_privateKey = malloc(PRIVATE_KEY_SIZE + 1);
    strcpy(_privateKey, rootPrivateKey_cache);
    mbedtls_platform_zeroize(rootPrivateKey_cache, sizeof(rootPrivateKey_cache));
    return _privateKey;
}

void wallet_db_reset_device()
{
    xTaskCreate(reset_task, "reset_task", 4 * 1024, NULL, 1, NULL);
}

char *wallet_db_passcode_static_error_msg()
{
    wallet_data_version_1_t walletData;
    if (wallet_db_load_wallet_data(&walletData) == false)
    {
        return "Can't load wallet data";
    }
    if (walletData.incorrectPinCount > 0)
    {
        sprintf(temp, "Passcode error %d/%d", (walletData.incorrectPinCount), walletData.incorrectPinCountMax);
        return temp;
    }
    return NULL;
}
