# Desarrollo e investigación de AULA Control

Aplicación Windows para controlar iluminación, perfiles y gestos multimedia de la unidad EPOMAKER EA75 / AULA F75 comprobada en este proyecto, por USB y receptor 2.4 GHz. Incluye las skins Glass y Neumorfismo, español e inglés, batería inalámbrica y funcionamiento en bandeja. El control RGB/batería por Bluetooth sigue pendiente.

## Instalar la versión 1.0

El instalador generado es `dist/AULA-Control-1.0.0-Setup-x64.exe`. Instala para el usuario actual en `%LOCALAPPDATA%\Programs\AULA Control`, crea un acceso en Inicio y ofrece un acceso de escritorio opcional. Incluye los auxiliares y el runtime de C++; requiere Windows 10 2004 o posterior, x64, y un controlador gráfico compatible con Vulkan.

**Firma y SmartScreen:** la versión 1.0.0 se distribuye sin firma digital. Windows puede mostrar un aviso de SmartScreen o de editor desconocido; algunas políticas del equipo pueden impedir su ejecución. Descarga el Setup desde la publicación del proyecto y compara su SHA256 con el archivo `.sha256` adjunto. El hash comprueba que el archivo coincide con el publicado; no sustituye una firma digital. Información de Microsoft: [SmartScreen y reputación de aplicaciones](https://learn.microsoft.com/windows/apps/package-and-deploy/smartscreen-reputation).

Desde la carpeta de descarga puedes consultar el hash con PowerShell:

```powershell
Get-FileHash -Algorithm SHA256 .\AULA-Control-1.0.0-Setup-x64.exe
```

Actualizar o desinstalar conserva `%LOCALAPPDATA%\AulaControl`, donde están los perfiles y preferencias. Antes de instalar o desinstalar, termina la aplicación con **Salir de AULA Control** desde la bandeja o Preferencias; la X puede ocultarla. El inicio con Windows existente se migra a la ruta instalada. La primera instalación no lo activa por sí sola.

Para generar otra vez el paquete y ejecutar la compilación Release y sus pruebas:

```powershell
.\packaging\build-setup.ps1
```

Detalles de construcción, dependencias y validación del instalador: [docs/instalador-1.0.md](instalador-1.0.md).

## Diagnóstico HID

La herramienta de investigación enumera todas las interfaces HID y marca las que usan páginas vendor-defined (`0xFF00`–`0xFFFF`). Permite abrir una interfaz, mostrar su descriptor de informes y escuchar informes de entrada. La marca es una pista, no una identificación del teclado. No envía comandos ni modifica el dispositivo. Las herramientas de diagnóstico y pruebas no se incluyen en el instalador.

## Compilar

Se necesita CMake 3.20 o posterior, Git, conexión a Internet para descargar hidapi y un compilador C++17 para Windows, por ejemplo Visual Studio 2022 con la carga de trabajo **Desarrollo para el escritorio con C++**.

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\hid_scanner.exe
```

La ubicación del ejecutable puede variar con el generador de CMake. Para volver a enumerar, escribe `R`; para abrir una interfaz, escribe su número; la escucha es opcional y termina después de 10 informes o 10 segundos; también puedes detenerla con `Q`. Algunas interfaces pueden estar bloqueadas por Windows u otro programa, y algunas no producen informes de entrada espontáneos. El descriptor que se muestra es el descriptor de informes HID, no los descriptores USB completos.

## Identificar el teclado

Ejecuta el escáner con el teclado conectado por cable y compara los resultados al desconectarlo. Repite con el receptor de 2.4 GHz. Guarda VID, PID, interfaz, usage page y ruta de las interfaces que aparezcan o desaparezcan. El teclado y el receptor pueden presentar identificadores diferentes; otras interfaces vendor-defined pueden pertenecer a dispositivos distintos. Los campos de protocolo, tamaños y funciones de configuración deben verificarse con evidencia de esta unidad antes de implementar escrituras.

Las rutas y números de serie pueden identificar dispositivos concretos: revísalos antes de compartir la salida. La captura de informes puede contener datos de entrada; selecciona la interfaz cuidadosamente.

## Conexiones USB y 2.4 GHz

El receptor probado es `EA75 Dongle` (`05AC:024F`). En modo cableado, esta unidad se identifica como `EPOMAKER EA75` (`0C45:8006`); requiere un protocolo diferente. La build habitual en `build/Release/` incorpora ambas rutas. Se validaron las respuestas USB y el rojo fijo en esta unidad; el alcance de las comprobaciones físicas se detalla en [soporte USB](soporte-usb.md). Bluetooth aún no tiene control RGB/batería implementado.

Los auxiliares seleccionan USB con prioridad cuando aparece el canal compatible en modo cableado, aunque el receptor siga conectado. Al retirar el cable vuelven a seleccionar el receptor. La GUI puede restaurar el último efecto al verificar la nueva conexión, según **Preferencias → Restaurar último efecto**. El nombre de producto y el descriptor completo se verifican: compartir VID/PID no basta para admitir otro teclado.

## Color RGB fijo

Con el receptor conectado, indica rojo, verde y azul como enteros de 0 a 255. Sin `--aplicar` solo se muestran los paquetes; con la opción se envían. Por ejemplo:

```powershell
.\build\Release\rgb_prueba.exe 255 255 0
.\build\Release\rgb_prueba.exe 255 255 0 --aplicar
```

El ejemplo solicita amarillo. El programa comprueba el canal y descriptor observados, y usa brillo inicial 3/5. Los canales individuales y las mezclas amarillo y blanco se verificaron visualmente por 2.4 GHz; las validaciones USB se registran por separado. No recupera el efecto anterior automáticamente. Para exigir una ruta, añade `--transporte usb` o `--transporte 2g`: si la selección cambió, el envío se bloquea. Sin `--aplicar` no enumera ni abre dispositivos.


## Menú de control

Ejecuta desde PowerShell `powershell -NoProfile -ExecutionPolicy Bypass -File .\control.ps1` para consultar batería o elegir un color RGB fijo. Introduce cada canal de 0 a 255; `Q` cancela la entrada del color. El menú necesita los ejecutables Release ya compilados y utiliza las mismas comprobaciones que las herramientas individuales. El brillo permanece en 3/5 y no se restaura automáticamente el efecto anterior.

## Interfaz gráfica Raven

Ejecuta `.\build\Release\aula_control.exe` con el receptor 2.4 GHz conectado o con el teclado en modo cableado USB. Selecciona un color y pulsa **Aplicar al teclado**. Por 2.4 GHz, la batería se consulta al iniciar y después de cada intervalo elegido en **Dispositivo** (30 o 60 segundos). Si una consulta falla, la última lectura válida permanece marcada como anterior; sin lecturas válidas se muestra `--`. Por USB se muestra la conexión y batería no disponible, sin presentar porcentajes del receptor ni consultar batería periódicamente. La ventana usa `rgb_prueba.exe`, `bateria_f75.exe` y `hidapi.dll` desde la misma carpeta; conserva esos archivos juntos. El brillo nativo comienza en 3/5 y puede ajustarse desde Iluminación. La aplicación requiere un entorno compatible con Vulkan para la interfaz Raven. La consulta y el envío se ejecutan en segundo plano con un límite de espera de ocho segundos; se cancelan al cerrar la ventana.

Al abrir aparece una pantalla de preparación animada con el estado real de la comprobación. **Entrar a la aplicación** permite empezar a editar mientras se completa. La pantalla termina al recibir la primera respuesta o un resultado de ausencia/error; no añade una espera artificial. Los perfiles y preferencias se preparan aunque el receptor no esté conectado.

La cabecera muestra por separado el estado de conexión y la batería recibida por 2.4 GHz. El canal se busca cada cinco segundos mediante enumeración, sin enviar comandos. Al cambiar de conexión se exige verificar el nuevo canal; al volver a 2.4 GHz se programa una consulta de batería inmediata. Con **Restaurar último efecto** activado, se reenvían los últimos ajustes guardados después de verificar la comunicación, una vez por conexión. **Aplicar** queda desactivado si no hay un canal, hay varios candidatos o su interfaz no se pudo verificar.

**Brillo del teclado** ajusta el nivel nativo de **1 a 5**, también en multicolor y en efectos con paleta propia. **Ajuste RGB** conserva la atenuación fina de 0 a 100 % para color único; se oculta cuando el teclado genera los colores. Ambos ajustes se combinan en color único. Pulsa **Aplicar al teclado** para enviar el cambio: editar solo actualiza la vista previa. Los niveles nativos y el porcentaje RGB no son una medición calibrada de luz.

El usuario confirmó el funcionamiento del brillo nativo en la gran mayoría de efectos, sin enumerar excepciones. Se integra esa confirmación sin repetir las pruebas físicas ni presentarla como una validación individual de cada modo. El resultado puede variar según el efecto. No se habilita el valor nativo 0 ni se interpreta el nivel 1 como un apagado universal.

### Velocidad y multicolor

En **Iluminación**, el selector **Velocidad** ofrece niveles de 1 a 5 y comienza en 3. La comparación física de Ola horizontal confirmó que 5 es más rápido que 1. No representa segundos ni una frecuencia calibrada. La vista previa adapta su ritmo de forma aproximada; pulsa **Aplicar al teclado** para enviar el cambio real. En Color fijo y Confeti estático la velocidad queda desactivada.

**Paleta → RGB arcoíris** se comprobó visualmente en esta unidad con las **18 opciones del selector**, conservando el comportamiento observado de cada efecto. Incluye fijo (colores estáticos por tecla), respiración, lluvia, olas, vórtice, reactivos y recorridos. Confeti estático y Transición gradual usan su propia paleta, y también conservaron su comportamiento al activar multicolor. La nota reconoce los efectos probados. El selector RGB queda desactivado y su ajuste fino se oculta cuando la paleta la genera el teclado; el brillo nativo permanece disponible y la velocidad se habilita en los efectos animados. No se generaliza esta prueba a otras variantes del F75.

El brillo nativo y la velocidad se conservan al guardar perfiles y al recuperar el último envío. Los perfiles anteriores se leen con brillo 3 y, si no guardaban velocidad, con velocidad 3. El formato se actualiza únicamente al guardar o modificar la biblioteca, conservando los otros perfiles.

Para repetir una comparación desde PowerShell, ejecuta cada línea por separado y observa el teclado entre cambios:

```powershell
.\build\Release\rgb_prueba.exe 255 0 0 --modo 11 --velocidad 1 --aplicar
.\build\Release\rgb_prueba.exe 255 0 0 --modo 11 --velocidad 5 --aplicar
.\build\Release\rgb_prueba.exe 255 0 0 --modo 11 --velocidad 5 --multicolor --aplicar
```

La herramienta también admite `--brillo 1-5`, que modifica únicamente el campo nativo de brillo y su checksum. Sin esa opción usa 3, como antes. La interfaz añade la opción elegida tanto en color único como en multicolor.

Sin `--aplicar`, la herramienta solo imprime los paquetes. La construcción y el checksum tienen pruebas independientes sin acceder al teclado.

La ventana principal organiza el selector de color, los efectos, el brillo nativo y el ajuste RGB en paneles, y muestra la última consulta de batería. Los efectos multicolor pueden usar una paleta interna; el brillo nativo actúa sobre esa iluminación sin atenuar sus colores mediante RGB. Una consulta válida informa el porcentaje recibido, cuya precisión real aún no está calibrada.

El selector incluye muestras de colores rápidos y una vista previa del color seleccionado. Elegir una muestra o mover el brillo o el ajuste RGB solo cambia la selección en pantalla; pulsa **Aplicar al teclado** para enviarla. La vista previa representa ambos ajustes de forma aproximada y no reproduce la curva física de luz del firmware.

La representación del teclado se basa en la imagen de referencia y muestra una simulación aproximada del color, la intensidad y el efecto seleccionado. No lee el estado real del teclado ni reproduce con exactitud las animaciones del firmware; los efectos reactivos se ilustran sin capturar pulsaciones. La vista previa no configura teclas individuales. El tamaño mínimo de la ventana es de 900 × 760 unidades para mantener visibles los controles.

La ventana abre a 1440 × 900 en **Inicio**, con navegación lateral hacia Iluminación, Perilla, Perfiles y Dispositivo. Inicio muestra el último efecto enviado y accesos a la configuración, junto al resumen de iluminación, biblioteca, acción multimedia y lectura de batería. Iluminación conserva la vista previa editable: los cambios pendientes no sustituyen el efecto de Inicio. Por debajo de 1240 unidades de ancho, la barra se compacta a iconos con nombres accesibles y ayudas al pasar el cursor. El teclado ocupa el espacio flexible de Iluminación, los colores rápidos permanecen visibles y Aplicar se mantiene al pie del panel de ajustes. Los paneles comparten radios, márgenes y sombras de luz/sombra para el relieve neumórfico. Detalles de esta base visual y de la futura pasada de acabados: [docs/diseno-interfaz.md](diseno-interfaz.md).

**Recuperar último envío** devuelve a la vista previa los últimos ajustes guardados, también de sesiones anteriores; no envía cambios por sí solo. Sin un envío guardado queda desactivado e Inicio muestra **Sin efecto guardado**. Cuando la biblioteca está vacía, **Preparar mi primer perfil** guía hacia Iluminación. Guardar se habilita con un nombre válido.

El último envío se guarda en `%LOCALAPPDATA%\AulaControl\last-lighting.txt`, separado de los perfiles: efecto, RGB, paleta, ajuste RGB, brillo nativo y velocidad. Al reabrir se recuperan esos controles. **Preferencias → Restaurar último efecto**, activado según la elección del usuario, también los envía automáticamente al iniciar o recuperar la conexión, incluso en la bandeja. Se puede desactivar sin borrar la memoria. Editar o fallar un envío no reemplaza el último guardado. Las dos vistas comparten el tiempo de animación cuando muestran esos mismos ajustes; se compensa la espera del auxiliar a partir de su hora de envío. La fase sigue siendo aproximada: no hay lectura validada del efecto o de los LED del firmware, ni detección de cambios hechos con la perilla. Detalles y pruebas: [docs/memoria-iluminacion.md](memoria-iluminacion.md).

### Skin Glass

En **Apariencia** puedes alternar entre **Glass** y **Neumorfismo** sin reiniciar la aplicación. Glass utiliza paneles de vidrio esmerilado, desenfoque y reflejos de Raven/Vulkan sobre un fondo de luz difusa. La composición toma como referencia visual el video de Glassy Music aportado por el usuario; su implementación es propia y no incorpora código de esa aplicación.

El fondo ofrece **Aurora**, con luz cian y violeta, o **Seguir el color seleccionado**, que acompaña el color de la vista previa local. La paleta multicolor utiliza Aurora. El fondo se calcula cuando cambia el color y su textura se reutiliza mientras permanece igual. No captura el escritorio ni necesita conexión a Internet o nuevas imágenes.

La skin y el ambiente se conservan en `%LOCALAPPDATA%\AulaControl\appearance.txt`. Glass es el estilo inicial cuando todavía no hay preferencia guardada. Cambiar la apariencia no envía comandos al teclado ni modifica los perfiles o la opción multimedia.

## Perilla y multimedia

La pestaña **Perilla y multimedia** explica el modo nativo: mantén pulsada la perilla unos 3–5 segundos para alternar entre iluminación y multimedia. En la unidad probada, girar a la izquierda baja el volumen, girar a la derecha lo sube y el clic corto silencia.

La opción **Activar clics multimedia** empieza desactivada la primera vez y recuerda después tu elección, también al iniciar con Windows. Mientras AULA Control esté abierta, incluso en la bandeja, sustituye Silencio de **todos los dispositivos y programas**: un clic reproduce/pausa, dos clics rápidos pasan a la siguiente pista y tres envían Pista anterior. El reproductor decide si esta última acción reinicia la canción o vuelve a la anterior. El giro conserva el volumen nativo.

Un filtro de 100 ms ignora pares de eventos duplicados sin sumarlos como otro clic ni retrasar el gesto. Uno o dos clics esperan hasta 400 ms después de la última liberación para distinguirlos de un gesto más largo. El triple clic responde al soltar la tercera pulsación; los clics adicionales del mismo grupo no repiten la acción. Las pulsaciones mantenidas no generan acciones multimedia. Desactivar la opción o salir de AULA retira la sustitución; salir conserva la preferencia para la próxima apertura. No necesita PowerToys ni cambia el firmware.

La prueba física confirmó el volumen y, el 17 de noviembre, los tres gestos multimedia. También se comprobó que la opción sigue activa al salir y reabrir AULA. Referencias investigadas y límites: [docs/perilla-multimedia.md](perilla-multimedia.md).

## Bandeja y preferencias generales

La sección **Preferencias** permite cerrar o minimizar a la bandeja, recordar tamaño/posición, iniciar con Windows, abrir en segundo plano y restaurar el último efecto de iluminación. Por defecto, la X oculta la ventana y conserva la batería y la opción multimedia de la sesión. Para terminar, usa **Salir de AULA Control** en Preferencias o en el menú del icono junto al reloj. Si el icono no está visible, revisa los iconos ocultos de Windows.

En **Preferencias → General → Idioma** puedes elegir **Español** o **English**. El cambio es inmediato y se recuerda al abrir de nuevo. Incluye navegación, efectos, estados, ayudas y menú de la bandeja; los nombres de los perfiles se conservan. Los textos de la interfaz se limitan a controles, instrucciones y estados, sin lemas promocionales. Las herramientas de investigación por terminal mantienen su salida actual.

Un clic en el icono restaura la ventana; su menú ofrece Abrir, Preferencias y Salir. Abrir otra vez el ejecutable recupera la instancia existente. Iniciar con Windows es opcional y comienza desactivado; al activarlo abre en segundo plano para el usuario actual. Los ajustes generales, el intervalo de batería y la elección de clics multimedia se guardan en `%LOCALAPPDATA%\AulaControl\preferences.txt`. Detalles y verificación: [docs/bandeja-preferencias.md](bandeja-preferencias.md).

## Rendimiento y estabilidad

Las herramientas y la metodología para medir CPU, RAM y GPU están en [docs/pruebas-recursos.md](pruebas-recursos.md). La simulación RGB limita sus actualizaciones a 30 por segundo y se detiene al minimizar u ocultar la ventana.

Resultados de la sesión medida: [docs/resultados-recursos-2026-11-16.md](resultados-recursos-2026-11-16.md).

## Perfiles de iluminación

La pestaña **Perfiles** permite guardar hasta 24 combinaciones con nombre: color RGB, efecto, brillo nativo, ajuste RGB, paleta y velocidad. Primero prepara la combinación en **Iluminación**, después escribe un nombre y pulsa **Guardar**. Usar exactamente el mismo nombre actualiza ese perfil.

Selecciona un perfil y pulsa **Cargar en vista previa** para recuperar sus ajustes. Se abre Iluminación para revisarlos; pulsa **Aplicar al teclado** para enviarlos. Guardar y cargar perfiles no envía comandos de iluminación. El indicador bajo el selector distingue los cambios pendientes de los ajustes que coinciden con el último envío guardado, y señala si se ha enviado en la conexión actual; no lee el estado del firmware.

Los perfiles se conservan en `%LOCALAPPDATA%\AulaControl\lighting-profiles.txt`, también al cerrar la aplicación. **Eliminar** borra el perfil seleccionado y **Deshacer borrado** recupera el último borrado de esta sesión. El arranque recupera el último envío de iluminación y la preferencia multimedia guardada. La restauración automática usa esa memoria independiente; no elige un perfil de la biblioteca ni aplica cambios pendientes.

Si el archivo guardado es ilegible o usa otro formato, la aplicación evita sobrescribirlo. Detalles del almacenamiento y comprobaciones: [docs/perfiles-iluminacion.md](perfiles-iluminacion.md).

## Estado del dispositivo

La pestaña **Dispositivo** muestra el porcentaje recibido, una barra animada, la antigüedad de la última lectura y el próximo intento. Puedes elegir consultas cada **30 o 60 segundos**; la elección se recuerda para la próxima apertura. La primera ejecución usa 60 segundos. No es necesario pulsar un botón para actualizar.

Una consulta fallida no equivale a batería vacía. **Receptor no detectado** procede de la enumeración; **Teclado sin respuesta** significa que se detecta el receptor pero no llegó una respuesta válida. El reposo, apagado o falta de alcance son posibilidades, no estados que el protocolo identifique con certeza. También hay mensajes para un receptor no accesible, varios candidatos y una interfaz no verificable. La lectura anterior permanece en ámbar y se reintenta automáticamente. El porcentaje sigue siendo experimental, sin calibración física de precisión. Las vistas previas y el indicador de arranque dejan de solicitar redibujos al ocultarse o minimizarse.

Más detalles sobre el estado, los tiempos de espera y la validación: [docs/estado-dispositivo.md](estado-dispositivo.md).

Para ejecutar las comprobaciones del controlador y la interfaz:

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
