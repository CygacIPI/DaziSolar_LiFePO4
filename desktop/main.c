#include "battery.h"
#include "raylib.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define HISTORY_CAPACITY 60
#define DO_D_PCT 80

static const Color COLOR_BACKGROUND = { 15, 22, 32, 255 };
static const Color COLOR_PANEL = { 25, 35, 48, 255 };
static const Color COLOR_PANEL_LIGHT = { 33, 46, 61, 255 };
static const Color COLOR_BORDER = { 48, 64, 82, 255 };
static const Color COLOR_TEXT = { 237, 243, 249, 255 };
static const Color COLOR_MUTED = { 151, 168, 184, 255 };
static const Color COLOR_CYAN = { 72, 203, 220, 255 };
static const Color COLOR_GREEN = { 85, 207, 145, 255 };
static const Color COLOR_AMBER = { 245, 185, 75, 255 };
static const Color COLOR_RED = { 239, 103, 103, 255 };

typedef struct {
    battery_data_t data;
    time_t timestamp;
} history_sample_t;

typedef struct {
    history_sample_t samples[HISTORY_CAPACITY];
    size_t count;
} history_t;

static void draw_text(const char *text, int x, int y, int size, Color color)
{
    DrawText(text, x, y, size, color);
}

static void draw_panel(Rectangle bounds)
{
    DrawRectangleRounded(bounds, 0.12f, 8, COLOR_PANEL);
    DrawRectangleRoundedLinesEx(bounds, 0.12f, 8, 1.0f, COLOR_BORDER);
}

static bool button(Rectangle bounds, const char *label, bool selected, Color accent)
{
    const Vector2 mouse = GetMousePosition();
    const bool hovered = CheckCollisionPointRec(mouse, bounds);
    const bool pressed = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const Color fill = selected ? accent : (hovered ? COLOR_PANEL_LIGHT : COLOR_PANEL);
    const Color text = selected ? COLOR_BACKGROUND : COLOR_TEXT;

    DrawRectangleRounded(bounds, 0.25f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.25f, 8, selected ? accent : COLOR_BORDER);
    const int label_width = MeasureText(label, 15);
    draw_text(label, (int)(bounds.x + (bounds.width - label_width) / 2),
              (int)(bounds.y + (bounds.height - 15) / 2), 15, text);
    return pressed;
}

static void draw_metric_card(Rectangle bounds, const char *title,
                             const char *value, const char *detail,
                             Color accent)
{
    draw_panel(bounds);
    draw_text(title, (int)bounds.x + 16, (int)bounds.y + 13, 14, COLOR_MUTED);
    draw_text(value, (int)bounds.x + 16, (int)bounds.y + 35, 25, accent);
    if (detail != NULL) {
        draw_text(detail, (int)bounds.x + 16, (int)bounds.y + 68, 12, COLOR_MUTED);
    }
}

static void draw_energy_card(Rectangle bounds,
                             const battery_calculations_t *calculations)
{
    char energy[32];
    char detail[96];
    snprintf(energy, sizeof(energy), "%.2f kWh",
             calculations->stored_energy_Wh / 1000.0);
    snprintf(detail, sizeof(detail), "Nominal %.2f kWh  |  util (DoD %d%%) %.2f kWh",
             calculations->nominal_energy_Wh / 1000.0, DO_D_PCT,
             calculations->usable_nominal_energy_Wh / 1000.0);

    draw_panel(bounds);
    draw_text("ENERGIA ALMACENADA", (int)bounds.x + 18, (int)bounds.y + 14,
              14, COLOR_MUTED);
    draw_text(energy, (int)bounds.x + 18, (int)bounds.y + 37, 33, COLOR_CYAN);
    draw_text(detail, (int)bounds.x + 18, (int)bounds.y + 91, 12, COLOR_MUTED);
}

