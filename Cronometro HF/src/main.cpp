#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <esp_http_server.h>

// ===================
// Configuração do Modelo da Câmera
// ===================
#define CAMERA_MODEL_AI_THINKER // Possui PSRAM

// ===========================
// Configuração Wi-Fi
// ===========================
const char *ssid = "Cilla Tech Park";
const char *password = "@@ctp.br";

// ===========================
// Pinos do HC-12 (ajustados para não conflitar com câmera)
// ===========================
#define RXD2 16 // HC-12 TX
#define TXD2 15 // HC-12 RX

// ===========================
// Configuração dos pinos da câmera
// ===========================
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Variáveis globais para o handler de stream
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// Protótipos de função
esp_err_t stream_handler(httpd_req_t *req);

// Servidor HTTP para streaming da câmera
httpd_handle_t stream_httpd = NULL;

void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port += 1;

    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
    };

    Serial.printf("Iniciando servidor de stream na porta: '%d'\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
        Serial.println("Servidor de streaming iniciado");
    } else {
        Serial.println("Erro ao iniciar servidor de streaming");
    }
}

esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char *part_buf[64];
    static int64_t last_frame = 0;
    if (!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Falha na captura da câmera");
            res = ESP_FAIL;
        } else {
            if (fb->format != PIXFORMAT_JPEG) {
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
                fb = NULL;
                if (!jpeg_converted) {
                    Serial.println("Falha na conversão para JPEG");
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf = fb->buf;
                _jpg_buf_len = fb->len;
            }
        }

        if (res == ESP_OK) {
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }

        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if (_jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }

        if (res != ESP_OK) break;

        int64_t current_frame = esp_timer_get_time();
        int64_t frame_time = current_frame - last_frame;
        last_frame = current_frame;
        // Serial.printf("MJPG: %uKB %lldms (%.1ffps)\n", (uint32_t)(_jpg_buf_len / 1024), frame_time / 1000, 1000000.0 / frame_time);
    }

    last_frame = 0;
    return res;
}

void setup() {
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    Serial.println("\nIniciando sistema...");

    // Configuração da câmera (ajustada para evitar erros de heap)
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
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;

    config.frame_size = FRAMESIZE_VGA;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    if (!psramFound()) {
        Serial.println("AVISO: PSRAM não detectada (modo DRAM forçado)");
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Falha na inicialização da câmera: 0x%x\n", err);
        return;
    }

    WiFi.begin(ssid, password);
    WiFi.setSleep(false);

    Serial.print("Conectando ao WiFi");
    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
        Serial.print(".");
        delay(500);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nFalha na conexão WiFi");
    } else {
        Serial.println("\nWiFi conectado");
        Serial.print("Endereço IP: ");
        Serial.println(WiFi.localIP());
        startCameraServer();
    }

    Serial.println("Sistema pronto para operação");
    Serial.println("Aguardando comunicações...");
}

void loop() {
    // Envio do PC para o HC-12 + espera por ACK
    if (Serial.available()) {
        String msg = Serial.readStringUntil('\n');
        bool ackRecebido = false;

        Serial2.println(msg);
        Serial.print("[Gugu Negro] ");
        Serial.println(msg);

        unsigned long t0 = millis();
        while (millis() - t0 < 3000) {
            if (Serial2.available()) {
                String resposta = Serial2.readStringUntil('\n');
                resposta.trim();
                if (resposta == "ACK") {
                    ackRecebido = true;
                    break;
                }
            }
        }

        if (ackRecebido) {
            Serial.println("[ACK recebido]");
        } else {
            Serial.println("[Sem ACK - a outra ponta não respondeu]");
        }
    }

    // Receber dados do HC-12 e enviar ACK de volta
    if (Serial2.available()) {
        String recebido = Serial2.readStringUntil('\n');
        recebido.trim();

        if (recebido.length() > 0) {
            Serial.print("[Morgana Roluda] ");
            Serial.println(recebido);
            Serial2.println("ACK");
        }
    }

    delay(100);
}
