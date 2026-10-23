# Plan de validacion electrica y de integridad

## Proposito

La compilacion y las pruebas unitarias comprueban comportamiento del software; no demuestran que el sistema mida correctamente una bateria real. Este plan separa validacion de datos, pruebas de laboratorio y evaluacion con el equipo real.

## Estado de preparacion

- **Integridad de datos:** validaciones y pruebas unitarias en desarrollo; todavia no hay parser de BMS.
- **Validacion electrica:** pendiente de definir tolerancias aprobadas y obtener lecturas reales comparables.
- **Hardware:** pendiente confirmar adaptadores, aislamiento, documentacion y supervision.
- **Resultado actual:** solo simulacion. No se han medido ni certificado valores de una bateria.

## Fase A: integridad del modelo y del parser

Antes de conectar equipo real:

- Rechazar registros nulos, pack ID fuera de 1..15, SOC fuera de 0..100, conteos fuera de los arrays y fuentes desconocidas.
- Si FULL_CAP/nominal capacity esta presente, rechazar capacidad restante superior a la nominal. Si la capacidad nominal no esta disponible, permitir que quede marcada como desconocida.
- Rechazar tensiones de celda cero cuando el registro declara esa celda presente.
- Validar tamanos de trama, version, direccion, checksum/CRC y unidades unicamente segun el protocolo oficial.
- Probar limites, campos truncados, datos invalidos, checksum incorrecto, campos opcionales ausentes y secuencias de reconexion mediante fixtures.
- Conservar errores explicitos. No convertir tramas invalidas en muestras con aspecto de exito.
- No exigir que SOC porcentual coincida exactamente con remaining capacity: redondeo y convenciones del BMS pueden diferir. Registrar ambas magnitudes.
- No exigir igualdad exacta entre tension del pack y suma de celdas: se necesita conocer la precision, sincronizacion y tolerancia del equipo antes de definir un criterio.

## Fase B: protocolo y banco de prueba

1. Obtener y revisar la especificacion del fabricante para el modelo y firmware exactos.
2. Identificar la interfaz y adaptador compatibles; confirmar aislamiento, pinout, parametros electricos y modo de lectura con el tutor/fabricante. El manual fotografiado aporta pinouts de RJ45, pero deben cotejarse con la revision exacta del equipo y el cable/adaptador antes de conectar.
3. Preparar fixtures de tramas oficiales y probar el parser sin bateria.
4. Verificar que las solicitudes solo consultan datos. RS232 queda excluido; CAN debe permanecer en escucha; no realizar escrituras ni comandos de configuracion.
5. Confirmar tiempos de espera, desconexion, recuperacion y limites de sondeo documentados.

No son criterios validos descubrir registros mediante tanteo ni enviar solicitudes a un dongle cuyo protocolo no esta confirmado. No asumir que los conectores RJ45 son Ethernet; no conectarlos a una LAN. Mantener RS232/RJ11 excluido porque el manual lo asocia a configuracion y actualizacion.

## Fase C: comparacion con bateria real

Debe realizarse solo con autorizacion del tutor y siguiendo el manual del fabricante. Las baterias de 51.2 V nominales y alta capacidad pueden entregar corrientes peligrosas. No abrir el pack, modificar protecciones, puentear terminales ni conectar directamente pines desconocidos a un PC. La eleccion del adaptador y la conexion fisica deben ser revisadas por una persona cualificada.

Para cada sesion, registrar fecha/hora, modelo, firmware, pack maestro, instrumentos y su calibracion, estado de carga/descarga/reposo y condiciones relevantes. Comparar de forma sincronizada:

- Tension del pack: BMS frente a instrumento de tension adecuado.
- Corriente y signo: BMS frente a pinza amperimetrica DC adecuada.
- SOC y capacidad restante: BMS frente a la app, indicando resolucion/redondeo.
- Energia calculada por la app, documentando que usa `V medida x Ah restantes`.
- Temperaturas y tensiones de celda: BMS frente a la app, solo si el protocolo entrega esos campos.

Realizar observaciones en reposo, carga y descarga solo dentro de condiciones autorizadas. No provocar sobrecorriente, sobretension, sobredescarga ni extremos termicos para probar alarmas.

### Criterios de aceptacion

Las tolerancias deben definirse y aprobarse **antes** de tomar medidas, atendiendo a exactitud especificada del BMS y de los instrumentos. El ±1 % de tension mencionado en requisitos es solo una propuesta inicial, no una tolerancia confirmada. No hay tolerancias aprobadas aun para corriente, SOC, capacidad, temperaturas ni celdas. Sin instrumentos y tolerancias acordados, el resultado se informa como observacional, no como validacion aprobada.

Para magnitudes con valor de referencia distinto de cero, calcular:

`error porcentual = 100 x |valor_app - valor_referencia| / |valor_referencia|`

Para referencia cero o cerca de cero, usar diferencia absoluta y la resolucion/incertidumbre instrumental en lugar de dividir por cero. Presentar resultados por magnitud y estado operativo, incluidos casos fallidos.

## Registro de resultados (plantilla)

| Fecha/hora | Estado | Magnitud | BMS | Instrumento | App | Error/diferencia | Tolerancia aprobada | Resultado |
|---|---|---|---:|---:|---:|---:|---:|---|
| Pendiente | Pendiente | Pendiente | — | — | — | — | Pendiente | No probado |
