/*********************
 *      INCLUDES
 *********************/
#include "stdio.h"
#include "stdlib.h"
#include "utility/trezor/bip39_english.h"
#include "string.h"
#include "ui/ui_mnemonic.h"
#include "ui/ui_events.h"
#include "alloc_utils.h"
#include "lang.h"
#include "esp_log.h"
#include "ui/ui_style.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "PHRASE_INPUT_PAGE"
#define BIP39_WORDLIST_LEN ((sizeof(wordlist) / sizeof(wordlist[0])) - 1)
#define START_WITH(prefix, prefix_len, str) (strncmp(str, prefix, prefix_len) == 0)

/**********************
 *  STATIC VARIABLES
 **********************/
static const char **phrases;
static size_t phrases_len;
static alloc_utils_memory_struct *alloc_utils_memory_struct_pointer;
static lv_obj_t *parent;
static lv_obj_t *current_page;
static lv_obj_t *content;
static lv_obj_t *keyboard;
static lv_obj_t *words;
static lv_obj_t *btn_del;
static char *current_input;
static int cue_from;
static int cue_to;
static char cue_letter[26];
static size_t cue_letter_len;
static int mnemonic_type;
static lv_obj_t *event_target;
static size_t page_idx = 0;
/**********************
 *  STATIC PROTOTYPES
 **********************/
static void phrase_choose_event_handler(lv_event_t *e);
static void msgbox_confirm_event_handler(lv_event_t *e);
static void msgbox_retry_event_handler(lv_event_t *e);
static void update_keyboard_button();
static void phrase_input_handler(lv_event_t *e);
static void send_mnemonic_confirm_event(void);

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void ui_mnemonic_init(lv_obj_t *lv_parent, size_t lv_parent_width, size_t lv_parent_height, lv_obj_t *event_target, int mnemonic_type);
void ui_mnemonic_destroy(void);

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void msgbox_confirm_event_handler(lv_event_t *e)
{
    if (lvgl_port_lock(0))
    {
        lv_obj_t *mbox = lv_event_get_user_data(e);
        lv_msgbox_close(mbox);
        lvgl_port_unlock();
    }
    send_mnemonic_confirm_event();
}

