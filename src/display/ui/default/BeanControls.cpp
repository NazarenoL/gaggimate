#include "BeanControls.h"
#include <algorithm>
#include <cmath>
#include <display/ui/default/DefaultUI.h>
#include <display/core/Controller.h>
#include <display/core/PluginManager.h>
#include <display/util/PsramAllocator.h>
#include <display/plugins/ShotHistoryPlugin.h>
#include <display/ui/default/eez/screens.h>
#include <display/ui/default/eez/ui.h>

void BeanControls::init(Controller *owner, PluginManager *plugins) {
    controller = owner;
    plugins->on("beans:changed", [this](Event const &) { libraryChanged = true; });
    plugins->on("beans:brew:finished", [this](Event const &event) {
        std::lock_guard<std::mutex> guard(pendingMutex);
        pendingBean = event.getString("id");
    });
    for (const char *event : {"controller:brew:start", "controller:mode:change", "ota:update:start", "controller:error"}) {
        plugins->on(event, [this](Event const &) { dismiss = true; });
    }
}

bool BeanControls::loadLibrary() {
    JsonDocument request(&psramAllocator), response(&psramAllocator);
    request["tp"] = "req:beans:list";
    ShotHistory.handleBeansRequest(request, response);
    if (!response["error"].isNull()) return false;
    beans.clear();
    selectedId = response["selectedId"].as<String>();
    fallbackGrind = response["lastGrindSetting"].isNull() ? 10 : lround(response["lastGrindSetting"].as<double>() * 10);
    // The full library is needed for the just-brewed bean even if it is no longer in the recent ten.
    JsonArray library = response["beans"].as<JsonArray>();
    auto add = [&](JsonObject bean) {
        Bean entry;
        entry.id = bean["id"].as<String>();
        entry.name = bean["name"].as<String>();
        entry.roaster = bean["roaster"].as<String>();
        entry.hasGrind = !bean["grindSetting"].isNull();
        entry.grind = entry.hasGrind ? lround(bean["grindSetting"].as<double>() * 10) : fallbackGrind;
        beans.push_back(entry);
    };
    if (grindPrompt) {
        for (JsonObject bean : library) if (bean["id"].as<String>() == promptId) add(bean);
    } else {
        for (JsonVariant id : response["recentIds"].as<JsonArray>()) {
            for (JsonObject bean : library) if (bean["id"].as<String>() == id.as<String>()) add(bean);
        }
    }
    return true;
}

void BeanControls::openSelector() {
    close();
    grindPrompt = false;
    preview = 0;
    bool loaded = loadLibrary();
    for (size_t i = 0; i < beans.size(); ++i) if (beans[i].id == selectedId) preview = i;
    show(false);
    if (!loaded) lv_label_set_text(message, "Could not read beans. Try again.");
}

lv_obj_t *BeanControls::label(const char *text, int y, const lv_font_t *font) {
    lv_obj_t *obj = lv_label_create(overlay);
    lv_obj_set_width(obj, 330);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
    lv_label_set_text(obj, text);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, y);
    return obj;
}

lv_obj_t *BeanControls::button(const char *text, int x, int y, int width) {
    lv_obj_t *obj = lv_btn_create(overlay);
    lv_obj_set_size(obj, width, 55);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, x, y);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x303030), 0);
    lv_obj_set_style_text_color(obj, lv_color_white(), 0);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, 0);
    lv_obj_set_style_opa(obj, LV_OPA_50, LV_STATE_DISABLED);
    lv_obj_t *caption = lv_label_create(obj);
    lv_label_set_text(caption, text);
    lv_obj_center(caption);
    lv_obj_add_event_cb(obj, onClick, LV_EVENT_CLICKED, this);
    return obj;
}

