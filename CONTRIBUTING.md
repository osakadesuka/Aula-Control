# Contributing / Cómo contribuir

**English**

Reports and suggestions are welcome in English or Spanish. Use the issue form and include the app version, exact keyboard variant, connection, Windows version, steps and expected/actual behavior. Remove serial numbers, full HID paths and personal information from attachments.

For code changes, describe the behavior and run relevant checks from [the build guide](docs/building.md). Keep USB and 2.4 GHz protocols distinct, preserve the device checks, and separate hardware evidence from automated tests. The source license is pending; discuss code contributions with the maintainer before submitting them.

Update both English and Spanish in `src/localization.hpp` when changing visible app text, and both READMEs when behavior or compatibility changes. Release announcements have a bilingual template in `packaging/release-notes.txt.in`.

**Español**

Puedes informar de errores y proponer funciones en español o inglés. Incluye versión de la aplicación, variante exacta del teclado, conexión, Windows, pasos y resultado esperado/observado. Elimina números de serie, rutas HID completas y datos personales de los adjuntos.

Para cambiar código, explica el comportamiento y ejecuta las comprobaciones apropiadas de [la guía de compilación](docs/building.md) o [las notas de desarrollo](docs/desarrollo.md). Mantén separados los protocolos USB y 2.4 GHz, conserva la identificación del dispositivo y diferencia evidencia física y pruebas automáticas. La licencia principal sigue pendiente; habla con el mantenedor antes de proponer código.

Actualiza inglés y español en `src/localization.hpp` al cambiar textos de la aplicación, y ambos READMEs al cambiar el uso o la compatibilidad. Las publicaciones tienen una plantilla bilingüe en `packaging/release-notes.txt.in`.
