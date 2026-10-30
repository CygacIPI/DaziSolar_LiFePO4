#include "battery.h"
#include "battery_csv.h"
#include "raylib.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define HISTORY_CAPACITY 60
#define DO_D_PCT 80

static const Color COLOR_BACKGROUND = {246, 248, 250, 255};
static const Color COLOR_PANEL = {255, 255, 255, 255};
static const Color COLOR_PANEL_LIGHT = {246, 249, 251, 255};
static const Color COLOR_BORDER = {224, 230, 235, 255};
static const Color COLOR_TEXT = {35, 48, 60, 255};
static const Color COLOR_MUTED = {119, 132, 143, 255};
static const Color COLOR_CYAN = {42, 117, 163, 255};
static const Color COLOR_GREEN = {111, 163, 63, 255};
static const Color COLOR_AMBER = {204, 143, 42, 255};
static const Color COLOR_RED = {194, 73, 65, 255};
static const Color COLOR_SIDEBAR = {255, 255, 255, 255};

static Font UI_FONT;
static Font UI_FONT_BOLD;
static bool UI_FONT_LOADED;
static bool UI_FONT_BOLD_LOADED;

typedef struct
{
    battery_data_t data;
    time_t timestamp;
} history_sample_t;

typedef struct
{
    history_sample_t samples[HISTORY_CAPACITY];
    size_t count;
} history_t;

static void draw_text(const char *text, int x, int y, int size, Color color)
{
    if (UI_FONT_LOADED)
    {
        DrawTextEx(UI_FONT, text, (Vector2){(float)x, (float)y},
                   (float)size, 0.5f, color);
    }
    else
    {
        DrawText(text, x, y, size, color);
    }
}

static void draw_text_bold(const char *text, int x, int y, int size, Color color)
{
    if (UI_FONT_BOLD_LOADED)
    {
        DrawTextEx(UI_FONT_BOLD, text, (Vector2){(float)x, (float)y},
                   (float)size, 0.5f, color);
    }
    else
    {
        draw_text(text, x, y, size, color);
    }
}

static float measure_text_width(const char *text, int size)
{
    if (UI_FONT_LOADED)
    {
        return MeasureTextEx(UI_FONT, text, (float)size, 0.5f).x;
    }
    return (float)MeasureText(text, size);
}

static void draw_text_fit(const char *text, int x, int y, int size,
                          Color color, float max_width)
{
    if (text == NULL || max_width <= 0.0f)
    {
        return;
    }

    char visible[160];
    const size_t length = strlen(text);
    if (length >= sizeof(visible))
    {
        snprintf(visible, sizeof(visible), "%s", text);
    }
    else
    {
        memcpy(visible, text, length + 1);
    }

    if (measure_text_width(visible, size) > max_width)
    {
        size_t visible_length = strlen(visible);
        while (visible_length > 3)
        {
            visible_length--;
            visible[visible_length] = '\0';
            if (visible_length > 3)
            {
                visible[visible_length - 1] = '.';
                visible[visible_length - 2] = '.';
                visible[visible_length - 3] = '.';
                visible[visible_length] = '\0';
            }
            if (measure_text_width(visible, size) <= max_width)
            {
                break;
            }
        }
    }
    draw_text(visible, x, y, size, color);
}