static void draw_soc_card(Rectangle bounds, const battery_data_t *data)
{
    char percent[16];
    char capacity[64];
    snprintf(percent, sizeof(percent), "%u%%", data->soc_pct);
    snprintf(capacity, sizeof(capacity), "%.2f Ah restantes de %.0f Ah",
             (double)data->remaining_mAh / 1000.0,
             (double)data->nominal_mAh / 1000.0);

    draw_panel(bounds);
    draw_text("ESTADO DE CARGA", (int)bounds.x + 18, (int)bounds.y + 14,
              14, COLOR_MUTED);
    draw_text(percent, (int)bounds.x + 18, (int)bounds.y + 36, 33, COLOR_GREEN);
    const Rectangle bar = { bounds.x + 18, bounds.y + 83, bounds.width - 36, 13 };
    DrawRectangleRounded(bar, 0.5f, 8, COLOR_PANEL_LIGHT);
    Rectangle fill = bar;
    fill.width *= (float)data->soc_pct / 100.0f;
    if (fill.width > 0) {
        DrawRectangleRounded(fill, 0.5f, 8, COLOR_GREEN);
    }
    draw_text(capacity, (int)bounds.x + 18, (int)bounds.y + 103, 12, COLOR_MUTED);
}

static void draw_runtime_card(Rectangle bounds,
                              const battery_calculations_t *calculations)
{
    char runtime[32];
    char detail[64];
    if (calculations->runtime_available) {
        snprintf(runtime, sizeof(runtime), "%.1f h", calculations->runtime_h);
        snprintf(detail, sizeof(detail), "Estimacion a %.0f W de descarga",
                 calculations->power_W);
    } else {
        snprintf(runtime, sizeof(runtime), "--");
        snprintf(detail, sizeof(detail), "Disponible durante descarga");
    }

    draw_panel(bounds);
    draw_text("AUTONOMIA ESTIMADA", (int)bounds.x + 18, (int)bounds.y + 14,
              14, COLOR_MUTED);
    draw_text(runtime, (int)bounds.x + 18, (int)bounds.y + 38, 33, COLOR_AMBER);
    draw_text(detail, (int)bounds.x + 18, (int)bounds.y + 91, 12, COLOR_MUTED);
}

static void draw_cells_panel(Rectangle bounds, const battery_data_t *data)
{
    draw_panel(bounds);
    draw_text("TENSION POR CELDA", (int)bounds.x + 18, (int)bounds.y + 14,
              14, COLOR_MUTED);

    uint16_t minimum = UINT16_MAX;
    uint16_t maximum = 0;
    for (uint8_t i = 0; i < data->cell_count; ++i) {
        if (data->cell_mV[i] < minimum) minimum = data->cell_mV[i];
        if (data->cell_mV[i] > maximum) maximum = data->cell_mV[i];
    }
    char delta[48];
    snprintf(delta, sizeof(delta), "Diferencia max.: %u mV",
             data->cell_count > 0 ? (unsigned)(maximum - minimum) : 0u);
    const int delta_width = MeasureText(delta, 12);
    draw_text(delta, (int)(bounds.x + bounds.width - delta_width - 18),
              (int)bounds.y + 16, 12, COLOR_CYAN);

    if (data->cell_count == 0) {
        draw_text("Sin datos de celdas", (int)bounds.x + 18,
                  (int)bounds.y + 64, 16, COLOR_MUTED);
        return;
    }

    const int columns = data->cell_count > 8 ? 8 : data->cell_count;
    const float gap = 8.0f;
    const float cell_width = (bounds.width - 36.0f - gap * (columns - 1)) / columns;
    const float cell_height = 52.0f;
    const float start_y = bounds.y + 48.0f;

    for (uint8_t i = 0; i < data->cell_count; ++i) {
        const int row = i / columns;
        const int column = i % columns;
        const Rectangle cell = {
            bounds.x + 18.0f + column * (cell_width + gap),
            start_y + row * (cell_height + 9.0f),
            cell_width,
            cell_height
        };
        char value[24];
        char label[12];
        snprintf(value, sizeof(value), "%.3f V",
                 (double)data->cell_mV[i] / 1000.0);
        snprintf(label, sizeof(label), "CEL %u", (unsigned)(i + 1));
        DrawRectangleRounded(cell, 0.15f, 6, COLOR_PANEL_LIGHT);
        draw_text(label, (int)cell.x + 7, (int)cell.y + 7, 10, COLOR_MUTED);
        draw_text(value, (int)cell.x + 7, (int)cell.y + 26, 13, COLOR_TEXT);
    }
}

