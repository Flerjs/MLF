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

#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Winmm.lib")

using namespace Gdiplus;

// Forward declaration
class VisualNovelApp;

// Флаги для состояния приложения
enum AppState {
    STATE_MENU,
    STATE_GAME
};

// Глобальные переменные
AppState g_appState = STATE_MENU;
VisualNovelApp* g_gameApp = nullptr;
HWND g_mainHwnd = nullptr;
HWND g_gameWindow = nullptr;
bool g_isFullscreen = true;
HWND g_loadingHwnd = nullptr;

enum ControlIds {
    IDC_CHOICE1 = 1101,
    IDC_CHOICE2 = 1102,
    IDC_CHOICE3 = 1103,
    IDC_CHOICE4 = 1104,
    IDC_RESTART = 1201,
    IDC_EXIT = 1202
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

// ==================== ЗАГРУЗОЧНЫЙ ЭКРАН ====================
LRESULT CALLBACK LoadingWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        RECT rc;
        GetClientRect(hwnd, &rc);
        
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
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowLoadingScreen() {
    if (g_loadingHwnd) return;
    
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    
    WNDCLASSW loadingWc{};
    loadingWc.lpfnWndProc = LoadingWndProc;
    loadingWc.hInstance = hInst;
    loadingWc.lpszClassName = L"LoadingWindowClass";
    loadingWc.hCursor = LoadCursor(nullptr, IDC_WAIT);
    loadingWc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&loadingWc);
    
    g_loadingHwnd = CreateWindowExW(
        WS_EX_TOPMOST, L"LoadingWindowClass", L"Загрузка",
        WS_POPUP | WS_VISIBLE,
        0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInst, nullptr
    );
    
    ShowWindow(g_loadingHwnd, SW_SHOW);
    UpdateWindow(g_loadingHwnd);
    
    // Обновляем отображение
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    // Ждем 1 секунду
    Sleep(1000);
}

void HideLoadingScreen() {
    if (g_loadingHwnd) {
        DestroyWindow(g_loadingHwnd);
        g_loadingHwnd = nullptr;
    }
}

// ==================== КЛАСС ОСНОВНОЙ ИГРЫ ====================
class VisualNovelApp {
    public:
        explicit VisualNovelApp(HWND hwnd)
            : hwnd_(hwnd), text_index_(0), speed_ms_(8), has_key_(false), sound_index_(0), waiting_for_click_(false) {
            srand(static_cast<unsigned>(time(nullptr)));
            load_images();
            create_controls();
            start_game();
        }
    
