// Canary binary: triggers data race detectable by TSan.
// Expected exit code with TSan: non-zero (sanitizer abort).
#include <thread>

int global_counter = 0;  // NOLINT

void increment()
{
    for (int i = 0; i < 10000; ++i)
    {
        // Intentional unsynchronized access - TSan should detect race
        global_counter++;  // NOLINT
    }
}

int main()
{
    std::thread t1(increment);
    std::thread t2(increment);
    t1.join();
    t2.join();
    return 0;
}
