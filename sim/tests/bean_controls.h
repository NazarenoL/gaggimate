// Integration tests against the real bean storage and LVGL widgets, in an isolated simulator directory.
#include <cassert>
#include <cmath>
#include <display/ui/default/BeanControls.h>
#include <display/ui/default/eez/images.h>

static JsonDocument beanRequest(const char *type, const String &id = "", double grind = 0) {
    JsonDocument request, response;
    request["tp"] = type;
    request["id"] = id;
    if (grind)
        request["grindSetting"] = grind;
    ShotHistory.handleBeansRequest(request, response);
    assert(response["error"].isNull());
    return response;
}

static lv_obj_t *findBeanText(lv_obj_t *parent, const char *text) {
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(parent); ++i) {
        auto *child = lv_obj_get_child(parent, i);
        if (lv_obj_check_type(child, &lv_label_class) && strcmp(lv_label_get_text(child), text) == 0)
            return child;
        if (auto *found = findBeanText(child, text))
            return found;
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

static lv_obj_t *findBeanIcon(lv_obj_t *parent, const lv_img_dsc_t *image) {
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(parent); ++i) {
        auto *child = lv_obj_get_child(parent, i);
        if (lv_obj_check_type(child, &lv_img_class) && lv_img_get_src(child) == image)
            return child;
        if (auto *found = findBeanIcon(child, image))
            return found;
    }
    return nullptr;
}

static void clickBeanIcon(const lv_img_dsc_t *image) {
    auto *icon = findBeanIcon(lv_layer_top(), image);
    assert(icon);
    auto *button = lv_obj_get_parent(icon);
    assert(!lv_obj_has_state(button, LV_STATE_DISABLED));
    lv_event_send(button, LV_EVENT_CLICKED, nullptr);
}

