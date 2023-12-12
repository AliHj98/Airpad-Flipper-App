// by @YourName
#include <furi.h>
#include <furi_hal_power.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <gui/elements.h>

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

typedef struct {
    FuriMutex* mutex;
    bool right_click_active;
    bool left_click_active;  // Modified: Flag to track Left Click state
} PluginState;

// Modified: Simulated GPIO functions (replace with actual GPIO functions)
static void set_right_click_gpio(bool state) {
    // Simulated GPIO control, replace with actual GPIO control for PA7
    furi_hal_gpio_init(&gpio_ext_pa7, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_write(&gpio_ext_pa7, state);
}

static void set_left_click_gpio(bool state) {
    // Simulated GPIO control, replace with actual GPIO control for PA6
    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_write(&gpio_ext_pa6, state);
}

static void render_callback(Canvas* const canvas, void* ctx) {
    furi_assert(ctx);
    const PluginState* plugin_state = ctx;
    furi_mutex_acquire(plugin_state->mutex, FuriWaitForever);

    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 64, 2, AlignCenter, AlignTop, "AirMouse Experimental");

    canvas_set_font(canvas, FontSecondary);
    if (!plugin_state->right_click_active) {
        elements_multiline_text_aligned(
            canvas, 64, 28, AlignCenter, AlignTop, "Press Right for right click");
    } else {
        elements_multiline_text_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Right click is pressed!");
        elements_multiline_text_aligned(
            canvas, 64, 40, AlignCenter, AlignTop, "Pressed");
    }

    if (plugin_state->left_click_active) {
        elements_multiline_text_aligned(
            canvas, 64, 52, AlignCenter, AlignTop, "Left click is pressed!");
    } else {
        elements_multiline_text_aligned(
            canvas, 64, 52, AlignCenter, AlignTop, "Press LEFT button for left click");
    }

    furi_mutex_release(plugin_state->mutex);
}

static void input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void set_right_click_state(PluginState* const plugin_state, bool state) {
    plugin_state->right_click_active = state;

    if (state) {
        set_right_click_gpio(true);  // Turn on PA7 when Right Click becomes active
    } else {
        set_right_click_gpio(false);  // Turn off PA7 when Right Click becomes inactive
    }

    furi_mutex_release(plugin_state->mutex);
}

static void set_left_click_state(PluginState* const plugin_state, bool state) {
    plugin_state->left_click_active = state;

    if (state) {
        set_left_click_gpio(true);  // Turn on PA6 when Left Click becomes active
    } else {
        set_left_click_gpio(false);  // Turn off PA6 when Left Click becomes inactive
    }

    furi_mutex_release(plugin_state->mutex);
}

int32_t airmouseesp32_app(void* p) {
    UNUSED(p);

    uint8_t attempts = 0;
    while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }
    furi_delay_ms(200);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(PluginEvent));

    PluginState* plugin_state = malloc(sizeof(PluginState));

    plugin_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if (!plugin_state->mutex) {
        FURI_LOG_E("gpio_controller", "cannot create mutex\r\n");
        furi_message_queue_free(event_queue);
        free(plugin_state);
        return 255;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, plugin_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    PluginEvent event;
    for (bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);

        furi_mutex_acquire(plugin_state->mutex, FuriWaitForever);
        
        if (event_status == FuriStatusOk) {
            if (event.type == EventTypeKey) {
                if (event.input.type == InputTypePress) {
                    switch (event.input.key) {
                    case InputKeyRight:
                        // Toggle Right Click state when the "Right" button is pressed
                        set_right_click_state(plugin_state, true);
                        break;
                    case InputKeyLeft:
                        // Toggle Left Click state when the "Left" button is pressed
                        set_left_click_state(plugin_state, true);
                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    default:
                        break;
                    }
                } else if (event.input.type == InputTypeRelease) {
                    switch (event.input.key) {
                    case InputKeyRight:
                        // Set Right Click state to inactive on button release
                        set_right_click_state(plugin_state, false);
                        break;
                    case InputKeyLeft:
                        // Set Left Click state to inactive on button release
                        set_left_click_state(plugin_state, false);
                        break;
                    default:
                        break;
                    }
                }
            
            }
        }

        furi_mutex_release(plugin_state->mutex);
        view_port_update(view_port);
    }
    if(furi_hal_power_is_otg_enabled()) {
            furi_hal_power_disable_otg();
                }
                
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(plugin_state->mutex);

    return 0;
}