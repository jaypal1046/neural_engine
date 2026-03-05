// Quick tool to create a test FP32 model file
#include <fstream>
#include <vector>
#include <random>
#include <iostream>

int main() {
    const size_t num_floats = 10000;  // 40 KB model

    std::vector<float> data(num_floats);

    // Random values
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (size_t i = 0; i < num_floats; i++) {
        data[i] = dist(gen);
    }

    // Write to file
    std::ofstream file("test_model.bin", std::ios::binary);
    file.write(reinterpret_cast<const char*>(&num_floats), sizeof(num_floats));
    file.write(reinterpret_cast<const char*>(data.data()), num_floats * sizeof(float));
    file.close();

    std::cout << "Created test_model.bin with " << num_floats << " floats ("
              << (num_floats * sizeof(float) / 1024.0) << " KB)\n";

    return 0;
}
