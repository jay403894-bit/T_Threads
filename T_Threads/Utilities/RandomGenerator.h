#include <random>

class RandomGenerator{
public:
    static RandomGenerator& Get() {
        static RandomGenerator instance;
        return instance;
    };
    RandomGenerator() : engine(std::random_device{}()) {}
    RandomGenerator(const RandomGenerator& other) = delete;
    RandomGenerator& operator=(const RandomGenerator&) = delete;
    ~RandomGenerator() = default;

    int GenerateInt(int min, int max);
    double GenerateDouble(double min, double max);
  

private:
    std::default_random_engine engine;
};