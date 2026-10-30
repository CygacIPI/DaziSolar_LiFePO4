# Plan de validacion electrica y de integridad

## Proposito

La compilacion y las pruebas unitarias comprueban comportamiento del software; no demuestran que el sistema mida correctamente una bateria real. Este plan separa validacion de datos, pruebas de laboratorio y evaluacion con el equipo real.

## Estado de preparacion

- **Integridad de datos del nucleo:** validaciones y pruebas unitarias automatizadas para entradas, campos desconocidos, calculos, alarmas, contador de Coulomb, secuencias del simulador y CSV. No hay parser de BMS.
- **Validacion electrica:** pendiente de definir tolerancias aprobadas y obtener lecturas reales comparables.
- **Hardware:** pendiente confirmar adaptadores, aislamiento, documentacion y supervision.
- **Resultado actual:** solo simulacion. No se han medido ni certificado valores de una bateria.

## Fase A: integridad del modelo y del parser

Antes de conectar equipo real:

- Rechazar registros nulos, pack ID fuera de 1..15, SOC fuera de 0..100, conteos fuera de los arrays y fuentes desconocidas.
- Si FULL_CAP/nominal capacity esta presente, rechazar capacidad restante superior a la nominal. Si la capacidad nominal no esta disponible, permitir que quede marcada como desconocida.
- Rechazar tensiones de celda cero cuando el registro declara esa celda presente.
- Mantener separados los datos estructuralmente invalidos y las mediciones fuera de los umbrales de operacion: una tension o temperatura fuera de rango puede ser una lectura valida que debe activar una alarma, no descartarse como desconocida. Los rangos de alarma actuales son provisionales y no constituyen una validacion del sensor.
- No imponer un rango fisico universal adicional a las temperaturas hasta conocer las especificaciones del sensor BMS; validar por ahora presencia/conteo, rango representable y umbrales configurados.
- Cuando SOC, capacidad restante y nominal estan disponibles, aceptar una diferencia de hasta 1 punto porcentual entre SOC y `100 x restante / nominal`; rechazar diferencias superiores. La tolerancia contempla la resolucion/redondeo y mantiene valido el ejemplo 68 % frente a 68,87 Ah. No evaluar la relacion si falta cualquiera de los tres campos.
- Validar tamanos de trama, version, direccion, checksum/CRC y unidades unicamente segun el protocolo oficial.
- Probar limites, campos truncados, datos invalidos, checksum incorrecto, campos opcionales ausentes y secuencias de reconexion mediante fixtures.
- Conservar errores explicitos. No convertir tramas invalidas en muestras con aspecto de exito.
- Registrar SOC y capacidades en cada muestra. La tolerancia de software no demuestra la exactitud ni la convencion SOC del BMS; esta debe confirmarse con el protocolo/documentacion oficial y validacion experimental.
- No exigir igualdad exacta entre tension del pack y suma de celdas: se necesita conocer la precision, sincronizacion y tolerancia del equipo antes de definir un criterio.

### Alarmas de monitorizacion

- Los limites de corriente solo estan configurados para capacidades nominales de 100.000 mAh y 335.000 mAh; esos valores y todos los umbrales siguen siendo provisionales.
- Con capacidad desconocida o no admitida, `battery_assess_alarms()` continua evaluando los limites de tension/temperatura, pero informa `current_limit_evaluable = false`. No se interpreta la ausencia de alarma de corriente como confirmacion de que la corriente sea segura. La interfaz muestra esta condicion.
- Los wrappers de compatibilidad `battery_evaluate_alarms*()` devuelven `false` cuando no es posible evaluar el limite de corriente. Para recuperar las alarmas que si pudieron evaluarse y el estado de evaluabilidad, usar `battery_assess_alarms*()`.
- La alarma general de tension alta se activa por encima de 57.600 mV. Durante carga, una lectura por encima de 58.400 mV activa en su lugar la alarma especifica de tension alta de carga; ambas no se emiten a la vez. A 58.400 mV exactos solo aplica la alarma general (si se supera 57.600 mV). Son umbrales provisionales, no protecciones.

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
