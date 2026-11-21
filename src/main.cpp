#include <hidapi.h>
#include <windows.h>
#include <conio.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace {
struct Device {
    std::string path;
    unsigned short vid, pid, usage_page, usage;
    int interface_number;
    std::wstring product, manufacturer, serial;
};

std::wstring copy(const wchar_t* s) { return s ? s : L""; }
std::string utf8(const std::wstring& s) {
    if (s.empty()) return "(no disponible)";
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "(conversion fallida)";
    std::string result(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), n, nullptr, nullptr);
    return result;
}
void hex16(unsigned value) {
    std::cout << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << value
              << std::dec << std::setfill(' ');
}
void dump(const unsigned char* bytes, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        if (i % 16 == 0) std::cout << "\n  " << std::hex << std::setw(4) << std::setfill('0') << i << ": ";
        std::cout << std::setw(2) << static_cast<unsigned>(bytes[i]) << ' ';
    }
    std::cout << std::dec << std::setfill(' ') << '\n';
}
std::vector<Device> enumerate() {
    std::vector<Device> devices;
    hid_device_info* head = hid_enumerate(0, 0);
    for (auto* p = head; p; p = p->next) {
        if (!p->path) continue;
        devices.push_back({p->path, p->vendor_id, p->product_id, p->usage_page, p->usage,
                           p->interface_number, copy(p->product_string), copy(p->manufacturer_string), copy(p->serial_number)});
    }
    hid_free_enumeration(head);
    return devices;
}
void list(const std::vector<Device>& devices) {
    if (devices.empty()) std::cout << "No se encontraron interfaces HID.\n";
    for (std::size_t i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];
        std::cout << "\n[" << i << "] " << (d.usage_page >= 0xFF00 ? "[CANDIDATA VENDOR-DEFINED] " : "")
                  << utf8(d.product) << "\n  VID: ";
        hex16(d.vid); std::cout << "  PID: "; hex16(d.pid);
        std::cout << "  Interfaz: " << d.interface_number << "  Usage Page: "; hex16(d.usage_page);
        std::cout << "  Usage: "; hex16(d.usage);
        std::cout << "\n  Fabricante: " << utf8(d.manufacturer) << "\n  Serie: " << utf8(d.serial)
                  << "\n  Ruta: " << d.path << '\n';
    }
}
void inspect(const Device& d, bool offerListen = true) {
    hid_device* handle = hid_open_path(d.path.c_str());
    if (!handle) {
        std::cout << "No se pudo abrir la interfaz: " << utf8(copy(hid_error(nullptr))) << "\n";
        return;
    }
    std::cout << "Interfaz abierta. No se enviaran comandos.\n";
    std::array<unsigned char, HID_API_MAX_REPORT_DESCRIPTOR_SIZE> descriptor{};
    const int n = hid_get_report_descriptor(handle, descriptor.data(), descriptor.size());
    if (n < 0) std::cout << "No se pudo leer el descriptor de informes: " << utf8(copy(hid_error(handle))) << '\n';
    else { std::cout << "Descriptor HID (" << n << " bytes):"; dump(descriptor.data(), static_cast<std::size_t>(n)); }
    if (!offerListen) { hid_close(handle); return; }
    std::cout << "Escuchar hasta 10 informes durante 10 segundos? (s/N): ";
    std::string answer;
    if (std::getline(std::cin, answer) && (answer == "s" || answer == "S")) {
        std::cout << "Escuchando; pulsa Q para detener.\n";
        std::array<unsigned char, 4096> buffer{};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        unsigned count = 0;
        while (count < 10 && std::chrono::steady_clock::now() < deadline) {
            if (_kbhit()) {
                const int key = _getch();
                if (key == 'q' || key == 'Q') break;
            }
            const int received = hid_read_timeout(handle, buffer.data(), buffer.size(), 50);
            if (received < 0) {
                std::cout << "Error de lectura: " << utf8(copy(hid_error(handle))) << '\n';
                break;
            }
            if (received > 0) {
                ++count;
                std::cout << "Informe de entrada (" << received << " bytes):";
                dump(buffer.data(), static_cast<std::size_t>(received));
            }
        }
        std::cout << "Escucha finalizada: " << count << " informe(s).\n";
    }
    hid_close(handle);
}
}
int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    if (hid_init() != 0) { std::cerr << "No se pudo inicializar hidapi.\n"; return 1; }
    std::cout << "Escaner HID AULA F75: las candidatas vendor-defined no identifican por si solas el teclado.\n";
    auto devices = enumerate();
    if (argc == 2 && std::string(argv[1]) == "--listar") { list(devices); hid_exit(); return 0; }
    if (argc == 7 && std::string(argv[1]) == "--inspeccionar") {
        try {
            std::array<unsigned long, 5> ids{};
            for (std::size_t i = 0; i < ids.size(); ++i) {
                const std::string argument(argv[i + 2]); std::size_t used = 0;
                ids[i] = std::stoul(argument, &used, 0);
                if (used != argument.size() || ids[i] > 65535) throw std::out_of_range("identifier");
            }
            std::vector<Device> matches;
            for (const auto& d : devices)
                if (d.vid == ids[0] && d.pid == ids[1] && d.interface_number == static_cast<int>(ids[2])
                    && d.usage_page == ids[3] && d.usage == ids[4]) matches.push_back(d);
            list(matches);
            if (matches.size() != 1) { std::cerr << "Se requiere una sola coincidencia.\n"; hid_exit(); return 1; }
            inspect(matches.front(), false); hid_exit(); return 0;
        } catch (const std::exception&) { std::cerr << "Identificadores invalidos.\n"; hid_exit(); return 2; }
    }
    if (argc != 1) {
        std::cerr << "Uso: hid_scanner.exe [--listar|--inspeccionar VID PID interfaz pagina uso]\n";
        hid_exit(); return 2;
    }
    list(devices);
    for (;;) {
        std::cout << "\nNumero para abrir; R para volver a enumerar; Q para salir: ";
        std::string input;
        if (!std::getline(std::cin, input) || input == "q" || input == "Q") break;
        if (input == "r" || input == "R") { devices = enumerate(); list(devices); continue; }
        try {
            std::size_t pos = 0;
            const auto index = std::stoull(input, &pos);
            if (pos != input.size() || index >= devices.size()) throw std::out_of_range("index");
            inspect(devices[static_cast<std::size_t>(index)]);
        } catch (const std::exception&) { std::cout << "Seleccion invalida.\n"; }
    }
    hid_exit();
}
