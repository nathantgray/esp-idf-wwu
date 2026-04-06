/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Temperature Display - NeoPixel 7-Segment Display Driver Implementation
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include "esp_log.h"
#include "neopixel.h"
#include "neopixel_display.h"

static const char *TAG = "neopixel_display";

/* NeoPixel context */
static tNeopixelContext neopixel_ctx = NULL;

/* Current brightness */
static uint8_t brightness = 255;

/* Thread safety */
static portMUX_TYPE neopixel_mux = portMUX_INITIALIZER_UNLOCKED;

/* Decimal point is the 8th segment (optional, not used for temp display) */

/* 7-segment patterns (bit 0-6 = segments a-g) */
const uint8_t digitPatterns[10] = {
    0b1111110, /* 0 = a,b,c,d,e,f */
    0b1000010, /* 1 = b,c */
    0b0110111, /* 2 = a,b,d,e,g */
    0b1100111, /* 3 = a,b,c,d,g */
    0b1001011, /* 4 = b,c,f,g */
    0b1101101, /* 5 = a,c,d,f,g */
    0b1111101, /* 6 = a,c,d,e,f,g */
    0b1000110, /* 7 = a,b,c */
    0b1111111, /* 8 = a,b,c,d,e,f,g */
    0b1101111  /* 9 = a,b,c,d,f,g */
};

/* Decimal point is the 8th segment (optional, not used for temp display) */

/**
 * @brief Get LED index for a specific digit and segment
 *
 * @param digit Digit position (0-2, left to right)
 * @param segment Segment number (0-6)
 * @return Starting LED index for this segment
 */
static int segmentStartIndex(int digit, int segment) {
    return digit * LEDS_PER_DIGIT + segment * LEDS_PER_SEGMENT;
}

/**
 * @brief Apply brightness to an RGB color and return as single uint32_t
 */
static uint32_t applyBrightness(uint8_t r, uint8_t g, uint8_t b) {
    portENTER_CRITICAL(&neopixel_mux);
    uint8_t brightness_local = brightness;
    portEXIT_CRITICAL(&neopixel_mux);
    
    r = (uint16_t)r * brightness_local / 255;
    g = (uint16_t)g * brightness_local / 255;
    b = (uint16_t)b * brightness_local / 255;
    
    return NP_RGB(r, g, b);
}

