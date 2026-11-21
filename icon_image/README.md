# Icono de AULA Control

- `download.png`: imagen original elegida por el usuario, conservada.
- `app-icon-v2.png`: adaptación generada con la herramienta integrada de imágenes, a partir del original. Símbolo de monitor turquesa, ola y burbuja, con transparencia y formas grandes para tamaños pequeños.
- `app-icon-v2.prompt.txt`: prompt exacto usado en la edición.
- `icon-preview-v2.png`: comprobación de las entradas del ICO a su tamaño real sobre fondos claro y oscuro.

El icono activo está en `resources/aula_control.ico`, incrustado en el ejecutable por el recurso 101. Raven utiliza ese recurso para la ventana y la barra de tareas.

Para regenerarlo después de actualizar el PNG:

```powershell
.\tools\build_icon.ps1
cmake --build build --config Release --target aula_control
```

El script genera nueve entradas RGBA de 16, 20, 24, 32, 40, 48, 64, 128 y 256 píxeles; conserva la transparencia y las proporciones.