void BeanControls::show(bool prompt) {
    grindPrompt = prompt;
    overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 480, 480);
    lv_obj_center(overlay);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(overlay, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    title = label(prompt ? "Next grind?" : "Beans", 65, &lv_font_montserrat_34);
    name = label("", 125, &lv_font_montserrat_24);
    detail = label("", 185, &lv_font_montserrat_18);
    value = label("", 252, &lv_font_montserrat_34);
    lv_obj_set_width(value, 140);
    left = button(LV_SYMBOL_LEFT, -130, 245, 65);
    right = button(LV_SYMBOL_RIGHT, 130, 245, 65);
    save = button(prompt ? "Save" : "Select", 0, 320, 160);
    back = button(prompt ? "Skip" : "Back", 0, 390, 120);
    message = label("", 215, &lv_font_montserrat_18);
    for (lv_obj_t *obj : {name, detail, message}) {
        lv_obj_set_height(obj, obj == name ? 56 : 26);
        lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    }
    lv_obj_update_layout(overlay);
    refresh();
}

void BeanControls::refresh() {
    bool empty = beans.empty();
    lv_label_set_text(name, empty ? (grindPrompt ? "Bean no longer available" : "No beans yet") : beans[preview].name.c_str());
    lv_label_set_text(detail, empty ? "Add beans in the web UI" : beans[preview].roaster.c_str());
    char text[40];
    if (grindPrompt) snprintf(text, sizeof(text), "%.1f", grind / 10.0);
    else snprintf(text, sizeof(text), "%d / %u", empty ? 0 : preview + 1, static_cast<unsigned>(beans.size()));
    lv_label_set_text(value, text);
    auto disable = [](lv_obj_t *obj, bool disabled) {
        if (disabled) lv_obj_add_state(obj, LV_STATE_DISABLED);
        else lv_obj_clear_state(obj, LV_STATE_DISABLED);
    };
    disable(left, empty || (grindPrompt ? grind <= 10 : preview == 0));
    disable(right, empty || (grindPrompt ? grind >= 160 : preview + 1 >= static_cast<int>(beans.size())));
    disable(save, empty);
}

void BeanControls::close() {
    if (overlay) lv_obj_del(overlay);
    overlay = nullptr;
}

void BeanControls::onClick(lv_event_t *event) {
    static_cast<BeanControls *>(lv_event_get_user_data(event))->click(lv_event_get_target(event));
}

void BeanControls::click(lv_obj_t *clicked) {
    if (clicked == back) { close(); return; }
    if (beans.empty()) return;
    lv_label_set_text(message, "");
    if (clicked == left || clicked == right) {
        int delta = clicked == left ? -1 : 1;
        if (grindPrompt) grind = std::clamp(grind + delta, 10, 160);
        else preview = std::clamp(preview + delta, 0, static_cast<int>(beans.size()) - 1);
        refresh();
    } else if (clicked == save) {
        JsonDocument request(&psramAllocator), response(&psramAllocator);
        request["tp"] = grindPrompt ? "req:beans:grind" : "req:beans:select";
        request["id"] = beans[preview].id;
        if (grindPrompt) request["grindSetting"] = grind / 10.0;
        ShotHistory.handleBeansRequest(request, response);
        if (!response["error"].isNull()) lv_label_set_text(message, response["error"].as<const char *>());
        else {
            bool selecting = !grindPrompt;
            close();
            if (selecting) controller->getUI()->changeScreen(SCREEN_ID_BREW_SCREEN);
        }
    }
}

void BeanControls::loop() {
    if (!controller) return;
    if (dismiss.exchange(false)) close();
    String pending;
    {
        std::lock_guard<std::mutex> guard(pendingMutex);
        pending = pendingBean;
        pendingBean = "";
    }
    if (!pending.isEmpty() && !controller->isActive() && !controller->isErrorState() && controller->getMode() == MODE_BREW) {
        close();
        promptId = pending;
        grindPrompt = true;
        preview = 0;
        if (loadLibrary() && !beans.empty()) {
            grind = beans[0].grind;
            show(true);
        }
    }
    if (libraryChanged.exchange(false) && overlay) {
        String previewId = beans.empty() ? "" : beans[preview].id;
        if (!loadLibrary()) { lv_label_set_text(message, "Could not read beans. Try again."); return; }
        preview = 0;
        for (size_t i = 0; i < beans.size(); ++i) if (beans[i].id == previewId) preview = i;
        refresh();
    }
}
