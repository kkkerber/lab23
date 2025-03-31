#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include <limits>
#include <chrono>
#include <iomanip>

using namespace std;
using namespace std::chrono;

// Клас таймера
class Timer {
public:
    void start() {
        startTime = high_resolution_clock::now();
    }

    long long stop() {
        auto endTime = high_resolution_clock::now();
        return duration_cast<microseconds>(endTime - startTime).count();
    }

private:
    high_resolution_clock::time_point startTime;
};

// Генерація випадкових чисел
void fillVector(vector<int>& arr) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(-10000, 10000);
    for (auto& x : arr) x = dist(gen);
}

// Послідовна обробка
pair<int, int> getSequential(const vector<int>& arr) {
    int diffOdd = 0;
    int minOdd = numeric_limits<int>::max();

    for (int val : arr) {
        if (val % 2 != 0) {
            diffOdd -= val;
            minOdd = min(minOdd, val);
        }
    }

    return {diffOdd, minOdd};
}

// Блокуюча обробка з вимірюванням часу очікування м'ютекса
pair<pair<int, int>, long long> getBlocking(const vector<int>& arr) {
    int diffOdd = 0;
    int minOdd = numeric_limits<int>::max();
    mutex m;
    atomic<long long> waitTime(0);

    auto worker = [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            if (arr[i] % 2 != 0) {
                auto t0 = high_resolution_clock::now();
                m.lock();
                auto t1 = high_resolution_clock::now();
                waitTime += duration_cast<microseconds>(t1 - t0).count();

                diffOdd -= arr[i];
                minOdd = min(minOdd, arr[i]);
                m.unlock();
            }
        }
    };

    int threadCount = 4;
    vector<thread> threads;
    int chunkSize = arr.size() / threadCount;

    for (int i = 0; i < threadCount; ++i) {
        int start = i * chunkSize;
        int end = (i == threadCount - 1) ? arr.size() : start + chunkSize;
        threads.emplace_back(worker, start, end);
    }

    for (auto& t : threads) t.join();

    return {{diffOdd, minOdd}, waitTime.load()};
}

// Неблокуюча версія з атомарними змінними
pair<int, int> getNonBlocking(const vector<int>& arr) {
    atomic<int> diffOdd(0);
    atomic<int> minOdd(numeric_limits<int>::max());

    auto worker = [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            int val = arr[i];
            if (val % 2 != 0) {
                int oldVal, newVal;

                // CAS для віднімання
                do {
                    oldVal = diffOdd.load();
                    newVal = oldVal - val;
                } while (!diffOdd.compare_exchange_weak(oldVal, newVal));

                // CAS для мінімуму
                do {
                    oldVal = minOdd.load();
                    newVal = min(oldVal, val);
                } while (val < oldVal && !minOdd.compare_exchange_weak(oldVal, newVal));
            }
        }
    };

    int threadCount = 4;
    vector<thread> threads;
    int chunkSize = arr.size() / threadCount;

    for (int i = 0; i < threadCount; ++i) {
        int start = i * chunkSize;
        int end = (i == threadCount - 1) ? arr.size() : start + chunkSize;
        threads.emplace_back(worker, start, end);
    }

    for (auto& t : threads) t.join();

    return {diffOdd.load(), minOdd.load()};
}
int main() {
    vector<int> sizes = {1000, 10000, 100000, 1000000};

    cout << setw(12) << "Size"
         << setw(18) << "Sequential"
         << setw(18) << "Blocking"
         << setw(20) << "NonBlocking" << endl;

    for (int size : sizes) {
        vector<int> arr(size);
        fillVector(arr);

        Timer timer;

        timer.start();
        auto seq = getSequential(arr);
        auto tSeq = timer.stop();

        timer.start();
        auto [blocking, _] = getBlocking(arr);  // Ігноруємо waitTime
        auto tBlocking = timer.stop();

        timer.start();
        auto nonBlocking = getNonBlocking(arr);
        auto tNonBlocking = timer.stop();

        cout << setw(12) << size
             << setw(18) << tSeq
             << setw(18) << tBlocking
             << setw(20) << tNonBlocking << endl;
    }

    return 0;
}

