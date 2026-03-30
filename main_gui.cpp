#include <windows.h>
#include <gdiplus.h>
#include <mmsystem.h>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <random>
#include <queue>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include "scenes_data.h"

#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Winmm.lib")

using namespace Gdiplus;

class VisualNovelApp;

// Флаги для состояния приложения
enum AppState {
    STATE_MENU,
    STATE_GAME,
    STATE_LOADING
};

// Глобальные переменные
AppState g_appState = STATE_MENU;
VisualNovelApp* g_gameApp = nullptr;
HWND g_mainHwnd = nullptr;
bool g_isFullscreen = true;
int g_screenWidth = 0;
int g_screenHeight = 0;

// Структура для хранения настроек
struct GameSettings {
    int volume = 100;
    bool sound_enabled = true;
};

// Глобальные настройки
GameSettings g_settings;

enum ControlIds {
    IDC_CHOICE1 = 1101,
    IDC_CHOICE2 = 1102,
    IDC_CHOICE3 = 1103,
    IDC_CHOICE4 = 1104,
    IDC_EXIT = 1202,
    IDC_START_GAME = 2001,
    IDC_SETTINGS = 2002,
    IDC_EXIT_GAME = 2003,
    IDC_VOLUME_UP = 2006,
    IDC_VOLUME_DOWN = 2007,
    IDC_SOUND_TOGGLE = 2008,
    IDC_CLOSE_SETTINGS = 2009
};

struct ActiveSound {
    std::wstring alias;
    int semitone_shift;
};

struct Choice {
    std::wstring text;
    std::function<void()> action;
};

struct ClickableArea {
    RECT rect;
    std::function<void()> action;
};

// ========== СТРУКТУРА ДЛЯ СЦЕН ==========
struct SceneData {
    std::wstring speaker;
    std::wstring text;
    bool has_sprite;
    std::wstring sprite_name;
    std::wstring background_name;
    bool has_choices;
    std::vector<std::pair<std::wstring, std::wstring>> choices;
    std::map<int, SceneData> choice_scenes;
    
    SceneData() : has_sprite(false), has_choices(false) {}
};

// ==================== ЗАГРУЗОЧНЫЙ ЭКРАН ====================
void DrawLoadingScreen(HDC hdc, RECT rc) {
    HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &rc, blackBrush);
    DeleteObject(blackBrush);
    
    SetBkMode(hdc, TRANSPARENT);
    HFONT font = CreateFontW(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Comic Sans MS");
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetTextColor(hdc, RGB(255, 215, 0));
    
    RECT textRect;
    textRect.left = rc.left;
    textRect.top = rc.bottom / 2 - 50;
    textRect.right = rc.right;
    textRect.bottom = rc.bottom / 2;
    
    DrawTextW(hdc, L"ЗАГРУЗКА...", -1, &textRect, DT_CENTER | DT_SINGLELINE);
    
    static int dotCount = 0;
    static DWORD lastTime = 0;
    DWORD currentTime = GetTickCount();
    if (currentTime - lastTime > 500) {
        dotCount = (dotCount + 1) % 4;
        lastTime = currentTime;
    }
    
    std::wstring dots;
    for (int i = 0; i < dotCount; i++) dots += L".";
    
    RECT dotsRect;
    dotsRect.left = rc.left;
    dotsRect.top = rc.bottom / 2 + 20;
    dotsRect.right = rc.right;
    dotsRect.bottom = rc.bottom / 2 + 80;
    DrawTextW(hdc, dots.c_str(), -1, &dotsRect, DT_CENTER | DT_SINGLELINE);
    
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

// ==================== КЛАСС SCENELOADER ====================
class SceneLoader {
public:
    std::map<std::wstring, SceneData> scenes;
    
    bool LoadScenes(const std::wstring& filename) {
        std::string narrow_filename;
        for (wchar_t ch : filename) {
            narrow_filename += static_cast<char>(ch);
        }
        std::ifstream file(narrow_filename);
        if (!file.is_open()) {
            return false;
        }
        
        std::string line;
        SceneData currentScene;
        std::wstring currentSceneName;
        bool inChoices = false;
        bool inChoiceActions = false;
        
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            std::wstring wline = utf8_to_wstring(line);
            
            if (wline[0] == L'[' && wline.back() == L']') {
                if (!currentSceneName.empty()) {
                    scenes[currentSceneName] = currentScene;
                }
                currentSceneName = wline.substr(1, wline.length() - 2);
                currentScene = SceneData();
                inChoices = false;
                inChoiceActions = false;
            }
            else if (wline.find(L':') != std::wstring::npos && !inChoices && !inChoiceActions) {
                size_t colonPos = wline.find(L':');
                currentScene.speaker = wline.substr(0, colonPos);
                
                if (currentScene.speaker == L"Мысли") {
                    currentScene.speaker = L"";
                }
                
                std::wstring rest = wline.substr(colonPos + 1);
                size_t pipePos = rest.find(L'|');
                
                if (pipePos != std::wstring::npos) {
                    currentScene.text = rest.substr(0, pipePos);
                    currentScene.text.erase(currentScene.text.find_last_not_of(L" \t") + 1);
                    
                    std::wstring flags = rest.substr(pipePos + 1);
                    flags.erase(0, flags.find_first_not_of(L" \t"));
                    
                    std::vector<std::wstring> parts;
                    size_t start = 0;
                    size_t commaPos;
                    while ((commaPos = flags.find(L',', start)) != std::wstring::npos) {
                        parts.push_back(flags.substr(start, commaPos - start));
                        start = commaPos + 1;
                    }
                    parts.push_back(flags.substr(start));
                    
                    for (auto& part : parts) {
                        part.erase(0, part.find_first_not_of(L" \t"));
                        part.erase(part.find_last_not_of(L" \t") + 1);
                    }
                    
                    if (parts.size() > 0) {
                        currentScene.has_sprite = (parts[0] == L"true");
                    }
                    if (parts.size() > 1 && !parts[1].empty()) {
                        currentScene.sprite_name = parts[1];
                    }
                    if (parts.size() > 2 && !parts[2].empty()) {
                        currentScene.background_name = parts[2];
                    }
                } else {
                    currentScene.text = rest;
                    currentScene.has_sprite = false;
                    currentScene.background_name = L"room";
                }
                
                currentScene.has_choices = false;
            }
            else if (wline == L"#Выбор") {
                inChoices = true;
                currentScene.has_choices = true;
                currentScene.choices.clear();
            }
            else if (inChoices && wline.find(L"if") == std::wstring::npos) {
                if (wline.size() > 0 && isdigit(wline[0])) {
                    size_t spacePos = wline.find(L' ');
                    if (spacePos != std::wstring::npos) {
                        int choiceNum = std::stoi(wline.substr(0, spacePos));
                        std::wstring choiceText = wline.substr(spacePos + 1);
                        currentScene.choices.push_back({choiceText, L""});
                    }
                }
            }
            else if (wline.find(L"if") == 0) {
                inChoices = false;
                inChoiceActions = true;
                
                size_t spacePos = wline.find(L' ');
                if (spacePos != std::wstring::npos) {
                    size_t colonPos = wline.find(L':');
                    if (colonPos != std::wstring::npos) {
                        int choiceNum = std::stoi(wline.substr(spacePos + 1, colonPos - spacePos - 1));
                        
                        std::wstring choiceText = wline.substr(colonPos + 1);
                        
                        size_t pipePos = choiceText.find(L'|');
                        SceneData choiceScene;
                        
                        if (pipePos != std::wstring::npos) {
                            choiceScene.text = choiceText.substr(0, pipePos);
                            choiceScene.text.erase(choiceScene.text.find_last_not_of(L" \t") + 1);
                            
                            std::wstring flags = choiceText.substr(pipePos + 1);
                            flags.erase(0, flags.find_first_not_of(L" \t"));
                            
                            std::vector<std::wstring> parts;
                            size_t start = 0;
                            size_t commaPos;
                            while ((commaPos = flags.find(L',', start)) != std::wstring::npos) {
                                parts.push_back(flags.substr(start, commaPos - start));
                                start = commaPos + 1;
                            }
                            parts.push_back(flags.substr(start));
                            
                            for (auto& part : parts) {
                                part.erase(0, part.find_first_not_of(L" \t"));
                                part.erase(part.find_last_not_of(L" \t") + 1);
                            }
                            
                            if (parts.size() > 0) {
                                choiceScene.has_sprite = (parts[0] == L"true");
                            }
                            if (parts.size() > 1 && !parts[1].empty()) {
                                choiceScene.sprite_name = parts[1];
                            }
                            if (parts.size() > 2 && !parts[2].empty()) {
                                choiceScene.background_name = parts[2];
                            }
                        } else {
                            choiceScene.text = choiceText;
                            choiceScene.has_sprite = false;
                        }
                        
                        choiceScene.speaker = L"";
                        choiceScene.has_choices = false;
                        
                        currentScene.choice_scenes[choiceNum] = choiceScene;
                    }
                }
            }
        }
        
        if (!currentSceneName.empty()) {
            scenes[currentSceneName] = currentScene;
        }
        
        file.close();
        return true;
    }
    
private:
    std::wstring utf8_to_wstring(const std::string& str) {
        if (str.empty()) return L"";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
        return wstr;
    }
};