static const char *alarm_label(uint32_t alarm)
{
    switch (alarm) {
    case BATTERY_ALARM_VOLTAGE_LOW: return "Tension de pack baja";
    case BATTERY_ALARM_VOLTAGE_HIGH: return "Tension de pack alta";
    case BATTERY_ALARM_CHARGE_VOLTAGE_HIGH: return "Tension de carga alta";
    case BATTERY_ALARM_CHARGE_TEMPERATURE: return "Temperatura fuera de carga";
    case BATTERY_ALARM_DISCHARGE_TEMPERATURE: return "Temperatura fuera de descarga";
    case BATTERY_ALARM_CHARGE_CURRENT_HIGH: return "Corriente de carga alta";
    case BATTERY_ALARM_DISCHARGE_CURRENT_HIGH: return "Corriente de descarga alta";
    default: return "Alarma desconocida";
    }
}

static void draw_conditions_panel(Rectangle bounds, const battery_data_t *data)
{
    draw_panel(bounds);
    draw_text("TEMPERATURAS Y ALARMAS", (int)bounds.x + 18,
              (int)bounds.y + 14, 14, COLOR_MUTED);

    char temperatures[64];
    if (data->temp_count == 0) {
        snprintf(temperatures, sizeof(temperatures), "Sin datos de temperatura");
    } else {
        int used = snprintf(temperatures, sizeof(temperatures), "Temperaturas: ");
        for (uint8_t i = 0; i < data->temp_count && used > 0 &&
                             (size_t)used < sizeof(temperatures); ++i) {
            const int written = snprintf(
                temperatures + used, sizeof(temperatures) - (size_t)used,
                "%s%.1f C", i == 0 ? "" : " / ",
                (double)data->temp_dC[i] / 10.0);
            if (written < 0) break;
            used += written;
        }
    }
    draw_text(temperatures, (int)bounds.x + 18, (int)bounds.y + 47,
              15, COLOR_TEXT);

    const int alarm_y = (int)bounds.y + 83;
    int alarm_row = 0;
    uint32_t known_alarms = 0;
    for (uint32_t bit = 1; bit <= BATTERY_ALARM_DISCHARGE_CURRENT_HIGH; bit <<= 1) {
        if ((data->alarms & bit) == 0) continue;
        known_alarms |= bit;
        draw_text(alarm_label(bit), (int)bounds.x + 18,
                  alarm_y + alarm_row * 20, 14, COLOR_RED);
        alarm_row++;
    }

    if (known_alarms == 0) {
        draw_text("Sin alarmas activas", (int)bounds.x + 18,
                  alarm_y, 15, COLOR_GREEN);
    }
}

static float sample_power_W(const history_sample_t *sample)
{
    const double voltage_V = (double)sample->data.voltage_mV / 1000.0;
    const double current_A = (double)sample->data.current_mA / 1000.0;
    return (float)(voltage_V * current_A);
}

static void draw_power_chart(Rectangle bounds, const history_t *history)
{
    draw_panel(bounds);
    draw_text("POTENCIA EN TIEMPO REAL", (int)bounds.x + 18,
              (int)bounds.y + 12, 14, COLOR_MUTED);
    draw_text("Carga +  /  Descarga -", (int)bounds.x + 18,
              (int)bounds.y + 33, 12, COLOR_MUTED);

    const Rectangle plot = {
        bounds.x + 65,
        bounds.y + 22,
        bounds.width - 88,
        bounds.height - 48
    };
    if (history->count < 2 || plot.width <= 0 || plot.height <= 0) {
        draw_text("Esperando muestras del simulador...",
                  (int)plot.x + 12, (int)plot.y + 20, 14, COLOR_MUTED);
        return;
    }

    float max_abs = 100.0f;
    for (size_t i = 0; i < history->count; ++i) {
        const float power = sample_power_W(&history->samples[i]);
        if (power < 0.0f && -power > max_abs) max_abs = -power;
        if (power > max_abs) max_abs = power;
    }
    max_abs *= 1.15f;

    const float zero_y = plot.y + plot.height / 2.0f;
    DrawLineEx((Vector2){ plot.x, plot.y }, (Vector2){ plot.x, plot.y + plot.height },
               1.0f, COLOR_BORDER);
    DrawLineEx((Vector2){ plot.x, zero_y }, (Vector2){ plot.x + plot.width, zero_y },
               1.0f, COLOR_BORDER);
    DrawLineEx((Vector2){ plot.x, plot.y + plot.height },
               (Vector2){ plot.x + plot.width, plot.y + plot.height },
               1.0f, COLOR_BORDER);
    draw_text("W", (int)bounds.x + 31, (int)plot.y + 2, 12, COLOR_MUTED);

    for (size_t i = 1; i < history->count; ++i) {
        const float previous_power = sample_power_W(&history->samples[i - 1]);
        const float current_power = sample_power_W(&history->samples[i]);
        const Vector2 previous = {
            plot.x + (float)(i - 1) * plot.width / (float)(history->count - 1),
            zero_y - previous_power * (plot.height / 2.0f) / max_abs
        };
        const Vector2 current = {
            plot.x + (float)i * plot.width / (float)(history->count - 1),
            zero_y - current_power * (plot.height / 2.0f) / max_abs
        };
        DrawLineEx(previous, current, 2.5f, COLOR_CYAN);
    }
}

