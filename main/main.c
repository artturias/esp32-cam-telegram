#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_camera.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "esp_crt_bundle.h"
#include "esp_sntp.h"
#include "driver/gpio.h"
#include <time.h>
#include "secrets.h"

// WiFi Configuration (from secrets.h)
#define WIFI_PASS WIFI_PASSWORD

// Telegram Configuration (from secrets.h)
#define TELEGRAM_API_URL "https://api.telegram.org/bot" TELEGRAM_BOT_TOKEN

// Camera pins for AI-Thinker ESP32-CAM
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0      5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

// LED Flash pin for ESP32-CAM
#define CAM_PIN_FLASH   4

static const char *TAG = "ESP32-CAM-TELEGRAM";
static EventGroupHandle_t wifi_event_group;
static int last_update_id = 0;
static bool flash_enabled = true;  // Flash mode: enabled by default

#define WIFI_CONNECTED_BIT BIT0

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from WiFi, reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected! IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        
        // Disable WiFi power save for maximum performance
        esp_wifi_set_ps(WIFI_PS_NONE);
        ESP_LOGI(TAG, "WiFi power save disabled for max throughput");
        
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "Connecting to WiFi...");
    return ESP_OK;
}

static esp_err_t camera_init(void)
{
    camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sscb_sda = CAM_PIN_SIOD,
        .pin_sscb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_XGA,   // 1024x768 for balance of quality and speed
        .jpeg_quality = 8,              // Quality 8 produces ~30-50KB images with good detail
        .fb_count = 1,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return err;
    }

    // Detect camera sensor and configure accordingly for maximum quality
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        if (s->id.PID == OV2640_PID) {
            ESP_LOGI(TAG, "OV2640 camera detected");
        } else if (s->id.PID == OV3660_PID) {
            ESP_LOGI(TAG, "OV3660 camera detected");
        }
        
        // Orientation settings
        if (s->set_vflip) s->set_vflip(s, 1);
        if (s->set_hmirror) s->set_hmirror(s, 1);
        
        // Quality enhancement settings for best image quality
        if (s->set_brightness) s->set_brightness(s, 0);      // -2 to 2, 0 = default
        if (s->set_contrast) s->set_contrast(s, 2);           // -2 to 2, 2 = maximum contrast for detail
        if (s->set_saturation) s->set_saturation(s, 2);       // -2 to 2, 2 = maximum color saturation
        if (s->set_sharpness) s->set_sharpness(s, 3);         // -2 to 3, 3 = maximum sharpness for detail
        if (s->set_denoise) s->set_denoise(s, 1);             // Reduced for faster capture (was 5)
        
        // Exposure control for optimal lighting
        if (s->set_ae_level) s->set_ae_level(s, 0);           // Auto exposure level (0 = normal)
        if (s->set_aec2) s->set_aec2(s, 1);                   // Auto exposure enabled
        if (s->set_dcw) s->set_dcw(s, 1);                     // Downsample color window
        if (s->set_agc_gain) s->set_agc_gain(s, 0);           // Auto gain control (0 = auto)
        
        // White balance
        if (s->set_awb_gain) s->set_awb_gain(s, 1);           // Auto white balance gain
        
        ESP_LOGI(TAG, "Camera quality settings optimized");
    }

    // Initialize flash LED
    gpio_config_t flash_conf = {
        .pin_bit_mask = (1ULL << CAM_PIN_FLASH),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&flash_conf);
    gpio_set_level(CAM_PIN_FLASH, 0);  // Start with flash off
    ESP_LOGI(TAG, "Flash LED initialized on GPIO%d", CAM_PIN_FLASH);

    ESP_LOGI(TAG, "Camera initialized successfully");
    return ESP_OK;
}