static void send_mnemonic_confirm_event(void)
{
    if (phrases_len == (size_t)mnemonic_type)
    {
        char *phrase = malloc(sizeof(char) * 12 * phrases_len + 1);
        phrase[0] = '\0';
        for (size_t i = 0; i < phrases_len; i++)
        {
            strcat(phrase, phrases[i]);
            if (i < phrases_len - 1)
            {
                strcat(phrase, " ");
            }
        }
        if (lvgl_port_lock(0))
        {
            lv_result_t re = lv_obj_send_event(
                event_target == NULL ? parent : event_target,
                phrase == NULL ? UI_EVENT_PHRASE_CANCEL : UI_EVENT_PHRASE_CONFIRM,
                (void *)phrase);
            if (re == LV_RESULT_INVALID)
            {
                printf("lv_obj_send_event failed\n");
                free(phrase);
            }
            lvgl_port_unlock();
        }
    }
}
static void msgbox_retry_event_handler(lv_event_t *e)
{
    if (lvgl_port_lock(0))
    {
        lv_obj_t *mbox = lv_event_get_user_data(e);
        lv_msgbox_close(mbox);

        /* Restore keyboard, words bar, and btn_del visibility */
        if (keyboard)
        {
            lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        }
        if (words)
        {
            lv_obj_remove_flag(words, LV_OBJ_FLAG_HIDDEN);
        }
        if (btn_del)
        {
            lv_obj_remove_flag(btn_del, LV_OBJ_FLAG_HIDDEN);
        }

        /* Reset phrase input to start over */
        phrases_len = 0;
        current_input[0] = '\0';
        page_idx = 0;
        lv_obj_clean(content);
        update_keyboard_button();

        lvgl_port_unlock();
    }
}
static void phrase_choose_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        if (phrases_len < (size_t)mnemonic_type)
        {
            if (lvgl_port_lock(0))
            {
                lv_obj_t *clicked_item = lv_event_get_target(e);
                const char *item_arg = (const char *)lv_obj_get_user_data(clicked_item);
                phrases[phrases_len] = item_arg;
                phrases_len++;
                current_input[0] = '\0';
                page_idx = 0;
                {
                    lv_obj_t *obj = lv_button_create(content);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_bg_opa(obj, 255, 0);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x2d3748), 0);
                    lv_obj_set_style_border_width(obj, 1, 0);
                    lv_obj_set_style_border_color(obj, lv_color_hex(0x4a5568), 0);
                    lv_obj_set_style_radius(obj, 4, 0);
                    lv_obj_t *label = lv_label_create(obj);
                    lv_label_set_text_fmt(label, "#%zu %s", phrases_len, item_arg);
                    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
                    lv_obj_center(label);

                    // scroll `content` to bottom
                    lv_obj_scroll_to_y(content, 999, LV_ANIM_ON);
                }
                lvgl_port_unlock();
            }
            update_keyboard_button();
        }
        if (phrases_len == (size_t)mnemonic_type)
        {
            if (lvgl_port_lock(0))
            {
                /* Hide keyboard, word suggestion bar, and btn_del so they don't overlap with the confirmation modal */
                if (keyboard)
                {
                    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
                }
                if (words)
                {
                    lv_obj_add_flag(words, LV_OBJ_FLAG_HIDDEN);
                }
                if (btn_del)
                {
                    lv_obj_add_flag(btn_del, LV_OBJ_FLAG_HIDDEN);
                }

                lv_obj_t *mbox = lv_msgbox_create(NULL);
                lv_obj_set_size(mbox, lv_pct(100), lv_pct(100));
                lv_obj_set_style_bg_opa(mbox, 255, 0);
                lv_obj_set_style_bg_color(mbox, lv_color_hex(0x121824), 0);
                lv_obj_set_style_border_width(mbox, 0, 0);
                lv_obj_set_style_radius(mbox, 0, 0);
                lv_obj_center(mbox);
                /* Apply font so Chinese characters render correctly */
                const lv_font_t *mbox_font = lang_font();
                if (mbox_font)
                {
                    lv_obj_set_style_text_font(mbox, mbox_font, 0);
                }

                lv_obj_t *title_lbl = lv_msgbox_add_title(mbox, lang_str(STR_MNEMONIC_PHRASE));
                if (title_lbl && mbox_font)
                {
                    lv_obj_set_style_text_font(title_lbl, mbox_font, 0);
                    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xffffff), 0);
                }

                char *text = malloc(sizeof(char) * 15 * phrases_len + 32);
                text[0] = '\0';
                for (size_t i = 0; i < phrases_len; i++)
                {
                    char line_buf[32];
                    sprintf(line_buf, "#%zu %s%s", i + 1, phrases[i], ((i + 1) % 2 == 0) ? "\n" : "  ");
                    strcat(text, line_buf);
                }

                lv_obj_t *content_obj = lv_msgbox_get_content(mbox);
                if (content_obj)
                {
                    lv_obj_set_scroll_dir(content_obj, LV_DIR_VER);
                    lv_obj_set_scrollbar_mode(content_obj, LV_SCROLLBAR_MODE_AUTO);
                    lv_obj_set_style_bg_opa(content_obj, 0, 0);
                }
                lv_obj_t *text_label = lv_msgbox_add_text(mbox, text);
                if (text_label)
                {
                    lv_obj_set_style_text_color(text_label, lv_color_hex(0xffffff), 0);
                    if (mbox_font)
                    {
                        lv_obj_set_style_text_font(text_label, mbox_font, 0);
                    }
                }
                free(text);

                lv_obj_t *btn_confirm = lv_msgbox_add_footer_button(mbox, lang_str(STR_CONFIRM));
                lv_obj_set_size(btn_confirm, 90, 38);
                lv_obj_set_style_bg_opa(btn_confirm, 255, 0);
                lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0x2b6cb0), 0);
                lv_obj_set_style_text_color(btn_confirm, lv_color_hex(0xffffff), 0);
                lv_obj_add_event_cb(btn_confirm, msgbox_confirm_event_handler, LV_EVENT_CLICKED, mbox);

                lv_obj_t *btn_retry = lv_msgbox_add_footer_button(mbox, lang_str(STR_RETRY));
                lv_obj_set_size(btn_retry, 90, 38);
                lv_obj_set_style_bg_opa(btn_retry, 255, 0);
                lv_obj_set_style_bg_color(btn_retry, lv_color_hex(0x4a5568), 0);
                lv_obj_set_style_text_color(btn_retry, lv_color_hex(0xffffff), 0);
                lv_obj_add_event_cb(btn_retry, msgbox_retry_event_handler, LV_EVENT_CLICKED, mbox);

                lvgl_port_unlock();
            }
        }
    }

    bool debug_mode = false;
    if (debug_mode)
    {
        //  until exhaust file evidence reopen mad stumble beach acquire judge fuel raccoon cram arrange sugar swim cluster exile picture curtain velvet choice surge aware
        phrases[0] = "until";
        phrases[1] = "exhaust";
        phrases[2] = "file";
        phrases[3] = "evidence";
        phrases[4] = "reopen";
        phrases[5] = "mad";
        phrases[6] = "stumble";
        phrases[7] = "beach";
        phrases[8] = "acquire";
        phrases[9] = "judge";
        phrases[10] = "fuel";
        phrases[11] = "raccoon";
        phrases[12] = "cram";
        phrases[13] = "arrange";
        phrases[14] = "sugar";
        phrases[15] = "swim";
        phrases[16] = "cluster";
        phrases[17] = "exile";
        phrases[18] = "picture";
        phrases[19] = "curtain";
        phrases[20] = "velvet";
        phrases[21] = "choice";
        phrases[22] = "surge";
        phrases[23] = "aware";
        phrases_len = 24;

        send_mnemonic_confirm_event();
    }
}
static const char *letter_strs[26] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z"};
static const char *dynamic_btnm_map[16]; /* 3x3 = 9 buttons + 2 newlines + terminator + spare */