static void append_sample(history_t *history, const battery_data_t *data)
{
    if (history->count == HISTORY_CAPACITY) {
        memmove(history->samples, history->samples + 1,
                (HISTORY_CAPACITY - 1) * sizeof(history->samples[0]));
        history->count--;
    }
    history->samples[history->count].data = *data;
    history->samples[history->count].timestamp = time(NULL);
    history->count++;
}

static bool export_history_csv(const history_t *history, char *error, size_t error_size)
{
    FILE *file = fopen("historial_simulado.csv", "w");
    if (file == NULL) {
        snprintf(error, error_size, "No se pudo abrir el CSV: %s", strerror(errno));
        return false;
    }

    if (fprintf(file,
                "timestamp,pack_id,voltage_mV,current_mA,soc_pct,"
                "remaining_mAh,nominal_mAh,cycles,alarms,stored_energy_Wh,"
                "cell_count") < 0) {
        snprintf(error, error_size, "No se pudo escribir la cabecera CSV");
        fclose(file);
        return false;
    }
    for (uint8_t i = 0; i < BATTERY_MAX_CELLS; ++i) {
        if (fprintf(file, ",cell_%u_mV", (unsigned)(i + 1)) < 0) {
            snprintf(error, error_size, "No se pudo escribir la cabecera CSV");
            fclose(file);
            return false;
        }
    }
    if (fprintf(file, ",temp_count") < 0) {
        snprintf(error, error_size, "No se pudo escribir la cabecera CSV");
        fclose(file);
        return false;
    }
    for (uint8_t i = 0; i < BATTERY_MAX_TEMPERATURES; ++i) {
        if (fprintf(file, ",temp_%u_dC", (unsigned)(i + 1)) < 0) {
            snprintf(error, error_size, "No se pudo escribir la cabecera CSV");
            fclose(file);
            return false;
        }
    }
    if (fputc('\n', file) == EOF) {
        snprintf(error, error_size, "No se pudo terminar la cabecera CSV");
        fclose(file);
        return false;
    }

    for (size_t i = 0; i < history->count; ++i) {
        const history_sample_t *sample = &history->samples[i];
        const battery_data_t *data = &sample->data;
        battery_calculations_t calculations;
        if (!battery_calculate(data, DO_D_PCT, &calculations)) {
            snprintf(error, error_size, "Muestra invalida; no se exporto el CSV");
            fclose(file);
            return false;
        }
        if (fprintf(file, "%lld,%u,%lu,%ld,%u,%lu,%lu,%u,%lu,%.3f,%u",
                    (long long)sample->timestamp,
                    (unsigned)data->pack_id,
                    (unsigned long)data->voltage_mV,
                    (long)data->current_mA,
                    (unsigned)data->soc_pct,
                    (unsigned long)data->remaining_mAh,
                    (unsigned long)data->nominal_mAh,
                    (unsigned)data->cycles,
                    (unsigned long)data->alarms,
                    calculations.stored_energy_Wh,
                    (unsigned)data->cell_count) < 0) {
            snprintf(error, error_size, "Error al escribir muestras en CSV");
            fclose(file);
            return false;
        }
        for (uint8_t cell = 0; cell < BATTERY_MAX_CELLS; ++cell) {
            const int result = cell < data->cell_count
                                   ? fprintf(file, ",%u", (unsigned)data->cell_mV[cell])
                                   : fprintf(file, ",");
            if (result < 0) {
                snprintf(error, error_size, "Error al escribir celdas en CSV");
                fclose(file);
                return false;
            }
        }
        if (fprintf(file, ",%u", (unsigned)data->temp_count) < 0) {
            snprintf(error, error_size, "Error al escribir temperaturas en CSV");
            fclose(file);
            return false;
        }
        for (uint8_t temp = 0; temp < BATTERY_MAX_TEMPERATURES; ++temp) {
            const int result = temp < data->temp_count
                                   ? fprintf(file, ",%d", (int)data->temp_dC[temp])
                                   : fprintf(file, ",");
            if (result < 0) {
                snprintf(error, error_size, "Error al escribir temperaturas en CSV");
                fclose(file);
                return false;
            }
        }
        if (fputc('\n', file) == EOF) {
            snprintf(error, error_size, "Error al terminar una fila CSV");
            fclose(file);
            return false;
        }
    }

    if (fclose(file) != 0) {
        snprintf(error, error_size, "Error al cerrar el CSV");
        return false;
    }
    snprintf(error, error_size, "CSV exportado: historial_simulado.csv (se reemplaza al exportar)");
    return true;
}

