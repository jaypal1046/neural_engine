#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>
#include "ans.h"

int main() {
    // Test 1: simple round-trip with uniform distribution
    {
        const char* msg = "hello world hello world hello world";
        size_t n = strlen(msg);

        uint32_t freq[256] = {};
        for (size_t i = 0; i < n; ++i) freq[(uint8_t)msg[i]]++;

        AnsTable table;
        bool ok = ans_build_table(freq, table);
        assert(ok && "build_table failed");

        // Check cumul[256] == ANS_M
        assert(table.cumul[256] == ANS_M && "cumul[256] != ANS_M");

        // Encode (backwards)
        AnsEncoder enc;
        for (int i = (int)n - 1; i >= 0; --i)
            enc.put((uint8_t)msg[i], table);
        auto encoded = enc.finish();

        // Decode
        AnsDecoder dec;
        dec.init(encoded.data(), encoded.size());
        std::string decoded(n, '\0');
        for (size_t i = 0; i < n; ++i)
            decoded[i] = (char)dec.get(table);

        assert(decoded == std::string(msg) && "round-trip mismatch!");
        std::cout << "Test 1 PASS: '" << msg << "'\n";
        std::cout << "  Original: " << n << " bytes  Encoded: "
                  << encoded.size() << " bytes\n";
    }

    // Test 2: serialize/deserialize frequency table
    {
        const char* msg = "aaabbc";
        size_t n = strlen(msg);
        uint32_t freq[256] = {};
        for (size_t i = 0; i < n; ++i) freq[(uint8_t)msg[i]]++;

        AnsTable t1;
        ans_build_table(freq, t1);

        std::vector<uint8_t> buf;
        ans_write_freqs(t1, buf);
        assert(buf.size() == 512 && "freq size != 512");

        AnsTable t2 = ans_read_freqs(buf.data());
        for (int s = 0; s < 256; ++s)
            assert(t1.freq[s] == t2.freq[s] && "freq mismatch after serialize");

        // Round-trip with deserialized table
        AnsEncoder enc;
        for (int i = (int)n - 1; i >= 0; --i)
            enc.put((uint8_t)msg[i], t1);
        auto encoded = enc.finish();

        AnsDecoder dec;
        dec.init(encoded.data(), encoded.size());
        std::string decoded(n, '\0');
        for (size_t i = 0; i < n; ++i)
            decoded[i] = (char)dec.get(t2);

        assert(decoded == std::string(msg) && "Test 2 round-trip mismatch");
        std::cout << "Test 2 PASS: freq serialize/deserialize\n";
    }

    // Test 3: encode a block of 1000 bytes with skewed distribution
    {
        std::vector<uint8_t> data(1000);
        uint32_t freq[256] = {};
        // 70% 'A', 20% 'B', 10% 'C'
        for (int i = 0; i < 700; ++i) { data[i] = 'A'; freq['A']++; }
        for (int i = 700; i < 900; ++i) { data[i] = 'B'; freq['B']++; }
        for (int i = 900; i < 1000; ++i) { data[i] = 'C'; freq['C']++; }

        AnsTable table;
        ans_build_table(freq, table);

        AnsEncoder enc;
        for (int i = 999; i >= 0; --i)
            enc.put(data[i], table);
        auto encoded = enc.finish();

        AnsDecoder dec;
        dec.init(encoded.data(), encoded.size());
        std::vector<uint8_t> decoded(1000);
        for (int i = 0; i < 1000; ++i)
            decoded[i] = dec.get(table);

        assert(decoded == data && "Test 3 round-trip mismatch");
        // Entropy: 0.7*log2(1/0.7) + 0.2*log2(1/0.2) + 0.1*log2(1/0.1) = 1.157 bits/sym
        // Encoded should be close to 1000 * 1.157 / 8 = ~144 bytes
        std::cout << "Test 3 PASS: skewed dist 1000 bytes -> "
                  << encoded.size() << " bytes (entropy ~ 144B)\n";
    }

    std::cout << "\nAll ANS tests PASSED!\n";
    return 0;
}
