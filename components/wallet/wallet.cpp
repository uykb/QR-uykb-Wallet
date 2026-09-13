/*********************
 *      INCLUDES
 *********************/
#include <wallet.h>
#include "rlp.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <Bitcoin.h>
#include <Hash.h>
#include <utility/trezor/sha3.h>
#include <utility/trezor/secp256k1.h>
#include <utility/trezor/ecdsa.h>
#include <transaction_factory.h>
#include <memory>
#include <unordered_map>
#include <esp_log.h>
/*********************
 *      DEFINES
 *********************/
#define TAG "wallet"
#define MAX_PATH_LEN 32
#define ETH_DERIVATION_PATH "m/44'/60'/0'/"
#define BTC_DERIVATION_PATH "m/84'/0'/0'/"

/**********************
 *      MACROS
 **********************/
#define _debug_print_map_size() \
    ESP_LOGI(TAG, "shared_ptr_map size: %zu", shared_ptr_map.size());

/**********************
 *  STATIC VARIABLES
 **********************/
static std::unordered_map<uintptr_t, std::shared_ptr<HDPrivateKey>> shared_ptr_map;

extern "C"
{

    /**********************
     *  STATIC PROTOTYPES
     **********************/
    static uintptr_t make_shared_ptr(std::shared_ptr<HDPrivateKey> ptr);
    static void free_shared_ptr(uintptr_t ptr);
    static HDPrivateKey *get_shared_ptr(uintptr_t ptr);

    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    Wallet wallet_init_from_mnemonic(const char *mnemonic);
    Wallet wallet_init_from_xprv(const char *xprv);
    void wallet_free(Wallet wallet);

    char *wallet_root_private_key(Wallet wallet);
    void wallet_eth_key_fingerprint(Wallet wallet, publickey_fingerprint_t *fingerprint);
    Wallet wallet_derive(Wallet wallet, const char *path);
    Wallet wallet_derive_btc(Wallet wallet, unsigned int index);
    Wallet wallet_derive_eth(Wallet wallet, unsigned int index);
    void wallet_get_btc_address_segwit(Wallet wallet, char address[43]);
    void wallet_get_btc_address_legacy(Wallet wallet, char address[43]);
    void wallet_get_eth_address(Wallet wallet, char address[43]);
    void wallet_eth_sign(Wallet wallet, const uint8_t hash[32], uint8_t signature[65]);
    void wallet_bin_to_hex_string(const uint8_t *bin, size_t bin_len, char **hex_string);

    /**********************
     *   STATIC FUNCTIONS
     **********************/
    static uintptr_t make_shared_ptr(std::shared_ptr<HDPrivateKey> ptr)
    {
        //_debug_print_map_size();

        uintptr_t _ptr = (uintptr_t)ptr.get();
        shared_ptr_map[_ptr] = ptr;
        return _ptr;
    }
    static void free_shared_ptr(uintptr_t ptr)
    {
        if (shared_ptr_map.find(ptr) != shared_ptr_map.end())
        {
            shared_ptr_map.erase(ptr);
        }
    }
    static HDPrivateKey *get_shared_ptr(uintptr_t ptr)
    {
        return shared_ptr_map[ptr].get();
    }

    /**********************
     *   GLOBAL FUNCTIONS
     **********************/
    Wallet wallet_init_from_mnemonic(const char *mnemonic)
    {
        auto wallet = HDPrivateKey{mnemonic, ""};
        return make_shared_ptr(std::make_shared<HDPrivateKey>(wallet));
    }
    Wallet wallet_init_from_xprv(const char *xprv)
    {
        auto wallet = HDPrivateKey{xprv};
        return make_shared_ptr(std::make_shared<HDPrivateKey>(wallet));
    }
    void wallet_free(Wallet wallet)
    {
        free_shared_ptr(wallet);
    }

    char *wallet_root_private_key(Wallet wallet)
    {
        HDPrivateKey *_wallet = get_shared_ptr(wallet);
        auto str = _wallet->xprv();
        char *cstr = (char *)malloc(str.length() + 1);
        strcpy(cstr, str.c_str());
        return cstr;
    }
    void wallet_eth_key_fingerprint(Wallet wallet, publickey_fingerprint_t *fingerprint)
    {
        HDPrivateKey *_wallet = get_shared_ptr(wallet);
        HDPrivateKey account = _wallet->derive(ETH_DERIVATION_PATH);
        account.xpub().sec(fingerprint->public_key, 33);
        memcpy(fingerprint->chain_code, account.xpub().chainCode, 32);
        account.xpub().fingerprint(fingerprint->fingerprint);
    }
    Wallet wallet_derive(Wallet wallet, const char *path)
    {
        HDPrivateKey *_wallet = get_shared_ptr(wallet);
        auto derived = _wallet->derive(path);
        return make_shared_ptr(std::make_shared<HDPrivateKey>(derived));
    }
    Wallet wallet_derive_btc(Wallet wallet, unsigned int index)
    {
        HDPrivateKey *_wallet = get_shared_ptr(wallet);
        char *derive_path = new char[64];
        snprintf(derive_path, 64, "%s%d/%d/", BTC_DERIVATION_PATH, 0, index);
        HDPrivateKey account = _wallet->derive(derive_path);
        delete[] derive_path;
        return make_shared_ptr(std::make_shared<HDPrivateKey>(account));
    }
    void wallet_get_btc_address_legacy(Wallet wallet, char address[43])
    {
        HDPrivateKey *_wallet = get_shared_ptr(wallet);
        auto str = _wallet->legacyAddress();
        strcpy(address, str.c_str());
    }
    void wallet_get_btc_address_segwit(Wallet wallet, char address[43])
    {
        HDPrivateKey *_wallet = get_shared_ptr(wallet);
        auto str = _wallet->segwitAddress();
        strcpy(address, str.c_str());
    }
    char *wallet_get_btc_xpub(Wallet wallet, const char *path)
    {
        HDPrivateKey *_wallet = get_shared_ptr(wallet);
        HDPrivateKey account = _wallet->derive(path);
        char buf[128] = {0};
        account.xpub(buf, sizeof(buf));
        char *cstr = (char *)malloc(strlen(buf) + 1);
        strcpy(cstr, buf);
        return cstr;
    }
    Wallet wallet_derive_eth(Wallet wallet, unsigned int index)
    {
        HDPrivateKey *_wallet = get_shared_ptr(wallet);
        char *derive_path = new char[64];
        snprintf(derive_path, 64, "%s%d/%d/", ETH_DERIVATION_PATH, 0, index);
        auto derived = _wallet->derive(derive_path);
        delete[] derive_path;
        return make_shared_ptr(std::make_shared<HDPrivateKey>(derived));
    }
    void wallet_get_eth_address(Wallet wallet, char address[43])
    {
        HDPrivateKey *_wallet = get_shared_ptr(wallet);
        uint8_t xy[64] = {0};
        uint8_t eth_address[64] = {0};
        memcpy(xy, _wallet->publicKey().point, 64);
        keccak_256(xy, 64, eth_address);
        auto str = "0x" + toHex(eth_address + 12, 20);
        strcpy(address, str.c_str());
    }
    void wallet_eth_sign(Wallet wallet, const uint8_t hash[32], uint8_t signature[65])
    {
        HDPrivateKey *_wallet = get_shared_ptr(wallet);
        Signature sig = _wallet->sign(hash);
        // sig.index += 27;
        sig.bin((uint8_t *)signature, 65);
    }
    void wallet_bin_to_hex_string(const uint8_t *bin, size_t bin_len, char **hex_string)
    {
        auto str = toHex(bin, bin_len);
        *hex_string = (char *)malloc(str.length() + 1);
        strcpy(*hex_string, str.c_str());
    }
}