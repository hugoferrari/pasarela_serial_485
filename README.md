# Pasarela Serial RS485 — Medidor DLT645

Pasarela para **ESP32 Heltec WiFi LoRa 32 V2** que recibe tramas en formato hexadecimal por la consola serial (USB), las transmite por RS485 al medidor y devuelve la respuesta completa por la misma consola.

## Descripción general

El firmware actúa como un puente entre la PC y un medidor eléctrico que habla el protocolo **DLT645** por RS485:

1. Espera a que el usuario escriba una cadena hexadecimal en el monitor serie (con o sin espacios entre bytes).
2. Convierte esa cadena en bytes y la envía por RS485.
3. Muestra un eco de confirmación (`TX:`).
4. Espera la respuesta del medidor (hasta 2 segundos).
5. Muestra la trama recibida completa, incluyendo preámbulo (`RX:`).
6. Vuelve a quedar en espera de una nueva trama.

## Hardware requerido

| Componente | Descripción |
|---|---|
| Placa | ESP32 Heltec WiFi LoRa 32 V2 |
| Transceptor RS485 | Módulo MAX485, SN65HVD, o similar |
| Medidor | Medidor Timewave DTZY2397 trifásico con protocolo DLT645 |
| Cableado | Par trenzado para bus RS485 (A/B) |

## Conexiones de pines

### ESP32 ↔ Módulo RS485

| ESP32 (GPIO) | Módulo RS485 | Función |
|---|---|---|
| GPIO 14 | DI (TX del módulo) | Transmisión hacia el bus RS485 |
| GPIO 27 | RO (RX del módulo) | Recepción desde el bus RS485 |
| GPIO 25 | DE + RE (unidos) | Control de dirección (TX/RX) |
| 3.3 V | VCC | Alimentación del módulo |
| GND | GND | Tierra común |

### Módulo RS485 ↔ Medidor

| Módulo RS485 | Medidor |
|---|---|
| A (+) | Terminal A del medidor |
| B (−) | Terminal B del medidor |

> **Nota:** Si no hay comunicación, probá invertir A y B en el bus RS485. Algunos fabricantes etiquetan los terminales de forma distinta (A/B vs D+/D−).

### Control de dirección RS485

El pin `GPIO 25` controla el modo del transceptor:

- **HIGH** → modo transmisión (envía datos al medidor).
- **LOW** → modo recepción (escucha la respuesta del medidor).

Esto asume que los pines **DE** y **RE** del módulo RS485 están unidos entre sí. Si tu módulo los tiene separados, conectá ambos al GPIO 25.

### Consola serial (USB)

La comunicación con la PC se realiza por el puerto USB nativo del ESP32:

- **Velocidad:** 115200 baudios
- **Formato:** 8N1 (8 bits, sin paridad, 1 bit de parada)
- Compatible con el Monitor Serie del IDE de Arduino.

## Configuración del firmware

Los parámetros principales se definen al inicio del archivo `.ino`:

```cpp
#define RS485_SPEED 2400                // Velocidad del medidor (DLT645)
#define SERIAL_SPEED 115200             // Velocidad de la consola USB

#define RS485_CONTROL_PIN 25            // Pin DE/RE del transceptor
#define RS485_RX_PIN 27                 // Pin RX hacia el módulo RS485
#define RS485_TX_PIN 14                 // Pin TX hacia el módulo RS485
```

| Parámetro | Valor | Descripción |
|---|---|---|
| `RS485_SPEED` | 2400 | Velocidad estándar del protocolo DLT645 |
| `SERIAL_SPEED` | 115200 | Velocidad del monitor serie por USB |
| Formato RS485 | 8E1 | 8 bits, paridad par, 1 bit de parada |
| Timeout de respuesta | 2000 ms | Tiempo máximo de espera por trama RX |

## Cómo funciona el código

### Arquitectura de dos UART

El ESP32 usa dos puertos serie independientes:

| Puerto | Uso | Velocidad | Formato |
|---|---|---|---|
| `Serial` (UART0) | Consola USB con la PC | 115200 | 8N1 |
| `RS485` (UART1) | Bus RS485 con el medidor | 2400 | 8E1 |

Esta separación permite usar el Monitor Serie del IDE (8N1) sin interferir con la comunicación del medidor (8E1).

### Flujo de ejecución

```
┌─────────────┐      ┌──────────────┐     ┌─────────────┐      ┌──────────────┐
│  Esperar    │────▶│  Enviar por  │────▶│  Recibir    │────▶│  Mostrar RX  │
│  trama hex  │      │  RS485 (TX:) │     │  respuesta  │      │  y esperar   │
│  por Serial │      │              │     │  por RS485  │      │  de nuevo    │
└─────────────┘      └──────────────┘     └─────────────┘      └──────────────┘
```

### Funciones principales

| Función | Descripción |
|---|---|
| `leerCadenaHex()` | Acumula caracteres desde `Serial` entre iteraciones de `loop()`. Detecta fin de línea por `\n`/`\r` o por 300 ms de inactividad, y delega el parseo a `parsearHex()`. |
| `parsearHex()` | Convierte la cadena hexadecimal en bytes. Acepta ambos formatos (con o sin espacios) y detecta automáticamente cuál se usó para responder en el mismo estilo. |
| `hexVal()` | Convierte un carácter hexadecimal (`0-9`, `A-F`, `a-f`) a su valor numérico (0–15). |
| `enviarTrama()` | Activa el pin de control RS485, envía los bytes por `RS485`, desactiva el pin y muestra el eco `TX:` por `Serial`. |
| `recibirTrama()` | Escucha por `RS485` hasta 2 segundos. Captura desde el primer byte `0xFE` (preámbulo) o `0x68` hasta el byte final `0x16`. |
| `imprimirTrama()` | Imprime una trama en hexadecimal mayúscula. Usa espacios entre bytes solo si la entrada los tenía. |

