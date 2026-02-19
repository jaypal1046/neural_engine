#include <iostream>
#include <cstring>
#include "ans.h"

int main() {
    const char* msg = "hello";
    size_t n = strlen(msg);
    uint32_t freq[256] = {};
    for (size_t i = 0; i < n; ++i) freq[(uint8_t)msg[i]]++;

    AnsTable table;
    ans_build_table(freq, table);
    for (int s = 0; s < 256; ++s)
        if (table.freq[s] > 0)
            std::cout << "'" << (char)s << "' freq=" << table.freq[s]
                      << " cumul=" << table.cumul[s] << "\n";
    std::cout << "cumul[256]=" << table.cumul[256] << "\n\n";

    AnsEncoder enc;
    for (int i = (int)n-1; i >= 0; --i) {
        enc.put((uint8_t)msg[i], table);
        std::cout << "encode '" << msg[i] << "' -> state=" << enc.state << " buf.size=" << enc.buf.size() << "\n";
    }
    auto encoded = enc.finish();
    std::cout << "\nencoded (" << encoded.size() << " bytes): ";
    for (auto b : encoded) std::cout << (int)b << " ";
    std::cout << "\n\n";

    AnsDecoder dec;
    dec.init(encoded.data(), encoded.size());
    std::cout << "init state=" << dec.state << " pos=" << dec.pos << "\n";
    std::string decoded(n, '?');
    for (size_t i = 0; i < n; ++i) {
        uint8_t sym = dec.get(table);
        decoded[i] = (char)sym;
        std::cout << "decoded[" << i << "]=" << (char)sym << " state=" << dec.state << "\n";
    }
    std::cout << "\noriginal: " << msg << "\ndecoded:  " << decoded << "\n";
    return 0;
}
