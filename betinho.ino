#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

const char* WIFI_SSID = "teste";
const char* WIFI_PASS = "12345678";
const char* apiUrlGet = "https://betinho-service.onrender.com/scheduledTime/getAll?token=eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.XkioJmy3Ta-mgTotFxqUzZfG4xP73ycm-4kTIxUpM9o";
const char* apiUrlPost = "https://betinho-service.onrender.com/currentQuantity/create?token=eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.XkioJmy3Ta-mgTotFxqUzZfG4xP73ycm-4kTIxUpM9o";

Servo meuServo;  // Cria um objeto Servo para controlar um servo motor
ThreeWire myWire(25, 17, 26); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);
const int botaoPin = 27;
bool servoAtivo = false;
const int ledPin = 16;
int estadoBotao;               // Variável para armazenar o estado do botão
int ultimoEstadoBotao = HIGH;  // Variável para armazenar o último estado do botão
int estadoLed = LOW;           // Estado inicial do LED
int contadorCliques = 0;       // Contador de cliques no botão
int contadorLoops = 0;         // Contador de loops
int contadorData = 0;
int lastHttpRequestStatus = 0;  // Variável para armazenar o código de resposta HTTP da última solicitação

const size_t bufferSize = JSON_ARRAY_SIZE(3) + 3 * JSON_OBJECT_SIZE(1) + 90;
DynamicJsonDocument jsonBuffer(bufferSize);
String horarios[3];

String getDateTimeString(const RtcDateTime &dt);

void setup() {
  Serial.begin(9600);
  Rtc.Begin();

  meuServo.attach(14);  // O pino D1 do Wemos D1 R32
  pinMode(botaoPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);


  //wifi
  Serial.println("Conectando à rede Wi-Fi, aguarde...");

  // Adicionando mensagens de depuração
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("Senha: ");
  Serial.println(WIFI_PASS);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Esperar pela conexão
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado com sucesso!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha na conexão. Verifique suas credenciais ou a disponibilidade da rede.");
  }
  makeHttpRequestWithRetry();
}

void loop() {
  RtcDateTime now = Rtc.GetDateTime();
  String dateTimeString = getDateTimeString(now);
  if (contadorLoops <= 4) {

    estadoBotao = digitalRead(botaoPin);

    if (estadoBotao == LOW) {
      // Incrementa o contador de cliques
      servoAtivo = true;
      contadorCliques++;
    }

    int minutosNow = now.Hour() * 60 + now.Minute();
    int minutosPrimeiroHorario = atoi(horarios[contadorData].c_str()) * 60 + atoi(horarios[contadorData].c_str() + 3);

    Serial.println("Hora atual em minutos: " + String(minutosNow));
    Serial.println("Primeiro horário em minutos: " + String(minutosPrimeiroHorario));

    if (compareDateTime(horarios[contadorData], now) && contadorCliques % 2 == 1 && servoAtivo) {
      if (contadorData != 0) {
        digitalWrite(ledPin, HIGH);
        moveServo(180);  // Mova o servo para a posição 180
        delay(390);
        stopServo();
        delay(2000);
      }
      int gramas = generateRandomValue(contadorData);
      postToApi(gramas);
      contadorLoops++;
      contadorData++;
    } else if (compareDateTimeFinal(now, 23, 50) && contadorCliques % 2 == 1 && servoAtivo) {
      int gramas = generateRandomValue(contadorData);
      postToApi(gramas);
      estadoLed = LOW;
      digitalWrite(ledPin, LOW);

      // Para o servo motor
      stopServo();

      // Reinicia os contadores
      contadorCliques = 0;
      contadorLoops = 1;
    }
    else if (minutosPrimeiroHorario < minutosNow)
    {
  contadorData++;
    }
  }

  ultimoEstadoBotao = estadoBotao;
}

void moveServo(int position) {
  meuServo.write(position);  // Define a posição do servo
}

void stopServo() {
  meuServo.write(90);  // Envie um pulso neutro para parar o servo
}

void makeHttpRequest() {
  HTTPClient http;

  Serial.println("Fazendo solicitação HTTP...");

  // Enviar a solicitação GET
  http.begin(apiUrlGet);
  lastHttpRequestStatus = http.GET();

  if (lastHttpRequestStatus > 0) {
    Serial.print("Resposta da API: ");
    String payload = http.getString();
    Serial.println(payload);

    // Processar a resposta JSON
    processJsonResponse(payload);
  } else {
    Serial.print("Erro na solicitação HTTP. Código de resposta: ");
    Serial.println(lastHttpRequestStatus);

    // Exibir mensagem de erro com base no código de resposta
    switch (lastHttpRequestStatus) {
      case HTTP_CODE_BAD_REQUEST:
        Serial.println("Erro: Solicitação inválida");
        break;
      case HTTP_CODE_UNAUTHORIZED:
        Serial.println("Erro: Não autorizado");
        break;
      case HTTP_CODE_NOT_FOUND:
        Serial.println("Erro: Recurso não encontrado");
        break;
      // Adicione mais casos conforme necessário
      default:
        Serial.println("Erro desconhecido");
        break;
    }
  }

  http.end();
}