static bool load_ui_fonts(void)
{
    const char *regular_paths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf"};
    const char *bold_paths[] = {
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/arialbd.ttf"};

    for (size_t i = 0; i < sizeof(regular_paths) / sizeof(regular_paths[0]); ++i)
    {
        if (FileExists(regular_paths[i]))
        {
            UI_FONT = LoadFontEx(regular_paths[i], 64, NULL, 0);
            if (UI_FONT.texture.id != 0)
            {
                UI_FONT_LOADED = true;
                SetTextureFilter(UI_FONT.texture, TEXTURE_FILTER_BILINEAR);
                break;
            }
        }
    }

    for (size_t i = 0; i < sizeof(bold_paths) / sizeof(bold_paths[0]); ++i)
    {
        if (FileExists(bold_paths[i]))
        {
            UI_FONT_BOLD = LoadFontEx(bold_paths[i], 64, NULL, 0);
            if (UI_FONT_BOLD.texture.id != 0)
            {
                UI_FONT_BOLD_LOADED = true;
                SetTextureFilter(UI_FONT_BOLD.texture, TEXTURE_FILTER_BILINEAR);
                break;
            }
        }
    }

    return UI_FONT_LOADED;
}

static void draw_panel(Rectangle bounds)
{
    DrawRectangle((int)bounds.x + 1, (int)bounds.y + 2,
                  (int)bounds.width, (int)bounds.height,
                  (Color){29, 45, 59, 10});
    DrawRectangleRec(bounds, COLOR_PANEL);
    DrawRectangleLinesEx(bounds, 1.0f, COLOR_BORDER);
}

static bool button(Rectangle bounds, const char *label, bool selected, Color accent)
{
    const Vector2 mouse = GetMousePosition();
    const bool hovered = CheckCollisionPointRec(mouse, bounds);
    const bool pressed = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const Color fill = selected ? accent : (hovered ? COLOR_PANEL_LIGHT : COLOR_PANEL);
    const Color text = selected ? COLOR_PANEL : COLOR_TEXT;

    DrawRectangleRec(bounds, fill);
    DrawRectangleLinesEx(bounds, 1.0f, selected ? accent : COLOR_BORDER);
    const int label_width = (int)measure_text_width(label, 15);
    draw_text(label, (int)(bounds.x + (bounds.width - (float)label_width) / 2.0f),
              (int)(bounds.y + (bounds.height - 15) / 2), 15, text);
    return pressed;
}

static void draw_metric_card(Rectangle bounds, const char *title,
                             const char *value, const char *detail,
                             Color accent)
{
    draw_panel(bounds);
    draw_text(title, (int)bounds.x + 16, (int)bounds.y + 13, 13, COLOR_MUTED);
    draw_text_bold(value, (int)bounds.x + 16, (int)bounds.y + 35, 24, accent);
    if (detail != NULL)
    {
        draw_text(detail, (int)bounds.x + 16, (int)bounds.y + 68, 12, COLOR_MUTED);
    }
}

static void draw_energy_card(Rectangle bounds,
                             const battery_calculations_t *calculations)
{
    char energy[32];
    char nominal[48];
    char usable[48];
    if (calculations->stored_energy_available)
    {
        snprintf(energy, sizeof(energy), "%.2f kWh",
                 calculations->stored_energy_Wh / 1000.0);
    }
    else
    {
        snprintf(energy, sizeof(energy), "No disponible");
    }
    if (calculations->nominal_energy_available)
    {
        snprintf(nominal, sizeof(nominal), "Nominal %.2f kWh",
                 calculations->nominal_energy_Wh / 1000.0);
        snprintf(usable, sizeof(usable), "Util DoD %d%%: %.2f kWh", DO_D_PCT,
                 calculations->usable_nominal_energy_Wh / 1000.0);
    }
    else
    {
        snprintf(nominal, sizeof(nominal), "Capacidad nominal desconocida");
        usable[0] = '\0';
    }

    draw_panel(bounds);
    draw_text("ENERGIA ALMACENADA", (int)bounds.x + 18, (int)bounds.y + 14,
              13, COLOR_MUTED);
    draw_text_bold(energy, (int)bounds.x + 18, (int)bounds.y + 37, 32, COLOR_CYAN);
    draw_text(nominal, (int)bounds.x + 18, (int)bounds.y + 88, 11, COLOR_MUTED);
    if (usable[0] != '\0')
    {
        draw_text(usable, (int)bounds.x + 18, (int)bounds.y + 104, 11,
                  COLOR_MUTED);
    }
}

static void draw_soc_card(Rectangle bounds, const battery_data_t *data)
{
    char percent[16];
    char capacity[64];
    const bool has_soc = battery_has_field(data, BATTERY_FIELD_SOC);
    const bool has_remaining =
        battery_has_field(data, BATTERY_FIELD_REMAINING_CAPACITY);
    const bool has_nominal =
        battery_has_field(data, BATTERY_FIELD_NOMINAL_CAPACITY);
    snprintf(percent, sizeof(percent), has_soc ? "%u%%" : "--",
             has_soc ? data->soc_pct : 0u);
    if (has_remaining && has_nominal)
    {
        snprintf(capacity, sizeof(capacity), "%.2f Ah restantes de %.0f Ah",
                 (double)data->remaining_mAh / 1000.0,
                 (double)data->nominal_mAh / 1000.0);
    }
    else if (has_remaining)
    {
        snprintf(capacity, sizeof(capacity), "%.2f Ah restantes",
                 (double)data->remaining_mAh / 1000.0);
    }
    else
    {
        snprintf(capacity, sizeof(capacity), "Capacidad restante desconocida");
    }

    draw_panel(bounds);
    draw_text("ESTADO DE CARGA", (int)bounds.x + 18, (int)bounds.y + 14,
              14, COLOR_MUTED);
    draw_text_bold(percent, (int)bounds.x + 18, (int)bounds.y + 36, 33,
                   COLOR_CYAN);
    const Rectangle bar = {bounds.x + 18, bounds.y + 83, bounds.width - 36, 13};
    DrawRectangleRec(bar, COLOR_PANEL_LIGHT);
    Rectangle fill = bar;
    fill.width *= has_soc ? (float)data->soc_pct / 100.0f : 0.0f;
    if (fill.width > 0)
    {
        DrawRectangleRec(fill, COLOR_CYAN);
    }
    draw_text(capacity, (int)bounds.x + 18, (int)bounds.y + 103, 12, COLOR_MUTED);
}

static void draw_runtime_card(Rectangle bounds,
                              const battery_calculations_t *calculations)
{
    char runtime[32];
    char detail[64];
    if (calculations->runtime_available)
    {
        snprintf(runtime, sizeof(runtime), "%.1f h", calculations->runtime_h);
        snprintf(detail, sizeof(detail), "Estimacion a %.0f W de descarga",
                 calculations->power_W);
    }
    else
    {
        snprintf(runtime, sizeof(runtime), "--");
        snprintf(detail, sizeof(detail), "Disponible durante descarga");
    }

    draw_panel(bounds);
    draw_text("AUTONOMIA ESTIMADA", (int)bounds.x + 18, (int)bounds.y + 14,
              14, COLOR_MUTED);
    draw_text_bold(runtime, (int)bounds.x + 18, (int)bounds.y + 38, 33, COLOR_CYAN);
    draw_text(detail, (int)bounds.x + 18, (int)bounds.y + 91, 12, COLOR_MUTED);
}

static void draw_cells_panel(Rectangle bounds, const battery_data_t *data)
{
    draw_panel(bounds);
    draw_text("TENSION POR CELDA", (int)bounds.x + 18, (int)bounds.y + 14,
              14, COLOR_MUTED);

    if (!battery_has_field(data, BATTERY_FIELD_CELLS))
    {
        draw_text("Datos de celdas no disponibles", (int)bounds.x + 18,
                  (int)bounds.y + 64, 15, COLOR_MUTED);
        return;
    }
    if (data->cell_count == 0)
    {
        draw_text("Sin celdas reportadas", (int)bounds.x + 18,
                  (int)bounds.y + 64, 15, COLOR_MUTED);
        return;
    }

    uint16_t minimum = UINT16_MAX;
    uint16_t maximum = 0;
    for (uint8_t i = 0; i < data->cell_count; ++i)
    {
        if (data->cell_mV[i] < minimum)
            minimum = data->cell_mV[i];
        if (data->cell_mV[i] > maximum)
            maximum = data->cell_mV[i];
    }
    char delta[48];
    snprintf(delta, sizeof(delta), "Diferencia max.: %u mV",
             data->cell_count > 0 ? (unsigned)(maximum - minimum) : 0u);
    const int delta_width = (int)measure_text_width(delta, 12);
    draw_text(delta, (int)(bounds.x + bounds.width - (float)delta_width - 18.0f),
              (int)bounds.y + 16, 12, COLOR_MUTED);

    const int columns = data->cell_count > 16  ? 8
                        : data->cell_count > 4 ? 4
                                               : data->cell_count;
    const float gap = 6.0f;
    const float cell_width =
        (bounds.width - 36.0f - gap * (float)(columns - 1)) /
        (float)columns;
    const float cell_height = 34.0f;
    const float start_y = bounds.y + 48.0f;

    for (uint8_t i = 0; i < data->cell_count; ++i)
    {
        const int row = i / columns;
        const int column = i % columns;
        const Rectangle cell = {
            bounds.x + 18.0f + (float)column * (cell_width + gap),
            start_y + (float)row * (cell_height + 5.0f),
            cell_width,
            cell_height};
        char value[24];
        char label[12];
        snprintf(value, sizeof(value), "%.3f V",
                 (double)data->cell_mV[i] / 1000.0);
        snprintf(label, sizeof(label), "CELDA %02u", (unsigned)(i + 1));
        DrawRectangleRec(cell, COLOR_PANEL_LIGHT);
        DrawRectangleLinesEx(cell, 1.0f, COLOR_BORDER);
        draw_text(label, (int)cell.x + 8, (int)cell.y + 3, 10, COLOR_MUTED);
        draw_text_bold(value, (int)cell.x + 8, (int)cell.y + 17, 13,
                       COLOR_TEXT);
    }
}

static const char *alarm_label(uint32_t alarm)
{
    switch (alarm)
    {
    case BATTERY_ALARM_VOLTAGE_LOW:
        return "Tension de pack baja";
    case BATTERY_ALARM_VOLTAGE_HIGH:
        return "Tension de pack alta";
    case BATTERY_ALARM_CHARGE_VOLTAGE_HIGH:
        return "Tension de carga alta";
    case BATTERY_ALARM_CHARGE_TEMPERATURE:
        return "Temperatura fuera de carga";
    case BATTERY_ALARM_DISCHARGE_TEMPERATURE:
        return "Temperatura fuera de descarga";
    case BATTERY_ALARM_CHARGE_CURRENT_HIGH:
        return "Corriente de carga alta";
    case BATTERY_ALARM_DISCHARGE_CURRENT_HIGH:
        return "Corriente de descarga alta";
    default:
        return "Alarma desconocida";
    }
}

static void draw_conditions_panel(Rectangle bounds, const battery_data_t *data)
{
    draw_panel(bounds);
    draw_text("CONDICIONES (ALARMAS PROVISIONALES)", (int)bounds.x + 18,
              (int)bounds.y + 14, 14, COLOR_MUTED);

    if (!battery_has_field(data, BATTERY_FIELD_TEMPERATURES))
    {
        draw_text("Temperaturas no disponibles", (int)bounds.x + 18,
                  (int)bounds.y + 48, 13, COLOR_MUTED);
    }
    else if (data->temp_count == 0)
    {
        draw_text("Sin sensores reportados", (int)bounds.x + 18,
                  (int)bounds.y + 48, 13, COLOR_MUTED);
    }
    else
    {
        const uint8_t sensors_per_row = 4;
        for (uint8_t row = 0; row < 2; ++row)
        {
            char temperatures[80] = "";
            size_t used = 0;
            const uint8_t first = (uint8_t)(row * sensors_per_row);
            for (uint8_t i = first;
                 i < data->temp_count && i < (uint8_t)(first + sensors_per_row);
                 ++i)
            {
                const int written = snprintf(
                    temperatures + used, sizeof(temperatures) - used,
                    "%sT%u %.1f C", i == first ? "" : "  ",
                    (unsigned)(i + 1), (double)data->temp_dC[i] / 10.0);
                if (written < 0 ||
                    (size_t)written >= sizeof(temperatures) - used)
                {
                    break;
                }
                used += (size_t)written;
            }
            if (used > 0)
            {
                draw_text(temperatures, (int)bounds.x + 18,
                          (int)bounds.y + 46 + (int)row * 17, 12, COLOR_TEXT);
            }
        }
    }

    const int alarm_y = (int)bounds.y + 96;
    int alarm_row = 0;
    uint32_t known_alarms = 0;
    battery_alarm_assessment_t assessment;
    if (!battery_assess_alarms(data, &assessment))
    {
        draw_text("No se pudieron evaluar las condiciones",
                  (int)bounds.x + 18, (int)bounds.y + 83, 12, COLOR_RED);
        return;
    }

    if (!assessment.current_limit_evaluable)
    {
        draw_text("Sobrecorriente no evaluable: falta dato o perfil",
                  (int)bounds.x + 18, (int)bounds.y + 82, 11, COLOR_AMBER);
    }
    else if (!battery_has_field(data, BATTERY_FIELD_VOLTAGE) ||
             !battery_has_field(data, BATTERY_FIELD_TEMPERATURES) ||
             data->temp_count == 0)
    {
        draw_text("Evaluacion incompleta: faltan tension o temperatura",
                  (int)bounds.x + 18, (int)bounds.y + 82, 11, COLOR_AMBER);
    }
    if (!battery_has_field(data, BATTERY_FIELD_ALARMS))
    {
        draw_text("Alarmas no disponibles", (int)bounds.x + 18,
                  alarm_y, 13, COLOR_MUTED);
        return;
    }
    for (uint32_t bit = 1; bit <= BATTERY_ALARM_DISCHARGE_CURRENT_HIGH; bit <<= 1)
    {
        if ((data->alarms & bit) == 0)
            continue;
        known_alarms |= bit;
        draw_text(alarm_label(bit), (int)bounds.x + 18,
                  alarm_y + alarm_row * 16, 12, COLOR_RED);
        alarm_row++;
    }

    if (known_alarms == 0)
    {
        const bool alarm_inputs_complete =
            assessment.current_limit_evaluable &&
            battery_has_field(data, BATTERY_FIELD_VOLTAGE) &&
            battery_has_field(data, BATTERY_FIELD_TEMPERATURES) &&
            data->temp_count > 0;
        const char *message = alarm_inputs_complete
                                  ? "Sin alarmas activas"
                                  : "Sin alarmas detectadas; evaluacion incompleta";
        draw_text(message, (int)bounds.x + 18, alarm_y,
                  alarm_inputs_complete ? 13 : 11,
                  alarm_inputs_complete ? COLOR_GREEN : COLOR_AMBER);
    }
}

static bool sample_power_W(const history_sample_t *sample, float *power_W)
{
    if (sample == NULL || power_W == NULL ||
        !battery_has_field(&sample->data, BATTERY_FIELD_CURRENT))
    {
        return false;
    }
    battery_calculations_t calculations;
    if (!battery_calculate(&sample->data, DO_D_PCT, &calculations) ||
        !calculations.power_available)
    {
        return false;
    }
    const double signed_power =
        sample->data.current_mA < 0 ? -calculations.power_W
                                    : calculations.power_W;
    *power_W = (float)signed_power;
    return isfinite(*power_W);
}

static float chart_scale_for_power(float required)
{
    if (!isfinite(required) || required <= 0.0f)
    {
        return 100.0f;
    }

    const float exponent = floorf(log10f(required));
    const float base = powf(10.0f, exponent);
    const float normalized = required / base;
    const float multiplier = normalized <= 1.0f   ? 1.0f
                             : normalized <= 2.0f ? 2.0f
                             : normalized <= 5.0f ? 5.0f
                                                  : 10.0f;
    return multiplier * base;
}

static void format_chart_power(float power_W, char *text, size_t text_size)
{
    const float magnitude = fabsf(power_W);
    if (magnitude < 0.0005f)
    {
        snprintf(text, text_size, "0 W");
    }
    else if (magnitude >= 1000.0f)
    {
        snprintf(text, text_size, "%+.1f kW", (double)(power_W / 1000.0f));
    }
    else if (magnitude >= 10.0f)
    {
        snprintf(text, text_size, "%+.0f W", (double)power_W);
    }
    else if (magnitude >= 1.0f)
    {
        snprintf(text, text_size, "%+.1f W", (double)power_W);
    }
    else
    {
        snprintf(text, text_size, "%+.0f mW", (double)(power_W * 1000.0f));
    }
}

static void draw_time_label(time_t timestamp, int x, int y, Color color,
                            bool center, bool align_right)
{
    struct tm *time_pointer = gmtime(&timestamp);
    if (time_pointer == NULL)
    {
        return;
    }
    const struct tm utc = *time_pointer;
    char label[16];
    if (strftime(label, sizeof(label), "%H:%M:%S", &utc) == 0)
    {
        return;
    }
    const int label_width = (int)measure_text_width(label, 10);
    const int label_x = center        ? x - label_width / 2
                        : align_right ? x - label_width
                                      : x;
    draw_text(label, label_x, y, 10, color);
}

static void draw_power_chart(Rectangle bounds, const history_t *history)
{
    draw_panel(bounds);
    draw_text("POTENCIA ESTIMADA POR MUESTRA (UTC)", (int)bounds.x + 18,
              (int)bounds.y + 12, 14, COLOR_MUTED);

    const Rectangle plot = {
        bounds.x + 76,
        bounds.y + 43,
        bounds.width - 96,
        bounds.height - 73};
    if (history->count < 2 || plot.width <= 0 || plot.height <= 0)
    {
        draw_text("Esperando muestras del simulador...",
                  (int)bounds.x + 18, (int)bounds.y + 58, 13, COLOR_MUTED);
        return;
    }

    const int legend_y = (int)bounds.y + 14;
    const int legend_x = (int)(bounds.x + bounds.width - 177.0f);
    DrawLineEx((Vector2){(float)legend_x, (float)legend_y + 7.0f},
               (Vector2){(float)legend_x + 14.0f, (float)legend_y + 7.0f},
               2.5f, COLOR_GREEN);
    draw_text("Carga +", legend_x + 19, legend_y, 11, COLOR_TEXT);
    DrawLineEx((Vector2){(float)legend_x + 91.0f, (float)legend_y + 7.0f},
               (Vector2){(float)legend_x + 105.0f, (float)legend_y + 7.0f},
               2.5f, COLOR_CYAN);
    draw_text("Descarga -", legend_x + 110, legend_y, 11, COLOR_TEXT);

    float required_scale = 0.0f;
    for (size_t i = 0; i < history->count; ++i)
    {
        float power;
        if (!sample_power_W(&history->samples[i], &power))
        {
            continue;
        }
        if (fabsf(power) > required_scale)
            required_scale = fabsf(power);
    }
    const float max_abs = chart_scale_for_power(required_scale * 1.1f);

    const float half_height = plot.height / 2.0f;
    const float zero_y = plot.y + half_height;
    DrawLineEx((Vector2){plot.x, plot.y}, (Vector2){plot.x, plot.y + plot.height},
               1.0f, COLOR_BORDER);
    DrawLineEx((Vector2){plot.x, plot.y + plot.height},
               (Vector2){plot.x + plot.width, plot.y + plot.height},
               1.0f, COLOR_BORDER);

    for (int tick = 0; tick <= 4; ++tick)
    {
        const float fraction = (float)tick / 4.0f;
        const float y = plot.y + fraction * plot.height;
        const float value = max_abs * (1.0f - 2.0f * fraction);
        const Color grid_color = tick == 2 ? COLOR_MUTED : COLOR_BORDER;
        DrawLineEx((Vector2){plot.x, y},
                   (Vector2){plot.x + plot.width, y},
                   tick == 2 ? 1.5f : 1.0f, grid_color);
        char label[24];
        format_chart_power(value, label, sizeof(label));
        const int label_width = (int)measure_text_width(label, 10);
        draw_text(label, (int)plot.x - label_width - 8,
                  (int)y - 5, 10, COLOR_MUTED);
    }

    for (int tick = 0; tick <= 2; ++tick)
    {
        const float x = plot.x + (float)tick * plot.width / 2.0f;
        if (tick == 1)
        {
            DrawLineEx((Vector2){x, plot.y},
                       (Vector2){x, plot.y + plot.height},
                       1.0f, COLOR_BORDER);
        }
        const size_t sample_index =
            (history->count - 1) * (size_t)tick / 2u;
        draw_time_label(history->samples[sample_index].timestamp,
                        (int)x, (int)(plot.y + plot.height + 5.0f),
                        COLOR_MUTED, tick == 1, tick == 2);
    }

    for (size_t i = 1; i < history->count; ++i)
    {
        float previous_power;
        float current_power;
        if (!sample_power_W(&history->samples[i - 1], &previous_power) ||
            !sample_power_W(&history->samples[i], &current_power))
        {
            continue;
        }
        const Vector2 previous = {
            plot.x + (float)(i - 1) * plot.width / (float)(history->count - 1),
            zero_y - previous_power * (plot.height / 2.0f) / max_abs};
        const Vector2 current = {
            plot.x + (float)i * plot.width / (float)(history->count - 1),
            zero_y - current_power * half_height / max_abs};
        const float midpoint = (previous_power + current_power) / 2.0f;
        const Color line_color = midpoint > 0.0f   ? COLOR_GREEN
                                 : midpoint < 0.0f ? COLOR_CYAN
                                                   : COLOR_MUTED;
        DrawLineEx(previous, current, 2.5f, line_color);
    }
}

static bool append_sample(history_t *history, const battery_data_t *data)
{
    if (history == NULL || history->count > HISTORY_CAPACITY ||
        data == NULL || !battery_data_is_valid(data))
    {
        return false;
    }
    const time_t timestamp = time(NULL);
    if (timestamp == (time_t)-1)
    {
        return false;
    }
    if (history->count == HISTORY_CAPACITY)
    {
        memmove(history->samples, history->samples + 1,
                (HISTORY_CAPACITY - 1) * sizeof(history->samples[0]));
        history->count--;
    }
    history->samples[history->count].data = *data;
    history->samples[history->count].timestamp = timestamp;
    history->count++;
    return true;
}

static void draw_sidebar(int screen_height, Texture2D logo, bool logo_loaded)
{
    const float sidebar_width = 220.0f;
    DrawRectangle(0, 0, (int)sidebar_width, screen_height, COLOR_SIDEBAR);
    DrawLine((int)sidebar_width, 0, (int)sidebar_width, screen_height, COLOR_BORDER);

    if (logo_loaded)
    {
        const float logo_height = 38.0f;
        const float logo_width = logo_height *
                                 (float)logo.width / (float)logo.height;
        const Rectangle source = {
            0.0f, 0.0f, (float)logo.width, (float)logo.height};
        const Rectangle destination = {
            24.0f, 22.0f, logo_width, logo_height};
        DrawTexturePro(logo, source, destination, (Vector2){0.0f, 0.0f},
                       0.0f, WHITE);
    }
    else
    {
        draw_text_bold("DAZI SOLAR", 24, 31, 22, COLOR_TEXT);
    }
    DrawLine(24, 78, 196, 78, COLOR_BORDER);

    draw_text("MONITOREO", 24, 103, 11, COLOR_MUTED);
    const Rectangle active_item = {12, 128, 196, 40};
    DrawRectangleRec(active_item, (Color){239, 246, 233, 255});
    DrawRectangle(12, 128, 3, 40, COLOR_GREEN);
    DrawCircle(34, 148, 4.0f, COLOR_GREEN);
    draw_text("Panel principal", 50, 139, 14, COLOR_TEXT);

    draw_text("SISTEMA", 24, 196, 11, COLOR_MUTED);
    DrawCircle(34, 232, 4.0f, COLOR_CYAN);
    draw_text("Pack de bateria", 50, 224, 14, COLOR_TEXT);
    DrawCircle(34, 270, 4.0f, COLOR_MUTED);
    draw_text("Celdas y sensores", 50, 262, 14, COLOR_MUTED);
    DrawCircle(34, 308, 4.0f, COLOR_MUTED);
    draw_text("Historial", 50, 300, 14, COLOR_MUTED);

    const float footer_y = (float)screen_height - 100.0f;
    DrawLine(24, (int)footer_y, 196, (int)footer_y, COLOR_BORDER);
    DrawCircle(34, (int)footer_y + 28, 4.0f, COLOR_CYAN);
    draw_text("Origen de datos", 50, (int)footer_y + 19, 12, COLOR_MUTED);
    draw_text("Simulador local", 50, (int)footer_y + 39, 13, COLOR_TEXT);
}

static bool export_history_csv(const history_t *history, char *error, size_t error_size)
{
    if (history == NULL || error == NULL || error_size == 0 ||
        history->count > HISTORY_CAPACITY)
    {
        if (error != NULL && error_size > 0)
        {
            snprintf(error, error_size, "Historial invalido para exportar.");
        }
        return false;
    }

    const time_t now = time(NULL);
    const struct tm *utc_pointer = gmtime(&now);
    if (utc_pointer == NULL)
    {
        snprintf(error, error_size, "No se pudo obtener la hora UTC para el nombre del CSV.");
        return false;
    }
    const struct tm utc = *utc_pointer;
    char timestamp[24];
    if (strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &utc) == 0)
    {
        snprintf(error, error_size, "No se pudo formatear la hora UTC.");
        return false;
    }

    char filename[64];
    FILE *file = NULL;
    for (unsigned suffix = 0; suffix < 1000; ++suffix)
    {
        if (snprintf(filename, sizeof(filename),
                     "historial_simulado_%s_%03u.csv",
                     timestamp, suffix) >= (int)sizeof(filename))
        {
            snprintf(error, error_size, "El nombre del CSV excede el limite.");
            return false;
        }
        file = fopen(filename, "wx");
        if (file != NULL)
        {
            break;
        }
        if (errno != EEXIST)
        {
            snprintf(error, error_size, "No se pudo crear el CSV: %s", strerror(errno));
            return false;
        }
    }
    if (file == NULL)
    {
        snprintf(error, error_size, "No se encontro un nombre CSV libre.");
        return false;
    }

    battery_csv_sample_t samples[HISTORY_CAPACITY];
    for (size_t i = 0; i < history->count; ++i)
    {
        samples[i].timestamp_unix_s = (int64_t)history->samples[i].timestamp;
        samples[i].data = history->samples[i].data;
    }

    bool success = battery_csv_write(file, samples, history->count);
    if (!success)
    {
        snprintf(error, error_size,
                 "No se pudo escribir el historial; se conserva el CSV parcial: %s",
                 filename);
    }
    if (fflush(file) != 0)
    {
        snprintf(error, error_size,
                 "Error al vaciar el CSV; archivo conservado: %s", filename);
        success = false;
    }
    if (fclose(file) != 0)
    {
        snprintf(error, error_size,
                 "Error al cerrar el CSV; archivo conservado: %s", filename);
        success = false;
    }
    if (success)
    {
        snprintf(error, error_size, "CSV exportado: %s", filename);
    }
    return success;
}