static void update_keyboard_button()
{
    if (lvgl_port_lock(0))
    {
        cue_from = -1;
        cue_to = -1;
        memset(cue_letter, 0, 26);
        cue_letter_len = 0;
        size_t current_input_len = strlen(current_input);
        char next_letter = '\0';
        for (size_t i = 0; i < BIP39_WORDLIST_LEN; i++)
        {
            const char *word = wordlist[i];
            if (START_WITH(current_input, current_input_len, word))
            {
                if (cue_from == -1)
                {
                    cue_from = i;
                }
                if (strlen(word) > current_input_len)
                {
                    next_letter = word[current_input_len];
                    if (cue_letter_len == 0 || cue_letter[cue_letter_len - 1] != next_letter)
                    {
                        cue_letter[cue_letter_len] = next_letter;
                        cue_letter_len++;
                    }
                }
            }
            else if (cue_from != -1)
            {
                cue_to = i - 1;
                break;
            }
        }
        if (cue_from != -1 && cue_to == -1)
        {
            cue_to = BIP39_WORDLIST_LEN - 1;
        }

        size_t total_pages = (cue_letter_len + 8) / 9;
        if (total_pages == 0) total_pages = 1;
        if (page_idx >= total_pages)
        {
            page_idx = 0;
        }

        size_t start = page_idx * 9;
        size_t count = (start < cue_letter_len) ? (cue_letter_len - start) : 0;
        if (count > 9) count = 9;

        size_t map_i = 0;
        if (count == 0)
        {
            dynamic_btnm_map[0] = "";
        }
        else
        {
            for (size_t i = 0; i < count; i++)
            {
                char c = cue_letter[start + i];
                dynamic_btnm_map[map_i++] = (c >= 'a' && c <= 'z') ? letter_strs[c - 'a'] : "?";
                /* newline after 3rd and 6th button to form 3x3 */
                if ((i == 2 || i == 5) && count > (i + 1))
                {
                    dynamic_btnm_map[map_i++] = "\n";
                }
            }
            dynamic_btnm_map[map_i] = "";
        }

        lv_buttonmatrix_set_map(keyboard, dynamic_btnm_map);

        lv_obj_t *child;
        while ((child = lv_obj_get_child(words, 0)))
        {
            lv_obj_del(child);
        }
        if (cue_from >= 0)
        {
            for (size_t i = cue_from;; i++)
            {
                if (i > cue_to || i > cue_from + 5)
                {
                    break;
                }
                lv_obj_t *obj = lv_button_create(words);
                lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                lv_obj_set_style_bg_opa(obj, 255, 0);
                lv_obj_set_style_bg_color(obj, lv_color_hex(0x2d3748), 0);
                lv_obj_set_style_border_width(obj, 1, 0);
                lv_obj_set_style_border_color(obj, lv_color_hex(0x4a5568), 0);
                lv_obj_set_style_radius(obj, 4, 0);
                lv_obj_t *label = lv_label_create(obj);
                lv_label_set_text(label, wordlist[i]);
                lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
                lv_obj_center(label);
                lv_obj_set_user_data(obj, (void *)wordlist[i]);
                lv_obj_add_event_cb(obj, phrase_choose_event_handler, LV_EVENT_CLICKED, NULL);
            }
        }
        lvgl_port_unlock();
    }
}

