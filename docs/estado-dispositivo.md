# Batería, actualización y comunicación

La pestaña Dispositivo y la tarjeta superior comparten la misma lectura de batería. Muestran porcentaje y antigüedad sin interpretar la ausencia de respuesta como batería vacía. La barra usa la interpolación de Raven para animar cambios de nivel. Los niveles de 0–20% se resaltan en rojo; una lectura anterior se marca en ámbar.

La primera consulta se programa al abrir. El intervalo de sesión puede cambiar entre 30 y 60 segundos; al abrir de nuevo comienza en 60. La siguiente consulta se calcula desde el final del intento anterior. Las consultas de batería y los envíos RGB siguen serializados; si toca consultar durante un envío, se espera a que termine y la interfaz lo indica. Los tiempos usan un reloj monotónico.

Una respuesta válida sustituye el porcentaje y su instante de recepción. Si falla la siguiente consulta, se conservan ambos y se muestra «Lectura anterior · sin respuesta reciente». Si nunca hubo una lectura válida se muestra `--`. El 0% es una lectura válida, diferente de un dato ausente. Si una consulta se retrasa más de cinco segundos respecto de su programación, la lectura también se marca como anterior.

La detección del receptor es independiente de la respuesta de batería. Se enumera su interfaz candidata cada cinco segundos, sin enviar informes al dispositivo. Receptor detectado no equivale a teclado despierto: una consulta sin respuesta puede deberse a reposo, apagado, alcance u otro error. La precisión física del porcentaje permanece sin calibrar.

## Inicio y recuperación

La pantalla inicial utiliza el tema elegido y un indicador animado mientras se busca el receptor y se comprueba el teclado. Muestra que los perfiles y preferencias están preparados y permite entrar a la aplicación en cualquier momento. Termina automáticamente ante la primera comprobación completa, incluso si el resultado es ausencia o fallo. No usa porcentajes ficticios ni espera artificial para prolongar la carga. El indicador deja de solicitar redibujos al abandonar esa pantalla o minimizar la ventana.

La cabecera y Dispositivo distinguen receptor no detectado, comprobación pendiente, última comunicación correcta, teclado sin respuesta, receptor no accesible, varios candidatos e interfaz no verificable. La ausencia de respuesta no se presenta como reposo confirmado. Una apertura fallida no prueba ocupación: el mensaje también contempla falta de acceso.

Al desaparecer el receptor se invalida la actualidad de la lectura sin borrar porcentaje o fecha. Se desactiva Aplicar, manteniendo disponibles la edición y la biblioteca. Cuando vuelve a aparecer un único receptor se solicita inmediatamente una lectura; el intervalo de batería normal sigue siendo 30/60 segundos. Si solo despierta el teclado y el receptor nunca desapareció, su recuperación se observará en la próxima consulta programada. No se envía RGB al reconectar, ni se restaura un perfil automáticamente.

`src/device_state.hpp` conserva la presencia observada, el resultado de comunicación y una generación que cambia al variar la presencia. Un resultado de una generación anterior no puede reemplazar una lectura de la conexión nueva. La comprobación inicial también termina si falta el auxiliar o expira su tiempo de espera.

## Salida estructurada del auxiliar

`bateria_f75.exe --detectar` únicamente enumera las interfaces que coinciden con VID/PID, interfaz y usos conocidos. Devuelve una línea `AULA_DEVICE 1 present`, `absent`, `ambiguous` o `error`. No abre el canal de control para enviar comandos ni modifica el teclado.

`bateria_f75.exe --estado` conserva la consulta de batería, el descriptor exacto y la validación de checksum. Devuelve `AULA_DEVICE 1 ready <0-100>` o un resultado `absent`, `ambiguous`, `busy`, `incompatible`, `write_error`, `read_error`, `no_response` o `error`. Un resultado estructurado válido termina con código 0 aunque describa un problema del dispositivo; un error de uso termina con 2. La GUI exige además salida completa y válida del ejecutor. La modalidad anterior `--consultar` mantiene sus mensajes y su código distinto de cero si no obtiene batería.

El parser rechaza versiones desconocidas, registros duplicados, valores fuera de rango y datos adicionales. La presencia solo se acepta de un resultado de enumeración; la batería solo se sustituye ante `ready` con porcentaje. Los mensajes de diagnóstico humanos no se interpretan como estados.

## Ejecución de herramientas

`src/tool_runner.hpp` ejecuta las herramientas existentes en segundo plano. Lee únicamente los bytes disponibles de la salida, limita la captura a 64 KiB y comprueba el proceso cada 25 ms cuando no hay datos. Una captura excedida no se considera un resultado correcto. Cada operación tiene ocho segundos de espera y una señal de cancelación al cerrar la ventana. En esos casos se termina exclusivamente el proceso auxiliar creado por esa operación y se liberan sus identificadores.

No se añadieron comandos nuevos de dispositivo ni se cambió el paquete de consulta. La lectura conserva las comprobaciones de descriptor y checksum de `bateria_f75`. La GUI usa ahora el resultado estructurado de `--estado`; el parser anterior de porcentaje queda cubierto para compatibilidad. Una salida mal formada no sustituye la lectura anterior. Detección, consulta y RGB utilizan una única operación activa y el mismo límite de espera/cancelación.

Referencias de API: [PeekNamedPipe](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-peeknamedpipe) y [TerminateProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess). El extremo de lectura del canal pertenece a un único hilo; no se comparte entre lectores.

## Verificación

- Compilación Release y ocho pruebas CTest: estado del dispositivo, protocolo RGB, perilla, perfiles, batería, ejecución de auxiliares, protocolo de batería y GUI.
- Estado: límites 0/100, formato inválido o ambiguo, conservación del valor y de su antigüedad, cambio de intervalo, recuperación tras fallo y ausencia de consultas simultáneas.
- Auxiliares controlados sin HID: captura de stdout/stderr, salida excesiva, error del proceso, ejecutable ausente, interrupción por espera excedida y cancelación. Los auxiliares que simulan bloqueo se terminan durante la prueba.
- GUI sin dispositivo: cuatro pestañas a 900 × 760, representación de una lectura anterior de 95% y recuperación a 0%.
- Ventana real Raven/Vulkan: recepción de 95%, cambio de 60 a 30 segundos y una nueva lectura automática con reinicio de antigüedad y cuenta atrás. La conservación ante fallo se comprobó mediante pruebas simuladas, sin desconectar el teclado del usuario.
- Nuevas simulaciones: arranque sin receptor y con error del auxiliar, reconexión, interfaz no verificable, falta de acceso, ausencia de respuesta, lectura válida de 0 y descarte de un resultado antiguo. Se comprueba conservación de batería y refresco inmediato sin operaciones superpuestas.
- La GUI sin hardware dibuja la pantalla inicial y los estados de Dispositivo en ambas skins a cuatro tamaños. La comprobación real de los nuevos modos del auxiliar detectó el receptor y devolvió `no_response` para esa consulta; no se interpreta la causa ni se pidió al usuario desconectar su teclado.
