#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "HITO1_BASICO";

// Asignación de pines según el conexionado físico de la protoboard
#define LED_PIN              GPIO_NUM_18
#define SWITCH_PIN           GPIO_NUM_19

// Lógica de activación: 0 = Conectado a GND (Pull-up interno activo)
#define SWITCH_ACTIVE_LEVEL  0

// Frecuencias base del Hito 1: 1 Hz y 2 Hz (f = 1/T_ms)
#define PERIOD_INACTIVE_MS   1000  // Periodo con switch abierto (1000 ms -> 1.0 Hz)
#define PERIOD_ACTIVE_MS     500   // Periodo con switch cerrado (500 ms -> 2.0 Hz)

// Cálculo automático de semiperiodos para Duty Cycle del 50%
#define HALF_PERIOD_INACTIVE_MS  (PERIOD_INACTIVE_MS / 2)
#define HALF_PERIOD_ACTIVE_MS    (PERIOD_ACTIVE_MS / 2)

static void configure_gpio(void)
{
    // =========================================================================
    // 1. CONFIGURACIÓN DEL LED (ACTUADOR DIGITAL - SALIDA PUSH-PULL)
    // =========================================================================
    gpio_config_t io_conf_led = {
        .pin_bit_mask = (1ULL << LED_PIN),      // Selecciona el pin 18 desplazando un 1 binario sin signo de 64 bits (ULL) a la posición 18
        .mode = GPIO_MODE_OUTPUT,               // Configura el pin como salida digital para poder inyectar 3.3V (ON) o 0V (OFF)
        .pull_up_en = GPIO_PULLUP_DISABLE,      // Desactiva la resistencia interna de Pull-up a 3.3V (no necesaria en una salida)
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // Desactiva la resistencia interna de Pull-down a GND (el circuito usa resistencia externa de 220 Ohm)
        .intr_type = GPIO_INTR_DISABLE          // Inhabilita las interrupciones hardware (el LED solo recibe órdenes, no genera eventos)
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf_led)); // Escribe la estructura en los registros del chip y aborta si la función no devuelve ESP_OK

    // =========================================================================
    // 2. CONFIGURACIÓN DEL SWITCH (SENSOR MECÁNICO - ENTRADA DIGITAL CON PULL-UP)
    // =========================================================================
    gpio_config_t io_conf_switch = {
        .pin_bit_mask = (1ULL << SWITCH_PIN),   // Selecciona el pin 19 desplazando el bit a la posición 19
        .mode = GPIO_MODE_INPUT,                // Configura el pin como entrada de alta impedancia para medir el nivel de tensión externo
        .pull_up_en = GPIO_PULLUP_ENABLE,       // ACTIVA el resistor interno de Pull-Up (~45 kOhm a 3.3V): evita que el pin quede "flotante" al abrir el switch
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // Desactiva el Pull-down para no crear un divisor resistivo que distorsione la lectura
        .intr_type = GPIO_INTR_DISABLE          // Desactiva interrupciones en la versión básica (el estado se comprueba por sondeo periódico/polling)
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf_switch)); // Escribe la configuración del switch en el hardware y comprueba que no haya errores

    // =========================================================================
    // 3. ESTADO INICIAL SEGURO Y TELEMETRÍA DE ARRANQUE
    // =========================================================================
    // Fuerza nivel bajo (0V) en el pin del LED para garantizar que el sistema arranca con el LED apagado
    ESP_ERROR_CHECK(gpio_set_level(LED_PIN, 0));

    // Envía mensaje de confirmación por el puerto serie (UART) para registrar la correcta inicialización
    ESP_LOGI(TAG, "Hardware inicializado (LED: GPIO%d, SW: GPIO%d).", LED_PIN, SWITCH_PIN);
}

void app_main(void)
{
    configure_gpio();

    uint8_t led_state = 0;
    int previous_switch_level = -1;

    while (1) {
        int switch_level = gpio_get_level(SWITCH_PIN);
        bool is_active = (switch_level == SWITCH_ACTIVE_LEVEL);

        // 1. Selección de parámetros temporales según el switch
        uint32_t total_period_ms = is_active ? PERIOD_ACTIVE_MS : PERIOD_INACTIVE_MS;

        // -------------------------------------------------------------------------
        // half_period_ms: Semiperiodo de la señal (Duty Cycle 50%).
        //                Define el tiempo exacto que el LED permanece ENCENDIDO 
        //                y el tiempo exacto que permanece APAGADO:
        //                - Switch inactivo (1 Hz / Periodo 1000 ms) -> 500 ms ON / 500 ms OFF
        //                - Switch activo   (2 Hz / Periodo  500 ms) -> 250 ms ON / 250 ms OFF
        // -------------------------------------------------------------------------
        uint32_t half_period_ms  = is_active ? HALF_PERIOD_ACTIVE_MS : HALF_PERIOD_INACTIVE_MS;

        // 2. Cálculo matemático de la frecuencia real
        float frequency_hz = 1000.0f / (float)total_period_ms;

        // 3. Registro UART exclusivamente ante cambios físicos
        if (switch_level != previous_switch_level) {
            ESP_LOGI(TAG, "Switch %s -> Frecuencia: %.1f Hz (Periodo: %lu ms)",
                     is_active ? "ACTIVADO" : "DESACTIVADO",
                     frequency_hz,
                     total_period_ms);
            previous_switch_level = switch_level;
        }

        // 4. Conmutación del actuador y retardo del semiperiodo
        led_state = !led_state;
        ESP_ERROR_CHECK(gpio_set_level(LED_PIN, led_state));
        vTaskDelay(pdMS_TO_TICKS(half_period_ms));
    }
}