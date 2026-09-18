/**
 * @file soldered_bmp585.c
 * @brief Implementation for the soldered-bmp585 component
 *
 * Wraps Bosch's BMP5_SensorAPI, which lives unmodified in bmp5_api/, in an
 * ESP-IDF flavoured I2C driver.
 *
 * @author Soldered Electronics
 */

#include <string.h>
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soldered_bmp585.h"

// *****************************************************************************
// Section: Bosch Sensor API callbacks
//
// The sensor API reaches the outside world through these three callbacks. The
// interface descriptor it hands back to them is the handle itself, so that they
// can both talk over I2C and record the ESP-IDF error code.

/**
 * @brief Delay callback handed to the Bosch API
 *
 * @param[in] period_us Duration of the delay in microseconds
 * @param[in] intf_ptr Pointer to the handle, unused here
 */
static void bmp585_delay_us(uint32_t period_us, void *intf_ptr)
{
    const uint32_t tick_us = portTICK_PERIOD_MS * 1000;

    (void)intf_ptr;

    if (period_us >= tick_us) {
        vTaskDelay(period_us / tick_us);
        period_us %= tick_us;
    }

    if (period_us) {
        esp_rom_delay_us(period_us);
    }
}

/**
 * @brief I2C write callback handed to the Bosch API
 *
 * @param[in] reg_addr Register address of the sensor
 * @param[in] reg_data Data to be written to the sensor
 * @param[in] length Length of the transfer
 * @param[in] intf_ptr Pointer to the handle
 *
 * @return BMP5_OK if successful, a Bosch API error code otherwise
 */
static BMP5_INTF_RET_TYPE bmp585_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    bmp585_t *dev = (bmp585_t *)intf_ptr;
    uint8_t buf[32];
    esp_err_t err;

    if ((dev == NULL) || (dev->i2c_dev == NULL)) {
        return BMP5_E_NULL_PTR;
    }

    if (length + 1 > sizeof(buf)) {
        return BMP5_E_COM_FAIL;
    }

    /* The register address and the data go out as one transaction, so build
     * them into a single buffer first. */
    buf[0] = reg_addr;
    memcpy(&buf[1], reg_data, length);

    err = i2c_master_transmit(dev->i2c_dev, buf, length + 1, SOLDERED_BMP585_I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        return BMP5_E_COM_FAIL;
    }

    return BMP5_OK;
}

/**
 * @brief I2C read callback handed to the Bosch API
 *
 * @param[in] reg_addr Register address of the sensor
 * @param[out] reg_data Buffer the data is read into
 * @param[in] length Length of the transfer
 * @param[in] intf_ptr Pointer to the handle
 *
 * @return BMP5_OK if successful, a Bosch API error code otherwise
 */
static BMP5_INTF_RET_TYPE bmp585_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    bmp585_t *dev = (bmp585_t *)intf_ptr;
    esp_err_t err;

    if ((dev == NULL) || (dev->i2c_dev == NULL)) {
        return BMP5_E_NULL_PTR;
    }

    err = i2c_master_transmit_receive(dev->i2c_dev, &reg_addr, 1, reg_data, length, SOLDERED_BMP585_I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        return BMP5_E_COM_FAIL;
    }

    return BMP5_OK;
}

// *****************************************************************************
// Section: Initialization

esp_err_t soldered_bmp585_init(bmp585_t *dev, i2c_master_bus_handle_t bus, uint8_t i2c_addr)
{
    bmp585_config_t config = {
        .i2c_addr = i2c_addr,
        .scl_speed_hz = SOLDERED_BMP585_DEFAULT_SCL_SPEED_HZ,
    };

    return soldered_bmp585_init_with_config(dev, bus, &config);
}