### Captura de la respuesta

La recepción comienza al detectar:

- `0xFE` — byte de preámbulo (típico en DLT645, puede repetirse varias veces).
- `0x68` — byte de inicio de trama (si no hay preámbulo).

La captura finaliza al recibir `0x16` (byte de fin de trama). La respuesta mostrada incluye el preámbulo completo.

## Guía de uso

### 1. Cargar el firmware

1. Abrí el proyecto en el IDE de Arduino o PlatformIO.
2. Seleccioná la placa **Heltec WiFi LoRa 32 (V2)**.
3. Compilá y subí el firmware al ESP32.

### 2. Abrir el monitor serie

- **Velocidad:** 115200
- **Fin de línea:** cualquiera funciona (`Nueva línea`, `Retorno de carro`, o `Sin fin de línea`)

Al iniciar deberías ver:

```
Pasarela serial RS485 iniciada
Esperando trama (hex)...
```

### 3. Enviar una trama

Escribí una cadena hexadecimal y presioná Enter (o esperá 300 ms sin escribir). Ambos formatos son válidos:

**Con espacios:**

```
FE FE FE FE 68 35 29 00 26 20 01 68 11 04 34 45 93 37 CD 16
```

**Sin espacios:**

```
FEFEFEFE6835290026200168110434459337CD16
```

El firmware detecta automáticamente el formato de entrada y responde en el mismo estilo: si enviás con espacios, `TX:` y `RX:` se muestran con espacios; si enviás sin espacios, la salida también va sin espacios.

### 4. Leer la respuesta

El firmware mostrará el eco de lo enviado y la respuesta del medidor. Ejemplo con entrada con espacios:

```
TX: FE FE FE FE 68 35 29 00 26 20 01 68 11 04 34 45 93 37 CD 16
RX: FE FE FE FE 68 35 29 00 26 20 01 68 91 13 34 45 93 37 7C 55 33 33 33 33 33 33 33 33 33 33 33 33 33 C4 16
Esperando trama (hex)...
```

Ejemplo con entrada sin espacios:

```
TX: FEFEFEFE6835290026200168110434459337CD16
RX: FEFEFEFE68352900262001689113344593337C553333333333333333333333C416
Esperando trama (hex)...
```

Si el medidor no responde en 2 segundos:

```
TX: FE FE FE FE 68 35 29 00 26 20 01 68 11 04 34 45 93 37 CD 16
Timeout
Esperando trama (hex)...
```

### 5. Enviar otra trama

Simplemente escribí una nueva cadena hexadecimal. No hace falta reiniciar el ESP32.

## Ejemplos de tramas

### Lectura de energía activa total (DI: 00 01 00 00)

Solicitud (con espacios):

```
FE FE FE FE 68 35 29 00 26 20 01 68 11 04 33 34 33 37 CD 16
```

Equivalente sin espacios:

```
FEFEFEFE6835290026200168110433343337CD16
```

### Lectura con dirección de medidor específica

Reemplazá los 6 bytes de dirección (después del primer `68`) por la dirección de tu medidor. La dirección en DLT645 se envía en orden inverso (LSB primero).

Ejemplo con dirección `012026002935`:

```
FE FE FE FE 68 35 29 00 26 20 01 68 11 04 34 45 93 37 CD 16
```

o:

```
FEFEFEFE6835290026200168110434459337CD16
```

> Los bytes `35 29 00 26 20 01` corresponden a la dirección `012026002935` en formato DLT645 (orden inverso).

## Solución de problemas

| Síntoma | Posible causa | Solución |
|---|---|---|
| No aparece `TX:` al escribir | Monitor serie a velocidad incorrecta | Verificá que esté en **115200** |
| Aparece `TX:` pero no `RX:` | Medidor no responde o cableado incorrecto | Revisá conexiones A/B, alimentación del medidor y dirección en la trama |
| Aparece `Timeout` | Velocidad o paridad incorrecta en RS485 | Confirmá que el medidor use **2400 8E1** |
| Respuesta corrupta o incompleta | Bus RS485 sin terminación o cable largo | Agregá resistencia de terminación de 120 Ω entre A y B en los extremos del bus |
| `TX:` con bytes incorrectos o incompletos | Formato de entrada inválido | Usá pares hex de 2 dígitos (`FE`, no `F` ni `0xFE`). Con o sin espacios entre bytes. |
| `TX:` muestra solo el primer byte | Cadena recibida incompleta | Pegá la trama completa y presioná Enter, o esperá 300 ms tras el último carácter |
| Sin comunicación tras conectar | Terminales A/B invertidos | Intercambiá los cables A y B del bus RS485 |

## Estructura del proyecto

```
pasarela_serial_485_heltec/
├── pasarela_serial_485_heltec.ino   # Firmware principal
└── README.md                        # Este archivo
```

## Licencia

Uso libre para proyectos personales y comerciales.