// Send text message to Telegram
static esp_err_t telegram_send_message(char *chat_id, const char *text)
{
    char url[512];
    snprintf(url, sizeof(url), TELEGRAM_API_URL "/sendMessage");

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");

    // URL encode the message and prepare POST data
    char post_data[1024];
    snprintf(post_data, sizeof(post_data), "chat_id=%s&text=%s", chat_id, text);

    esp_err_t err = esp_http_client_open(client, strlen(post_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection for message: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int written = esp_http_client_write(client, post_data, strlen(post_data));
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to write message data");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status_code == 200) {
        ESP_LOGI(TAG, "Message sent successfully");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to send message, status: %d", status_code);
        return ESP_FAIL;
    }
}

static esp_err_t telegram_send_photo(char *chat_id, camera_fb_t *fb)
{
    char url[512];
    snprintf(url, sizeof(url), TELEGRAM_API_URL "/sendPhoto");

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = NULL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 60000,  // 60 seconds for XGA images (~30-50KB)
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Add form data headers
    esp_http_client_set_header(client, "Content-Type", "multipart/form-data; boundary=----WebKitFormBoundary1234567890");

    // Prepare form data
    char form_start[512];
    snprintf(form_start, sizeof(form_start),
        "------WebKitFormBoundary1234567890\r\n"
        "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n"
        "%s\r\n"
        "------WebKitFormBoundary1234567890\r\n"
        "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n",
        chat_id);

    char form_end[] = "\r\n------WebKitFormBoundary1234567890--\r\n";

    int form_start_len = strlen(form_start);
    int form_end_len = strlen(form_end);
    int total_len = form_start_len + fb->len + form_end_len;
    
    ESP_LOGI(TAG, "Sending photo: %d bytes (form_start=%d, image=%d, form_end=%d)", 
             total_len, form_start_len, fb->len, form_end_len);

    // Open connection with known content length and stream the body
    esp_err_t err = esp_http_client_open(client, total_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        // Retry once after a short delay
        vTaskDelay(pdMS_TO_TICKS(500));
        err = esp_http_client_open(client, total_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Retry open failed: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            return err;
        }
    }
    
    ESP_LOGI(TAG, "HTTP connection opened, writing data...");

    int written = 0;
    written = esp_http_client_write(client, form_start, form_start_len);
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to write form start");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Form start written: %d bytes", form_start_len);

    written = esp_http_client_write(client, (const char *)fb->buf, fb->len);
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to write image body");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Image data written: %d bytes", fb->len);

    written = esp_http_client_write(client, form_end, form_end_len);
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to write form footer");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Form end written: %d bytes", form_end_len);
    
    ESP_LOGI(TAG, "All data written, fetching response headers...");

    // Read response
    int content_len = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP Status = %d, content_length = %d", status_code, content_len);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status_code == 200) {
        ESP_LOGI(TAG, "Photo sent successfully to chat %s", chat_id);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to send photo, status code: %d", status_code);
        return ESP_FAIL;
    }
}

