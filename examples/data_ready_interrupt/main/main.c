/**
 * @file main.c
 * @brief Uses the BMP585's physical interrupt pin to know when a new
 *        pressure/temperature reading is ready, instead of polling the
 *        interrupt status register
 *
 * Connect the breakout board to the I2C pins of your board, or use a Qwiic
 * cable, and wire the sensor's INT pin to PIN_NUM_INT.
 *
 * Product used is www.solde.red/333189
 *
 * @author Soldered Electronics
 */

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "soldered_bmp585.h"

static const char *TAG = "BMP585_DRDY_INT";

// Change these to match how your breakout is wired
#define PIN_NUM_SDA GPIO_NUM_8
#define PIN_NUM_SCL GPIO_NUM_9
#define PIN_NUM_INT GPIO_NUM_4

static QueueHandle_t data_ready_queue;

static void IRAM_ATTR data_ready_isr(void *arg)
{
    // Only ISR-safe calls are allowed here, the actual I2C read happens in app_main()
    xQueueSendFromISR(data_ready_queue, &arg, NULL);
}

void app_main(void)
{
    bmp585_t sensor;

    // The I2C bus belongs to the application, not to the driver, so that other
    // Qwiic devices can share it. Create it first, then hand it to the driver.
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_NUM_SDA,
        .scl_io_num = PIN_NUM_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    // Start the sensor on its default I2C address (0x47)
    esp_err_t err = soldered_bmp585_init(&sensor, bus, BMP5_I2C_ADDR_SEC);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BMP585 initialization failed: %s (%s)", esp_err_to_name(err),
                 soldered_bmp585_status_string(&sensor));
        return;
    }

    // Configure the interrupt pin as push/pull, active high, pulsed
    soldered_bmp585_configure_interrupt(&sensor, BMP5_PULSED, BMP5_ACTIVE_HIGH, BMP5_INTR_PUSH_PULL, true);

    // Only assert the pin on a new data-ready reading
    soldered_bmp585_set_interrupt_source(&sensor, true, false, false, false);

    // Set up the GPIO the sensor's INT pin is wired to
    data_ready_queue = xQueueCreate(1, sizeof(void *));

    gpio_config_t int_pin_cfg = {
        .pin_bit_mask = 1ULL << PIN_NUM_INT,
                             .mode = GPIO_MODE_INPUT,
                             .pull_down_en = GPIO_PULLDOWN_ENABLE,
                             .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&int_pin_cfg);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_NUM_INT, data_ready_isr, NULL);

    void *ignored;
    while (1) {
        // Block until the ISR signals a new reading
        xQueueReceive(data_ready_queue, &ignored, portMAX_DELAY);

        if (soldered_bmp585_get_sensor_data(&sensor) == BMP5_OK) {
            ESP_LOGI(TAG, "Pressure: %.2f Pa, Temperature: %.2f degC", sensor.data.pressure, sensor.data.temperature);
        } else {
            ESP_LOGE(TAG, "Failed to read sensor data: %s", soldered_bmp585_status_string(&sensor));
        }
    }
}
