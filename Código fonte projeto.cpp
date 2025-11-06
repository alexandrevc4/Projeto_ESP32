#include <WiFi.h>
#include "DHT.h"

// === Mapeamento de Hardware ===
#define lamp1 13
#define lamp2 14
#define lamp3 27
#define lamp4 26
#define lamp5 32
#define lamp6 33

#define gasSensor 35  //mq-5

#define sirenePin 23

#define DHTPIN  15 //DHT22        
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

const char *ssid     = "Alexandre";
const char *password = "Senha_do_wifi";

// === Estados ===
bool lampState[6] = {false, false, false, false, false, false};
bool gasDetected = false;
int limiteGas = 1000;

float temperatura = 0.0;
float umidade = 0.0;

WiFiServer server(80);

// Nomes personalizados das lâmpadas
const char* lampNames[6] = {
  "Quarto 1",
  "Quarto 2",
  "Banheiro",
  "Sala",
  "Cozinha",
  "Externa"
};

void setup() {
  Serial.begin(115200);

  int relayPins[] = {lamp1, lamp2, lamp3, lamp4, lamp5, lamp6};
  for (int i = 0; i < 6; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }

  pinMode(gasSensor, INPUT);
  pinMode(sirenePin, OUTPUT);
  digitalWrite(sirenePin, HIGH);

  dht.begin(); // inicializa o DHT22

  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(741);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
  Serial.println(WiFi.localIP());

  server.begin();
}

void loop() {
  verificaGas();
  lerTemperatura();
  handleClient();
}

void verificaGas() {
  int leitura = analogRead(gasSensor);
  Serial.print("MQ-5: ");
  Serial.println(leitura);

  if (leitura > limiteGas) {
    gasDetected = true;
    digitalWrite(sirenePin, LOW);
  } else {
    gasDetected = false;
    digitalWrite(sirenePin, HIGH);
  }

  delay(500);
}

void lerTemperatura() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(h) && !isnan(t)) {
    umidade = h;
    temperatura = t;
    Serial.printf("Temperatura: %.1f °C | Umidade: %.1f %%\n", temperatura, umidade);
  } else {
    Serial.println("Falha ao ler DHT22!");
  }
}

void handleClient() {
  WiFiClient client = server.available();

  if (client) {
    Serial.println("Novo cliente conectado");
    String currentLine = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        currentLine += c;

        if (c == '\n' && currentLine.endsWith("\r\n\r\n")) {
          int relayPins[] = {lamp1, lamp2, lamp3, lamp4, lamp5, lamp6};

          // Processa comandos
          for (int i = 0; i < 6; i++) {
            String toggleCmd = "GET /toggle" + String(i + 1);
            if (currentLine.indexOf(toggleCmd) >= 0) {
              lampState[i] = !lampState[i];
              digitalWrite(relayPins[i], lampState[i] ? LOW : HIGH); // ativo em LOW
            }
          }

          if (currentLine.indexOf("GET /onAll") >= 0) {
            for (int i = 0; i < 6; i++) {
              lampState[i] = true;
              digitalWrite(relayPins[i], LOW);
            }
          }

          if (currentLine.indexOf("GET /offAll") >= 0) {
            for (int i = 0; i < 6; i++) {
              lampState[i] = false;
              digitalWrite(relayPins[i], HIGH);
            }
          }

          // Envia resposta HTML
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println();
          client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'>");
          client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
          client.println("<style>");
          client.println("body { font-family: Arial; background-color: #f2f2f2; text-align: center; padding: 20px; }");
          client.println(".button { padding: 15px 30px; font-size: 22px; margin: 10px; border: none; border-radius: 10px; cursor: pointer; width: 80%; max-width: 300px; }");
          client.println(".on { background-color: #4CAF50; color: white; }");
          client.println(".off { background-color: #f44336; color: white; }");
          client.println("h2 { font-size: 28px; margin-bottom: 20px; }");
          client.println(".card { background: white; padding: 15px; border-radius: 10px; box-shadow: 0 0 5px rgba(0,0,0,0.1); }");
          client.println(".footer { margin-top: 40px; font-size: 14px; color: #555; }");
          client.println("</style></head><body>");

          client.println("<h2>Controle de Iluminação 💡</h2>");

          // Grid das lâmpadas com nomes personalizados
          client.println("<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 20px; max-width: 650px; margin: auto;'>");
          for (int i = 0; i < 6; i++) {
            client.printf(
              "<div class='card'>"
              "<p style='font-weight: bold; font-size: 18px;'>%s: %s</p>"
              "<a href=\"/toggle%d\"><button class=\"button %s\">%s</button></a>"
              "</div>",
              lampNames[i],
              lampState[i] ? "💡 Ligada" : "🌑 Desligada",
              i + 1,
              lampState[i] ? "off" : "on",
              lampState[i] ? "Desligar" : "Ligar"
            );
          }
          client.println("</div>");

          // Botões Ligar Todas / Desligar Todas em card
          client.println("<div class='card' style='max-width: 650px; margin: 30px auto 10px;'>");
          client.println("<div style='display: flex; gap: 20px;'>");
          client.println("<a href=\"/onAll\" style='flex: 1;'><button class=\"button on\" style='width: 100%;'>🔆 Ligar Todas</button></a>");
          client.println("<a href=\"/offAll\" style='flex: 1;'><button class=\"button off\" style='width: 100%;'>🌑 Desligar Todas</button></a>");
          client.println("</div></div>");

          // Status do gás
          client.println("<div class='card' style='max-width: 350px; margin: 20px auto;'>");
          client.println("<h3>Status do Gás:</h3>");
          if (gasDetected) {
            client.println("<p style='color:red; font-size: 20px;'>🔴 GÁS DETECTADO!.</p>");
          } else {
            client.println("<p style='color:green; font-size: 20px;'>🟢 Ambiente Seguro</p>");
          }
          client.println("</div>");

          // Status Temperatura e Umidade
          client.println("<div class='card' style='max-width: 310px; margin: 20px auto;'>");
          client.println("<h3>Temperatura e Umidade 🌡️</h3>");
          client.printf("<p style='font-size: 20px;'>🌡️ %.1f °C</p>", temperatura);
          client.printf("<p style='font-size: 20px;'>💧 %.1f %%</p>", umidade);
          client.println("</div>");

          // Rodapé
          client.println("<div class='footer'>Desenvolvido por Alexandre Alves</div>");

          client.println("</body></html>");
          client.println();
          break;
        }
      }
    }
    client.stop();
    Serial.println("Cliente desconectado");
  }
}

