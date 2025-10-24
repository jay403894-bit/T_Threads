#include <random>
#include <mutex>
class RandomGenerator{
public:
    static RandomGenerator& instance() {
        static RandomGenerator instance;
        return instance;
    };
    RandomGenerator() : engine_(std::random_device{}()) {}
    RandomGenerator(const RandomGenerator& other) = delete;
    RandomGenerator& operator=(const RandomGenerator&) = delete;
    ~RandomGenerator() = default;

    int generateInt(int min, int max);
    float generateFloat(float min, float max);
    double generateDouble(double min, double max);
  

private:
    std::default_random_engine engine_;
    std::mutex rng_mutex_;
};