esp_err_t neopixel_display_init(int gpio_pin) {
    if (neopixel_ctx != NULL) {
        ESP_LOGW(TAG, "Display already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Initializing NeoPixel display on GPIO %d with %d LEDs", gpio_pin, TOTAL_LEDS);
    neopixel_ctx = neopixel_Init(TOTAL_LEDS, gpio_pin);
    
    if (neopixel_ctx == NULL) {
        ESP_LOGE(TAG, "Failed to initialize neopixel context");
        return ESP_FAIL;
    }

    /* Clear the display */
    neopixel_display_clear();
    
    ESP_LOGI(TAG, "NeoPixel display initialized successfully");
    return ESP_OK;
}

void neopixel_display_deinit(void) {
    if (neopixel_ctx != NULL) {
        neopixel_display_clear();
        neopixel_Deinit(neopixel_ctx);
        neopixel_ctx = NULL;
        ESP_LOGI(TAG, "NeoPixel display deinitialized");
    }
}

void neopixel_get_color_for_temp(float temperature, uint8_t *r, uint8_t *g, uint8_t *b) {
    /* Color mapping:
       Cold (-20°C): Green (0, 255, 0)
       Cool (5°C): Cyan (0, 255, 255)
       Warm (20°C): Yellow (255, 255, 0)
       Hot (35°C): Orange (255, 165, 0)
       Hotter (50°C): Red (255, 0, 0)
    */
    
    if (temperature <= TEMP_COLD_MIN) {
        /* Pure green */
        *r = 0;
        *g = 255;
        *b = 0;
    } else if (temperature < TEMP_COOL) {
        /* Green to Cyan: increase blue */
        float ratio = (temperature - TEMP_COLD_MIN) / (TEMP_COOL - TEMP_COLD_MIN);
        *r = 0;
        *g = 255;
        *b = (uint8_t)(255 * ratio);
    } else if (temperature < TEMP_WARM) {
        /* Cyan to Yellow: reduce blue, increase red */
        float ratio = (temperature - TEMP_COOL) / (TEMP_WARM - TEMP_COOL);
        *r = (uint8_t)(255 * ratio);
        *g = 255;
        *b = (uint8_t)(255 * (1.0f - ratio));
    } else if (temperature < TEMP_HOT) {
        /* Yellow to Orange: reduce green */
        float ratio = (temperature - TEMP_WARM) / (TEMP_HOT - TEMP_WARM);
        *r = 255;
        *g = (uint8_t)(255 * (1.0f - ratio * 0.35f)); /* Final green is ~165 */
        *b = 0;
    } else if (temperature < TEMP_HOT_MAX) {
        /* Orange to Red: reduce green further */
        float ratio = (temperature - TEMP_HOT) / (TEMP_HOT_MAX - TEMP_HOT);
        *r = 255;
        *g = (uint8_t)(165 * (1.0f - ratio));
        *b = 0;
    } else {
        /* Pure red */
        *r = 255;
        *g = 0;
        *b = 0;
    }
}

esp_err_t neopixel_display_temperature(float temperature) {
    if (neopixel_ctx == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Clamp temperature for display purposes */
    float displayTemp = temperature;
    if (displayTemp > 99.9f) displayTemp = 99.9f;
    if (displayTemp < -99.9f) displayTemp = -99.9f;

    /* Get color for current temperature */
    uint8_t r, g, b;
    neopixel_get_color_for_temp(temperature, &r, &g, &b);
    uint32_t color_rgb = applyBrightness(r, g, b);

    /* Extract digits */
    int displayValue = (int)(fabsf(displayTemp)); /* Integer temperature only */
    
    int hundreds = (displayValue / 100) % 10;
    int tens = (displayValue / 10) % 10;
    int ones = displayValue % 10;

    /* Prepare pixel array */
    tNeopixel pixels[TOTAL_LEDS];
    for (int i = 0; i < TOTAL_LEDS; i++) {
        pixels[i].index = i;
        pixels[i].rgb = 0x000000;  /* Default: black */
    }

    /* Set segments for each digit */
    for (int digit_idx = 0; digit_idx < NUM_DIGITS; digit_idx++) {
        int digit_value = 0;
        
        if (displayTemp < 0) {
            /* Negative temperature: tens and ones only */
            if (digit_idx == 0) digit_value = tens;
            else if (digit_idx == 1) digit_value = ones;
            else digit_value = 0;  /* Blank third position */
        } else {
            /* Positive temperature: all three digits */
            if (digit_idx == 0) digit_value = hundreds;
            else if (digit_idx == 1) digit_value = tens;
            else digit_value = ones;
        }
        
        /* Get 7-segment pattern for this digit */
        uint8_t pattern = digitPatterns[digit_value];
        
        /* Set pixels for each segment of this digit */
        for (int segment = 0; segment < 7; segment++) {
            if (pattern & (1 << segment)) {
                int start_idx = segmentStartIndex(digit_idx, segment);
                for (int led = 0; led < LEDS_PER_SEGMENT && start_idx + led < TOTAL_LEDS; led++) {
                    pixels[start_idx + led].rgb = color_rgb;
                }
            }
        }
    }

    /* Update display */
    if (!neopixel_SetPixel(neopixel_ctx, pixels, TOTAL_LEDS)) {
        ESP_LOGE(TAG, "Failed to set pixels");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t neopixel_display_clear(void) {
    if (neopixel_ctx == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    int total_pixels = TOTAL_LEDS;
    tNeopixel pixels[total_pixels];
    for (int i = 0; i < total_pixels; i++) {
        pixels[i].index = i;
        pixels[i].rgb = 0x000000;  /* Black */
    }
    
    if (!neopixel_SetPixel(neopixel_ctx, pixels, total_pixels)) {
        ESP_LOGE(TAG, "Failed to clear display");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t neopixel_display_set_brightness(uint8_t new_brightness) {
    portENTER_CRITICAL(&neopixel_mux);
    brightness = new_brightness;
    portEXIT_CRITICAL(&neopixel_mux);
    
    ESP_LOGI(TAG, "Brightness set to %d", new_brightness);
    return ESP_OK;
}
