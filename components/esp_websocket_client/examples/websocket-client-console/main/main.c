#include <stdio.h>
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_console.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_websocket_client.h"
#include "websocket_client_vfs.h"
#include "cmd_nvs.h"
#include "cmd_system.h"

static const char* TAG = "example";
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void websocket_app_start(void);
static void run_console_task(void);
static void console_task(void* arg);

typedef struct {
    esp_websocket_client_handle_t client;
    RingbufHandle_t from_websocket;
    RingbufHandle_t to_websocket;
} console_task_args_t;

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
    websocket_app_start();
}

static void websocket_app_start(void)
{
    websocket_client_vfs_config_t config = {
        .base_path = "/websocket",
        .send_timeout_ms = 10000,
        .recv_timeout_ms = 10000,
        .recv_buffer_size = 256,
        .fallback_stdout = stdout
    };
    ESP_ERROR_CHECK(websocket_client_vfs_register(&config));

    esp_websocket_client_config_t websocket_cfg = {};
    websocket_cfg.uri = "ws://10.0.2.83:8080";
    websocket_cfg.reconnect_timeout_ms = 1000;
    websocket_cfg.network_timeout_ms = 10000;

    ESP_LOGI(TAG, "Connecting to %s...", websocket_cfg.uri);

    esp_websocket_client_handle_t client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
    esp_websocket_client_start(client);
    websocket_client_vfs_add_client(client, 0);

    run_console_task();

    while(true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    esp_websocket_client_close(client, portMAX_DELAY);
    ESP_LOGI(TAG, "Websocket Stopped");
    esp_websocket_client_destroy(client);
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Websocket connected");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Websocket disconnected");
        break;
    case WEBSOCKET_EVENT_DATA:
        if (data->op_code == 0x08 && data->data_len == 2) {
            ESP_LOGI(TAG, "Received closed message with code=%d", 256 * data->data_ptr[0] + data->data_ptr[1]);
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "Websocket error");
        break;
    }

    websocket_client_vfs_event_handler(data->client, event_id, event_data);
}

static void run_console_task(void)
{
    xTaskCreate(console_task, "console_task", 16384, NULL, 5, NULL);
}

static void console_task(void* arg)
{
    FILE* websocket_io = fopen("/websocket/0", "r+");
    if (websocket_io == NULL) {
        ESP_LOGE(TAG, "Failed to open websocket I/O file");
        vTaskDelete(NULL);
    }

    stdin = websocket_io;
    stdout = websocket_io;
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Initialize the console */
    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
#if CONFIG_LOG_COLORS
            .hint_color = atoi(LOG_COLOR_CYAN),
#endif
    };
    ESP_ERROR_CHECK( esp_console_init(&console_config) );
    esp_console_register_help_command();
    register_nvs();
    register_system_common();

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);

    /* Set command maximum length */
    linenoiseSetMaxLineLen(console_config.max_cmdline_length);

    /* Don't return empty lines */
    linenoiseAllowEmpty(false);

    linenoiseSetDumbMode(1);

    const char* prompt = LOG_COLOR_I "websocket> " LOG_RESET_COLOR;

    printf("\n"
           "This is an example of ESP-IDF console component.\n"
           "Type 'help' to get the list of commands.\n"
           "Use UP/DOWN arrows to navigate through command history.\n"
           "Press TAB when typing command name to auto-complete.\n"
           "Press Enter or Ctrl+C will terminate the console environment.\n");

    /* Main loop */
    while(true) {
        /* Get a line using linenoise.
         * The line is returned when ENTER is pressed.
         */
        char* line = linenoise(prompt);
        if (line == NULL) {
            continue;
        }
        
        /* Add the command to the history if not empty*/
        if (strlen(line) > 0) {
            linenoiseHistoryAdd(line);
        }

        /* Try to run the command */
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        } else if (err != ESP_OK) {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        /* linenoise allocates line buffer on the heap, so need to free it */
        linenoiseFree(line);
    }
}