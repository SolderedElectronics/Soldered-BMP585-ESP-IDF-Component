/**
 * @file main.c
 * @brief Configures oversampling/IIR filtering and takes single-shot readings
 *        in forced power mode with the BMP585 sensor over I2C
 *
 * Forced mode triggers one measurement then returns the sensor to standby
 * automatically, useful for low-power applications where you only need
 * occasional readings.
 *
 * Product used is www.solde.red/333189
 *
 * @author Soldered Electronics
 */

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soldered_bmp585.h"

static const char *TAG = "BMP585_FORCED";

// Change these to match how your breakout is wired
#define PIN_NUM_SDA GPIO_NUM_8
#define PIN_NUM_SCL GPIO_NUM_9

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

    // Lower the oversampling from the driver's default since forced mode is
    // typically used for infrequent, quick readings. ODR is ignored in
    // forced mode, but still needs to be set to a valid value
    soldered_bmp585_set_osr_odr_config(&sensor, BMP5_OVERSAMPLING_1X, BMP5_OVERSAMPLING_1X, BMP5_ODR_50_HZ, true);

    // Bypass the IIR filter since forced mode readings aren't continuous
    // enough for the filter to settle
    soldered_bmp585_set_iir_config(&sensor, BMP5_IIR_FILTER_BYPASS, BMP5_IIR_FILTER_BYPASS);

    // Trigger the first measurement
    soldered_bmp585_set_mode(&sensor, BMP5_POWERMODE_FORCED);

    while (1) {
        // Wait for the measurement to be ready
        uint8_t interrupt_status = 0;
        soldered_bmp585_get_interrupt_status(&sensor, &interrupt_status);

        if (interrupt_status & BMP5_INT_ASSERTED_DRDY) {
            if (soldered_bmp585_get_sensor_data(&sensor) == BMP5_OK) {
                ESP_LOGI(TAG, "Pressure: %.2f Pa, Temperature: %.2f degC", sensor.data.pressure,
                         sensor.data.temperature);
            } else {
                ESP_LOGE(TAG, "Failed to read sensor data: %s", soldered_bmp585_status_string(&sensor));
            }

            // Trigger the next measurement
            soldered_bmp585_set_mode(&sensor, BMP5_POWERMODE_FORCED);

            // Only take a reading once per second
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
