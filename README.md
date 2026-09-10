# Sistemas Basados en Computador (UPM) - Hito 1: Control de Frecuencia de Parpadeo

## Metadatos del Proyecto
* **Identificador de Grupo:** `SBC26T06`
* **Plataforma Hardware:** ESP32-WROOM-32 (Xtensa Dual-Core 32-bit LX6)
* **Framework y Entorno:** ESP-IDF v5.x / FreeRTOS Kernel
* **Lenguaje:** C (C99 / C11)
* **Repositorio GitHub:** [Enlace al repositorio](https://github.com/...)
* **Espacio de Trabajo SharePoint:** [Directorio del Grupo en SharePoint](https://upm365.sharepoint.com/sites/SBC26T06)
* **Vídeo Demostrativo:** [Enlace a la demostración (< 1 min)](https://upm365.sharepoint.com/sites/SBC26T06/...)

---

## 1. Descripción General
Este proyecto implementa el control de cadencia de un diodo LED mediante un conmutador mecánico de dos posiciones, abordando el problema desde dos paradigmas arquitectónicos distintos para sistemas empotrados:

1. **Variante Básica (`Hito1_Basico`):** Gobernado mediante sondeo secuencial periódico (*polling*) dentro de la tarea principal de usuario[cite: 2].
2. **Variante Avanzada (`Hito1_Avanzado`):** Arquitectura reactiva dirigida por interrupciones de hardware (*ISR*) en memoria interna `IRAM`, sincronización entre procesos mediante colas FreeRTOS (`xQueue`) y separación de responsabilidades multihilo[cite: 2].

Ambas variantes generan una onda cuadrada simétrica (Duty Cycle del 50%) conmutando entre **1.0 Hz** ($T = 1000\text{ ms}$) y **2.0 Hz** ($T = 500\text{ ms}$)[cite: 2].

---

## 2. Especificación de Hardware y Conexiones (BOM)

| Componente | Pin ESP32 | Modo GPIO | Configuración Eléctrica | Descripción Funcional |
| :--- | :--- | :--- | :--- | :--- |
| **LED Indicador** | `GPIO 18` | Salida (`OUTPUT`) | Resistencia externa $220\ \Omega$ en serie a GND | Actuador luminoso (ON: 3.3 V, OFF: 0 V) |
| **Slide Switch** | `GPIO 19` | Entrada (`INPUT`) | Resistencia interna `Pull-Up` (~45 kΩ) activa | Conmutador de frecuencia (Activo a GND) |
| **GND Común** | `GND` | Referencia | Plano de masa compartido | Cátodo del LED y terminal de conmutación |

### Lógica de Polarización
* **Switch Abierto (Reposo):** Polarizado a $3.3\text{ V}$ mediante Pull-Up interno $\rightarrow$ Nivel Lógico `1` $\rightarrow$ **1.0 Hz** (Periodo: 1000 ms, 500 ms ON / 500 ms OFF).
* **Switch Cerrado (Activo):** Cortocircuitado a GND $\rightarrow$ Nivel Lógico `0` $\rightarrow$ **2.0 Hz** (Periodo: 500 ms, 250 ms ON / 250 ms OFF).

---

## 3. Estructura del Árbol de Directorios

El repositorio cumple de forma estricta con la rúbrica de empaquetado de la UPM, coexistiendo ambas variantes bajo la carpeta raíz del grupo y excluyendo cualquier archivo residual de compilación (`build/`):

```text
Hito1_SBC26T06/
├── README.md
├── Hito1_Basico/
│   ├── CMakeLists.txt
│   └── main/
│       ├── CMakeLists.txt
│       └── main.c
└── Hito1_Avanzado/
    ├── CMakeLists.txt
    └── main/
        ├── CMakeLists.txt
        └── main.c
```

---

## 4. Compilación, Flasheo y Monitorización Independiente

Dado que la carpeta raíz `Hito1_SBC26T06` actúa como un contenedor de entrega para que ambas soluciones coexistan en el mismo directorio, cada variante constituye un proyecto de CMake autónomo. Deben compilarse y flashearse de forma independiente mediante cualquiera de los dos métodos siguientes:

---

### Método 1: Terminal ESP-IDF (Recomendado - CLI)

Abra la terminal integrada en la raíz del espacio de trabajo y acceda al directorio de la variante que desee evaluar:

#### Opción A: Ejecución de la Variante Básica (`Hito1_Basico` - Polling)
```bash
# 1. Navegar al proyecto básico
cd Hito1_Basico

# 2. Establecer el target del SoC (solo la primera vez)
idf.py set-target esp32

# 3. Compilar, flashear en el dispositivo y abrir el monitor serie
idf.py build flash monitor
```

#### Opción B: Ejecución de la Variante Avanzada (`Hito1_Avanzado` - ISR + FreeRTOS)

```bash
# 1. Navegar al proyecto avanzado (desde la raíz del repositorio)
cd Hito1_Avanzado

# 2. Establecer el target del SoC (solo la primera vez)
idf.py set-target esp32

# 3. Compilar, flashear en el dispositivo y abrir el monitor serie
idf.py build flash monitor
```

---

### 🛠️ Comandos Útiles y Operaciones de Mantenimiento

#### 1. Configuración del Hardware y Target
```bash
# Establecer el microcontrolador objetivo (ESP32 SoC) - Obligatorio la 1ª vez
idf.py set-target esp32

# Abrir el menú de configuración Kconfig (FreeRTOS, Drivers, Log level)
idf.py menuconfig
```

#### 2. Compilación, Flasheo y Monitorización
```bash
# Compilar el código fuente sin flashear (análisis estático y de sintaxis)
idf.py build

# Compilar, flashear con autodetección de puerto y abrir el monitor serie UART
idf.py build flash monitor

# Forzar puerto serie manualmente (Linux)
idf.py -p /dev/ttyUSB0 flash monitor

# Forzar puerto serie manualmente (Windows)
idf.py -p COM3 flash monitor
```
> **Control UART:** Pulsa `Ctrl + ]` para salir de la monitorización serie sin detener la ejecución en el ESP32.

#### 3. Ejecución desde la Raíz del Repositorio (Flag `-C`)
```bash
# Variante Básica (Polling)
idf.py -C Hito1_Basico build flash monitor

# Variante Avanzada (ISR + FreeRTOS)
idf.py -C Hito1_Avanzado build flash monitor
```

#### 4. Diagnóstico de Memoria y Periféricos
```bash
# Inspección de consumo estático de memoria Flash y RAM interna (DRAM / IRAM)
idf.py size

# Desglose de uso de memoria por componentes (FreeRTOS, Drivers, App)
idf.py size-components

# Borrado completo de la memoria Flash física (resuelve particiones o NVS corrupto)
idf.py erase-flash
```

#### 5. Auditoría y Limpieza Pre-Entrega (Obligatorio Rúbrica UPM)
```bash
# Limpiar binarios y eliminar la carpeta build/ del subproyecto actual
idf.py fullclean

# Limpiar ambas variantes desde la raíz en un solo paso
idf.py -C Hito1_Basico fullclean
idf.py -C Hito1_Avanzado fullclean

# Purga exhaustiva de artefactos residuales y configuraciones locales
rm -rf Hito1_Basico/build Hito1_Avanzado/build
rm -f Hito1_Basico/sdkconfig* Hito1_Avanzado/sdkconfig*
```

> **Aviso de Calificación:** Comprueba que no existan carpetas `build/` en ninguno de los directorios antes de generar el archivo `Hito1_SBC26T06.zip` para su subida a SharePoint.