// Get updates from Telegram
static void telegram_get_updates_task(void *pvParameters)
{
    char url[512];
    char *response_buffer = malloc(16384);  // Allocate on heap to avoid stack overflow
    
    if (!response_buffer) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        return;
    }
    
    while (1) {
        // Wait for WiFi connection
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
        
        snprintf(url, sizeof(url), TELEGRAM_API_URL "/getUpdates?offset=%d&timeout=5&limit=1", last_update_id + 1);
        ESP_LOGI(TAG, "Polling Telegram API (offset=%d)...", last_update_id + 1);
        
        esp_http_client_config_t config = {
            .url = url,
            .timeout_ms = 35000,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };
        
        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_open(client, 0);
        if (err == ESP_OK) {
            int content_length = esp_http_client_fetch_headers(client);
            int status_code = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "getUpdates response: status=%d, length=%d", status_code, content_length);
            
            if (status_code == 200 && content_length != ESP_FAIL) {
                int total_read = 0;
                while (total_read < 16383) {
                    int chunk = esp_http_client_read(client, response_buffer + total_read, 16383 - total_read);
                    if (chunk <= 0) break;
                    total_read += chunk;
                }
                ESP_LOGI(TAG, "Read %d bytes from response", total_read);
                if (total_read > 0) {
                    response_buffer[total_read] = '\0';
                    
                    // Simple JSON parsing to find /photo command
                    char *text_ptr = strstr(response_buffer, "\"text\":");
                    char *chat_id_ptr = strstr(response_buffer, "\"chat\":{\"id\":");
                    char *update_id_ptr = strstr(response_buffer, "\"update_id\":");
                    
                    if (update_id_ptr) {
                        int update_id = atoi(update_id_ptr + 12);
                        if (update_id > last_update_id) {
                            last_update_id = update_id;
                        }
                    }
                    
                    if (text_ptr && chat_id_ptr) {
                        // Extract command
                        char *cmd_start = text_ptr + 8;
                        if (*cmd_start == '"') cmd_start++;
                        
                        // Extract chat_id for all commands
                        char *id_start = chat_id_ptr + 13;
                        char chat_id[32];
                        int i = 0;
                        while (id_start[i] >= '0' && id_start[i] <= '9' && i < 31) {
                            chat_id[i] = id_start[i];
                            i++;
                        }
                        chat_id[i] = '\0';
                        
                        // Handle /start command
                        if (strncmp(cmd_start, "/start", 6) == 0) {
                            ESP_LOGI(TAG, "Received /start from chat %s", chat_id);
                            telegram_send_message(chat_id, 
                                "Welcome to ESP32-CAM Baby Monitor!\n\n"
                                "Available commands:\n"
                                "/photo - Take a photo (wait 15-30s)\n"
                                "/flash on - Enable LED flash\n"
                                "/flash off - Disable LED flash\n"
                                "/help - Show this message\n\n"
                                "NOTE: Photos take 15-30 seconds to upload.");
                        }
                        // Handle /help command
                        else if (strncmp(cmd_start, "/help", 5) == 0) {
                            ESP_LOGI(TAG, "Received /help from chat %s", chat_id);
                            char help_msg[512];
                            snprintf(help_msg, sizeof(help_msg),
                                "ESP32-CAM Commands:\n\n"
                                "/photo - Capture and send photo\n"
                                "/flash on - Turn flash ON\n"
                                "/flash off - Turn flash OFF\n"
                                "/help - Show this help\n\n"
                                "Current flash: %s\n\n"
                                "Note: Photo capture takes 15-30 seconds.", 
                                flash_enabled ? "ON" : "OFF");
                            telegram_send_message(chat_id, help_msg);
                        }
                        // Handle /flash command
                        else if (strncmp(cmd_start, "/flash", 6) == 0) {
                            // Check for 'on' or 'off' after /flash
                            if (strncmp(cmd_start + 7, "on", 2) == 0) {
                                flash_enabled = true;
                                ESP_LOGI(TAG, "Flash enabled by chat %s", chat_id);
                                telegram_send_message(chat_id, "Flash enabled");
                            } else if (strncmp(cmd_start + 7, "off", 3) == 0) {
                                flash_enabled = false;
                                ESP_LOGI(TAG, "Flash disabled by chat %s", chat_id);
                                telegram_send_message(chat_id, "Flash disabled");
                            } else {
                                char flash_status[128];
                                snprintf(flash_status, sizeof(flash_status),
                                    "Flash is currently: %s\n\n"
                                    "Use /flash on or /flash off",
                                    flash_enabled ? "ON" : "OFF");
                                telegram_send_message(chat_id, flash_status);
                            }
                        }
                        // Handle /photo command
                        else if (strncmp(cmd_start, "/photo", 6) == 0) {
                            ESP_LOGI(TAG, "[PERF] Received /photo command at %lld ms", esp_timer_get_time()/1000);
                            
                            // Flush any stale frames from buffer to prevent overflow
                            ESP_LOGI(TAG, "Flushing camera buffers...");
                            camera_fb_t *stale_fb = esp_camera_fb_get();
                            if (stale_fb) {
                                esp_camera_fb_return(stale_fb);
                            }
                            // Give camera time to stabilize after flush
                            vTaskDelay(pdMS_TO_TICKS(100));
                            
                            // Turn on flash FIRST if enabled
                            if (flash_enabled) {
                                gpio_set_level(CAM_PIN_FLASH, 1);
                                ESP_LOGI(TAG, "Flash LED ON");
                                vTaskDelay(pdMS_TO_TICKS(800));  // 800ms for sensor to adjust exposure
                            }
                            
                            // Capture photo
                            ESP_LOGI(TAG, "[PERF] Starting capture at %lld ms", esp_timer_get_time()/1000);
                            camera_fb_t *fb = esp_camera_fb_get();
                            ESP_LOGI(TAG, "[PERF] Capture complete at %lld ms", esp_timer_get_time()/1000);
                            
                            // Turn off flash immediately after capture
                            if (flash_enabled) {
                                gpio_set_level(CAM_PIN_FLASH, 0);
                                ESP_LOGI(TAG, "Flash LED OFF");
                            }
                            
                            if (fb) {
                                ESP_LOGI(TAG, "Photo captured: %d bytes", fb->len);
                                
                                // NOW send feedback (non-blocking, user gets status during upload)
                                telegram_send_message(chat_id, "Photo captured! Uploading...");
                                
                                ESP_LOGI(TAG, "[PERF] Starting upload at %lld ms", esp_timer_get_time()/1000);
                                esp_err_t result = telegram_send_photo(chat_id, fb);
                                ESP_LOGI(TAG, "[PERF] Upload complete at %lld ms", esp_timer_get_time()/1000);
                                
                                // CRITICAL: Return frame buffer immediately to prevent overflow
                                esp_camera_fb_return(fb);
                                
                                if (result != ESP_OK) {
                                    telegram_send_message(chat_id, "Failed to send photo. Please try again.");
                                }
                            } else {
                                ESP_LOGE(TAG, "Camera capture failed - buffer overflow or timeout");
                                telegram_send_message(chat_id, "Camera busy. Wait 2 seconds and try again.");
                                // Flush again to recover from error state
                                stale_fb = esp_camera_fb_get();
                                if (stale_fb) esp_camera_fb_return(stale_fb);
                            }
                        }
                    }
                }
            }
        } else {
            ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        }
        
        esp_http_client_cleanup(client);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    free(response_buffer);
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP32-CAM Telegram Baby Monitor Starting...");

    // Initialize camera
    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed!");
        return;
    }

    // Initialize WiFi
    wifi_init();

    // Wait for WiFi connection
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "WiFi connected successfully!");

    // Sync time for TLS certificate validation
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    // wait until time is set
    for (int i = 0; i < 40; i++) {
        time_t now; struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year > (2019 - 1900)) break;
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Start Telegram polling task
    xTaskCreate(telegram_get_updates_task, "telegram_task", 8192, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Bot is ready! Send /photo command in Telegram to get a photo.");
}
