#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include "config.h"

ESP8266WebServer server(80);

// ===== ESTADOS =====
bool valvulaAberta = false;
bool modoAutomatico = true;
bool jaRegouHoje = false;
bool irrigandoAgora = false;

unsigned long inicioRega = 0;

int ultimaUmidade = 0;
String statusSolo = "Sem leitura";

// ===== CONTROLE DA VALVULA (RELÉ ATIVO EM LOW) =====

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

// ===== LEITURA DO SENSOR =====

int lerUmidade() {
  return analogRead(PINO_SENSOR);
}

// ===== API STATUS =====

void enviarStatus() {
  String json = "{";
  json += "\"umidade\":" + String(ultimaUmidade) + ",";
  json += "\"statusSolo\":\"" + statusSolo + "\",";
  json += "\"valvula\":\"" + String(valvulaAberta ? "ABERTA" : "FECHADA") + "\",";
  json += "\"modo\":\"" + String(modoAutomatico ? "AUTOMATICO" : "MANUAL") + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

// ===== PAGINA WEB =====

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
  html += "<p>Umidade: <b id='umidade'>---</b></p>";
  html += "<p>Status Solo: <b id='statusSolo'>---</b></p>";

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
  fecharValvula();

  Serial.println("Conectando WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado");
  Serial.println(WiFi.localIP());

  // NTP Brasil GMT-3
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Sincronizando hora...");
  delay(3000);

  // ROTAS
  server.on("/", paginaPrincipal);
  server.on("/status", enviarStatus);

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

  server.on("/abrir", []() {
    if (!modoAutomatico && !irrigandoAgora) abrirValvula();
    paginaPrincipal();
  });

  server.on("/fechar", []() {
    if (!modoAutomatico) fecharValvula();
    paginaPrincipal();
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

// ===== LOOP =====

void loop() {
  server.handleClient();

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  int horaAtual = timeinfo->tm_hour;
  int minutoAtual = timeinfo->tm_min;

  // Reset diário à meia-noite
  if (horaAtual == 0 && minutoAtual == 0) {
    jaRegouHoje = false;
  }

  // ===== DISPARO EXATO ÀS 16:00 =====
  if (modoAutomatico &&
      horaAtual == HORA_REGA &&
      minutoAtual == 0 &&
      !jaRegouHoje &&
      !irrigandoAgora) {

    ultimaUmidade = lerUmidade();

    Serial.print("Umidade: ");
    Serial.println(ultimaUmidade);

    if (ultimaUmidade > SOLO_SECO) {
      statusSolo = "Solo seco - irrigando";
      abrirValvula();
      inicioRega = millis();
      irrigandoAgora = true;
    } else {
      statusSolo = "Solo umido - sem irrigacao";
      jaRegouHoje = true;
    }
  }

  // ===== CONTROLE SEM DELAY =====
  if (irrigandoAgora) {
    if (millis() - inicioRega >= TEMPO_REGA) {
      fecharValvula();
      irrigandoAgora = false;
      jaRegouHoje = true;
      Serial.println("Rega finalizada automaticamente");
    }
  }
}
