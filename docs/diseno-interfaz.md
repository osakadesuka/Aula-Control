# Base visual de AULA Control

Esta iteración organiza la interfaz antes de la pasada final de acabados solicitada por el usuario. Conserva las funciones existentes de iluminación, perfiles, batería y perilla.

## Distribución

- Ventana inicial de 1440 × 900, con mínimo de 900 × 760 unidades lógicas.
- Navegación lateral con iconos propios para Inicio, Iluminación, Perilla, Perfiles, Dispositivo, Apariencia y Preferencias, indicador de selección y cabecera contextual.
- Español e inglés con selector en Preferencias. Textos directos, sin lemas ni descripciones promocionales; las ayudas conservan solo instrucciones y advertencias necesarias. El cambio de idioma mantiene selección RGB, efecto, valores de controles y nombres escritos.
- Barra lateral compacta por debajo de 1240 unidades de ancho; conserva nombres accesibles y ayudas al pasar el cursor. El panel de ajustes también reduce su ancho.
- Vista del teclado flexible: crece con el área disponible y mantiene las proporciones de la imagen. Las zonas RGB calibradas no cambian.
- El teclado de Inicio e Iluminación usa la misma vista fija con relieve. La fotografía aporta leyendas, plástico y acabado de la perilla. Una máscara por tecla concentra la luz en el hueco del interruptor y en el bisel inferior. La carcasa incluye el indicador junto a Home, el canto frontal y sus dos difusores. Las máscaras se preparan una vez al cargar la aplicación y se reutilizan al cambiar de efecto, tamaño o página.
- Colores rápidos visibles junto al selector completo. Cambian la vista previa; el envío sigue siendo explícito mediante Aplicar al teclado.
- Botón de aplicar situado al pie del panel de ajustes. Los perfiles y el estado del dispositivo comparten la misma distribución de paneles.

## Componentes y movimiento

`src/ui_components.hpp` reúne los paneles y botones de navegación. Cada panel comparte una geometría de 22 unidades de radio, con una sombra clara desplazada hacia arriba/izquierda y otra oscura hacia abajo/derecha. El borde de la cara es sutil, y la zona de vista previa tiene un fondo rebajado con radio de 18 unidades.

La selección de navegación usa un tono cian apagado y un icono que interpola su color. Las páginas aparecen con una transición de opacidad; los controles conservan las respuestas de foco, hover y pulsación de Raven. La barra de batería continúa animando sus cambios de nivel. Las páginas ocultas no solicitan animaciones del teclado ni exponen sus controles a los lectores de pantalla.

La base conserva una paleta de carbón con acento cian. La pasada de acabado descrita al final de este documento revisa fondos, movimiento, jerarquía, sombras y controles de ambas skins.

## Verificación

La prueba de GUI recorre las siete secciones con ambas skins a 900 × 760, 1120 × 800, 1440 × 900 y 1920 × 1080. Comprueba límites de controles, navegación, encabezados y una vista previa de al menos 600 unidades de ancho a partir de 1440. También conserva las pruebas de restauración de perfiles y estados de batería.

Se revisaron con Raven/Vulkan la pantalla de Iluminación, la navegación a Perilla y Perfiles, los relieves, la nueva jerarquía y la conservación de un perfil real existente. La revisión no guardó ni eliminó perfiles del usuario.

## Inicio y acciones de edición

La aplicación abre en Inicio. Esta pantalla muestra la vista previa simulada del último efecto enviado y su resumen, el estado pendiente de la selección editable, el número de perfiles y la acción multimedia elegida para el clic de la perilla. Sin un envío guardado muestra la fotografía sin iluminación simulada y **Sin efecto guardado**. La tarjeta global conserva la última lectura de batería. Hay accesos a las cuatro secciones de configuración.

Editar la iluminación o cargar un perfil cambia la vista previa de Iluminación y el aviso de cambios pendientes. El teclado y el resumen de Inicio conservan el último envío hasta completar otro correctamente. No lee ni atribuye un perfil activo al firmware. Solo se redibuja el teclado de la página visible; ambas vistas comparten un reloj cuando representan el último envío, por lo que navegar o editar otra selección no reinicia esa animación.

