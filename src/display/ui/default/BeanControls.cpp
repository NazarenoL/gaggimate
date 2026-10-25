#include "BeanControls.h"
#include <algorithm>
#include <cmath>
#include <display/core/Controller.h>
#include <display/core/PluginManager.h>
#include <display/core/process/BrewProcess.h>
#include <display/plugins/ShotHistoryPlugin.h>
#include <display/ui/default/DefaultUI.h>
#include <display/ui/default/eez/images.h>
#include <display/ui/default/eez/screens.h>
#include <display/ui/default/eez/ui.h>
#include <display/util/PsramAllocator.h>

void BeanControls::init(Controller *owner, PluginManager *plugins) {
    controller = owner;
    plugins->on("beans:barcode", [this](Event const &event) {
        if (controller->getMode() == MODE_STANDBY || controller->isActive() || controller->isUpdating() ||
            controller->isErrorState())
            return;
        std::lock_guard<std::mutex> guard(pendingMutex);
        pendingCode = event.getString("code");
    });
    plugins->on("beans:changed", [this](Event const &) { libraryChanged = true; });
    plugins->on("beans:brew:finished", [this](Event const &event) {
        unsigned long durationMs = 0;
        {
            std::lock_guard<std::recursive_mutex> processGuard(controller->getProcessLock());
            Process *last = controller->getLastProcess();
            if (last && last->getType() == MODE_BREW) {
                auto *brew = static_cast<BrewProcess *>(last);
                unsigned long end = brew->processPhase == ProcessPhase::FINISHED ? brew->finished : millis();
                durationMs = end - brew->processStarted;
            }
        }
        std::lock_guard<std::mutex> guard(pendingMutex);
        pendingBean = event.getString("id");
        pendingBrewDurationMs = durationMs;
    });
    for (const char *event : {"controller:brew:start", "controller:mode:change", "ota:update:start", "controller:error"}) {
        plugins->on(event, [this](Event const &) {
            dismiss = true;
            std::lock_guard<std::mutex> guard(pendingMutex);
            pendingCode = "";
        });
    }
}

bool BeanControls::loadLibrary() {
    JsonDocument request(&psramAllocator), response(&psramAllocator);
    request["tp"] = "req:beans:list";
    ShotHistory.handleBeansRequest(request, response);
    if (!response["error"].isNull())
        return false;
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
        for (JsonObject bean : library)
            if (bean["id"].as<String>() == promptId)
                add(bean);
    } else {
        for (JsonVariant id : response["recentIds"].as<JsonArray>()) {
            for (JsonObject bean : library)
                if (bean["id"].as<String>() == id.as<String>())
                    add(bean);
        }
    }
    return true;
}

void BeanControls::openSelector() {
    if (controller->getMode() == MODE_STANDBY || controller->isActive() || controller->isUpdating() || controller->isErrorState())
        return;
    close();
    grindPrompt = false;
    preview = 0;
    bool loaded = loadLibrary();
    for (size_t i = 0; i < beans.size(); ++i)
        if (beans[i].id == selectedId)
            preview = i;
    show(false);
    if (!loaded)
        lv_label_set_text(message, "Could not read beans. Try again.");
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

lv_obj_t *BeanControls::iconButton(const lv_img_dsc_t *image, int x, int y) {
    lv_obj_t *obj = lv_btn_create(overlay);
    lv_obj_set_size(obj, 64, 64);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, x, y);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_opa(obj, LV_OPA_50, LV_STATE_DISABLED);
    lv_obj_t *icon = lv_img_create(obj);
    lv_img_set_src(icon, image);
    lv_obj_set_style_img_recolor(icon, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), 0);
    lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_center(icon);
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
    title = label(prompt ? "Next grind?" : "Beans", prompt ? 65 : 92, prompt ? &lv_font_montserrat_34 : &lv_font_montserrat_18);
    name = label("", prompt ? 125 : 175, &lv_font_montserrat_24);
    detail = label("", prompt ? 185 : 215, &lv_font_montserrat_18);
    if (prompt) {
        char text[40];
        snprintf(text, sizeof(text), "Last brew: %.1f s", brewDurationMs / 1000.0);
        label(text, 215, &lv_font_montserrat_18);
    }
    value = label("", prompt ? 252 : 128, prompt ? &lv_font_montserrat_34 : &lv_font_montserrat_18);
    lv_obj_set_width(value, 140);
    if (prompt) {
        left = button(LV_SYMBOL_LEFT, -130, 245, 65);
        right = button(LV_SYMBOL_RIGHT, 130, 245, 65);
        save = button("Save", 0, 335, 160);
        back = button("Skip", 0, 400, 120);
    } else {
        left = iconButton(&img_angle_left_40x40, -190, 208);
        right = iconButton(&img_angle_right_40x40, 190, 208);
        save = iconButton(&img_check_40x40, 0, 348);
        scan = button("Scan new code", 0, 292, 220);
        lv_obj_set_height(scan, 42);
        lv_obj_set_style_text_font(scan, &lv_font_montserrat_18, 0);
        scanCaption = lv_obj_get_child(scan, 0);
        // Match the profile picker's scrolling bean name and single check.
        lv_obj_set_height(name, 30);
        lv_label_set_long_mode(name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    }
    message = label("", prompt ? 300 : 412, &lv_font_montserrat_18);
    if (!prompt) {
        lv_obj_set_width(message, 260);
        lv_obj_set_height(message, 50);
    }
    for (lv_obj_t *obj : {name, detail, message}) {
        if (prompt || obj == detail) {
            lv_obj_set_height(obj, obj == name ? 56 : 26);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
        }
    }
    lv_obj_update_layout(overlay);
    refresh();
}

