# Informe de estado y requisitos para continuar los sprints

**Fecha:** 27/11/2026  
**Proyecto:** Dazi Solar - Monitor LiFePO4  
**Objetivo:** resumir el estado verificable y los datos necesarios para continuar el desarrollo sin asumir caracteristicas no confirmadas del BMS.

## 1. Resumen ejecutivo

El proyecto dispone de un nucleo portable en C11, una aplicacion desktop con raylib, un simulador de bateria y pruebas automatizadas. Tambien incluye un parser y un simulador offline para tramas PACE Modbus RTU.

El siguiente trabajo de software puede avanzar con los datos ya disponibles: endurecer el parser, completar fixtures y mejorar el modelo de errores/reconexion. La conexion fisica sigue pendiente de confirmar la revision exacta del BMS, el puerto, el cableado y la compatibilidad del protocolo con la unidad DZPW-1600 concreta.

No se debe presentar la integracion como validada ni conectar el puerto a la bateria hasta revisar la documentacion aplicable y el adaptador con una persona cualificada. El producto actual sigue siendo desktop C11/raylib; una app Android no forma parte del estado implementado.

## 2. Estado por sprint

| Sprint | Estado | Evidencia y trabajo restante |
|---|---|---|
| 0 - Entorno y build | Completo para desktop | CMake, C11, MSYS2 UCRT64, raylib y configuracion core-only. |
| 1 - Nucleo y simulador | Implementado | Modelo de datos, validacion, calculos, alarmas provisionales, contador de Coulomb, simulador y CSV. Mantener los limites como orientativos; no son protecciones certificadas. |
| 2 - Interfaz simulada | Implementado | Panel raylib para simulacion, metricas, celdas, alarmas y grafico. No muestra telemetria del BMS real. |
| 3 - Preparacion de protocolo | Parcial/completado en software | Se identifico PACE P16S100A como referencia comunicada por el usuario. Hay investigacion comunitaria de PACE Modbus y documentacion del estado. La revision real de PCB/firmware de esta unidad aun requiere evidencia fisica. |
| 4 - Transporte real | Parcial; bloqueado para hardware | El parser y simulador Modbus offline existen. Falta confirmar que PACE_MODBUS aplica al firmware DZPW-1600, validar el puerto/adaptador y desarrollar el transporte serial con manejo de timeouts/reconexion. No se ha conectado el BMS. |
| 5 - Validacion, memoria y entrega | Pendiente | Requiere lecturas reales autorizadas, instrumentos adecuados, tolerancias acordadas, resultados reproducibles, manual de usuario y paquete de entrega. |
| Opcional - Pasarela ESP32/Android | Fuera del alcance actual | Requiere decidir alcance y hardware/software objetivo. No es requisito para cerrar el prototipo desktop. |

## 3. Lo que ya esta implementado para Sprint 4

- Funciones C para calcular/verificar CRC Modbus RTU.
- Construccion de solicitud de lectura de holding registers con funcion 0x03.
- Decodificacion offline de corriente, tension, SOC, capacidades, ciclos, 16 celdas y cuatro temperaturas.
- Generacion offline de respuesta Modbus sintetica para probar el ciclo solicitud-respuesta-parser.
- Pruebas unitarias de CRC, campos y escalas usando fixtures sinteticos.
- Separacion entre `core/` y la capa de interfaz.

Estas pruebas verifican el comportamiento del software frente a datos sinteticos. No prueban el mapa de registros en la bateria DZPW-1600 ni la comunicacion electrica del adaptador.

## 4. Informacion que necesito para integrar el primer transporte real

### Prioridad 1: identificar la unidad

1. Foto legible de la etiqueta del BMS/PCB con modelo completo, revision y cualquier cadena de firmware.
2. Confirmacion de si la referencia es `P16S100A`, `PC16S100A` u otra cadena. No tratar estas denominaciones como intercambiables sin comprobar la etiqueta.
3. Modelo DZPW exacto, capacidad de placa (314 Ah) y revision del manual de usuario.
4. Si el fabricante o distribuidor confirma por escrito que esta revision usa `PACE_MODBUS` sobre el puerto RS485 indicado.

### Prioridad 2: puerto y adaptador

1. Foto del panel/conectores que permita identificar el RJ45 de RS485 junto al CAN, sin desconectar ni abrir el pack.
2. Modelo exacto del adaptador USB-RS485 disponible y su documentacion; preferiblemente aislado galvanicamente.
3. Confirmacion documentada del pinout y del cable concreto. No asumir que el pinout RJ45 comunitario coincide con el pinout del manual DZPW.
4. Direccion DIP actual, obtenida visualmente sin cambiarla. El ejemplo comunitario usa 0x01, pero no se debe asumir que esa es la direccion de la unidad.
5. Confirmacion de velocidad/formato serie aplicables. 9600 8N1 es una referencia comunitaria de configuracion, no una medicion de esta unidad.
6. Confirmacion de si es necesario habilitar el protocolo mediante PbmsTools y quien esta autorizado a hacerlo. No cambiar configuracion como parte de una prueba de solo lectura.

