# Estado de investigacion de protocolos

## Estado actual

Las fotos del manual aportadas por el usuario confirman la disposicion de puertos y algunos pinouts. La referencia PACE y la implementacion comunitaria aportan una hipotesis de protocolo para P16S100A, pero no confirman su aplicacion a la revision exacta de la unidad DZPW-1600. Por ello, aun no se debe activar el transporte fisico para esa bateria.

Tras identificar el modulo P16S100A/PACE y contrastarlo con la documentacion
comunitaria verificada, se considera Modbus RTU sobre RS485 una hipotesis
documentada y candidata para el primer transporte real, no una integracion
comprobada con la unidad DZPW-1600 concreta.

La referencia comunitaria documenta:

- puerto RS485 junto al puerto CAN;
- 9600 bps;
- formato serie 8N1;
- direccion Modbus esclava 0x01 por defecto en el ejemplo;
- holding registers de solo lectura desde el registro 0;
- tension, corriente, SOC, capacidad, ciclos, celdas y temperaturas;
- CRC Modbus RTU.

El proyecto incluye un simulador offline PACE/Modbus que:

- construye solicitudes Modbus RTU funcion 0x03;
- valida CRC y direccion de esclavo;
- genera respuestas con holding registers sintéticos;
- decodifica el resultado al modelo `battery_data_t`.

Este simulador no abre puertos, no utiliza 9600/19200 bps, no envia bytes al
BMS y no sustituye la validacion de la revision de firmware de la unidad real.

## Identificacion del BMS

El usuario identifica el BMS de la bateria Dazi Solar **DZPW-1600** como un modelo
PACE **P16S100A**. En mensajes anteriores tambien aparecio la cadena **PC16S100A**.
No se dispone aun de una fotografia legible de la etiqueta/PCB que permita resolver
si son referencias distintas, una lectura de etiqueta o una variante del mismo modulo.
La referencia completa de hardware y firmware sigue pendiente.

La comunidad documenta PACE_MODBUS para varias revisiones P16S100A, pero esa
evidencia no demuestra que la unidad DZPW-1600 concreta use el mismo protocolo,
configuracion o mapa de registros. El parser del proyecto es offline y no establece
esa compatibilidad.

| Interfaz | Confirmado en las paginas aportadas | No confirmado / bloqueo |
|---|---|---|
| Wi-Fi/dongle | El dibujo de componentes identifica un modulo Wi-Fi. | Protocolo, modelo/firmware, direccionamiento, puerto y mapa de datos. Modbus TCP/502 no esta confirmado; no escanear ni enviar solicitudes. |
| CAN (RJ45 de X1, parte A) | Pagina 5: pin 2 = CGND, pin 4 = CAN-H, pin 5 = CAN-L; otros pines X1 aparecen NC. El manual general del proyecto indica Pylon, pendiente de confirmar la especificacion aplicable. | Bitrate, IDs, formato, escalas, checksum/heartbeat y condiciones exactas de escucha. |
| RS485-1 (RJ45 de X1, parte B) | Pagina 5: pines 9/15 = RS485-A1, 10/16 = RS485-B1, 11/14 = GND; 12/13 = NC. Pagina 5 dice que RS485-1/CAN se usa para comunicar el pack con equipo externo (por ejemplo, inversor). | Protocolo exacto, velocidad, mapa de campos y confirmacion de que se puede consultar independientemente del inversor. |
| RS485-2 (RJ45 dual X2) | Pagina 6: A = pines 2/7, B = pines 1/8, GND = pines 3/6; 4/5 = NC. El segundo conector X2 replica el pinout en pines 9-16. Pagina 6 indica 9600 bps por defecto y uso para software BMS de PC e interconexion de packs en paralelo. | Framing, direccionamiento, paridad/stop bits, checksum, comandos de lectura y mapa de datos. 9600 bps es el valor por defecto, pero verificar configuracion real antes del uso. |
| RS232 (RJ11) | Pagina 6: pin 3 = TX, pin 4 = RX, pin 5 = GND; el manual lo describe para software BMS que puede modificar parametros y actualizar firmware. | Excluido de la aplicacion por riesgo de escritura/actualizacion. No implementar ni conectar. |