Iluminación incorpora **Recuperar último envío**: carga en vista previa los ajustes guardados del último envío, incluso de sesiones anteriores. El botón queda desactivado sin memoria válida y recuperarlos manualmente no los envía. Al abrir se cargan esos ajustes; **Preferencias → Restaurar último efecto** controla su envío automático después de verificar la conexión. La memoria y los límites de sincronización se describen en [memoria de iluminación](memoria-iluminacion.md).

La biblioteca vacía presenta **Preparar mi primer perfil** en lugar de un selector vacío y una acción de carga inutilizable. La opción de guardar se activa al introducir un nombre válido. Los perfiles existentes y el borrado con opción de deshacer conservan su comportamiento.

El 20 de noviembre se verificó con la ventana real que elegir Respiración sin aplicarla conserva la ola horizontal multicolor en Inicio y marca los cambios pendientes. Salir y reabrir recuperó la ola, brillo 4/5 y velocidad 2/5, y el envío automático se completó desde la bandeja. El usuario confirmó la restauración tras desconectar y reconectar el receptor. Las comprobaciones sin dispositivo incluyen la separación entre selección y último envío, la recuperación de todos sus ajustes y las dos skins e idiomas a cuatro tamaños. No se modificó la biblioteca real.

## Apariencia y Glass

La sexta sección, Apariencia, permite cambiar de skin en caliente. Glass parte de la referencia de video suministrada por el usuario: un ambiente difuminado común, superficies translúcidas y controles flotantes. `src/ui_skin.hpp` define el tema, un fondo procedural de luces suaves y la preferencia local. No se incorporó código de Glassy Music ni se usa su contenido multimedia.

Los paneles usan una sola capa de material `R_MATERIAL_FROSTED_GLASS`; las dos capas de relieve neumórfico se vuelven transparentes para evitar fondos opacos superpuestos. Raven/Vulkan procesa el desenfoque y los reflejos. Se reduce la refracción y el borde luminoso para mantener una apariencia suave. La navegación inactiva queda plana y la selección utiliza vidrio; el foco por teclado conserva el acento cian. Los textos secundarios neutros aumentan su contraste en Glass.

Aurora mezcla cian y violeta sobre una base oscura. La alternativa sigue el RGB seleccionado en la vista previa y vuelve a Aurora cuando se elige multicolor. Se genera una textura de 640 × 400, con un ligero tramado para reducir bandas; se reutiliza si los colores no cambian. No hay un bucle de animación del fondo, captura del escritorio ni recurso externo. Es un fondo interno de la aplicación, sobre el que se dibuja el vidrio.

La skin y el ambiente se guardan por separado de la biblioteca, en `%LOCALAPPDATA%\AulaControl\appearance.txt`, mediante un archivo temporal y reemplazo atómico. Sin una preferencia válida se inicia Glass/Aurora. Un fallo de escritura mantiene el estilo de la sesión e informa que no se pudo guardar. Las comprobaciones de GUI no leen ni escriben las preferencias del usuario y verifican que cambiar la skin conserve la selección de iluminación y el clic multimedia desactivado.

## Capacidades de iluminación

El catálogo de efectos contiene su nombre, identificador y capacidades: multicolor comprobado, paleta propia y presencia de animación. Las 18 opciones actuales se comprobaron en multicolor con el usuario. La nota de compatibilidad y los controles de velocidad, color e intensidad usan este catálogo. Los identificadores que no pertenecen al catálogo no anuncian capacidades. Confeti estático y Transición gradual conservan su explicación de paleta propia; Color fijo y Confeti mantienen la velocidad desactivada.

El visualizador calcula el color y la intensidad de cada una de las 82 posiciones con el reloj del último envío o de la selección editable. Las demostraciones reactivas usan pulsaciones simuladas. La ola horizontal y la vertical desplazan el espectro en el sentido del recorrido. La cascada termina cada fila antes de iluminar la siguiente. El reparto de colores y la duración son aproximaciones visuales basadas en las pruebas del teclado.

