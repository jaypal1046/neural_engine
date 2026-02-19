#include <iostream>
#include <vector>
#include <cstring>
#include "ans.h"

int main() {
    // Minimal test: single symbol repeated
    const char* msg = "aaa";
    size_t n = 3;
    uint32_t freq[256] = {};
    for (size_t i = 0; i < n; ++i) freq[(uint8_t)msg[i]]++;

    AnsTable table;
    bool ok = ans_build_table(freq, table);
    std::cout << "build ok=" << ok << "\n";
    std::cout << "freq['a']=" << table.freq['a'] << " (expect " << ANS_M << ")\n";
    std::cout << "cumul['a']=" << table.cumul['a'] << ", cumul[256]=" << table.cumul[256] << "\n";

    // Encode backwards: a, a, a -> encode index 2, 1, 0
    AnsEncoder enc;
    for (int i = (int)n-1; i >= 0; --i) {
        std::cout << "encoding '" << msg[i] << "' state before=" << enc.state << "\n";
        enc.put((uint8_t)msg[i], table);
        std::cout << "  state after=" << enc.state << " buf.size=" << enc.buf.size() << "\n";
    }
    auto encoded = enc.finish();
    std::cout << "encoded size=" << encoded.size() << ": ";
    for (auto b : encoded) std::cout << (int)b << " ";
    std::cout << "\n";

    AnsDecoder dec;
    dec.init(encoded.data(), encoded.size());
    std::cout << "decoder init state=" << dec.state << "\n";
    for (size_t i = 0; i < n; ++i) {
        uint8_t sym = dec.get(table);
        std::cout << "decoded[" << i << "]=" << (char)sym << " state=" << dec.state << "\n";
    }
    return 0;
}