static int testBeanControls() {
    controller.setMode(MODE_BREW);
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
    assert(!findBeanText(lv_layer_top(), "Back"));
    assert(!findBeanText(lv_layer_top(), "Select"));
    clickBeanIcon(&img_check_40x40);
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
    clickBeanIcon(&img_angle_right_40x40);
    assert(findBeanText(lv_layer_top(), "Bean 12"));
    clickBeanIcon(&img_check_40x40);
    assert(beanRequest("req:beans:list")["selectedId"].as<String>() == "12");
    controller.getUI()->loop();
    assert(strcmp(lv_label_get_text(objects.obj6), "Bean 12") == 0);
    assert(findBeanText(objects.profile_info, "Grind -"));

    // Link multiple symbologies to one bean using the display. Payloads retain leading zeros.
    controls.openSelector();
    clickBeanButton("Scan new code");
    assert(findBeanText(lv_layer_top(), "Cancel scan"));
    events.trigger("beans:barcode", "code", String("0012345678905"));
    controls.loop();
    assert(findBeanText(lv_layer_top(), "Code saved"));
    clickBeanButton("Scan new code");
    events.trigger("beans:barcode", "code", String("https://roaster.example/bag/42"));
    controls.loop();
    library = beanRequest("req:beans:list");
    assert(library["beans"][11]["codes"].size() == 2);
    assert(library["beans"][11]["codes"][0].as<String>() == "0012345678905");
    clickBeanIcon(&img_check_40x40);

    auto codeRequest = [](const char *type, const String &id, const String &code) {
        JsonDocument request, response;
        request["tp"] = type;
        request["id"] = id;
        request["code"] = code;
        ShotHistory.handleBeansRequest(request, response);
        return response;
    };
    // Codes on older beans outside the recent ten still jump directly to their owner.
    assert(codeRequest("req:beans:code", "1", "old-bag")["error"].isNull());
    assert(!codeRequest("req:beans:code", "1", "0012345678905")["error"].isNull());
    events.trigger("beans:barcode", "code", String("old-bag"));
    controls.loop();
    library = beanRequest("req:beans:list");
    assert(library["selectedId"].as<String>() == "1");
    assert(library["recentIds"][0].as<String>() == "1");
    // A known scan wins even while waiting to register a code to a different bean.
    controls.openSelector();
    clickBeanButton("Scan new code");
    events.trigger("beans:barcode", "code", String("https://roaster.example/bag/42"));
    controls.loop();
    assert(beanRequest("req:beans:list")["selectedId"].as<String>() == "12");
    assert(!findBeanText(lv_layer_top(), "Beans"));
    // Exercise controller -> comms callback -> scanner plugin -> actual display.
    controller.getUI()->loop();
    controller.loop();
    auto *client = controller.getClientController();
    uint32_t awakeSession = client->simulatedBarcodeSession();
    assert(awakeSession != 0);
    client->simulateBarcode("old-bag");
    controller.getUI()->loop();
    assert(beanRequest("req:beans:list")["selectedId"].as<String>() == "1");
    controller.setMode(MODE_STANDBY);
    client->simulateBarcode(awakeSession, "0012345678905");
    controller.getUI()->loop();
    assert(beanRequest("req:beans:list")["selectedId"].as<String>() == "1");
    controller.setMode(MODE_BREW);
    controller.loop();
    client->simulateBarcode(awakeSession, "0012345678905");
    controller.getUI()->loop();
    assert(beanRequest("req:beans:list")["selectedId"].as<String>() == "1");
    client->simulateBarcode("0012345678905");
    controller.getUI()->loop();
    assert(beanRequest("req:beans:list")["selectedId"].as<String>() == "12");
    assert(codeRequest("req:beans:code", "12", "0012345678905")["beans"][11]["codes"].size() == 2);

    // Older clients preserve codes when editing a bean, and invalid edits are atomic.
    JsonDocument edit, editResponse;
    edit["tp"] = "req:beans:save";
    edit["id"] = "12";
    edit["bean"]["name"] = "Bean 12";
    edit["bean"]["roaster"] = "Test roaster";
    ShotHistory.handleBeansRequest(edit, editResponse);
    assert(editResponse["beans"][11]["codes"].size() == 2);
    edit["bean"]["codes"].to<JsonArray>().add("old-bag");
    editResponse.clear();
    ShotHistory.handleBeansRequest(edit, editResponse);
    assert(!editResponse["error"].isNull());
    assert(beanRequest("req:beans:list")["beans"][11]["codes"].size() == 2);
    assert(!codeRequest("req:beans:code", "12", "bad\ncode")["error"].isNull());
    assert(!codeRequest("req:beans:code", "12", String("bad\0code", 8))["error"].isNull());
    // Storage failure must leave the prior library intact and return an error.
    assert(LittleFS.mkdir("/beans.tmp"));
    assert(!codeRequest("req:beans:code", "12", "not-saved")["error"].isNull());
    assert(beanRequest("req:beans:list")["beans"][11]["codes"].size() == 2);
    assert(LittleFS.rmdir("/beans.tmp"));
    for (int i = 0; i < 16; ++i)
        assert(codeRequest("req:beans:code", "5", String("code-") + i)["error"].isNull());
    assert(!codeRequest("req:beans:code", "5", "too-many")["error"].isNull());
    assert(!codeRequest("req:beans:scan", "", "unknown")["error"].isNull());
    assert(beanRequest("req:beans:list")["selectedId"].as<String>() == "12");

    // Standby rejects scans both at receipt and at consumption; queued scans cannot wake the machine.
    controller.setMode(MODE_STANDBY);
    events.trigger("beans:barcode", "code", String("old-bag"));
    controls.loop();
    assert(beanRequest("req:beans:list")["selectedId"].as<String>() == "12");
    assert(controller.getMode() == MODE_STANDBY);
    assert(!codeRequest("req:beans:scan", "", "old-bag")["error"].isNull());
    assert(!codeRequest("req:beans:code", "12", "standby-code")["error"].isNull());
    controller.setMode(MODE_BREW);
    events.trigger("beans:barcode", "code", String("old-bag"));
    events.trigger("controller:mode:change");
    controls.loop();
    assert(beanRequest("req:beans:list")["selectedId"].as<String>() == "12");
    controls.openSelector();
    clickBeanButton("Scan new code");
    clickBeanButton("Cancel scan");
    events.trigger("beans:barcode", "code", String("not-linked"));
    controls.loop();
    assert(beanRequest("req:beans:list")["beans"][11]["codes"].size() == 2);
    clickBeanIcon(&img_check_40x40);

    // Explicit [] removes codes; deleting an owner releases a code for reuse.
    edit["id"] = "6";
    edit["bean"]["codes"].to<JsonArray>().add("temporary-code");
    editResponse.clear();
    ShotHistory.handleBeansRequest(edit, editResponse);
    assert(editResponse["error"].isNull());
    edit["bean"]["codes"].to<JsonArray>();
    editResponse.clear();
    ShotHistory.handleBeansRequest(edit, editResponse);
    assert(editResponse["beans"][5]["codes"].size() == 0);
    assert(codeRequest("req:beans:code", "6", "temporary-code")["error"].isNull());
    beanRequest("req:beans:delete", "6");
    assert(codeRequest("req:beans:code", "7", "temporary-code")["error"].isNull());

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
        if (bean["id"].as<String>() == "2")
            assert(std::abs(bean["grindSetting"].as<double>() - 6.2) < 0.00001);
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
        if (bean["id"].as<String>() == "2")
            assert(bean["grindSetting"].as<double>() == 16);
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
    clickBeanIcon(&img_check_40x40);
    beanRequest("req:beans:select", "3", 6.4);
    controller.activate(true);
    assert(controller.isActive());
    assert(!codeRequest("req:beans:scan", "", "old-bag")["error"].isNull());
    events.trigger("beans:barcode", "code", String("old-bag"));
    controls.loop();
    assert(beanRequest("req:beans:list")["selectedId"].as<String>() == "3");
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
    puts("PASS: codes, collisions, limits, legacy edits, standby and active guards, recent ten, selection, grind and real brew");
    return 0;
}
