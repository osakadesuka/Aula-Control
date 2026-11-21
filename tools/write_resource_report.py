"""Write the measured-session report after summarizing baseline and optimized runs."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
runs = root / 'build/resource-check'
load = lambda path: json.loads(path.read_text(encoding='utf-8-sig'))
baseline = {r['phase']: r for r in load(runs / 'baseline/summary.json')}
optimized = {r['phase']: r for r in load(runs / 'optimized/summary.json')}
meta = load(runs / 'optimized/metadata.json')
ui = load(runs / 'ui-checks.json')
gpu_context = load(runs / 'optimized/gpu-context.json')
global_gpu = gpu_context['nvidia'].split(',')[3].strip()
helper_rows = load(runs / 'helpers/samples.json')
phases = [('glass_static', 'Glass, color fijo'), ('glass_animated', 'Glass, ola RGB'),
          ('neumorphic_animated', 'Neumorfismo, ola RGB'), ('glass_minimized', 'Minimizada'), ('glass_tray', 'Bandeja')]
lines = ['# Resultados de estabilidad y recursos · 16 de noviembre de 2026', '',
         f"Equipo: {meta['cpu'][0]['Name'].strip()}, {meta['logicalProcessors']} procesadores lógicos, 32 GB de RAM y {meta['gpu'][0]['Name']}. Windows 11 Pro, build 26300. Compilación Release, Raven/Vulkan y ventana normal de 1440 × 900 de contenido.", '',
         f'Se mantuvieron otros programas abiertos, incluido un juego y un navegador. La lectura de NVIDIA guardada en gpu-context.json registró un {global_gpu} % de GPU global: esa cifra no pertenece a AULA. Los valores siguientes se atribuyen al PID de la interfaz. La carga y las frecuencias del resto del equipo impiden interpretar el antes/después como un benchmark aislado.', '',
         '## Consumo de la versión final', '',
         '| Escenario | Muestras | Tramo analizado (s) | CPU media / P95 (%) | RAM residente media (MiB) | GPU media / P95 (%) |',
         '|---|---:|---:|---:|---:|---:|']
for key, name in phases:
    r = optimized[key]
    lines.append(f"| {name} | {r['samples']} | {r['span_s']:.1f} | {r['cpu_pct']['mean']:.3f} / {r['cpu_pct']['p95']:.3f} | {r['working_set_mib']['mean']:.2f} | {r['gpu_pct']['mean']:.3f} / {r['gpu_pct']['p95']:.3f} |")
tray = optimized['glass_tray']
lines += ['', f"La bandeja mantiene aproximadamente {tray['private_mib']['mean']:.2f} MiB de memoria privada comprometida, {tray['gpu_dedicated_mib']['mean']:.2f} MiB de memoria dedicada de GPU y {tray['gpu_shared_mib']['mean']:.2f} MiB de memoria compartida de GPU. Son métricas distintas que no deben sumarse a la RAM residente. Ocultar suspende el renderizado; no destruye las texturas y recursos que se reutilizan al restaurar.", '',
          '## Cambio realizado', '',
          'Se limitan a 30 por segundo las solicitudes de actualización de la simulación RGB y del indicador de arranque. Los controles y las transiciones mantienen su temporización de Raven. Los modos fijos multicolor y Confeti dejan de solicitar repintados continuos; el modo fijo multicolor también mantiene sus colores estáticos en la simulación. La minimización y la bandeja suspenden las animaciones propias.', '',
          '| Ola horizontal multicolor, velocidad 5 | CPU media (%) | GPU media (%) |', '|---|---:|---:|']
for name, r in [('Inicial', baseline['glass_animated']), ('Con límite de actualizaciones', optimized['glass_animated'])]:
    lines.append(f"| {name} | {r['cpu_pct']['mean']:.3f} | {r['gpu_pct']['mean']:.3f} |")
lines += ['', 'Estos son los valores observados en esta sesión. El límite se verifica con un reloj simulado y ráfagas de eventos; no se midieron FPS de presentación. La fase inicial llamada `glass_minimized` no confirmó minimización y se excluye de las comparaciones. La segunda ejecución registra el estado nativo de ventana y descarta muestras incompatibles con cada escenario.', '',
          '## Estabilidad', '',
          f"- {ui['automated_suite_after_change']['tests']} pruebas repetidas {ui['automated_suite_after_change']['repetitions']} veces: 250 ejecuciones sin fallos después del cambio. La versión inicial también pasó 225 ejecuciones.",
          f"- {ui['navigation_visits']} visitas de navegación, con {ui['complete_seven_page_cycles']} ciclos completos por las siete secciones; además, una maximización/restauración y comprobaciones de minimización y bandeja."]
first, last = optimized['navigation_batch1'], optimized['navigation_batch2']
lines += [f"- Durante las tandas de navegación, la mediana de memoria privada pasó de {first['private_mib']['first']:.3f} a {last['private_mib']['last']:.3f} MiB; los handles, de {first['handles']['first']:.0f} a {last['handles']['last']:.0f}. Los objetos GDI/USER se mantuvieron en {last['gdi_objects']['last']:.0f}/{last['user_objects']['last']:.0f}. No se observó crecimiento sostenido durante las repeticiones.",
          '- Ninguna muestra del proceso se registró como no respondiente. No se encontraron eventos de fallo o bloqueo de AULA ni de sus auxiliares en el registro Application durante el periodo consultado. La lectura del registro devolvió únicamente que no había eventos coincidentes.',
          '- Las consultas de batería continuaron programadas; no se enviaron comandos RGB ni se guardaron o eliminaron perfiles desde las acciones de prueba. El hash de la biblioteca de perfiles se conservó. Se restauraron Glass, el fondo que sigue el color y español.', '',
          'Después de más de dos minutos en la bandeja, se restauró la ventana: la batería tenía una lectura de hacía 5 s y la vista previa volvió a animarse. Se comprobó una salida completa mediante Salir, sin terminar el proceso a la fuerza. El programa queda cerrado al finalizar estas pruebas.', '',
          '## Auxiliares de batería', '',
          'Se midieron aparte, sin añadir su consumo a los porcentajes del proceso gráfico. Los tiempos de CPU tienen la resolución del contador de Windows y no son tiempos de respuesta del teclado.', '',
          '| Operación | Ejecuciones | CPU media por ejecución (ms) | Tiempo real medio (ms) | Fallos |', '|---|---:|---:|---:|---:|']
for argument in ('--detectar', '--estado'):
    rows = [r for r in helper_rows if r['argument'] == argument]
    lines.append(f"| `{argument}` | {len(rows)} | {sum(r['cpu_ms'] for r in rows)/len(rows):.2f} | {sum(r['wall_ms'] for r in rows)/len(rows):.2f} | {sum(r['exitCode'] != 0 for r in rows)} |")
lines += ['', 'A la frecuencia configurada de 5 s para detección y 60 s para batería, esos tiempos equivalen aproximadamente a 0,06 puntos porcentuales adicionales de CPU total en este equipo. Es una estimación por frecuencia, no una medición continua del grupo de procesos. No se midió el pico de RAM de los auxiliares de vida breve.', '',
          '## Alcance y datos', '',
          'La sesión comprueba funcionamiento y consumo durante minutos. No certifica ausencia de fugas a largo plazo ni sustituye una prueba de varias horas, suspensión del equipo, cambio de GPU o reinicio del controlador. El consumo de potencia eléctrica de AULA no se midió.', '',
          'Metodología, definiciones de Windows y comandos para repetir: [pruebas-recursos.md](pruebas-recursos.md). Datos originales en `build/resource-check/baseline`, `build/resource-check/optimized`, `build/resource-check/helpers`, `ui-checks.json` y los registros de CTest. Los CSV conservan todas las muestras y los resúmenes excluyen cinco segundos de cada extremo de las fases.', '',
          f"SHA-256 del ejecutable final: `{meta['sha256']}`.",
          f"SHA-256 de la biblioteca conservada: `{ui['profile_sha256']}`.", '']
destination = root / 'docs/resultados-recursos-2026-11-16.md'
destination.write_text('\n'.join(lines), encoding='utf-8')
print(destination)
