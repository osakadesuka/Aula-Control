# Perfiles locales de iluminación

Cada perfil contiene un nombre, los tres canales RGB sin escalar (0–255), un efecto del selector, un ajuste fino RGB de 0–100, la elección entre color único y arcoíris, velocidad 1–5 y brillo nativo 1–5 (ambos inicialmente 3). Se guardan como máximo 24 perfiles. Los nombres distinguen mayúsculas, admiten Unicode y se recortan los espacios de sus extremos. Un nombre idéntico actualiza la entrada existente.

El almacenamiento utiliza `%LOCALAPPDATA%\AulaControl\lighting-profiles.txt`, separado del ejecutable. El formato de texto versionado actual comienza con `AULA_LIGHTING_PROFILES 3`; los nombres se escriben entre comillas, escapando comillas y barras invertidas. Cada registro se valida por completo antes de aceptarlo. Los modos 3 y 20 siguen fuera del selector y se rechazan en los perfiles.

Se escribe un archivo temporal en la misma carpeta y se reemplaza el destino después de completar y cerrar la escritura. Un fallo conserva la biblioteca anterior en memoria. Una lectura fallida bloquea las modificaciones para evitar sobrescribir datos no comprendidos. Los perfiles son de este equipo; no hay sincronización entre instancias abiertas simultáneamente.

Guardar, seleccionar y cargar un perfil solo afectan al archivo o a la vista previa. Aplicarlo sigue siendo una acción explícita en Iluminación. Por separado, la aplicación recuerda la activación del remapeo multimedia y el último envío de iluminación. **Restaurar último efecto** permite reenviar este último al abrir o reconectar; no selecciona un perfil de la biblioteca ni envía una edición pendiente. Véase [memoria de iluminación](memoria-iluminacion.md).

La interfaz compara la selección con los ajustes del último envío guardado, incluso de sesiones anteriores. Si el usuario edita mientras se está enviando otra selección, esos cambios continúan pendientes al terminar. Inicio muestra el último envío y no la selección pendiente. Un envío completado no prueba que el firmware haya aplicado cada parámetro; se conserva la comprobación visual del teclado como referencia.

La biblioteca admite las versiones 1, 2 y 3. La versión 1 recibe velocidad 3; las versiones 1 y 2 reciben brillo nativo 3, que era el valor fijo anterior. La versión 2 añadía velocidad; la versión 3 añade brillo nativo al final de cada registro. Solo se escribe al guardar, actualizar o eliminar explícitamente un perfil. La lectura no modifica el archivo. Se conserva la protección de formatos futuros o ilegibles y se comprueba que ambas migraciones mantengan los perfiles no editados.

Cambiar la velocidad o el brillo nativo también marca la selección como pendiente de envío. Al cargar un perfil se restauran todos los ajustes. En efectos estáticos se conserva el nivel de velocidad en el perfil, pero se envía el valor conocido 3 porque no hay animación que acelerar. El brillo nativo se envía siempre. La atenuación RGB se combina con él solo en color único; en multicolor y modos con paleta propia se conserva el valor RGB para recuperarlo al volver a color único.

## Validación

- Compilación Release sin advertencias nuevas y CTest: conversión multimedia, perfiles y distribución de las tres pestañas.
- Pruebas de perfiles: lectura CRLF, Unicode y caracteres escapados, persistencia, actualización sin duplicados, límite de 24, eliminación/restauración, rechazo de datos incompletos o fuera de rango y conservación ante errores de lectura/escritura.
- Prueba de GUI sin dispositivo: restauración de un perfil multicolor y uno de color único, incluida la intensidad; dimensiones de controles a 900 × 760. No accede a la biblioteca real ni activa la perilla.
- Revisión con Raven/Vulkan y una carpeta de datos temporal: guardar un perfil a 100%, reducir la selección a 48% y cargar el perfil recuperó 100% en vista previa, sin pulsar Aplicar al teclado.
