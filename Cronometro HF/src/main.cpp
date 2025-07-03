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
#define RXD2 16  // HC-12 TX 
#define TXD2 15  // HC-12 RX 

// ===========================
// Configuração dos pinos da câmera
// ===========================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Protótipos de função
void startCameraServer();
void setupLedFlash(int pin);

// Função do servidor HTTP para streaming da câmera
void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) {
            camera_fb_t *fb = esp_camera_fb_get();
            if (!fb) {
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }
            httpd_resp_set_type(req, "image/jpeg");
            httpd_resp_send(req, (const char *)fb->buf, fb->len);
            esp_camera_fb_return(fb);
            return ESP_OK;
        },
        .user_ctx = NULL
    };

    // Iniciar o servidor HTTP
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &index_uri);
        Serial.println("Servidor HTTP iniciado");
    } else {
        Serial.println("Erro ao iniciar servidor HTTP");
    }
}

void setup() {
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    
    Serial.println("\nIniciando sistema...");

    // Configuração da câmera
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
    config.frame_size = FRAMESIZE_UXGA;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 10;
    config.fb_count = 2;

    // Verifica PSRAM
    if (!psramFound()) {
        Serial.println("AVISO: PSRAM não detectada, ajustando configuração");
        config.frame_size = FRAMESIZE_SVGA;
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.fb_count = 1;
    }

    // Inicializa a câmera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Falha na inicialização da câmera: 0x%x\n", err);
        return;
    }

    // Conecta ao WiFi
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
        
        // Inicia servidor de câmera
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
    while (millis() - t0 < 3000) { // espera até 3 segundos por ACK
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