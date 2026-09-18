/**
 * @file soldered_bmp585.h
 * @brief Public API for the soldered-bmp585 component
 *
 * ESP-IDF driver for the Soldered BMP585 barometric pressure sensor breakout
 * board over I2C. It is a thin, snake_case wrapper around Bosch's
 * BMP5_SensorAPI, which lives unmodified in bmp5_api/. Functions here are
 * prefixed soldered_bmp585_ rather than bmp5_ to keep naming consistent with
 * the rest of Soldered's ESP-IDF components.
 *
 * The I2C bus belongs to the application, not to this driver, so that other
 * Qwiic devices can share it. Create it with i2c_new_master_bus() and hand the
 * handle to soldered_bmp585_init().
 *
 * @author Soldered Electronics
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "bmp5.h"
#include "bmp5_defs.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

/** Returned by soldered_bmp585_check_status() when the last call failed */
#define SOLDERED_BMP585_ERROR INT8_C(-1)

/** I2C clock the sensor is driven at unless soldered_bmp585_init_with_config() says otherwise */
#define SOLDERED_BMP585_DEFAULT_SCL_SPEED_HZ 400000

/** How long a single I2C transaction may take before it is given up on */
#define SOLDERED_BMP585_I2C_TIMEOUT_MS 1000

/**
 * @brief One pressure/temperature reading
 */
typedef struct {
    float pressure;    /**< Pressure in Pa */
    float temperature; /**< Temperature in degrees Celsius */
} bmp585_data_t;

/**
 * @brief Optional settings of a BMP585, passed to soldered_bmp585_init_with_config()
 */
typedef struct {
    uint8_t i2c_addr;      /**< I2C address, BMP5_I2C_ADDR_PRIM or BMP5_I2C_ADDR_SEC */
    uint32_t scl_speed_hz; /**< I2C clock in Hz, 0 for ::SOLDERED_BMP585_DEFAULT_SCL_SPEED_HZ */
} bmp585_config_t;

/**
 * @brief Handle for one BMP585 breakout board
 *
 * Create one per breakout board. All fields are managed by the driver; treat
 * the struct as opaque and read state through the accessor functions.
 */
typedef struct {
    i2c_master_dev_handle_t i2c_dev; /**< I2C device handle, created by soldered_bmp585_init() */
    uint8_t i2c_addr;                /**< I2C address the sensor answers on */

    int8_t status; /**< Bosch Sensor API result code of the last executed call */

    struct bmp5_dev sensor; /**< Bosch API device structure */
    bmp585_data_t data;     /**< Last reading fetched by soldered_bmp585_get_sensor_data() */
} bmp585_t;

/**
 * @brief Attach a BMP585 to an already initialized I2C bus
 *
 * Adds the sensor as a device on `bus`, soft resets it, reads out its ID and
 * leaves it in normal power mode with 64x temperature oversampling, 4x
 * pressure oversampling and a 50Hz output data rate. The bus itself must
 * already exist, created with i2c_new_master_bus(); this leaves it free to be
 * shared with other devices.
 *
 * Runs at ::SOLDERED_BMP585_DEFAULT_SCL_SPEED_HZ; use
 * soldered_bmp585_init_with_config() to pick a different clock.
 *
 * @param[out] dev Handle to initialize
 * @param[in] bus I2C bus the breakout is wired to, previously initialized with
 *                i2c_new_master_bus()
 * @param[in] i2c_addr I2C address of the sensor, BMP5_I2C_ADDR_PRIM or
 *                     BMP5_I2C_ADDR_SEC depending on the state of the SDO pin
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a NULL argument,
 *         ESP_ERR_NOT_FOUND if the sensor did not answer, or the error
 *         returned by i2c_master_bus_add_device(). On ESP_ERR_NOT_FOUND the
 *         Bosch API result is left in `dev->status`
 */
esp_err_t soldered_bmp585_init(bmp585_t *dev, i2c_master_bus_handle_t bus, uint8_t i2c_addr);

/**
 * @brief Attach a BMP585 to an already initialized I2C bus with custom settings
 *
 * Same as soldered_bmp585_init(), but lets you pick the I2C clock.
 *
 * @param[out] dev Handle to initialize
 * @param[in] bus I2C bus the breakout is wired to, previously initialized with
 *                i2c_new_master_bus()
 * @param[in] config Settings to apply, see ::bmp585_config_t
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a NULL argument,
 *         ESP_ERR_NOT_FOUND if the sensor did not answer, or the error
 *         returned by i2c_master_bus_add_device()
 */
esp_err_t soldered_bmp585_init_with_config(bmp585_t *dev, i2c_master_bus_handle_t bus, const bmp585_config_t *config);

/**
 * @brief Detach the sensor from the I2C bus
 *
 * Does not deinitialize the bus itself, since the bus is owned by the caller.
 *
 * @param[in,out] dev Handle previously initialized with soldered_bmp585_init()
 *
 * @return ESP_OK on success, or the error returned by i2c_master_bus_rm_device()
 */
esp_err_t soldered_bmp585_deinit(bmp585_t *dev);

/**
 * @brief Read multiple consecutive registers
 *
 * @param[in,out] dev Handle
 * @param[in] reg_addr Start register address
 * @param[out] reg_data Buffer the data is read into
 * @param[in] length Number of registers to read
 *
 * @return BMP5_OK on success, a Bosch API error code otherwise
 */