bool compareDateTime(String dateTime, RtcDateTime now) {
  // Implemente a lógica para comparar o "dateTime" com a hora atual
  // Aqui está um exemplo simples que compara apenas a hora
  String hourStr = dateTime.substring(11, 13);
  int hour = hourStr.toInt();
  return now.Hour() >= hour;
}

bool compareDateTimeFinal(RtcDateTime now, int targetHour, int targetMinute) {
  // Comparar com a hora e minuto desejados
  return now.Hour() == targetHour && now.Minute() == targetMinute;
}

void postToApi(float grams) {
  HTTPClient http;

  // Obter a hora atual
  RtcDateTime now = Rtc.GetDateTime();
  String dateTimeString = getDateTimeString(now);

  // Configurar a URL da API para a solicitação POST
  http.begin(apiUrlPost);

  // Configurar os cabeçalhos da solicitação POST
  http.addHeader("Content-Type", "application/json");

  // Criar um objeto JSON para os dados a serem enviados
  const size_t capacity = JSON_OBJECT_SIZE(2) + 30;  // Ajuste o tamanho conforme necessário
  DynamicJsonDocument doc(capacity);
  doc["dateTime"] = dateTimeString;  // Adiciona a data atual no formato timestamp
  doc["grams"] = grams;

  // Converter o objeto JSON em uma string
  String jsonPayload;
  serializeJson(doc, jsonPayload);
  Serial.print(grams);
  Serial.print(jsonPayload);

  int tentativas = 0;
  const int maxTentativas = 3;

  while (tentativas < maxTentativas) {
    // Enviar a solicitação POST
    int httpCode = http.POST(jsonPayload);

    if (httpCode > 0) {
      String response = http.getString();
      response.trim();  // Remover espaços em branco extras
      Serial.println("Resposta da API após POST: " + response);
      break;  // Sai do loop se a solicitação for bem-sucedida
    } else {
      Serial.print("Erro na solicitação POST. Código de status HTTP: ");
      Serial.println(httpCode);
      Serial.print("Mensagem de erro: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }

    tentativas++;
    delay(1000);  // Aguarda 1 segundo entre as tentativas
  }

  if (tentativas == maxTentativas) {
    Serial.println("Número máximo de tentativas atingido. A solicitação POST não pôde ser concluída.");
  }

  http.end();
}


int generateRandomValue(int caseValue) {
  switch (caseValue) {
    case 0:
      return random(75, 86);
    case 1:
      return random(57, 63);
    case 2:
      return random(37, 43);
    case 3:
      return random(17, 23);
    case 4:
      return random(0, 5);
  }
}

String getDateTimeString(const RtcDateTime &dt)
{
    char datestring[20];
    snprintf_P(datestring,
               sizeof(datestring),
               PSTR("%02u:%02u:%02u"),
               dt.Hour(),
               dt.Minute(),
               dt.Second());
    return String(datestring);
}

void makeHttpRequestWithRetry() {
  int tentativas = 0;
  const int maxTentativas = 3;

  while (tentativas < maxTentativas) {
    // Tente fazer a solicitação HTTP
    Serial.println("Tentativa #" + String(tentativas + 1) + " de " + String(maxTentativas));
    makeHttpRequest();

    // Verifique se a solicitação foi bem-sucedida
    if (lastHttpRequestStatus == HTTP_CODE_OK) {
      break;  // Saia do loop se a solicitação for bem-sucedida
    }

    tentativas++;
    delay(5000);  // Aguarde 5 segundos antes de tentar novamente
  }
}

void processJsonResponse(String jsonString) {
  deserializeJson(jsonBuffer, jsonString);

  if (jsonBuffer.is<JsonArray>()) {
    JsonArray root = jsonBuffer.as<JsonArray>();

    Serial.println("Horários obtidos:");

    for (int i = 0; i < root.size(); i++) {
      String time = root[i]["time"];
      Serial.println("Horário " + String(i + 1) + ": " + time);

      // Adicionar o horário ao vetor
      horarios[i] = time;

      // Aguarde 1 segundo entre cada horário
      delay(1000);
    }

  } else {
    Serial.println("Falha ao analisar JSON");
  }
}
