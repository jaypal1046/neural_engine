#include <iostream>
#include <vector>
#include <cstring>
#include "ans.h"

int main() {
    // Two symbols, equal frequency
    const char* msg = "ab";
    size_t n = 2;
    uint32_t freq[256] = {};
    for (size_t i = 0; i < n; ++i) freq[(uint8_t)msg[i]]++;

    AnsTable table;
    ans_build_table(freq, table);
    std::cout << "freq['a']=" << table.freq['a'] << " freq['b']=" << table.freq['b'] << "\n";
    std::cout << "cumul['a']=" << table.cumul['a'] << " cumul['b']=" << table.cumul['b'] << "\n";
    // Check decode table at slot 0 and ANS_M/2
    std::cout << "sym[0]=" << (char)table.sym[0] << " sym[" << ANS_M/2 << "]=" << (char)table.sym[ANS_M/2] << "\n";

    // Encode "ab" backwards: encode 'b' then 'a'
    AnsEncoder enc;
    for (int i = (int)n-1; i >= 0; --i) {
        std::cout << "encode '" << msg[i] << "' state=" << enc.state << "\n";
        enc.put((uint8_t)msg[i], table);
        std::cout << "  -> state=" << enc.state << " buf.size=" << enc.buf.size() << "\n";
    }
    auto encoded = enc.finish();
    std::cout << "encoded: ";
    for (auto b : encoded) std::cout << (int)b << " ";
    std::cout << "\n";

    // Decode
    AnsDecoder dec;
    dec.init(encoded.data(), encoded.size());
    std::cout << "init state=" << dec.state << "\n";
    for (size_t i = 0; i < n; ++i) {
        uint8_t sym = dec.get(table);
        std::cout << "got '" << (char)sym << "' state=" << dec.state << "\n";
    }
    return 0;
}