int8_t soldered_bmp585_read_regs(bmp585_t *dev, uint8_t reg_addr, uint8_t *reg_data, uint32_t length);

/**
 * @brief Write multiple consecutive registers
 *
 * @param[in,out] dev Handle
 * @param[in] reg_addr Start register address
 * @param[in] reg_data Data to write
 * @param[in] length Number of registers to write
 *
 * @return BMP5_OK on success, a Bosch API error code otherwise
 */
int8_t soldered_bmp585_write_regs(bmp585_t *dev, uint8_t reg_addr, const uint8_t *reg_data, uint32_t length);

/**
 * @brief Trigger a soft reset of the sensor
 *
 * All settings are lost, so the sensor has to be reconfigured afterwards.
 *
 * @param[in,out] dev Handle
 *
 * @return BMP5_OK on success, a Bosch API error code otherwise
 */
int8_t soldered_bmp585_soft_reset(bmp585_t *dev);

/**
 * @brief Set the power mode
 *
 * @param[in,out] dev Handle
 * @param[in] mode BMP5_POWERMODE_STANDBY, BMP5_POWERMODE_NORMAL, BMP5_POWERMODE_FORCED,
 *                 BMP5_POWERMODE_CONTINUOUS or BMP5_POWERMODE_DEEP_STANDBY
 *
 * @return BMP5_OK on success, a Bosch API error code otherwise
 */
int8_t soldered_bmp585_set_mode(bmp585_t *dev, enum bmp5_powermode mode);

/**
 * @brief Get the power mode
 *
 * @param[in,out] dev Handle
 * @param[out] mode Current power mode
 *
 * @return BMP5_OK on success, a Bosch API error code otherwise
 */
int8_t soldered_bmp585_get_mode(bmp585_t *dev, enum bmp5_powermode *mode);

/**
 * @brief Set oversampling, output data rate and pressure-enable configuration
 *
 * @param[in,out] dev Handle
 * @param[in] osr_t Temperature oversampling, one of the BMP5_OVERSAMPLING_* macros
 * @param[in] osr_p Pressure oversampling, one of the BMP5_OVERSAMPLING_* macros
 * @param[in] odr Output data rate, one of the BMP5_ODR_* macros
 * @param[in] press_en Whether to enable pressure measurement
 *
 * @return BMP5_OK on success, a Bosch API error code otherwise
 */
int8_t soldered_bmp585_set_osr_odr_config(bmp585_t *dev, uint8_t osr_t, uint8_t osr_p, uint8_t odr, bool press_en);

/**
 * @brief Get the current oversampling, output data rate and pressure-enable configuration
 *
 * @param[in,out] dev Handle
 * @param[out] config Current configuration
 *
 * @return BMP5_OK on success, a Bosch API error code otherwise
 */
int8_t soldered_bmp585_get_osr_odr_config(bmp585_t *dev, struct bmp5_osr_odr_press_config *config);

/**
 * @brief Set the IIR filter coefficient used for temperature and pressure data
 *
 * @param[in,out] dev Handle
 * @param[in] iir_t Temperature IIR coefficient, one of the BMP5_IIR_FILTER_* macros
 * @param[in] iir_p Pressure IIR coefficient, one of the BMP5_IIR_FILTER_* macros
 *
 * @return BMP5_OK on success, a Bosch API error code otherwise
 */
int8_t soldered_bmp585_set_iir_config(bmp585_t *dev, uint8_t iir_t, uint8_t iir_p);

/**
 * @brief Get the current IIR filter configuration
 *
 * @param[in,out] dev Handle
 * @param[out] config Current configuration
 *
 * @return BMP5_OK on success, a Bosch API error code otherwise
 */
int8_t soldered_bmp585_get_iir_config(bmp585_t *dev, struct bmp5_iir_config *config);

/**
 * @brief Get the data-ready / FIFO / OOR interrupt status
 *
 * Useful for polling since this breakout doesn't expose the sensor's
 * interrupt pin.
 *
 * @param[in,out] dev Handle
 * @param[out] status Bitmask of BMP5_INT_ASSERTED_* flags
 *
 * @return BMP5_OK on success, a Bosch API error code otherwise
 */
int8_t soldered_bmp585_get_interrupt_status(bmp585_t *dev, uint8_t *status);

/**
 * @brief Read a new pressure/temperature measurement into `dev->data`
 *
 * @param[in,out] dev Handle
 *
 * @return BMP5_OK on success, a Bosch API error code otherwise
 */
int8_t soldered_bmp585_get_sensor_data(bmp585_t *dev);

/**
 * @brief Check whether the last call failed
 *
 * @param[in] dev Handle
 *
 * @return ::SOLDERED_BMP585_ERROR if the last call failed, BMP5_OK otherwise
 */
int8_t soldered_bmp585_check_status(const bmp585_t *dev);

/**
 * @brief Get a brief text description of the last status code
 *
 * @param[in] dev Handle
 *
 * @return String describing the status code, an empty string when it is
 *         BMP5_OK. The string is static and does not have to be freed
 */
const char *soldered_bmp585_status_string(const bmp585_t *dev);

#ifdef __cplusplus
}
#endif