        ~VisualNovelApp() {
            for (auto* img : images_) delete img;
            if (scaled_bg_cache_) delete scaled_bg_cache_;
            if (scaled_character_cache_) delete scaled_character_cache_;  // Добавить эту строку
            if (character_sprite_) delete character_sprite_;  // Добавить эту строку
            
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
    
        void reset_game() {
            previous_choice_.clear();
            has_key_ = false;
            text_index_ = 0;
            sound_counter_ = 0;
            current_choices_.clear();
            after_animation_ = nullptr;
            waiting_for_click_ = false;
            on_click_action_ = nullptr;

            // Сбрасываем спрайт
            keep_sprite_ = false;
            if (character_sprite_) {
                delete character_sprite_;
                character_sprite_ = nullptr;
            }
            if (scaled_character_cache_) {
                delete scaled_character_cache_;
                scaled_character_cache_ = nullptr;
            }

            clear_text();
            start_game();
        }
    
        bool IsWaitingForClick() const {
            return waiting_for_click_;
        }
        
        RECT GetTextAreaRect() const {
            return text_area_.rect;
        }
        
        void OnTextAreaClick() {
            if (waiting_for_click_ && on_click_action_) {
                waiting_for_click_ = false;
                auto action = on_click_action_;
                on_click_action_ = nullptr;
                action();
            }
        }
        
        void WaitForClick(std::function<void()> after) {
            waiting_for_click_ = true;
            on_click_action_ = after;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    
        void on_paint(HDC hdc) {
            // Если спрайт должен сохраняться, но его нет, перезагружаем
            if (keep_sprite_ && !character_sprite_ && !current_sprite_path_.empty()) {
                set_character_sprite(current_sprite_path_, true);
            }

            RECT rc;
            GetClientRect(hwnd_, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
        
            HDC memdc = CreateCompatibleDC(hdc);
            HBITMAP back = CreateCompatibleBitmap(hdc, w, h);
            HGDIOBJ old = SelectObject(memdc, back);
        
            Graphics g(memdc);
            g.SetInterpolationMode(InterpolationModeBilinear);
        
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

            
        
            // ========== ОТРИСОВКА СПРАЙТА ПЕРСОНАЖА ==========
            if (character_sprite_) {
                int sprite_width = 300;
                int sprite_height = 400;
                int sprite_x = w - sprite_width - 50;
                int sprite_y = h - sprite_height - 100;
                
                if (!scaled_character_cache_ || 
                    scaled_character_cache_->GetWidth() != sprite_width || 
                    scaled_character_cache_->GetHeight() != sprite_height) {
                    if (scaled_character_cache_) delete scaled_character_cache_;
                    scaled_character_cache_ = new Bitmap(sprite_width, sprite_height, PixelFormat32bppARGB);
                    Graphics char_g(scaled_character_cache_);
                    char_g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
                    char_g.DrawImage(character_sprite_, 0, 0, sprite_width, sprite_height);
                }
                g.DrawImage(scaled_character_cache_, sprite_x, sprite_y, sprite_width, sprite_height);
            }
    
            const int panel_x = 24;
            const int panel_y = rc.bottom - 285;
            const int panel_w = w - 48;
            const int panel_h = 170;
            SolidBrush panel_brush(Color(165, 22, 24, 38));
            g.FillRectangle(&panel_brush, panel_x, panel_y, panel_w, panel_h);
    
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
    
            StringFormat center_fmt;
            center_fmt.SetAlignment(StringAlignmentCenter);
            center_fmt.SetLineAlignment(StringAlignmentCenter);
            RectF name_rect((REAL)name_x, (REAL)name_y, (REAL)name_w, (REAL)name_h);
            g.DrawString(current_speaker_.c_str(), -1, &name_font, name_rect, &center_fmt, &text_brush);
    
            StringFormat body_fmt;
            body_fmt.SetAlignment(StringAlignmentNear);
            body_fmt.SetLineAlignment(StringAlignmentNear);
            body_fmt.SetFormatFlags(StringFormatFlagsLineLimit);
            RectF body_rect((REAL)panel_x + 18, (REAL)panel_y + 18, (REAL)panel_w - 36, (REAL)panel_h - 30);
            g.DrawString(shown_text_.c_str(), -1, &text_font, body_rect, &body_fmt, &text_brush);
    
            // Сохраняем область текста для кликов
            text_area_.rect.left = panel_x;
            text_area_.rect.right = panel_x + panel_w;
            text_area_.rect.top = panel_y;
            text_area_.rect.bottom = panel_y + panel_h;
    
            BitBlt(hdc, 0, 0, w, h, memdc, 0, 0, SRCCOPY);
            SelectObject(memdc, old);
            DeleteObject(back);
            DeleteDC(memdc);
    
            // Отрисовка индикатора ожидания клика
            if (waiting_for_click_) {
                static int blink_counter = 0;
                static DWORD last_blink_time = 0;
                DWORD current_time = GetTickCount();
                
                if (current_time - last_blink_time > 300) {
                    blink_counter = (blink_counter + 1) % 2;
                    last_blink_time = current_time;
                }
                
                if (blink_counter == 0) {
                    HBRUSH indicatorBg = CreateSolidBrush(RGB(22, 24, 38));
                    HPEN indicatorPen = CreatePen(PS_SOLID, 1, RGB(255, 215, 0));
                    
                    RECT indicatorRect;
                    indicatorRect.left = panel_x + panel_w - 50;
                    indicatorRect.top = panel_y + panel_h - 35;
                    indicatorRect.right = panel_x + panel_w - 15;
                    indicatorRect.bottom = panel_y + panel_h - 10;
                    
                    FillRect(hdc, &indicatorRect, indicatorBg);
                    
                    HPEN oldPen = (HPEN)SelectObject(hdc, indicatorPen);
                    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    Rectangle(hdc, indicatorRect.left, indicatorRect.top, indicatorRect.right, indicatorRect.bottom);
                    
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, RGB(255, 215, 0));
                    HFONT indicatorFont = CreateFontW(
                        -18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Wingdings"
                    );
                    HFONT oldFont = (HFONT)SelectObject(hdc, indicatorFont);
                    
                    RECT textRect = indicatorRect;
                    DrawTextW(hdc, L"➤", -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    
                    SelectObject(hdc, oldFont);
                    SelectObject(hdc, oldPen);
                    SelectObject(hdc, oldBrush);
                    
                    DeleteObject(indicatorFont);
                    DeleteObject(indicatorPen);
                    DeleteObject(indicatorBg);
                }
            }
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

        void keep_current_sprite() {
            keep_sprite_ = true;
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
                ShowLoadingScreen();
                g_appState = STATE_MENU;
                ShowWindow(hwnd_, SW_HIDE);
                if (g_mainHwnd) {
                    ShowWindow(g_mainHwnd, SW_SHOW);
                }
                HideLoadingScreen();
                return;
            }
    
            if (id >= IDC_CHOICE1 && id <= IDC_CHOICE4) {
                int idx = id - IDC_CHOICE1;
                if (idx >= 0 && idx < static_cast<int>(current_choices_.size())) {
                    disable_choices();
                    auto cb = current_choices_[idx].action;
                    if (cb) cb();
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
    
        std::vector<Image*> images_;
        Image* bg_room_{};
        Image* bg_corridor_{};
        Image* bg_library_{};
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
        
        // Для клика по тексту
        ClickableArea text_area_;
        bool waiting_for_click_ = false;
        std::function<void()> on_click_action_;
        static constexpr int BLINK_INTERVAL_MS = 300;
        
        // Для спрайта персонажа
        Image* character_sprite_{nullptr};
        Bitmap* scaled_character_cache_{nullptr};
        int char_w_{0};
        int char_h_{0};
        
        // Для сохранения спрайта между сценами
        bool keep_sprite_{false};
        std::wstring current_sprite_path_;

        static std::wstring s2ws(const std::string& s) {
            if (s.empty()) return L"";
            int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
            std::wstring out(n - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], n);
            return out;

        
        }
    
        void load_images() {
            bg_room_ = load_one(L"backgrounds\\room.jpg");
            bg_corridor_ = load_one(L"backgrounds\\corridor.jpg");
            bg_library_ = load_one(L"backgrounds\\library.jpg");
            bg_bathroom_ = load_one(L"backgrounds\\bathroom.jpg");
            bg_basement_ = load_one(L"backgrounds\\basement.jpg");
            bg_ending_ = load_one(L"backgrounds\\ending_good.jpg");
            if (!bg_ending_) {
                bg_ending_ = load_one(L"backgrounds\\edning_good.jpg");
            }
            
            // Загружаем спрайт персонажа
            character_sprite_ = load_one(L"characters\\louis.png");  // Укажите путь к вашему спрайту
            if (!character_sprite_) {
                // Если спрайт не загрузился, используем заглушку или nullptr
                character_sprite_ = nullptr;
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
    
        void create_controls() {
            ui_font_ = CreateFontW(
                -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Comic Sans MS"
            );
    
            for (int i = 0; i < 4; ++i) {
                int id = IDC_CHOICE1 + i;
                choice_btns_[i] = CreateWindowW(
                    L"BUTTON", L"",
                    WS_CHILD | BS_OWNERDRAW,
                    60, 670 + i * 40, 900, 45, hwnd_, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr
                );
                SendMessageW(choice_btns_[i], WM_SETFONT, (WPARAM)ui_font_, TRUE);
                EnableWindow(choice_btns_[i], FALSE);
                ShowWindow(choice_btns_[i], SW_HIDE);
            }
    
            exit_btn_ = CreateWindowW(
                L"BUTTON", L"Выйти в меню",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                920, 10, 95, 30, hwnd_, (HMENU)IDC_EXIT, GetModuleHandleW(nullptr), nullptr
            );
            SendMessageW(exit_btn_, WM_SETFONT, (WPARAM)ui_font_, TRUE);
    
            on_resize();
        }
    
        void set_background(Image* bg) {
            current_bg_ = bg;
            scaled_bg_source_ = nullptr;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    
        void ensure_scaled_background(int w, int h) {
            if (!current_bg_ || w <= 0 || h <= 0) return;
            if (scaled_bg_cache_ && scaled_bg_source_ == current_bg_ && scaled_bg_w_ == w && scaled_bg_h_ == h) {
                return;
            }
            if (scaled_bg_cache_) {
                delete scaled_bg_cache_;
                scaled_bg_cache_ = nullptr;
            }
            scaled_bg_cache_ = new Bitmap(w, h, PixelFormat32bppARGB);
            Graphics bg_g(scaled_bg_cache_);
            bg_g.SetInterpolationMode(InterpolationModeBilinear);
            bg_g.DrawImage(current_bg_, 0, 0, w, h);
            scaled_bg_source_ = current_bg_;
            scaled_bg_w_ = w;
            scaled_bg_h_ = h;
        }
    
        int semitones_to_tempo(int semitones) {
            double ratio = pow(2.0, semitones / 12.0);
            return static_cast<int>(ratio * 100.0);
        }
    
        void play_type_sound(wchar_t ch) {
            if (ch == L' ' || ch == L'\r' || ch == L'\n' || ch == L'\t') return;
            sound_counter_++;
            if ((sound_counter_ % 2) != 0) return;
    
            int semitone_shift = (rand() % 13) - 6;
            
            if (active_sounds_.size() >= MAX_CONCURRENT_SOUNDS) {
                std::wstring close_cmd = L"close " + active_sounds_[0].alias;
                mciSendStringW(close_cmd.c_str(), nullptr, 0, nullptr);
                active_sounds_.erase(active_sounds_.begin());
            }
    
            std::wstring alias = L"textsnd_" + std::to_wstring(sound_index_++);
            
            std::wstring open_cmd = L"open \"" + std::wstring(kTextSoundFile) + L"\" type mpegvideo alias " + alias;
            if (mciSendStringW(open_cmd.c_str(), nullptr, 0, nullptr) == 0) {
                int tempo = semitones_to_tempo(semitone_shift);
                std::wstring set_cmd = L"set " + alias + L" tempo " + std::to_wstring(tempo);
                mciSendStringW(set_cmd.c_str(), nullptr, 0, nullptr);
                
                std::wstring play_cmd = L"play " + alias;
                mciSendStringW(play_cmd.c_str(), nullptr, 0, nullptr);
                
                active_sounds_.push_back({alias, semitone_shift});
            } else {
                PlaySoundW(L"SystemAsterisk", nullptr, SND_ALIAS | SND_ASYNC | SND_NOSTOP | SND_NODEFAULT);
            }
        }
    
        RECT dialogue_dirty_rect() const {
            RECT rc;
            GetClientRect(hwnd_, &rc);
            RECT dirty{};
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
    
        void set_character_sprite(const std::wstring& sprite_filename, bool keep = false) {
            keep_sprite_ = keep;
            current_sprite_path_ = sprite_filename;
            
            std::wstring sprite_path = L"characters\\" + sprite_filename;
            
            if (character_sprite_) {
                delete character_sprite_;
                character_sprite_ = nullptr;
            }
            if (scaled_character_cache_) {
                delete scaled_character_cache_;
                scaled_character_cache_ = nullptr;
            }
            
            character_sprite_ = load_one(sprite_path.c_str());
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        void hide_character_sprite() {
            if (character_sprite_) {
                delete character_sprite_;
                character_sprite_ = nullptr;
            }
            if (scaled_character_cache_) {
                delete scaled_character_cache_;
                scaled_character_cache_ = nullptr;
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        void animate_text(const std::wstring& text, 
            std::function<void()> after = nullptr, 
            const std::wstring& speaker = L"Нарратор", 
            bool has_choices = false) {
    KillTimer(hwnd_, 1);
    disable_choices();

    // Если после анимации будут выборы, не ждем клик
    if (!has_choices) {
    // Сохраняем действие после клика
    auto final_action = after;
    after = [this, final_action]() {
        WaitForClick(final_action);
    };
    }

    current_speaker_ = speaker;
    current_text_ = text + L"\r\n";
    shown_text_.clear();
    sound_counter_ = 0;
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
                
                // Сохраняем спрайт перед выполнением следующего действия
                if (keep_sprite_ && character_sprite_) {
                    // Спрайт сохранится
                }
                
                cb();
            }
        }
    
        void set_choices(const std::vector<Choice>& choices) {
            current_choices_ = choices;
            
            RECT rc;
            GetClientRect(hwnd_, &rc);
            
            // Параметры текстовой области
            int panel_x = 24;
            int panel_y = rc.bottom - 400;
            int panel_w = rc.right - 48;
            
            // Параметры кнопок - ширина 80% от ширины текстовой области
            int btn_h = 45;
            int btn_w = static_cast<int>(panel_w * 0.4);  // 80% ширины текстовой области
            int btn_spacing = 8;
            
            // Центрируем кнопки относительно текстовой области
            int btn_x = panel_x + (panel_w - btn_w) / 2;
            int choice_y = panel_y - 10;
            
            for (int i = 0; i < 4; ++i) {
                if (i < static_cast<int>(choices.size())) {
                    std::wstring label = std::to_wstring(i + 1) + L". " + choices[i].text;
                    SetWindowTextW(choice_btns_[i], label.c_str());
                    
                    int btn_y = choice_y - (choices.size() - i) * (btn_h + btn_spacing);
                    
                    MoveWindow(choice_btns_[i], btn_x, btn_y, btn_w, btn_h, TRUE);
                    
                    ShowWindow(choice_btns_[i], SW_SHOW);
                    EnableWindow(choice_btns_[i], TRUE);
                    InvalidateRect(choice_btns_[i], nullptr, TRUE);
                } else {
                    ShowWindow(choice_btns_[i], SW_HIDE);
                    EnableWindow(choice_btns_[i], FALSE);
                }
            }
        }
    
        void disable_choices() {
            for (auto* btn : choice_btns_) {
                EnableWindow(btn, FALSE);
                ShowWindow(btn, SW_HIDE);
            }
            current_choices_.clear();
            
            // Сбрасываем ожидание клика
            waiting_for_click_ = false;
            on_click_action_ = nullptr;
            
            // НЕ удаляем и не скрываем спрайт здесь!
        }
    
        void start_game() {
            previous_choice_.clear();
            has_key_ = false;
            clear_text();
            one();
        }
    

        //Гл. 1, эп. 1
        void one() {
            set_background(bg_room_);
            animate_text(
                L"...\r\n",
                [this]() { twosix(); },
                L"",
                false
            );
        }
        void two() {
            set_background(bg_room_);
    
            animate_text(
               L"Ну вот.",
                [this]() { three(); },
               L"",
               false
            ); 
        }
        void three() {
            set_background(bg_room_);
    
            animate_text(
               L"Вот и наступил третий понедельник.",
                [this]() { four(); },
               L"",
               false
            ); 
        }
        void four() {
            set_background(bg_room_);
    
            animate_text(
               L"Мне наконец-то больше не нужно ходить на занятия.",
                [this]() { five(); },
               L"",
               false
            ); 
        }
        void five() {
            set_background(bg_room_);
    
            animate_text(
               L"Больше не нужно ничего учить. Уже третий понедельник.",
                [this]() { seven(); },
               L"",
               false
            ); 
        }
        void seven() {
            set_background(bg_room_);
            animate_text(
                L"...",
                [this]() { 
                    eight();
                },
                L"",
                false
            );
        }
        void eight() {
            set_background(bg_room_);
            animate_text(
                L"Опять плохо спалось.",
                [this]() { 
                    nine();
                },
                L"",
                false
            );
        }
        void nine() {
            set_background(bg_room_);
            animate_text(
                L"Кто бы мог подумать, что даже в свободных условиях качество моего сна не изменится.",
                [this]() { 
                    onezero();
                },
                L"",
                false
            );
        }
        void onezero() {
            set_background(bg_room_);
            animate_text(
                L"Эта привычка к плохому сну теперь со мной навсегда?",
                [this]() { 
                    oneone();
                },
                L"",
                false
            );
        }
        void oneone() {
            set_background(bg_room_);
            animate_text(
                L"Или мне лучше прекратить заниматься ночью чем угодно, кроме сна...",
                [this]() { 
                    onetwo();
                },
                L"",
                false
            );
        }
        void onetwo() {
            set_background(bg_room_);
            animate_text(
                L"...",
                [this]() { 
                    onefour();
                },
                L"",
                false
            );
        }
        void onefour() {
            set_background(bg_room_);
            animate_text(
                L"Я лениво присел, отбрасывая от себя одеяло.",
                [this]() { 
                    onefive();
                },
                L"",
                false
            );
        }
        void onefive() {
            set_background(bg_room_);
            animate_text(
                L"Утро было достаточно тёплым. Здесь, в Майами, лето круглый год.",
                [this]() { 
                    onesix();
                },
                L"",
                false
            );
        }
        void onesix() {
            set_background(bg_room_);
            animate_text(
                L"Но какой от этого толк, если я всё равно каждый день сижу под кондиционером?",
                [this]() { 
                    oneseven();
                },
                L"",
                false
            );
        }
        void oneseven() {
            set_background(bg_room_);
            animate_text(
                L"...",
                [this]() { 
                    oneeight();
                },
                L"",
                false
            );
        }
        void oneeight() {
            set_background(bg_room_);
            animate_text(
                L"Я встал с кровати и выпрямился, слегка размял спину. Глаза проделали недолгий путь до моей верхней одежды.",
                [this]() { 
                    onenine();
                },
                L"",
                false
            );
        }
        void onenine() {
            set_background(bg_room_);
            animate_text(
                L"Процесс занял не более двух минут, и теперь я был одет и направлялся в ванную.",
                [this]() { 
                    twozero();
                },
                L"",
                false
            );
        }
        void twozero() {
            set_background(bg_corridor_);
            animate_text(
                L"Как только я вышел из комнаты, где-то из кухни послышался голос.",
                [this]() { 
                    twoone();
                },
                L"",
                false
            );
        };
        void twoone() {
            set_background(bg_corridor_);
            animate_text(
                L"Уже проснулся, Лу? Доброе утро!",
                [this]() { 
                    twotwo();
                },
                L"Мама",
                false
            );
        };
        void twotwo() {
            set_background(bg_corridor_);
            animate_text(
                L"Мама, как обычно, просыпается раньше меня. Ответ последовал автоматически.",
                [this]() { 
                    twothree();
                },
                L"",
                false
            );
        };
        void twothree() {
            set_background(bg_corridor_);
            animate_text(
                L"Утро, мам.",
                [this]() { 
                    twofour();
                },
                L"Луис",
                false
            );
        };
        void twofour() {
            set_background(bg_corridor_);
            animate_text(
                L"Мой путь оставался прежним, и я добрался до ванной.",
                [this]() { 
                    twofive();
                },
                L"",
                false
            );
        };
        void twofive() {
            set_background(bg_bathroom_);
            animate_text(
                L"В зеркале я увидел всё того же себя.",
                [this]() { 
                    twosix();
                },
                L"",
                false
            );
        };
        void twosix() {
            set_background(bg_bathroom_);
            set_character_sprite_by_name(L"Луис", true);
            animate_text(
                L"Луис Мэрион.",
                [this]() { 
                    
                },
                L"",
                false
            );
        };






        void show_ending(const std::wstring& ending_type) {
            set_background(bg_ending_);
            std::wstring t = L"\r\n==================================================\r\n";
        
            if (ending_type == L"good_ending") {
                t += L"         ХОРОШАЯ КОНЦОВКА\r\n";
                t += L"==================================================\r\n\r\n";
                t += L"Вы разгадали тайну усадьбы и обрели свободу!\r\n";
                t += L"Духи предков благодарны вам за то, что вы вернули им покой.\r\n";
                t += L"Вы выходите из дома на рассвете, и он исчезает за вашей спиной.\r\n";
                t += L"Эта история навсегда останется в вашей памяти...\r\n";
            } else if (ending_type == L"neutral_ending") {
                t += L"         НЕЙТРАЛЬНАЯ КОНЦОВКА\r\n";
                t += L"==================================================\r\n\r\n";
                t += L"Вы выбрались из усадьбы, но не разгадали ее тайну.\r\n";
                t += L"Иногда вам снятся странные сны о том месте.\r\n";
                t += L"Кто знает, вернетесь ли вы туда когда-нибудь...\r\n";
            } else {
                t += L"         ПЛОХАЯ КОНЦОВКА\r\n";
                t += L"==================================================\r\n\r\n";
                t += L"Вы стали частью усадьбы навсегда.\r\n";
                t += L"Теперь вы - ее хранитель, ждущий следующего путника.\r\n";
                t += L"Ваша история закончилась здесь, но для кого-то она только начинается...\r\n";
            }
            t += L"\r\n==================================================\r\n";
            t += L"\r\nХотите сыграть еще раз?";
        }
    };

// ==================== ГЛАВНОЕ МЕНЮ ====================
class MainMenu {
    public:
        MainMenu(HWND hwnd) : hwnd_(hwnd), g_gameWindow(nullptr) {
            create_controls();
        }
    
        ~MainMenu() {
            if (title_font_) DeleteObject(title_font_);
            if (button_font_) DeleteObject(button_font_);
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
            
            RECT titleRect;
            titleRect.left = 0;
            titleRect.top = rc.bottom / 4 - 60;
            titleRect.right = rc.right;
            titleRect.bottom = rc.bottom / 4;
            
            SetTextColor(memdc, RGB(255, 215, 0));
            DrawTextW(memdc, L"Майами: уроки свободы", -1, &titleRect, DT_CENTER | DT_SINGLELINE);
            
            SelectObject(memdc, button_font_);
            SetTextColor(memdc, RGB(200, 200, 220));
            
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memdc, 0, 0, SRCCOPY);
            SelectObject(memdc, oldFont);
            DeleteObject(back);
            DeleteDC(memdc);
            
        }
    
        void on_command(WORD id) {
            if (id == IDC_START_GAME) {
                ShowLoadingScreen();
                
                g_appState = STATE_GAME;
                ShowWindow(hwnd_, SW_HIDE);
                
                if (g_gameApp && g_gameWindow) {
                    g_gameApp->reset_game();
                    HideLoadingScreen();
                    ShowWindow(g_gameWindow, SW_SHOW);
                    SetForegroundWindow(g_gameWindow);
                } else {
                    HINSTANCE hInst = GetModuleHandleW(nullptr);
                    
                    g_gameWindow = CreateWindowExW(
                        WS_EX_TOPMOST, L"GameWindowClass", L"Майами: уроки свободы",
                        WS_POPUP | WS_VISIBLE,
                        0, 0,
                        GetSystemMetrics(SM_CXSCREEN),
                        GetSystemMetrics(SM_CYSCREEN),
                        nullptr, nullptr, hInst, nullptr
                    );
                    
                    if (g_gameWindow) {
                        g_gameApp = new VisualNovelApp(g_gameWindow);
                        SetWindowLongPtrW(g_gameWindow, GWLP_USERDATA, (LONG_PTR)g_gameApp);
                        HideLoadingScreen();
                        ShowWindow(g_gameWindow, SW_SHOW);
                        UpdateWindow(g_gameWindow);
                    } else {
                        HideLoadingScreen();
                    }
                }
            }
            else if (id == IDC_SETTINGS) {
                MessageBoxW(hwnd_, L"Настройки будут доступны в следующей версии!", 
                            L"Информация", MB_OK | MB_ICONINFORMATION);
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
        }
    
    private:
        HWND hwnd_;
        HWND start_btn_;
        HWND settings_btn_;
        HWND exit_btn_;
        HWND g_gameWindow;
        HFONT title_font_;
        HFONT button_font_;
    
        static constexpr int IDC_START_GAME = 2001;
        static constexpr int IDC_SETTINGS = 2002;
        static constexpr int IDC_EXIT_GAME = 2003;
    
        void create_controls() {
            title_font_ = CreateFontW(
                -48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Comic Sans MS"
            );
            
            button_font_ = CreateFontW(
                -24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Comic Sans MS"
            );
            
            start_btn_ = CreateWindowW(
                L"BUTTON", L"Начать игру",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 300, 50, hwnd_, (HMENU)IDC_START_GAME, GetModuleHandleW(nullptr), nullptr
            );
            SendMessageW(start_btn_, WM_SETFONT, (WPARAM)button_font_, TRUE);
            
            settings_btn_ = CreateWindowW(
                L"BUTTON", L"Настройки",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 300, 50, hwnd_, (HMENU)IDC_SETTINGS, GetModuleHandleW(nullptr), nullptr
            );
            SendMessageW(settings_btn_, WM_SETFONT, (WPARAM)button_font_, TRUE);
            
            exit_btn_ = CreateWindowW(
                L"BUTTON", L"Выход",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 300, 50, hwnd_, (HMENU)IDC_EXIT_GAME, GetModuleHandleW(nullptr), nullptr
            );
            SendMessageW(exit_btn_, WM_SETFONT, (WPARAM)button_font_, TRUE);
            
            on_resize();
        }
    };

// ==================== ОБРАБОТЧИКИ ОКОН ====================
LRESULT CALLBACK MenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static MainMenu* menu = nullptr;
    
    switch (msg) {
    case WM_CREATE:
        menu = new MainMenu(hwnd);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)menu);
        return 0;
    case WM_SIZE:
        if (menu) menu->on_resize();
        return 0;
    case WM_COMMAND:
        if (menu) menu->on_command(LOWORD(wParam));
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (menu) menu->on_paint(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        if (menu) delete menu;
        ChangeDisplaySettings(nullptr, 0);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<VisualNovelApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    static HBRUSH darkBrush = CreateSolidBrush(RGB(24, 26, 40));

    switch (msg) {
    case WM_CREATE:
        return 0;
        
    case WM_SIZE:
        if (app) app->on_resize();
        return 0;
        
    case WM_COMMAND:
        if (app) app->on_command(LOWORD(wParam));
        return 0;
        
    case WM_TIMER:
        if (app) app->on_timer(wParam);
        return 0;
        
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (app) app->on_paint(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    
    case WM_ERASEBKGND:
        return 1;
        
    case WM_LBUTTONDOWN: {
        if (app && app->IsWaitingForClick()) {
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);
            
            // Проверяем, попали ли в область текста
            RECT textRect = app->GetTextAreaRect();
            if (PtInRect(&textRect, pt)) {
                app->OnTextAreaClick();
            }
        }
        return 0;
    }
    
    case WM_SETCURSOR: {
        if (app && app->IsWaitingForClick()) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            RECT textRect = app->GetTextAreaRect();
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
        
        // Обрабатываем только наши кнопки выбора
        if (lpDIS->CtlType == ODT_BUTTON && 
            lpDIS->CtlID >= IDC_CHOICE1 && 
            lpDIS->CtlID <= IDC_CHOICE4) {
            
            HDC hdc = lpDIS->hDC;
            RECT rc = lpDIS->rcItem;
            
            // Создаем кисть для фона (как у области с именем)
            HBRUSH bgBrush = CreateSolidBrush(RGB(110, 78, 100));
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);
            
            // Создаем перо для рамки (золотистый цвет)
            HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(230, 220, 235));
            HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            
            // Рисуем прямоугольную рамку
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            
            // Получаем текст кнопки
            wchar_t text[256];
            GetWindowTextW(lpDIS->hwndItem, text, 256);
            
            // Настройки для текста
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(245, 238, 248));
            
            // Создаем шрифт для кнопок
            HFONT buttonFont = CreateFontW(
                -18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Comic Sans MS"
            );
            HFONT oldFont = (HFONT)SelectObject(hdc, buttonFont);
            
            // Отступ для текста
            RECT textRect = rc;
            textRect.left += 20;
            textRect.right -= 10;
            
            // Рисуем текст
            DrawTextW(hdc, text, -1, &textRect, DT_VCENTER | DT_SINGLELINE);
            
            // Эффект нажатия (если кнопка нажата)
            if (lpDIS->itemState & ODS_SELECTED) {
                HBRUSH pressedBrush = CreateSolidBrush(RGB(80, 58, 70));
                FrameRect(hdc, &rc, pressedBrush);
                DeleteObject(pressedBrush);
            }
            
            // Эффект фокуса (если кнопка в фокусе)
            if (lpDIS->itemState & ODS_FOCUS) {
                RECT focusRect = rc;
                focusRect.left += 2;
                focusRect.top += 2;
                focusRect.right -= 2;
                focusRect.bottom -= 2;
                DrawFocusRect(hdc, &focusRect);
            }
            
            // Восстанавливаем старые объекты GDI
            SelectObject(hdc, oldFont);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            
            // Удаляем созданные объекты
            DeleteObject(buttonFont);
            DeleteObject(borderPen);
            
            return TRUE;
        }
        break;
    }
    
    case WM_DESTROY:
        if (app) {
            delete app;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            g_gameApp = nullptr;
            g_gameWindow = nullptr;
        }
        if (g_mainHwnd) {
            ShowLoadingScreen();
            g_appState = STATE_MENU;
            ShowWindow(g_mainHwnd, SW_SHOW);
            HideLoadingScreen();
        }
        return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ==================== ТОЧКА ВХОДА ====================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // Регистрируем класс загрузочного окна
    WNDCLASSW loadingWc{};
    loadingWc.lpfnWndProc = LoadingWndProc;
    loadingWc.hInstance = hInstance;
    loadingWc.lpszClassName = L"LoadingWindowClass";
    loadingWc.hCursor = LoadCursor(nullptr, IDC_WAIT);
    loadingWc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&loadingWc);
    
    // Регистрируем класс окна меню
    WNDCLASSW menuWc{};
    menuWc.lpfnWndProc = MenuWndProc;
    menuWc.hInstance = hInstance;
    menuWc.lpszClassName = L"MenuWindowClass";
    menuWc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    menuWc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&menuWc);
    
    // Регистрируем класс окна игры
    WNDCLASSW gameWc{};
    gameWc.lpfnWndProc = GameWndProc;
    gameWc.hInstance = hInstance;
    gameWc.lpszClassName = L"GameWindowClass";
    gameWc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    gameWc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&gameWc);

    // Полноэкранный режим
    DEVMODE dmScreenSettings;
    memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
    dmScreenSettings.dmSize = sizeof(dmScreenSettings);
    dmScreenSettings.dmPelsWidth = GetSystemMetrics(SM_CXSCREEN);
    dmScreenSettings.dmPelsHeight = GetSystemMetrics(SM_CYSCREEN);
    dmScreenSettings.dmBitsPerPel = 32;
    dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST, L"MenuWindowClass", L"Майами: уроки свободы",
        WS_POPUP | WS_VISIBLE,
        0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) {
        GdiplusShutdown(gdiplusToken);
        return 1;
    }
    
    g_mainHwnd = hwnd;
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