static uint8_t draw_header(int screen_width, int content_x, uint8_t pack_id,
                           const char *status,
                           bool status_error, bool *export_clicked)
{
    draw_text_bold("Resumen del sistema", content_x, 20, 26, COLOR_TEXT);
    draw_text("Monitor de bateria LiFePO4",
              content_x, 53, 13, COLOR_MUTED);

    const float control_y = 28.0f;
    const float button_h = 36.0f;
    const float export_w = 118.0f;
    const float pack_w = 76.0f;
    const float gap = 8.0f;
    const float start_x = (float)screen_width - 28.0f -
                          export_w - 2.0f * (pack_w + gap) - 2.0f * gap;
    draw_text("PACK", (int)start_x - 43, (int)control_y + 11, 11, COLOR_MUTED);

    const bool select_100 = button(
        (Rectangle){start_x, control_y, pack_w, button_h},
        "100 Ah", pack_id == 1, COLOR_CYAN);
    const bool select_335 = button(
        (Rectangle){start_x + pack_w + gap, control_y, pack_w, button_h},
        "335 Ah", pack_id == 2, COLOR_CYAN);
    *export_clicked = button(
        (Rectangle){start_x + 2.0f * (pack_w + gap) + gap,
                    control_y, export_w, button_h},
        "Exportar CSV", false, COLOR_CYAN);

    const Rectangle demo_badge = {(float)content_x, 78.0f, 304.0f, 20.0f};
    DrawRectangleRec(demo_badge, (Color){255, 246, 229, 255});
    DrawRectangleLinesEx(demo_badge, 1.0f, (Color){239, 218, 180, 255});
    draw_text_bold("DEMO  /  SIMULADOR  /  BMS NO CONECTADO",
                   content_x + 9, 82, 10, COLOR_AMBER);
    const Color status_color = status_error ? COLOR_RED : COLOR_MUTED;
    draw_text_fit(status, content_x, 102, 11, status_color,
                  (float)screen_width - (float)content_x - 28.0f);
    if (select_100 && pack_id != 1)
        return 1;
    if (select_335 && pack_id != 2)
        return 2;
    return 0;
}