**Brillo del teclado** muestra el nivel nativo 1–5 en todas las paletas. **Ajuste RGB** conserva el porcentaje fino para color único, en una fila compacta que se oculta en multicolor y paletas propias. Esto evita un porcentaje ficticio de brillo multicolor y mantiene los controles dentro de la ventana mínima. Inicio y Perfiles incluyen el brillo en sus resúmenes. La simulación combina ambos ajustes y no representa una curva de luz calibrada. Los perfiles antiguos mantienen brillo 3/5. La preferencia multimedia se guarda por separado y se recupera al abrir.

## Preparación y estado de conexión

El primer contenido de la ventana es una tarjeta centrada con identidad AULA, indicador circular animado, estado real de la comprobación y acceso para entrar mientras continúa. Utiliza la skin guardada y termina al completar la primera comprobación o encontrar un problema. No introduce una pausa artificial. Después, Inicio y las seis secciones mantienen su distribución habitual y añaden una línea de comunicación en la cabecera, separada de la batería.

Los mensajes diferencian la detección del receptor de una respuesta del teclado. La ausencia de receptor deja la edición disponible y bloquea Aplicar; la reconexión verifica la comunicación antes de restaurar el último envío si está habilitado. Un teclado que no responde conserva la hipótesis de reposo como posibilidad. El detalle y el próximo intento aparecen en Dispositivo. Los estados y la pantalla inicial se comprueban en las dos skins y los cuatro tamaños habituales.

## Pulido visual y movimiento

La simulación RGB y el indicador de arranque solicitan como máximo 30 actualizaciones por segundo. Los eventos de entrada no añaden fotogramas de la simulación fuera de ese presupuesto. Los controles y las transiciones conservan el ritmo de Raven. Color fijo multicolor y Confeti son estáticos también en la vista previa; no mantienen un bucle de repintado. El indicador y la simulación se suspenden al ocultar o minimizar. Metodología de medición: [pruebas-recursos.md](pruebas-recursos.md).

Los paneles comparten geometría y se distinguen por función: lateral más opaco, espacio principal del teclado con borde cian discreto, paneles normales y tarjetas compactas con menos sombra y marco. Neumorfismo utiliza sombras claras y oscuras de menor alcance; Glass reduce refracción, saturación y brillo del borde. La navegación ocupa 48 unidades de alto con espacios de 9, y conserva etiquetas y tooltips al compactarse. El icono de Perfiles ya no dibuja un rectángulo opaco sobre el vidrio.

Glass mantiene el ambiente elegido con luces más contenidas. Neumorfismo incorpora un degradado neutral de carbón. Ambos usan la misma textura de fondo estática y cacheada: no hay regeneración continua ni nuevos recursos externos. Los controles comparten colores de foco, hover, pulsación, desactivado, listas y tooltips. Salir y Eliminar pasan a acciones discretas con texto coral. La selección pendiente tiene una banda de estado diferenciada, sin atribuir confirmación del firmware al envío.

La cabecera abrevia la identidad y usa un título de 32 unidades. Se distinguen títulos, texto principal y notas secundarias. Se revisaron acentos y nombres de efectos en español. Aplicar muestra «Aplicando…» durante el envío; la información de envío, éxito y error conserva colores semánticos. La paleta y las zonas del teclado no se alteraron.

Cada cambio de sección reproduce un único deslizamiento corto, en la dirección de la navegación lateral, sin rebote ni retraso añadido. Pulsar la sección actual no reinicia la animación. Se retiraron los fundidos independientes de los paneles y el parpadeo del teclado tras un envío; la velocidad general de las transiciones usa el factor 1.25 de Raven. Este factor no cambia la velocidad seleccionada del efecto RGB. Raven lee la preferencia de animaciones de Windows; la vista previa y el indicador de arranque también respetan su desactivación. No se solicitan animaciones propias con la ventana oculta o minimizada.

La compilación Release y las diez pruebas cubren las siete páginas en ambas skins a 900 × 760, 1120 × 800, 1440 × 900 y 1920 × 1080. Se añadió una comprobación de congelación de la simulación al desactivar animaciones. La revisión con la ventana real recorrió las siete secciones en Glass y comparó Apariencia e Iluminación en Neumorfismo; se restauró Glass con el ambiente que sigue el color seleccionado. No se guardaron ni eliminaron perfiles ni se enviaron comandos RGB. Las últimas correcciones de texto de Dispositivo se validaron mediante la prueba de GUI.
