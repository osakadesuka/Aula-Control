# Resultados de estabilidad y recursos · 16 de noviembre de 2026

Equipo: AMD Ryzen 5 5600G with Radeon Graphics, 12 procesadores lógicos, 32 GB de RAM y NVIDIA GeForce RTX 5060. Windows 11 Pro, build 26300. Compilación Release, Raven/Vulkan y ventana normal de 1440 × 900 de contenido.

Se mantuvieron otros programas abiertos, incluido un juego y un navegador. La lectura de NVIDIA guardada en gpu-context.json registró un 88 % de GPU global: esa cifra no pertenece a AULA. Los valores siguientes se atribuyen al PID de la interfaz. La carga y las frecuencias del resto del equipo impiden interpretar el antes/después como un benchmark aislado.

## Consumo de la versión final

| Escenario | Muestras | Tramo analizado (s) | CPU media / P95 (%) | RAM residente media (MiB) | GPU media / P95 (%) |
|---|---:|---:|---:|---:|---:|
| Glass, color fijo | 64 | 65.9 | 0.165 / 0.733 | 136.51 | 0.133 / 0.207 |
| Glass, ola RGB | 78 | 80.2 | 0.453 / 1.000 | 139.54 | 1.860 / 2.170 |
| Neumorfismo, ola RGB | 62 | 63.5 | 0.355 / 0.750 | 139.79 | 1.237 / 1.294 |
| Minimizada | 44 | 44.8 | 0.079 / 0.497 | 139.33 | 0.000 / 0.000 |
| Bandeja | 123 | 128.0 | 0.027 / 0.125 | 139.70 | 0.000 / 0.000 |

La bandeja mantiene aproximadamente 174.31 MiB de memoria privada comprometida, 74.52 MiB de memoria dedicada de GPU y 49.97 MiB de memoria compartida de GPU. Son métricas distintas que no deben sumarse a la RAM residente. Ocultar suspende el renderizado; no destruye las texturas y recursos que se reutilizan al restaurar.

## Cambio realizado

Se limitan a 30 por segundo las solicitudes de actualización de la simulación RGB y del indicador de arranque. Los controles y las transiciones mantienen su temporización de Raven. Los modos fijos multicolor y Confeti dejan de solicitar repintados continuos; el modo fijo multicolor también mantiene sus colores estáticos en la simulación. La minimización y la bandeja suspenden las animaciones propias.

| Ola horizontal multicolor, velocidad 5 | CPU media (%) | GPU media (%) |
|---|---:|---:|
| Inicial | 1.933 | 10.850 |
| Con límite de actualizaciones | 0.453 | 1.860 |

Estos son los valores observados en esta sesión. El límite se verifica con un reloj simulado y ráfagas de eventos; no se midieron FPS de presentación. La fase inicial llamada `glass_minimized` no confirmó minimización y se excluye de las comparaciones. La segunda ejecución registra el estado nativo de ventana y descarta muestras incompatibles con cada escenario.

## Estabilidad

- 10 pruebas repetidas 25 veces: 250 ejecuciones sin fallos después del cambio. La versión inicial también pasó 225 ejecuciones.
- 90 visitas de navegación, con 12 ciclos completos por las siete secciones; además, una maximización/restauración y comprobaciones de minimización y bandeja.
- Durante las tandas de navegación, la mediana de memoria privada pasó de 174.871 a 174.863 MiB; los handles, de 680 a 676. Los objetos GDI/USER se mantuvieron en 21/34. No se observó crecimiento sostenido durante las repeticiones.
- Ninguna muestra del proceso se registró como no respondiente. No se encontraron eventos de fallo o bloqueo de AULA ni de sus auxiliares en el registro Application durante el periodo consultado. La lectura del registro devolvió únicamente que no había eventos coincidentes.
- Las consultas de batería continuaron programadas; no se enviaron comandos RGB ni se guardaron o eliminaron perfiles desde las acciones de prueba. El hash de la biblioteca de perfiles se conservó. Se restauraron Glass, el fondo que sigue el color y español.

Después de más de dos minutos en la bandeja, se restauró la ventana: la batería tenía una lectura de hacía 5 s y la vista previa volvió a animarse. Se comprobó una salida completa mediante Salir, sin terminar el proceso a la fuerza. El programa queda cerrado al finalizar estas pruebas.

## Auxiliares de batería

Se midieron aparte, sin añadir su consumo a los porcentajes del proceso gráfico. Los tiempos de CPU tienen la resolución del contador de Windows y no son tiempos de respuesta del teclado.

| Operación | Ejecuciones | CPU media por ejecución (ms) | Tiempo real medio (ms) | Fallos |
|---|---:|---:|---:|---:|
| `--detectar` | 20 | 32.03 | 101.22 | 0 |
| `--estado` | 3 | 36.46 | 124.11 | 0 |

A la frecuencia configurada de 5 s para detección y 60 s para batería, esos tiempos equivalen aproximadamente a 0,06 puntos porcentuales adicionales de CPU total en este equipo. Es una estimación por frecuencia, no una medición continua del grupo de procesos. No se midió el pico de RAM de los auxiliares de vida breve.

## Alcance y datos

La sesión comprueba funcionamiento y consumo durante minutos. No certifica ausencia de fugas a largo plazo ni sustituye una prueba de varias horas, suspensión del equipo, cambio de GPU o reinicio del controlador. El consumo de potencia eléctrica de AULA no se midió.

Metodología, definiciones de Windows y comandos para repetir: [pruebas-recursos.md](pruebas-recursos.md). Datos originales en `build/resource-check/baseline`, `build/resource-check/optimized`, `build/resource-check/helpers`, `ui-checks.json` y los registros de CTest. Los CSV conservan todas las muestras y los resúmenes excluyen cinco segundos de cada extremo de las fases.

SHA-256 del ejecutable final: `59339762CDB8DBC06B15D3C91668BE395C898FCE10E6B223D9E82B66D9F90216`.
SHA-256 de la biblioteca conservada: `D5C811B5D3CBCC2C6D1EDBE9E6FA3456E12FCFFC6F82EA171D4A1DD040109149`.