static void phrase_input_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED)
    {
        lvgl_port_lock(0);
        uint32_t id = lv_buttonmatrix_get_selected_button(obj);
        if (id == LV_BUTTONMATRIX_BUTTON_NONE)
        {
            lvgl_port_unlock();
            return;
        }
        const char *txt = lv_buttonmatrix_get_button_text(obj, id);
        lvgl_port_unlock();

        if (txt == NULL)
        {
            return;
        }

        if (strlen(txt) == 1 && txt[0] >= 'A' && txt[0] <= 'Z')
        {
            char c = tolower((unsigned char)txt[0]);
            size_t len = strlen(current_input);
            if (len < 19)
            {
                current_input[len] = c;
                current_input[len + 1] = '\0';
            }
            page_idx = 0;
            update_keyboard_button();
        }
    }
}

static void del_btn_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        if (current_input[0] == '\0')
        {
            if (phrases_len > 0)
            {
                phrases_len--;
                if (lvgl_port_lock(0))
                {
                    size_t child_count = lv_obj_get_child_count(content);
                    if (child_count > 0)
                    {
                        lv_obj_t *child = lv_obj_get_child(content, child_count - 1);
                        lv_obj_del(child);
                        lv_obj_scroll_to_y(content, 999, LV_ANIM_ON);
                    }
                    lvgl_port_unlock();
                }
            }
        }
        else
        {
            current_input[strlen(current_input) - 1] = '\0';
        }
        page_idx = 0;
        update_keyboard_button();
    }
}

static lv_point_t swipe_start = {0, 0};
static bool swipe_in_progress = false;

