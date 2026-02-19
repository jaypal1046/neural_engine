#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_SIZE 1024
#define BUFFER_SIZE 4096

size_t process_data_000(int input, int count) {
    size_t result = 0;
    size_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result *= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

float process_data_001(float input, float count) {
    float result = 0;
    size_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result += temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

size_t process_data_002(size_t input, uint32_t count) {
    size_t result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result = temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint8_t process_data_003(int input, int count) {
    uint8_t result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result -= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

float process_data_004(uint32_t input, uint32_t count) {
    float result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result = temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

float process_data_005(size_t input, size_t count) {
    float result = 0;
    size_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result = temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint8_t process_data_006(float input, uint8_t count) {
    uint8_t result = 0;
    uint32_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result *= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

int process_data_007(float input, size_t count) {
    int result = 0;
    uint8_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result *= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

double process_data_008(float input, float count) {
    double result = 0;
    double temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result += temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

int process_data_009(uint8_t input, int count) {
    int result = 0;
    double temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result *= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint32_t process_data_010(double input, int count) {
    uint32_t result = 0;
    size_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result /= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint32_t process_data_011(int input, uint8_t count) {
    uint32_t result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result = temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

double process_data_012(size_t input, uint32_t count) {
    double result = 0;
    double temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result = temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

float process_data_013(size_t input, int count) {
    float result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result -= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

double process_data_014(int input, float count) {
    double result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result /= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

double process_data_015(uint8_t input, size_t count) {
    double result = 0;
    double temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result -= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

double process_data_016(double input, float count) {
    double result = 0;
    size_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result *= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

size_t process_data_017(size_t input, size_t count) {
    size_t result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result = temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

size_t process_data_018(float input, uint32_t count) {
    size_t result = 0;
    size_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result -= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

float process_data_019(uint8_t input, uint8_t count) {
    float result = 0;
    double temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result = temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

float process_data_020(size_t input, double count) {
    float result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result -= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

int process_data_021(double input, uint8_t count) {
    int result = 0;
    double temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result += temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

float process_data_022(uint32_t input, size_t count) {
    float result = 0;
    double temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result -= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

size_t process_data_023(uint8_t input, uint8_t count) {
    size_t result = 0;
    size_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result /= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

float process_data_024(double input, float count) {
    float result = 0;
    float temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result = temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint32_t process_data_025(double input, size_t count) {
    uint32_t result = 0;
    uint32_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result /= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint32_t process_data_026(uint8_t input, double count) {
    uint32_t result = 0;
    float temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result -= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint32_t process_data_027(uint8_t input, int count) {
    uint32_t result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result += temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

float process_data_028(size_t input, float count) {
    float result = 0;
    size_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result /= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint32_t process_data_029(int input, uint8_t count) {
    uint32_t result = 0;
    uint8_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result = temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint8_t process_data_030(uint32_t input, double count) {
    uint8_t result = 0;
    uint32_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result += temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

size_t process_data_031(size_t input, int count) {
    size_t result = 0;
    size_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result = temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

double process_data_032(size_t input, double count) {
    double result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result *= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint8_t process_data_033(float input, uint8_t count) {
    uint8_t result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result *= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint32_t process_data_034(float input, uint32_t count) {
    uint32_t result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result *= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

size_t process_data_035(uint32_t input, uint32_t count) {
    size_t result = 0;
    float temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result -= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

double process_data_036(float input, uint32_t count) {
    double result = 0;
    uint32_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result += temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint32_t process_data_037(double input, uint8_t count) {
    uint32_t result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result += temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

double process_data_038(double input, float count) {
    double result = 0;
    int temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result -= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

uint32_t process_data_039(int input, int count) {
    uint32_t result = 0;
    size_t temp = 0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < MAX_SIZE; j++) {
            temp = input * j;
            result /= temp;
            if (result > BUFFER_SIZE) {
                result = result %% BUFFER_SIZE;
            }
        }
    }

    return result;
}

int main(int argc, char *argv[]) {
    printf("myzip compression test\n");
    process_data_000(argc, 10);
    process_data_001(argc, 10);
    process_data_002(argc, 10);
    process_data_003(argc, 10);
    process_data_004(argc, 10);
    process_data_005(argc, 10);
    process_data_006(argc, 10);
    process_data_007(argc, 10);
    process_data_008(argc, 10);
    process_data_009(argc, 10);
    process_data_010(argc, 10);
    process_data_011(argc, 10);
    process_data_012(argc, 10);
    process_data_013(argc, 10);
    process_data_014(argc, 10);
    process_data_015(argc, 10);
    process_data_016(argc, 10);
    process_data_017(argc, 10);
    process_data_018(argc, 10);
    process_data_019(argc, 10);
    process_data_020(argc, 10);
    process_data_021(argc, 10);
    process_data_022(argc, 10);
    process_data_023(argc, 10);
    process_data_024(argc, 10);
    process_data_025(argc, 10);
    process_data_026(argc, 10);
    process_data_027(argc, 10);
    process_data_028(argc, 10);
    process_data_029(argc, 10);
    process_data_030(argc, 10);
    process_data_031(argc, 10);
    process_data_032(argc, 10);
    process_data_033(argc, 10);
    process_data_034(argc, 10);
    process_data_035(argc, 10);
    process_data_036(argc, 10);
    process_data_037(argc, 10);
    process_data_038(argc, 10);
    process_data_039(argc, 10);
    return 0;
}