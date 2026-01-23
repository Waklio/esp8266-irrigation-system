#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "config.h"

ESP8266WebServer server(80);

// ===== ESTADOS =====
bool valvulaAberta = false;
bool modoAutomatico = false;
unsigned long ultimoTempoLeitura = 0;
int ultimaUmidade = 0;
String statusSolo = "Sem leitura";

// ===== FUNÇÕES HARDWARE =====

void abrirValvula() {
  digitalWrite(PINO_VALVULA, LOW);
  valvulaAberta = true;
  Serial.println("Valvula ABERTA");
}

void fecharValvula() {
  digitalWrite(PINO_VALVULA, HIGH);
  valvulaAberta = false;
  Serial.println("Valvula FECHADA");
}

int lerUmidade() {
  digitalWrite(PINO_SENSOR_VCC, HIGH);
  delay(300);
  int leitura = analogRead(PINO_SENSOR);
  digitalWrite(PINO_SENSOR_VCC, LOW);
  return leitura;
}

// ===== API STATUS (AJAX) =====

void enviarStatus() {
  String json = "{";
  json += "\"umidade\":" + String(ultimaUmidade) + ",";
  json += "\"statusSolo\":\"" + statusSolo + "\",";
  json += "\"valvula\":\"" + String(valvulaAberta ? "ABERTA" : "FECHADA") + "\",";
  json += "\"modo\":\"" + String(modoAutomatico ? "AUTOMATICO" : "MANUAL") + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

// ===== PÁGINA WEB =====

void paginaPrincipal() {
  String html = "<html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial;text-align:center;}";
  html += "button{font-size:18px;padding:15px;margin:10px;width:220px;}";
  html += "</style></head><body>";

  html += "<h1>Sistema de Irrigacao</h1>";

  html += "<p>Modo: <b id='modo'>---</b></p>";
  html += "<p>Valvula: <b id='valvula'>---</b></p>";
  html += "<p>Umidade (A0): <b id='umidade'>---</b></p>";
  html += "<p>Status do Solo: <b id='statusSolo'>---</b></p>";

  html += "<a href='/manual'><button>Modo Manual</button></a><br>";
  html += "<a href='/auto'><button>Modo Automatico</button></a><br><br>";
  html += "<a href='/abrir'><button>Abrir Valvula</button></a><br>";
  html += "<a href='/fechar'><button>Fechar Valvula</button></a>";

  html += "<script>";
  html += "function atualizarStatus(){";
  html += "fetch('/status')";
  html += ".then(res => res.json())";
  html += ".then(data => {";
  html += "document.getElementById('modo').innerText = data.modo;";
  html += "document.getElementById('valvula').innerText = data.valvula;";
  html += "document.getElementById('umidade').innerText = data.umidade;";
  html += "document.getElementById('statusSolo').innerText = data.statusSolo;";
  html += "});";
  html += "}";
  html += "setInterval(atualizarStatus, 2000);";
  html += "window.onload = atualizarStatus;";
  html += "</script>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

// ===== SETUP =====

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PINO_VALVULA, OUTPUT);
  pinMode(PINO_SENSOR_VCC, OUTPUT);

  fecharValvula();
  digitalWrite(PINO_SENSOR_VCC, LOW);

  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Wi-Fi conectado. IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", paginaPrincipal);
  server.on("/status", enviarStatus);

  server.on("/abrir", []() {
    if (!modoAutomatico) abrirValvula();
    paginaPrincipal();
  });

  server.on("/fechar", []() {
    if (!modoAutomatico) fecharValvula();
    paginaPrincipal();
  });

  server.on("/manual", []() {
    modoAutomatico = false;
    Serial.println("Modo MANUAL ativado");
    paginaPrincipal();
  });

  server.on("/auto", []() {
    modoAutomatico = true;
    Serial.println("Modo AUTOMATICO ativado");
    paginaPrincipal();
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

// ===== LOOP =====

void loop() {
  server.handleClient();

  if (modoAutomatico) {
    unsigned long agora = millis();

    if (agora - ultimoTempoLeitura >= INTERVALO_LEITURA) {
      ultimoTempoLeitura = agora;

      ultimaUmidade = lerUmidade();

      Serial.print("Umidade: ");
      Serial.println(ultimaUmidade);

      if (ultimaUmidade > SOLO_SECO) {
        statusSolo = "Solo SECO, irrigando";
        Serial.println(statusSolo);

        abrirValvula();
        delay(TEMPO_REGA);
        fecharValvula();
      } else {
        statusSolo = "Solo OK, sem irrigacao";
        Serial.println(statusSolo);
      }

      Serial.println("----------------------------");
    }
  }
}
