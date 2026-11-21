#include <hidapi.h>
#include <windows.h>
#include <array>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <charconv>
#include "rgb_protocol.hpp"
#include "control_hid.hpp"
#include "wired_rgb_protocol.hpp"
#include "last_lighting.hpp"

void dump(const unsigned char* p, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) std::cout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << unsigned(p[i]) << ' ';
    std::cout << std::dec << std::setfill(' ') << '\n';
}
bool send(hid_device* h, const RgbPacket& p) {
    std::array<unsigned char, 33> buf{};
    std::copy(p.begin(), p.end(), buf.begin() + 1);
    int n = hid_write(h, buf.data(), buf.size());
    if (n != static_cast<int>(buf.size())) { std::cerr << "Escritura fallida o incompleta: " << n << '\n'; return false; }
    return true;
}
bool send_wired(hid_device* handle, const WiredRgbPackets& packets) {
    const auto stage = [&](const WiredRgbReport& report, bool expectAck) {
        if (hid_send_feature_report(handle, report.data(), report.size()) != static_cast<int>(report.size())) {
            std::cerr << "Escritura USB incompleta.\n"; return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (expectAck) {
            WiredRgbReport reply{};
            const auto received = hid_get_feature_report(handle, reply.data(), reply.size());
            std::cout << "Respuesta USB (" << received << "): ";
            if (received > 0) dump(reply.data(), static_cast<std::size_t>(received));
            if (received != static_cast<int>(reply.size()) || !wired_rgb_ack(reply, report[2])) {
                std::cerr << "Respuesta USB distinta de la esperada; se detuvo la secuencia.\n"; return false;
            }
        }
        return true;
    };
    if (!stage(packets.begin, true)) return false;
    if (!stage(packets.select, true) || !stage(packets.effect, false) || !stage(packets.save, true)) {
        // End the session without guessing another endpoint or protocol.
        stage(packets.finish, false); return false;
    }
    return stage(packets.finish, false);
}
int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 4) {
        std::cout << "Uso: rgb_prueba.exe R G B [--brillo 1-5] [--modo 1-20] [--velocidad 1-5] [--multicolor] [--transporte usb|2g] [--aplicar]\n";
        return argc == 1 ? 0 : 2;
    }
    std::array<unsigned char, 3> rgb{};
    for (int i = 0; i < 3; ++i) {
        int value = 0;
        const char* first = argv[i + 1];
        const char* last = first + std::strlen(first);
        auto parsed = std::from_chars(first, last, value);
        if (first == last || parsed.ec != std::errc{} || parsed.ptr != last || value < 0 || value > 255) {
            std::cerr << "Cada color debe ser un numero entero entre 0 y 255.\n"; return 2;
        }
        rgb[i] = static_cast<unsigned char>(value);
    }
    int brightness = 3, mode = 1, speed = 3; bool apply = false, multicolor = false, seenBrightness = false, seenMode = false, seenSpeed = false;
    std::optional<ControlTransport> requestedTransport;
    for (int i = 4; i < argc; ++i) {
        if (std::strcmp(argv[i], "--transporte") == 0 && !requestedTransport && i + 1 < argc) {
            requestedTransport = parse_transport(argv[++i]);
            if (!requestedTransport) { std::cerr << "Transporte invalido.\n"; return 2; }
            continue;
        }
        if (std::strcmp(argv[i], "--aplicar") == 0 && !apply) { apply = true; continue; }
        if (std::strcmp(argv[i], "--multicolor") == 0 && !multicolor) { multicolor = true; continue; }
        const bool isBrightness = std::strcmp(argv[i], "--brillo") == 0;
        const bool isMode = std::strcmp(argv[i], "--modo") == 0;
        const bool isSpeed = std::strcmp(argv[i], "--velocidad") == 0;
        if ((!isBrightness && !isMode && !isSpeed) || (isBrightness && seenBrightness) || (isMode && seenMode) || (isSpeed && seenSpeed) || ++i >= argc) {
            std::cerr << "Opcion invalida o repetida.\n"; return 2;
        }
        int value = 0; const char* first = argv[i]; const char* last = first + std::strlen(first);
        auto parsed = std::from_chars(first, last, value);
        if (first == last || parsed.ec != std::errc{} || parsed.ptr != last || value < 1 || value > (isMode ? 20 : 5)) {
            std::cerr << "Brillo y velocidad permitidos: 1-5; modo experimental: 1-20.\n"; return 2;
        }
        if (isBrightness) { brightness = value; seenBrightness = true; }
        else if (isMode) { mode = value; seenMode = true; }
        else { speed = value; seenSpeed = true; }
    }
    const auto packets = rgb_packets({rgb, mode, brightness, speed, multicolor});
    if (!packets) return 2;
    const auto& commit = packets->commit; const auto& effect = packets->effect;
    const auto wired = wired_rgb_packets({rgb, mode, brightness, speed, multicolor});
    const auto showPackets = [&](ControlTransport transport) {
        if (transport == ControlTransport::usb) {
            std::cout << "Informes USB, incluido Report ID 0 (65 bytes):\n";
            for (const auto* report : {&wired->begin, &wired->select, &wired->effect, &wired->save, &wired->finish}) dump(report->data(), report->size());
        } else {
            std::cout << "Payloads 2.4 GHz (32 bytes; Report ID 0 adicional al enviar):\n";
            dump(commit.data(), commit.size()); dump(effect.data(), effect.size());
        }
    };
    std::cout << "RGB (" << unsigned(rgb[0]) << ", " << unsigned(rgb[1]) << ", " << unsigned(rgb[2]) << "), modo " << mode << ", brillo " << brightness << "/5, velocidad " << speed << "/5, paleta " << (multicolor ? "RGB multicolor" : "un solo color") << ".\n";
    if (!apply) {
        showPackets(requestedTransport.value_or(ControlTransport::receiver));
        std::cout << "Vista previa: no se accedio al teclado. Agrega --aplicar para enviar.\n"; return 0;
    }
    if (hid_init() != 0) { std::cerr << "No se pudo iniciar HID.\n"; return 1; }
    const auto selected = enumerate_control_endpoint();
    if (!selected.endpoint || (requestedTransport && *requestedTransport != selected.transport)) {
        std::cerr << "Canal ausente, ambiguo o cambiado. No se envio nada.\n"; hid_exit(); return 1;
    }
    auto* h = hid_open_path(selected.endpoint->path.c_str());
    if (!h) { std::cerr << "No se pudo abrir la interfaz.\n"; hid_exit(); return 1; }
    if (!verify_control_endpoint(h, selected.transport)) {
        std::cerr << "Descriptor distinto o no disponible; no se envio nada.\n"; hid_close(h); hid_exit(); return 1;
    }
    std::cout << "Transporte: " << transport_name(selected.transport) << "; enviando a " << selected.endpoint->path << '\n';
    showPackets(selected.transport);
    if (selected.transport == ControlTransport::usb) {
        const bool sent = wired && send_wired(h, *wired);
        const auto sentAt = lighting_wall_time_ms();
        hid_close(h); hid_exit();
        if (sent) std::cout << "Secuencia USB completada. Comprueba visualmente la iluminacion.\nAULA_LIGHTING 1 usb " << sentAt << '\n';
        return sent ? 0 : 1;
    }
    if (!send(h, commit)) { hid_close(h); hid_exit(); return 1; }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (!send(h, effect)) { hid_close(h); hid_exit(); return 1; }
    const auto sentAt = lighting_wall_time_ms();
    std::cout << "Paquetes enviados; comprueba visualmente el teclado. La escritura no confirma que se haya aplicado.\n";
    std::array<unsigned char, 128> input{};
    const int n = hid_read_timeout(h, input.data(), input.size(), 500);
    if (n > 0) { std::cout << "Informe recibido (" << n << " bytes): "; dump(input.data(), static_cast<std::size_t>(n)); }
    else if (n == 0) std::cout << "Sin informe de respuesta en 500 ms.\n";
    else std::cout << "Error al leer respuesta.\n";
    hid_close(h); hid_exit();
    std::cout << "AULA_LIGHTING 1 2g " << sentAt << '\n';
}