int main(void)
{
    const uint32_t pack_capacities_mAh[] = {100000, 335000};
    battery_simulator_t simulator;
    battery_data_t latest_data;
    battery_calculations_t calculations;
    history_t histories[2] = {0};
    history_t *history = &histories[0];
    uint8_t selected_pack = 1;
    double sample_timer = 0.0;
    char status[160] = "Simulacion activa; muestra cada segundo.";
    bool status_error = false;

    if (!battery_simulator_init(&simulator, selected_pack,
                                pack_capacities_mAh[selected_pack - 1]) ||
        !battery_simulator_next(&simulator, &latest_data) ||
        !battery_calculate(&latest_data, DO_D_PCT, &calculations))
    {
        fprintf(stderr, "No se pudo iniciar el simulador de bateria.\n");
        return 1;
    }
    if (!append_sample(history, &latest_data))
    {
        fprintf(stderr, "No se pudo registrar la muestra inicial del simulador.\n");
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1360, 860, "DAZI Solar | Monitor LiFePO4");
    SetWindowMinSize(1120, 740);
    SetTargetFPS(60);

    Texture2D logo = {0};
    bool logo_loaded = false;
    if (!load_ui_fonts())
    {
        fprintf(stderr, "No se pudo cargar Segoe UI ni Arial; se usara la fuente integrada de raylib.\n");
    }
    if (FileExists("assets/dazi-solar-logo.png"))
    {
        logo = LoadTexture("assets/dazi-solar-logo.png");
        logo_loaded = logo.id != 0;
    }
    if (!logo_loaded)
    {
        fprintf(stderr, "No se pudo cargar assets/dazi-solar-logo.png; se usara texto alternativo.\n");
    }

    while (!WindowShouldClose())
    {
        const float frame_time = GetFrameTime();
        sample_timer += frame_time;

        if (sample_timer >= 1.0)
        {
            const double elapsed_ms = sample_timer * 1000.0;
            uint32_t elapsed_milliseconds =
                elapsed_ms >= (double)UINT32_MAX
                    ? UINT32_MAX
                    : (uint32_t)elapsed_ms;
            sample_timer -= (double)elapsed_milliseconds / 1000.0;
            if (!battery_simulator_step(&simulator, elapsed_milliseconds,
                                        &latest_data) ||
                !battery_calculate(&latest_data, DO_D_PCT, &calculations))
            {
                snprintf(status, sizeof(status), "Error: no se pudo obtener una muestra simulada.");
                status_error = true;
            }
            else
            {
                if (!append_sample(history, &latest_data))
                {
                    snprintf(status, sizeof(status),
                             "Error: no se pudo registrar la muestra en el historial.");
                    status_error = true;
                }
                else
                {
                    snprintf(status, sizeof(status),
                             "Simulacion activa; muestra cada segundo.");
                    status_error = false;
                }
            }
        }

        const int screen_width = GetScreenWidth();
        const int screen_height = GetScreenHeight();
        const float sidebar_width = 220.0f;
        const float margin = sidebar_width + 28.0f;
        const float gap = 14.0f;
        const float content_width = (float)screen_width - margin - 28.0f;
        const float top_y = 128.0f;
        const float top_h = 128.0f;
        const float top_w = (content_width - 2.0f * gap) / 3.0f;
        const float metrics_y = top_y + top_h + 12.0f;
        const float metrics_h = 92.0f;
        const float metric_w = (content_width - 3.0f * gap) / 4.0f;
        const float panels_y = metrics_y + metrics_h + 14.0f;
        float panels_h = (float)screen_height - 532.0f;
        if (panels_h > 220.0f)
            panels_h = 220.0f;
        if (panels_h < 168.0f)
            panels_h = 168.0f;
        const float cells_w = content_width * 0.59f;
        const float chart_y = panels_y + panels_h + 14.0f;
        const float chart_h = (float)screen_height - chart_y - 24.0f;

        BeginDrawing();
        ClearBackground(COLOR_BACKGROUND);
        draw_sidebar(screen_height, logo, logo_loaded);
        bool export_clicked = false;
        const uint8_t requested_pack = draw_header(screen_width, (int)margin,
                                                   selected_pack, status,
                                                   status_error, &export_clicked);

        char voltage[40];
        char current[40];
        char power[40];
        char cycles[40];
        if (battery_has_field(&latest_data, BATTERY_FIELD_VOLTAGE))
        {
            snprintf(voltage, sizeof(voltage), "%.3f V",
                     (double)latest_data.voltage_mV / 1000.0);
        }
        else
        {
            snprintf(voltage, sizeof(voltage), "No disponible");
        }
        if (battery_has_field(&latest_data, BATTERY_FIELD_CURRENT))
        {
            snprintf(current, sizeof(current), "%+.2f A",
                     (double)latest_data.current_mA / 1000.0);
        }
        else
        {
            snprintf(current, sizeof(current), "No disponible");
        }
        if (calculations.power_available)
        {
            if (latest_data.current_mA == 0)
            {
                snprintf(power, sizeof(power), "0 W");
            }
            else
            {
                const double signed_power_W =
                    latest_data.current_mA < 0 ? -calculations.power_W
                                               : calculations.power_W;
                snprintf(power, sizeof(power), "%+.0f W", signed_power_W);
            }
        }
        else
        {
            snprintf(power, sizeof(power), "No disponible");
        }
        if (battery_has_field(&latest_data, BATTERY_FIELD_CYCLES))
        {
            snprintf(cycles, sizeof(cycles), "%u", (unsigned)latest_data.cycles);
        }
        else
        {
            snprintf(cycles, sizeof(cycles), "--");
        }

        draw_energy_card((Rectangle){margin, top_y, top_w, top_h}, &calculations);
        draw_soc_card((Rectangle){margin + top_w + gap, top_y, top_w, top_h},
                      &latest_data);
        draw_runtime_card((Rectangle){margin + 2.0f * (top_w + gap),
                                      top_y, top_w, top_h},
                          &calculations);

        draw_metric_card((Rectangle){margin, metrics_y, metric_w, metrics_h},
                         "TENSION", voltage, "PACK_V simulado", COLOR_TEXT);
        draw_metric_card((Rectangle){margin + metric_w + gap, metrics_y,
                                     metric_w, metrics_h},
                         "CORRIENTE MEDIA", current,
                         !battery_has_field(&latest_data, BATTERY_FIELD_CURRENT)
                             ? "Desconocida"
                             : "Media del intervalo",
                         !battery_has_field(&latest_data, BATTERY_FIELD_CURRENT)
                             ? COLOR_MUTED
                         : latest_data.current_mA > 0
                             ? COLOR_GREEN
                         : latest_data.current_mA < 0
                             ? COLOR_CYAN
                             : COLOR_TEXT);
        draw_metric_card((Rectangle){margin + 2.0f * (metric_w + gap), metrics_y,
                                     metric_w, metrics_h},
                         "POTENCIA ESTIMADA", power,
                         "V x |I|; signo segun corriente",
                         latest_data.current_mA > 0   ? COLOR_GREEN
                         : latest_data.current_mA < 0 ? COLOR_CYAN
                                                      : COLOR_TEXT);
        draw_metric_card((Rectangle){margin + 3.0f * (metric_w + gap), metrics_y,
                                     metric_w, metrics_h},
                         "CICLOS", cycles, "Dato simulado", COLOR_TEXT);

        draw_cells_panel((Rectangle){margin, panels_y, cells_w, panels_h},
                         &latest_data);
        draw_conditions_panel((Rectangle){margin + cells_w + gap, panels_y,
                                          content_width - cells_w - gap, panels_h},
                              &latest_data);
        draw_power_chart((Rectangle){margin, chart_y, content_width, chart_h},
                         history);

        if (requested_pack != 0)
        {
            selected_pack = requested_pack;
            history = &histories[selected_pack - 1];
            if (!battery_simulator_init(&simulator, selected_pack,
                                        pack_capacities_mAh[selected_pack - 1]) ||
                !battery_simulator_next(&simulator, &latest_data) ||
                !battery_calculate(&latest_data, DO_D_PCT, &calculations))
            {
                snprintf(status, sizeof(status), "Error al cambiar pack de simulacion.");
                status_error = true;
            }
            else
            {
                if (!append_sample(history, &latest_data))
                {
                    snprintf(status, sizeof(status),
                             "Error: no se pudo registrar la muestra del nuevo pack.");
                    status_error = true;
                }
                else
                {
                    snprintf(status, sizeof(status),
                             "Historial independiente por perfil. Modo simulacion.");
                    status_error = false;
                }
            }
        }

        if (export_clicked)
        {
            status_error = !export_history_csv(history, status, sizeof(status));
        }

        EndDrawing();
    }

    if (logo_loaded)
    {
        UnloadTexture(logo);
    }
    if (UI_FONT_LOADED)
    {
        UnloadFont(UI_FONT);
    }
    if (UI_FONT_BOLD_LOADED)
    {
        UnloadFont(UI_FONT_BOLD);
    }
    CloseWindow();
    return 0;
}
