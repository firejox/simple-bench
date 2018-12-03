#ifndef SIMPLEBENCH_HPP
#define SIMPLEBENCH_HPP

#include <cmath>
#include <chrono>
#include <ratio>
#include <cstdio>
#include <utility>

namespace SimpleBenchmark {
    using namespace std::chrono_literals;

    using TimeUnit = std::chrono::duration<double, std::milli>;
    using DeciSecond = std::chrono::duration<double, std::deci>;
    using Second = std::chrono::duration<double>;

    template<char ...ch>
        struct TaskName {
            static constexpr char name[sizeof...(ch) + 1] = {ch..., '\0'};
        };

#define tname(s) s##_tname

    template <class T, T... chars>
    constexpr TaskName<chars...> operator ""_tname() { return {}; }

    template<class Name, class Func>
        struct Task : Name, Func {
            static int cycles;
            static size_t size;
            static double mean;
            static double variance;
            static double stddev;
            static double relative_stddev;

            using of = Name;

            template<class Init>
                void run_warmup(TimeUnit warmup, Init &init) const {
                    int count = 0;
                    TimeUnit total(0);

                    using self = typename std::remove_cv<std::remove_reference_t<decltype(*this)>>::type;

                    while (warmup.count() > 0) {
                        init();

                        auto before = std::chrono::high_resolution_clock::now();

                        Func::operator()();

                        auto after = std::chrono::high_resolution_clock::now();

                        auto tmp = std::chrono::duration_cast<TimeUnit>(after - before);

                        total += tmp;
                        warmup -= tmp;
                        count++;
                    }

                    self::cycles = std::max(1, int(count / std::chrono::duration_cast<DeciSecond>(total).count()));
                }

            template<class Init>
                void run_calculate(TimeUnit calc_time, Init &init) const {
                    using self = typename std::remove_cv<std::remove_reference_t<decltype(*this)>>::type;
                    self::size = 0;
                    
                    do {
                        TimeUnit subtotal(0);

                        for (int i = 0; i < self::cycles; i++) {
                            init();

                            auto before = std::chrono::high_resolution_clock::now();

                            Func::operator()();

                            auto after = std::chrono::high_resolution_clock::now();

                            subtotal += std::chrono::duration_cast<TimeUnit>(after - before);
                        }

                        calc_time -= subtotal;

                        auto measure_n = std::chrono::duration_cast<Second>(subtotal).count();
                        self::size++;

                        if (size > 1) {
                            auto delta = measure_n - self::mean;
                            self::mean += delta / self::size;
                            auto delta2 = measure_n - self::mean;
                            self::variance += delta * delta2;

                        } else {
                            self::mean = measure_n;
                        }

                    } while(calc_time.count() > 0);

                    self::variance /= self::size;
                    self::stddev = std::sqrt(self::variance);
                    self::relative_stddev = 100.0 * (self::stddev / self::mean);
                }

            constexpr void clean() const {
                using self = typename std::remove_cv<std::remove_reference_t<decltype(*this)>>::type;

                self::cycles = 0;
                self::size = 0;
                self::mean = 0;
                self::variance = 0;
                self::stddev = 0;
                self::relative_stddev = 0;
            }

        };

    template<class Name, class Func>
        int Task<Name,Func>::cycles = 0;
    template<class Name, class Func>
        size_t Task<Name,Func>::size = 0;
    template<class Name, class Func>
        double Task<Name,Func>::mean = 0;
    template<class Name, class Func>
        double Task<Name,Func>::variance = 0;
    template<class Name, class Func>
        double Task<Name,Func>::stddev = 0;
    template<class Name, class Func>
        double Task<Name,Func>::relative_stddev = 0;

    template<class Name, class Func>
        Task(Name, Func) -> Task<Name, Func>;

    template<class Init, class ...T>
        class IPS : Init, T... {

            template<size_t ...I>
                constexpr void run(TimeUnit warmup, TimeUnit calc, bool is_tty, std::index_sequence<I...>) const {
                    const auto init = [this]() constexpr { Init::operator()(); };
                    ((([&]() constexpr {
                       T::clean();
                       T::run_warmup(warmup, init);
                       T::run_calculate(calc, init);
                       report(I, is_tty, std::make_index_sequence<sizeof...(T)>());
                       })()),...);
                }

            template <size_t ...I>
                constexpr void report(size_t x, bool is_tty, std::index_sequence<I...>) const {
                    auto ftsk = fast_task(x, std::make_index_sequence<sizeof...(T)>());
                    auto stsk = slow_task(x, std::make_index_sequence<sizeof...(T)>());
                    auto lntsk = longest_name_task(x, std::make_index_sequence<sizeof...(T)>());
                    auto sz = snprintf(NULL, 0, "%5.2lf", (double)(ftsk) / stsk) - 3;

                    if (is_tty && x)
                        printf("\e[%zuA", x);


                    (((I <= x) && 
                      (([&]()constexpr ->bool{
                        printf("%*s", (int)lntsk, T::of::name);

                        if (T::mean < 1e3)
                            printf(" %6.2lf  (%6.2lfs )", T::mean, 1 / T::mean);
                        else if (T::mean < 1e6)
                            printf(" %6.2lfk (%6.2lfms)", T::mean/1e3, 1e3 / T::mean);
                        else if (T::mean < 1e9)
                            printf(" %6.2lfM (%6.2lfus)", T::mean/1e6, 1e6 / T::mean);
                        else
                            printf(" %6.2lfG (%6.2lfns)", T::mean/1e9, 1e9 / T::mean);

                        printf(" (±%5.2lf%%)", T::relative_stddev);

                        if (ftsk == T::size)
                        printf(" %*s fastest\n", sz + 3, "");
                        else
                        printf(" %*.2lf× slower\n", sz, (double)ftsk / T::size);

                        return true;
                      })())) &&...);
                }

            template <size_t...I>
                constexpr size_t fast_task(size_t x, std::index_sequence<I...>) const {
                    size_t sz = 0;
                    (((I <= x) && (((sz < T::size) && (sz = T::size)) ||true)) &&...);
                    return sz;
                }

            template <size_t...I>
                constexpr size_t slow_task(size_t x, std::index_sequence<I...>) const {
                    size_t sz = std::numeric_limits<size_t>::max();

                    (((I <= x) && (((sz > T::size) && (sz = T::size)) ||true)) &&...);
                    return sz;
                }

            template <size_t...I>
                constexpr size_t longest_name_task(size_t x, std::index_sequence<I...>) const {
                    size_t sz = 0;        
                    (((I <= x) && (((sz < sizeof(T::of::name)) && (sz = sizeof(T::of::name))) ||true)) &&...);
                    return sz;
                }
            public:

            IPS(Init init, T... task) : Init(init), T(task)... {}

            constexpr void run(TimeUnit warmup, TimeUnit calc, bool is_tty = true) const {
                run(warmup, calc, is_tty, std::make_index_sequence<sizeof...(T)>());
            }
        };

    template <class Init, class ...T>
        IPS(Init, T...) -> IPS<Init, T...>;

    constexpr auto dummy_init = []()constexpr {};
};

#endif