void BeanControls::refresh() {
    bool empty = beans.empty();
    lv_label_set_text(name, empty ? (grindPrompt ? "Bean no longer available" : "No beans yet") : beans[preview].name.c_str());
    lv_label_set_text(detail, empty ? "Add beans in the web UI" : beans[preview].roaster.c_str());
    char text[40];
    if (grindPrompt)
        snprintf(text, sizeof(text), "%.1f", grind / 10.0);
    else
        snprintf(text, sizeof(text), "%d / %u", empty ? 0 : preview + 1, static_cast<unsigned>(beans.size()));
    lv_label_set_text(value, text);
    auto disable = [](lv_obj_t *obj, bool disabled) {
        if (disabled)
            lv_obj_add_state(obj, LV_STATE_DISABLED);
        else
            lv_obj_clear_state(obj, LV_STATE_DISABLED);
    };
    disable(left, scanArmed || empty || (grindPrompt ? grind <= 10 : preview == 0));
    disable(right, scanArmed || empty || (grindPrompt ? grind >= 160 : preview + 1 >= static_cast<int>(beans.size())));
    disable(save, grindPrompt && empty);
    if (scan)
        disable(scan, empty);
}

void BeanControls::close() {
    if (overlay)
        lv_obj_del(overlay);
    overlay = nullptr;
    back = scan = scanCaption = nullptr;
    scanArmed = false;
    scanBeanId = "";
}

void BeanControls::onClick(lv_event_t *event) {
    static_cast<BeanControls *>(lv_event_get_user_data(event))->click(lv_event_get_target(event));
}

void BeanControls::click(lv_obj_t *clicked) {
    if (clicked == back) {
        close();
        return;
    }
    if (!grindPrompt && clicked == save && beans.empty()) {
        close();
        return;
    }
    if (beans.empty())
        return;
    lv_label_set_text(message, "");
    if (clicked == scan) {
        scanArmed = !scanArmed;
        scanBeanId = scanArmed ? beans[preview].id : "";
        lv_label_set_text(scanCaption, scanArmed ? "Cancel scan" : "Scan new code");
        lv_label_set_text(message, scanArmed ? "Scan a code for these beans" : "");
        controller->updateLastAction();
        refresh();
    } else if (clicked == left || clicked == right) {
        int delta = clicked == left ? -1 : 1;
        if (grindPrompt)
            grind = std::clamp(grind + delta, 10, 160);
        else
            preview = std::clamp(preview + delta, 0, static_cast<int>(beans.size()) - 1);
        refresh();
    } else if (clicked == save) {
        JsonDocument request(&psramAllocator), response(&psramAllocator);
        request["tp"] = grindPrompt ? "req:beans:grind" : "req:beans:select";
        request["id"] = beans[preview].id;
        if (grindPrompt)
            request["grindSetting"] = grind / 10.0;
        ShotHistory.handleBeansRequest(request, response);
        if (!response["error"].isNull())
            lv_label_set_text(message, response["error"].as<const char *>());
        else {
            bool selecting = !grindPrompt;
            close();
            if (selecting)
                controller->getUI()->changeScreen(SCREEN_ID_BREW_SCREEN);
        }
    }
}

void BeanControls::scanned(const String &code) {
    if (controller->getMode() == MODE_STANDBY || controller->isActive() || controller->isUpdating() ||
        controller->isErrorState() || (overlay && grindPrompt))
        return;
    JsonDocument request(&psramAllocator), response(&psramAllocator);
    request["tp"] = "req:beans:scan";
    request["code"] = code;
    ShotHistory.handleBeansRequest(request, response);
    if (response["error"].isNull()) {
        close();
        controller->updateLastAction();
        controller->getUI()->changeScreen(SCREEN_ID_BREW_SCREEN);
        return;
    }
    // Known codes always select their owner, even while adding a new code.
    if (response["error"].as<String>() == "Unknown code" && scanArmed) {
        request["tp"] = "req:beans:code";
        request["id"] = scanBeanId;
        response.clear();
        ShotHistory.handleBeansRequest(request, response);
        if (response["error"].isNull()) {
            scanArmed = false;
            scanBeanId = "";
            lv_label_set_text(scanCaption, "Scan new code");
            lv_label_set_text(message, "Code saved");
            controller->updateLastAction();
            refresh();
            return;
        }
    }
    if (!overlay)
        openSelector();
    if (overlay) {
        lv_label_set_text(message, response["error"].as<String>() == "Unknown code"
                                       ? "Unknown code. Pick beans, then scan new code."
                                       : response["error"].as<const char *>());
    }
}

void BeanControls::loop() {
    if (!controller)
        return;
    if (dismiss.exchange(false))
        close();
    String pending, code;
    {
        std::lock_guard<std::mutex> guard(pendingMutex);
        pending = pendingBean;
        brewDurationMs = pendingBrewDurationMs;
        pendingBean = "";
        code = pendingCode;
        pendingCode = "";
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
    if (!code.isEmpty())
        scanned(code);
    if (libraryChanged.exchange(false) && overlay) {
        String previewId = beans.empty() ? "" : beans[preview].id;
        if (!loadLibrary()) {
            lv_label_set_text(message, "Could not read beans. Try again.");
            return;
        }
        preview = 0;
        for (size_t i = 0; i < beans.size(); ++i)
            if (beans[i].id == previewId)
                preview = i;
        if (scanArmed && (beans.empty() || beans[preview].id != scanBeanId)) {
            scanArmed = false;
            scanBeanId = "";
            lv_label_set_text(scanCaption, "Scan new code");
            lv_label_set_text(message, "Bean no longer available");
        }
        refresh();
    }
}
