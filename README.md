# ThalosPruebasPCB

Firmware de prueba para una PCB con **STM32WBA55CGU6**: lectura de tres
**VL6180X**, una matriz termica **AMG8831** de 8 x 8 pixeles y un LED en PB14.
Utiliza HAL de STM32 y **FreeRTOS con CMSIS-RTOS2**. Incluye un programa para
usar una **ESP32-WROOM-32** como puente UART a USB y observar las lecturas.

## Conexiones del STM32

Alimentar los modulos de sensores por su entrada **VIN a 3.3 V**, con GND comun.
Los cuatro sensores comparten I2C1: sus SCL se unen a PA15 y sus SDA a PB3.

| Funcion | Pin STM32 | Conexion |
| --- | --- | --- |
| I2C1 SCL | PA15 | SCL de los tres VL y la AMG |
| I2C1 SDA | PB3 | SDA de los tres VL y la AMG |
| VL centro / frente | PB0 | SHDN del VL frontal |
| VL izquierdo | PB1 | SHDN del VL izquierdo |
| VL derecho | PB2 | SHDN del VL derecho |
| LED de prueba | PB14 | Resistencia de 330 ohm, anodo del LED; catodo a GND |
| USART1 TX | PB12 | RX del receptor serial |
| USART1 RX | PA8 | Opcional; no se necesita para observar las impresiones |
| Programacion SWD | PA13 / PA14 | SWDIO / SWCLK del ST-LINK, con GND comun |

En la AMG8831, AD0 va a GND para usar la direccion `0x68`; INT queda libre.
En los modulos VL6180X, GPIO y 2v8 quedan libres. Cada SHDN tiene su propio
GPIO, configurado como salida open drain con nivel inicial bajo.

Al arrancar, el firmware habilita un VL a la vez y asigna las direcciones
`0x30` (centro), `0x31` (izquierdo) y `0x32` (derecho).

## Tareas y salida

| Tarea | Funcion |
| --- | --- |
| `tofTask` / `StartTofTask` | Inicializa y lee los tres VL por polling; imprime distancias validas en milimetros |
| `thermalTask` / `StartThermalTask` | Lee los 64 pixeles y muestra la matriz en grados Celsius |
| `heartbeatTask` / `StartHeartbeatTask` | Cambia el estado del LED cada 500 ms |

`i2c1Mutex` protege las transferencias del bus compartido y `telemetryMutex`
protege las impresiones por USART1. La salida serial utiliza **115200 baudios,
8N1, sin control de flujo**. Una lectura de distancia no valida no se imprime;
los errores de comunicacion se muestran aparte.

El LED se enciende antes de iniciar el planificador y luego parpadea desde la
tarea de heartbeat. El parpadeo permite comprobar la ejecucion de FreeRTOS;
las mediciones se comprueban en la terminal serial.

## Compilar y cargar el STM32 desde macOS

Requisitos: Python 3, GNU Make y las herramientas GCC ARM y STM32CubeProgrammer
incluidas en STM32CubeIDE. `tools/find_tool.py` localiza esas herramientas en
PATH o dentro de `/Applications/STM32CubeIDE.app`; no instala herramientas.

```sh
git clone https://github.com/LeoDuffed/ThalosPruebasPCB.git
cd ThalosPruebasPCB
make -j8
make probe
make connect
make flash
```

`make flash` compila, carga el ELF por SWD, verifica y reinicia el STM32.
Los resultados ELF, BIN, HEX y MAP quedan en `build/`. No hace falta conservar
las carpetas Debug o Release de CubeIDE para compilar con el Makefile.

```sh
make run          # Reiniciar el STM32
make size         # Ver uso de memoria
make clean        # Borrar solamente build/
make help         # Mostrar comandos
```

La conexion predeterminada usa reset por software. Con NRST conectado al
ST-LINK se puede usar conexion bajo reset:

```sh
make flash CONNECT_MODE=UR RESET=HWrst SWD_FREQ=100
```

Con varios ST-LINK conectados, seleccionar uno con `SERIAL=numero_del_STLINK`.
Si las herramientas estan en otro lugar, definir `TOOLCHAIN_BIN` y
`CUBE_PROGRAMMER_BIN` con las rutas de sus directorios bin.

## Ver las mediciones mediante la ESP32

El ST-LINK V2 se usa para programar el STM32. La salida de las mediciones sale
por PB12; el programa de `esp32_serial_bridge/` la recibe y la reenvia al USB.
Esta prueba corresponde a una ESP32-WROOM-32, entorno PlatformIO `esp32dev`.

| STM32 | ESP32 |
| --- | --- |
| PB12 (USART1 TX) | GPIO16 (RX2) |
| GND | GND |

Alimentar ambas placas. Para esta prueba no hace falta conectar PA8 del STM32
ni GPIO17 de la ESP32. El puente no filtra mensajes: muestra distancias,
matrices y errores. Si no recibe bytes durante tres segundos, muestra un aviso
`[ESP32] Sin datos del STM32 por GPIO16...`.

Requiere PlatformIO Core. El Makefile usa por defecto
`~/.platformio/penv/bin/platformio`; se puede cambiar con `PIO=/ruta/platformio`.
El puerto predeterminado es `/dev/cu.usbserial-0001`; comprobarlo con
`ls /dev/cu.*` y reemplazarlo si cambia.

```sh
make esp32-build
# El siguiente comando sustituye el programa actual de la ESP32 por el puente.
make esp32-flash ESP_PORT=/dev/cu.usbserial-0001
make serial ESP_PORT=/dev/cu.usbserial-0001
```

Con el monitor abierto, pulsar RESET en el STM32 para ver el arranque. Cerrar
el monitor con Ctrl+C antes de volver a cargar la ESP32. Alternativamente,
se puede abrir el mismo puerto con `screen /dev/cu.usbserial-0001 115200`.

## STM32CubeMX y STM32CubeIDE

Abrir `ThalosPruebasPCB.ioc` para editar el hardware e importar `.project` en
STM32CubeIDE. La configuracion usa TrustZone deshabilitado, X-CUBE-FREERTOS
1.6.0, heap_4 de 16 KiB, tick de 1000 Hz, HAL timebase TIM17 e ICACHE 1-way.

`main.c` inicializa los perifericos y arranca el planificador. Las tareas y sus
funciones auxiliares estan en `Core/Src/app_freertos.c`; los drivers estan en
`vl6180x.c` y `amg88xx.c`, con sus encabezados en `Core/Inc/`.

Antes de regenerar, abrir la version actualizada del `.ioc` desde disco y
conservar los nombres `tofTask`, `thermalTask` y `heartbeatTask`. El codigo
propio de las tareas queda dentro de bloques `USER CODE` para conservarlo al
regenerar. El nombre frontal del GPIO es `VL_FRONT`.

## Validacion y componentes incluidos

Se comprobaron la compilacion del STM32 con Make/GCC y la del puente ESP32 con
PlatformIO. Se verifico en la PCB el encendido del LED PB14; el parpadeo y las
lecturas de esta version deben comprobarse con el montaje y la terminal serial.

El repositorio incluye los drivers HAL, CMSIS y FreeRTOS necesarios para la
compilacion, con sus avisos y archivos de licencia originales en `Drivers/` y
`Middlewares/`. Las carpetas de compilacion y la configuracion privada local
se excluyen mediante `.gitignore`.
