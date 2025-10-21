#include "RandomGenerator.h"
int RandomGenerator::GenerateInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(engine);
}

double RandomGenerator::GenerateDouble(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(engine);
}