/*
Three-phase meter reading - 1 - obtain current and voltage data - Issue command
FE FE FE FE 68 35 29 00 26 20 01 68 11 04 34 45 93 37 CD 16
Three-phase meter reading - 2 - obtain active power and power factor data - Issue command
FE FE FE FE 68 35 29 00 26 20 01 68 11 04 35 45 93 37 CE 16
Three-phase meter reading - 3 - obtain electricity consumption data - Issue command
FE FE FE FE 68 35 29 00 26 20 01 68 11 04 33 32 33 33 55 16
Three-phase meter reading - 4 - obtain reactive power energy 1 - Issue command
FE FE FE FE 68 35 29 00 26 20 01 68 11 04 34 63 93 37 EB 16
Three-phase meter reading - 5 - obtain reactive power energy 2 - Issue command
FE FE FE FE 68 35 29 00 26 20 01 68 11 04 35 63 93 37 EC 16
Cut off
FEFEFEFE6835290026200168140D2122333A3533333363636363CC6C16
Cut On
FEFEFEFE6835290026200168140D2122333A3533333363636363882816
*/

#define RS485_SPEED 2400                // Velocidad de comunicación para RS485
#define SERIAL_SPEED 115200             // Velocidad de comunicación para el serial

#define RS485_CONTROL_PIN 25            // Pin de control para RS485
#define RS485_RX_PIN 27                 // Pin de recepción para RS485
#define RS485_TX_PIN 14                 // Pin de transmisión para RS485

HardwareSerial RS485(1);

byte txBuffer[120];
int txLength = 0;
bool hexConEspacios = false;

byte buffer[120];
int frameLength = 0;

bool parsearHex(char *cadena, int len);
int hexVal(char c);

void setup() {

  pinMode(RS485_CONTROL_PIN, OUTPUT);
  digitalWrite(RS485_CONTROL_PIN, LOW);

  Serial.begin(SERIAL_SPEED);
  RS485.begin(RS485_SPEED, SERIAL_8E1, RS485_RX_PIN, RS485_TX_PIN);

  delay(1000);
  Serial.println("Pasarela serial RS485 iniciada");
  Serial.println("Esperando trama (hex)...");
}

void loop() {

  if (leerCadenaHex()) {

    enviarTrama();

    if (recibirTrama()) {
      Serial.print("RX: ");
      imprimirTrama(buffer, frameLength);
    }

    Serial.println("Esperando trama (hex)...");
  }
}

bool leerCadenaHex() {

  static char cadena[400];
  static int idx = 0;
  static unsigned long ultimoLectura = 0;

  const unsigned long TIMEOUT_MS = 300;

  while (Serial.available()) {

    char c = Serial.read();
    ultimoLectura = millis();

    if (c == '\n' || c == '\r') {

      if (idx == 0)
        continue;

      cadena[idx] = '\0';
      int len = idx;
      idx = 0;
      return parsearHex(cadena, len);
    }

    if (idx < (int)sizeof(cadena) - 1)
      cadena[idx++] = c;
  }

  if (idx > 0 && ultimoLectura > 0 && millis() - ultimoLectura >= TIMEOUT_MS) {

    cadena[idx] = '\0';
    int len = idx;
    idx = 0;
    ultimoLectura = 0;
    return parsearHex(cadena, len);
  }

  return false;
}

bool parsearHex(char *cadena, int len) {

  hexConEspacios = false;

  for (int i = 0; i < len; i++) {
    if (cadena[i] == ' ' || cadena[i] == '\t') {
      hexConEspacios = true;
      break;
    }
  }

  txLength = 0;

  for (int i = 0; i < len && txLength < (int)sizeof(txBuffer); ) {

    while (i < len && (cadena[i] == ' ' || cadena[i] == '\t'))
      i++;

    if (i + 1 >= len)
      break;

    int alto = hexVal(cadena[i]);
    int bajo = hexVal(cadena[i + 1]);

    if (alto < 0 || bajo < 0) {
      i++;
      continue;
    }

    txBuffer[txLength++] = (alto << 4) | bajo;
    i += 2;
  }

  return txLength > 0;
}

int hexVal(char c) {

  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;

  return -1;
}

void enviarTrama() {

  digitalWrite(RS485_CONTROL_PIN, HIGH);

  RS485.write(txBuffer, txLength);
  RS485.flush();

  digitalWrite(RS485_CONTROL_PIN, LOW);

  Serial.print("TX: ");
  imprimirTrama(txBuffer, txLength);
}

bool recibirTrama() {

  unsigned long timeout = millis();
  bool start = false;

  frameLength = 0;

  while (millis() - timeout < 2000) {

    if (RS485.available()) {

      byte b = RS485.read();

      if (!start) {

        if (b == 0xFE || b == 0x68) {
          start = true;
          buffer[frameLength++] = b;
        }

      } else {

        buffer[frameLength++] = b;

        if (b == 0x16)
          return true;

        if (frameLength >= sizeof(buffer))
          return false;
      }
    }
  }

  Serial.println("Timeout");
  return false;
}

void imprimirTrama(byte *frame, int len) {

  for (int i = 0; i < len; i++) {

    imprimirByteHex(frame[i]);

    if (hexConEspacios && i < len - 1)
      Serial.print(" ");
  }

  Serial.println();
}

void imprimirByteHex(byte b) {

  const char hex[] = "0123456789ABCDEF";

  Serial.print(hex[b >> 4]);
  Serial.print(hex[b & 0x0F]);
}
