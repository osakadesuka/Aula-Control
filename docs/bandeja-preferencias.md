# Bandeja y preferencias generales

AULA Control mantiene una sola instancia por sesión de Windows. Ejecutar otra vez el programa restaura su ventana, incluso si estaba oculta. Un duplicado iniciado con `--background` no interrumpe la sesión existente. El evento de restauración también funciona si la primera instancia aún está creando su interfaz.

## Uso diario

El icono junto al reloj utiliza el recurso de la aplicación. Un clic abre la ventana; su menú contextual ofrece **Abrir AULA Control**, **Preferencias** y **Salir de AULA Control**. El texto del icono muestra comunicación y última batería, marcada como anterior cuando corresponde. Windows puede situarlo dentro de los iconos ocultos.

La nueva sección Preferencias contiene:

- **Idioma**: Español o English, aplicado sin reiniciar y guardado para la siguiente apertura. El menú de la bandeja utiliza el idioma elegido.

- **Cerrar a la bandeja**, inicialmente activado: la X oculta la ventana y conserva la sesión. Desactivado, la X termina la aplicación.
- **Minimizar a la bandeja**, inicialmente desactivado: sustituye la minimización normal por ocultación.
- **Recordar tamaño y posición**, inicialmente activado: conserva rectángulo normal y estado maximizado, sin restaurar el estado minimizado. Windows ajusta posiciones completamente fuera de pantalla al cambiar los monitores.
- **Iniciar con Windows**, inicialmente desactivado si no hay una entrada válida para este ejecutable: registra solo al usuario actual, sin elevación, usando `--background`.
- **Abrir en segundo plano**, inicialmente desactivado: también oculta una nueva apertura manual. Ejecutarlo otra vez restaura la ventana.
- **Restaurar último efecto**, activado según la elección del usuario: reenvía los últimos ajustes guardados al abrir o recuperar la conexión, después de verificar la comunicación. Desactivarlo conserva la memoria y la vista previa sin enviarlas. No selecciona un perfil ni aplica ediciones pendientes.
- Acciones explícitas **Ocultar en la bandeja** y **Salir de AULA Control**.

Ocultar conserva consultas, detección, selección local y estado multimedia de la sesión. Ocultar o mostrar la ventana no envía iluminación por sí mismo; la restauración al iniciar o reconectar también puede ejecutarse mientras está en la bandeja. La opción global de clics multimedia comienza desactivada en la primera apertura y después recuerda la elección, tanto activada como desactivada. Al abrir en segundo plano se restaura también. Salir cancela auxiliares y gestos pendientes, retira el remapeo, quita el icono y libera la instancia sin borrar esa preferencia.

Oculta o minimizada, la ventana no solicita animaciones del teclado ni del arranque; el bucle espera hasta 250 ms entre comprobaciones. Esto no constituye una medición de consumo. Si no se puede añadir el icono, la ventana queda accesible y cerrar termina el programa. Se reintenta añadirlo cada cinco segundos. `TaskbarCreated` permite reconstruirlo tras reiniciarse Explorer; si se pierde la bandeja con la ventana oculta, se restaura la ventana.

## Persistencia e inicio

`%LOCALAPPDATA%\AulaControl\preferences.txt`, formato `AULA_PREFERENCES 4`, guarda comportamiento de ventana, apertura oculta, intervalo de batería 30/60, idioma, clics multimedia, restauración de iluminación y posición. Está separado de perfiles, apariencia y [memoria del último envío](memoria-iluminacion.md). La primera línea de valores termina en idioma (`es`/`en`), multimedia (`0`/`1`) y restauración (`0`/`1`). Se leen los formatos 1, 2 y 3 conservando sus ajustes: multimedia comienza desactivado para 1/2; la restauración comienza activada según la elección del usuario, pero requiere una memoria de iluminación válida. La escritura es atómica. Archivos ilegibles o formatos desconocidos no se sobrescriben: los ajustes siguen disponibles en la sesión con un aviso.

El inicio con Windows se consulta en `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, valor `AULA Control`. Solo se modifica al accionar su casilla; guarda una ruta entre comillas y `--background`. Si se mueve el programa hay que reactivar la opción desde su nueva ubicación. La aplicación no altera la aprobación de inicio de Windows: si el usuario lo deshabilita allí, debe habilitarlo desde Windows. No se ha probado un inicio de sesión real.

## Verificación

Las doce pruebas CTest cubren persistencia, datos inválidos, protección de formatos futuros, restauración única entre instancias y registro/borrado de inicio en una clave aislada de prueba. No registran el programa de prueba para iniciar con Windows. La GUI recorre siete secciones, dos idiomas, dos skins y cuatro tamaños, incluyendo los controles de los tres gestos y la restauración del último efecto. Las pruebas de preferencias comprueban lectura de versiones 1/2/3/4 y conservación independiente de las elecciones multimedia y restauración, activadas y desactivadas.

La comprobación inicial de bandeja verificó ocultación, restauración y salida con la política anterior de multimedia desactivado en cada apertura. El 17 de noviembre se verificó la política nueva en la ventana real: activar los gestos, salir completamente, abrir en segundo plano y restaurar la ventana conserva la casilla y el estado Activo. Se mantuvieron English, batería cada 30 segundos, apertura en bandeja y la entrada existente de inicio con Windows. No se enviaron comandos RGB ni se modificaron perfiles. No se reinició Windows durante esta comprobación.

Referencias: [Shell_NotifyIcon](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shell_notifyiconw), [área de notificación](https://learn.microsoft.com/en-us/windows/win32/shell/notification-area), [Run y RunOnce](https://learn.microsoft.com/en-us/windows/win32/setupapi/run-and-runonce-registry-keys) y [SetWindowPlacement](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowplacement).

## Corrección de aspecto y cierre del menú

La grabación del usuario mostró el menú blanco y su permanencia al interactuar fuera. Se conservó un menú nativo de Windows y se personalizó su pintura: carbón, texto claro, selección cian y separación entre acciones. `src/tray_menu.hpp` mantiene los identificadores, textos, navegación y atajos A/P/S. Las medidas siguen el DPI; con alto contraste o lector de pantalla activo se utiliza el menú de texto estándar del sistema.

El propietario anterior permanecía oculto al pedir el primer plano, lo que no aseguraba el foco necesario para cerrar el menú correctamente. Ahora se hace visible una ventana auxiliar transparente de un píxel únicamente mientras se sigue el menú y se comprueba que recibió el primer plano. La pérdida de activación o cancelación termina el seguimiento; al cerrarse se vuelve a ocultar el auxiliar. No se usan hooks globales de ratón ni funciones de temas no documentadas.

Las pruebas de escritorio verifican textos, identificadores, medidas a 100/150/200 %, atajos y variante estándar accesible. Se inspeccionó el mismo menú de producción mediante una ventana de prueba aislada: fondo oscuro, apertura repetida, cierre al clicar fuera y al pulsar Escape, sin ejecutar una acción al cancelar. La automatización reactivó la ventana de prueba al intentar seleccionar una opción, cancelando el menú: no se registra esa tentativa como una selección correcta ni como prueba de las acciones desde el icono real. No se enviaron comandos RGB ni se modificaron perfiles.

La ventana de prueba opcional se compila con `cmake --build build --config Release --target tray_menu_check` y se abre con `build\Release\tray_menu_check.exe`. Es un recurso de desarrollo separado de la aplicación. Referencias de la corrección: [TrackPopupMenuEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackpopupmenuex), [SetForegroundWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow) y [menús dibujados por la aplicación](https://learn.microsoft.com/en-us/windows/win32/menurc/using-menus).
