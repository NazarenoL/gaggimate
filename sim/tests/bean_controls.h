// Integration tests against the real bean storage and LVGL widgets, in an isolated simulator directory.
#include <display/ui/default/BeanControls.h>
#include <cassert>
#include <cmath>

static JsonDocument beanRequest(const char *type, const String &id = "", double grind = 0) {
    JsonDocument request, response;
    request["tp"] = type;
    request["id"] = id;
    if (grind) request["grindSetting"] = grind;
    ShotHistory.handleBeansRequest(request, response);
    assert(response["error"].isNull());
    return response;
}

static lv_obj_t *findBeanText(lv_obj_t *parent, const char *text) {
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(parent); ++i) {
        auto *child = lv_obj_get_child(parent, i);
        if (lv_obj_check_type(child, &lv_label_class) && strcmp(lv_label_get_text(child), text) == 0) return child;
        if (auto *found = findBeanText(child, text)) return found;
    }
    return nullptr;
}

static void clickBeanButton(const char *text) {
    auto *label = findBeanText(lv_layer_top(), text);
    assert(label);
    auto *button = lv_obj_get_parent(label);
    assert(!lv_obj_has_state(button, LV_STATE_DISABLED));
    lv_event_send(button, LV_EVENT_CLICKED, nullptr);
}

