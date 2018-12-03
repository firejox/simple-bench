# Simple Benchmark Library

Simple Benchmark Library is inspired from [crystal](https://github.com/crystal-lang/crystal) benchmark library
 and wroted in c++. This required **gnu string literal operator templates** and c++17 supports.

## Example

```c++
#include "SimpleBench.hpp"
#include <array>
#include <algorithm>
#include <random>

int main(void) {
    {
        using namespace SimpleBenchmark;
        constexpr size_t N = 100000;
        std::array<int, N> arr;
        std::random_device rd;
        std::mt19937 gen(rd());

        IPS ips = {
            [&arr, &gen](){
                for (int i = 0; i < N; i++)
                    arr[i] = i;
                std::shuffle(arr.begin(), arr.end(), gen);
            },
            Task {
                "selection sort"_tname,
                [&arr](){
                    const auto end = arr.end();
                    for (auto i = arr.begin(); i < end; i++)
                        std::iter_swap(i, std::min_element(i, end));
                }
            },
            Task {
                "std sort"_tname,
                [&arr](){
                    std::sort(arr.begin(), arr.end());
                }
            }
        };

        ips.run(2s, 5s);
    }
    return 0;
}
```

## Result

```
 selection sort  30.44  (  0.03s ) (± 0.00%) 55.00× slower
       std sort   0.09  ( 10.95s ) (± 0.79%)       fastest
```
