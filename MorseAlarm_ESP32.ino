#include <WiFi.h>
#include <WebServer.h>
#include "time.h"

// Credenziali wifi
const char* ssid     = "";
const char* password = "";

// GPIO (pin hardware)
const int PIN_PUNTO  = 21;
const int PIN_LINEA  = 22;
const int PIN_INVIO  = 23;
const int PIN_BUZZER = 4;
const int LED_ROSSI[] = {33, 5, 19};
const int LED_BIANCHI[] = {14, 32, 18};
const int PIN_RGB_ROSSO = 27;
const int PIN_RGB_VERDE = 26;
const int PIN_RGB_BLU   = 25;

// Variabili
WebServer server(80);
int  svegliaOre    = 8;
int  svegliaMinuti = 30;

bool allarmeInCorso   = false; // serve a capire quando la sveglia sta suonando o meno
bool attesaMinutoSuccessivo = false; // serve a non far suonare la sveglia più volte 

int frequenzaAllarme = 1000; // frequenza di base del buzzer
unsigned long ultimoAumentoTempoMillis = 0; // cronometro a intervalli per i 30 secondi

int    sfidaAttuale[3];
int    cifraDaRisolvere = 0;
String sequenzaInserita = "";

const String MORSE_0 = "-----";
const String MORSE_1 = ".----";

// codice per pagina web--------------------------------------------------

void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html lang="it">
<head>
  <meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>MorsAlarm</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Orbitron:wght@700&display=swap');
    :root { --bg: #0a0c10; --accent: #00e5ff; --text: #c8d0e0; }
    body { background: var(--bg); color: var(--text); font-family: 'Share Tech Mono', monospace; display: flex; flex-direction: column; align-items: center; justify-content: center; min-height: 100vh; margin: 0; }
    .card { background: #111318; border: 1px solid #1e2330; border-radius: 12px; padding: 30px; width: 90%; max-width: 380px; text-align: center; box-shadow: 0 0 30px rgba(0,229,255,0.1); }
    h1 { font-family: 'Orbitron', sans-serif; color: var(--accent); margin-bottom: 20px; }
    .time-display { background: rgba(0,229,255,0.05); border: 1px solid var(--accent); padding: 15px; border-radius: 8px; font-size: 2rem; font-family: 'Orbitron'; margin-bottom: 20px; }
    input { background: #000; border: 1px solid #1e2330; color: var(--accent); padding: 10px; width: 65px; text-align: center; font-size: 1.2rem; margin: 5px; outline: none; }
    button { width: 100%; padding: 12px; background: transparent; border: 1px solid var(--accent); color: var(--accent); font-family: 'Orbitron'; cursor: pointer; margin-top: 15px; transition: 0.3s; }
    button:hover { background: var(--accent); color: #000; }
  </style>
</head>
<body>
  <div class="card">
    <h1>MORSALARM</h1>
    <div class="time-display">)rawhtml";
  html += (svegliaOre < 10 ? "0" : "") + String(svegliaOre) + ":" + (svegliaMinuti < 10 ? "0" : "") + String(svegliaMinuti);
  html += R"rawhtml(</div>
    <form action="/set" method="GET">
      <input type="number" name="h" min="0" max="23" placeholder="HH" required>
      <input type="number" name="m" min="0" max="59" placeholder="MM" required>
      <button type="submit">SET ALARM</button>
    </form>
  </div>
</body>
</html>)rawhtml";
  server.send(200, "text/html", html);
}

void handleSet() {
  if (server.hasArg("h") && server.hasArg("m")) {
    svegliaOre= server.arg("h").toInt();
    svegliaMinuti= server.arg("m").toInt();
    attesaMinutoSuccessivo = false; 
    Serial.print("Nuova sveglia impostata: ");
    Serial.print(svegliaOre); 
    Serial.print(":");
    Serial.println(svegliaMinuti);
    server.sendHeader("Location", "/");
    server.send(303);
  }
}


// SETUP codice 
void setup() {
  Serial.begin(115200);
  delay(1500); // Tempo per aprire il monitor seriale
  
  Serial.println("\n--- AVVIO MORSALARM SYSTEM ---");

  pinMode(PIN_PUNTO, INPUT_PULLUP); pinMode(PIN_LINEA, INPUT_PULLUP); pinMode(PIN_INVIO, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_RGB_ROSSO, OUTPUT); pinMode(PIN_RGB_VERDE, OUTPUT); pinMode(PIN_RGB_BLU, OUTPUT);
  for (int i = 0; i < 3; i++) { pinMode(LED_ROSSI[i], OUTPUT); pinMode(LED_BIANCHI[i], OUTPUT); }

  Serial.print("Connessione a: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // qui prova a connettersi al wifi, WL_CONNECTED sarà 3 se è connesso, mentre "t<20" sono i numeri di tentavi che fa per connettersi
  
  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 20) {
    digitalWrite(PIN_RGB_BLU, HIGH); delay(400); digitalWrite(PIN_RGB_BLU, LOW); delay(400); 
    Serial.print(".");
    t++;
  }

// qui controlla che lo status sia = a 3, e di seguito ci dice l'indirizzo IP 

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n--- WIFI CONNESSO! ---");
    Serial.print("Indirizzo IP: ");
    Serial.println(WiFi.localIP()); 
    Serial.println("----------------------");
    
    digitalWrite(PIN_RGB_VERDE, HIGH); 
    
    // it.pool.ntp.org è il client italiano che ci fornisce l'orario
    // i due 3600 raffigurano i due diversi fusi orari in base alla "stagione" (ora legale e ora solare)
    
    configTime(3600, 3600, "it.pool.ntp.org");
    delay(5000); // Attende 5 secondi per dare tempo al protocollo NTP di sincronizzare l'orario
    digitalWrite(PIN_RGB_VERDE, LOW);
  } else {
    Serial.println("\n--- CONNESSIONE FALLITA! ---");
    digitalWrite(PIN_RGB_ROSSO, HIGH); 
    delay(5000); 
    digitalWrite(PIN_RGB_ROSSO, LOW);
  }
  // ci porta alla root del server (infatti alla fine dell'url abbiamo una "/" invisibile)
  server.on("/", handleRoot);
  
  // qui invece ci porta direttamente alla fase di set, ovvero dove impostiamo l'orario
  server.on("/set", handleSet);
  server.begin();
  Serial.println("Web Server avviato.");

  // qui ci genera il seme di partenza per calcolare i numeri casuali per i led
  // analogRead(0) legge un pin in "floating" cioè non collegato a nulla, e quindi legge i valori di tensione in quel punto e si comporta come una piccola antenna
  randomSeed(analogRead(0));
}