static int testBeanControls() {
    controller.getUI()->changeScreen(SCREEN_ID_BREW_SCREEN);
    controller.getUI()->loop();
    controller.getUI()->loop();
    assert(strcmp(lv_label_get_text(objects.obj6), "No bean selected") == 0);
    PluginManager events;
    BeanControls controls;
    controls.init(&controller, &events);
    controls.openSelector();
    lv_obj_update_layout(lv_layer_top());
    lv_refr_now(nullptr);
    SdlDriver::getInstance()->pumpAndRender();
    SdlDriver::getInstance()->screenshot("empty-beans.bmp");
    assert(findBeanText(lv_layer_top(), "No beans yet"));
    clickBeanButton("Back");
    for (int i = 1; i <= 12; ++i) {
        JsonDocument request, response;
        request["tp"] = "req:beans:save";
        request["bean"]["name"] = String("Bean ") + i;
        request["bean"]["roaster"] = "Test roaster";
        ShotHistory.handleBeansRequest(request, response);
        assert(response["error"].isNull());
    }
    auto library = beanRequest("req:beans:list");
    assert(library["recentIds"].size() == 10);
    assert(library["recentIds"][0].as<String>() == "12");
    library = beanRequest("req:beans:select", "2", 6.1);
    controller.getUI()->loop();
    assert(strcmp(lv_label_get_text(objects.obj6), "Bean 2") == 0);
    assert(findBeanText(objects.profile_info, "Grind 6.1"));
    assert(objects.profile_select_button && lv_obj_get_parent(objects.profile_select_button) != lv_obj_get_parent(objects.obj6));
    assert(library["recentIds"][0].as<String>() == "2");
    controls.openSelector();
    assert(findBeanText(lv_layer_top(), "Bean 2"));
    assert(findBeanText(lv_layer_top(), "1 / 10"));
    lv_refr_now(nullptr);
    SdlDriver::getInstance()->pumpAndRender();
    SdlDriver::getInstance()->screenshot("bean-selector.bmp");
    clickBeanButton(LV_SYMBOL_RIGHT);
    assert(findBeanText(lv_layer_top(), "Bean 12"));
    clickBeanButton("Select");
    assert(beanRequest("req:beans:list")["selectedId"].as<String>() == "12");
    controller.getUI()->loop();
    assert(strcmp(lv_label_get_text(objects.obj6), "Bean 12") == 0);
    assert(findBeanText(objects.profile_info, "Grind -"));

    // A different bean may be selected in the web UI while the previous shot finishes.
    events.trigger("beans:brew:finished", "id", String("2"));
    controller.setMode(MODE_BREW);
    controls.loop();
    assert(findBeanText(lv_layer_top(), "Next grind?"));
    assert(findBeanText(lv_layer_top(), "6.1"));
    lv_refr_now(nullptr);
    SdlDriver::getInstance()->pumpAndRender();
    SdlDriver::getInstance()->screenshot("next-grind.bmp");
    clickBeanButton(LV_SYMBOL_RIGHT);
    assert(findBeanText(lv_layer_top(), "6.2"));
    clickBeanButton("Save");
    library = beanRequest("req:beans:list");
    assert(library["selectedId"].as<String>() == "12");
    for (JsonObject bean : library["beans"].as<JsonArray>()) {
        if (bean["id"].as<String>() == "2") assert(std::abs(bean["grindSetting"].as<double>() - 6.2) < 0.00001);
    }
    beanRequest("req:beans:grind", "2", 16);
    events.trigger("beans:brew:finished", "id", String("2"));
    controls.loop();
    auto *arrow = findBeanText(lv_layer_top(), LV_SYMBOL_RIGHT);
    assert(arrow && lv_obj_has_state(lv_obj_get_parent(arrow), LV_STATE_DISABLED));
    clickBeanButton(LV_SYMBOL_LEFT);
    assert(findBeanText(lv_layer_top(), "15.9"));
    clickBeanButton("Skip");
    library = beanRequest("req:beans:list");
    for (JsonObject bean : library["beans"].as<JsonArray>()) {
        if (bean["id"].as<String>() == "2") assert(bean["grindSetting"].as<double>() == 16);
    }
    beanRequest("req:beans:grind", "2", 1);
    events.trigger("beans:brew:finished", "id", String("2"));
    controls.loop();
    arrow = findBeanText(lv_layer_top(), LV_SYMBOL_LEFT);
    assert(arrow && lv_obj_has_state(lv_obj_get_parent(arrow), LV_STATE_DISABLED));
    beanRequest("req:beans:delete", "2");
    events.trigger("beans:changed");
    controls.loop();
    assert(findBeanText(lv_layer_top(), "Bean no longer available"));
    auto *save = findBeanText(lv_layer_top(), "Save");
    assert(save && lv_obj_has_state(lv_obj_get_parent(save), LV_STATE_DISABLED));
    clickBeanButton("Skip");
    // Exercise the real menu and brew event wiring, rather than only synthetic prompt events.
    controller.getUI()->changeScreen(SCREEN_ID_MENU_SCREEN_NEW);
    controller.getUI()->loop();
    controller.getUI()->loop();
    assert(objects.btn_grind_1 && !lv_obj_has_flag(objects.btn_grind_1, LV_OBJ_FLAG_HIDDEN));
    controller.getUI()->onBeanSwitch();
    assert(findBeanText(lv_layer_top(), "Beans"));
    clickBeanButton("Back");
    beanRequest("req:beans:select", "3", 6.4);
    controller.activate(true);
    assert(controller.isActive());
    controller.getUI()->loop();
    beanRequest("req:beans:select", "4", 7.3);
    controller.deactivate();
    controller.getUI()->loop();
    assert(findBeanText(lv_layer_top(), "Next grind?"));
    assert(findBeanText(lv_layer_top(), "Bean 3"));
    assert(findBeanText(lv_layer_top(), "6.4"));
    clickBeanButton(LV_SYMBOL_LEFT);
    clickBeanButton("Save");
    assert(beanRequest("req:beans:list")["selectedId"].as<String>() == "4");
    controller.getUI()->loop();
    assert(strcmp(lv_label_get_text(objects.obj6), "Bean 4") == 0);
    assert(findBeanText(objects.profile_info, "Grind 7.3"));
    beanRequest("req:beans:grind", "4", 7.5);
    controller.getUI()->loop();
    assert(findBeanText(objects.profile_info, "Grind 7.5"));
    controller.clear();
    controller.getUI()->markDirty();
    controller.getUI()->loop();
    lv_refr_now(nullptr);
    SdlDriver::getInstance()->pumpAndRender();
    SdlDriver::getInstance()->screenshot("brew-bean-summary.bmp");
    controller.clear();
    controller.onFlush();
    assert(controller.isActive());
    controller.getUI()->loop();
    controller.deactivate();
    controller.getUI()->loop();
    assert(!findBeanText(lv_layer_top(), "Next grind?"));
    controller.clear();
    controller.getUI()->markDirty();
    controller.getUI()->loop();
    // Long names must leave the grind visible; deleting the selected bean clears the caption.
    JsonDocument request, response;
    request["tp"] = "req:beans:save";
    request["bean"]["name"] = "A very long bean name that cannot fit inside the brew screen caption";
    request["bean"]["roaster"] = "Test roaster";
    ShotHistory.handleBeansRequest(request, response);
    assert(response["error"].isNull());
    String longId = response["beans"][response["beans"].size() - 1]["id"].as<String>();
    beanRequest("req:beans:select", longId, 8.2);
    controller.getUI()->loop();
    lv_obj_update_layout(objects.profile_info);
    auto *grindLabel = findBeanText(objects.profile_info, "Grind 8.2");
    assert(grindLabel);
    lv_area_t rowArea, grindArea;
    lv_obj_get_coords(lv_obj_get_parent(objects.obj6), &rowArea);
    lv_obj_get_coords(grindLabel, &grindArea);
    assert(grindArea.x2 <= rowArea.x2);
    lv_refr_now(nullptr);
    SdlDriver::getInstance()->pumpAndRender();
    SdlDriver::getInstance()->screenshot("brew-long-bean-summary.bmp");
    beanRequest("req:beans:delete", longId);
    controller.getUI()->loop();
    assert(strcmp(lv_label_get_text(objects.obj6), "No bean selected") == 0);
    puts("PASS: recent ten, selection, saved default, adjustment, bounds, skip, deletion, menu, real brew and utility flush");
    return 0;
}
