#include "RandomGenerator.h"
int RandomGenerator::generateInt(int min, int max) {
    std::lock_guard<std::mutex> lock(rng_mutex_);
    std::uniform_int_distribution<int> dist(min, max);
    return dist(engine_);
}
float RandomGenerator::generateFloat(float min, float max) {
    std::lock_guard<std::mutex> lock(rng_mutex_);
    std::uniform_real_distribution<float> dist(min, max);
    return dist(engine_);
}

double RandomGenerator::generateDouble(double min, double max) {
    std::lock_guard<std::mutex> lock(rng_mutex_);
    std::uniform_real_distribution<double> dist(min, max);
    return dist(engine_);
}