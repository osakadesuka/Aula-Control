# Perilla y controles multimedia

## Evidencia de esta unidad

El usuario confirmó que, tras mantener pulsada la perilla unos 3–5 segundos, el giro cambia el volumen y el clic corto silencia. Esto permite conservar el volumen nativo y sustituir únicamente la acción de Silencio en Windows. No se ha identificado un comando de firmware para guardar Reproducir/Pausar en esta variante `05AC:024F`.

El usuario eligió expresamente una sustitución global, desactivada inicialmente. El 17 de noviembre pidió recordar la activación y añadir dos y tres clics. La pestaña indica el alcance, las acciones y el estado. La casilla no cambia automáticamente el modo físico de la perilla: si sigue controlando RGB, hay que mantenerla pulsada para pasar a multimedia.

El 20 de noviembre confirmó también el giro de volumen y el clic Reproducir/Pausar por cable USB (`EPOMAKER EA75`, `0C45:8006`), tras poner la perilla en modo multimedia. En esa sesión se investigó después un conteo intermitente de clics y, tras instalar el filtro de duplicados descrito abajo, el usuario confirmó que los tres gestos cuentan correctamente. La validación anterior de los tres gestos fue por 2.4 GHz.

## Referencias investigadas

- [Reddit: Aula F75 Knob rebind](https://www.reddit.com/r/Epomaker/comments/1dv2w37/aula_f75_knob_rebind/): usuarios describen el cambio de modo con pulsación larga y el clic Silencio. Una respuesta propone cambiar Silencio por Reproducir/Pausar con PowerToys, con respuestas favorables. Son experiencias de otras unidades; la prueba del usuario confirma el modo nativo en la nuestra.
- [Aula Lights, código Dial.swift](https://github.com/rushi007941/aula-lights/blob/main/Dial.swift): implementación abierta para macOS con acciones de perilla y multimedia. Identifica los usos de entrada `0xE9`, `0xEA` y `0xE2` como derecha, izquierda y clic. Es una referencia de arquitectura; sus API de macOS no se incorporan al programa Windows y sus comandos de configuración no validan esta variante.
- [Microsoft PowerToys Keyboard Manager](https://learn.microsoft.com/en-us/windows/powertoys/keyboard-manager): referencia de remapeo mientras el programa está en ejecución. Nuestra opción funciona sin instalar PowerToys.
- [Microsoft: LowLevelKeyboardProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc) y [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput): documentación de las API Windows usadas en la implementación propia.

## Implementación y límites

`src/media_knob.hpp` instala `WH_KEYBOARD_LL` en un hilo con bucle de mensajes cuando la preferencia está activa, también al restaurarla durante el arranque. Para `VK_VOLUME_MUTE`, consume la entrada y agrupa los clics con `src/media_clicks.hpp`:

| Pulsaciones cortas | Acción enviada a Windows |
| --- | --- |
| Una | `VK_MEDIA_PLAY_PAUSE` |
| Dos | `VK_MEDIA_NEXT_TRACK` |
| Tres | `VK_MEDIA_PREV_TRACK` |

La ventana entre pulsaciones es de 400 ms desde la última liberación válida. Una pulsación posterior a ese plazo inicia otro grupo. Uno o dos clics esperan al final del grupo, evitando Pausa antes de Siguiente o Anterior. El triple responde al soltar la tercera pulsación, al ser el gesto máximo; queda marcado como enviado hasta terminar el grupo. Los clics posteriores del mismo grupo no vuelven a enviar Anterior ni generan una Pausa adicional. Un fallo de envío tampoco se reintenta con los clics extra.

El filtro de duplicados ignora un nuevo par pulsar/soltar si empieza a menos de 100 ms del inicio de la última pulsación aceptada. Consume ambos eventos sin incrementar el conteo ni ampliar el plazo original. Una separación de exactamente 100 ms sí cuenta; las repeticiones de KeyDown mientras se mantiene pulsado tampoco suman clics. Una pulsación superior a 600 ms cancela el grupo pendiente. Desactivar o salir también reinicia el filtro. No se añaden esperas dentro del hook: un temporizador de mensajes existe únicamente mientras hay un grupo pendiente. Se descartan grupos vencidos hace más de un segundo para no reproducir una acción antigua después de una suspensión o bloqueo del hilo.

Los demás eventos pasan de largo; subir/bajar volumen no se sustituye. Se aceptan eventos Silencio marcados como inyectados porque esta perilla también llega por esa vía. Cada acción envía un par pulsar/soltar mediante `SendInput`, etiquetado en `dwExtraInfo` para excluir la salida propia del remapeo. Referencias de temporización y teclas: [SetTimer](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-settimer) y [códigos de teclas virtuales](https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes).

El hook no informa del dispositivo de origen, por lo que afecta a Silencio de otros teclados, controles y software que envíe esa tecla. No registra texto. La preferencia se guarda atómicamente en `preferences.txt` versión 3; una instalación nueva empieza desactivada y posteriormente recuerda tanto activar como desactivar. Las versiones 1/2 se migran conservando sus ajustes y sin activar la función hasta elegirla.

Desactivar o salir cancela gestos pendientes, elimina el temporizador y retira el hook. Un mutex evita dos instancias activas de esta función en la misma sesión. Si falla la instalación, se muestra el error y se conserva la elección para reintentar en la próxima apertura; la casilla permite desactivarla. Un fallo de `SendInput` se informa sin repetir la acción ni reenviar Silencio con retraso; si solo se insertó la pulsación, se intenta liberar la tecla.

Windows dirige la tecla multimedia al reproductor que la atienda; el estado confirma el envío, no que el contenido haya cambiado. Pista anterior puede reiniciar la canción actual o volver a la anterior según el reproductor. `SendInput` puede estar limitado por los niveles de integridad de los procesos. Otros remapeadores también pueden interferir.

## Verificación

El 17 de noviembre de 2026 se compiló Release y pasaron las diez pruebas CTest. La lógica de gestos se comprueba con un emisor simulado: uno/dos/tres clics sin acciones intermedias, límite de grupo, repeticiones, pulsación larga, salida propia, cancelación, grupo obsoleto, reloj de 64 bits y fallo de envío sin duplicados. No pulsa teclas multimedia reales. Las pruebas de escritorio verifican lectura y escritura de la preferencia activada/desactivada y compatibilidad de formatos. El modo `--test` de la GUI comprueba las etiquetas de ambos estados en dos idiomas, dos skins y cuatro tamaños, sin activar el hook ni consultar HID.

Se revisó la ventana real con Raven/Vulkan: activar, salir completamente y volver a abrir en segundo plano conserva los tres gestos en estado Activo. El usuario confirmó físicamente que funcionan los tres: un clic reproduce/pausa, dos avanzan y tres envían Anterior. Se conservaron el idioma English, el intervalo de batería de 30 segundos y la entrada de inicio con Windows. No se reinició Windows. La build previa validada durante dos días se conservó en `build/validated-2026-11-17/`, junto con una copia de las preferencias anteriores. No se modificaron perfiles ni se enviaron cambios RGB.

El 15 de noviembre el usuario había confirmado físicamente el volumen y Reproducir/Pausar de la implementación anterior, que todavía desactivaba la opción en cada apertura.

## Corrección del filtro de entrada

En una prueba posterior el clic seguía silenciando. El diagnóstico en vivo del 15 de noviembre recibió cuatro pulsaciones Silencio, cinco Bajar volumen y quince Subir volumen, todas con `LLKHF_INJECTED`; no hubo conversiones. El filtro anterior descartaba todos esos eventos. Esto identifica el motivo del descarte, sin identificar qué componente genera la marca.

Se sustituyó ese filtro por una etiqueta propia `dwExtraInfo` en la salida. El alcance global incluye ahora explícitamente Silencio generado por software. La prueba de regresión cubre entradas externas, repeticiones y la exclusión de la etiqueta propia.

La segunda prueba recibió cuatro entradas Silencio marcadas como inyectadas y observó cuatro eventos Reproducir/Pausar: `Remapped=4`, `Failures=0`. El usuario confirmó que los clics pausaban y reanudaban el contenido. Las seis comprobaciones automáticas de aquella build pasaron con la corrección; entonces la casilla todavía se desactivaba al abrir.

Para repetir el diagnóstico de 30 segundos se puede ejecutar `media_knob_test.exe --diagnose` con la opción desactivada en otras instancias. Activa temporalmente el remapeo, cuenta únicamente seis teclas multimedia (Silencio, Volumen −/+, Reproducir/Pausar, Siguiente y Anterior) y muestra acciones enviadas y fallos; termina retirando ambos hooks. No registra las demás teclas ni guarda preferencias.

## Conteo de clics y respuesta — 20 de noviembre de 2026

El usuario informó de clics individuales que avanzaban de canción y grupos triples que a veces se interpretaban como otros gestos, y propuso bajar la espera a 100 ms. Se añadió un observador temporal `media_knob_test.exe --observe`: durante 90 segundos escucha exclusivamente las seis teclas multimedia anteriores y sus liberaciones, con tiempos relativos, marca de inyección y etiqueta de salida propia. No remapea ni genera entrada; puede funcionar junto a AULA. El callback escribe en un buffer fijo de hasta 1024 eventos y el diagnóstico los imprime después de retirar el hook. No guarda teclas de escritura normal.

La captura USB produjo 100 eventos multimedia. El quinto clic individual entregó dos pares Silencio completos: el primero empezó en 39562 ms y el segundo en 39578 ms, solo 16 ms después. La build anterior envió Siguiente a 39984 ms. La marca `LLKHF_INJECTED` estaba presente en las entradas; no se identificó qué componente duplicó el evento. Los intervalos de los clics intencionados dentro de grupos fueron aproximadamente 250–350 ms. Por eso una ventana de agrupación de 100 ms separaría esos dobles/triples en acciones individuales.

Se incorporó el filtro de 100 ms y la respuesta inmediata del triple. La regresión reproduce los 34 pares Silencio de la captura y verifica cinco acciones Reproducir/Pausar, cinco Siguiente y seis Anterior, eliminando el Siguiente incorrecto del quinto clic individual. También comprueba el umbral 99/100 ms, que los duplicados no retrasen la salida, el reinicio del filtro y el fallo del triple sin reintentos. Pasaron las once pruebas CTest tanto en la candidata como en `build/Release/`. Se actualizó el ejecutable habitual y su alias; perfiles y preferencias permanecieron intactos. El usuario abrió la nueva build y confirmó físicamente que ahora cuenta correctamente los tres gestos, sin avanzar de canción con un clic individual.

La build USB previa a esta corrección se conservó en `build/validated-usb-2026-11-20/`, con sus auxiliares y copia de preferencias. El diagnóstico local está en `build/wired-research/media-edges-usb.txt`.
