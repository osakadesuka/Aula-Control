#include <hidapi.h>
#include <windows.h>
#include <array>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "device_state.hpp"
#include "control_hid.hpp"

using Packet = std::array<unsigned char, 32>;
Packet query() { Packet p{}; p[0] = 0x20; p[1] = 0x01; p[31] = 0x21; return p; }
void dump(const unsigned char* p, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) std::cout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << unsigned(p[i]) << (i + 1 == n ? '\n' : ' ');
    std::cout << std::dec << std::setfill(' ');
}
bool valid(const unsigned char* p, std::size_t n) {
    if (n != 32 || p[0] != 0x20 || p[1] != 0x01 || p[2] != 0 || p[3] > 100) return false;
    unsigned sum = 0;
    for (std::size_t i = 0; i < 31; ++i) sum += p[i];
    return static_cast<unsigned char>(sum) == p[31];
}
int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    const auto packet = query();
    if (argc == 2 && std::strcmp(argv[1], "--self-test") == 0) {
        Packet p{}; p[0] = 0x20; p[1] = 1; p[3] = 100; p[31] = 0x85;
        if (!valid(p.data(), p.size())) return 1;
        p[31] ^= 1;
        if (valid(p.data(), p.size())) return 1;
        if (valid(p.data(), 31)) return 1;
        std::cout << "Pruebas de respuesta: OK\n"; return 0;
    }
    std::optional<ControlTransport> requestedTransport;
    if (argc == 4 && std::strcmp(argv[2], "--transporte") == 0) {
        requestedTransport = parse_transport(argv[3]);
        if (!requestedTransport) return 2;
    }
    const bool action = argc == 2 || (argc == 4 && requestedTransport.has_value());
    const bool detect = action && std::strcmp(argv[1], "--detectar") == 0;
    const bool structured = detect || (action && std::strcmp(argv[1], "--estado") == 0);
    const bool legacy = action && std::strcmp(argv[1], "--consultar") == 0;
    if (!structured) {
        std::cout << "Consulta experimental de bateria: payload de 32 bytes:\n";
        dump(packet.data(), packet.size());
    }
    if (argc == 1) { std::cout << "Simulacion: no se abrio ningun dispositivo. Para enviar: bateria_f75.exe --consultar\n"; return 0; }
    if (!structured && !legacy) { std::cerr << "Uso: bateria_f75.exe [--consultar|--estado|--detectar] [--transporte usb|2g]\n"; return 2; }
    ControlTransport transport = ControlTransport::receiver;
    const auto report = [&](DeviceCode code, std::optional<int> percent = {}) {
        if (structured) {
            std::cout << "AULA_DEVICE 2 " << transport_name(transport) << ' ' << device_code_name(code);
            if (code == DeviceCode::ready && percent) std::cout << ' ' << *percent;
            std::cout << '\n'; return 0;
        }
        return code == DeviceCode::ready ? 0 : 1;
    };
    if (hid_init() != 0) { if (!structured) std::cerr << "Error al inicializar hidapi.\n"; return report(DeviceCode::error); }
    const auto selected = enumerate_control_endpoint();
    transport = selected.transport;
    if (!selected.endpoint) {
        if (!structured) std::cerr << "Canal ausente o ambiguo. No se envio nada.\n";
        hid_exit(); return report(selected.ambiguous ? DeviceCode::ambiguous : DeviceCode::absent);
    }
    if (requestedTransport && *requestedTransport != transport) {
        hid_exit(); return report(DeviceCode::incompatible);
    }
    // Read-only enumeration; the control channel is not opened for commands.
    if (detect) { hid_exit(); return report(DeviceCode::present); }
    auto* h = hid_open_path(selected.endpoint->path.c_str());
    if (!h) { if (!structured) std::cerr << "No se pudo abrir la interfaz.\n"; hid_exit(); return report(DeviceCode::busy); }
    if (!verify_control_endpoint(h, transport)) {
        if (!structured) std::cerr << "Descriptor distinto o no disponible; no se envio nada.\n";
        hid_close(h); hid_exit(); return report(DeviceCode::incompatible);
    }
    if (transport == ControlTransport::usb) {
        if (!structured) std::cout << "Canal USB verificado. Bateria no disponible por esta conexion.\n";
        hid_close(h); hid_exit(); return report(DeviceCode::ready);
    }
    std::array<unsigned char, 33> output{};
    std::copy(packet.begin(), packet.end(), output.begin() + 1);
    if (!structured) std::cout << "Enviando una consulta por " << selected.endpoint->path << "\n";
    const int written = hid_write(h, output.data(), output.size());
    if (written != static_cast<int>(output.size())) {
        if (!structured) std::cerr << "Error o escritura incompleta (" << written << ").\n";
        hid_close(h); hid_exit(); return report(DeviceCode::write_error);
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    std::array<unsigned char, 128> input{};
    std::optional<int> percent;
    DeviceCode outcome = DeviceCode::no_response;
    unsigned reports = 0;
    while (std::chrono::steady_clock::now() < deadline && reports < 20) {
        const int n = hid_read_timeout(h, input.data(), input.size(), 100);
        if (n < 0) { if (!structured) std::cerr << "Error al leer la respuesta.\n"; outcome = DeviceCode::read_error; break; }
        if (!n) continue;
        ++reports;
        if (!structured) { std::cout << "Respuesta (" << n << " bytes): "; dump(input.data(), static_cast<std::size_t>(n)); }
        if (valid(input.data(), static_cast<std::size_t>(n))) {
            if (!structured) std::cout << "Porcentaje informado (experimental): " << unsigned(input[3]) << "%\n";
            percent = input[3]; outcome = DeviceCode::ready; break;
        }
    }
    if (!percent && !structured) std::cout << "No se recibio una respuesta valida; no se puede determinar la bateria.\n";
    hid_close(h); hid_exit(); return report(outcome, percent);
}
