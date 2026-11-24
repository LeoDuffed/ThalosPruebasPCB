# STM32WBA55CGU6: tres VL6180X + AMG8831 + FreeRTOS CMSIS-RTOS2.
# Compilacion independiente de los archivos generados en Debug/ y Release/.
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

PYTHON ?= python3
CC := $(shell "$(PYTHON)" tools/find_tool.py gcc)
OBJCOPY := $(shell "$(PYTHON)" tools/find_tool.py objcopy)
SIZE := $(shell "$(PYTHON)" tools/find_tool.py size)
PROGRAMMER = $(shell "$(PYTHON)" tools/find_tool.py programmer)

BUILD := build
TARGET := $(BUILD)/ThalosPruebasPCB
LINKER_SCRIPT := STM32WBA55CGUX_FLASH.ld
FREERTOS := Middlewares/Third_Party/FreeRTOS/Source
PORT := $(FREERTOS)/portable/GCC/ARM_CM33_NTZ/non_secure
HAL := Drivers/STM32WBAxx_HAL_Driver

CPU := -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
CPPFLAGS += -DDEBUG -DUSE_HAL_DRIVER -DSTM32WBA55xx \
    -ICore/Inc -I$(HAL)/Inc -I$(HAL)/Inc/Legacy \
    -IDrivers/CMSIS/Device/ST/STM32WBAxx/Include -IDrivers/CMSIS/Include \
    -I$(FREERTOS)/include -I$(PORT) -I$(FREERTOS)/CMSIS_RTOS_V2 \
    -IMiddlewares/Third_Party/CMSIS/RTOS2/Include
CFLAGS += $(CPU) -std=gnu11 -O0 -g3 -Wall \
    -ffunction-sections -fdata-sections -fstack-usage \
    -MMD -MP --specs=nano.specs
ASFLAGS += $(CPU) -g3 -x assembler-with-cpp -MMD -MP --specs=nano.specs
LDFLAGS += $(CPU) -T$(LINKER_SCRIPT) \
    --specs=nosys.specs --specs=nano.specs -static \
    -Wl,--gc-sections -Wl,-Map=$(TARGET).map
LDLIBS += -Wl,--start-group -lc -lm -Wl,--end-group

