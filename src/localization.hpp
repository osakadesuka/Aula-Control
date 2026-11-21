#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace i18n {
enum class Language { spanish, english };
inline const char* code(Language language) { return language == Language::english ? "en" : "es"; }
struct Translation { std::string_view es, en; };
// Canonical UI text stays Spanish; only application-owned text enters this table.
// User profile names and input values never pass through the translator.
inline constexpr Translation strings[]{
    {"Último guardado", "Last saved"}, {"Último enviado", "Last sent"}, {"Sin efecto guardado", "No saved effect"},
    {"Restaurar último efecto", "Restore last effect"},
    {"Vista previa de la selección. Inicio muestra el último efecto enviado. La fase de animación es aproximada.", "Selection preview. Home shows the last sent effect. Animation phase is approximate."},
    {"Último efecto enviado por AULA. No lee cambios hechos desde el teclado. La fase de animación es aproximada.", "Last effect sent by AULA. Does not read changes made on the keyboard. Animation phase is approximate."},
    {"Recupera los ajustes del último envío guardado.", "Recovers settings from the last saved send."},
    {"No se pudo leer el último efecto guardado.", "Could not read the last saved effect."},
    {"Reaplica automáticamente el último efecto enviado correctamente. No envía cambios pendientes.", "Automatically reapplies the last successfully sent effect on startup or reconnect. Does not send pending edits."},
    {"Restaurando iluminación…", "Restoring lighting…"}, {"Iluminación restaurada.", "Lighting restored."},
    {"No se pudo confirmar el envío de iluminación.", "Could not confirm the lighting send."},
    {"Ajustes enviados. No se pudo guardar el último efecto.", "Settings sent. Could not save the last effect."},
    {"Iluminación", "Lighting"}, {"Perilla", "Knob"}, {"Perfiles", "Profiles"},
    {"Dispositivo", "Device"}, {"Inicio", "Home"}, {"Apariencia", "Appearance"}, {"Preferencias", "Settings"},
    {"Iniciando", "Starting"}, {"Continuar", "Continue"}, {"Vista previa", "Preview"},
    {"Configuración", "Configuration"}, {"Ventana", "Window"}, {"General", "General"},
    {"Idioma", "Language"}, {"Tema", "Theme"}, {"Fondo", "Background"}, {"Vista del tema", "Theme preview"},
    {"Neumorfismo", "Neumorphic"}, {"Color seleccionado", "Selected color"},
    {"Color", "Color"}, {"Coral", "Coral"}, {"Ámbar", "Amber"}, {"Menta", "Mint"},
    {"Cian", "Cyan"}, {"Violeta", "Violet"}, {"Blanco", "White"},
    {"Abrir selector de color", "Open color picker"}, {"COLOR SELECCIONADO", "SELECTED COLOR"},
    {"PALETA DEL EFECTO", "EFFECT PALETTE"}, {"Paleta del teclado", "Keyboard palette"},
    {"EFECTO", "EFFECT"}, {"PALETA", "PALETTE"}, {"Color único", "Single color"}, {"RGB arcoíris", "Rainbow RGB"},
    {"Color fijo", "Static color"}, {"Reactivo al pulsar", "Reactive keys"}, {"Apagado aleatorio", "Random fade"},
    {"Lluvia", "Rain"}, {"Confeti estático", "Static confetti"}, {"Respiración", "Breathing"},
    {"Transición gradual de colores", "Color cycle"}, {"Ola desde el centro", "Center wave"},
    {"Ola vertical", "Vertical wave"}, {"Ola horizontal", "Horizontal wave"}, {"Vórtice", "Vortex"},
    {"Reactivo por fila", "Reactive row"}, {"Barrido horizontal reactivo", "Reactive horizontal sweep"},
    {"Ola reactiva", "Reactive wave"}, {"Cascada por filas", "Row cascade"}, {"Látigo desde el centro", "Center whip"},
    {"Ola diagonal rápida", "Fast diagonal wave"}, {"Filas en direcciones opuestas", "Opposing rows"},
    {"VELOCIDAD   ·   EFECTO ESTÁTICO", "SPEED   ·   STATIC EFFECT"},
    {"Este efecto usa su propia paleta.", "This effect uses its own palette."},
    {"Aplicar al teclado", "Apply to keyboard"}, {"Aplicando…", "Applying…"},
    {"Recuperar último envío", "Restore last sent settings"},
    {"Recupera los ajustes del último envío de esta sesión.", "Restore the settings last sent in this session."},
    {"Envía los ajustes seleccionados.", "Send the selected settings."},
    {"Cambios pendientes", "Unapplied changes"}, {"Último envío", "Last sent settings"},
    {"Selecciona un efecto válido.", "Select a valid effect."}, {"Enviando ajustes…", "Sending settings…"},
    {"Ajustes enviados.", "Settings sent."}, {"Envío completado. Hay cambios pendientes.", "Settings sent. Unapplied changes remain."},
    {"El envío tardó demasiado. Revisa el receptor.", "Sending timed out. Check the receiver."},
    {"No se pudo comunicar. Revisa el receptor.", "Communication failed. Check the receiver."},
    {"Perfil cargado. Pulsa Aplicar.", "Profile loaded. Click Apply."},
    {"Último envío cargado en vista previa.", "Last sent settings loaded in preview."},
    {"Controles multimedia", "Media controls"}, {"MODO MULTIMEDIA", "MEDIA MODE"},
    {"Activar clics multimedia", "Enable media clicks"},
    {"UN CLIC", "ONE PRESS"}, {"DOS CLICS", "TWO PRESSES"}, {"TRES CLICS", "THREE PRESSES"}, {"—", "—"},
    {"Siguiente pista", "Next track"}, {"Pista anterior", "Previous track"},
    {"La opción se recuerda al abrir AULA Control.", "This setting is remembered when AULA Control starts."},
    {"Filtra duplicados durante 100 ms. Uno o dos clics esperan hasta 400 ms; tres responden al soltar el tercero. Las pulsaciones largas se ignoran.", "Filters duplicates for 100 ms. One or two presses wait up to 400 ms; three respond when the third is released. Long presses are ignored."},
    {"Clics multimedia activos", "Media clicks enabled"},
    {"No se pudo enviar la acción multimedia.", "Could not send the media action."},
    {"Siguiente pista enviada.", "Next track sent."}, {"Pista anterior enviada.", "Previous track sent."},
    {"Reproducir / pausar enviado.", "Play / pause sent."},
    {"Mantén pulsada la perilla 3–5 segundos para cambiar entre iluminación y volumen.", "Hold the knob for 3–5 seconds to switch between lighting and volume."},
    {"GIRO IZQUIERDO", "TURN LEFT"}, {"GIRO DERECHO", "TURN RIGHT"}, {"CLIC CORTO", "PRESS"},
    {"Bajar volumen", "Volume down"}, {"Subir volumen", "Volume up"}, {"Silencio", "Mute"},
    {"Reproducir / pausar", "Play / pause"}, {"Usar el clic para reproducir / pausar", "Use press for play / pause"},
    {"Reasigna Silencio de todos los dispositivos y programas de Windows.", "Remaps Mute from all Windows devices and applications."},
    {"Se desactiva al salir.", "Disabled when the application exits."},
    {"Activo", "Enabled"}, {"Desactivado", "Disabled"},
    {"No se pudo activar. Comprueba otras instancias.", "Could not enable. Check other instances."},
    {"Windows no pudo enviar la tecla multimedia.", "Windows could not send the media key."},
    {"PERILLA", "KNOB"}, {"Clic: Silencio", "Press: Mute"}, {"Clic: reproducir / pausar", "Press: Play / pause"},
    {"Guardar perfil", "Save profile"}, {"Selección actual", "Current selection"}, {"NOMBRE DEL PERFIL", "PROFILE NAME"},
    {"Nombre del perfil", "Profile name"}, {"Guardar", "Save"},
    {"El mismo nombre actualiza el perfil. Guardar no aplica cambios.", "The same name updates the profile. Saving does not apply changes."},
    {"Perfiles guardados", "Saved profiles"}, {"Crear perfil", "Create profile"}, {"No hay perfiles", "No profiles"},
    {"Cargar en vista previa", "Load preview"}, {"Cargar no aplica cambios al teclado.", "Loading does not apply changes to the keyboard."},
    {"Eliminar", "Delete"}, {"Deshacer borrado", "Undo deletion"}, {"Efecto no disponible", "Effect unavailable"},
    {"Paleta propia del efecto", "Built-in palette"}, {"por verificar", "unverified"},
    {"No se pudieron leer los perfiles.", "Could not read profiles."},
    {"Perfil guardado.", "Profile saved."}, {"Perfil actualizado.", "Profile updated."},
    {"Escribe un nombre válido de hasta 32 caracteres.", "Enter a valid name up to 32 characters."},
    {"Límite de 24 perfiles. Actualiza o elimina uno.", "24-profile limit reached. Update or delete a profile."},
    {"No se pudo guardar el perfil.", "Could not save the profile."},
    {"No se pudo eliminar el perfil.", "Could not delete the profile."},
    {"Perfil eliminado.", "Profile deleted."}, {"Ese nombre ya existe.", "That name already exists."},
    {"No se pudo restaurar el perfil.", "Could not restore the profile."}, {"Perfil restaurado.", "Profile restored."},
    {"Biblioteca no disponible", "Profile library unavailable"}, {"Sin perfiles", "No profiles"},
    {"ILUMINACIÓN", "LIGHTING"}, {"PERFILES", "PROFILES"}, {"Ver batería", "View battery"},
    {"Estado del teclado", "Keyboard status"}, {"BATERÍA", "BATTERY"},
    {"USB", "USB"}, {"75%  /  USB", "75%  /  USB"}, {"CONEXIÓN  /  USB", "CONNECTION  /  USB"},
    {"Conectado por USB", "Connected via USB"}, {"Teclado no detectado", "Keyboard not detected"},
    {"Conecta el cable en modo USB o el receptor 2.4 GHz.", "Connect the cable in USB mode or the 2.4 GHz receiver."},
    {"Batería no disponible por USB.", "Battery unavailable via USB."},
    {"Batería automática solo por 2.4 GHz.", "Automatic battery readings use 2.4 GHz only."},
    {"Comprueba el cable y el modo USB del teclado.", "Check the cable and the keyboard's USB mode."},
    {"Varios dispositivos compatibles", "Multiple compatible devices"},
    {"Conecta un solo teclado o receptor compatible.", "Connect only one compatible keyboard or receiver."},
    {"Canal de control no accesible", "Control interface unavailable"},
    {"Comprueba que otro programa no esté usando el teclado.", "Check that another application is not using the keyboard."},
    {"Vuelve a conectar el teclado o receptor.", "Reconnect the keyboard or receiver."},
    {"Revisa la conexión y los archivos de la aplicación.", "Check the connection and application files."},
    {"Buscando teclado...", "Searching for keyboard..."}, {"Detección de conexión cada 5 s.", "Connection detection every 5 s."},
    {"El envío tardó demasiado. Revisa la conexión.", "Sending timed out. Check the connection."},
    {"No se pudo comunicar. Revisa la conexión.", "Could not communicate. Check the connection."},
    {"BATERÍA  /  RECEPTOR 2.4 GHz", "BATTERY  /  2.4 GHz RECEIVER"},
    {"BATERÍA  /  2.4 GHz", "BATTERY  /  2.4 GHz"}, {"BATERÍA  /  SIN RECEPTOR", "BATTERY  /  NO RECEIVER"},
    {"BATERÍA  /  SIN RESPUESTA", "BATTERY  /  NO RESPONSE"}, {"BATERÍA  /  LECTURA ANTERIOR", "BATTERY  /  PREVIOUS READING"},
    {"Comprobando receptor...", "Checking receiver..."}, {"Buscando receptor...", "Searching for receiver..."},
    {"Comprobando teclado...", "Checking keyboard..."}, {"Consultando receptor...", "Reading receiver..."},
    {"Esperando primera lectura", "Waiting for first reading"}, {"Esperando consulta", "Waiting for reading"},
    {"Sin lectura válida", "No valid reading"}, {"Receptor no detectado", "Receiver not detected"},
    {"Conecta el receptor 2.4 GHz.", "Connect the 2.4 GHz receiver."},
    {"Lectura anterior · consulta pendiente", "Previous reading · update pending"},
    {"Última comunicación correcta", "Last communication successful"}, {"Teclado sin respuesta", "Keyboard not responding"},
    {"Comprueba que el teclado esté en modo 2.4 GHz.", "Check that the keyboard is in 2.4 GHz mode."},
    {"Varios receptores compatibles", "Multiple compatible receivers"},
    {"Conecta un solo receptor compatible.", "Connect only one compatible receiver."},
    {"Receptor no accesible", "Receiver unavailable"}, {"Comprueba el acceso al receptor.", "Check access to the receiver."},
    {"Canal de control sin verificar", "Control interface unverified"},
    {"Vuelve a conectar el receptor.", "Reconnect the receiver."}, {"No se pudo comprobar el teclado", "Could not check keyboard"},
    {"Revisa el receptor y los archivos de la aplicación.", "Check the receiver and application files."},
    {"Valor informado por el teclado. Precisión sin calibrar.", "Value reported by the keyboard. Accuracy is not calibrated."},
    {"Actualización automática", "Automatic updates"}, {"INTERVALO DE CONSULTA", "UPDATE INTERVAL"},
    {"Cada 30 segundos", "Every 30 seconds"}, {"Cada 60 segundos", "Every 60 seconds"},
    {"Consulta en curso...", "Reading..."}, {"Consulta pendiente.", "Reading queued."},
    {"Detección del receptor cada 5 s.", "Receiver detection every 5 s."},
    {" · actualizando", " · updating"}, {"actualizando", "updating"}, {" (anterior)", " (previous)"},
    {"No se pudo guardar el intervalo.", "Could not save the update interval."},
    {"Cerrar a la bandeja", "Close to tray"}, {"Minimizar a la bandeja", "Minimize to tray"},
    {"Recordar tamaño y posición", "Remember window size and position"},
    {"Ocultar en la bandeja", "Hide to tray"}, {"Salir de AULA Control", "Exit AULA Control"},
    {"Iniciar con Windows", "Start with Windows"}, {"Abrir en segundo plano", "Start in tray"},
    {"El inicio con Windows abre en la bandeja.", "Windows startup opens in the tray."},
    {"Si mueves el programa, reactiva el inicio con Windows.", "If you move the application, re-enable Windows startup."},
    {"No se pudieron leer las preferencias. Los cambios no se guardarán.", "Could not read settings. Changes will not be saved."},
    {"No se pudieron guardar las preferencias.", "Could not save settings."},
    {"La bandeja no está disponible.", "The system tray is unavailable."},
    {"No se pudo cambiar el inicio con Windows.", "Could not change Windows startup."},
    {"No se pudo crear el icono de la bandeja. Cerrar termina la aplicación.", "Could not create the tray icon. Closing exits the application."},
    {"No se pudo comprobar el inicio con Windows.", "Could not check Windows startup."},
    {"La bandeja no está disponible. Se restauró la ventana.", "The system tray is unavailable. The window was restored."},
    {"No se pudo guardar la apariencia.", "Could not save appearance settings."},
    {"No se pudieron cargar los perfiles.", "Could not load profiles."},
    {"Brillo del teclado", "Keyboard brightness"}, {"Ajuste fino RGB del color", "Color RGB adjustment"},
    {"Velocidad del efecto", "Effect speed"}, {"Skin de la aplicación", "Application theme"}, {"Ambiente del fondo", "Background"},
    {"Nivel de 1 a 5. Pulsa Aplicar para enviarlo.", "Level from 1 to 5. Click Apply to send it."},
    {"Atenúa el color único de 0 a 100 %.", "Dims a single color from 0 to 100%."},
    {"Simulación local. Aplicar envía los ajustes al teclado.", "Local simulation. Apply sends settings to the keyboard."},
    {"En multicolor se usa Aurora.", "Multicolor uses Aurora."},
};
inline constexpr Translation patterns[]{
    {"BRILLO DEL TECLADO   {0} / 5", "KEYBOARD BRIGHTNESS   {0} / 5"},
    {"VELOCIDAD   {0} / 5", "SPEED   {0} / 5"}, {"AJUSTE RGB   {0}%", "RGB ADJUSTMENT   {0}%"},
    {"Brillo {0}/5", "Brightness {0}/5"}, {"Velocidad {0}/5", "Speed {0}/5"}, {"Ajuste {0}%", "Adjustment {0}%"},
    {"{0} perfil guardado", "{0} saved profile"}, {"{0} perfiles guardados", "{0} saved profiles"},
    {"{0} / 24 perfiles", "{0} / 24 profiles"},
    {"Lectura hace {0} s", "Updated {0} s ago"}, {"Lectura hace {0} min", "Updated {0} min ago"},
    {"Última lectura: hace {0} s.", "Last reading: {0} s ago."}, {"Última lectura: hace {0} min.", "Last reading: {0} min ago."},
    {"Próxima consulta en {0} s.", "Next reading in {0} s."},
    {"Reproducir / pausar enviado · {0}", "Play / pause sent · {0}"},
};
inline bool match_number_pattern(std::string_view input, std::string_view pattern, std::array<std::string, 4>& args) {
    std::size_t at = 0, pos = 0;
    while (pos < pattern.size()) {
        if (pattern[pos] == '{' && pos + 2 < pattern.size() && pattern[pos + 2] == '}') {
            const auto index = pattern[pos + 1] - '0'; if (index < 0 || index >= 4) return false;
            const auto begin = at;
            while (at < input.size() && input[at] >= '0' && input[at] <= '9') ++at;
            if (at == begin) return false;
            args[index] = std::string(input.substr(begin, at - begin)); pos += 3;
        } else {
            if (at == input.size() || input[at++] != pattern[pos++]) return false;
        }
    }
    return at == input.size();
}
inline std::string substitute(std::string_view pattern, const std::array<std::string, 4>& args) {
    std::string result;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '{' && i + 2 < pattern.size() && pattern[i + 2] == '}' && pattern[i + 1] >= '0' && pattern[i + 1] <= '3') {
            result += args[pattern[i + 1] - '0']; i += 2;
        } else result += pattern[i];
    }
    return result;
}
inline std::string translate(std::string_view source, Language language) {
    if (language == Language::spanish) return std::string(source);
    for (const auto& entry : strings) if (source == entry.es) return std::string(entry.en);
    for (const auto& entry : patterns) {
        std::array<std::string, 4> args;
        if (match_number_pattern(source, entry.es, args)) return substitute(entry.en, args);
    }
    constexpr std::string_view separator = " · ";
    const auto split = source.find(separator);
    if (split != std::string_view::npos)
        return translate(source.substr(0, split), language) + std::string(separator) + translate(source.substr(split + separator.size()), language);
    return std::string(source); // Numbers, app/model names, units and color codes are invariant.
}
inline bool covered(std::string_view source) {
    for (const auto& entry : strings) if (source == entry.es) return true;
    for (const auto& entry : patterns) {
        std::array<std::string, 4> args;
        if (match_number_pattern(source, entry.es, args)) return true;
    }
    constexpr std::string_view separator = " · ";
    const auto split = source.find(separator);
    if (split != std::string_view::npos) return covered(source.substr(0, split)) && covered(source.substr(split + separator.size()));
    for (const auto invariant : {"", "A", "AULA Control", "AULA F75", "2.4 GHz", "75%  /  2.4 GHz", "Glass", "Aurora", "Español", "English"})
        if (source == invariant) return true;
    if (source.size() == 7 && source[0] == '#') {
        for (const char c : source.substr(1)) if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return false;
        return true;
    }
    if (source.substr(0, 4) == "RGB ") source.remove_prefix(4);
    for (const char c : source) if ((c < '0' || c > '9') && c != '%' && c != '-' && c != ',' && c != ' ') return false;
    return true;
}
} // namespace i18n