// ==================== КЛАСС ОСНОВНОЙ ИГРЫ ====================
class VisualNovelApp {
public:
    explicit VisualNovelApp(HWND hwnd)
        : hwnd_(hwnd), text_index_(0), speed_ms_(6), has_key_(false), 
        sound_index_(0), waiting_for_click_(false), sprite_needs_update_(true) {
        srand(static_cast<unsigned>(time(nullptr)));
        chars_per_tick_ = 2;
        load_images();
        preload_sprites();
        create_controls();
    }
    
    ~VisualNovelApp() {
        for (auto* img : images_) delete img;
        if (scaled_bg_cache_) delete scaled_bg_cache_;
        if (scaled_character_cache_) delete scaled_character_cache_;
        sprite_cache_.clear();
        
        for (const auto& sound : active_sounds_) {
            std::wstring close_cmd = L"close " + sound.alias;
            mciSendStringW(close_cmd.c_str(), nullptr, 0, nullptr);
        }
        active_sounds_.clear();
        
        if (sound_opened_) {
            mciSendStringW(L"close textsnd", nullptr, 0, nullptr);
        }
        if (ui_font_) DeleteObject(ui_font_);
    }
    
    void initialize_game() {
        start_game();
    }

    void show() {
        ShowWindow(exit_btn_, SW_SHOW);
        // Если игра еще не инициализирована, запускаем
        if (!game_initialized_) {
            initialize_game();
            game_initialized_ = true;
        }
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
    
    void hide() {
        ShowWindow(exit_btn_, SW_HIDE);
        // Скрываем кнопки выбора
        for (int i = 0; i < 4; ++i) {
            ShowWindow(choice_btns_[i], SW_HIDE);
        }
    }
    
    void start_game() {
        previous_choice_.clear();
        has_key_ = false;
        clear_text();
        
        scene_data_.Initialize();
        
        if (scene_data_.scenes.empty()) {
            MessageBoxW(hwnd_, L"Не удалось загрузить сцены!", L"Ошибка", MB_OK);
            return;
        }
        
        play_scene(L"scene_32");
    }
    
    void reset_game() {
        previous_choice_.clear();
        has_key_ = false;
        text_index_ = 0;
        sound_counter_ = 0;
        current_choices_.clear();
        after_animation_ = nullptr;
        waiting_for_click_ = false;
        on_click_action_ = nullptr;
        
        keep_sprite_ = false;
        sprite_needs_update_ = true;
        character_sprite_ = nullptr;
        
        if (scaled_character_cache_) {
            delete scaled_character_cache_;
            scaled_character_cache_ = nullptr;
        }
        
        clear_text();
        start_game();
    }
    
    void on_paint(HDC hdc) {
        RECT rc;
        GetClientRect(hwnd_, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
    
        HDC memdc = CreateCompatibleDC(hdc);
        HBITMAP back = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ old = SelectObject(memdc, back);
    
        Graphics g(memdc);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    
        // Отрисовка фона
        if (current_bg_) {
            ensure_scaled_background(w, h);
            if (scaled_bg_cache_) {
                g.DrawImage(scaled_bg_cache_, 0, 0, w, h);
            }
        } else {
            SolidBrush b(Color(255, 44, 62, 80));
            g.FillRectangle(&b, 0, 0, rc.right, rc.bottom);
        }
    
        // Отрисовка спрайта
        if (character_sprite_) {
            int sprite_width = 300;
            int sprite_height = 420;
            int sprite_x = 33;
            int sprite_y = h - sprite_height - 200;
            
            if (sprite_needs_update_) {
                if (scaled_character_cache_) {
                    delete scaled_character_cache_;
                    scaled_character_cache_ = nullptr;
                }
                scaled_character_cache_ = new Bitmap(sprite_width, sprite_height, PixelFormat32bppARGB);
                Graphics char_g(scaled_character_cache_);
                char_g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
                char_g.DrawImage(character_sprite_, 0, 0, sprite_width, sprite_height);
                sprite_needs_update_ = false;
            }
            
            if (scaled_character_cache_) {
                g.DrawImage(scaled_character_cache_, sprite_x, sprite_y, sprite_width, sprite_height);
            }
        }
    
        // Панель диалога
        const int panel_x = 24;
        const int panel_y = rc.bottom - 285;
        const int panel_w = w - 48;
        const int panel_h = 170;
        
        text_area_.rect.left = panel_x;
        text_area_.rect.right = panel_x + panel_w;
        text_area_.rect.top = panel_y;
        text_area_.rect.bottom = panel_y + panel_h;
        
        SolidBrush panel_brush(Color(165, 22, 24, 38));
        g.FillRectangle(&panel_brush, panel_x, panel_y, panel_w, panel_h);
    
        // Область с именем
        GraphicsPath name_path;
        const int name_x = 50;
        const int name_y = panel_y - 36;
        const int name_w = 230;
        const int name_h = 42;
        name_path.StartFigure();
        name_path.AddLine(name_x + 20, name_y, name_x + name_w - 20, name_y);
        name_path.AddLine(name_x + name_w - 20, name_y, name_x + name_w, name_y + name_h / 2);
        name_path.AddLine(name_x + name_w, name_y + name_h / 2, name_x + name_w - 20, name_y + name_h);
        name_path.AddLine(name_x + name_w - 20, name_y + name_h, name_x + 20, name_y + name_h);
        name_path.AddLine(name_x + 20, name_y + name_h, name_x, name_y + name_h / 2);
        name_path.AddLine(name_x, name_y + name_h / 2, name_x + 20, name_y);
        name_path.CloseFigure();
        SolidBrush name_brush(Color(205, 110, 78, 100));
        Pen name_pen(Color(230, 220, 220, 235), 2.0f);
        g.FillPath(&name_brush, &name_path);
        g.DrawPath(&name_pen, &name_path);
    
        FontFamily ff(L"Comic Sans MS");
        Font name_font(&ff, 16.0f, FontStyleBold, UnitPixel);
        Font text_font(&ff, 16.0f, FontStyleRegular, UnitPixel);
        SolidBrush text_brush(Color(245, 238, 238, 248));
    
        if (!current_speaker_.empty()) {
            StringFormat center_fmt;
            center_fmt.SetAlignment(StringAlignmentCenter);
            center_fmt.SetLineAlignment(StringAlignmentCenter);
            RectF name_rect((REAL)name_x, (REAL)name_y, (REAL)name_w, (REAL)name_h);
            g.DrawString(current_speaker_.c_str(), -1, &name_font, name_rect, &center_fmt, &text_brush);
        }
    
        StringFormat body_fmt;
        body_fmt.SetAlignment(StringAlignmentNear);
        body_fmt.SetLineAlignment(StringAlignmentNear);
        body_fmt.SetFormatFlags(StringFormatFlagsLineLimit);
        RectF body_rect((REAL)panel_x + 18, (REAL)panel_y + 18, (REAL)panel_w - 36, (REAL)panel_h - 30);
        g.DrawString(shown_text_.c_str(), -1, &text_font, body_rect, &body_fmt, &text_brush);
    
        BitBlt(hdc, 0, 0, w, h, memdc, 0, 0, SRCCOPY);
        SelectObject(memdc, old);
        DeleteObject(back);
        DeleteDC(memdc);
    
        // Индикатор ожидания клика
        if (waiting_for_click_) {
            static int blink_counter = 0;
            static DWORD last_blink_time = 0;
            DWORD current_time = GetTickCount();
            
            if (current_time - last_blink_time > 500) {
                blink_counter = (blink_counter + 1) % 2;
                last_blink_time = current_time;
            }
            
            if (blink_counter == 0) {
                HBRUSH indicatorBg = CreateSolidBrush(RGB(255, 215, 0));
                RECT indicatorRect;
                indicatorRect.left = panel_x + panel_w - 40;
                indicatorRect.top = panel_y + panel_h - 30;
                indicatorRect.right = panel_x + panel_w - 15;
                indicatorRect.bottom = panel_y + panel_h - 10;
                FillRect(hdc, &indicatorRect, indicatorBg);
                DeleteObject(indicatorBg);
            }
        }
    }
    
    void set_sound_enabled(bool enabled) {
        sound_enabled_ = enabled;
    }
    
    bool IsWaitingForClick() const { return waiting_for_click_; }
    RECT GetTextAreaRect() const { return text_area_.rect; }
    
    void OnTextAreaClick() {
        if (waiting_for_click_ && on_click_action_) {
            waiting_for_click_ = false;
            auto action = on_click_action_;
            on_click_action_ = nullptr;
            action();
            return;
        }
    }
    
    void WaitForClick(std::function<void()> after) {
        waiting_for_click_ = true;
        on_click_action_ = after;
        RECT dirty = dialogue_dirty_rect();
        InvalidateRect(hwnd_, &dirty, FALSE);
    }
    
    void on_resize() {
        RECT rc;
        GetClientRect(hwnd_, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        MoveWindow(exit_btn_, w - 105, 10, 95, 30, TRUE);
        
        if (!current_choices_.empty()) {
            int panel_y = h - 285;
            int choice_y = panel_y - 80;
            int btn_h = 45;
            int btn_w = 500;
            int btn_x = (w - btn_w) / 2;
            for (int i = 0; i < static_cast<int>(current_choices_.size()); ++i) {
                MoveWindow(choice_btns_[i], btn_x, choice_y - (current_choices_.size() - i) * (btn_h + 8), btn_w, btn_h, TRUE);
            }
        }
    }
    
    void on_command(WORD id) {
        if (id == IDC_EXIT) {
            g_appState = STATE_MENU;
            InvalidateRect(hwnd_, nullptr, TRUE);
            return;
        }
    
        if (id >= IDC_CHOICE1 && id <= IDC_CHOICE4) {
            int idx = id - IDC_CHOICE1;
            if (idx >= 0 && idx < static_cast<int>(current_choices_.size())) {
                disable_choices();
                auto cb = current_choices_[idx].action;
                if (cb) {
                    cb();
                }
                return;
            }
        }
    }
    
    void on_timer(UINT_PTR timer_id) {
        if (timer_id != 1) return;
        animate_text_step();
    }
    
private:
    HWND hwnd_;
    HWND choice_btns_[4]{};
    HWND exit_btn_{};
    HFONT ui_font_{};

    bool game_initialized_ = false;

    std::vector<Image*> images_;
    Image* bg_room_{};
    Image* bg_corridor_{};
    Image* bg_kitchen_{};
    Image* bg_basement_{};
    Image* bg_bathroom_{};
    Image* bg_ending_{};
    Image* current_bg_{};
    Bitmap* scaled_bg_cache_{};
    Image* scaled_bg_source_{};
    int scaled_bg_w_{0};
    int scaled_bg_h_{0};

    std::wstring current_text_;
    std::wstring shown_text_;
    std::wstring current_speaker_ = L"Нарратор";
    size_t text_index_;
    int speed_ms_;
    int chars_per_tick_ = 1;
    int sound_counter_ = 0;
    bool sound_opened_ = true;
    static constexpr const wchar_t* kTextSoundFile = L"sounds\\soundtext.mp3";
    
    std::vector<ActiveSound> active_sounds_;
    int sound_index_ = 0;
    static constexpr int MAX_CONCURRENT_SOUNDS = 10;

    std::wstring previous_choice_;
    bool has_key_;
    std::vector<Choice> current_choices_;
    std::function<void()> after_animation_;
    
    ClickableArea text_area_;
    bool waiting_for_click_ = false;
    std::function<void()> on_click_action_;
    static constexpr int BLINK_INTERVAL_MS = 300;
    
    Image* character_sprite_{nullptr};
    Bitmap* scaled_character_cache_{nullptr};
    bool keep_sprite_{false};
    std::wstring current_sprite_path_;
    bool sound_enabled_{true};
    bool sprite_needs_update_{true};
    
    std::map<std::wstring, Image*> sprite_cache_;
    
    SceneDataBase scene_data_;
    std::wstring next_scene_;
    
    void load_images() {
        bg_room_ = load_one(L"backgrounds\\room.jpg");
        bg_corridor_ = load_one(L"backgrounds\\corridor.jpg");
        bg_kitchen_ = load_one(L"backgrounds\\kitchen.jpg");
        bg_bathroom_ = load_one(L"backgrounds\\bathroom.jpg");
        bg_basement_ = load_one(L"backgrounds\\basement.jpg");
        bg_ending_ = load_one(L"backgrounds\\ending_good.jpg");
        
        if (!bg_ending_) {
            bg_ending_ = load_one(L"backgrounds\\edning_good.jpg");
        }
    }
    
    Image* load_one(const wchar_t* path) {
        auto* img = new Image(path);
        if (img->GetLastStatus() != Ok) {
            delete img;
            return nullptr;
        }
        images_.push_back(img);
        return img;
    }
    
    void preload_sprites() {
        std::vector<std::wstring> sprites = {
            L"luis.png",
            L"mom.png",
            L"luis_mirror.png"
        };
        
        for (const auto& sprite_name : sprites) {
            std::wstring sprite_path = L"characters\\" + sprite_name;
            if (fileExists(sprite_path)) {
                Image* img = new Image(sprite_path.c_str());
                if (img->GetLastStatus() == Ok) {
                    sprite_cache_[sprite_name] = img;
                    images_.push_back(img);
                } else {
                    delete img;
                }
            }
        }
    }
    
    void create_controls() {
        ui_font_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Comic Sans MS");
        
        for (int i = 0; i < 4; ++i) {
            int id = IDC_CHOICE1 + i;
            choice_btns_[i] = CreateWindowW(L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
                60, 670 + i * 40, 900, 45, hwnd_, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(choice_btns_[i], WM_SETFONT, (WPARAM)ui_font_, TRUE);
            EnableWindow(choice_btns_[i], FALSE);
            ShowWindow(choice_btns_[i], SW_HIDE);
        }
        
        exit_btn_ = CreateWindowW(L"BUTTON", L"Выйти в меню", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            920, 10, 95, 30, hwnd_, (HMENU)IDC_EXIT, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(exit_btn_, WM_SETFONT, (WPARAM)ui_font_, TRUE);
        on_resize();
    }
    
    void set_background(Image* bg) {
        current_bg_ = bg;
        scaled_bg_source_ = nullptr;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
    
    void set_background_by_name(const std::wstring& bg_name) {
        if (bg_name == L"room") set_background(bg_room_);
        else if (bg_name == L"corridor") set_background(bg_corridor_);
        else if (bg_name == L"kitchen") set_background(bg_kitchen_);
        else if (bg_name == L"basement") set_background(bg_basement_);
        else if (bg_name == L"bathroom") set_background(bg_bathroom_);
        else if (bg_name == L"ending") set_background(bg_ending_);
        else set_background(bg_room_);
    }
    
    void ensure_scaled_background(int w, int h) {
        if (!current_bg_ || w <= 0 || h <= 0) return;
        
        if (scaled_bg_cache_ && scaled_bg_source_ == current_bg_ && 
            scaled_bg_w_ == w && scaled_bg_h_ == h) {
            return;
        }
        
        if (scaled_bg_cache_) {
            delete scaled_bg_cache_;
            scaled_bg_cache_ = nullptr;
        }
        
        scaled_bg_cache_ = new Bitmap(w, h, PixelFormat32bppARGB);
        Graphics bg_g(scaled_bg_cache_);
        bg_g.SetInterpolationMode(InterpolationModeHighQualityBilinear);
        bg_g.DrawImage(current_bg_, 0, 0, w, h);
        
        scaled_bg_source_ = current_bg_;
        scaled_bg_w_ = w;
        scaled_bg_h_ = h;
    }
    
    int semitones_to_tempo(int semitones) {
        return static_cast<int>(pow(2.0, semitones / 12.0) * 100.0);
    }
    
    void play_type_sound(wchar_t ch) {
        if (!sound_enabled_) return;
        if (ch == L' ' || ch == L'\r' || ch == L'\n' || ch == L'\t') return;
        
        sound_counter_++;
        if ((sound_counter_ % 7) != 0) return;
        
        static bool soundLoaded = false;
        static std::vector<BYTE> soundBuffer;
        
        if (!soundLoaded) {
            std::wstring path = L"sounds\\type.wav";
            HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD fileSize = GetFileSize(hFile, NULL);
                soundBuffer.resize(fileSize);
                DWORD bytesRead;
                ReadFile(hFile, soundBuffer.data(), fileSize, &bytesRead, NULL);
                CloseHandle(hFile);
                soundLoaded = true;
            }
        }
        
        if (soundLoaded && !soundBuffer.empty()) {
            PlaySoundW((LPCWSTR)soundBuffer.data(), NULL, SND_MEMORY | SND_ASYNC);
        } else {
            Beep(850, 20);
        }
    }
    
    RECT dialogue_dirty_rect() const {
        RECT rc, dirty;
        GetClientRect(hwnd_, &rc);
        dirty.left = 24;
        dirty.right = rc.right - 24;
        dirty.top = rc.bottom - 325;
        dirty.bottom = rc.bottom - 105;
        return dirty;
    }
    
    void clear_text() {
        shown_text_.clear();
        RECT dirty = dialogue_dirty_rect();
        InvalidateRect(hwnd_, &dirty, FALSE);
    }
    
    void set_text_box(const std::wstring& text) {
        shown_text_ = text;
        RECT dirty = dialogue_dirty_rect();
        InvalidateRect(hwnd_, &dirty, FALSE);
    }
    
    bool fileExists(const std::wstring& path) {
        DWORD attrib = GetFileAttributesW(path.c_str());
        return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
    }
    
    void set_character_sprite(const std::wstring& sprite_filename, bool keep = false) {
        keep_sprite_ = keep;
        current_sprite_path_ = sprite_filename;
        
        auto it = sprite_cache_.find(sprite_filename);
        if (it != sprite_cache_.end()) {
            character_sprite_ = it->second;
        } else {
            character_sprite_ = nullptr;
        }
        
        if (scaled_character_cache_) {
            delete scaled_character_cache_;
            scaled_character_cache_ = nullptr;
        }
        
        sprite_needs_update_ = true;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
    
    void set_character_sprite_by_name(const std::wstring& character_name, bool keep = false) {
        std::wstring sprite_filename;
        
        if (character_name == L"Луис") {
            sprite_filename = L"luis.png";
        } 
        else if (character_name == L"Мама") {
            sprite_filename = L"mom.png";
        }
        else if (character_name == L"Луис_зеркало") {
            sprite_filename = L"luis_mirror.png";
        }
        else {
            hide_character_sprite();
            return;
        }
        
        set_character_sprite(sprite_filename, keep);
    }
    
    void hide_character_sprite() {
        character_sprite_ = nullptr;
        sprite_needs_update_ = true;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
    
    void animate_text(const std::wstring& text, 
        std::function<void()> after = nullptr, 
        const std::wstring& speaker = L"Нарратор", 
        bool has_choices = false) {
        KillTimer(hwnd_, 1);
        disable_choices();
        waiting_for_click_ = false;
        on_click_action_ = nullptr;
        
        sound_counter_ = 0;
        
        if (!has_choices && after) {
            auto final_action = after;
            after = [this, final_action]() { 
                WaitForClick(final_action); 
            };
        }
        
        current_speaker_ = speaker;
        current_text_ = text + L"\r\n";
        shown_text_.clear();
        text_index_ = 0;
        after_animation_ = std::move(after);
        set_text_box(L"");
        SetTimer(hwnd_, 1, speed_ms_, nullptr);
    }
    
    void animate_text_step() {
        if (text_index_ < current_text_.size()) {
            int remaining = static_cast<int>(current_text_.size() - text_index_);
            int chunk = std::min(chars_per_tick_, remaining);
            for (int i = 0; i < chunk; ++i) {
                wchar_t ch = current_text_[text_index_ + static_cast<size_t>(i)];
                shown_text_.push_back(ch);
                play_type_sound(ch);
            }
            text_index_ += static_cast<size_t>(chunk);
            set_text_box(shown_text_);
            return;
        }
        
        KillTimer(hwnd_, 1);
        
        if (after_animation_) {
            auto cb = after_animation_;
            after_animation_ = nullptr;
            cb();
        }
    }
    
    void set_choices(const std::vector<Choice>& choices) {
        current_choices_ = choices;
        
        RECT rc;
        GetClientRect(hwnd_, &rc);
        
        int panel_x = 24;
        int panel_y = rc.bottom - 285;
        int panel_w = rc.right - 48;
        int btn_h = 45;
        int btn_w = static_cast<int>(panel_w * 0.6);
        int btn_spacing = 10;
        int btn_x = panel_x + (panel_w - btn_w) / 2;
        int choice_y = panel_y - 20;
        
        for (int i = 0; i < 4; ++i) {
            ShowWindow(choice_btns_[i], SW_HIDE);
            EnableWindow(choice_btns_[i], FALSE);
        }
        
        for (int i = 0; i < static_cast<int>(choices.size()); ++i) {
            std::wstring label = std::to_wstring(i + 1) + L". " + choices[i].text;
            SetWindowTextW(choice_btns_[i], label.c_str());
            
            int btn_y = choice_y - (choices.size() - i) * (btn_h + btn_spacing);
            
            MoveWindow(choice_btns_[i], btn_x, btn_y, btn_w, btn_h, TRUE);
            ShowWindow(choice_btns_[i], SW_SHOW);
            EnableWindow(choice_btns_[i], TRUE);
            InvalidateRect(choice_btns_[i], nullptr, TRUE);
        }
    }
    
    void disable_choices() {
        for (auto* btn : choice_btns_) {
            EnableWindow(btn, FALSE);
            ShowWindow(btn, SW_HIDE);
        }
        current_choices_.clear();
        waiting_for_click_ = false;
        on_click_action_ = nullptr;
    }
    
    void play_scene(const std::wstring& scene_name) {
        auto it = scene_data_.scenes.find(scene_name);
        if (it == scene_data_.scenes.end()) {
            MessageBoxW(hwnd_, (L"Сцена не найдена: " + scene_name).c_str(), L"Ошибка", MB_OK);
            return;
        }
        
        SceneNode& scene = it->second;
        
        if (!scene.background_name.empty()) {
            set_background_by_name(scene.background_name);
        }
        
        if (scene.has_sprite && !scene.sprite_name.empty()) {
            set_character_sprite_by_name(scene.sprite_name, true);
        } else {
            hide_character_sprite();
        }
        
        std::wstring next_scene = scene_data_.GetNextScene(scene_name);
        
        if (scene.has_choices && !scene.choices.empty()) {
            animate_text(
                scene.text,
                [this, scene]() {
                    std::vector<Choice> choices;
                    for (size_t i = 0; i < scene.choices.size(); i++) {
                        int choice_num = (int)i + 1;
                        auto it_target = scene.choice_targets.find(choice_num);
                        if (it_target != scene.choice_targets.end()) {
                            std::wstring target_scene = it_target->second;
                            choices.push_back({
                                scene.choices[i].first,
                                [this, target_scene]() { 
                                    play_scene(target_scene); 
                                }
                            });
                        }
                    }
                    set_choices(choices);
                },
                scene.speaker,
                true
            );
        } else {
            std::function<void()> next_action = nullptr;
            if (!next_scene.empty()) {
                next_action = [this, next_scene]() { 
                    play_scene(next_scene); 
                };
            }
            
            animate_text(
                scene.text,
                next_action,
                scene.speaker,
                false
            );
        }
    }
};

// ==================== ГЛАВНОЕ МЕНЮ ====================
class MainMenu {
public:
    MainMenu(HWND hwnd) : hwnd_(hwnd), show_settings_(false) {
        create_controls();
    }
    
    ~MainMenu() {
        if (title_font_) DeleteObject(title_font_);
        if (button_font_) DeleteObject(button_font_);
        if (settings_font_) DeleteObject(settings_font_);
    }
    
    void on_paint(HDC hdc) {
        RECT rc;
        GetClientRect(hwnd_, &rc);
        
        HDC memdc = CreateCompatibleDC(hdc);
        HBITMAP back = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ old = SelectObject(memdc, back);
        
        for (int y = 0; y < rc.bottom; y++) {
            COLORREF color = RGB(
                20 + (y * 15 / rc.bottom),
                15 + (y * 10 / rc.bottom),
                35 + (y * 20 / rc.bottom)
            );
            HPEN pen = CreatePen(PS_SOLID, 1, color);
            HBRUSH brush = CreateSolidBrush(color);
            SelectObject(memdc, pen);
            SelectObject(memdc, brush);
            Rectangle(memdc, 0, y, rc.right, y + 1);
            DeleteObject(pen);
            DeleteObject(brush);
        }
        
        HPEN goldPen = CreatePen(PS_SOLID, 2, RGB(218, 165, 32));
        HBRUSH oldBrush = (HBRUSH)SelectObject(memdc, GetStockObject(NULL_BRUSH));
        
        for (int i = 0; i < 3; i++) {
            SelectObject(memdc, goldPen);
            Ellipse(memdc, 50 + i * 150, 50, 150 + i * 150, 150);
            Ellipse(memdc, rc.right - 150 - i * 150, rc.bottom - 150, rc.right - 50 - i * 150, rc.bottom - 50);
        }
        
        SelectObject(memdc, oldBrush);
        DeleteObject(goldPen);
        
        SetBkMode(memdc, TRANSPARENT);
        HFONT oldFont = (HFONT)SelectObject(memdc, title_font_);
        RECT titleRect = {0, rc.bottom / 4 - 60, rc.right, rc.bottom / 4};
        SetTextColor(memdc, RGB(255, 215, 0));
        DrawTextW(memdc, L"Майами: уроки свободы", -1, &titleRect, DT_CENTER | DT_SINGLELINE);
        
        if (show_settings_) {
            RECT settingsRect = {rc.right / 2 - 200, rc.bottom / 6 - 60, rc.right / 2 + 200, rc.bottom / 6 + 200};
            HBRUSH darkBrush = CreateSolidBrush(RGB(30, 30, 45));
            FillRect(memdc, &settingsRect, darkBrush);
            DeleteObject(darkBrush);
            
            HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(218, 165, 32));
            SelectObject(memdc, borderPen);
            Rectangle(memdc, settingsRect.left, settingsRect.top, settingsRect.right, settingsRect.bottom);
            DeleteObject(borderPen);
            
            SelectObject(memdc, settings_font_);
            RECT settingsTitleRect = settingsRect;
            settingsTitleRect.top += 10;
            settingsTitleRect.bottom = settingsTitleRect.top + 35;
            SetTextColor(memdc, RGB(255, 215, 0));
            DrawTextW(memdc, L"Настройки звука", -1, &settingsTitleRect, DT_CENTER | DT_SINGLELINE);
            
            SelectObject(memdc, settings_font_);
            SetTextColor(memdc, RGB(200, 200, 220));
            RECT volTextRect = {settingsRect.left + 20, settingsRect.top + 55, settingsRect.right - 100, settingsRect.top + 85};
            DrawTextW(memdc, L"Громкость:", -1, &volTextRect, DT_LEFT | DT_SINGLELINE);
            
            wchar_t volText[32];
            wsprintfW(volText, L"%d%%", g_settings.volume);
            RECT volValueRect = {settingsRect.left + 130, settingsRect.top + 55, settingsRect.right - 50, settingsRect.top + 85};
            DrawTextW(memdc, volText, -1, &volValueRect, DT_LEFT | DT_SINGLELINE);
            
            RECT soundTextRect = {settingsRect.left + 20, settingsRect.top + 95, settingsRect.right - 100, settingsRect.top + 125};
            DrawTextW(memdc, g_settings.sound_enabled ? L"Звук: Включен" : L"Звук: Выключен", -1, &soundTextRect, DT_LEFT | DT_SINGLELINE);
        }
        
        SelectObject(memdc, button_font_);
        SetTextColor(memdc, RGB(200, 200, 220));
        
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memdc, 0, 0, SRCCOPY);
        SelectObject(memdc, oldFont);
        DeleteObject(back);
        DeleteDC(memdc);
    }
    
    void show() {
        ShowWindow(start_btn_, SW_SHOW);
        ShowWindow(settings_btn_, SW_SHOW);
        ShowWindow(exit_btn_, SW_SHOW);
        // Если настройки открыты, показываем их кнопки
        if (show_settings_) {
            ShowWindow(vol_down_btn_, SW_SHOW);
            ShowWindow(vol_up_btn_, SW_SHOW);
            ShowWindow(sound_toggle_btn_, SW_SHOW);
            ShowWindow(close_settings_btn_, SW_SHOW);
        }
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
    
    void hide() {
        ShowWindow(start_btn_, SW_HIDE);
        ShowWindow(settings_btn_, SW_HIDE);
        ShowWindow(exit_btn_, SW_HIDE);
        ShowWindow(vol_down_btn_, SW_HIDE);
        ShowWindow(vol_up_btn_, SW_HIDE);
        ShowWindow(sound_toggle_btn_, SW_HIDE);
        ShowWindow(close_settings_btn_, SW_HIDE);
        show_settings_ = false;
    }

    void on_command(WORD id) {
        if (id == IDC_START_GAME) {
            g_appState = STATE_GAME;
            if (g_gameApp) {
                g_gameApp->reset_game();
            } else {
                g_gameApp = new VisualNovelApp(hwnd_);
            }
            InvalidateRect(hwnd_, nullptr, TRUE);
        }
        else if (id == IDC_SETTINGS) {
            show_settings_ = !show_settings_;
            ShowWindow(vol_down_btn_, show_settings_ ? SW_SHOW : SW_HIDE);
            ShowWindow(vol_up_btn_, show_settings_ ? SW_SHOW : SW_HIDE);
            ShowWindow(sound_toggle_btn_, show_settings_ ? SW_SHOW : SW_HIDE);
            ShowWindow(close_settings_btn_, show_settings_ ? SW_SHOW : SW_HIDE);
            InvalidateRect(hwnd_, nullptr, TRUE);
        }
        else if (id == IDC_VOLUME_UP) {
            change_volume(10);
        }
        else if (id == IDC_VOLUME_DOWN) {
            change_volume(-10);
        }
        else if (id == IDC_SOUND_TOGGLE) {
            g_settings.sound_enabled = !g_settings.sound_enabled;
            if (g_gameApp) {
                g_gameApp->set_sound_enabled(g_settings.sound_enabled);
            }
            InvalidateRect(hwnd_, nullptr, TRUE);
        }
        else if (id == IDC_CLOSE_SETTINGS) {
            show_settings_ = false;
            ShowWindow(vol_down_btn_, SW_HIDE);
            ShowWindow(vol_up_btn_, SW_HIDE);
            ShowWindow(sound_toggle_btn_, SW_HIDE);
            ShowWindow(close_settings_btn_, SW_HIDE);
            InvalidateRect(hwnd_, nullptr, TRUE);
        }
        else if (id == IDC_EXIT_GAME) {
            PostQuitMessage(0);
        }
    }
    
    void on_resize() {
        RECT rc;
        GetClientRect(hwnd_, &rc);
        int centerX = rc.right / 2;
        
        MoveWindow(start_btn_, centerX - 150, rc.bottom / 2 - 40, 300, 50, TRUE);
        MoveWindow(settings_btn_, centerX - 150, rc.bottom / 2 + 30, 300, 50, TRUE);
        MoveWindow(exit_btn_, centerX - 150, rc.bottom / 2 + 100, 300, 50, TRUE);
        
        int settingsY = rc.bottom / 6 - 20;
        int settingsCenterX = rc.right / 2;
        
        MoveWindow(vol_down_btn_, settingsCenterX - 20, settingsY + 10, 40, 35, TRUE);
        MoveWindow(vol_up_btn_, settingsCenterX + 60, settingsY + 10, 40, 35, TRUE);
        MoveWindow(sound_toggle_btn_, settingsCenterX - 190, settingsY + 95, 200, 35, TRUE);
        MoveWindow(close_settings_btn_, settingsCenterX - 75, settingsY + 170, 150, 35, TRUE);
    }
    
private:
    HWND hwnd_;
    HWND start_btn_;
    HWND settings_btn_;
    HWND exit_btn_;
    HFONT title_font_;
    HFONT button_font_;
    HFONT settings_font_;
    bool show_settings_;
    
    HWND vol_down_btn_;
    HWND vol_up_btn_;
    HWND sound_toggle_btn_;
    HWND close_settings_btn_;
    
    void create_controls() {
        title_font_ = CreateFontW(-48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Comic Sans MS");
        
        button_font_ = CreateFontW(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Comic Sans MS");
        
        settings_font_ = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        
        start_btn_ = CreateWindowW(L"BUTTON", L"Начать игру", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 300, 50, hwnd_, (HMENU)IDC_START_GAME, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(start_btn_, WM_SETFONT, (WPARAM)button_font_, TRUE);
        
        settings_btn_ = CreateWindowW(L"BUTTON", L"Настройки", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 300, 50, hwnd_, (HMENU)IDC_SETTINGS, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(settings_btn_, WM_SETFONT, (WPARAM)button_font_, TRUE);
        
        exit_btn_ = CreateWindowW(L"BUTTON", L"Выход", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 300, 50, hwnd_, (HMENU)IDC_EXIT_GAME, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(exit_btn_, WM_SETFONT, (WPARAM)button_font_, TRUE);
        
        vol_down_btn_ = CreateWindowW(L"BUTTON", L"◀", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 40, 35, hwnd_, (HMENU)IDC_VOLUME_DOWN, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(vol_down_btn_, WM_SETFONT, (WPARAM)settings_font_, TRUE);
        ShowWindow(vol_down_btn_, SW_HIDE);
        
        vol_up_btn_ = CreateWindowW(L"BUTTON", L"▶", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 40, 35, hwnd_, (HMENU)IDC_VOLUME_UP, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(vol_up_btn_, WM_SETFONT, (WPARAM)settings_font_, TRUE);
        ShowWindow(vol_up_btn_, SW_HIDE);
        
        sound_toggle_btn_ = CreateWindowW(L"BUTTON", L"Вкл/Выкл звук", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 200, 35, hwnd_, (HMENU)IDC_SOUND_TOGGLE, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(sound_toggle_btn_, WM_SETFONT, (WPARAM)settings_font_, TRUE);
        ShowWindow(sound_toggle_btn_, SW_HIDE);
        
        close_settings_btn_ = CreateWindowW(L"BUTTON", L"Закрыть", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 150, 35, hwnd_, (HMENU)IDC_CLOSE_SETTINGS, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(close_settings_btn_, WM_SETFONT, (WPARAM)settings_font_, TRUE);
        ShowWindow(close_settings_btn_, SW_HIDE);
        
        on_resize();
    }
    
    void change_volume(int delta) {
        int new_volume = g_settings.volume + delta;
        if (new_volume >= 0 && new_volume <= 100) {
            g_settings.volume = new_volume;
            InvalidateRect(hwnd_, nullptr, TRUE);
        }
    }
};

// ==================== ОБРАБОТЧИК ГЛАВНОГО ОКНА ====================
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static MainMenu* menu = nullptr;
    static HBRUSH darkBrush = CreateSolidBrush(RGB(24, 26, 40));
    
    switch (msg) {
    case WM_CREATE: {
        menu = new MainMenu(hwnd);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)menu);
        
        g_screenWidth = GetSystemMetrics(SM_CXSCREEN);
        g_screenHeight = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, g_screenWidth, g_screenHeight, SWP_SHOWWINDOW);
        return 0;
    }
    case WM_SIZE:
        if (menu) menu->on_resize();
        if (g_gameApp) g_gameApp->on_resize();
        return 0;
        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            
            if (g_appState == STATE_MENU && menu) {
                if (id == IDC_START_GAME) {
                    // Скрываем меню
                    menu->hide();
                    
                    // Создаем игру если еще не создана
                    if (!g_gameApp) {
                        g_gameApp = new VisualNovelApp(hwnd);
                    }
                    
                    // Показываем игру (инициализация произойдет в show())
                    g_gameApp->show();
                    g_appState = STATE_GAME;
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                else if (id == IDC_EXIT_GAME) {
                    PostQuitMessage(0);
                }
                else {
                    menu->on_command(id);
                }
            }
            else if (g_appState == STATE_GAME && g_gameApp) {
                if (id == IDC_EXIT) {
                    // Скрываем игру
                    g_gameApp->hide();
                    
                    // Показываем меню
                    menu->show();
                    g_appState = STATE_MENU;
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                else {
                    g_gameApp->on_command(id);
                }
            }
            return 0;
        }
    case WM_TIMER:
        if (g_gameApp && g_appState == STATE_GAME) g_gameApp->on_timer(wParam);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        if (g_appState == STATE_MENU && menu) {
            menu->on_paint(hdc);
        } 
        else if (g_appState == STATE_GAME && g_gameApp) {
            g_gameApp->on_paint(hdc);
        }
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN: {
        if (g_appState == STATE_GAME && g_gameApp && g_gameApp->IsWaitingForClick()) {
            POINT pt = {LOWORD(lParam), HIWORD(lParam)};
            RECT textRect = g_gameApp->GetTextAreaRect();
            if (PtInRect(&textRect, pt)) {
                g_gameApp->OnTextAreaClick();
            }
        }
        return 0;
    }
    case WM_SETCURSOR: {
        if (g_appState == STATE_GAME && g_gameApp && g_gameApp->IsWaitingForClick()) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            RECT textRect = g_gameApp->GetTextAreaRect();
            if (PtInRect(&textRect, pt)) {
                SetCursor(LoadCursor(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, RGB(24, 26, 40));
        SetTextColor(hdc, RGB(235, 235, 245));
        return (LRESULT)darkBrush;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
        if (lpDIS->CtlType == ODT_BUTTON && 
            lpDIS->CtlID >= IDC_CHOICE1 && 
            lpDIS->CtlID <= IDC_CHOICE4) {
            
            HDC hdc = lpDIS->hDC;
            RECT rc = lpDIS->rcItem;
            
            HBRUSH bgBrush = CreateSolidBrush(RGB(110, 78, 100));
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);
            
            HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(230, 220, 235));
            HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            
            wchar_t text[256];
            GetWindowTextW(lpDIS->hwndItem, text, 256);
            
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(245, 238, 248));
            
            HFONT buttonFont = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Comic Sans MS");
            HFONT oldFont = (HFONT)SelectObject(hdc, buttonFont);
            
            RECT textRect = rc;
            textRect.left += 20;
            textRect.right -= 10;
            DrawTextW(hdc, text, -1, &textRect, DT_VCENTER | DT_SINGLELINE);
            
            if (lpDIS->itemState & ODS_SELECTED) {
                HBRUSH pressedBrush = CreateSolidBrush(RGB(80, 58, 70));
                FrameRect(hdc, &rc, pressedBrush);
                DeleteObject(pressedBrush);
            }
            
            SelectObject(hdc, oldFont);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(buttonFont);
            DeleteObject(borderPen);
            return TRUE;
        }
        break;
    }
    case WM_DESTROY:
        delete menu;
        if (g_gameApp) delete g_gameApp;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ==================== ТОЧКА ВХОДА ====================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    
    WNDCLASSW wc = {};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MainWindowClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST, L"MainWindowClass", L"Майами: уроки свободы",
        WS_POPUP | WS_VISIBLE,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr, hInstance, nullptr);
    
    if (!hwnd) {
        GdiplusShutdown(gdiplusToken);
        return 1;
    }
    
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    GdiplusShutdown(gdiplusToken);
    return 0;
}