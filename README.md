# Soldered BMP585 Barometric Pressure Sensor Component

| ![Soldered BMP585 Barometric Pressure Sensor breakout](TODO_PRODUCT_IMAGE_URL) |
| :------------------------------------------------------------------------------------: |
|                      [Soldered BMP585 Barometric Pressure Sensor breakout](https://www.solde.red/333189)                      |

<!-- TODO: product not released yet (SKU 333189), swap the image URL above once the listing is live -->

ESP-IDF component for the Soldered BMP585 breakout board. The BMP585 is a Bosch barometric pressure sensor, measuring absolute pressure with ±6 Pa relative accuracy at output data rates up to 240 Hz, with configurable oversampling and an on-chip IIR filter, and supporting normal, forced and continuous power modes for balancing measurement speed against power consumption. The board connects over I2C and is part of the [Qwiic ecosystem](https://soldered.com/collections/qwiic-ecosystem), so no soldering is needed to hook it up.

### Installation

Add it to your project with the component manager:

```bash
idf.py add-dependency "solderedelectronics/soldered-bmp585"
```

Or clone this repository into your project's `components/` folder.

### Usage

The I2C bus belongs to your application, not to the driver, so that other Qwiic devices can share it. Create the bus first, then hand it over:

```c
#include "driver/i2c_master.h"
#include "soldered_bmp585.h"

i2c_master_bus_config_t bus_cfg = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = GPIO_NUM_21,
    .scl_io_num = GPIO_NUM_22,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
i2c_master_bus_handle_t bus;
ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

bmp585_t sensor;
ESP_ERROR_CHECK(soldered_bmp585_init(&sensor, bus, BMP5_I2C_ADDR_SEC));

if (soldered_bmp585_get_sensor_data(&sensor) == BMP5_OK) {
    printf("%.2f Pa, %.2f degC\n", sensor.data.pressure, sensor.data.temperature);
}
```

`soldered_bmp585_init_with_config()` takes a different I2C clock.

Every call leaves the Bosch API result code in the handle, so `soldered_bmp585_check_status()` and `soldered_bmp585_status_string()` describe what went wrong after any of them.

### Examples

- **basic_readings** - reads pressure and temperature in a loop in normal power mode, the mode most applications want
- **forced_mode_custom_config** - configures oversampling/IIR filtering and takes single-shot readings in forced power mode, for low-power, infrequent-reading use cases
- **data_ready_interrupt** - uses the sensor's physical interrupt pin to know when a new reading is ready, instead of polling the interrupt status register

Build any of them with:

```bash
cd examples/basic_readings
idf.py set-target esp32
idf.py build flash monitor
```

### Repository Contents

- **/src** - source files (.c), with the unmodified Bosch BMP5_SensorAPI in `src/bmp5_api/`
- **/include** - header files (.h), with the Bosch API headers in `include/bmp5_api/`
- **/examples** - examples for using the library
- **_other_** - idf_component.yml manifest file for ESP Component Registry

### Hardware design

You can find hardware design for this board in the Soldered BMP585 Barometric Pressure Sensor breakout hardware repository.

### Documentation

Access library documentation [here](https://docs.soldered.com/).

### About Soldered

<img src="https://raw.githubusercontent.com/SolderedElectronics/Soldered-Generic-Arduino-Library/dev/extras/Soldered-logo-color.png" alt="soldered-logo" width="500"/>

At Soldered, we design and manufacture a wide selection of electronic products to help you turn your ideas into acts and bring you one step closer to your final project. Our products are intented for makers and crafted in-house by our experienced team in Osijek, Croatia. We believe that sharing is a crucial element for improvement and innovation, and we work hard to stay connected with all our makers regardless of their skill or experience level. Therefore, all our products are open-source. Finally, we always have your back. If you face any problem concerning either your shopping experience or your electronics project, our team will help you deal with it, offering efficient customer service and cost-free technical support anytime. Some of those might be useful for you:

- [Web Store](https://www.soldered.com/shop)
- [Tutorials & Projects](https://soldered.com/learn)
- [Documentation](https://docs.soldered.com)

### Original source

This component is possible thanks to the original [BMP5_SensorAPI](https://github.com/boschsensortec/BMP5_SensorAPI) by Bosch Sensortec. Thank you, Bosch. The Bosch API is BSD-3-Clause licensed, its license is kept alongside the sources in `src/bmp5_api/LICENSE`.

### Open-source license

Soldered invests vast amounts of time into hardware & software for these products, which are all open-source. Please support future development by buying one of our products.

Check license details in the LICENSE file. Long story short, use these open-source files for any purpose you want to, as long as you apply the same open-source licence to it and disclose the original source. No warranty - all designs in this repository are distributed in the hope that they will be useful, but without any warranty. They are provided "AS IS", therefore without warranty of any kind, either expressed or implied. The entire quality and performance of what you do with the contents of this repository are your responsibility. In no event, Soldered (TAVU) will be liable for your damages, losses, including any general, special, incidental or consequential damage arising out of the use or inability to use the contents of this repository.

## Have fun!

And thank you from your fellow makers at Soldered Electronics.
