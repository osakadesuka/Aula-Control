# Investigación del protocolo: receptor 2.4 GHz

Este archivo conserva el historial de hipótesis y pruebas del receptor. La identificación cableada `EPOMAKER EA75 / 0C45:8006` y su protocolo distinto se documentan en [soporte USB](soporte-usb.md).

Fecha: 15 de noviembre de 2026. Estado: investigación; ningún comando probado en este teclado.

## Observaciones de esta unidad

El receptor desaparece de la enumeración al desconectarlo y reaparece al conectarlo. Se identifica como `EA75 Dongle`, VID `05AC`, PID `024F`. El modelo exacto del teclado y su firmware no están confirmados; su aspecto es compatible con un F75 estándar.

- Interfaz 3: usage page `FF60`, usage `0061`, descriptor de entrada y salida de 32 bytes, sin Report ID explícito.
- Interfaz 4: misma usage page y usage, descriptor de entrada y salida de 64 bytes, sin Report ID explícito. No se observaron informes espontáneos durante 10 segundos.
- Abrir las interfaces y leer descriptores funcionó. No se han enviado informes de salida ni validado funciones.

## Referencias externas

[Simon-Martens/F75_Initializer](https://github.com/Simon-Martens/F75_Initializer) describe una captura de un receptor `05AC:024F` de un F75 Max: interfaz 3, informes de interrupción de 32 bytes y checksum consistente en sumar los primeros 31 bytes y conservar el byte menos significativo. Es una coincidencia importante con nuestras observaciones, pero no prueba que el teclado del usuario tenga el mismo firmware.

[Ghost-CR/F75_Initializer](https://github.com/Ghost-CR/F75_Initializer) publica constructores de paquetes para ese receptor y describe RGB y consulta de batería. [El catálogo de paquetes de aula-f75-max](https://github.com/tarikmanoar/aula-f75-max/blob/main/docs/packet-catalog.md) resume esos comandos y los clasifica como experimentales para su propio receptor. Describe una consulta de batería cuyo payload de 32 bytes comienza `20 01`, termina en checksum `21` y cuya respuesta esperada comienza `20 01 00` seguida del porcentaje. Esto es una hipótesis externa, no un resultado de esta unidad; una consulta requiere enviar un informe de salida y no es una lectura pasiva.

Ese catálogo también describe una secuencia RGB y señala que no dispone de comandos documentados para su interfaz de 64 bytes. No se debe extrapolar que la interfaz 4 de esta unidad tenga la misma función. Las fuentes se centran en modelos Max; la coincidencia de identificadores no garantiza compatibilidad de comandos ni ausencia de efectos secundarios.

## Próximos pasos

1. Si es posible, identificar modelo y revisión desde etiqueta, caja o identificación USB en modo cableado.
2. Comparar las fuentes originales y capturas disponibles, especialmente canal, Report ID, longitudes y respuesta de batería. En hidapi, el buffer de `hid_write` incluye el byte inicial del Report ID: un payload de 32 bytes sin ID explícito requiere comprobar el encuadre de 33 bytes con ID 0 en Windows.
3. Preparar una prueba explícita y limitada de consulta de batería, con timeout y validación de respuesta, únicamente tras revisar la evidencia. No implementar escrituras de RGB, remapeo o configuración como si estuvieran verificadas.
4. Si el software oficial funciona en alguna modalidad, capturar tráfico de una acción conocida con USBPcap/Wireshark para comparar con las hipótesis públicas. No modificar binarios ni firmware como paso automático.

## Prueba de batería preparada

`bateria_f75.exe` sin argumentos solo muestra el paquete; `bateria_f75.exe --consultar` envía una única consulta experimental. Busca una sola interfaz con los identificadores anteriores y comprueba que su descriptor coincida exactamente con el observado antes de escribir. Ante otra cantidad de candidatos o descriptor diferente, aborta. La escritura usa Report ID 0 seguido del payload de 32 bytes, conforme al contrato de `hid_write`; espera hasta 3 segundos o 20 informes y comprueba longitud, encabezado, porcentaje y checksum. Si no recibe una respuesta válida, no presenta un porcentaje. No envía inicialización ni cambia ajustes. No se ha ejecutado contra el dispositivo.

El vector de respuesta `20 01 00 64 ... 85` y el paquete de consulta proceden de [protocol_core.py](https://github.com/Ghost-CR/F75_Initializer/blob/master/aula_hacky/protocol_core.py); el código fuente denomina el paquete consulta de sesión, por lo que su interpretación como batería sigue siendo una hipótesis de otras implementaciones. [Documentación de hidapi](https://libusb.info/hidapi/group__API.html).

## Resultado observado en la unidad

El usuario ejecutó `bateria_f75.exe --consultar` en modo 2.4 GHz. Se abrió la interfaz 3 y se recibió una respuesta de 32 bytes: `20 01 00 5F`, seguida de ceros y checksum final `80`. El checksum coincide: `(0x20 + 0x01 + 0x5F) & 0xFF = 0x80`. La interpretación publicada del byte 3 da 95 %. La consulta y respuesta se observaron en esta unidad; que el valor represente el nivel real de batería sigue pendiente de contrastar con una fuente independiente o cambios de carga.

## Investigación RGB inalámbrico

[Ghost-CR/protocol_core.py](https://github.com/Ghost-CR/F75_Initializer/blob/master/aula_hacky/protocol_core.py) construye un informe de 32 bytes que comienza `05 10 00`, incluye modo, color en orden BGR, brillo, velocidad y una suma en byte 31. [El catálogo de aula-f75-max](https://github.com/tarikmanoar/aula-f75-max/blob/main/docs/packet-catalog.md) describe enviar primero `0F`, esperar 50 ms y enviar el informe del efecto. Ambas referencias proceden de la familia Max; no hay confirmación de que el efecto funcione en este teclado. El script Windows RGB de Ghost-CR trabaja por cable, no demuestra RGB por 2.4 GHz. El ID USB también aparece en otros teclados, por lo que por sí solo no identifica modelo ni protocolo.

Se agregó `rgb_preview.exe`, que solo genera y muestra un ejemplo rojo estático sin abrir dispositivos. El modo 1 y parámetros son hipótesis basadas en las referencias; no se ha enviado ni probado ningún comando RGB. Antes de habilitar envíos hay que conocer cómo recuperar el efecto actual y distinguir un cambio observado de un ACK genérico. La herramienta de batería no confirma la compatibilidad de RGB.

## Primera prueba RGB en este teclado

Con el receptor 2.4 GHz, se enviaron el paquete `0F` y después `05 10 00 01 00 00 FF ...`, con brillo y velocidad 3. El usuario observó iluminación azul, aunque la hipótesis inicial interpretaba el byte 6 como rojo. Se recibió un informe con encabezado `0F`, que corresponde al primer paquete; no demuestra por sí solo que el efecto se haya aplicado. La observación visual indica que el byte 6 probablemente controla azul en esta unidad. Se preparó una segunda prueba con `FF` en el byte 4 como posible rojo; aún no está verificada. La confirmación de verde y de la correspondencia completa requiere pruebas adicionales.

## Segunda prueba RGB en este teclado

El usuario ejecutó la prueba con `FF` en el byte 4 y observó rojo fijo. Junto con la prueba anterior, se ha observado que el byte 4 produce rojo y el byte 6 produce azul, con modo 1 y los demás parámetros de esta prueba. El byte 5 todavía no se ha verificado como verde. La respuesta recibida `0F ... 0F` corresponde al primer paquete de la secuencia, no es confirmación específica del efecto; el resultado se estableció por observación visual.

## Prueba del canal verde

El usuario ejecutó la secuencia con `FF` en el byte 5 y observó iluminación verde. Las tres pruebas confirman visualmente rojo, verde y azul en los bytes 4, 5 y 6 respectivamente, en modo fijo 1 y con brillo y velocidad 3, por el receptor y firmware probados. La mezcla de colores, otros brillos y modos permanecen sin validar.

## Mezclas RGB observadas

El usuario confirmó visualmente que `255 255 0` produce amarillo y que `255 255 255` produce blanco, usando modo fijo 1 y brillo 3. En la captura de blanco también aparecen comandos enviados en secuencia; solo se registra el resultado final blanco, sin atribuir observaciones a los colores intermedios.

## Investigación de brillo y efectos

Las referencias [Ghost-CR](https://github.com/Ghost-CR/F75_Initializer/blob/master/aula_hacky/protocol_core.py) y [aula-driver](https://github.com/thibaultmiclo/aula-driver/blob/main/aula/wireless.py) sitúan el modo en byte 3, brillo en byte 12 y velocidad en byte 13 de la trama de 32 bytes. Los valores y nombres de efectos pertenecen a otros modelos y no están confirmados en esta unidad. La herramienta `rgb_prueba.exe` admite `--brillo 1` a `5` y `--modo 1` o `2` para pruebas explícitas; sin `--aplicar` solo muestra el paquete. El modo 1 y brillo 3 se observaron antes; los demás valores aún no se han probado en el teclado. Cada envío cambia el ajuste actual completo, no solo el brillo. La GUI continúa usando modo 1 y brillo 3.

## Prueba de brillo en esta unidad

En modo fijo 1 y blanco, el usuario observó que el valor 1 no se veía, el 5 parecía máximo y el 3 se veía casi igual que el 5. Esto confirma que modificar el byte 12 cambia la iluminación en las condiciones probadas, pero no caracteriza la escala ni confirma que 1 sea apagado absoluto. Los valores 2 y 4 permanecen sin probar.

## Prueba de modo 2

El usuario ejecutó modo 2 con rojo y brillo 5 y no observó animación; describió la iluminación como roja fija. No se puede identificar el efecto con esta observación ni asegurar que los modos 1 y 2 sean idénticos. El usuario confirmó de nuevo que brillo 1 se ve apagado; en brillo 3 lo percibe como máximo. Los niveles 2 y 4 no se han caracterizado.

## Comparación adicional de brillo

En blanco, modo 1, el usuario observó intensidad aparentemente igual con valores 2 y 4; tampoco apreció diferencias claras frente a 3 y 5. Con valor 1 la iluminación parecía apagada. No se ha medido la intensidad, por lo que no se descartan diferencias pequeñas. No hay evidencia aún para presentar los valores 2–5 como una escala porcentual calibrada. Se propone probar escalado de las componentes RGB manteniendo el byte de brillo en 3; si funciona sería atenuación por color, no una lectura o ajuste calibrado de brillo físico.

## Atenuación por canales RGB

El usuario comparó blanco RGB `(64,64,64)` con `(255,255,255)`, ambos con brillo del protocolo 3, y observó que el primero se veía aproximadamente a la mitad. La interfaz gráfica incorpora una intensidad aproximada de 0 a 100 % que multiplica cada canal seleccionado antes de enviar; 100 % conserva el color elegido y 0 % envía RGB negro. La escala no es una medición calibrada de luz y el extremo 0 no se ha verificado físicamente. Se mantiene el brillo del protocolo en 3 y el modo fijo 1.

## Validación visual de intensidad desde la interfaz

El usuario probó el deslizador con rojo, aumentando y disminuyendo en intervalos del 25 %, y observó cambios de intensidad en ambos sentidos. Esto respalda el funcionamiento práctico del escalado RGB con ese color; no establece una escala de luz físicamente lineal ni valida todos los colores o efectos. No se ha confirmado por separado la respuesta exacta al extremo 0 %.

## Siguiente prueba de efectos

Se habilitó experimentalmente modo 3 en `rgb_prueba.exe`. [Aula-Manager](https://github.com/free-soldier28/Aula-Manager/blob/main/docs/PROTOCOL.md) describe una onda para el identificador 3 en otro F75 y protocolo, pero no establece el efecto de esta unidad. Modo 2 se observó estático aquí, así que no se extrapolan nombres. Se validó la construcción sin envío; falta prueba física. El comando de modo 1 permite volver al color fijo previamente observado.

## Resultado del modo 3

El usuario no observó animaciones al pulsar teclas ni cambios de intensidad después de la prueba del modo 3. Esta observación no identifica el modo ni demuestra que todos los efectos sean incompatibles: podría faltar otro parámetro, secuencia o corresponder a otra variante. La aplicación mantiene solo color fijo y atenuación por canales como funciones observadas.

## Modo 2 reactivo observado

Tras seleccionar con la perilla un efecto descrito como confeti estático, el usuario ejecutó `rgb_prueba.exe 255 0 0 --modo 2 --brillo 3 --aplicar` y observó iluminación individual al pulsar teclas. Esto contrasta con una prueba previa de modo 2 en la que no percibió animación. El estado anterior o los parámetros podrían influir; todavía no se ha aislado la causa ni verificado si el efecto es persistente. El resultado es observación del conjunto de la secuencia, no prueba de que el byte de modo por sí solo seleccione el efecto.

## Modo 2 reproducido

El usuario aplicó modo 1 con rojo y brillo 3 y observó rojo fijo. Después aplicó modo 2 con los mismos parámetros y volvió a observar iluminación individual al presionar teclas. La diferencia es reproducible bajo esta secuencia, aunque no se ha caracterizado el efecto con otros colores o niveles. La interfaz permite seleccionar modo fijo o reactivo y conserva la atenuación RGB aproximada.

## Modo 4 observado

Con modo 4, rojo y brillo 3, el usuario observó que la iluminación de las teclas se apagaba de forma aparentemente aleatoria; al regresar a modo 1 observó rojo fijo. Se añadió a la interfaz como «Apagado aleatorio», nombre descriptivo y no oficial. La respuesta recibida correspondía al primer paquete `0F`; la evidencia del efecto es visual.

## Modo 5 observado

Con modo 5, rojo y brillo 3, el usuario observó un efecto similar al modo 4, más rápido y parecido a gotas de lluvia. El modo 1 restableció el rojo fijo. Se añadió como «Lluvia» a la interfaz; el nombre es descriptivo y no oficial. El selector aún requiere prueba visual con este modo.

El usuario confirmó que el modo «Lluvia» funciona desde la interfaz gráfica. Se preparó el modo 6 para una prueba posterior; aún no se ha observado en el teclado.

## Modo 6 observado

El usuario seleccionó modo 6 con rojo y brillo 3 y observó confeti estático: teclas de diferentes colores sin animación. El resultado multicolor a pesar del rojo enviado sugiere que este modo puede usar una paleta propia; no se ha verificado cómo responde al selector de color o a la atenuación de canales. Se añadió como «Confeti estatico» en la interfaz; queda pendiente probar esa opción en la ventana.

El usuario confirmó que «Confeti estatico» funciona desde la interfaz gráfica. Se preparó el modo 7 para prueba; todavía no se ha observado su comportamiento.

## Modo 7 observado

El usuario ejecutó modo 7 con rojo y brillo 3 y observó un efecto de respiración con color estático. El modo 1 volvió al rojo fijo. Se añadió como «Respiracion» a la interfaz; aún falta verificar esa opción desde la ventana.

El usuario confirmó que «Respiracion» funciona desde la interfaz gráfica. Se preparó el modo 8 para prueba; todavía no se ha observado su comportamiento.

## Modo 8 observado

Con modo 8, rojo y brillo 3, el usuario describió un efecto «seamless» que cambia el color del teclado de forma gradual. Se añadió a la interfaz como «Transicion gradual de colores»; la opción y su respuesta al color e intensidad aún deben comprobarse desde la ventana.

El usuario confirmó que «Transicion gradual de colores» funciona desde la interfaz gráfica. Se preparó el modo 9 para prueba; todavía no se ha observado su comportamiento.

## Modo 9 observado

Con modo 9, rojo y brillo 3, el usuario identificó el efecto como ola y observó respiración en rojo. No se ha confirmado cómo cambiar la paleta o activar varios colores. Se añadió a la interfaz como «Ola»; la opción aún debe comprobarse desde la ventana.

El usuario confirmó que «Ola» funciona desde la interfaz gráfica. Se preparó el modo 10 para prueba; todavía no se ha observado su comportamiento.

## Modo 10 observado

Con modo 10, rojo y brillo 3, el usuario observó un efecto de ola de arriba hacia abajo. Se añadió a la interfaz como «Ola vertical»; aún falta comprobar la opción desde la ventana.

El usuario confirmó que «Ola vertical» funciona desde la interfaz gráfica. Se preparó el modo 11 para prueba; todavía no se ha observado su comportamiento.

## Modo 11 observado y aclaración del modo 9

Con modo 11, rojo y brillo 3, el usuario observó una ola horizontal. También aclaró que la ola del modo 9 comienza desde el centro. La interfaz los presenta como «Ola horizontal» y «Ola desde el centro», respectivamente; el nuevo selector aún requiere prueba en ventana.

El usuario confirmó que los efectos de ola funcionan desde la interfaz gráfica. Se preparó el modo 12 para prueba; todavía no se ha observado su comportamiento.

## Modo 12 observado

Con modo 12, rojo y brillo 3, el usuario observó una animación giratoria similar a un vórtice. Se añadió a la interfaz como «Vortice»; aún falta comprobar esa opción desde la ventana.

El usuario confirmó que «Vortice» funciona desde la interfaz gráfica. Se preparó el modo 13 para prueba; todavía no se ha observado su comportamiento.

## Modo 13 observado

Con modo 13, rojo y brillo 3, el usuario observó un efecto reactivo que ilumina la fila de la tecla pulsada. Se añadió a la interfaz como «Reactivo por fila»; aún falta comprobar esa opción desde la ventana.

El usuario confirmó que «Reactivo por fila» funciona desde la interfaz gráfica. Se preparó el modo 14 para prueba; todavía no se ha observado su comportamiento.

## Modo 14 observado

Con modo 14, rojo y brillo 3, el usuario observó que al pulsar una tecla la iluminación recorre horizontalmente las teclas. Se añadió a la interfaz como «Barrido horizontal reactivo»; aún falta comprobar esa opción desde la ventana.

El usuario confirmó que «Barrido horizontal reactivo» funciona desde la interfaz gráfica. Se preparó el modo 15 para prueba; todavía no se ha observado su comportamiento.

## Modo 15 observado

Con modo 15, rojo y brillo 3, el usuario describió una animación similar al barrido reactivo anterior, pero más suave. Se añadió a la interfaz como «Ola reactiva»; aún falta comprobar esa opción desde la ventana.

El usuario confirmó que «Ola reactiva» funciona desde la interfaz gráfica. Se preparó el modo 16 para prueba; todavía no se ha observado su comportamiento.

## Modo 16 observado

Con modo 16, rojo y brillo 3, el usuario observó una animación automática: las filas se iluminan horizontalmente una después de otra, de arriba hacia abajo. Se añadió como «Cascada por filas»; aún falta comprobar la opción desde la interfaz gráfica.

El usuario confirmó que «Cascada por filas» funciona desde la interfaz gráfica. Se preparó el modo 17 para prueba; todavía no se ha observado su comportamiento.

## Modo 17 observado

Con modo 17, rojo y brillo 3, el usuario describió una animación como un látigo que comienza desde el centro y se mueve de arriba hacia abajo en ambas direcciones. Se añadió como «Latigo desde el centro»; aún falta comprobar la opción desde la interfaz gráfica.

El usuario confirmó que «Latigo desde el centro» funciona desde la interfaz gráfica. Se preparó el modo 18 para prueba; todavía no se ha observado su comportamiento.

## Modo 18 observado

Con modo 18, rojo y brillo 3, el usuario observó olas rápidas que avanzan desde Ctrl izquierda hacia Delete en la esquina superior derecha. Se añadió como «Ola diagonal rapida»; aún falta comprobar la opción desde la interfaz gráfica.

El usuario confirmó que «Ola diagonal rapida» funciona desde la interfaz gráfica. Se preparó el modo 19 para prueba; todavía no se ha observado su comportamiento.

## Modo 19 observado

Con modo 19, rojo y brillo 3, el usuario describió un recorrido de las filas en direcciones opuestas. Se añadió como «Filas en direcciones opuestas»; aún falta comprobar la opción desde la interfaz gráfica.

El usuario confirmó que «Filas en direcciones opuestas» funciona desde la interfaz gráfica. Se preparó el modo 20 para prueba; todavía no se ha observado su comportamiento.

## Modo 20: primera observación

Con modo 20, rojo y brillo 3, el usuario observó cambios muy rápidos entre aproximadamente cuatro modos y luego una ola horizontal multicolor (arcoíris). Es una única observación; no se sabe si la transición es parte del efecto, un estado transitorio o si el modo está soportado. No se ha añadido a la interfaz. Conviene partir de modo 1, repetir una vez y observar el resultado antes de caracterizarlo.

Tras la prueba del modo 20, el usuario confirmó que podía seguir escribiendo por 2.4 GHz. Sospecha que podría ser un reinicio, pero no se observó pérdida de conexión ni se comprobó un restablecimiento de ajustes. Se deja sin clasificar y fuera del selector; no se infiere que sea el último modo disponible.

## Alineación del visualizador

Las 82 zonas visuales se calibraron en coordenadas de la imagen `assets/keyboard-actual-v1.png` (1774 × 887), incluidas las teclas anchas y las flechas. Se verificaron los límites y la ausencia de solapamientos, y se revisó la superposición en la ventana con color único y arcoíris. Estas zonas son únicamente visuales: no equivalen a índices de LED del firmware y las animaciones siguen siendo aproximaciones.

## Velocidad y multicolor: prueba del 15 de noviembre de 2026

Se releyeron los constructores de [Ghost-CR/F75_Initializer](https://github.com/Ghost-CR/F75_Initializer/blob/master/aula_hacky/protocol_core.py) y [aula-driver](https://github.com/thibaultmiclo/aula-driver/blob/main/aula/protocol.py), junto a su [envoltura inalámbrica](https://github.com/thibaultmiclo/aula-driver/blob/main/aula/wireless.py). Coinciden en situar la paleta en el byte 11 y la velocidad en el byte 13 del informe inalámbrico de 32 bytes. La implementación propia conserva el orden R/G/B observado aquí, el commit previo, la pausa de 50 ms, el brillo interno 3 y la comprobación exacta del descriptor. No se extrapolan comandos de receptores con otros VID/PID.

`rgb_prueba.exe` admite `--velocidad 1-5`, con 3 por defecto. El constructor compartido rechaza valores fuera del rango y recalcula el checksum. No se habilita el nivel 0. La herramienta conserva el comportamiento sin escrituras cuando falta `--aplicar`.

Se compararon primero modo 11, rojo `(255,0,0)`, brillo 3 y velocidad 1, y luego la misma configuración con velocidad 5. El usuario confirmó que 5 es más rápido. Se añadió únicamente `--multicolor` manteniendo velocidad 5, y confirmó la ola arcoíris. Las respuestas recibidas fueron ecos de commit `0F`; la confirmación del cambio visual proviene del usuario, no del eco.

Después se probó multicolor en tres familias adicionales, con velocidad 3:

| Modo | Efecto | Observación del usuario |
| --- | --- | --- |
| 1 | Color fijo | Varios colores por tecla, estáticos. |
| 2 | Reactivo al pulsar | Las teclas pulsadas muestran distintos colores. |
| 7 | Respiración | Respira con varios colores simultáneamente. |
| 11 | Ola horizontal | Cambia de ola roja a arcoíris. |

Esa primera ronda confirmó las cuatro combinaciones indicadas. La validación posterior se describe a continuación. No se generaliza a otras unidades ni a la escala física de los niveles intermedios de velocidad. Tampoco se ha calibrado la relación entre velocidad del firmware y tiempo de la simulación. En ese momento quedaba pendiente el brillo nativo; su integración se documenta más abajo.

La GUI incluye velocidad en edición, envío, recuperación y perfiles. Los modos 1 y 6 conservan el valor 3 al enviar y muestran el deslizador desactivado por ser estáticos. Las pruebas sin hardware comparan el paquete rojo conocido, aíslan los bytes de velocidad y paleta, comprueban el checksum, validan la migración de perfiles y recorren ambas skins a cuatro tamaños. La revisión con Raven/Vulkan comprobó el deslizador 5/5, los estados desactivados y la vista previa arcoíris sin pulsar Aplicar.

### Ampliación: catálogo completo en multicolor

Se aplicó individualmente cada combinación restante con RGB `(255,0,0)`, brillo interno 3, velocidad 3 y `colorful=1`. El usuario observó cada efecto antes de avanzar al siguiente. En los reactivos se pidieron pulsaciones de prueba. Las escrituras fueron aceptadas por la herramienta con su comprobación de descriptor; la evidencia del resultado visual es la confirmación del usuario.

| Modo | Efecto | Resultado confirmado con multicolor |
| --- | --- | --- |
| 4 | Apagado aleatorio | Conserva el efecto con varios colores. |
| 5 | Lluvia | Conserva la animación de lluvia multicolor. |
| 6 | Confeti estático | Varios colores por tecla, sin animación. |
| 8 | Transición gradual | Conserva la transición suave de colores. |
| 9 | Ola desde el centro | Conserva la salida desde el centro con varios colores. |
| 10 | Ola vertical | Conserva el recorrido vertical multicolor. |
| 12 | Vórtice | Conserva el giro con varios colores. |
| 13 | Reactivo por fila | Las filas reaccionan a pulsaciones con varios colores. |
| 14 | Barrido horizontal reactivo | Cada pulsación dispara el barrido multicolor. |
| 15 | Ola reactiva | Cada pulsación dispara la ola suave multicolor. |
| 16 | Cascada por filas | Conserva el recorrido sucesivo con varios colores. |
| 17 | Látigo desde el centro | Conserva el movimiento hacia ambos lados en multicolor. |
| 18 | Ola diagonal rápida | Conserva el recorrido diagonal multicolor. |
| 19 | Filas en direcciones opuestas | Conserva los recorridos contrarios con varios colores. |

Con las cuatro pruebas anteriores, quedan confirmadas las **18 opciones disponibles**: modos 1, 2 y 4–19. Los modos 3 y 20 continúan fuera del selector y no se enviaron en esta ronda. Confeti y Transición gradual ya generaban colores propios con la otra selección de paleta; esta ronda confirma que `colorful=1` también conserva sus comportamientos.

Las capacidades se reúnen en `kLightingEffects`: identificador, nombre, multicolor confirmado, paleta propia y presencia de animación. Los controles y la nota de compatibilidad consultan ese catálogo. No se considera una prueba de velocidad individual para todos los modos ni una lectura de los ajustes guardados en el firmware.

### Brillo nativo: confirmación e integración

El usuario informó que ya había probado el brillo nativo y funcionaba sin detalles en la gran mayoría de efectos, posiblemente todos. Se acepta esa observación sin repetir la ronda de pruebas. No proporcionó una relación por modo ni una curva de intensidad; no se registra una validación individual o un porcentaje físico de luz que no se haya medido.

El campo ya existente en byte 12 se expone como **Brillo del teclado**, niveles 1–5. Se releyó el [constructor de referencia](https://github.com/Ghost-CR/F75_Initializer/blob/master/aula_hacky/protocol_core.py) para comprobar su independencia de RGB, paleta y velocidad. Se mantienen el rango y el descriptor que ya usaba nuestra herramienta. El nivel inicial y el valor de perfiles antiguos siguen siendo 3. No se habilita 0 ni se interpreta el nivel 1 como apagado en todos los efectos.

La GUI envía `--brillo` también para multicolor y paletas propias, sin modificar sus canales RGB por el ajuste fino. En color único, el ajuste RGB de 0–100 sigue multiplicando los canales y se combina con el brillo nativo. El control fino se oculta en las otras paletas, mientras el brillo nativo permanece disponible. Todos los cambios requieren Aplicar; no se realizan nuevas escrituras físicas como parte de esta integración.

El brillo forma parte de edición, resumen, recuperación del último envío y perfiles v3. La lectura de v1 y v2 conserva el valor 3 y no escribe el archivo. Las pruebas aíslan byte 12 y checksum en los cinco niveles, conservan los demás campos, comprueban migración y persistencia, y validan la distribución de los controles en ambas skins y las tres clases de paleta. La vista previa multiplica su luz simulada por `nivel / 5`; es una aproximación gráfica, sin calibración del firmware.

## Perilla multimedia

El usuario confirmó giro de volumen y clic Silencio tras mantener pulsada la perilla 3–5 segundos. Se añadió una opción de sesión en Windows para sustituir Silencio por Reproducir/Pausar, global y desactivada al abrir. El 15 de noviembre de 2026 confirmó que el clic pausa y reanuda con la opción activa y que el giro conserva el volumen. No implica un comando nuevo de configuración HID ni escritura de firmware. Investigación y alcance en [perilla-multimedia.md](perilla-multimedia.md).
