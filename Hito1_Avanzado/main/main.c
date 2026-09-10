#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "HITO1_AVANZADO";

// =============================================================================
// 1. ASIGNACIÓN DE HARDWARE Y POLARIDAD (BOM)
// =============================================================================
#define LED_PIN                  GPIO_NUM_18
#define SWITCH_PIN               GPIO_NUM_19
#define SWITCH_ACTIVE_LEVEL      0   // Lógica activa baja (0 = Conectado a GND)
#define ESP_INTR_FLAG_DEFAULT    0   // Asignación de interrupción estándar (Nivel 1-3)

// =============================================================================
// 2. PARÁMETROS TEMPORALES Y FRECUENCIA (f = 1000 / T_ms)
// =============================================================================
#define PERIOD_INACTIVE_MS       1000  // Switch abierto: 1000 ms -> 1.0 Hz
#define PERIOD_ACTIVE_MS         500   // Switch cerrado:  500 ms -> 2.0 Hz

#define HALF_PERIOD_INACTIVE_MS  (PERIOD_INACTIVE_MS / 2)  // 500 ms ON / 500 ms OFF
#define HALF_PERIOD_ACTIVE_MS    (PERIOD_ACTIVE_MS / 2)    // 250 ms ON / 250 ms OFF

#define DEBOUNCE_DELAY_MS        50    // Filtro anti-rebote mecánico por software

// =============================================================================
// 3. DIMENSIONAMIENTO DE FREERTOS (COLAS Y TAREAS)
// =============================================================================
#define GPIO_QUEUE_LEN           10    // Profundidad de la cola de eventos GPIO
#define SWITCH_TASK_STACK_SIZE   2048  // Tamaño de pila para la tarea del switch (bytes)
#define SWITCH_TASK_PRIORITY     10    // Alta prioridad: reacción inmediata a eventos
#define LED_TASK_STACK_SIZE      2048  // Tamaño de pila para la tarea del actuador (bytes)
#define LED_TASK_PRIORITY        5     // Prioridad media/baja: parpadeo cíclico de fondo

// Variable global compartida para la temporización (lectura/escritura atómica de 32 bits)
static volatile uint32_t s_half_period_ms = HALF_PERIOD_INACTIVE_MS;
static QueueHandle_t s_gpio_evt_queue = NULL;

// =============================================================================
// RUTINA DE SERVICIO DE INTERRUPCIÓN (ISR en IRAM)
// =============================================================================
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Encola el pin disparador sin bloquear
    xQueueSendFromISR(s_gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken);

    // Si una tarea de mayor prioridad despertó, fuerza un cambio de contexto inmediato
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// =============================================================================
// TAREA CONSUMIDORA: GESTIÓN DE EVENTOS DEL CONMUTADOR
// =============================================================================
static void switch_task(void *arg)
{
    uint32_t io_num;
    int last_level = -1;

    while (1) {
        // Bloqueo sin consumo de CPU hasta recibir notificación desde la ISR
        if (xQueueReceive(s_gpio_evt_queue, &io_num, portMAX_DELAY)) {
            // Retardo para amortiguar los rebotes eléctricos de los contactos
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY_MS));

            int current_level = gpio_get_level(io_num);
            if (current_level != last_level) {
                last_level = current_level;
                bool is_active = (current_level == SWITCH_ACTIVE_LEVEL);

                // Cálculo dinámico de tiempos y frecuencia física real
                uint32_t total_period_ms = is_active ? PERIOD_ACTIVE_MS : PERIOD_INACTIVE_MS;
                s_half_period_ms = is_active ? HALF_PERIOD_ACTIVE_MS : HALF_PERIOD_INACTIVE_MS;
                float frequency_hz = 1000.0f / (float)total_period_ms;

                ESP_LOGI(TAG, "ISR Event en GPIO %lu -> Switch %s | Frecuencia: %.1f Hz (Periodo: %lu ms)",
                         io_num,
                         is_active ? "ACTIVADO" : "DESACTIVADO",
                         frequency_hz,
                         total_period_ms);
            }
        }
    }
}

// =============================================================================
// TAREA ACTUADORA: CONTROL DEL DIODO LED
// =============================================================================
static void led_task(void *arg)
{
    uint8_t led_state = 0;
    while (1) {
        led_state = !led_state;
        gpio_set_level(LED_PIN, led_state);
        vTaskDelay(pdMS_TO_TICKS(s_half_period_ms));
    }
}

// =============================================================================
// CONFIGURACIÓN DE PERIFÉRICOS E INTERRUPCIONES
// =============================================================================
static void configure_peripherals(void)
{
    // 1. Configuración de salida del LED (Sin resistencias internas de pull)
    gpio_config_t io_conf_led = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf_led));

    // 2. Configuración del Switch (Entrada con Pull-Up e interrupción en ambos flancos)
    gpio_config_t io_conf_switch = {
        .pin_bit_mask = (1ULL << SWITCH_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf_switch));

    // 3. Creación de la cola para eventos de interrupción
    s_gpio_evt_queue = xQueueCreate(GPIO_QUEUE_LEN, sizeof(uint32_t));

    // 4. Instalación del servicio del despachador de interrupciones
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));
    ESP_ERROR_CHECK(gpio_isr_handler_add(SWITCH_PIN, gpio_isr_handler, (void *) SWITCH_PIN));

    // 5. Sincronización del estado físico de arranque antes de lanzar el scheduler
    int initial_level = gpio_get_level(SWITCH_PIN);
    s_half_period_ms = (initial_level == SWITCH_ACTIVE_LEVEL) ? HALF_PERIOD_ACTIVE_MS : HALF_PERIOD_INACTIVE_MS;

    ESP_LOGI(TAG, "Periféricos e interrupciones inicializados con éxito.");
}

// =============================================================================
// PUNTO DE ENTRADA PRINCIPAL
// =============================================================================
void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando Hito 1 - Variante Avanzada (ISR + FreeRTOS)...");
    configure_peripherals();

    // Creación modular de tareas con parámetros declarados
    xTaskCreate(switch_task, "switch_task", SWITCH_TASK_STACK_SIZE, NULL, SWITCH_TASK_PRIORITY, NULL);
    xTaskCreate(led_task, "led_task", LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIORITY, NULL);
}