esp_err_t soldered_bmp585_init_with_config(bmp585_t *dev, i2c_master_bus_handle_t bus, const bmp585_config_t *config)
{
    esp_err_t err;

    if ((dev == NULL) || (bus == NULL) || (config == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c_addr = config->i2c_addr;
    dev->status = BMP5_OK;

    i2c_device_config_t i2c_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->i2c_addr,
        .scl_speed_hz = config->scl_speed_hz ? config->scl_speed_hz : SOLDERED_BMP585_DEFAULT_SCL_SPEED_HZ,
    };

    err = i2c_master_bus_add_device(bus, &i2c_conf, &dev->i2c_dev);
    if (err != ESP_OK) {
        return err;
    }

    dev->sensor.intf = BMP5_I2C_INTF;
    dev->sensor.read = bmp585_i2c_read;
    dev->sensor.write = bmp585_i2c_write;
    dev->sensor.delay_us = bmp585_delay_us;
    dev->sensor.intf_ptr = dev;

    dev->status = bmp5_soft_reset(&dev->sensor);
    if (dev->status == BMP5_OK) {
        dev->status = bmp5_init(&dev->sensor);
    }
    if (dev->status == BMP5_OK) {
        dev->status = soldered_bmp585_set_osr_odr_config(dev, BMP5_OVERSAMPLING_64X, BMP5_OVERSAMPLING_4X,
                                                         BMP5_ODR_50_HZ, true);
    }
    if (dev->status == BMP5_OK) {
        dev->status = bmp5_set_power_mode(BMP5_POWERMODE_NORMAL, &dev->sensor);
    }

    if (dev->status != BMP5_OK) {
        i2c_master_bus_rm_device(dev->i2c_dev);
        dev->i2c_dev = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t soldered_bmp585_deinit(bmp585_t *dev)
{
    esp_err_t err;

    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (dev->i2c_dev == NULL) {
        return ESP_OK;
    }

    err = i2c_master_bus_rm_device(dev->i2c_dev);
    dev->i2c_dev = NULL;

    return err;
}

// *****************************************************************************
// Section: Register access

int8_t soldered_bmp585_read_regs(bmp585_t *dev, uint8_t reg_addr, uint8_t *reg_data, uint32_t length)
{
    dev->status = bmp5_get_regs(reg_addr, reg_data, length, &dev->sensor);

    return dev->status;
}

int8_t soldered_bmp585_write_regs(bmp585_t *dev, uint8_t reg_addr, const uint8_t *reg_data, uint32_t length)
{
    dev->status = bmp5_set_regs(reg_addr, reg_data, length, &dev->sensor);

    return dev->status;
}

int8_t soldered_bmp585_soft_reset(bmp585_t *dev)
{
    dev->status = bmp5_soft_reset(&dev->sensor);

    return dev->status;
}

// *****************************************************************************
// Section: Power mode

int8_t soldered_bmp585_set_mode(bmp585_t *dev, enum bmp5_powermode mode)
{
    dev->status = bmp5_set_power_mode(mode, &dev->sensor);

    return dev->status;
}

int8_t soldered_bmp585_get_mode(bmp585_t *dev, enum bmp5_powermode *mode)
{
    dev->status = bmp5_get_power_mode(mode, &dev->sensor);

    return dev->status;
}

// *****************************************************************************
// Section: Configuration

int8_t soldered_bmp585_set_osr_odr_config(bmp585_t *dev, uint8_t osr_t, uint8_t osr_p, uint8_t odr, bool press_en)
{
    struct bmp5_osr_odr_press_config config = {0};

    dev->status = bmp5_get_osr_odr_press_config(&config, &dev->sensor);
    if (dev->status != BMP5_OK) {
        return dev->status;
    }

    config.osr_t = osr_t;
    config.osr_p = osr_p;
    config.odr = odr;
    config.press_en = press_en ? BMP5_ENABLE : BMP5_DISABLE;

    dev->status = bmp5_set_osr_odr_press_config(&config, &dev->sensor);

    return dev->status;
}

int8_t soldered_bmp585_get_osr_odr_config(bmp585_t *dev, struct bmp5_osr_odr_press_config *config)
{
    dev->status = bmp5_get_osr_odr_press_config(config, &dev->sensor);

    return dev->status;
}

int8_t soldered_bmp585_set_iir_config(bmp585_t *dev, uint8_t iir_t, uint8_t iir_p)
{
    struct bmp5_iir_config config = {
        .set_iir_t = iir_t,
        .set_iir_p = iir_p,
        .shdw_set_iir_t = BMP5_ENABLE,
        .shdw_set_iir_p = BMP5_ENABLE,
    };

    dev->status = bmp5_set_iir_config(&config, &dev->sensor);

    return dev->status;
}

int8_t soldered_bmp585_get_iir_config(bmp585_t *dev, struct bmp5_iir_config *config)
{
    dev->status = bmp5_get_iir_config(config, &dev->sensor);

    return dev->status;
}

// *****************************************************************************
// Section: Data

int8_t soldered_bmp585_get_interrupt_status(bmp585_t *dev, uint8_t *status)
{
    dev->status = bmp5_get_interrupt_status(status, &dev->sensor);

    return dev->status;
}

int8_t soldered_bmp585_configure_interrupt(bmp585_t *dev, enum bmp5_intr_mode mode, enum bmp5_intr_polarity pol,
                                           enum bmp5_intr_drive drive, bool enable)
{
    dev->status =
        bmp5_configure_interrupt(mode, pol, drive, enable ? BMP5_INTR_ENABLE : BMP5_INTR_DISABLE, &dev->sensor);

    return dev->status;
}

int8_t soldered_bmp585_set_interrupt_source(bmp585_t *dev, bool data_ready, bool fifo_full, bool fifo_threshold,
                                            bool pressure_oor)
{
    struct bmp5_int_source_select source = {
        .drdy_en = data_ready ? BMP5_ENABLE : BMP5_DISABLE,
        .fifo_full_en = fifo_full ? BMP5_ENABLE : BMP5_DISABLE,
        .fifo_thres_en = fifo_threshold ? BMP5_ENABLE : BMP5_DISABLE,
        .oor_press_en = pressure_oor ? BMP5_ENABLE : BMP5_DISABLE,
    };

    dev->status = bmp5_int_source_select(&source, &dev->sensor);

    return dev->status;
}

int8_t soldered_bmp585_get_sensor_data(bmp585_t *dev)
{
    struct bmp5_osr_odr_press_config config = {0};
    struct bmp5_sensor_data data = {0};

    dev->status = bmp5_get_osr_odr_press_config(&config, &dev->sensor);
    if (dev->status != BMP5_OK) {
        return dev->status;
    }

    dev->status = bmp5_get_sensor_data(&data, &config, &dev->sensor);
    if (dev->status != BMP5_OK) {
        return dev->status;
    }

    dev->data.pressure = data.pressure;
    dev->data.temperature = data.temperature;

    return BMP5_OK;
}

// *****************************************************************************
// Section: Status

int8_t soldered_bmp585_check_status(const bmp585_t *dev)
{
    if (dev->status < BMP5_OK) {
        return SOLDERED_BMP585_ERROR;
    }

    return BMP5_OK;
}

const char *soldered_bmp585_status_string(const bmp585_t *dev)
{
    switch (dev->status) {
    case BMP5_OK:
        /* Don't return a text for OK. */
        return "";
    case BMP5_E_NULL_PTR:
        return "Null pointer";
    case BMP5_E_COM_FAIL:
        return "Communication failure";
    case BMP5_E_DEV_NOT_FOUND:
        return "Sensor not found";
    case BMP5_E_INVALID_CHIP_ID:
        return "Invalid chip id";
    case BMP5_E_NVM_NOT_READY:
        return "NVM not ready";
    case BMP5_E_POR_SOFTRESET:
        return "Power-on reset/softreset failure";
    case BMP5_E_INVALID_POWERMODE:
        return "Invalid powermode";
    case BMP5_E_INVALID_THRESHOLD:
        return "Invalid threshold";
    case BMP5_E_FIFO_FRAME_EMPTY:
        return "FIFO frame empty";
    case BMP5_E_NVM_INVALID_ADDR:
        return "Invalid NVM address";
    default:
        return "Undefined error code";
    }
}
