# Control por cable USB

## Identificación de esta unidad — 20 de noviembre de 2026

La comparación antes/después de conectar el cable y seleccionar modo USB identificó:

| Conexión | Producto | VID:PID | Canal de control |
| --- | --- | --- | --- |
| Receptor 2.4 GHz | EA75 Dongle | `05AC:024F` | Interfaz 3, página `FF60`, uso `0061`, payload de 32 bytes |
| Cable USB | EPOMAKER EA75, fabricante HFD | `0C45:8006` | Interfaz 2, página `FF13`, uso `0001`, payload de 64 bytes |

La identificación del sistema operativo aclara la variante de esta unidad; el aspecto de un F75 no basta para seleccionar su protocolo. El identificador `0C45:8006` también aparece en otros productos. La ruta USB requiere el nombre exacto `EPOMAKER EA75`, bus USB, interfaz/página/uso anteriores y coincidencia completa del descriptor de 48 bytes observado. No habilita genéricamente otros F75/EA75 ni otras interfaces del mismo teclado.

El descriptor cableado expone entrada, salida y Feature de 64 bytes, sin Report ID explícito. El buffer de hidapi añade Report ID 0: se usan Feature Reports de 65 bytes. La ruta inalámbrica conserva sus Output Reports de 33 bytes, incluido Report ID 0, y su checksum de payload.

## Selección de conexión y estado

`control_channel.hpp` decide la conexión; `control_hid.hpp` enumera y verifica los descriptores para ambos auxiliares. USB tiene prioridad cuando el canal de esta unidad aparece junto al receptor. Dos canales USB distintos son ambiguos y bloquean el envío; no se busca silenciosamente otra ruta. Desconectar el cable devuelve la selección al receptor, si está presente. El interruptor físico debe estar en USB para enumerar y utilizar el cable; conectarlo solo para cargar mientras se usa 2.4 GHz no anuncia soporte cableado.

La GUI detecta cambios cada cinco segundos. Cambiar de conexión invalida la generación de consultas anterior y exige verificar el nuevo canal. Las acciones incluyen `--transporte usb` o `--transporte 2g`; el auxiliar vuelve a enumerar y aborta si la selección cambió. Con **Restaurar último efecto** activado se reenvían los últimos ajustes guardados una vez tras verificar el canal; no se elige un perfil ni se envían cambios pendientes. Un fallo de acceso USB se reintenta con la siguiente detección y no habilita Aplicar hasta verificar el canal. Véase [memoria de iluminación](memoria-iluminacion.md).

La salida estructurada es `AULA_DEVICE 2 usb present`, `AULA_DEVICE 2 usb ready` o `AULA_DEVICE 2 2g ready <porcentaje>`, con los mismos códigos de error. El lector conserva compatibilidad con versión 1. `ready` por USB confirma acceso e identidad del canal, sin atribuir una respuesta de batería ni confirmación de un efecto. La GUI muestra USB y «Batería no disponible por USB», oculta barras de batería y pausa las consultas periódicas durante esa conexión. El intervalo elegido se conserva para volver a 2.4 GHz. No interpreta USB como 100 % ni como confirmación de carga.

## Protocolo RGB cableado

La implementación propia usa cinco payloads de 64 bytes, con Report ID 0 adicional:

1. Abrir sesión: `04 18`.
2. Seleccionar iluminación: `04 13`, byte 8 del payload igual a 1.
3. Efecto: modo en byte 0; R/G/B en 1–3; multicolor en 8; brillo en 9; velocidad en 10; dirección 0; `AA 55` en 14–15. El resto queda a cero.
4. Guardar: `04 02`.
5. Salir/aplicar: `04 F0`.

Se esperan 50 ms entre etapas. Abrir, seleccionar y guardar exigen una respuesta Feature de 65 bytes, Report ID 0 y prefijo `04 <comando> 00 01`. Se detiene la secuencia ante una escritura incompleta o un ACK distinto; no se intenta otro canal. Tras un fallo después de abrir se intenta finalizar la sesión. Los auxiliares ejecutados desde la GUI conservan el límite de ocho segundos y cancelación del proceso.

Referencias de interoperabilidad: [constructor y secuencia cableada de Ghost-CR](https://github.com/Ghost-CR/F75_Initializer/blob/master/aula_hacky/windows_rgb_test.py), [referencia del protocolo Sonix de ajazzctl](https://github.com/SpawW/ajazzctl/blob/main/PROTOCOL.md) y [contrato de hidapi](https://libusb.info/hidapi/group__API.html). Esas fuentes tratan otras variantes; se contrastaron los descriptores y las respuestas en esta unidad. No se copiaron funciones de remapeo, firmware o pantalla de esos proyectos.

## Pruebas realizadas y pendientes

- Comparación HID con receptor solo y con cable en modo USB; descriptor de control USB leído sin enviar comandos.
- Primera prueba cableada: rojo fijo RGB `(180, 0, 0)`, modo 1, brillo y velocidad 3. Se recibieron ACK correctos de apertura, selección y guardado. El usuario confirmó visualmente el rojo fijo correcto por USB, con el receptor todavía conectado.
- Con USB y receptor conectados, `--detectar` y `--estado --transporte usb` seleccionan USB. Solicitar `--transporte 2g` mientras USB tiene prioridad bloquea el envío antes de abrir/escribir.
- El usuario confirmó por cable el giro de volumen y el clic Reproducir/Pausar al poner la perilla en modo multimedia. Tras investigar eventos duplicados e incorporar el filtro de 100 ms y la respuesta inmediata del triple, confirmó que los tres gestos cuentan correctamente. La preferencia del remapeo global se conserva. La captura y la corrección se detallan en [perilla y multimedia](perilla-multimedia.md).
- Once pruebas CTest cubren identificación y exclusión de dispositivos ajenos, selección y ambigüedad, descriptores, encuadre USB, estado/reconexión, conservación del protocolo inalámbrico y distribución de las pantallas USB/2.4 GHz en ambos idiomas, ambas skins y cuatro tamaños. No equivalen a validar físicamente todos los efectos por cable.
- Se compiló inicialmente una candidata aislada en `build/wired/Release/`. Después de confirmar visualmente el rojo y salir de la aplicación, se actualizó `build/Release/` con el programa y auxiliares nuevos y se sincronizó el alias `aula_control_glass.exe`. Las once pruebas pasaron también en la ubicación habitual; el auxiliar instalado devolvió `usb present` y `usb ready`. El usuario abrió la build actualizada y confirmó que la interfaz reconoce la conexión USB. La comprobación de otros colores y efectos desde esa ventana sigue pendiente.
- Build estable anterior preservada en `build/validated-2026-11-20/`, con copia de preferencias anteriores. Se mantuvieron los perfiles existentes y las preferencias de idioma English, consulta cada 30 segundos, inicio en bandeja y clics multimedia activados.

No se ha reiniciado Windows ni se ha validado el cambio cable/receptor físicamente desde la nueva GUI. Los perfiles y preferencias existentes no se modifican como parte de la investigación. Los archivos de diagnóstico completos se mantienen locales en `build/wired-research/`.

## Bluetooth

Esta ampliación implementa la ruta USB y conserva 2.4 GHz. Escribir y usar teclas multimedia mediante Bluetooth en Windows no demuestra acceso al canal propietario de iluminación. No se declara soporte RGB/batería por Bluetooth: se debe enumerar esa conexión y comprobar sus capacidades por separado. No se enviarán comandos USB ni del dongle a un dispositivo Bluetooth por compartir un nombre.
