# Instalador AULA Control 1.0.0

## Uso

`dist/AULA-Control-1.0.0-Setup-x64.exe` es el paquete para Windows x64. El asistente está disponible en español e inglés y adopta la apariencia clara u oscura de Windows. No requiere permisos de administrador.

Instala en `%LOCALAPPDATA%\Programs\AULA Control` y crea **Inicio → AULA Control**. El acceso directo de escritorio es opcional y comienza desactivado. El ejecutable instalado es `aula_control.exe`. La versión 1.0.0 aparece en sus propiedades y en la lista de aplicaciones instaladas de Windows.

Requisitos: Windows 10 2004 o posterior, x64, y un controlador gráfico compatible con Vulkan. No instala controladores de gráficos ni un controlador de kernel. La compatibilidad de iluminación corresponde a la unidad y los canales comprobados por el proyecto: USB y receptor 2.4 GHz; RGB y batería por Bluetooth no están implementados.

La aplicación debe estar cerrada para instalar, actualizar o desinstalar. El instalador comprueba el mismo mutex de instancia que AULA y explica cómo elegir **Salir de AULA Control** desde la bandeja o Preferencias. No fuerza la terminación ni reinicia Windows.

## Archivos y datos

El paquete incluye el programa, `rgb_prueba.exe`, `bateria_f75.exe`, `hidapi.dll`, la fotografía del teclado, los avisos y licencias de terceros y las DLL redistribuibles de Visual C++ x64 tomadas de Visual Studio Build Tools. Las DLL se instalan junto al ejecutable para permitir una instalación por usuario; se actualizan con el siguiente paquete de AULA. No copia bibliotecas a System32.

El manifiesto `release-manifest.json` registra versión, arquitectura, versión del runtime, tamaños y SHA256 de los archivos distribuidos. El archivo `.sha256` junto al Setup permite comprobar su integridad. El instalador de AULA aún no tiene firma digital.

La distribución 1.0.0 utiliza ese Setup sin firma. `AULA-Control-1.0.0-release-notes.txt` contiene el aviso de SmartScreen en español e inglés, los límites de compatibilidad y el SHA256 de la compilación concreta. Se genera junto al instalador a partir de `packaging/release-notes.txt.in`, para adjuntarlo a la publicación. El aviso explica las posibles advertencias y restricciones de Windows; no promete su ausencia ni exige cambiar la configuración de seguridad del equipo.

Los datos permanecen en `%LOCALAPPDATA%\AulaControl`, fuera de la carpeta de instalación. Actualizar y desinstalar no borran perfiles, apariencia, preferencias ni memoria de iluminación. Los archivos añadidos por el usuario a la carpeta instalada tampoco se eliminan como parte de la lista de archivos del programa.

Si ya existe la entrada **AULA Control** en `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` con el formato conocido de esta aplicación, el instalador la dirige al ejecutable instalado y conserva `--background`. Esto incluye las builds de desarrollo. Sin una entrada previa no activa el inicio con Windows. El desinstalador solo elimina el valor si sigue apuntando a su propio ejecutable; conserva una ruta registrada después por otra copia.

## Construcción

La versión se define en `VERSION` y CMake la incorpora al recurso VERSIONINFO de la aplicación. El script de Inno Setup mantiene un AppId estable para que posteriores versiones actualicen la misma instalación.

```powershell
.\packaging\build-setup.ps1
```

El script compila en `build/release-1.0/`, ejecuta las trece pruebas CTest y prepara un payload con una lista explícita de archivos. Comprueba la versión, la GUI empaquetada con backend NULL y los dos auxiliares en modo de prueba, sin enviar HID. No incluye herramientas de diagnóstico, tests ni copias de respaldo.

Si no se indica `-IsccPath`, descarga Inno Setup 7.1.1 desde la publicación oficial de GitHub, verifica el SHA256 publicado y su firma de Pyrsys B.V., y lo extrae en modo portátil en `build/tools/innosetup-7.1.1`. No registra ese compilador como aplicación instalada. También admite `-BuildDirectory`, `-RuntimeDirectory` y `-SkipBuild`; este último exige que la versión del ejecutable ya coincida y vuelve a verificar el payload.

Referencias: [descarga oficial de Inno Setup](https://jrsoftware.org/inno-setup/downloads), [modo portátil](https://jrsoftware.org/inno-setup/documentation/topic_technotes.htm), [AppMutex](https://jrsoftware.org/inno-setup/documentation/topic_setup_appmutex.htm) y [distribución del runtime de C++](https://learn.microsoft.com/cpp/windows/choosing-a-deployment-method).

## Verificación del 20 de noviembre de 2026

- Release y las trece pruebas CTest pasaron. Se corrigió la advertencia de conversión numérica del visualizador; permanecen advertencias de compilación en fuentes de Raven sin cambios.
- La ejecución del Setup con AULA abierta se bloqueó antes de crear el destino y mostró la instrucción de salir desde la bandeja.
- `packaging/test-setup.ps1` probó instalación en inglés, actualización en español, reinstalación y dos desinstalaciones, con destino temporal dentro de `build/installer-smoke/`.
- Se comprobaron todos los hashes del manifiesto, versión de Windows/registro, registro de desinstalación por usuario y acceso de Inicio con la ruta y directorio de trabajo correctos.
- La GUI instalada pasó su prueba con backend NULL y stores aislados. No se alteró la iluminación ni la configuración multimedia durante esa prueba.
- Se comprobó que una primera instalación mantiene desactivado el arranque con Windows, que una actualización migra una entrada existente y que desinstalar elimina solo su propia ruta, conservando otra posterior.
- Perfiles, preferencias, apariencia y último efecto conservaron exactamente sus hashes durante la instalación, actualización y desinstalación. Se preservó además un archivo no instalado por el paquete. Al terminar la prueba temporal se restauró la entrada original de inicio con Windows.

La prueba de ciclo completo modifica temporalmente el registro de instalación y la entrada de inicio de AULA. Requiere salir de AULA y no tener otra copia instalada con el mismo AppId. Guarda una copia de los datos, conserva la entrada original en un bloque `finally` y utiliza una carpeta temporal propia; no debe ejecutarse sobre una instalación de uso diario.

```powershell
.\packaging\test-setup.ps1
```

La comprobación se realizó en este equipo. No sustituye una prueba en otro Windows sin herramientas de desarrollo ni un reinicio real de Windows.

También se revisó el asistente oscuro en español y se instaló la versión 1.0.0 en su carpeta predeterminada. La aplicación instalada abrió con Vulkan, reconoció USB y recuperó la ola horizontal multicolor, brillo y velocidad 3/5, el perfil existente y los clics multimedia activados. La ruta de inicio con Windows pasó a la copia instalada y su casilla apareció activada. Se comprobó que las DLL de MSVC cargadas por la GUI proceden de la carpeta instalada y que los archivos coinciden con el manifiesto.
