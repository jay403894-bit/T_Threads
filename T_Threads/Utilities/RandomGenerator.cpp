#include "RandomGenerator.h"
int RandomGenerator::GenerateInt(int min, int max) {
    std::lock_guard<std::mutex> lock(rngMutex);
    std::uniform_int_distribution<int> dist(min, max);
    return dist(engine);
}
float RandomGenerator::GenerateFloat(float min, float max) {
    std::lock_guard<std::mutex> lock(rngMutex);
    std::uniform_int_distribution<float> dist(min, max);
    return dist(engine);
}

double RandomGenerator::GenerateDouble(double min, double max) {
    std::lock_guard<std::mutex> lock(rngMutex);
    std::uniform_real_distribution<double> dist(min, max);
    return dist(engine);
}