static void gesture_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED)
    {
        lv_indev_t *indev = lv_event_get_indev(e);
        if (indev)
        {
            lv_indev_get_point(indev, &swipe_start);
            swipe_in_progress = true;
        }
    }
    else if (code == LV_EVENT_RELEASED)
    {
        if (!swipe_in_progress) return;
        swipe_in_progress = false;

        lv_indev_t *indev = lv_event_get_indev(e);
        if (indev == NULL) return;

        lv_point_t swipe_end = {0, 0};
        lv_indev_get_point(indev, &swipe_end);

        int32_t dx = swipe_end.x - swipe_start.x;
        int32_t dy = swipe_end.y - swipe_start.y;

        /* Only trigger on horizontal swipe with enough distance, not a tap */
        if (LV_ABS(dx) < 30 || LV_ABS(dx) < LV_ABS(dy) * 2) return;

        size_t total_pages = (cue_letter_len + 8) / 9;
        if (total_pages <= 1) return;

        if (dx < 0) /* swipe left = next page */
        {
            page_idx = (page_idx + 1) % total_pages;
        }
        else /* swipe right = prev page */
        {
            page_idx = (page_idx > 0) ? (page_idx - 1) : (total_pages - 1);
        }
        update_keyboard_button();
    }
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void ui_mnemonic_init(lv_obj_t *lv_parent, size_t parent_width, size_t parent_height, lv_obj_t *_event_target, int _mnemonic_type)
{
    mnemonic_type = _mnemonic_type;
    if (mnemonic_type != 12 && mnemonic_type != 24)
    {
        ESP_LOGE(TAG, "mnemonic_type must be 12 or 24 words!");
        return;
    }

    parent = lv_parent;
    event_target = _event_target;

    ALLOC_UTILS_INIT_MEMORY_STRUCT(alloc_utils_memory_struct_pointer);

    if (lvgl_port_lock(0))
    {
        /* get parent size */
        int words_height = 42;
        int keyboard_height = parent_height * 0.48;

        int32_t *col_dsc;
        ALLOC_UTILS_MALLOC_MEMORY(alloc_utils_memory_struct_pointer, col_dsc, sizeof(int32_t) * 2);
        col_dsc[0] = parent_width;
        col_dsc[1] = LV_GRID_TEMPLATE_LAST;

        int32_t *row_dsc;
        ALLOC_UTILS_MALLOC_MEMORY(alloc_utils_memory_struct_pointer, row_dsc, sizeof(int32_t) * 4);
        row_dsc[0] = LV_GRID_FR(1);
        row_dsc[1] = words_height;
        row_dsc[2] = keyboard_height;
        row_dsc[3] = LV_GRID_TEMPLATE_LAST;

        current_page = lv_obj_create(parent);
        lv_obj_set_scroll_dir(current_page, LV_DIR_NONE);
        lv_obj_set_style_grid_column_dsc_array(current_page, col_dsc, 0);
        lv_obj_set_style_grid_row_dsc_array(current_page, row_dsc, 0);
        lv_obj_set_size(current_page, parent_width, parent_height);
        lv_obj_set_layout(current_page, LV_LAYOUT_GRID);
        NO_BODER_PADDING_STYLE(current_page);
        
        lv_obj_set_style_bg_opa(current_page, 255, 0);
        lv_obj_set_style_bg_color(current_page, lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_color(current_page, lv_color_hex(0xffffff), 0);
        const lv_font_t *f = lang_font();
        if (f) {
            lv_obj_set_style_text_font(current_page, f, 0);
        }

        /* content */
        content = lv_obj_create(current_page);

        lv_obj_set_style_border_width(content, 0, 0);
        lv_obj_set_style_radius(content, 0, 0);
        lv_obj_set_style_outline_width(content, 0, 0);
        lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_pad_bottom(content, 40, 0);
        lv_obj_set_style_bg_opa(content, 255, 0);
        lv_obj_set_style_bg_color(content, lv_color_hex(0x000000), 0);

        lv_obj_set_size(content, lv_pct(100), lv_pct(100));
        lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_grid_cell(content, LV_GRID_ALIGN_STRETCH, 0, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);

        /* words_bar container (candidate words list only, full width) */
        lv_obj_t *words_bar = lv_obj_create(current_page);
        NO_BODER_PADDING_STYLE(words_bar);
        lv_obj_set_style_bg_opa(words_bar, 255, 0);
        lv_obj_set_style_bg_color(words_bar, lv_color_hex(0x000000), 0);
        lv_obj_set_size(words_bar, lv_pct(100), lv_pct(100));
        lv_obj_set_flex_flow(words_bar, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(words_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_grid_cell(words_bar, LV_GRID_ALIGN_STRETCH, 0, 1,
                             LV_GRID_ALIGN_STRETCH, 1, 1);
        lv_obj_set_style_pad_left(words_bar, 5, 0);
        lv_obj_set_style_pad_right(words_bar, 5, 0);

        /* words */
        words = lv_obj_create(words_bar);
        NO_BODER_PADDING_STYLE(words);
        lv_obj_set_style_bg_opa(words, 255, 0);
        lv_obj_set_style_bg_color(words, lv_color_hex(0x000000), 0);
        lv_obj_set_flex_grow(words, 1);
        lv_obj_set_height(words, lv_pct(100));
        lv_obj_set_flex_flow(words, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(words, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(words, LV_SCROLLBAR_MODE_OFF);

        /* DEL button: floating at top-right corner of screen (in header row) */
        lv_obj_t *scr = lv_scr_act();
        btn_del = lv_button_create(scr);
        lv_obj_set_size(btn_del, 54, 36); /* Same size as back button */
        lv_obj_set_style_bg_opa(btn_del, 0, 0); /* Transparent to blend with header */
        lv_obj_set_style_bg_color(btn_del, lv_color_hex(0x1a202c), 0);
        lv_obj_set_style_border_width(btn_del, 0, 0); /* No border */
        lv_obj_set_style_radius(btn_del, 0, 0);
        lv_obj_set_style_pad_all(btn_del, 0, 0);
        
        int scr_width = lv_obj_get_width(scr);
        lv_obj_set_pos(btn_del, scr_width - 54, 0); /* Align to top right */
        lv_obj_set_style_margin_all(btn_del, 0, 0);
        lv_obj_t *del_label = lv_label_create(btn_del);
        lv_label_set_text(del_label, LV_SYMBOL_BACKSPACE);
        lv_obj_set_style_text_color(del_label, lv_color_hex(0xffffff), 0);
        lv_obj_center(del_label);
        lv_obj_add_event_cb(btn_del, del_btn_event_handler, LV_EVENT_CLICKED, NULL);

        /* keyboard */
        keyboard = lv_buttonmatrix_create(current_page);
        NO_BODER_PADDING_STYLE(keyboard);
        lv_obj_set_style_bg_opa(keyboard, 255, 0);
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_color(keyboard, lv_color_hex(0xffffff), 0);
        
        /* Set styles for the individual buttons inside the matrix */
        lv_obj_set_style_bg_opa(keyboard, 255, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x2d3748), LV_PART_ITEMS);
        lv_obj_set_style_text_color(keyboard, lv_color_hex(0xffffff), LV_PART_ITEMS);
        lv_obj_set_style_border_width(keyboard, 1, LV_PART_ITEMS);
        lv_obj_set_style_border_color(keyboard, lv_color_hex(0x4a5568), LV_PART_ITEMS);
        
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_grid_cell(keyboard, LV_GRID_ALIGN_STRETCH, 0, 1,
                             LV_GRID_ALIGN_STRETCH, 2, 1);

        ALLOC_UTILS_MALLOC_MEMORY(alloc_utils_memory_struct_pointer, phrases, sizeof(char *) * 24);
        phrases_len = 0;
        /*max to 20 letters */
        ALLOC_UTILS_MALLOC_MEMORY(alloc_utils_memory_struct_pointer, current_input, sizeof(char) * 20);
        memset(current_input, 0, sizeof(char) * 20);
        page_idx = 0;
        update_keyboard_button();
        lv_obj_add_event_cb(keyboard, phrase_input_handler, LV_EVENT_CLICKED, NULL);
        /* Swipe detection via press/release on keyboard */
        lv_obj_add_event_cb(keyboard, gesture_event_handler, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(keyboard, gesture_event_handler, LV_EVENT_RELEASED, NULL);
        /* Also detect on main page background for swipes that miss the buttons */
        lv_obj_add_event_cb(current_page, gesture_event_handler, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(current_page, gesture_event_handler, LV_EVENT_RELEASED, NULL);

        lvgl_port_unlock();
    }
}

void ui_mnemonic_destroy(void)
{
    if (lvgl_port_lock(0))
    {
        if (current_page != NULL)
        {
            lv_obj_del(current_page);
            current_page = NULL;
        }
        if (btn_del != NULL)
        {
            lv_obj_del(btn_del);
            btn_del = NULL;
        }
        lvgl_port_unlock();
    }
    ALLOC_UTILS_FREE_MEMORY(alloc_utils_memory_struct_pointer);

    phrases = NULL;
    content = NULL;
    keyboard = NULL;
    words = NULL;
    current_input = NULL;
    cue_from = 0;
    cue_to = 0;
    cue_letter_len = 0;
}