static uint8_t draw_header(int screen_width, uint8_t pack_id, const char *status,
                           bool status_error, bool *export_clicked)
{
    draw_text("App LiFePO4", 28, 22, 29, COLOR_TEXT);
    draw_text("Monitor de solo lectura  |  Fuente: SIMULADOR",
              29, 56, 13, COLOR_MUTED);

    const float control_y = 24.0f;
    const float button_h = 36.0f;
    const float export_w = 118.0f;
    const float pack_w = 76.0f;
    const float gap = 8.0f;
    const float start_x = (float)screen_width - 28.0f -
                          export_w - 2.0f * (pack_w + gap) - 2.0f * gap;
    draw_text("PACK", (int)start_x - 48, (int)control_y + 10, 12, COLOR_MUTED);

    const bool select_100 = button(
        (Rectangle){ start_x, control_y, pack_w, button_h },
        "100 Ah", pack_id == 1, COLOR_CYAN);
    const bool select_335 = button(
        (Rectangle){ start_x + pack_w + gap, control_y, pack_w, button_h },
        "335 Ah", pack_id == 2, COLOR_CYAN);
    *export_clicked = button(
        (Rectangle){ start_x + 2.0f * (pack_w + gap) + gap,
                     control_y, export_w, button_h },
        "Exportar CSV", false, COLOR_CYAN);

    const Color status_color = status_error ? COLOR_RED : COLOR_MUTED;
    draw_text(status, 29, 79, 12, status_color);
    if (select_100 && pack_id != 1) return 1;
    if (select_335 && pack_id != 2) return 2;
    return 0;
}