La pantalla TPMUZN-1.2 parece permitir seleccionar un protocolo de comunicacion (pagina 9), pero la foto no documenta el significado de todas las opciones ni su formato sobre el cable. La seleccion en pantalla no sustituye el mapa de tramas/registros.

Los numeros 1-8 y 9-16 de X1/X2 son las referencias de pines de las dos interfaces RJ45 duales impresas en el manual; no asumir que se trata de Ethernet. No conectar estos puertos a una LAN ni a un adaptador hasta confirmar el cableado y la funcion de cada puerto.

## Evidencia necesaria antes de escribir un driver

Solicitar al fabricante o distribuidor, preferentemente por escrito:

- Referencia completa del modulo **P16S100A/PC16S100A** tal como aparece en la etiqueta, revision de PCB y version de firmware.
- Modelo exacto y revision de hardware/firmware del BMS, pantalla y dongle.
- Documento del protocolo correspondiente al puerto que se utilizara.
- Parametros fisicos completos: velocidad y formato serie, bitrate CAN, terminacion, aislamiento y pinout confirmado para el adaptador concreto.
- Formato de tramas, direccionamiento, longitud, endianness, checksum/CRC y unidades/escalas.
- Lista de campos disponibles: tension, corriente y signo, SOC, capacidad restante/nominal, celdas, temperaturas, ciclos y alarmas.
- Comandos de lectura permitidos, limites de frecuencia, tiempo de respuesta y numero maximo de conexiones.
- Confirmacion de que el pack maestro representa el conjunto cuando hay packs en paralelo.
- Version de firmware a la que aplica la especificacion y ejemplos de tramas de solo lectura.

Una captura de trafico solo es evidencia auxiliar: debe proceder del equipo propio, con autorizacion, y no reemplaza la documentacion del fabricante ni permite inferir escalas sin validar.

## Reglas de implementacion

1. No inventar registros, identificadores CAN, escalas, funciones ni checksums.
2. No establecer una conexion con la bateria ni transmitir datos hasta revisar la especificacion y confirmar el adaptador.
3. No usar RS232 ni implementar escritura/configuracion. Para Modbus, limitarse a funciones de lectura documentadas (0x03/0x04).
4. Para CAN, usar recepcion en modo escucha; no transmitir tramas.
5. Separar transporte, parser y nucleo. El parser debe rechazar longitudes, valores, checksums y versiones no validos con un error visible.
6. Registrar la fuente y el instante de adquisicion; no mezclar muestras de fuentes distintas sin indicarlo.
7. Probar primero con ejemplos oficiales/fixtures y simuladores locales. La prueba con hardware requerira un plan aprobado y supervision adecuada.

## Datos que faltan para continuar Sprint 3 con un transporte real

- Documento oficial del protocolo de aplicacion para el puerto a utilizar, incluido modelo/revision de hardware y firmware.
- Confirmar el primer puerto: RS485-2 (destinado al software BMS de PC), CAN o RS485-1.
- Modelo exacto del adaptador disponible. La mencion de "RS485-1/CAN" no aclara si hay dos adaptadores ni si son aislados.
- Para Wi-Fi: fabricante/modelo/revision del dongle y guia oficial de su interfaz.
- Para RS485: formato serie y reglas de consulta; para CAN: bitrate, IDs y especificacion de trama.

Hasta recibir esos datos, el estado del primer transporte real es **bloqueado por protocolo de aplicacion no confirmado**. Los pinouts y el baudrate por defecto de RS485-2 ayudan a identificar la capa fisica, pero no bastan para formar una consulta ni decodificar una respuesta. El simulador sigue siendo la unica fuente habilitada en la interfaz.
