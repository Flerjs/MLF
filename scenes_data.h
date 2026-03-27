// scenes_data.h
#pragma once
#include <string>
#include <vector>
#include <map>

struct SceneNode {
    std::wstring speaker;
    std::wstring text;
    bool has_sprite;
    std::wstring sprite_name;
    std::wstring background_name;
    bool has_choices;
    std::vector<std::pair<std::wstring, std::wstring>> choices;
    std::map<int, std::wstring> choice_targets;
    
    SceneNode() : has_sprite(false), has_choices(false) {}
};

class SceneDataBase {
public:
    std::map<std::wstring, SceneNode> scenes;
    std::map<std::wstring, std::wstring> scene_links;
    
    void Initialize() {
        int counter = 1;
        
        // ========== СЦЕНЫ 1-33 (ДО ПЕРВОГО ВЫБОРА) ==========
        AddScene(getName(counter++), L"", L"...", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Ну вот.", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Вот и наступил третий понедельник.", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Мне наконец-то больше не нужно ходить на занятия.", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Больше не нужно ничего учить. Уже третий понедельник.", false, L"", L"room");
        AddScene(getName(counter++), L"", L"...", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Опять плохо спалось.", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Кто бы мог подумать, что даже в свободных условиях качество моего сна не изменится.", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Эта привычка к плохому сну теперь со мной навсегда?", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Или мне лучше прекратить заниматься ночью чем угодно, кроме сна...", false, L"", L"room");
        AddScene(getName(counter++), L"", L"...", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Я лениво присел, отбрасывая от себя одеяло.", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Утро было достаточно тёплым. Здесь, в Майами, лето круглый год.", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Но какой от этого толк, если я всё равно каждый день сижу под кондиционером?", false, L"", L"room");
        AddScene(getName(counter++), L"", L"...", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Я встал с кровати и выпрямился, слегка размял спину. Глаза проделали недолгий путь до моей верхней одежды.", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Процесс занял не более двух минут, и теперь я был одет и направлялся в ванную.", false, L"", L"room");
        AddScene(getName(counter++), L"", L"Как только я вышел из комнаты, где-то из кухни послышался голос.", false, L"", L"corridor");
        AddScene(getName(counter++), L"Мама", L"Уже проснулся, Лу? Доброе утро!", false, L"", L"corridor");
        AddScene(getName(counter++), L"", L"Мама, как обычно, просыпается раньше меня. Ответ последовал автоматически.", false, L"", L"corridor");
        AddScene(getName(counter++), L"Луис", L"Утро, мам.", false, L"", L"corridor");
        AddScene(getName(counter++), L"", L"Мой путь оставался прежним, и я добрался до ванной.", false, L"", L"corridor");
        AddScene(getName(counter++), L"", L"В зеркале я увидел всё того же себя.", false, L"", L"bathroom");
        AddScene(getName(counter++), L"", L"Луис Мэрион.", true, L"Луис", L"bathroom");
        AddScene(getName(counter++), L"", L"Только мама и папа называют меня \"Лу\", мои сверстники никогда так ко мне не обращались.", true, L"Луис", L"bathroom");
        AddScene(getName(counter++), L"", L"Не то чтобы я жаловался... Я действительно не обладаю такой природной красотой, как мои одноклассники.", true, L"Луис", L"bathroom");
        AddScene(getName(counter++), L"", L"С чего бы это вдруг мной должны интересоваться, если я ничем не выделяюсь?", true, L"Луис", L"bathroom");
        AddScene(getName(counter++), L"", L"С другой стороны...", true, L"Луис", L"bathroom");
        AddScene(getName(counter++), L"", L"...", true, L"Луис", L"bathroom");
        AddScene(getName(counter++), L"Мама", L"Лу, будешь завтракать?", false, L"", L"bathroom");
        AddScene(getName(counter++), L"", L"Мне не дали завершить мысль, поэтому пришлось ответить.", true, L"Луис", L"bathroom");
        AddScene(getName(counter++), L"Луис", L"Да, мам, минуту.", true, L"Луис", L"bathroom");
        AddScene(getName(counter++), L"", L"Я быстро умылся и в последний раз посмотрел на своё лицо, прежде чем покинуть ванную.", true, L"Луис", L"bathroom");
        
        // ========== СЦЕНА С ВЫБОРОМ "КАК СПАЛОСЬ?" ==========
        SceneNode sleepChoice;
        sleepChoice.speaker = L"Мама";
        sleepChoice.text = L"Как спалось?";
        sleepChoice.has_sprite = false;
        sleepChoice.background_name = L"kitchen";
        sleepChoice.has_choices = true;
        sleepChoice.choices.push_back({L"Плохо", L""});
        sleepChoice.choices.push_back({L"Нормально", L""});
        sleepChoice.choice_targets[1] = L"sleep_bad_start";
        sleepChoice.choice_targets[2] = L"sleep_normal_start";
        scenes[L"sleep_choice"] = sleepChoice;
        
        // ========== ВЕТКА "ПЛОХО" ==========
        AddScene(L"sleep_bad_start", L"Луис", L"Плохо, если честно.", true, L"Луис", L"kitchen");
        AddScene(L"sleep_bad_2", L"Луис", L"Половину ночи смотрел видеоролики и во время просмотра отрубился.", true, L"Луис", L"kitchen");
        AddScene(L"sleep_bad_3", L"", L"Мама взглянула на меня так, словно расстроилась.", false, L"", L"kitchen");
        AddScene(L"sleep_bad_4", L"Мама", L"Ну ты даёшь... Ты же понимаешь, что скоро у тебя не получится так жить?", false, L"", L"kitchen");
        AddScene(L"sleep_bad_5", L"Мама", L"Тебе скоро выходить на работу, и как ты собираешься работать, если эти твои ролики для тебя важнее сна?", false, L"", L"kitchen");
        AddScene(L"sleep_bad_end", L"Луис", L"Мам, я понимаю.", true, L"Луис", L"kitchen");
        
        // ========== ВЕТКА "НОРМАЛЬНО" ==========
        AddScene(L"sleep_normal_start", L"Луис", L"Нормально, я думаю. Спал как спал.", true, L"Луис", L"kitchen");
        AddScene(L"sleep_normal_2", L"", L"Мама с лёгким подозрением взглянула на меня.", false, L"", L"kitchen");
        AddScene(L"sleep_normal_3", L"Мама", L"Врёшь же. Лу, ну зачем ты так? Снова не спал?", false, L"", L"kitchen");
        AddScene(L"sleep_normal_4", L"Мама", L"Тебе скоро выходить на работу, а ты даже режим сна привести в порядок не можешь? Понимаешь, что так не пойдёт?", false, L"", L"kitchen");
        AddScene(L"sleep_normal_end", L"Луис", L"Мам, я понимаю.", true, L"Луис", L"kitchen");
        
        // ========== ОБЩАЯ ЧАСТЬ ПОСЛЕ ВЫБОРА ==========
        AddScene(L"after_sleep_1", L"Мама", L"Понимает он... Ладно, завтракай. Я на работу.", false, L"", L"kitchen");
        AddScene(L"after_sleep_2", L"Луис", L"Хорошо.", true, L"Луис", L"kitchen");
        AddScene(L"after_sleep_3", L"", L"После этого мама спокойно обернулась и вышла из дома.", false, L"", L"kitchen");
        AddScene(L"after_sleep_4", L"", L"Она всегда такая.", false, L"", L"kitchen");
        AddScene(L"after_sleep_5", L"", L"Она переживает за меня, и я понимаю это.", false, L"", L"kitchen");
        AddScene(L"after_sleep_6", L"", L"Но иногда это немного выходило за рамки. И всё же я не могу судить её за это.", false, L"", L"kitchen");
        AddScene(L"after_sleep_7", L"", L"Я сел за стол и взглянул на свою тарелку, которую для меня приготовила мама.", false, L"", L"kitchen");
        AddScene(L"after_sleep_8", L"", L"Как обычно, яичница с беконом.", false, L"", L"kitchen");
        AddScene(L"after_sleep_9", L"", L"За всё это время я настолько успел привыкнуть к этому, что уже почти забыл, какое это блюдо на вкус.", false, L"", L"kitchen");
        AddScene(L"after_sleep_10", L"", L"Тем не менее, завтракать в тишине не хотелось, и я достал телефон.", false, L"", L"kitchen");
        AddScene(L"after_sleep_11", L"", L"Включив экран, я увидел видеоролик, который запустился автоматически после предыдущего.", false, L"", L"kitchen");
        AddScene(L"after_sleep_12", L"", L"Это был какой-то музыкальный клип. По всей видимости, рок-группы, которую я не знаю.", false, L"", L"kitchen");
        
        // ========== СЦЕНА С ВЫБОРОМ ВИДЕО ==========
        SceneNode videoChoice;
        videoChoice.speaker = L"";
        videoChoice.text = L"Включать ли?";
        videoChoice.has_sprite = false;
        videoChoice.background_name = L"kitchen";
        videoChoice.has_choices = true;
        videoChoice.choices.push_back({L"Включить клип", L""});
        videoChoice.choices.push_back({L"Следующее видео", L""});
        videoChoice.choice_targets[1] = L"watch_clip_start";
        videoChoice.choice_targets[2] = L"next_video_start";
        scenes[L"video_choice"] = videoChoice;
        
        // ========== ВЕТКА "ВКЛЮЧИТЬ КЛИП" ==========
        AddScene(L"watch_clip_start", L"", L"Я решил включить клип, раз уж он мне попался.", false, L"", L"kitchen");
        AddScene(L"watch_clip_2", L"", L"Ничего необычного. Просто рок-песня под визуальное сопровождение.", false, L"", L"kitchen");
        AddScene(L"watch_clip_3", L"", L"Однако...", false, L"", L"kitchen");
        AddScene(L"watch_clip_4", L"", L"Почему-то я не мог отвести взгляд от экрана телефона, даже когда ел.", false, L"", L"kitchen");
        AddScene(L"watch_clip_5", L"", L"Я даже не заметил, как к концу клипа вся тарелка уже была съедена.", false, L"", L"kitchen");
        AddScene(L"watch_clip_6", L"", L"Этот клип...", false, L"", L"kitchen");
        AddScene(L"watch_clip_7", L"", L"Чем эти ребята так меня зацепили?", false, L"", L"kitchen");
        AddScene(L"watch_clip_8", L"", L"Мне понравился смысл их песни, и выглядят они неплохо...", false, L"", L"kitchen");
        AddScene(L"watch_clip_end", L"", L"Я сохранил этот клип в избранное и закрыл телефон.", false, L"", L"kitchen");
        
        // ========== ВЕТКА "СЛЕДУЮЩЕЕ ВИДЕО" ==========
        AddScene(L"next_video_start", L"", L"Я решил пролистнуть этот клип, и мне попалось научное видео про пантер.", false, L"", L"kitchen");
        AddScene(L"next_video_2", L"", L"Так я и провёл свой завтрак.", false, L"", L"kitchen");
        AddScene(L"next_video_3", L"", L"Оказывается, пантеры отлично умеют лазать по деревьям.", false, L"", L"kitchen");
        AddScene(L"next_video_4", L"", L"Умеют приспособиться и к такой ситуации...", false, L"", L"kitchen");
        AddScene(L"next_video_end", L"", L"Уж лучше дружить с какой-нибудь пантерой, нежели быть её целью.", false, L"", L"kitchen");
        
        // ========== ФИНАЛЬНЫЕ СЦЕНЫ ==========
        AddScene(L"final_1", L"", L"...", false, L"", L"kitchen");
        AddScene(L"final_2", L"", L"Сегодня мне предстояло пройтись по магазинам, поэтому я быстро вымыл тарелки и поставил их на сушилку.", false, L"", L"kitchen");
        AddScene(L"final_3", L"", L"Нужно купить немного продуктов, которые уже закончились. Мама скинула мне список ещё вчера.", false, L"", L"kitchen");
        AddScene(L"final_4", L"", L"Пора снова выйти на улицу.", false, L"", L"kitchen");
        
        // ========== СВЯЗЫВАНИЕ СЦЕН ==========
        LinkScenes();
    }
    
    void LinkScenes() {
        // Связываем обычные сцены по порядку
        for (int i = 1; i <= 33; i++) {
            std::wstring current = L"scene_" + std::to_wstring(i);
            std::wstring next = L"scene_" + std::to_wstring(i + 1);
            if (scenes.find(next) != scenes.end()) {
                scene_links[current] = next;
            }
        }
        
        // Связываем последнюю сцену перед выбором с выбором
        scene_links[L"scene_33"] = L"sleep_choice";
        
        // Связываем ветку "Плохо"
        scene_links[L"sleep_bad_start"] = L"sleep_bad_2";
        scene_links[L"sleep_bad_2"] = L"sleep_bad_3";
        scene_links[L"sleep_bad_3"] = L"sleep_bad_4";
        scene_links[L"sleep_bad_4"] = L"sleep_bad_5";
        scene_links[L"sleep_bad_5"] = L"sleep_bad_end";
        scene_links[L"sleep_bad_end"] = L"after_sleep_1";
        
        // Связываем ветку "Нормально"
        scene_links[L"sleep_normal_start"] = L"sleep_normal_2";
        scene_links[L"sleep_normal_2"] = L"sleep_normal_3";
        scene_links[L"sleep_normal_3"] = L"sleep_normal_4";
        scene_links[L"sleep_normal_4"] = L"sleep_normal_end";
        scene_links[L"sleep_normal_end"] = L"after_sleep_1";
        
        // Связываем общую часть
        scene_links[L"after_sleep_1"] = L"after_sleep_2";
        scene_links[L"after_sleep_2"] = L"after_sleep_3";
        scene_links[L"after_sleep_3"] = L"after_sleep_4";
        scene_links[L"after_sleep_4"] = L"after_sleep_5";
        scene_links[L"after_sleep_5"] = L"after_sleep_6";
        scene_links[L"after_sleep_6"] = L"after_sleep_7";
        scene_links[L"after_sleep_7"] = L"after_sleep_8";
        scene_links[L"after_sleep_8"] = L"after_sleep_9";
        scene_links[L"after_sleep_9"] = L"after_sleep_10";
        scene_links[L"after_sleep_10"] = L"after_sleep_11";
        scene_links[L"after_sleep_11"] = L"after_sleep_12";
        scene_links[L"after_sleep_12"] = L"video_choice";
        
        // Связываем ветку "Включить клип"
        scene_links[L"watch_clip_start"] = L"watch_clip_2";
        scene_links[L"watch_clip_2"] = L"watch_clip_3";
        scene_links[L"watch_clip_3"] = L"watch_clip_4";
        scene_links[L"watch_clip_4"] = L"watch_clip_5";
        scene_links[L"watch_clip_5"] = L"watch_clip_6";
        scene_links[L"watch_clip_6"] = L"watch_clip_7";
        scene_links[L"watch_clip_7"] = L"watch_clip_8";
        scene_links[L"watch_clip_8"] = L"watch_clip_end";
        scene_links[L"watch_clip_end"] = L"final_1";
        
        // Связываем ветку "Следующее видео"
        scene_links[L"next_video_start"] = L"next_video_2";
        scene_links[L"next_video_2"] = L"next_video_3";
        scene_links[L"next_video_3"] = L"next_video_4";
        scene_links[L"next_video_4"] = L"next_video_end";
        scene_links[L"next_video_end"] = L"final_1";
        
        // Связываем финальные сцены
        scene_links[L"final_1"] = L"final_2";
        scene_links[L"final_2"] = L"final_3";
        scene_links[L"final_3"] = L"final_4";
    }
    
    std::wstring GetNextScene(const std::wstring& current_scene) {
        auto it = scene_links.find(current_scene);
        if (it != scene_links.end()) {
            return it->second;
        }
        return L"";
    }
    
    std::wstring getName(int number) {
        return L"scene_" + std::to_wstring(number);
    }
    
    void AddScene(const std::wstring& name, const std::wstring& speaker, 
                  const std::wstring& text, bool has_sprite, 
                  const std::wstring& sprite, const std::wstring& bg) {
        SceneNode scene;
        scene.speaker = speaker;
        scene.text = text;
        scene.has_sprite = has_sprite;
        scene.sprite_name = sprite;
        scene.background_name = bg;
        scene.has_choices = false;
        scenes[name] = scene;
    }
};