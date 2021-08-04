/**
 * Source file: app_unlocker.cpp
 * Created on: July 14, 2021
 * Last modified on: July 30, 2021
 * 
 * Comments:
 *  Main functions to implement the features
 */

#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "Arduino.h"

#include "fb_gfx.h"
#include "fd_forward.h"
#include "fr_forward.h"

#include "settings.h"

#define ENROLL_CONFIRM_TIMES 5
#define FACE_ID_SAVE_NUMBER 7

static mtmn_config_t mtmn_config = {0};
static int8_t is_enrolling = 0;
static face_id_list id_list = {0};

static int run_face_recognition(dl_matrix3du_t *image_matrix, box_array_t *net_boxes)
{
    dl_matrix3du_t *aligned_face = NULL;
    int matched_id = 0;

    aligned_face = dl_matrix3du_alloc(1, FACE_WIDTH, FACE_HEIGHT, 3);
    if (!aligned_face)
    {
        Serial.println("Could not allocate face recognition buffer");
        return matched_id;
    }

    if (align_face(net_boxes, image_matrix, aligned_face) == ESP_OK)
    {
        if (is_enrolling == 1)
        {
            int8_t left_sample_face = enroll_face(&id_list, aligned_face);

            if (left_sample_face == (ENROLL_CONFIRM_TIMES - 1))
            {
                Serial.printf("Enrolling Face ID: %d\n", id_list.tail);
            }

            Serial.printf("Enrolling Face ID: %d sample %d\n", id_list.tail, ENROLL_CONFIRM_TIMES - left_sample_face);

            if (left_sample_face == 0)
            {
                is_enrolling = 0;
                Serial.printf("Enrolled Face ID: %d\n", id_list.tail);
            }
        }
        else
        {
            matched_id = recognize_face(&id_list, aligned_face);
            if (matched_id >= 0)
            {
                Serial.printf("Match Face ID: %u\n", matched_id);
            }
            else
            {
                Serial.println("No Match Found");
                matched_id = -1;
            }
        }
    }
    else
    {
        Serial.println("Face Not Aligned");
    }

    dl_matrix3du_free(aligned_face);
    return matched_id;
}

static esp_err_t run_unlocker_engine()
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    dl_matrix3du_t *image_matrix = NULL;

    uint8_t *_jpg_buf = NULL;
    size_t _jpg_buf_len = 0;
    char *part_buf[64];
    bool detected = false;
    int face_id = 0;

    while (true)
    {
        detected = false;
        face_id = 0;
        fb = esp_camera_fb_get();

        if (!fb)
        {
            Serial.println("[INFO] Camera capture failed");
            res = ESP_FAIL;
        }
        else
        {
            if (fb->width > 400)
            {
                if (fb->format != PIXFORMAT_JPEG)
                {
                    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if (!jpeg_converted)
                    {
                        Serial.println("JPEG compression failed");
                        res = ESP_FAIL;
                    }
                }
                else
                {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
            }
            else
            {
                image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
                if (!image_matrix)
                {
                    Serial.println("dl_matrix3du_alloc failed");
                    res = ESP_FAIL;
                }
                else
                {
                    if (!fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item))
                    {
                        Serial.println("fmt2rgb888 failed");
                        res = ESP_FAIL;
                    }
                    else
                    {
                        box_array_t *net_boxes = face_detect(image_matrix, &mtmn_config);
                        if (net_boxes || fb->format != PIXFORMAT_JPEG)
                        {
                            if (net_boxes)
                            {
                                detected = true;
                                face_id = run_face_recognition(image_matrix, net_boxes);

                                /* release memory */
                                free(net_boxes->score);
                                free(net_boxes->box);
                                free(net_boxes->landmark);
                                free(net_boxes);
                            }
                            if (!fmt2jpg(image_matrix->item, fb->width * fb->height * 3, fb->width, fb->height, PIXFORMAT_RGB888, 90, &_jpg_buf, &_jpg_buf_len))
                            {
                                Serial.println("fmt2jpg failed");
                                res = ESP_FAIL;
                            }
                            esp_camera_fb_return(fb);
                            fb = NULL;
                        }
                        else
                        {
                            _jpg_buf = fb->buf;
                            _jpg_buf_len = fb->len;
                        }
                    }
                    dl_matrix3du_free(image_matrix);
                }
            }
        }

        /* release buffer */
        if (fb)
        {
            /* avoid use-after-free */
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }
        else if (_jpg_buf)
        {
            /* avoid use-after-free */
            free(_jpg_buf);
            _jpg_buf = NULL;
        }

        if (res != ESP_OK)
        {
            break;
        }
    }
    return res;
}

static void mtmn_init()
{
    mtmn_config.type = FAST;
    mtmn_config.min_face = 80;
    mtmn_config.pyramid = 0.707;
    mtmn_config.pyramid_times = 4;
    mtmn_config.p_threshold.score = 0.6;
    mtmn_config.p_threshold.nms = 0.7;
    mtmn_config.p_threshold.candidate_number = 20;
    mtmn_config.r_threshold.score = 0.7;
    mtmn_config.r_threshold.nms = 0.7;
    mtmn_config.r_threshold.candidate_number = 10;
    mtmn_config.o_threshold.score = 0.7;
    mtmn_config.o_threshold.nms = 0.7;
    mtmn_config.o_threshold.candidate_number = 1;
}

/**
 * @brief Start main unlocker engine
 * 
 * @param none
 * @returns none
 */

void startMainEngine()
{
    mtmn_init();
    face_id_init(&id_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);
    run_unlocker_engine();
}
