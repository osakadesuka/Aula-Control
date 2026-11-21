# Pruebas de estabilidad y recursos

Las mediciones utilizan el ejecutable Release y los contadores de Windows del proceso de AULA Control. El muestreador guarda datos en CSV, información del equipo y hash del ejecutable en JSON. No envía información fuera de la PC.

## Repetir una medición

Abre AULA Control y ejecuta desde la raíz del proyecto:

```powershell
$aulaProcess = Get-Process aula_control
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\measure_resources.ps1 -TargetProcessId $aulaProcess.Id -OutputDirectory .\build\resource-check\mi-prueba -Seconds 600
```

Usa una carpeta nueva para cada ejecución. En otra terminal, marca cada escenario después de comprobarlo en la ventana:

```powershell
'glass_animated' | Set-Content .\build\resource-check\mi-prueba\phase.txt -Encoding ASCII
```

Escenarios: reposo visible, ola horizontal multicolor a velocidad 5, otra página sin vista previa, Neumorfismo, minimización y bandeja. Mantén el mismo tamaño de ventana. No hace falta pulsar Aplicar: la simulación permite probar el renderizado sin cambiar la iluminación del teclado. Deja al menos un minuto por escenario y separa los cambios con una fase `setup`.

Para terminar y resumir:

```powershell
'done' | Set-Content .\build\resource-check\mi-prueba\stop -Encoding ASCII
python .\tools\summarize_resources.py .\build\resource-check\mi-prueba
ctest --test-dir build -C Release --output-on-failure --repeat until-fail:25
```

`samples.csv` conserva las muestras; `summary.json` contiene medias, percentil 95, máximos y medianas del inicio/final de cada fase. Se excluyen cinco segundos de cada extremo de la fase y las muestras cuyo estado de ventana contradice el escenario. El intervalo real consta en cada fila. El muestreador comprueba respuesta del proceso, handles, hilos, objetos GDI/USER y estado de la ventana; las lecturas son externas al programa medido.

## Interpretación

- CPU: diferencia del tiempo de CPU del proceso, dividida por el tiempo transcurrido y el número de procesadores lógicos. En un equipo de 12 hilos, ocupar un hilo completo representa aproximadamente 8,33 % del total. [Tiempo de CPU de un proceso](https://learn.microsoft.com/en-us/dotnet/api/system.diagnostics.process.totalprocessortime).
- RAM residente: working set, que incluye páginas privadas y compartidas presentes en RAM. La memoria privada comprometida se registra aparte; no deben sumarse como si fueran dos consumos independientes. [Working set de Windows](https://learn.microsoft.com/en-us/windows/win32/procthread/process-working-set).
- GPU: motor más ocupado atribuido al PID de AULA, sin sumar porcentajes de motores distintos. Se registran por separado memoria dedicada y compartida de GPU. Es una medición de actividad, no de potencia eléctrica. [Contadores de GPU de Windows](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/).

Los porcentajes de la tabla corresponden al proceso gráfico. Los auxiliares breves de batería se miden aparte con `tools/measure_helpers.ps1`; el compositor de escritorio y otros programas no se incluyen. No se detienen otras aplicaciones. La frecuencia y carga de la GPU, el controlador, el tamaño de ventana y el resto del equipo influyen en los resultados.

Una subida inicial puede corresponder a cachés del controlador, tipografía, accesibilidad o páginas visitadas por primera vez. La comprobación de estabilidad compara fases repetidas después del calentamiento. Estas pruebas detectan problemas en la sesión medida; no equivalen a una prueba de varios días ni certifican ausencia de fugas.
