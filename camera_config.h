/**
 * Header file: camera_config.h
 * Created on: July 15, 2021
 * Last modified on: July 15, 2021
 * 
 * Comments:
 * 	Select camera model
 * 
 * Warning:
 *  PSRAM IC required for UXGA resolution and high JPEG quality
 *  Ensure ESP32 Wrover Module or other board with PSRAM is selected
 *  Partial images will be transmitted if image exceeds buffer size
 */

#ifndef __CAMERA_CONFIG_H__
#define __CAMERA_CONFIG_H__

#define CAMERA_MODEL_AI_THINKER       // Has PSRAM
// #define CAMERA_MODEL_WROVER_KIT       // Has PSRAM
// #define CAMERA_MODEL_ESP_EYE          // Has PSRAM
// #define CAMERA_MODEL_M5STACK_PSRAM    // Has PSRAM
// #define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
// #define CAMERA_MODEL_M5STACK_WIDE     // Has PSRAM
// #define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
// #define CAMERA_MODEL_TTGO_T_JOURNAL   // No PSRAM

#endif
