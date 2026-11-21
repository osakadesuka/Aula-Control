# Memoria del último envío de iluminación

Implementación y comprobaciones del 20 de noviembre de 2026. El usuario eligió reaplicar automáticamente el último efecto al abrir AULA o reconectar el teclado.

## Comportamiento

El último envío completado guarda efecto, RGB original, paleta, ajuste RGB, brillo nativo y velocidad. Al abrir se recuperan esos controles. Inicio muestra ese efecto; Iluminación permite preparar otra selección sin sustituir la representación de Inicio. Editar, cargar un perfil o fallar un envío no reemplaza el último envío guardado.

**Recuperar último envío** carga esa memoria en la selección editable sin enviarla. Sin memoria válida, el botón queda desactivado e Inicio muestra **Sin efecto guardado**, con la fotografía sin iluminación simulada. Los perfiles de la biblioteca permanecen separados.

**Preferencias → Restaurar último efecto** controla el envío automático. Está activado según la elección del usuario y conserva su valor al salir. Requiere memoria válida y comunicación verificada; se intenta una vez por conexión, también al iniciar en bandeja. El auxiliar exige el transporte seleccionado para impedir un cambio silencioso de ruta. Un fallo no genera un bucle de reintentos de iluminación. Un envío manual también cuenta como intento en esa conexión.

Si el receptor sigue conectado mientras el teclado deja de responder, una consulta con resultado `no_response` permite un nuevo intento cuando vuelve la comunicación. La detección y la consulta conservan sus intervalos existentes. Desactivar la restauración no borra la memoria ni envía la selección pendiente.

## Archivo y confirmación

`%LOCALAPPDATA%\AulaControl\last-lighting.txt` utiliza este formato:

```text
AULA_LAST_LIGHTING 1
143 51 10 11 100 1 2 4
2g 1795220946338
```

La segunda línea contiene rojo, verde, azul, modo, ajuste RGB, multicolor, velocidad y brillo. La tercera guarda transporte (`usb` o `2g`) y hora del envío en milisegundos Unix. La hora es una referencia local, no telemetría del firmware.

El auxiliar emite `AULA_LIGHTING 1 <transporte> <hora>` al completar la secuencia. La GUI exige un proceso correcto y un único recibo válido del transporte esperado antes de actualizar la memoria. Por USB se mantienen los ACK de su secuencia; por receptor, completar la escritura no demuestra por sí solo el resultado visual de cada parámetro.

La escritura usa un temporal en la misma carpeta y reemplazo atómico. Se validan todos los valores y el catálogo de efectos. Archivos ilegibles, mayores de 1024 bytes o de formato desconocido se conservan y bloquean su sobrescritura; la interfaz informa del problema. Un fallo al guardar no se presenta como persistencia correcta. La memoria no modifica `lighting-profiles.txt`.

## Tiempo de la vista previa y límites

Inicio y la selección editable comparten un reloj cuando muestran el último envío. Una selección distinta usa su propio reloj; navegar o editar no reinicia la animación del último envío. Al completar otro envío, el reloj se ajusta a la hora del auxiliar, compensando su espera de respuesta. Mientras la aplicación está abierta se usa tiempo monotónico. Al recuperar una sesión se estima el tiempo transcurrido desde la hora guardada; un retroceso del reloj se limita a cero.

Ocultar o minimizar no solicita fotogramas del teclado. Al volver, el reloj incorpora el tiempo transcurrido. Se conserva el máximo de 30 actualizaciones por segundo y la preferencia de animaciones de Windows.

La representación sigue siendo una simulación. No hay una consulta validada del efecto activo ni de la fase de los LED para esta unidad. Los cambios hechos desde la perilla o desde otra aplicación no se leen; la restauración automática vuelve a enviar los ajustes de AULA. Los efectos reactivos se ilustran sin capturar pulsaciones. No se declara sincronización exacta con la fase física del firmware.

## Verificación

- Las doce pruebas CTest pasaron en la candidata `build/wired/` y en la build habitual `build/`. Cubren recibos estrictos, persistencia, protección de formatos futuros, ajustes inválidos, restauración una vez por conexión, relojes y conservación de preferencias.
- Las pruebas de GUI verifican la separación entre selección y último envío, su recuperación y la distribución de la nueva preferencia en ambos idiomas, ambas skins y cuatro tamaños. No utilizan preferencias ni perfiles reales.
- Con la ventana real se envió ola horizontal multicolor, brillo 4/5 y velocidad 2/5. Elegir Respiración sin aplicarla mantuvo la ola en Inicio y mostró cambios pendientes.
- Salir completamente y reabrir recuperó la ola y sus valores. Se observó **Lighting restored.** sin pulsar Aplicar y cambió la hora del recibo guardado, confirmando un envío desde la bandeja.
- El usuario confirmó que desconectar el receptor unos seis segundos y reconectarlo recupera la ola y los ajustes en el teclado real.
- La biblioteca real conservó SHA256 `D5C811B5D3CBCC2C6D1EDBE9E6FA3456E12FCFFC6F82EA171D4A1DD040109149`. Se mantuvieron idioma, intervalo de batería, apertura en bandeja y clics multimedia. Las preferencias se migraron al formato 4 para añadir la restauración.

La build anterior y sus preferencias quedaron en `build/validated-before-lighting-memory-2026-11-20/`. No se reinició Windows durante esta comprobación. El cambio físico entre cable y receptor y todos los efectos por USB requieren su propia validación; esta prueba de reconexión corresponde a 2.4 GHz.