### Prioridad 3: validacion de mediciones

1. Autorizacion del tutor y supervision de una persona cualificada para la prueba fisica.
2. Multimetro adecuado y pinza amperimetrica DC, con especificaciones/calibracion conocidas.
3. Estado operacional autorizado para comparar (reposo, carga o descarga) y registro sincronizado de las lecturas del BMS y de los instrumentos.
4. Tolerancias de aceptacion definidas antes de medir, teniendo en cuenta la precision del BMS y de los instrumentos.
5. Confirmacion de si el pack maestro representa al conjunto cuando hay baterias en paralelo.

No abrir la bateria, no puentear pines, no conectar un puerto RJ45 a Ethernet y no provocar condiciones extremas para probar alarmas. RS232 se mantiene excluido porque la documentacion lo asocia con configuracion/actualizacion. CAN permanece fuera del primer driver hasta disponer de su especificacion y adaptador apropiado.

## 5. Plan de trabajo propuesto

### Sprint 4A - endurecimiento offline

- Revisar los campos opcionales, bits de alarma y registros PACE antes de mapearlos al modelo de alarmas interno.
- Probar registros truncados, direccion equivocada, funcion Modbus inesperada, CRC incorrecto, valores imposibles y campos opcionales ausentes.
- Conservar las banderas de disponibilidad; no convertir registros ausentes en ceros medidos.
- Confirmar la convencion de signo de corriente, el escalado de capacidades y el rango representable.
- Añadir fixtures a partir de ejemplos de protocolo documentados, claramente etiquetados como comunitarios o del fabricante.

### Sprint 4B - transporte serial, solo tras confirmar requisitos fisicos

- Implementar una interfaz serial portatil/separada del parser; no incluir apertura de puerto en el nucleo de calculos.
- Parametrizar puerto y direccion explicitamente; no escanear direcciones.
- Limitar la solicitud al bloque de registros de lectura confirmado.
- Definir timeout, desconexion, frecuencia de sondeo y recuperacion segun documentacion del BMS/adaptador.
- Publicar una muestra solo despues de validar respuesta, CRC, direccion y campos.
- Mostrar el estado real de conexion y el ultimo error; nunca presentar una muestra antigua como nueva.
- Probar inicialmente con un simulador local/puerto virtual antes de cualquier conexion a la bateria.

### Sprint 5 - validacion y entrega

- Ejecutar la comparacion autorizada con BMS e instrumentos en reposo/carga/descarga.
- Registrar valores crudos, valores convertidos, hora, revision de BMS, direccion y configuracion del adaptador.
- Calcular diferencias/error y documentar resultados fallidos; no ajustar tolerancias despues de ver los resultados.
- Completar memoria FCT, manual de instalacion/uso, limitaciones, instrucciones de build y demo.
- Generar y probar el paquete de entrega en un entorno limpio.

## 6. Criterios de cierre

### Para considerar completo el transporte de software

- Fixtures documentados y pruebas negativas cubren el parser.
- Build con warnings estrictos y core-only pasan.
- No existen caminos de escritura ni comandos de configuracion.
- Timeouts, desconexion y datos invalidos generan errores visibles.
- La UI identifica fuente, conexion, antiguedad de muestra y modo demo.

### Para considerar validada la integracion con hardware

- Revision BMS/protocolo, pinout, direccion y adaptador confirmados para la unidad.
- Prueba supervisada y autorizada realizada sin modificar las protecciones.
- Mediciones comparadas con instrumentos y tolerancias previamente aprobadas.
- Resultados y condiciones de prueba registrados en la memoria del proyecto.

## 7. Riesgos y decisiones pendientes

- **Compatibilidad del protocolo:** que el modulo sea PACE P16S100A no demuestra por si solo que la unidad DZPW-1600 use la misma revision de PACE_MODBUS.
- **Identificacion de placa:** `P16S100A` y `PC16S100A` aparecen como referencias distintas en la informacion recibida; falta foto/confirmacion para resolverlo.
- **Pinout:** el pinout comunitario PACE puede no coincidir con los conectores del manual DZPW; prevalece la documentacion confirmada para la unidad y cable concretos.
- **Capacidad y limites:** la etiqueta indica 314 Ah. Los limites de corriente de alarma para ese perfil siguen sin validacion del fabricante; no extrapolar los umbrales de 100 Ah o 335 Ah.
- **CI:** el workflow remoto puede reflejar un commit anterior a las correcciones locales; comprobar el resultado del commit actual antes de declarar verde la CI.
- **Plataforma:** el estado actual es desktop C11/raylib. Android, JNI, SQLite y una pasarela ESP32 requieren una decision de alcance y no deben darse por existentes.

## 8. Proximo paso recomendado

Para avanzar sin bloquear el trabajo de software, continuar con Sprint 4A (fixtures y pruebas negativas offline). Para empezar 4B, compartir primero las fotos de identificacion del BMS y del puerto, el modelo del adaptador disponible y cualquier respuesta del proveedor sobre el protocolo/firmware.
