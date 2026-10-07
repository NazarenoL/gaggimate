#ifndef BEANCONTROLS_H
#define BEANCONTROLS_H

#include <ArduinoJson.h>
#include <atomic>
#include <lvgl.h>
#include <mutex>
#include <vector>

class Controller;
class PluginManager;

// All LVGL work stays on the display task; plugin callbacks only queue changes.
class BeanControls {
  public:
    void init(Controller *controller, PluginManager *plugins);
    void openSelector();
    void loop();

  private:
    struct Bean {
        String id, name, roaster;
        int grind = 10;
        bool hasGrind = false;
    };
    bool loadLibrary();
    void show(bool grindPrompt);
    void refresh();
    void close();
    void click(lv_obj_t *button);
    static void onClick(lv_event_t *event);
    lv_obj_t *label(const char *text, int y, const lv_font_t *font);
    lv_obj_t *button(const char *text, int x, int y, int width);

    Controller *controller = nullptr;
    std::vector<Bean> beans;
    String selectedId, promptId;
    int preview = 0, grind = 10, fallbackGrind = 10;
    bool grindPrompt = false;
    lv_obj_t *overlay = nullptr, *title = nullptr, *name = nullptr, *detail = nullptr;
    lv_obj_t *value = nullptr, *left = nullptr, *right = nullptr, *save = nullptr;
    lv_obj_t *back = nullptr, *message = nullptr;
    std::mutex pendingMutex;
    String pendingBean;
    std::atomic<bool> libraryChanged{false}, dismiss{false};
};

#endif
