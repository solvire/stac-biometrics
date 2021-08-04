/**
 * Source file: esp_vehicle_locker.ino
 * Created on: July 12, 2021
 * Last modified on: August 2, 2021
 * 
 * Comments:
 * 	ESP32-CAM main script
 *  - GPIO 0 must be connected to GND to upload a sketch
 *  - After connecting GPIO 0 to GND, press the ESP32-CAM on-board RESET button to put your board in flashing mode
 */

/* enable WiFi */
#include <WiFi.h>
#include "wifi_config.h"

/* configure esp camera */
#include "esp_camera.h"
#include "camera_config.h"
#include "camera_pins.h"
#include "settings.h"

/* disable brownout problems */
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"

/* mount sdcard */
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

/* read and write from flash memory */
#include <EEPROM.h>

#define EEPROM_SIZE 1                  /* number of bytes you want to access */
#define BUILT_IN_LED 4                 /* built-in LED pin */
#define BUILT_IN_LED_GPIO GPIO_NUM_4   /* built-in LED pin GPIO */
#define MOTION_SENSOR_GPIO GPIO_NUM_13 /* external interrupt pin GPIO */

RTC_DATA_ATTR int bootCount = 0;

/* function prototypes */
void startCameraServer();
void startMainEngine();

/**
 * @brief Add log with new line for debug only
 * 
 * @param str: log message to be displayed
 * @returns none
 */

void logn(const char *str)
{
#ifdef LOGGING
    Serial.println(str);
#endif
}

/**
 * @brief Add log without new line for debug only
 * 
 * @param str: log message to be displayed
 * @returns none
 */

void log(const char *str)
{
#ifdef LOGGING
    Serial.print(str);
#endif
}

/**
 * @brief Add log without format string for debug only
 * 
 * @param fmt: format string
 * @param args: variodic parameters
 * @returns none
 */

void logf(const char *fmt, ...)
{
#ifdef LOGGING
    va_list args;
    va_start(args, fmt);
    Serial.printf(fmt, args);
    va_end(args);
#endif
}

/**
 * @brief Control the ESP32-CAM white on-board LED (flash) connected to GPIO 4
 *        When the ESP32-CAM takes a photo, it flashes the on-board LED.
 *        After taking the photo, the LED remains on, so we send instructions to turn it off.
 * 
 * @param mode: on/off flag {true, false}
 * @returns none
 */

void turnOnLed(bool mode)
{
    if (mode)
    {
        /* turn on */
        pinMode(BUILT_IN_LED, INPUT);
        digitalWrite(BUILT_IN_LED, LOW);
        rtc_gpio_hold_dis(BUILT_IN_LED_GPIO);
    }
    else
    {
        /* turn off */
        pinMode(BUILT_IN_LED, OUTPUT);
        digitalWrite(BUILT_IN_LED, LOW);
        rtc_gpio_hold_en(BUILT_IN_LED_GPIO);
    }
}

/**
 * @brief Camera initialization (OV2640)
 * 
 * @param none
 * @returns true if successful, false otherwise
 */

bool cameraInit()
{
    /* pin configuration */
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    /* if PSRAM IC present, init with UXGA resolution and higher JPEG quality for larger pre-allocated frame buffer */
    if (psramFound())
    {
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    }
    else
    {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

#if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
#endif

    /* camera initialization */
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        logf("[INFO] Failed to initialize camera with error 0x%x\n", err);
        return false;
    }

    logn("[INFO] Camera initialization success");
    return true;
}

/**
 * @brief Initialize WiFi and wait for the connection established
 * 
 * @param ssid: WiFi ssid
 * @param password: WiFi password
 * @returns none
 */

void wifiConnect(const char *ssid, const char *password)
{
    /* start WiFi */
    WiFi.begin(ssid, password);
    logn("[INFO] Waiting for WiFi connection ...");

    /* check WiFi connection */
    int cnt = 0;
    while ((cnt++ < MAX_ATTEMPT) && (WiFi.status() != WL_CONNECTED))
    {
        delay(500);
        log(".");
    }
    logn("\n[INFO] WiFi connected");
}

/**
 * @brief Initialize SD card
 * 
 * @param none
 * @returns true if successful, false otherwise
 */

bool sdCardInit()
{
    esp_err_t ret = ESP_FAIL;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 3,
    };
    sdmmc_card_t *card;

    logn("[INFO] Mounting SD card ...");
    ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret == ESP_OK)
    {
        logn("[INFO] SD card mount successfully!");
        return true;
    }

    logf("[INFO] Failed to mount SD card VFAT filesystem. Error: %d\n", ret);
    return false;
}

/**
 * @brief Main entry to initialize device
 * 
 * @param none
 * @returns none
 */

void setup()
{
    /* disable brownout detector */
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    /* serial initialization */
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    /* turn on the built-in LED to show it's working */
    turnOnLed(true);

    /* camera initialization */
    if (cameraInit())
    {
        sensor_t *s = esp_camera_sensor_get();

        /* initial sensors are flipped vertically and colors are a bit saturated */
        if (s->id.PID == OV3660_PID)
        {
            s->set_vflip(s, 1);       /* flip it back */
            s->set_brightness(s, 1);  /* up the brightness just a bit */
            s->set_saturation(s, -2); /* lower the saturation */
        }

        /* drop down frame size for higher initial frame rate */
        s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
        s->set_vflip(s, 1);
        s->set_hmirror(s, 1);
#endif

#ifdef WIFI_EN
        /* wifi initialization */
        wifiConnect(SSID, PASSWORD);
        startCameraServer();

        log("[INFO] Camera ready! Use 'http://");
        log(WiFi.localIP().toString().c_str());
        logn("' to connect.");
#else
        /* run unlocker engine */
        sdCardInit();
        startMainEngine();

        /* turn off LED to show it's done */
        turnOnLed(false);

        /* enable external interrupt */
        esp_sleep_enable_ext0_wakeup(MOTION_SENSOR_GPIO, 0);
        delay(1000);

        /* put the esp32 in deep sleep */
        logn("[INFO] Going to sleep now ...");
        esp_deep_sleep_start();
        logn("[NEVER] This will never be printed!");
#endif
    }
}

/**
 * @brief Infinite loop
 * 
 * @param none
 * @returns none
 */

void loop()
{
    /* do nothing */
}
