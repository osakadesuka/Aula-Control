#include <array>
#include <iomanip>
#include <iostream>

using Packet = std::array<unsigned char, 32>;
void checksum(Packet& p) {
    unsigned sum = 0;
    for (unsigned i = 0; i < 31; ++i) sum += p[i];
    p[31] = static_cast<unsigned char>(sum);
}
void show(const char* label, const Packet& p) {
    std::cout << label << "\n";
    for (auto byte : p) std::cout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << unsigned(byte) << ' ';
    std::cout << std::dec << '\n';
}
int main() {
    Packet commit{}; commit[0] = 0x0f; checksum(commit);
    Packet effect{};
    effect[0] = 0x05; effect[1] = 0x10; effect[3] = 1;
    effect[4] = 0xff; // rojo hipotetico: canal observado distinto a la referencia
    effect[12] = 3; effect[13] = 3;
    effect[17] = 0xaa; effect[18] = 0x55; checksum(effect);
    std::cout << "SIMULACION: ejemplo RGB rojo fijo; no se abre ni modifica ningun dispositivo.\n";
    show("1. Commit:", commit);
    show("2. Efecto:", effect);
}