int main(void)
{
    const uint32_t pack_capacities_mAh[] = { 100000, 335000 };
    battery_simulator_t simulator;
    battery_data_t latest_data;
    battery_calculations_t calculations;
    history_t history = { 0 };
    uint8_t selected_pack = 1;
    double sample_timer = 0.0;
    char status[160] = "Muestras simuladas cada segundo. No hay conexion con el BMS.";
    bool status_error = false;

    if (!battery_simulator_init(&simulator, selected_pack,
                                pack_capacities_mAh[selected_pack - 1]) ||
        !battery_simulator_next(&simulator, &latest_data) ||
        !battery_calculate(&latest_data, DO_D_PCT, &calculations)) {
        fprintf(stderr, "No se pudo iniciar el simulador de bateria.\n");
        return 1;
    }
    append_sample(&history, &latest_data);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 820, "App LiFePO4 - Monitor simulado");
    SetWindowMinSize(1000, 700);
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        const float frame_time = GetFrameTime();
        sample_timer += frame_time;

        if (sample_timer >= 1.0) {
            sample_timer = 0.0;
            if (!battery_simulator_next(&simulator, &latest_data) ||
                !battery_calculate(&latest_data, DO_D_PCT, &calculations)) {
                snprintf(status, sizeof(status), "Error: no se pudo obtener una muestra simulada.");
                status_error = true;
            } else {
                append_sample(&history, &latest_data);
                snprintf(status, sizeof(status),
                         "Muestras simuladas cada segundo. No hay conexion con el BMS.");
                status_error = false;
            }
        }

        const int screen_width = GetScreenWidth();
        const int screen_height = GetScreenHeight();
        const float margin = 28.0f;
        const float gap = 14.0f;
        const float content_width = (float)screen_width - 2.0f * margin;
        const float top_y = 112.0f;
        const float top_h = 130.0f;
        const float top_w = (content_width - 2.0f * gap) / 3.0f;
        const float metrics_y = top_y + top_h + 12.0f;
        const float metrics_h = 92.0f;
        const float metric_w = (content_width - 3.0f * gap) / 4.0f;
        const float panels_y = metrics_y + metrics_h + 14.0f;
        float panels_h = (float)screen_height - 532.0f;
        if (panels_h > 220.0f) panels_h = 220.0f;
        if (panels_h < 168.0f) panels_h = 168.0f;
        const float cells_w = content_width * 0.59f;
        const float chart_y = panels_y + panels_h + 14.0f;
        const float chart_h = (float)screen_height - chart_y - 24.0f;

        BeginDrawing();
        ClearBackground(COLOR_BACKGROUND);
        bool export_clicked = false;
        const uint8_t requested_pack = draw_header(screen_width, selected_pack, status,
                                                   status_error, &export_clicked);

        char voltage[40];
        char current[40];
        char power[40];
        char cycles[40];
        snprintf(voltage, sizeof(voltage), "%.3f V",
                 (double)latest_data.voltage_mV / 1000.0);
        snprintf(current, sizeof(current), "%+.2f A",
                 (double)latest_data.current_mA / 1000.0);
        snprintf(power, sizeof(power), "%+.0f W",
                 (double)latest_data.voltage_mV / 1000.0 *
                 (double)latest_data.current_mA / 1000.0);
        snprintf(cycles, sizeof(cycles), "%u", (unsigned)latest_data.cycles);

        draw_energy_card((Rectangle){ margin, top_y, top_w, top_h }, &calculations);
        draw_soc_card((Rectangle){ margin + top_w + gap, top_y, top_w, top_h },
                      &latest_data);
        draw_runtime_card((Rectangle){ margin + 2.0f * (top_w + gap),
                                       top_y, top_w, top_h },
                          &calculations);

        draw_metric_card((Rectangle){ margin, metrics_y, metric_w, metrics_h },
                         "TENSION", voltage, "PACK_V simulado", COLOR_TEXT);
        draw_metric_card((Rectangle){ margin + metric_w + gap, metrics_y,
                                      metric_w, metrics_h },
                         "CORRIENTE", current,
                         latest_data.current_mA > 0 ? "Carga simulada" :
                         (latest_data.current_mA < 0 ? "Descarga simulada" : "En reposo"),
                         latest_data.current_mA > 0 ? COLOR_GREEN : COLOR_CYAN);
        draw_metric_card((Rectangle){ margin + 2.0f * (metric_w + gap), metrics_y,
                                      metric_w, metrics_h },
                         "POTENCIA", power, "V x I con signo", COLOR_AMBER);
        draw_metric_card((Rectangle){ margin + 3.0f * (metric_w + gap), metrics_y,
                                      metric_w, metrics_h },
                         "CICLOS", cycles, "Dato de ejemplo", COLOR_TEXT);

        draw_cells_panel((Rectangle){ margin, panels_y, cells_w, panels_h },
                         &latest_data);
        draw_conditions_panel((Rectangle){ margin + cells_w + gap, panels_y,
                                           content_width - cells_w - gap, panels_h },
                              &latest_data);
        draw_power_chart((Rectangle){ margin, chart_y, content_width, chart_h },
                         &history);

        if (requested_pack != 0) {
            selected_pack = requested_pack;
            history.count = 0;
            if (!battery_simulator_init(&simulator, selected_pack,
                                        pack_capacities_mAh[selected_pack - 1]) ||
                !battery_simulator_next(&simulator, &latest_data) ||
                !battery_calculate(&latest_data, DO_D_PCT, &calculations)) {
                snprintf(status, sizeof(status), "Error al cambiar pack de simulacion.");
                status_error = true;
            } else {
                append_sample(&history, &latest_data);
                status_error = false;
            }
        }

        if (export_clicked) {
            status_error = !export_history_csv(&history, status, sizeof(status));
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