C_SOURCES := $(wildcard Core/Src/*.c) $(wildcard $(HAL)/Src/*.c) \
    $(wildcard $(FREERTOS)/*.c) \
    $(FREERTOS)/CMSIS_RTOS_V2/cmsis_os2.c \
    $(PORT)/port.c $(PORT)/portasm.c \
    $(FREERTOS)/portable/MemMang/heap_4.c
ASM_SOURCES := $(wildcard Core/Startup/*.s)
OBJECTS := $(addprefix $(BUILD)/,$(C_SOURCES:.c=.o)) \
    $(addprefix $(BUILD)/,$(ASM_SOURCES:.s=.o))
DEPS := $(OBJECTS:.o=.d)

# Conexion con cuatro cables: SWDIO, SWCLK, GND y referencia/alimentacion.
# Si se conecta tambien NRST: CONNECT_MODE=UR RESET=HWrst.
SWD_FREQ ?= 1000
CONNECT_MODE ?= NORMAL
RESET ?= SWrst
SERIAL ?=
CONNECT = port=SWD freq=$(SWD_FREQ) mode=$(CONNECT_MODE) reset=$(RESET) $(if $(SERIAL),sn=$(SERIAL))

.PHONY: all flash probe connect run size clean help check-toolchain check-programmer
all: $(TARGET).elf $(TARGET).bin $(TARGET).hex

check-toolchain:
	@test -n "$(CC)" -a -x "$(CC)" || { echo "No encuentro GCC de ARM. Revisa STM32CubeIDE o TOOLCHAIN_BIN." >&2; exit 1; }
	@test -n "$(OBJCOPY)" -a -x "$(OBJCOPY)" || { echo "No encuentro arm-none-eabi-objcopy." >&2; exit 1; }
	@test -n "$(SIZE)" -a -x "$(SIZE)" || { echo "No encuentro arm-none-eabi-size." >&2; exit 1; }

check-programmer:
	@test -n "$(PROGRAMMER)" -a -x "$(PROGRAMMER)" || { echo "No encuentro STM32_Programmer_CLI. Revisa CubeIDE o CUBE_PROGRAMMER_BIN." >&2; exit 1; }

$(OBJECTS): | check-toolchain

$(BUILD)/%.o: %.c Makefile
	@mkdir -p "$(@D)"
	"$(CC)" $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(BUILD)/%.o: %.s Makefile
	@mkdir -p "$(@D)"
	"$(CC)" $(CPPFLAGS) $(ASFLAGS) -c "$<" -o "$@"

$(TARGET).elf: $(OBJECTS) $(LINKER_SCRIPT) Makefile | check-toolchain
	"$(CC)" $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o "$@"
	"$(SIZE)" "$@"

$(TARGET).bin: $(TARGET).elf | check-toolchain
	"$(OBJCOPY)" -O binary "$<" "$@"

$(TARGET).hex: $(TARGET).elf | check-toolchain
	"$(OBJCOPY)" -O ihex "$<" "$@"

probe: check-programmer
	"$(PROGRAMMER)" -l st-link

connect: check-programmer
	"$(PROGRAMMER)" -c $(CONNECT)

flash: all check-programmer
	"$(PROGRAMMER)" -c $(CONNECT) -w "$(TARGET).elf" -v -rst

run: check-programmer
	"$(PROGRAMMER)" -c $(CONNECT) -rst

size: $(TARGET).elf
	"$(SIZE)" "$<"

clean:
	rm -rf build

help:
	@echo "make -j8      Compilar el firmware (ELF, BIN y HEX en build/)"
	@echo "make probe    Detectar ST-LINK por USB"
	@echo "make connect  Comprobar conexion SWD con el STM32"
	@echo "make flash    Compilar, cargar, verificar y reiniciar"
	@echo "make run      Reiniciar el STM32"
	@echo "make esp32-build  Compilar el puente serial de la ESP32"
	@echo "make esp32-flash  Cargar el puente (reemplaza el firmware de la ESP32)"
	@echo "make serial      Ver la salida USB de la ESP32 a 115200"
	@echo "Puerto ESP32: ESP_PORT=/dev/cu.usbserial-0001; salir del monitor: Ctrl+C"
	@echo "make size     Ver uso de flash y RAM"
	@echo "make clean    Borrar solamente build/"
	@echo "Opciones: SWD_FREQ=100 | SERIAL=numero_del_STLINK"
	@echo "Con NRST conectado: CONNECT_MODE=UR RESET=HWrst"
	@echo "Requiere python3 y las herramientas ya instaladas con CubeIDE."

# ESP32-WROOM-32: puente de recepcion UART2 -> USB.
PIO ?= $(HOME)/.platformio/penv/bin/platformio
ESP_PORT ?= /dev/cu.usbserial-0001

.PHONY: esp32-build esp32-flash serial check-platformio
check-platformio:
	@test -x "$(PIO)" || { echo "No encuentro PlatformIO. Indica PIO=/ruta/al/platformio." >&2; exit 1; }

esp32-build: check-platformio
	"$(PIO)" run --project-dir esp32_serial_bridge

# Reemplaza el firmware actual de la ESP32 por el puente serial.
esp32-flash: check-platformio
	"$(PIO)" run --project-dir esp32_serial_bridge --target upload --upload-port "$(ESP_PORT)"

serial: check-platformio
	"$(PIO)" device monitor --project-dir esp32_serial_bridge --port "$(ESP_PORT)" --baud 115200 --dtr 0 --rts 0

-include $(DEPS)