// LOOP codice

void loop() {
  server.handleClient();
  struct tm timeinfo;

  // Qui va a "prendere" l'orario attuale; se non ci riesce ferma il loop.
  if (!getLocalTime(&timeinfo)) return;

  // CONTROLLO ATTIVAZIONE
  if (timeinfo.tm_hour == svegliaOre && timeinfo.tm_min == svegliaMinuti) {
      if (!allarmeInCorso && !attesaMinutoSuccessivo) {
          Serial.println(">>> SVEGLIA SCATTATA! <<<");
          allarmeInCorso   = true;
          cifraDaRisolvere = 0;
          sequenzaInserita = "";
          
          frequenzaAllarme = 1000; // Resetta il suono a un livello normale
          ultimoAumentoTempoMillis = millis(); // Fa partire il cronometro degli intervalli
          
          int comb = random(0, 8); // genera un numero casuale da 0 (binario 000) a 7 (binario 111)
          
          Serial.print("Combinazione binaria generata: ");
          for (int i = 0; i < 3; i++) {
            // scompone il numero intero ottenuto prima da random in bit
            // 2-i serve a leggere i bit da sinistra verso destra (invece del classico destra sinistra)
            // ed assegna uno per uno i bit ai vari led
            
            sfidaAttuale[i] = bitRead(comb, 2 - i);
            Serial.print(sfidaAttuale[i]);
          }
          Serial.println();

          // Animazione di avvio

          // fa accendere tutti i led ed emettere un piccolo bip dal buzzer per far capire che la sfida è iniziata
          for(int i=0; i<3; i++){ digitalWrite(LED_ROSSI[i], HIGH); tone(PIN_BUZZER, 800, 100); delay(150); }
          
          // lascia accesi/spegne i led, così da formare la combinazione morse
          for(int i=0; i<3; i++) digitalWrite(LED_ROSSI[i], sfidaAttuale[i]);
      }
  } else {
      // Quando scatta il minuto successivo a quello della sveglia, resetta il blocco
      // per permettere alla sveglia di suonare di nuovo il giorno dopo.
      attesaMinutoSuccessivo = false;
  }

  // GESTIONE SVEGLIA
  if (allarmeInCorso) {

    // Se sono passati 30 secondi (30000 millisecondi) dall'ultimo controllo, aumenta la frequenza
    if (millis() - ultimoAumentoTempoMillis >= 30000) {
      frequenzaAllarme = min(frequenzaAllarme + 200, 4000); // Cap a 4kHz
      ultimoAumentoTempoMillis = millis(); // Resetta il cronometro per i prossimi 30 secondi
      
      Serial.print("Passati 30 secondi! Nuova frequenza: ");
      Serial.println(frequenzaAllarme);
    }

    // qui facciamo suonare il buzzer in modo alternato
    // lo gestiamo con millis() perchè delay bloccherebbe il programma e quindi manderebbe in blocco anche la pagina web 
    if (millis() % 1000 < 200) tone(PIN_BUZZER, frequenzaAllarme); else noTone(PIN_BUZZER);

    if (digitalRead(PIN_PUNTO) == LOW) {
      sequenzaInserita += "."; 
      Serial.print(".");
      tone(PIN_BUZZER, 1200, 100);

      // logica anti rimbalzo, finchè teniamo premuto il pulsante risleva solamente 1 pressione
      // delay(150) ci serve per non far registrare al pulsante più di un tocco al suo rilascio 
      
      while (digitalRead(PIN_PUNTO) == LOW); delay(150);
    }
    if (digitalRead(PIN_LINEA) == LOW) {
      sequenzaInserita += "-"; 
      Serial.print("-");
      tone(PIN_BUZZER, 900, 200);
      // logica anti rimbalzo, uguale a quello del punto
      while (digitalRead(PIN_LINEA) == LOW); delay(150);
    }

    if (digitalRead(PIN_INVIO) == LOW) {
      delay(250);
      Serial.println(" [INVIO]");

      // qui prende il valore della sfida del led attuale, e controlla se sia un 1 o uno 0
      String target = (sfidaAttuale[cifraDaRisolvere] == 1) ? MORSE_1 : MORSE_0;

      if (sequenzaInserita == target) {
        Serial.println("Cifra Corretta!");
        digitalWrite(LED_BIANCHI[cifraDaRisolvere], HIGH);
        cifraDaRisolvere++;
        tone(PIN_BUZZER, 2000, 400);
        if (cifraDaRisolvere >= 3) {
          Serial.println("--- SVEGLIA RISOLTA! ---");
          allarmeInCorso = false;
          attesaMinutoSuccessivo = true; 
          noTone(PIN_BUZZER);
          delay(1000);
          for (int i = 0; i < 3; i++) { digitalWrite(LED_ROSSI[i], LOW); digitalWrite(LED_BIANCHI[i], LOW); }
        }
      } else {
        Serial.println("Codice Errato! Riprova.");
        frequenzaAllarme = min(frequenzaAllarme + 200, 4000); // Cap a 4kHz
        Serial.print("Errore! Nuova frequenza: ");
        Serial.println(frequenzaAllarme);
        digitalWrite(PIN_RGB_ROSSO, HIGH); tone(PIN_BUZZER, 150, 600); delay(600); digitalWrite(PIN_RGB_ROSSO, LOW);
      }
      sequenzaInserita = ""; 
      // logica anti rimbalzo, uguale a punto e linea
      while (digitalRead(PIN_INVIO) == LOW); delay(150);
    }
  }
}
