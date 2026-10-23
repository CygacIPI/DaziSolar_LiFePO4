# App LiFePO4

Aplicacion de escritorio para monitorizar baterias LiFePO4. La Fase 1 se desarrolla en PC con C11; la interfaz y los transportes no deben acoplarse al futuro nucleo portable. La app sera de solo lectura y no se conectara a un BMS hasta disponer de documentacion confirmada del protocolo.

## Sprint 0: Windows 10/11

### Instalar herramientas

1. Instala MSYS2 desde [msys2.org](https://www.msys2.org/).
2. Abre **MSYS2 UCRT64** desde el menu Inicio y actualiza los paquetes:

   ```bash
   pacman -Syu
   ```

   Si la terminal se cierra o pide reiniciar, vuelve a abrir **MSYS2 UCRT64** y repite el comando hasta que no queden actualizaciones pendientes.
3. Instala compilador, CMake, Ninja y raylib:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-raylib
   ```
4. Comprueba que se encuentran las herramientas:

   ```bash
   echo $MSYSTEM
   which gcc
   which cmake
   gcc --version
   cmake --version
   ninja --version
   ```

   `echo $MSYSTEM` debe mostrar `UCRT64`. Las rutas de `gcc` y `cmake` deben empezar por `/ucrt64/bin/`. Si `cmake` no aparece, confirma que estás en **MSYS2 UCRT64** y ejecuta de nuevo:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
   ```

   Espera a que `pacman` termine sin errores y vuelve a comprobar `which cmake`. No ejecutes los comandos de compilación hasta que `cmake --version` funcione.

### Configurar, compilar y probar

En la misma terminal **MSYS2 UCRT64**, entra en la carpeta del repositorio. Para esta instalacion, ejecuta:

```bash
cd /c/Users/DELL/Documents/DaziSolar_LiFePO4
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

CTest ejecuta las pruebas del nucleo portatil. Para abrir la ventana raylib:

```bash
./build/lifepo4-desktop.exe
```

Se abre la ventana **App LiFePO4 - Monitor simulado**. Cierrala con la X o con Escape. Antes de volver a compilar, cierra la ventana para que Windows pueda reemplazar el ejecutable.

Si cambias de generador o de compilador, elimina solo la carpeta generada `build` y vuelve a ejecutar la configuracion de CMake desde cero.

## Sprint 1: nucleo y simulador

- `core/` contiene el modelo de datos, validacion, calculos de energia/potencia/autonomia, alarmas y un simulador determinista.
- `core-tests` verifica el ejemplo energetico, limites de entrada, umbrales de alarma, conteo de Coulomb y ambas capacidades de pack indicadas.
- Los datos del simulador son ilustrativos; no son lecturas de una bateria.
- La energia almacenada se calcula como tension medida en voltios por capacidad restante en Ah. La energia nominal usa 51.2 V por capacidad nominal; la energia util nominal aplica el DoD solicitado como porcentaje.
- La autonomia solo se informa cuando la corriente indica descarga (corriente negativa); se estima dividiendo energia almacenada por potencia `V * |I|`. No es una prediccion garantizada de uso real.
- Si en el futuro el BMS no entrega capacidad restante, el nucleo ofrece un contador de Coulomb que integra corriente con el tiempo a partir de un SOC inicial. Su precision depende de la calibracion y deriva de corriente/SOC; no sustituye una lectura confirmada del BMS. No se incluye estimacion por tension porque no hay una curva de reposo verificada para este pack.

## Sprint 2: interfaz simulada

- La ventana raylib muestra energia medida, energia nominal/util nominal (DoD fijo al 80%), SOC, capacidad restante, autonomia estimada, tension, corriente, potencia, ciclos, temperaturas, alarmas y las 16 tensiones de celda.
- Se puede alternar entre los perfiles simulados de 100 Ah y 335 Ah. El grafico representa potencia con signo: positiva durante carga y negativa durante descarga.
- Las muestras se actualizan cada segundo y el grafico conserva hasta 60 muestras. **Exportar CSV** guarda ese historial en `historial_simulado.csv` en la carpeta de trabajo actual; cada exportacion reemplaza el archivo anterior.
- Esta pantalla es exclusivamente de simulacion. No existe conexion TCP, serie o CAN con la bateria; la informacion de celdas, temperatura y ciclos tambien es ficticia.

Para recompilar y verificar en MSYS2 UCRT64:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/lifepo4-desktop.exe
```

## Sprint 3: preparacion de protocolo y validacion

- La evaluacion tecnica adjunta valora la base como prototipo academico (6,5/10) e identifica como prioridades la integridad de datos (5/10), la validacion electrica (4/10) y la preparacion para hardware real (2/10). Estas puntuaciones no son certificaciones ni resultados de una prueba automatizada.
- El nucleo ahora devuelve motivos concretos cuando un registro incumple limites estructurales. Tambien comprueba que la capacidad restante no exceda FULL_CAP cuando esta disponible y que las celdas declaradas tengan una tension no nula.
- `BUILD_DESKTOP=OFF` permite compilar y probar el nucleo sin raylib, mejorando la separacion y portabilidad del codigo:

  ```bash
  cmake -S . -B build-core -G Ninja -DBUILD_DESKTOP=OFF -DCMAKE_BUILD_TYPE=Debug
  cmake --build build-core
  ctest --test-dir build-core --output-on-failure
  ```

- `docs/protocol-status.md` registra interfaces, evidencia necesaria y bloqueos. `docs/validation-plan.md` separa integridad software de medicion electrica e incluye una plantilla para resultados.
- Las fotos del manual confirman pines RJ45 para CAN, RS485-1 y RS485-2, y 9600 bps por defecto para RS485-2; no muestran el protocolo de aplicacion/mapa de datos necesario para implementar un parser.
- **No se implemento un driver real:** no se dispone aun de protocolo confirmado, mapa de registros/tramas ni adaptador verificado. Modbus TCP por Wi-Fi sigue siendo solo una sospecha, no un dato confirmado. No se realizan conexiones ni se transmiten bytes al BMS.
- La validacion de laboratorio no esta aprobada: las tolerancias deben acordarse antes de medir, y las pruebas con la bateria requieren autorizacion, supervisión cualificada y respeto del manual y las protecciones.

## Pendientes para el primer transporte real

El protocolo de Wi-Fi, RS485-2 y los detalles necesarios de CAN/Pylon siguen pendientes de confirmar. Para continuar hacen falta el documento oficial aplicable al modelo y firmware, la interfaz escogida y el modelo del adaptador disponible. No inventar direcciones, registros o escalas. Mantener la aplicacion en solo lectura; RS232 queda fuera de uso y CAN debe ser solo escucha.
