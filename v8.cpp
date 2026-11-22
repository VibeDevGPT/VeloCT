
// === sf_src/src/thread_native.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef THREAD_NATIVE_H_INCLUDED
#define THREAD_NATIVE_H_INCLUDED

#ifdef _MSC_VER
    #include <thread>
#else
    #include <pthread.h>
    #include <cstdlib>
    #include <cstring>
    #include <functional>
    #include <iostream>
    #include <tuple>
    #include <utility>


// === sf_src/src/misc.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>  // IWYU pragma: keep
// IWYU pragma: no_include <__exception/terminate.h>
#include <functional>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#if !defined(NO_PREFETCH) && (defined(_MSC_VER) || defined(__INTEL_COMPILER))
    #include <immintrin.h>
#endif

#define stringify2(x) #x
#define stringify(x) stringify2(x)

namespace Stockfish {

using u64 = std::uint64_t;
using u32 = std::uint32_t;
using u16 = std::uint16_t;
using u8  = std::uint8_t;

using i64 = std::int64_t;
using i32 = std::int32_t;
using i16 = std::int16_t;
using i8  = std::int8_t;

using usize = std::size_t;
using isize = std::ptrdiff_t;

#if defined(__GNUC__) && defined(IS_64BIT)
__extension__ using u128 = unsigned __int128;
__extension__ using i128 = signed __int128;
#endif

std::string engine_version_info();
std::string engine_info(bool to_uci = false);
std::string compiler_info();

// Prefetch hint enums for explicit call-site control.
enum class PrefetchRw {
    READ,
    WRITE
};

// NOTE: PrefetchLoc controls locality / cache level, not whether a prefetch
//       is issued. In particular, PrefetchLoc::NONE maps to a non-temporal /
//       lowest-locality prefetch (Intel: _MM_HINT_NTA, GCC/Clang: locality = 0)
//       and therefore still performs a prefetch. To completely disable
//       prefetching, define NO_PREFETCH so that prefetch() becomes a no-op.
enum class PrefetchLoc {
    NONE,      // Non-temporal / no cache locality (still issues a prefetch)
    LOW,       // Low locality (e.g. T2 / L2)
    MODERATE,  // Moderate locality (e.g. T1 / L1)
    HIGH       // High locality (e.g. T0 / closest cache)
};

// Preloads the given address into cache. This is a non-blocking
// function that doesn't stall the CPU waiting for data to be loaded from memory,
// which can be quite slow.
#ifdef NO_PREFETCH
template<PrefetchRw RW = PrefetchRw::READ, PrefetchLoc LOC = PrefetchLoc::HIGH>
void prefetch(const void*) {}
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)

constexpr int get_intel_hint(PrefetchRw rw, PrefetchLoc loc) {
    if (rw == PrefetchRw::WRITE)
    #ifdef _MM_HINT_ET0
        return _MM_HINT_ET0;
    #else
        // Fallback when write-prefetch hint is not available: use T0
        return _MM_HINT_T0;
    #endif
    switch (loc)
    {
    case PrefetchLoc::NONE :
        return _MM_HINT_NTA;
    case PrefetchLoc::LOW :
        return _MM_HINT_T2;
    case PrefetchLoc::MODERATE :
        return _MM_HINT_T1;
    case PrefetchLoc::HIGH :
        return _MM_HINT_T0;
    default :
        return _MM_HINT_T0;
    }
}

template<PrefetchRw RW = PrefetchRw::READ, PrefetchLoc LOC = PrefetchLoc::HIGH>
void prefetch(const void* addr) {
    _mm_prefetch(static_cast<const char*>(addr), get_intel_hint(RW, LOC));
}
#else
template<PrefetchRw RW = PrefetchRw::READ, PrefetchLoc LOC = PrefetchLoc::HIGH>
void prefetch(const void* addr) {
    __builtin_prefetch(addr, static_cast<int>(RW), static_cast<int>(LOC));
}
#endif

void start_logger(const std::filesystem::path& fname);

std::optional<usize> str_to_size_t(const std::string& s);

std::string           utf8_from_wstring(std::wstring_view s);
std::filesystem::path path_from_utf8(const std::string& path);

// Reads the file as bytes.
// Returns std::nullopt if the file does not exist.
std::optional<std::string> read_file_to_string(const std::string& path);

bool dbg_hit_on(bool cond, int slot = 0);
i64  dbg_mean_of(i64 value, int slot = 0);
i64  dbg_stdev_of(i64 value, int slot = 0);
i64  dbg_extremes_of(i64 value, int slot = 0);
void dbg_correl_of(i64 value1, i64 value2, int slot = 0);
void dbg_print();
void dbg_clear();

using TimePoint = std::chrono::milliseconds::rep;  // A value in milliseconds
static_assert(sizeof(TimePoint) == sizeof(i64), "TimePoint should be 64 bits");
inline TimePoint now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

inline std::vector<std::string_view> split(std::string_view s, std::string_view delimiter) {
    std::vector<std::string_view> res;

    if (s.empty())
        return res;

    usize begin = 0;
    for (;;)
    {
        const usize end = s.find(delimiter, begin);
        if (end == std::string_view::npos)
            break;

        res.emplace_back(s.substr(begin, end - begin));
        begin = end + delimiter.size();
    }

    res.emplace_back(s.substr(begin));

    return res;
}

void remove_whitespace(std::string& s);
bool is_whitespace(std::string_view s);

enum SyncCout {
    IO_LOCK,
    IO_UNLOCK
};
std::ostream& operator<<(std::ostream&, SyncCout);

#define sync_cout std::cout << IO_LOCK
#define sync_endl std::endl << IO_UNLOCK

void sync_cout_start();
void sync_cout_end();

// True if and only if the binary is compiled on a little-endian machine
static inline const u16  Le             = 1;
static inline const bool IsLittleEndian = *reinterpret_cast<const char*>(&Le) == 1;


// std::swap is not constexpr until C++20
template<typename T>
constexpr void swap_constexpr(T& a, T& b) {
    T tmp = a;
    a     = b;
    b     = tmp;
}


template<typename T, usize MaxSize>
class ValueList {

   public:
    usize size() const { return size_; }
    int   ssize() const { return int(size_); }
    void  push_back(const T& value) {
        assert(size_ < MaxSize);
        values_[size_++] = value;
    }
    // pushes back value if value < max
    void push_back_if_lt(const T& value, const T& max) {
        assert(size_ < MaxSize);
        values_[size_] = value;
        size_ += (value < max);
    }
    const T* begin() const { return values_; }
    const T* end() const { return values_ + size_; }
    const T& operator[](int index) const { return values_[index]; }

    T* make_space(usize count) {
        T* result = &values_[size_];
        size_ += count;
        assert(size_ <= MaxSize);
        return result;
    }

   private:
    T     values_[MaxSize];
    usize size_ = 0;
};


template<typename T, usize Size, usize... Sizes>
class MultiArray;

namespace Detail {

template<typename T, usize Size, usize... Sizes>
struct MultiArrayHelper {
    using ChildType = MultiArray<T, Sizes...>;
};

template<typename T, usize Size>
struct MultiArrayHelper<T, Size> {
    using ChildType = T;
};

template<typename To, typename From>
constexpr bool is_strictly_assignable_v =
  std::is_assignable_v<To&, From> && (std::is_same_v<To, From> || !std::is_convertible_v<From, To>);

}

// MultiArray is a generic N-dimensional array.
// The template parameters (Size and Sizes) encode the dimensions of the array.
template<typename T, usize Size, usize... Sizes>
class MultiArray {
    using ChildType = typename Detail::MultiArrayHelper<T, Size, Sizes...>::ChildType;
    using ArrayType = std::array<ChildType, Size>;
    ArrayType data_;

   public:
    using value_type             = typename ArrayType::value_type;
    using size_type              = typename ArrayType::size_type;
    using difference_type        = typename ArrayType::difference_type;
    using reference              = typename ArrayType::reference;
    using const_reference        = typename ArrayType::const_reference;
    using pointer                = typename ArrayType::pointer;
    using const_pointer          = typename ArrayType::const_pointer;
    using iterator               = typename ArrayType::iterator;
    using const_iterator         = typename ArrayType::const_iterator;
    using reverse_iterator       = typename ArrayType::reverse_iterator;
    using const_reverse_iterator = typename ArrayType::const_reverse_iterator;

    constexpr auto&       at(size_type index) { return data_.at(index); }
    constexpr const auto& at(size_type index) const { return data_.at(index); }

    constexpr auto& operator[](size_type index) noexcept {
        assert(index < Size);
        return data_[index];
    }
    constexpr const auto& operator[](size_type index) const noexcept {
        assert(index < Size);
        return data_[index];
    }

    constexpr auto&       front() noexcept { return data_.front(); }
    constexpr const auto& front() const noexcept { return data_.front(); }
    constexpr auto&       back() noexcept { return data_.back(); }
    constexpr const auto& back() const noexcept { return data_.back(); }

    auto*       data() { return data_.data(); }
    const auto* data() const { return data_.data(); }

    constexpr auto begin() noexcept { return data_.begin(); }
    constexpr auto end() noexcept { return data_.end(); }
    constexpr auto begin() const noexcept { return data_.begin(); }
    constexpr auto end() const noexcept { return data_.end(); }
    constexpr auto cbegin() const noexcept { return data_.cbegin(); }
    constexpr auto cend() const noexcept { return data_.cend(); }

    constexpr auto rbegin() noexcept { return data_.rbegin(); }
    constexpr auto rend() noexcept { return data_.rend(); }
    constexpr auto rbegin() const noexcept { return data_.rbegin(); }
    constexpr auto rend() const noexcept { return data_.rend(); }
    constexpr auto crbegin() const noexcept { return data_.crbegin(); }
    constexpr auto crend() const noexcept { return data_.crend(); }

    constexpr bool      empty() const noexcept { return data_.empty(); }
    constexpr size_type size() const noexcept { return data_.size(); }
    constexpr size_type max_size() const noexcept { return data_.max_size(); }

    template<typename U>
    void fill(const U& v) {
        static_assert(Detail::is_strictly_assignable_v<T, U>,
                      "Cannot assign fill value to entry type");
        for (auto& ele : data_)
            if constexpr (sizeof...(Sizes) == 0)
                ele = v;
            else
                ele.fill(v);
    }

    constexpr void swap(MultiArray<T, Size, Sizes...>& other) noexcept { data_.swap(other.data_); }
};

// Wrapper around std::atomic<T> which uses relaxed accesses or plain
// accesses, depending on the config. Intended use is e.g. wasm where
// the overhead of atomic instructions can be significant, and we only
// require non-tearing for the updates, while ensuring we use relaxed
// accesses otherwise.
template<typename T>
class RelaxedAtomic {
    static constexpr bool UseAtomic =
#ifdef USE_SLOPPY_ATOMICS
      !std::atomic<T>::is_always_lock_free || sizeof(T) > sizeof(usize);
#else
      true;
#endif

   public:
    RelaxedAtomic() = default;
    RelaxedAtomic(T val) :
        inner(val) {}
    RelaxedAtomic(const RelaxedAtomic& a) :
        inner(static_cast<T>(a)) {}

    T operator=(T val) {
        this->store(val, std::memory_order_relaxed);
        return val;
    }

    RelaxedAtomic& operator=(const RelaxedAtomic& a) {
        this->store(static_cast<T>(a), std::memory_order_relaxed);
        return *this;
    }

    operator T() const { return this->load(std::memory_order_relaxed); }

    RelaxedAtomic& operator+=(T val) {
        this->store(this->load(std::memory_order_relaxed) + val, std::memory_order_relaxed);
        return *this;
    }

    RelaxedAtomic& operator++() { return *this += 1; }

    RelaxedAtomic& operator--() { return *this -= 1; }

    T operator++(int) {
        T val = this->load(std::memory_order_relaxed);
        this->store(val + 1, std::memory_order_relaxed);
        return val;
    }

    T operator--(int) {
        T val = this->load(std::memory_order_relaxed);
        this->store(val - 1, std::memory_order_relaxed);
        return val;
    }

    RelaxedAtomic& operator-=(T val) {
        this->store(this->load(std::memory_order_relaxed) - val, std::memory_order_relaxed);
        return *this;
    }

    T load(std::memory_order order) const {
        assert(order == std::memory_order_relaxed);
        if constexpr (UseAtomic)
            return inner.load(order);
        else
            return inner;
    }

    void store(T val, std::memory_order order) {
        assert(order == std::memory_order_relaxed);
        if constexpr (UseAtomic)
            inner.store(val, order);
        else
            inner = val;
    }

   private:
    std::conditional_t<UseAtomic, std::atomic<T>, T> inner;
};

// xorshift64star Pseudo-Random Number Generator
// This class is based on original code written and dedicated
// to the public domain by Sebastiano Vigna (2014).
// It has the following characteristics:
//
//  -  Outputs 64-bit numbers
//  -  Passes Dieharder and SmallCrush test batteries
//  -  Does not require warm-up, no zeroland to escape
//  -  Internal state is a single 64-bit integer
//  -  Period is 2^64 - 1
//  -  Speed: 1.60 ns/call (Core i7 @3.40GHz)
//
// For further analysis see
//   <http://vigna.di.unimi.it/ftp/papers/xorshift.pdf>

class PRNG {

    u64 s;

    u64 rand64() {

        s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
        return s * 2685821657736338717LL;
    }

   public:
    PRNG(u64 seed) :
        s(seed) {
        assert(seed);
    }

    template<typename T>
    T rand() {
        return T(rand64());
    }

    // Special generator used to fast init magic numbers.
    // Output values only have 1/8th of their bits set on average.
    template<typename T>
    T sparse_rand() {
        return T(rand64() & rand64() & rand64());
    }
};

inline usize mul_hi64(u64 a, usize b) {
#if defined(__GNUC__) && defined(IS_64BIT) && !defined(__wasm__)
    return (u128(a) * u128(b)) >> 64;
#else
    u64 aL = u32(a), aH = a >> 32;
    u64 bL = u32(b), bH = u64(b) >> 32;
    u64 c1 = (aL * bL) >> 32;
    u64 c2 = aH * bL + c1;
    u64 c3 = aL * bH + u32(c2);
    return aH * bH + (c2 >> 32) + (c3 >> 32);
#endif
}

template<typename T1, typename T2>
inline constexpr T2 interpolate(T1 x, T1 x0, T1 x1, T2 y0, T2 y1) {
    assert(x0 != x1);
    return T2(y0 + (y1 - y0) * (x - x0) / (x1 - x0));
}

u64 hash_bytes(const char*, usize);

template<typename T>
inline usize get_raw_data_hash(const T& value) {
    // We must have no padding bytes because we're reinterpreting as char
    static_assert(std::has_unique_object_representations_v<T>);

    return static_cast<usize>(hash_bytes(reinterpret_cast<const char*>(&value), sizeof(value)));
}

template<typename T>
inline void hash_combine(usize& seed, const T& v) {
    usize x;
    // For primitive types we avoid using the default hasher, which may be
    // nondeterministic across program invocations
    if constexpr (std::is_integral_v<T>)
        x = v;
    else
        x = std::hash<T>{}(v);
    seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

inline u64 hash_string(const std::string& sv) { return hash_bytes(sv.data(), sv.size()); }

struct CommandLine {
   public:
    CommandLine(int _argc, char** _argv);

    CommandLine(const CommandLine&)            = delete;
    CommandLine& operator=(const CommandLine&) = delete;
    CommandLine(CommandLine&&)                 = default;
    CommandLine& operator=(CommandLine&&)      = default;

    static std::filesystem::path get_binary_directory(std::filesystem::path argv0);

    int    argc;
    char** argv;

   private:
#ifdef _WIN32
    std::vector<std::string> argv_storage;
    std::vector<char*>       argv_utf8;
#endif
};

namespace Utility {

template<typename T, typename Predicate>
void move_to_front(std::vector<T>& vec, Predicate pred) {
    auto it = std::find_if(vec.begin(), vec.end(), pred);

    if (it != vec.end())
        std::rotate(vec.begin(), it, it + 1);
}
}

#ifndef __has_builtin
    #define __has_builtin(x) 0
#endif

#if defined(__GNUC__)
    #define sf_always_inline inline __attribute__((always_inline))
#elif defined(_MSC_VER)
    #define sf_always_inline __forceinline
#else
    // plain inline for other compilers
    #define sf_always_inline inline
#endif

#if defined(__clang__)
    #define sf_assume(cond) __builtin_assume(cond)
#elif defined(__GNUC__)
    #if __GNUC__ >= 13
        #define sf_assume(cond) __attribute__((assume(cond)))
    #else
        #define sf_assume(cond) \
            do \
            { \
                if (!(cond)) \
                    __builtin_unreachable(); \
            } while (0)
    #endif
#elif defined(_MSC_VER)
    #define sf_assume(cond) __assume(cond)
#else
    // do nothing for other compilers
    #define sf_assume(cond)
#endif

#ifdef __GNUC__
    #define sf_unreachable() __builtin_unreachable()
#elif defined(_MSC_VER)
    #define sf_unreachable() __assume(0)
#else
    #define sf_unreachable()
#endif

#ifdef __GNUC__
    #define RESTRICT __restrict__
#elif defined(_MSC_VER)
    #define RESTRICT __restrict
#else
    #define RESTRICT
#endif

#ifdef __clang__
    #define UNROLLED_FOR(...) _Pragma("clang loop unroll(full)") for (__VA_ARGS__)
#elif defined(__GNUC__)
    #define UNROLLED_FOR(...) _Pragma("GCC unroll 32") for (__VA_ARGS__)
#else
    #define UNROLLED_FOR(...) for (__VA_ARGS__)
#endif

void set_console_utf8();

}  // namespace Stockfish

#endif  // #ifndef MISC_H_INCLUDED
#endif

namespace Stockfish {

struct NativeThreadOptions {
    bool largeStack{};

    NativeThreadOptions& setLargeStack(bool value) {
        largeStack = value;
        return *this;
    }
};

#ifdef _MSC_VER

// MSVC-compatible toolchains use std::thread because they do not provide
// pthreads by default. On all other platforms, pthreads is required and used.

using NativeThread = std::thread;

template<class Function, class... Args>
NativeThread create_native_thread(NativeThreadOptions options, Function&& fun, Args&&... args) {
    // TODO: implement fallible thread creation for MSVC
    (void) options;
    return NativeThread(std::forward<Function>(fun), std::forward<Args>(args)...);
}

#else

struct ThreadCallableBase {  // type erase F
    virtual ~ThreadCallableBase() = default;
    virtual void run()            = 0;
};

template<typename F, typename... Args>
struct ThreadCallable final: ThreadCallableBase {
    F                   func_;
    std::tuple<Args...> args_;
    ThreadCallable(F&& f, Args&&... args) :
        func_(std::forward<F>(f)),
        args_(std::make_tuple(std::forward<Args>(args)...)) {}
    void run() override { std::apply(func_, args_); }
};

// On OSX threads other than the main thread are created with a reduced stack
// size of 512KB by default, this is too low for deep searches, which require
// somewhat more than 1MB stack, so adjust it to TH_STACK_SIZE.
// The implementation calls pthread_create() with the stack size parameter
// equal to the Linux 8MB default, on platforms that support it.

class NativeThread {
    pthread_t thread;
    bool      running_ = false;

    static constexpr usize TH_STACK_SIZE = 8 * 1024 * 1024;

    void start(NativeThreadOptions options, ThreadCallableBase* func) {
        pthread_attr_t attr_storage, *attr = &attr_storage;
        pthread_attr_init(attr);
        if (options.largeStack)
            pthread_attr_setstacksize(attr, TH_STACK_SIZE);

        auto start_routine = [](void* ptr) -> void* {
            auto f = reinterpret_cast<ThreadCallableBase*>(ptr);
            // Call the function
            f->run();
            delete f;
            return nullptr;
        };

        const int rc = pthread_create(&thread, attr, start_routine, func);
        pthread_attr_destroy(attr);

        if (rc != 0)
            delete func;
        else
            running_ = true;
    }

   public:
    NativeThread()                               = default;
    NativeThread(const NativeThread&)            = delete;
    NativeThread& operator=(const NativeThread&) = delete;

    NativeThread(NativeThread&& other) noexcept {
        thread         = other.thread;
        running_       = other.running_;
        other.running_ = false;
    }

    NativeThread& operator=(NativeThread&& other) noexcept {
        if (&other != this)
        {
            assert(!running_ && "Thread was not joined");
            thread         = other.thread;
            running_       = other.running_;
            other.running_ = false;
        }
        return *this;
    }

    ~NativeThread() { assert(!running_ && "Thread was not joined"); }

    bool joinable() const { return running_; }
    void join() {
        if (running_)
        {
            pthread_join(thread, nullptr);
            running_ = false;
        }
    }

    template<class Function, class... Args>
    friend NativeThread create_native_thread(NativeThreadOptions, Function&&, Args&&...);
};

template<class Function, class... Args>
inline NativeThread
create_native_thread(NativeThreadOptions options, Function&& fun, Args&&... args) {
    NativeThread thread{};
    using Callable = ThreadCallable<std::decay_t<Function>, std::decay_t<Args>...>;
    if (auto func =
          new (std::nothrow) Callable(std::forward<Function>(fun), std::forward<Args>(args)...))
        thread.start(options, func);
    return thread;
}

#endif  // _MSC_VER

}  // namespace Stockfish

#endif  // #ifndef THREAD_NATIVE_H_INCLUDED

// === sf_src/src/attacks.cpp ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


// === sf_src/src/attacks.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ATTACKS_H_INCLUDED
#define ATTACKS_H_INCLUDED

#include <cassert>
#include <array>
#include <initializer_list>
#include <utility>


// === sf_src/src/types.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TYPES_H_INCLUDED
    #define TYPES_H_INCLUDED

// When compiling with provided Makefile (e.g. for Linux and OSX), configuration
// is done automatically. To get started type 'make help'.
//
// When Makefile is not used (e.g. with Microsoft Visual Studio) some switches
// need to be set manually:
//
// -DNDEBUG      | Disable debugging mode. Always use this for release.
//
// -DNO_PREFETCH | Disable use of prefetch asm-instruction. You may need this to
//               | run on some very old machines.
//
// -DUSE_POPCNT  | Add runtime support for use of popcnt asm-instruction. Works
//               | only in 64-bit mode and requires hardware with popcnt support.
//
// -DUSE_PEXT    | Add runtime support for use of pext asm-instruction. Works
//               | only in 64-bit mode and requires hardware with pext support.

    #include <cassert>
    #include <cstddef>
    #include <cstdint>
    #include <type_traits>

    #if defined(_MSC_VER)
        // Disable some silly and noisy warnings from MSVC compiler
        #pragma warning(disable: 4127)  // Conditional expression is constant
        #pragma warning(disable: 4146)  // Unary minus operator applied to unsigned type
        #pragma warning(disable: 4800)  // Forcing value to bool 'true' or 'false'
    #endif

// Predefined macros hell:
//
// __GNUC__                Compiler is GCC, Clang or ICX
// __clang__               Compiler is Clang or ICX
// __INTEL_LLVM_COMPILER   Compiler is ICX
// _MSC_VER                Compiler is MSVC
// _WIN32                  Building on Windows (any)
// _WIN64                  Building on Windows 64 bit

    // Enforce minimum GCC version
    #if defined(__GNUC__) && !defined(__clang__) \
      && (__GNUC__ < 9 || (__GNUC__ == 9 && __GNUC_MINOR__ < 3))
        #error "Stockfish requires GCC 9.3 or later for correct compilation"
    #endif

    // Enforce minimum Clang version
    #if defined(__clang__) && (__clang_major__ < 11)
        #error "Stockfish requires Clang 11.0 or later for correct compilation"
    #endif

    #define ASSERT_ALIGNED(ptr, alignment) assert(reinterpret_cast<uintptr_t>(ptr) % alignment == 0)

    #if defined(_MSC_VER)
        #if defined(_WIN64)      // No Makefile used
            #include <intrin.h>  // Microsoft header for _BitScanForward64()
            #define IS_64BIT
        #endif

        #include <nmmintrin.h>  // Microsoft header for _mm_popcnt_u64()
    #endif

    #if defined(USE_PEXT)
        #include <immintrin.h>  // Header for _pext_u64() intrinsic
        #define pext(b, m) _pext_u64(b, m)
        #define pdep(b, m) _pdep_u64(b, m)
    #endif

namespace Stockfish {

    #ifdef USE_POPCNT
constexpr bool HasPopCnt = true;
    #else
constexpr bool HasPopCnt = false;
    #endif

    #ifdef USE_PEXT
constexpr bool HasPext = true;
    #else
constexpr bool HasPext = false;
    #endif

    #ifdef IS_64BIT
constexpr bool Is64Bit = true;
    #else
constexpr bool Is64Bit = false;
    #endif

using Key      = u64;
using Bitboard = u64;

constexpr int MAX_MOVES = 256;
constexpr int MAX_PLY   = 246;

enum Color : u8 {
    WHITE,
    BLACK,
    COLOR_NB = 2
};

enum CastlingRights : u8 {
    NO_CASTLING,
    WHITE_OO,
    WHITE_OOO = WHITE_OO << 1,
    BLACK_OO  = WHITE_OO << 2,
    BLACK_OOO = WHITE_OO << 3,

    KING_SIDE      = WHITE_OO | BLACK_OO,
    QUEEN_SIDE     = WHITE_OOO | BLACK_OOO,
    WHITE_CASTLING = WHITE_OO | WHITE_OOO,
    BLACK_CASTLING = BLACK_OO | BLACK_OOO,
    ANY_CASTLING   = WHITE_CASTLING | BLACK_CASTLING,

    CASTLING_RIGHT_NB = 16
};

enum Bound : u8 {
    BOUND_NONE,
    BOUND_UPPER,
    BOUND_LOWER,
    BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};

// Value is used as an alias for int, this is done to differentiate between a search
// value and any other integer value. The values used in search are always supposed
// to be in the range (-VALUE_NONE, VALUE_NONE] and should not exceed this range.
using Value = int;

constexpr Value VALUE_ZERO     = 0;
constexpr Value VALUE_DRAW     = 0;
constexpr Value VALUE_NONE     = 32002;
constexpr Value VALUE_INFINITE = 32001;

constexpr Value VALUE_MATE             = 32000;
constexpr Value VALUE_MATE_IN_MAX_PLY  = VALUE_MATE - MAX_PLY;
constexpr Value VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY;

constexpr Value VALUE_TB                 = VALUE_MATE_IN_MAX_PLY - 1;
constexpr Value VALUE_TB_WIN_IN_MAX_PLY  = VALUE_TB - MAX_PLY;
constexpr Value VALUE_TB_LOSS_IN_MAX_PLY = -VALUE_TB_WIN_IN_MAX_PLY;


constexpr bool is_valid(Value value) { return value != VALUE_NONE; }

constexpr bool is_win(Value value) {
    assert(is_valid(value));
    return value >= VALUE_TB_WIN_IN_MAX_PLY;
}

constexpr bool is_loss(Value value) {
    assert(is_valid(value));
    return value <= VALUE_TB_LOSS_IN_MAX_PLY;
}

constexpr bool is_decisive(Value value) { return is_win(value) || is_loss(value); }

constexpr bool is_mate(Value value) {
    assert(is_valid(value));
    return value >= VALUE_MATE_IN_MAX_PLY;
}

constexpr bool is_mated(Value value) {
    assert(is_valid(value));
    return value <= VALUE_MATED_IN_MAX_PLY;
}

constexpr bool is_mate_or_mated(Value value) { return is_mate(value) || is_mated(value); }

constexpr Value mate_in(int ply) { return VALUE_MATE - ply; }

constexpr Value mated_in(int ply) { return -VALUE_MATE + ply; }

// In the code, we make the assumption that these values
// are such that non_pawn_material() can be used to uniquely
// identify the material on the board.
constexpr Value PawnValue   = 208;
constexpr Value KnightValue = 781;
constexpr Value BishopValue = 825;
constexpr Value RookValue   = 1276;
constexpr Value QueenValue  = 2538;


// clang-format off
enum PieceType : u8 {
    NO_PIECE_TYPE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
    ALL_PIECES = 0,
    PIECE_TYPE_NB = 8
};

enum Piece : u8 {
    NO_PIECE,
    W_PAWN = PAWN,     W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN = PAWN + 8, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    PIECE_NB = 16
};
// clang-format on

constexpr Value PieceValue[PIECE_NB] = {
  VALUE_ZERO, PawnValue, KnightValue, BishopValue, RookValue, QueenValue, VALUE_ZERO, VALUE_ZERO,
  VALUE_ZERO, PawnValue, KnightValue, BishopValue, RookValue, QueenValue, VALUE_ZERO, VALUE_ZERO};

using Depth = int;

// The following DEPTH_ constants are used for transposition table entries
// and quiescence search move generation stages. In regular search, the
// depth stored in the transposition table is literal: the search depth
// (effort) used to make the corresponding transposition table value. In
// quiescence search, however, the transposition table entries only store
// the current quiescence move generation stage (which should thus compare
// lower than any regular search depth).
constexpr Depth DEPTH_QS = 0;
// For transposition table entries where no searching at all was done
// (whether regular or qsearch) we use DEPTH_UNSEARCHED, which should thus
// compare lower than any quiescence or regular depth. DEPTH_NONE is used
// for the transposition table entry occupancy check (see tt.cpp), and
// should thus be lower than DEPTH_UNSEARCHED.
constexpr Depth DEPTH_UNSEARCHED = -2;
constexpr Depth DEPTH_NONE       = -3;

// clang-format off
enum Square : u8 {
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    SQ_NONE,

    SQUARE_ZERO = 0,
    SQUARE_NB   = 64
};
// clang-format on

static_assert(std::is_unsigned_v<std::underlying_type_t<Square>>);

enum Direction : i8 {
    NORTH = 8,
    EAST  = 1,
    SOUTH = -NORTH,
    WEST  = -EAST,

    NORTH_EAST = NORTH + EAST,
    SOUTH_EAST = SOUTH + EAST,
    SOUTH_WEST = SOUTH + WEST,
    NORTH_WEST = NORTH + WEST
};

inline constexpr Direction operator-(Direction dir) { return Direction(-i8(dir)); }

enum File : u8 {
    FILE_A,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_H,
    FILE_NB
};

enum Rank : u8 {
    RANK_1,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8,
    RANK_NB
};

// Keep track of what a move changes on the board (used by NNUE)
struct DirtyPiece {
    Piece  pc;        // this is never allowed to be NO_PIECE
    Square from, to;  // to should be SQ_NONE for promotions

    // if {add,remove}_sq is SQ_NONE, {add,remove}_pc is allowed to be
    // uninitialized
    // castling uses add_sq and remove_sq to remove and add the rook
    Square remove_sq, add_sq;
    Piece  remove_pc, add_pc;
};

// Keep track of what threats change on the board (used by NNUE)
struct DirtyThreat {
    static constexpr int PcSqOffset         = 0;
    static constexpr int ThreatenedSqOffset = 8;
    static constexpr int ThreatenedPcOffset = 16;
    static constexpr int PcOffset           = 20;

    DirtyThreat() { /* don't initialize data */ }
    DirtyThreat(u32 raw) :
        data(raw) {}
    DirtyThreat(Piece pc, Piece threatened_pc, Square pc_sq, Square threatened_sq, bool add) :
        data((u32(add) << 31) | (pc << PcOffset) | (threatened_pc << ThreatenedPcOffset)
             | (threatened_sq << ThreatenedSqOffset) | (pc_sq << PcSqOffset)) {}

    Piece  pc() const { return static_cast<Piece>(data >> PcOffset & 0xf); }
    Piece  threatened_pc() const { return static_cast<Piece>(data >> ThreatenedPcOffset & 0xf); }
    Square threatened_sq() const { return static_cast<Square>(data >> ThreatenedSqOffset & 0xff); }
    Square pc_sq() const { return static_cast<Square>(data >> PcSqOffset & 0xff); }
    bool   add() const { return data >> 31; }
    u32    raw() const { return data; }

   private:
    u32 data;
};

// A piece can be involved in at most 8 outgoing attacks and 16 incoming attacks.
// Moving a piece also can reveal at most 8 discovered attacks.
// This implies that a non-castling move can change at most (8 + 16) * 3 + 8 = 80 features.
// By similar logic, a castling move can change at most (5 + 1 + 3 + 9) * 2 = 36 features.
// Thus, 80 should work as an upper bound. Finally, 16 entries are added to accommodate
// unmasked vector stores near the end of the list.

using DirtyThreatList = ValueList<DirtyThreat, 96>;

struct DirtyThreats {
    DirtyThreatList list;
};

struct DirtyPawnPairs {
    Bitboard before[COLOR_NB];
    Bitboard after[COLOR_NB];
};

struct Dirties {
    DirtyPiece     dirtyPiece;
    DirtyThreats   dirtyThreats;
    DirtyPawnPairs dirtyPawnPairs;
};

    #define ENABLE_INCR_OPERATORS_ON(T) \
        constexpr T& operator++(T& d) { return d = T(int(d) + 1); } \
        constexpr T& operator--(T& d) { return d = T(int(d) - 1); }

ENABLE_INCR_OPERATORS_ON(PieceType)
ENABLE_INCR_OPERATORS_ON(Square)
ENABLE_INCR_OPERATORS_ON(File)
ENABLE_INCR_OPERATORS_ON(Rank)

    #undef ENABLE_INCR_OPERATORS_ON

constexpr Direction operator+(Direction d1, Direction d2) { return Direction(int(d1) + int(d2)); }
constexpr Direction operator*(int i, Direction d) { return Direction(i * int(d)); }

// Additional operators to add a Direction to a Square
constexpr Square  operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
constexpr Square  operator-(Square s, Direction d) { return Square(int(s) - int(d)); }
constexpr Square& operator+=(Square& s, Direction d) { return s = s + d; }
constexpr Square& operator-=(Square& s, Direction d) { return s = s - d; }

// Toggle color
constexpr Color operator~(Color c) { return Color(c ^ BLACK); }

// Swap A1 <-> A8
constexpr Square flip_rank(Square s) { return Square(s ^ SQ_A8); }

// Swap A1 <-> H1
constexpr Square flip_file(Square s) { return Square(s ^ SQ_H1); }

// Swap color of piece B_KNIGHT <-> W_KNIGHT
constexpr Piece operator~(Piece pc) { return Piece(pc ^ 8); }

constexpr CastlingRights operator&(Color c, CastlingRights cr) {
    return CastlingRights((c == WHITE ? WHITE_CASTLING : BLACK_CASTLING) & cr);
}

constexpr Square make_square(File f, Rank r) { return Square((r << 3) + f); }

constexpr Piece make_piece(Color c, PieceType pt) { return Piece((c << 3) + pt); }

constexpr PieceType type_of(Piece pc) { return PieceType(pc & 7); }

constexpr Color color_of(Piece pc) {
    assert(pc != NO_PIECE);
    return Color(pc >> 3);
}

constexpr bool is_ok(Square s) { return s <= SQ_H8; }

constexpr File file_of(Square s) { return File(s & 7); }

constexpr Rank rank_of(Square s) { return Rank(s >> 3); }

constexpr Square relative_square(Color c, Square s) { return Square(s ^ (c * 56)); }

constexpr Rank relative_rank(Color c, Rank r) { return Rank(r ^ (c * 7)); }

constexpr Rank relative_rank(Color c, Square s) { return relative_rank(c, rank_of(s)); }

constexpr Direction pawn_push(Color c) { return c == WHITE ? NORTH : SOUTH; }


// Based on a congruential pseudo-random number generator
constexpr Key make_key(u64 seed) { return seed * 6364136223846793005ULL + 1442695040888963407ULL; }


enum MoveType : u16 {
    NORMAL,
    PROMOTION  = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING   = 3 << 14
};

// A move needs 16 bits to be stored
//
// bit  0- 5: destination square (from 0 to 63)
// bit  6-11: origin square (from 0 to 63)
// bit 12-13: promotion piece type - 2 (from KNIGHT-2 to QUEEN-2)
// bit 14-15: special move flag: promotion (1), en passant (2), castling (3)
// NOTE: en passant bit is set only when a pawn can be captured
//
// Special cases are Move::none() and Move::null(). We can sneak these in because
// in any normal move the destination square and origin square are always different,
// but Move::none() and Move::null() have the same origin and destination square.

class Move {
   public:
    Move() = default;
    constexpr explicit Move(u16 d) :
        data(d) {}

    constexpr Move(Square from, Square to) :
        data((from << 6) + to) {}

    template<MoveType T>
    static constexpr Move make(Square from, Square to, PieceType pt = KNIGHT) {
        return Move(T + ((pt - KNIGHT) << 12) + (from << 6) + to);
    }

    constexpr Square from_sq() const {
        assert(is_ok());
        return Square((data >> 6) & 0x3F);
    }

    constexpr Square to_sq() const {
        assert(is_ok());
        return Square(data & 0x3F);
    }

    constexpr MoveType type_of() const { return MoveType(data & (3 << 14)); }

    constexpr PieceType promotion_type() const { return PieceType(((data >> 12) & 3) + KNIGHT); }

    constexpr bool is_ok() const { return none().data != data && null().data != data; }

    static constexpr Move null() { return Move(65); }
    static constexpr Move none() { return Move(0); }

    constexpr bool operator==(const Move& m) const { return data == m.data; }
    constexpr bool operator!=(const Move& m) const { return data != m.data; }

    constexpr explicit operator bool() const { return data != 0; }

    constexpr u16 raw() const { return data; }

    struct MoveHash {
        usize operator()(const Move& m) const { return make_key(m.data); }
    };

    static constexpr int FromSqShift = 6;
    static constexpr int ToSqShift   = 0;

   protected:
    u16 data;
};

}  // namespace Stockfish

#endif  // #ifndef TYPES_H_INCLUDED


// === sf_src/src/tune.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TUNE_H_INCLUDED
#define TUNE_H_INCLUDED

#include <memory>
#include <string>
#include <type_traits>  // IWYU pragma: keep
#include <utility>
#include <vector>


namespace Stockfish {

class OptionsMap;

using Range    = std::pair<int, int>;  // Option's min-max values
using RangeFun = Range(int);

// Default Range function, to calculate Option's min-max values
inline Range default_range(int v) { return v > 0 ? Range(0, 2 * v) : Range(2 * v, 0); }

struct SetRange {
    explicit SetRange(RangeFun f) :
        fun(f) {}
    SetRange(int min, int max) :
        fun(nullptr),
        range(min, max) {}
    Range operator()(int v) const { return fun ? fun(v) : range; }

    RangeFun* fun;
    Range     range;
};

#define SetDefaultRange SetRange(default_range)


// Tune class implements the 'magic' code that makes the setup of a fishtest tuning
// session as easy as it can be. Mainly you have just to remove const qualifiers
// from the variables you want to tune and flag them for tuning, so if you have:
//
//   const Value myValue[][2] = { { V(100), V(20) }, { V(7), V(78) } };
//
// If you have a my_post_update() function to run after values have been updated,
// and a my_range() function to set custom Option's min-max values, then you just
// remove the 'const' qualifiers and write somewhere below in the file:
//
//   TUNE(SetRange(my_range), myValue, my_post_update);
//
// You can also set the range directly, and restore the default at the end
//
//   TUNE(SetRange(-100, 100), myValue, SetDefaultRange);
//
// In case update function is slow and you have many parameters, you can add:
//
//   UPDATE_ON_LAST();
//
// And the values update, including post update function call, will be done only
// once, after the engine receives the last UCI option, that is the one defined
// and created as the last one, so the GUI should send the options in the same
// order in which have been defined.

class Tune {

    using PostUpdate = void();  // Post-update function

    Tune() { read_results(); }
    Tune(const Tune&)           = delete;
    void operator=(const Tune&) = delete;
    void read_results();

    static Tune& instance() {
        static Tune t;
        return t;
    }  // Singleton

    // Use polymorphism to accommodate Entry of different types in the same vector
    struct EntryBase {
        virtual ~EntryBase()       = default;
        virtual void init_option() = 0;
        virtual void read_option() = 0;
    };

    template<typename T>
    struct Entry: public EntryBase {

        static_assert(!std::is_const_v<T>, "Parameter cannot be const!");

        static_assert(std::is_same_v<T, int> || std::is_same_v<T, PostUpdate>,
                      "Parameter type not supported!");

        Entry(const std::string& n, T& v, const SetRange& r) :
            name(n),
            value(v),
            range(r) {}
        void operator=(const Entry&) = delete;  // Because 'value' is a reference
        void init_option() override;
        void read_option() override;

        std::string name;
        T&          value;
        SetRange    range;
    };

    // Our facility to fill the container, each Entry corresponds to a parameter
    // to tune. We use variadic templates to deal with an unspecified number of
    // entries, each one of a possible different type.
    static std::string next(std::string& names, bool pop = true);

    int add(const SetRange&, std::string&&) { return 0; }

    template<typename T, typename... Args>
    int add(const SetRange& range, std::string&& names, T& value, Args&&... args) {
        list.push_back(std::unique_ptr<EntryBase>(new Entry<T>(next(names), value, range)));
        return add(range, std::move(names), args...);
    }

    // Template specialization for arrays: recursively handle multi-dimensional arrays
    template<typename T, usize N, typename... Args>
    int add(const SetRange& range, std::string&& names, T (&value)[N], Args&&... args) {
        for (usize i = 0; i < N; i++)
            add(range, next(names, i == N - 1) + "[" + std::to_string(i) + "]", value[i]);
        return add(range, std::move(names), args...);
    }

    // Template specialization for SetRange
    template<typename... Args>
    int add(const SetRange&, std::string&& names, SetRange& value, Args&&... args) {
        return add(value, (next(names), std::move(names)), args...);
    }

    static void make_option(OptionsMap* options, const std::string& n, int v, const SetRange& r);

    std::vector<std::unique_ptr<EntryBase>> list;

   public:
    template<typename... Args>
    static int add(const std::string& names, Args&&... args) {
        return instance().add(SetDefaultRange, names.substr(1, names.size() - 2),
                              args...);  // Remove trailing parenthesis
    }
    static void init(OptionsMap& o) {
        options = &o;
        for (auto& e : instance().list)
            e->init_option();
        read_options();
    }  // Deferred, due to UCIEngine::Options access
    static void read_options() {
        for (auto& e : instance().list)
            e->read_option();
    }

    static bool        update_on_last;
    static OptionsMap* options;
};

template<typename... Args>
constexpr void tune_check_args(Args&&...) {
    static_assert((!std::is_fundamental_v<Args> && ...), "TUNE macro arguments wrong");
}

// Some macro magic :-) we define a dummy int variable that the compiler initializes calling Tune::add()
#define STRINGIFY(x) #x
#define UNIQUE2(x, y) x##y
#define UNIQUE(x, y) UNIQUE2(x, y)  // Two indirection levels to expand __LINE__
#define TUNE(...) \
    int UNIQUE(p, __LINE__) = []() -> int { \
        tune_check_args(__VA_ARGS__); \
        return Tune::add(STRINGIFY((__VA_ARGS__)), __VA_ARGS__); \
    }();

#define UPDATE_ON_LAST() bool UNIQUE(p, __LINE__) = Tune::update_on_last = true

}  // namespace Stockfish

#endif  // #ifndef TUNE_H_INCLUDED

// === sf_src/src/bitboard.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef BITBOARD_H_INCLUDED
#define BITBOARD_H_INCLUDED

#include <algorithm>
#include <array>
#include <cassert>
#include <string>
#include <type_traits>


namespace Stockfish {

namespace Bitboards {

std::string pretty(Bitboard b);

}  // namespace Stockfish::Bitboards

#ifdef USE_AVX512
// clang-format off
inline const __m512i AllSquares = _mm512_set_epi8(
    63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41,
    40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18,
    17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
// clang-format on
#endif

constexpr Bitboard FileABB = 0x0101010101010101ULL;
constexpr Bitboard FileBBB = FileABB << 1;
constexpr Bitboard FileCBB = FileABB << 2;
constexpr Bitboard FileDBB = FileABB << 3;
constexpr Bitboard FileEBB = FileABB << 4;
constexpr Bitboard FileFBB = FileABB << 5;
constexpr Bitboard FileGBB = FileABB << 6;
constexpr Bitboard FileHBB = FileABB << 7;

constexpr Bitboard Rank1BB = 0xFF;
constexpr Bitboard Rank2BB = Rank1BB << (8 * 1);
constexpr Bitboard Rank3BB = Rank1BB << (8 * 2);
constexpr Bitboard Rank4BB = Rank1BB << (8 * 3);
constexpr Bitboard Rank5BB = Rank1BB << (8 * 4);
constexpr Bitboard Rank6BB = Rank1BB << (8 * 5);
constexpr Bitboard Rank7BB = Rank1BB << (8 * 6);
constexpr Bitboard Rank8BB = Rank1BB << (8 * 7);

constexpr Bitboard square_bb(Square s) {
    assert(is_ok(s));
    return 1ULL << s;
}


// Overloads of bitwise operators between a Bitboard and a Square for testing
// whether a given bit is set in a bitboard, and for setting and clearing bits.

constexpr Bitboard  operator&(Bitboard b, Square s) { return b & square_bb(s); }
constexpr Bitboard  operator|(Bitboard b, Square s) { return b | square_bb(s); }
constexpr Bitboard  operator^(Bitboard b, Square s) { return b ^ square_bb(s); }
constexpr Bitboard& operator|=(Bitboard& b, Square s) { return b |= square_bb(s); }
constexpr Bitboard& operator^=(Bitboard& b, Square s) { return b ^= square_bb(s); }

constexpr Bitboard operator&(Square s, Bitboard b) { return b & s; }
constexpr Bitboard operator|(Square s, Bitboard b) { return b | s; }
constexpr Bitboard operator^(Square s, Bitboard b) { return b ^ s; }

constexpr Bitboard operator|(Square s1, Square s2) { return square_bb(s1) | s2; }

constexpr bool more_than_one(Bitboard b) { return b & (b - 1); }


// rank_bb() and file_bb() return a bitboard representing all the squares on
// the given file or rank.

constexpr Bitboard rank_bb(Rank r) { return Rank1BB << (8 * r); }

constexpr Bitboard rank_bb(Square s) { return rank_bb(rank_of(s)); }

constexpr Bitboard file_bb(File f) { return FileABB << f; }

constexpr Bitboard file_bb(Square s) { return file_bb(file_of(s)); }

// Moves a bitboard one or two steps as specified by dir
inline constexpr Bitboard shift(Bitboard b, Direction dir) {
    return dir == NORTH         ? b << 8
         : dir == SOUTH         ? b >> 8
         : dir == NORTH + NORTH ? b << 16
         : dir == SOUTH + SOUTH ? b >> 16
         : dir == EAST          ? (b & ~FileHBB) << 1
         : dir == WEST          ? (b & ~FileABB) >> 1
         : dir == NORTH_EAST    ? (b & ~FileHBB) << 9
         : dir == NORTH_WEST    ? (b & ~FileABB) << 7
         : dir == SOUTH_EAST    ? (b & ~FileHBB) >> 7
         : dir == SOUTH_WEST    ? (b & ~FileABB) >> 9
                                : 0;
}


// Returns the squares attacked by pawns of the given color
// from the squares in the given bitboard.
template<Color C>
constexpr Bitboard pawn_attacks_bb(Bitboard b) {
    return C == WHITE ? shift(b, NORTH_WEST) | shift(b, NORTH_EAST)
                      : shift(b, SOUTH_WEST) | shift(b, SOUTH_EAST);
}

constexpr Bitboard pawn_single_push_bb(Color c, Bitboard b) {
    return shift(b, c == WHITE ? NORTH : SOUTH);
}

inline constexpr auto PawnPairBB = []() {
    std::array<Bitboard, SQUARE_NB> result{};
    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        Bitboard file  = file_bb(s);
        Bitboard files = file | shift(file, EAST) | shift(file, WEST);
        result[s]      = files & ~(Rank1BB | Rank8BB) & ~square_bb(s);
    }
    return result;
}();

// Returns the squares that can host a pawn forming a "pawn pair" with a pawn
// on s: own file plus adjacent files, restricted to ranks 2-7, excluding s.
// The geometry is color-independent.
constexpr Bitboard pawn_pair_bb(Square s) { return PawnPairBB[s]; }

constexpr int edge_distance(File f) { return std::min(f, File(FILE_H - f)); }

template<typename T>
constexpr int constexpr_popcount(T v) {
    static_assert(std::is_integral_v<T>, "constexpr_popcount is undefined for non-integral types");

    if constexpr (sizeof(T) <= 8)
    {
        u64 b = static_cast<std::make_unsigned_t<T>>(v);

        b = b - ((b >> 1) & 0x5555555555555555ULL);
        b = (b & 0x3333333333333333ULL) + ((b >> 2) & 0x3333333333333333ULL);
        b = (b + (b >> 4)) & 0x0F0F0F0F0F0F0F0FULL;

        return static_cast<int>((b * 0x0101010101010101ULL) >> 56);
    }
    else
    {
        int result = 0;

        for (; v; v >>= static_cast<T>(1))
            if (v & static_cast<T>(1))
                ++result;

        return result;
    }
}

// Counts the number of non-zero bits in a bitboard.
inline int popcount(Bitboard b) {

#ifdef _MSC_VER

    return int(_mm_popcnt_u64(b));

#else  // Assumed gcc or compatible compiler

    return __builtin_popcountll(b);

#endif
}

// Returns the least significant bit in a non-zero bitboard.
inline Square lsb(Bitboard b) {
    assert(b);

#if defined(__GNUC__)  // GCC, Clang, ICX

    return Square(__builtin_ctzll(b));

#elif defined(_MSC_VER)
    #ifdef _WIN64  // MSVC, WIN64

    unsigned long idx;
    _BitScanForward64(&idx, b);
    return Square(idx);

    #else  // MSVC, WIN32
    unsigned long idx;

    if (b & 0xffffffff)
    {
        _BitScanForward(&idx, i32(b));
        return Square(idx);
    }

    _BitScanForward(&idx, i32(b >> 32));
    return Square(idx + 32);
    #endif
#else  // Compiler is neither GCC nor MSVC compatible
    #error "Compiler not supported."
#endif
}

// Returns the most significant bit in a non-zero bitboard.
inline Square msb(Bitboard b) {
    assert(b);

#if defined(__GNUC__)  // GCC, Clang, ICX

    return Square(63 ^ __builtin_clzll(b));

#elif defined(_MSC_VER)
    #ifdef _WIN64  // MSVC, WIN64

    unsigned long idx;
    _BitScanReverse64(&idx, b);
    return Square(idx);

    #else  // MSVC, WIN32

    unsigned long idx;

    if (b >> 32)
    {
        _BitScanReverse(&idx, i32(b >> 32));
        return Square(idx + 32);
    }

    _BitScanReverse(&idx, i32(b));
    return Square(idx);
    #endif
#else  // Compiler is neither GCC nor MSVC compatible
    #error "Compiler not supported."
#endif
}

// Returns the bitboard of the least significant
// square of a non-zero bitboard. It is equivalent to square_bb(lsb(bb)).
constexpr Bitboard least_significant_square_bb(Bitboard b) {
    assert(b);
    return b & -b;
}

// Finds and clears the least significant bit in a non-zero bitboard.
inline Square pop_lsb(Bitboard& b) {
    assert(b);
    const Square s = lsb(b);
    b &= b - 1;
    return s;
}

}  // namespace Stockfish

#endif  // #ifndef BITBOARD_H_INCLUDED

#ifdef __aarch64__
    #include <arm_acle.h>
    #define USE_HYPERBOLA_QUINT
#elif defined(__loongarch__) && __loongarch_grlen == 64
    #define USE_HYPERBOLA_QUINT
#elif defined(USE_AVX2)
    #include <immintrin.h>
    #define USE_DUAL_HYPERBOLA_QUINT
#endif

namespace Stockfish::Attacks {

void init();

#ifdef USE_HYPERBOLA_QUINT

inline Bitboard reverse_bb(Bitboard bb) {
    #if __has_builtin(__builtin_bitreverse64)
    return __builtin_bitreverse64(bb);
    #else
        #ifdef __aarch64__
            #if defined(__GNUC__) && !defined(__clang__) \
              && (__GNUC__ < 12 || (__GNUC__ == 12 && __GNUC_MINOR__ < 2))
    // no rbit in arm_acle.h
    Bitboard out;
    asm("rbit %0, %1" : "=r"(out) : "r"(bb));
    return out;
            #else
    return __rbitll(bb);
            #endif
        #else  // loongarch
    Bitboard out;
    asm("bitrev.d %0, %1" : "=r"(out) : "r"(bb));
    return out;
        #endif
    #endif
}

// Hyperbola quintessence implementation for ARM, thanks to the availability of an
// efficient bit reversal instruction.
// See https://www.chessprogramming.org/Hyperbola_Quintessence
struct Magic {
    // For rooks: file attacks, rank attacks. For bishops: diagonal/antidiagonal
    Bitboard mask1, mask2;

    Bitboard hyperbola(Square s, Bitboard occupied, Bitboard mask) const {
        Bitboard o   = occupied & mask;
        Bitboard fwd = o - square_bb(s);
        Bitboard rev = reverse_bb(o) - square_bb(Square(63 - int(s)));
        return (fwd ^ reverse_bb(rev)) & mask;
    }

    Bitboard attacks_bb(Square s, Bitboard occupied) const {
        return hyperbola(s, occupied, mask1) | hyperbola(s, occupied, mask2);
    }
};

const Magic& magic(Square s, PieceType pt);

#elif defined(USE_DUAL_HYPERBOLA_QUINT)

struct alignas(32) DualMagic {
    // file, diagonal, antidiagonal, unused
    Bitboard maskFile, maskDiag, maskAntidiag, maskNone;
    // Precomputed 2 * square_bb(sq), 2 * reverse(square_bb(sq))
    Bitboard r, rr;

    const u8* RESTRICT rankAttacksLookup;
    // 8 * rank_of(sq)
    int shift;

    // We always compute [bishop, rook] attacks at once, then rely on
    // compiler's DCE and CSE to eliminate unneeded re-computations or extractions.
    //
    // When using hyperbola quintessence, file, diagonal and antidiagonal attacks
    // can use a byte reversal rather than a full bit reversal (because all squares
    // reside in different bytes). Rank attacks cannot. Thus, for rank attacks
    // only, we use a compact lookup table indexed by the 6 inner bits of the rank's
    // occupancy (the edge squares never affect the attack set).
    std::pair<Bitboard, Bitboard> both_attacks_bb(Bitboard occupied) const {
        // Byteswap within 128-bit elements
        const auto bswap = [](__m256i v) {
            return _mm256_shuffle_epi8(v, _mm256_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                                          13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                                                          10, 11, 12, 13, 14, 15));
        };

        // Each lane contains a mask and we follow the same HQ algorithm as
        // given above in the ARM64 code path
        const __m256i mask = _mm256_load_si256(reinterpret_cast<const __m256i*>(this));
        const __m256i rs   = _mm256_set1_epi64x(r);
        const __m256i rrs  = _mm256_set1_epi64x(rr);

        __m256i o      = _mm256_and_si256(mask, _mm256_set1_epi64x(occupied));
        __m256i fwd    = _mm256_sub_epi64(o, rs);
        __m256i rev    = bswap(_mm256_sub_epi64(bswap(o), rrs));
        __m256i result = _mm256_and_si256(_mm256_xor_si256(fwd, rev), mask);

        // Lane 0: rook attacks (file only); lanes 1 and 2: bishop attacks
        const __m256i rookBishop =
          _mm256_or_si256(result, _mm256_permute4x64_epi64(result, _MM_SHUFFLE(3, 1, 2, 0)));

        Bitboard rowOccupancy = rankAttacksLookup[(occupied >> (shift + 1)) & 0x3f];
        Bitboard rankAttacks  = rowOccupancy << shift;

        // [bishop, rook]
        return {_mm_extract_epi64(_mm256_castsi256_si128(rookBishop), 1),
                _mm_cvtsi128_si64(_mm256_castsi256_si128(result)) + rankAttacks};
    }
};

extern const std::array<DualMagic, SQUARE_NB> DualMagics;
inline const DualMagic&                       dual_magic(Square s) { return DualMagics[s]; }

#else
// Magic holds all magic bitboards relevant data for a single square
struct Magic {
    Bitboard  mask;
    Bitboard* attacks;
    Bitboard  magic;
    unsigned  shift;

    // Compute the attack's index using the 'magic bitboards' approach
    unsigned index(Bitboard occupied) const {
        if (Is64Bit)
            return unsigned(((occupied & mask) * magic) >> shift);

        unsigned lo = unsigned(occupied) & unsigned(mask);
        unsigned hi = unsigned(occupied >> 32) & unsigned(mask >> 32);
        return (lo * unsigned(magic) ^ hi * unsigned(magic >> 32)) >> shift;
    }

    Bitboard attacks_bb([[maybe_unused]] Square s, Bitboard occupied) const {
        return attacks[index(occupied)];
    }
};

const Magic& magic(Square s, PieceType pt);

#endif

extern Bitboard LineBB[SQUARE_NB][SQUARE_NB];
extern Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
extern Bitboard inverse_RayPassBB[SQUARE_NB][SQUARE_NB];
extern Bitboard DiagBB[SQUARE_NB];
extern Bitboard AntiDiagBB[SQUARE_NB];

// horizontal/vertical/diagonal line passing through s1 and s2
inline Bitboard line_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return LineBB[s1][s2];
}

// horizontal/vertical/diagonal line segment from s1 to s2,
// excluding s1 but including s2
inline Bitboard between_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return BetweenBB[s1][s2];
}

// ray going from s1 to s2 and past, excluding s1
inline Bitboard ray_pass_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return inverse_RayPassBB[s2][s1];
}

inline Bitboard diag_bb(Square s) {
    assert(is_ok(s));
    return DiagBB[s];
}

inline Bitboard antidiag_bb(Square s) {
    assert(is_ok(s));
    return AntiDiagBB[s];
}

// Returns the bitboard of target square for the given step
// from the given square. If the step is off the board, returns empty bitboard.
constexpr Bitboard safe_destination(Square s, int step) {
    constexpr auto abs = [](int v) { return v < 0 ? -v : v; };
    Square         to  = Square(s + step);
    return is_ok(to) && abs(file_of(s) - file_of(to)) <= 2 ? square_bb(to) : Bitboard(0);
}

constexpr Bitboard sliding_attack(PieceType pt, Square sq, Bitboard occupied) {
    Bitboard            attacks             = 0;
    constexpr Direction RookDirections[4]   = {NORTH, SOUTH, EAST, WEST};
    constexpr Direction BishopDirections[4] = {NORTH_EAST, SOUTH_EAST, SOUTH_WEST, NORTH_WEST};

    for (Direction d : (pt == ROOK ? RookDirections : BishopDirections))
        for (Square s = sq; Bitboard dest = safe_destination(s, d); s += d)
        {
            attacks |= dest;
            if (occupied & dest)
                break;
        }

    return attacks;
}

constexpr Bitboard knight_attack(Square sq) {
    Bitboard b = {};
    for (int step : {-17, -15, -10, -6, 6, 10, 15, 17})
        b |= safe_destination(sq, step);
    return b;
}

constexpr Bitboard king_attack(Square sq) {
    Bitboard b = {};
    for (int step : {-9, -8, -7, -1, 1, 7, 8, 9})
        b |= safe_destination(sq, step);
    return b;
}

constexpr Bitboard pseudo_attacks(PieceType pt, Square sq) {
    switch (pt)
    {
    case PieceType::ROOK :
    case PieceType::BISHOP :
        return sliding_attack(pt, sq, 0);
    case PieceType::QUEEN :
        return sliding_attack(PieceType::ROOK, sq, 0) | sliding_attack(PieceType::BISHOP, sq, 0);
    case PieceType::KNIGHT :
        return knight_attack(sq);
    case PieceType::KING :
        return king_attack(sq);
    default :
        assert(false);
        return 0;
    }
}

inline constexpr auto PseudoAttacks = []() constexpr {
    std::array<std::array<Bitboard, SQUARE_NB>, PIECE_TYPE_NB> attacks{};

    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
    {
        attacks[WHITE][s1] = pawn_attacks_bb<WHITE>(square_bb(s1));
        attacks[BLACK][s1] = pawn_attacks_bb<BLACK>(square_bb(s1));

        attacks[KING][s1]   = pseudo_attacks(KING, s1);
        attacks[KNIGHT][s1] = pseudo_attacks(KNIGHT, s1);
        attacks[QUEEN][s1] = attacks[BISHOP][s1] = pseudo_attacks(BISHOP, s1);
        attacks[QUEEN][s1] |= attacks[ROOK][s1]  = pseudo_attacks(ROOK, s1);
    }

    return attacks;
}();

// Returns the pseudo attacks of the given piece type
// assuming an empty board.
sf_always_inline Bitboard attacks_bb(PieceType pt, Square s, Color c = COLOR_NB) {

    assert((pt != PAWN || c < COLOR_NB) && is_ok(s));
    return pt == PAWN ? PseudoAttacks[c][s] : PseudoAttacks[pt][s];
}

// Returns the attacks by the given piece
// assuming the board is occupied according to the passed Bitboard.
// Sliding piece attacks do not continue past an occupied square.
sf_always_inline Bitboard attacks_bb(PieceType pt, Square s, Bitboard occupied) {

    assert(pt != PAWN && is_ok(s));

#ifdef USE_DUAL_HYPERBOLA_QUINT
    [[maybe_unused]] const auto [bishop, rook] = dual_magic(s).both_attacks_bb(occupied);

    switch (pt)
    {
    case BISHOP :
        return bishop;
    case ROOK :
        return rook;
    case QUEEN :
        return bishop | rook;
    default :
        return PseudoAttacks[pt][s];
    }
#else
    switch (pt)
    {
    case BISHOP :
    case ROOK :
        return magic(s, pt).attacks_bb(s, occupied);
    case QUEEN :
        return magic(s, BISHOP).attacks_bb(s, occupied) | magic(s, ROOK).attacks_bb(s, occupied);
    default :
        return PseudoAttacks[pt][s];
    }
#endif
}

inline std::pair<Bitboard, Bitboard> both_attacks_bb(Square s, Bitboard occupied) {
#ifdef USE_DUAL_HYPERBOLA_QUINT
    return dual_magic(s).both_attacks_bb(occupied);
#else
    return {attacks_bb(BISHOP, s, occupied), attacks_bb(ROOK, s, occupied)};
#endif
}

}  // namespace Stockfish::Attacks

#endif  // #ifndef ATTACKS_H_INCLUDED

#include <array>


namespace Stockfish::Attacks {

Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard inverse_RayPassBB[SQUARE_NB][SQUARE_NB];
Bitboard DiagBB[SQUARE_NB];
Bitboard AntiDiagBB[SQUARE_NB];

namespace {

#ifndef USE_DUAL_HYPERBOLA_QUINT
alignas(64) Magic Magics[SQUARE_NB][2];
#endif

}

[[maybe_unused]] static constexpr Bitboard line_mask(Square sq, Direction d1, Direction d2) {
    Bitboard mask = 0;
    for (Direction d : {d1, d2})
    {
        Square s = sq;
        while (Bitboard dest = safe_destination(s, d))
        {
            mask |= dest;
            s += d;
        }
    }
    return mask;
}

#ifdef USE_HYPERBOLA_QUINT
static void init_magics(Magic magics[][2]) {
    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        Magic& rook = magics[s][ROOK - BISHOP];
        rook.mask1  = line_mask(s, NORTH, SOUTH);
        rook.mask2  = line_mask(s, EAST, WEST);

        Magic& bishop = magics[s][BISHOP - BISHOP];
        bishop.mask1  = line_mask(s, NORTH_EAST, SOUTH_WEST);
        bishop.mask2  = line_mask(s, NORTH_WEST, SOUTH_EAST);
    }
}

#elif defined(USE_DUAL_HYPERBOLA_QUINT)

// Sliding attacks within a rank, indexed by the slider's file and the
// 6 inner bits of the rank occupancy (edge squares never affect the
// attack set), yielding the 8-bit attack set on that rank
alignas(64) constexpr auto RankAttacks = []() {
    std::array<std::array<u8, 64>, FILE_NB> table{};
    for (int file = 0; file < 8; ++file)
        for (int occ6 = 0; occ6 < 64; ++occ6)
            table[file][occ6] = u8(sliding_attack(ROOK, Square(file), occ6 << 1));
    return table;
}();

static constexpr auto make_dual_magics() {
    std::array<DualMagic, SQUARE_NB> magics{};
    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        DualMagic& m        = magics[s];
        m.maskFile          = line_mask(s, NORTH, SOUTH);
        m.maskDiag          = line_mask(s, NORTH_EAST, SOUTH_WEST);
        m.maskNone          = 0;
        m.maskAntidiag      = line_mask(s, NORTH_WEST, SOUTH_EAST);
        m.r                 = square_bb(s) * 2;
        m.rr                = square_bb(Square(63 - int(s))) * 2;
        m.rankAttacksLookup = RankAttacks[int(file_of(s))].data();
        m.shift             = 8 * int(rank_of(s));
    }
    return magics;
}

alignas(64) extern constexpr std::array<DualMagic, SQUARE_NB> DualMagics = make_dual_magics();

#else

namespace {
void init_magics(PieceType pt, Bitboard table[], Magic magics[][2]) {

    int seeds[][RANK_NB] = {{8977, 44560, 54343, 38998, 5731, 95205, 104912, 17020},
                            {728, 10316, 55013, 32803, 12281, 15100, 16645, 255}};

    Bitboard occupancy[4096];
    int      epoch[4096] = {}, cnt = 0;
    Bitboard reference[4096] = {};
    int      size            = 0;

    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        Bitboard edges = ((Rank1BB | Rank8BB) & ~rank_bb(s)) | ((FileABB | FileHBB) & ~file_bb(s));

        Magic&   m       = magics[s][pt - BISHOP];
        Bitboard attacks = sliding_attack(pt, s, 0);
        m.mask           = attacks & ~edges;
        m.shift          = (Is64Bit ? 64 : 32) - popcount(m.mask);
        m.attacks        = s == SQ_A1 ? table : magics[s - 1][pt - BISHOP].attacks + size;
        size             = 0;

        Bitboard b = 0;
        do
        {
            occupancy[size] = b;
            reference[size] = sliding_attack(pt, s, b);

            size++;
            b = (b - m.mask) & m.mask;
        } while (b);

        PRNG rng(seeds[Is64Bit][rank_of(s)]);

        for (int i = 0; i < size;)
        {
            for (m.magic = 0; popcount((m.magic * m.mask) >> 56) < 6;)
                m.magic = rng.sparse_rand<Bitboard>();

            for (++cnt, i = 0; i < size; ++i)
            {
                unsigned idx = m.index(occupancy[i]);

                if (epoch[idx] < cnt)
                {
                    epoch[idx]     = cnt;
                    m.attacks[idx] = reference[i];
                }
                else if (m.attacks[idx] != reference[i])
                    break;
            }
        }
    }
}

    #if !defined(USE_DUAL_HYPERBOLA_QUINT) && !defined(USE_HYPERBOLA_QUINT)
static std::array<Bitboard, 0x19000> RookTable;
static std::array<Bitboard, 0x1480>  BishopTable;
    #endif
}

#endif

void init() {

#ifdef USE_HYPERBOLA_QUINT
    init_magics(Magics);
#elif !defined(USE_DUAL_HYPERBOLA_QUINT)
    init_magics(ROOK, RookTable.data(), Magics);
    init_magics(BISHOP, BishopTable.data(), Magics);
#endif

    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
    {
        for (PieceType pt : {BISHOP, ROOK})
            for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2)
            {
                if (PseudoAttacks[pt][s1] & s2)
                {
                    LineBB[s1][s2] = (attacks_bb(pt, s1, 0) & attacks_bb(pt, s2, 0)) | s1 | s2;
                    BetweenBB[s1][s2] =
                      (attacks_bb(pt, s1, square_bb(s2)) & attacks_bb(pt, s2, square_bb(s1)));
                    inverse_RayPassBB[s2][s1] =
                      attacks_bb(pt, s1, 0) & (attacks_bb(pt, s2, square_bb(s1)) | s2);
                }
                BetweenBB[s1][s2] |= s2;
            }

        DiagBB[s1]     = line_mask(s1, NORTH_EAST, SOUTH_WEST) | s1;
        AntiDiagBB[s1] = line_mask(s1, NORTH_WEST, SOUTH_EAST) | s1;
    }
}

#ifndef USE_DUAL_HYPERBOLA_QUINT
const Magic& magic(Square s, PieceType pt) {
    assert((pt == BISHOP || pt == ROOK) && is_ok(s));
    return Magics[s][pt - BISHOP];
}
#endif

}  // namespace Stockfish::Attacks

// === sf_src/src/bitboard.cpp ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


namespace Stockfish {

// Returns an ASCII representation of a bitboard suitable
// to be printed to standard output. Useful for debugging.
std::string Bitboards::pretty(Bitboard b) {

    std::string s = "+---+---+---+---+---+---+---+---+\n";

    for (Rank r = RANK_8;; --r)
    {
        for (File f = FILE_A; f <= FILE_H; ++f)
            s += b & make_square(f, r) ? "| X " : "|   ";

        s += "| " + std::to_string(1 + r) + "\n+---+---+---+---+---+---+---+---+\n";

        if (r == RANK_1)
            break;
    }
    s += "  a   b   c   d   e   f   g   h\n";

    return s;
}

}  // namespace Stockfish

// === sf_src/src/evaluate.cpp ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


// === sf_src/src/evaluate.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef EVALUATE_H_INCLUDED
#define EVALUATE_H_INCLUDED

#include <string>


namespace Stockfish {

class Position;

namespace Eval {

// The default net name MUST follow the format nn-[SHA256 first 12 digits].nnue
// for the build process (profile-build and fishtest) to work. Do not change the
// name of the macro or the location where this macro is defined, as it is used
// in the Makefile/Fishtest.
#define EvalFileDefaultName "nn-bddd9d57eb87.nnue"

namespace NNUE {
class Network;
struct AccumulatorCaches;
class AccumulatorStack;
}

std::string trace(Position& pos, const Eval::NNUE::Network& network);

Value evaluate(const NNUE::Network&           network,
               const Position&                pos,
               Eval::NNUE::AccumulatorStack&  accumulators,
               Eval::NNUE::AccumulatorCaches& caches,
               int                            optimism);
}  // namespace Eval

}  // namespace Stockfish

#endif  // #ifndef EVALUATE_H_INCLUDED

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>


// === sf_src/src/nnue/network.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <filesystem>


// === sf_src/src/nnue/nnue_architecture.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Input features and network structure used in NNUE evaluation function

#ifndef NNUE_ARCHITECTURE_H_INCLUDED
#define NNUE_ARCHITECTURE_H_INCLUDED

#include <cstdint>
#include <iosfwd>


// === sf_src/src/nnue/features/k32h3.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//Definition of input features K32H3 of NNUE evaluation function

#ifndef NNUE_FEATURES_K32H3_H_INCLUDED
#define NNUE_FEATURES_K32H3_H_INCLUDED

#include <array>


// === sf_src/src/nnue/nnue_common.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Constants used in NNUE evaluation function

#ifndef NNUE_COMMON_H_INCLUDED
#define NNUE_COMMON_H_INCLUDED

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>


#if defined(USE_AVX2)
    #include <immintrin.h>

#elif defined(USE_SSE41)
    #include <smmintrin.h>

#elif defined(USE_SSSE3)
    #include <tmmintrin.h>

#elif defined(USE_SSE2)
    #include <emmintrin.h>

#elif defined(USE_LASX)
    #include <lasxintrin.h>
    #include <lsxintrin.h>

#elif defined(USE_LSX)
    #include <lsxintrin.h>

#elif defined(USE_NEON)
    #include <arm_neon.h>
#endif

namespace Stockfish::Eval::NNUE {

using BiasType         = i16;
using ThreatWeightType = i8;
using WeightType       = i8;
using IndexType        = u32;

// Version of the evaluation file
constexpr u32 Version = 0x6A448AFAu;

// Constant used in evaluation value calculation
constexpr int OutputScale     = 16;
constexpr int WeightScaleBits = 6;
constexpr int FtMaxVal        = 255;
constexpr int HiddenOneVal    = 128;

// Size of cache line (in bytes)
constexpr usize CacheLineSize = 64;

// clang-format off
// Orient a square according to perspective
// Reflect kings on files a..d, so that it lands on e..h
alignas(CacheLineSize) inline constexpr u8 OrientTBL[SQUARE_NB] = {
    SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
    SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
    SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
    SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
    SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
    SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
    SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
    SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
};
// clang-format on


constexpr usize MaxSimdWidth = 32;

// Type of input feature after conversion
using TransformedFeatureType = u8;

// Round n up to be a multiple of base
template<typename IntType>
constexpr IntType ceil_to_multiple(IntType n, IntType base) {
    return (n + base - 1) / base * base;
}


// Utility to read an integer (signed or unsigned, any size)
// from a stream in little-endian order. We swap the byte order after the read if
// necessary to return a result with the byte ordering of the compiling machine.
template<typename IntType>
inline IntType read_little_endian(std::istream& stream) {
    IntType result;

    if (IsLittleEndian)
        stream.read(reinterpret_cast<char*>(&result), sizeof(IntType));
    else
    {
        u8                            u[sizeof(IntType)];
        std::make_unsigned_t<IntType> v = 0;

        stream.read(reinterpret_cast<char*>(u), sizeof(IntType));
        for (usize i = 0; i < sizeof(IntType); ++i)
            v = (v << 8) | u[sizeof(IntType) - i - 1];

        std::memcpy(&result, &v, sizeof(IntType));
    }

    return result;
}


// Utility to write an integer (signed or unsigned, any size)
// to a stream in little-endian order. We swap the byte order before the write if
// necessary to always write in little-endian order, independently of the byte
// ordering of the compiling machine.
template<typename IntType>
inline void write_little_endian(std::ostream& stream, IntType value) {

    if (IsLittleEndian)
        stream.write(reinterpret_cast<const char*>(&value), sizeof(IntType));
    else
    {
        u8                            u[sizeof(IntType)];
        std::make_unsigned_t<IntType> v = value;

        usize i = 0;
        // if constexpr to silence the warning about shift by 8
        if constexpr (sizeof(IntType) > 1)
        {
            for (; i + 1 < sizeof(IntType); ++i)
            {
                u[i] = u8(v);
                v >>= 8;
            }
        }
        u[i] = u8(v);

        stream.write(reinterpret_cast<char*>(u), sizeof(IntType));
    }
}


// Read integers in bulk from a little-endian stream.
// This reads N integers from stream s and puts them in array out.
template<typename IntType>
inline void read_little_endian(std::istream& stream, IntType* out, usize count) {
    if (IsLittleEndian)
        stream.read(reinterpret_cast<char*>(out), sizeof(IntType) * count);
    else
        for (usize i = 0; i < count; ++i)
            out[i] = read_little_endian<IntType>(stream);
}


// Write integers in bulk to a little-endian stream.
// This takes N integers from array values and writes them on stream s.
template<typename IntType>
inline void write_little_endian(std::ostream& stream, const IntType* values, usize count) {
    if (IsLittleEndian)
        stream.write(reinterpret_cast<const char*>(values), sizeof(IntType) * count);
    else
        for (usize i = 0; i < count; ++i)
            write_little_endian<IntType>(stream, values[i]);
}


}  // namespace Stockfish::Eval::NNUE

#endif  // #ifndef NNUE_COMMON_H_INCLUDED

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE::Features {

// Feature K32H3: Combination of the position of own king (32 buckets), a piece-square block (3 states)
// and piece-square positions. Position mirrored such that king is always on e..h files.
class K32H3 {

    // Unique number for each piece type on each square
    enum {
        PS_NONE     = 0,
        PS_W_PAWN   = 0,
        PS_B_PAWN   = 1 * SQUARE_NB,
        PS_W_KNIGHT = 2 * SQUARE_NB,
        PS_B_KNIGHT = 3 * SQUARE_NB,
        PS_W_BISHOP = 4 * SQUARE_NB,
        PS_B_BISHOP = 5 * SQUARE_NB,
        PS_W_ROOK   = 6 * SQUARE_NB,
        PS_B_ROOK   = 7 * SQUARE_NB,
        PS_W_QUEEN  = 8 * SQUARE_NB,
        PS_B_QUEEN  = 9 * SQUARE_NB,
        PS_KING     = 10 * SQUARE_NB,
        PS_NB       = 11 * SQUARE_NB
    };

    alignas(64) static constexpr u16 PieceSquareIndex[COLOR_NB][PIECE_NB] = {
      // Convention: W - us, B - them
      // Viewed from other side, W and B are reversed
      {PS_NONE, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, PS_KING, PS_NONE,
       PS_NONE, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, PS_KING, PS_NONE},
      {PS_NONE, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, PS_KING, PS_NONE,
       PS_NONE, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, PS_KING, PS_NONE}};

   public:
    // Hash value embedded in the evaluation file
    static constexpr u32 HashValue = 0xab005cefu;

    // Piece-square block of a perspective, in order of increasing heavy material:
    //   0: no rook or queen on the board,
    //   1: some rook or queen on the board, but the opponent has no queen,
    //   2: the opponent has a queen.
    static constexpr int NumBlocks = 3;

    // Block of each perspective
    using BlockState = std::array<u8, COLOR_NB>;

    static int        block(const Position& pos, Color perspective);
    static BlockState block_state(const Position& pos);

    // Number of features per block (22,528 piece-squares)
    static constexpr IndexType BlockDimensions =
      static_cast<IndexType>(SQUARE_NB / 2) * static_cast<IndexType>(PS_NB);

    static constexpr IndexType block_offset(int block) {
        return IndexType(block) * BlockDimensions;
    }

    // Number of feature dimensions (67,584 piece-squares)
    static constexpr IndexType Dimensions = NumBlocks * BlockDimensions;

#define B(v) (v * PS_NB)
    // clang-format off
    static constexpr IndexType KingBuckets[SQUARE_NB] = {
        B(28), B(29), B(30), B(31), B(31), B(30), B(29), B(28),
        B(24), B(25), B(26), B(27), B(27), B(26), B(25), B(24),
        B(20), B(21), B(22), B(23), B(23), B(22), B(21), B(20),
        B(16), B(17), B(18), B(19), B(19), B(18), B(17), B(16),
        B(12), B(13), B(14), B(15), B(15), B(14), B(13), B(12),
        B( 8), B( 9), B(10), B(11), B(11), B(10), B( 9), B( 8),
        B( 4), B( 5), B( 6), B( 7), B( 7), B( 6), B( 5), B( 4),
        B( 0), B( 1), B( 2), B( 3), B( 3), B( 2), B( 1), B( 0),
    };
    // clang-format on
#undef B

    // Maximum number of simultaneously active features.
    static constexpr IndexType MaxActiveDimensions = 32;
    using IndexList                                = ValueList<u16, MaxActiveDimensions>;
    using DiffType                                 = DirtyPiece;

#if defined(USE_AVX512ICL)
    // Compute all changed feature indices and write them to the given lists
    static void write_indices(const std::array<Piece, SQUARE_NB>& oldPieces,
                              const std::array<Piece, SQUARE_NB>& newPieces,
                              Bitboard                            removedBB,
                              Bitboard                            addedBB,
                              Color                               perspective,
                              Square                              ksq,
                              IndexList&                          removed,
                              IndexList&                          added);
#endif

    // Index of a feature within its block for a given king position and another piece
    // on some square
    static IndexType make_index(Color perspective, Square s, Piece pc, Square ksq);

    // Get a list of indices for recently changed features
    static void append_changed_indices(
      Color perspective, Square ksq, const DiffType& diff, IndexList& removed, IndexList& added);

    // Returns whether the change stored in this DirtyPiece means that a full
    // accumulator refresh is required
    static bool
    requires_refresh(const DiffType& diff, Color perspective, int blockBefore, int blockAfter);
};

}  // namespace Stockfish::Eval::NNUE::Features

#endif  // #ifndef NNUE_FEATURES_K32H3_H_INCLUDED

// === sf_src/src/nnue/features/full_threats.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)
  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//Definition of input features Full_Threats of NNUE evaluation function

#ifndef NNUE_FEATURES_FULL_THREATS_INCLUDED
#define NNUE_FEATURES_FULL_THREATS_INCLUDED



namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE::Features {

// Pawn diagonal threats only target knights and rooks (pawn-pawn relationships
// are handled by the PP_3Wide feature set), so pawns have 4 valid targets.
static constexpr int numValidTargets[PIECE_NB] = {0, 4, 10, 8, 8, 10, 0, 0,
                                                  0, 4, 10, 8, 8, 10, 0, 0};

class FullThreats {
   public:
    // Hash value embedded in the evaluation file
    static constexpr u32 HashValue = 0x2c41eaeau;

    // Number of feature dimensions
    static constexpr IndexType Dimensions = 59808;

    // clang-format off
    static constexpr int map[PIECE_TYPE_NB-2][PIECE_TYPE_NB-2] = {
      {-1,  0, -1,  1, -1, -1},
      { 0,  1,  2,  3,  4, -1},
      { 0,  1,  2,  3, -1, -1},
      { 0,  1,  2,  3, -1, -1},
      { 0,  1,  2,  3,  4, -1},
      {-1, -1, -1, -1, -1, -1}
    };
    // clang-format on

    // Maximum number of simultaneously active features.
    static constexpr IndexType MaxActiveDimensions = 256;
    using IndexList                                = ValueList<u16, MaxActiveDimensions>;
    using DiffType                                 = DirtyThreats;

    static IndexType
    make_index(Color perspective, Piece attkr, Square from, Square to, Piece attkd, Square ksq);

    // Get a list of indices for active features
    static void append_active_indices(Color perspective, const Position& pos, IndexList& active);

    // Get a list of indices for recently changed features
    static void append_changed_indices(Color                   perspective,
                                       Square                  ksq,
                                       const DiffType&         diff,
                                       IndexList&              removed,
                                       IndexList&              added,
                                       const ThreatWeightType* prefetchBase   = nullptr,
                                       IndexType               prefetchStride = 0);

    static void append_changed_indices_both(Square                  white_ksq,
                                            Square                  black_ksq,
                                            const DiffType&         diff,
                                            IndexList&              white_removed,
                                            IndexList&              white_added,
                                            IndexList&              black_removed,
                                            IndexList&              black_added,
                                            const ThreatWeightType* prefetchBase   = nullptr,
                                            IndexType               prefetchStride = 0);
};

}  // namespace Stockfish::Eval::NNUE::Features

#endif  // #ifndef NNUE_FEATURES_FULL_THREATS_INCLUDED

// === sf_src/src/nnue/features/pp_3wide.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NNUE_FEATURES_PP_3WIDE_INCLUDED
#define NNUE_FEATURES_PP_3WIDE_INCLUDED


namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE::Features {

class PP_3Wide {
   public:
    static constexpr u32 HashValue = 0x5c80fa69u;

    static constexpr IndexType PawnIds    = COLOR_NB * 48;
    static constexpr IndexType Dimensions = PawnIds * (PawnIds - 1) / 2;

    // Pawn pair feature indices are concatenated to threats, so this must equal ThreatFeatureSet::Dimensions;
    // see nnue_feature_transformer.h
    static constexpr IndexType IndexBase = 59808;
    using IndexList                      = ValueList<u16, 256>;
    using DiffType                       = DirtyPawnPairs;

    static IndexType make_index(
      Color perspective, Color color, Square from, Square to, Color pairedColor, Square ksq);

    static void append_active_indices(Color perspective, const Position& pos, IndexList& active);

    static void append_changed_indices(Color                   perspective,
                                       Square                  ksq,
                                       const DiffType&         diff,
                                       IndexList&              removed,
                                       IndexList&              added,
                                       const ThreatWeightType* prefetchBase   = nullptr,
                                       IndexType               prefetchStride = 0);

    static void append_changed_indices_both(Square                  white_ksq,
                                            Square                  black_ksq,
                                            const DiffType&         diff,
                                            IndexList&              white_removed,
                                            IndexList&              white_added,
                                            IndexList&              black_removed,
                                            IndexList&              black_added,
                                            const ThreatWeightType* prefetchBase   = nullptr,
                                            IndexType               prefetchStride = 0);
};

}  // namespace Stockfish::Eval::NNUE::Features

#endif  // #ifndef NNUE_FEATURES_PP_3WIDE_INCLUDED

// === sf_src/src/nnue/layers/affine_transform.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Definition of layer AffineTransform of NNUE evaluation function

#ifndef NNUE_LAYERS_AFFINE_TRANSFORM_H_INCLUDED
#define NNUE_LAYERS_AFFINE_TRANSFORM_H_INCLUDED

#include <cstdint>
#include <iostream>


// === sf_src/src/memory.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MEMORY_H_INCLUDED
#define MEMORY_H_INCLUDED

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>


#if defined(_WIN64)

    #if _WIN32_WINNT < 0x0601
        #undef _WIN32_WINNT
        #define _WIN32_WINNT 0x0601  // Force to include needed API prototypes
    #endif

    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #include <windows.h>

    // Some Windows headers (RPC/old headers) define short macros such
    // as 'small' expanding to 'char', which breaks identifiers in the code.
    // Undefine those macros immediately after including <windows.h>.
    #ifdef small
        #undef small
    #endif

    #include <psapi.h>

extern "C" {
using OpenProcessToken_t      = bool (*)(HANDLE, DWORD, PHANDLE);
using LookupPrivilegeValueA_t = bool (*)(LPCSTR, LPCSTR, PLUID);
using AdjustTokenPrivileges_t =
  bool (*)(HANDLE, BOOL, PTOKEN_PRIVILEGES, DWORD, PTOKEN_PRIVILEGES, PDWORD);
}
#endif

#if defined(__linux__) && !defined(__ANDROID__)
    #include <sys/mman.h>
    #include <unistd.h>
#endif


namespace Stockfish {

constexpr usize HugePageSize = usize(1) << 30;

[[noreturn]] inline void report_failed_allocation(usize bytes) {
    std::cerr << "Failed to allocate " << bytes << " bytes." << std::endl;
    std::exit(EXIT_FAILURE);
}

void* std_aligned_alloc(usize alignment, usize size);
void  std_aligned_free(void* ptr);

// Memory aligned by page size, min alignment: 4096 bytes
void* aligned_large_pages_alloc_with_hint(usize size, bool hugePageHint);
void* aligned_large_pages_alloc(usize size);
void  aligned_large_pages_free(void* mem);

bool has_large_pages();

// Frees memory which was placed there with placement new.
// Works for both single objects and arrays of unknown bound.
template<typename T, typename FREE_FUNC>
void memory_deleter(T* ptr, FREE_FUNC free_func) {
    if (!ptr)
        return;

    // Explicitly needed to call the destructor
    if constexpr (!std::is_trivially_destructible_v<T>)
        ptr->~T();

    free_func(ptr);
}

// Frees memory which was placed there with placement new.
// Works for both single objects and arrays of unknown bound.
template<typename T, typename FREE_FUNC>
void memory_deleter_array(T* ptr, FREE_FUNC free_func) {
    if (!ptr)
        return;


    // Move back on the pointer to where the size is allocated
    const usize array_offset = std::max(sizeof(usize), alignof(T));
    char*       raw_memory   = reinterpret_cast<char*>(ptr) - array_offset;

    if constexpr (!std::is_trivially_destructible_v<T>)
    {
        const usize size = *reinterpret_cast<usize*>(raw_memory);

        // Explicitly call the destructor for each element in reverse order
        for (usize i = size; i-- > 0;)
            ptr[i].~T();
    }

    free_func(raw_memory);
}

// Allocates memory for a single object and places it there with placement new
template<typename T, typename ALLOC_FUNC, typename... Args>
inline std::enable_if_t<!std::is_array_v<T>, T*> memory_allocator(ALLOC_FUNC alloc_func,
                                                                  Args&&... args) {
    void* raw_memory = alloc_func(sizeof(T));
    if (raw_memory == nullptr)
        report_failed_allocation(sizeof(T));
    ASSERT_ALIGNED(raw_memory, alignof(T));
    return new (raw_memory) T(std::forward<Args>(args)...);
}

// Allocates memory for an array of unknown bound and places it there with placement new
template<typename T, typename ALLOC_FUNC>
inline std::enable_if_t<std::is_array_v<T>, std::remove_extent_t<T>*>
memory_allocator(ALLOC_FUNC alloc_func, usize num) {
    using ElementType = std::remove_extent_t<T>;

    const usize array_offset = std::max(sizeof(usize), alignof(ElementType));

    // Save the array size in the memory location
    const usize bytes      = array_offset + num * sizeof(ElementType);
    char*       raw_memory = reinterpret_cast<char*>(alloc_func(bytes));
    if (raw_memory == nullptr)
        report_failed_allocation(bytes);
    ASSERT_ALIGNED(raw_memory, alignof(T));

    new (raw_memory) usize(num);

    for (usize i = 0; i < num; ++i)
        new (raw_memory + array_offset + i * sizeof(ElementType)) ElementType();

    // Need to return the pointer at the start of the array so that
    // the indexing in unique_ptr<T[]> works.
    return reinterpret_cast<ElementType*>(raw_memory + array_offset);
}

//
//
// aligned large page unique ptr
//
//

template<typename T>
struct LargePageDeleter {
    void operator()(T* ptr) const { return memory_deleter<T>(ptr, aligned_large_pages_free); }
};

template<typename T>
struct LargePageArrayDeleter {
    void operator()(T* ptr) const { return memory_deleter_array<T>(ptr, aligned_large_pages_free); }
};

template<typename T>
using LargePagePtr =
  std::conditional_t<std::is_array_v<T>,
                     std::unique_ptr<T, LargePageArrayDeleter<std::remove_extent_t<T>>>,
                     std::unique_ptr<T, LargePageDeleter<T>>>;

// make_unique_large_page for single objects
template<typename T, typename... Args>
std::enable_if_t<!std::is_array_v<T>, LargePagePtr<T>> make_unique_large_page(Args&&... args) {
    static_assert(alignof(T) <= 4096,
                  "aligned_large_pages_alloc() may fail for such a big alignment requirement of T");

    T* obj = memory_allocator<T>(aligned_large_pages_alloc, std::forward<Args>(args)...);

    return LargePagePtr<T>(obj);
}

// make_unique_large_page for arrays of unknown bound
template<typename T>
std::enable_if_t<std::is_array_v<T>, LargePagePtr<T>> make_unique_large_page(usize num) {
    using ElementType = std::remove_extent_t<T>;

    static_assert(alignof(ElementType) <= 4096,
                  "aligned_large_pages_alloc() may fail for such a big alignment requirement of T");

    ElementType* memory = memory_allocator<T>(aligned_large_pages_alloc, num);

    return LargePagePtr<T>(memory);
}

//
//
// aligned unique ptr
//
//

template<typename T>
struct AlignedDeleter {
    void operator()(T* ptr) const { return memory_deleter<T>(ptr, std_aligned_free); }
};

template<typename T>
struct AlignedArrayDeleter {
    void operator()(T* ptr) const { return memory_deleter_array<T>(ptr, std_aligned_free); }
};

template<typename T>
using AlignedPtr =
  std::conditional_t<std::is_array_v<T>,
                     std::unique_ptr<T, AlignedArrayDeleter<std::remove_extent_t<T>>>,
                     std::unique_ptr<T, AlignedDeleter<T>>>;

// make_unique_aligned for single objects
template<typename T, typename... Args>
std::enable_if_t<!std::is_array_v<T>, AlignedPtr<T>> make_unique_aligned(Args&&... args) {
    const auto func = [](usize size) { return std_aligned_alloc(alignof(T), size); };
    T*         obj  = memory_allocator<T>(func, std::forward<Args>(args)...);

    return AlignedPtr<T>(obj);
}

// make_unique_aligned for arrays of unknown bound
template<typename T>
std::enable_if_t<std::is_array_v<T>, AlignedPtr<T>> make_unique_aligned(usize num) {
    using ElementType = std::remove_extent_t<T>;

    const auto   func   = [](usize size) { return std_aligned_alloc(alignof(ElementType), size); };
    ElementType* memory = memory_allocator<T>(func, num);

    return AlignedPtr<T>(memory);
}


// Get the first aligned element of an array.
// ptr must point to an array of size at least `sizeof(T) * N + alignment` bytes,
// where N is the number of elements in the array.
template<uintptr_t Alignment, typename T>
T* align_ptr_up(T* ptr) {
    static_assert(alignof(T) < Alignment);

    const uintptr_t ptrint = reinterpret_cast<uintptr_t>(reinterpret_cast<char*>(ptr));
    return reinterpret_cast<T*>(
      reinterpret_cast<char*>((ptrint + (Alignment - 1)) / Alignment * Alignment));
}

#if defined(__linux__) && !defined(__ANDROID__)

// Allocate size bytes aligned to a 2 MB boundary using mmap.
// On success the returned pointer can be freed with munmap(ptr, size).
inline void* mmap_huge_aligned(usize size, int flags, int fd = -1, off_t offset = 0) {
    constexpr usize Alignment = 2 * 1024 * 1024;
    const long      pageSize  = sysconf(_SC_PAGESIZE);

    if (size >= Alignment && pageSize > 0)
    {
        const usize mappingSize =
          ((size + static_cast<usize>(pageSize) - 1) / static_cast<usize>(pageSize))
          * static_cast<usize>(pageSize);
        const usize reservationSize = mappingSize + Alignment;
        void*       reservation =
          mmap(nullptr, reservationSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (reservation != MAP_FAILED)
        {
            char* const base        = static_cast<char*>(reservation);
            char* const alignedBase = align_ptr_up<Alignment>(base);

            void* mapped =
              mmap(alignedBase, size, PROT_READ | PROT_WRITE, flags | MAP_FIXED, fd, offset);

            if (mapped != MAP_FAILED)
            {
                const usize prefixSize = static_cast<usize>(alignedBase - base);
                const usize suffixSize = reservationSize - prefixSize - mappingSize;
                if (prefixSize)
                    munmap(reservation, prefixSize);
                if (suffixSize)
                    munmap(alignedBase + mappingSize, suffixSize);
                return mapped;
            }

            munmap(reservation, reservationSize);
        }
    }

    return mmap(nullptr, size, PROT_READ | PROT_WRITE, flags, fd, offset);
}

#endif

#if defined(_WIN32)

template<typename FuncYesT, typename FuncNoT>
auto windows_try_with_large_page_privileges([[maybe_unused]] FuncYesT&& fyes, FuncNoT&& fno) {

    #if !defined(_WIN64)
    return fno();
    #else

    HANDLE hProcessToken{};
    LUID   luid{};

    const usize largePageSize = GetLargePageMinimum();
    if (!largePageSize)
        return fno();

    // Dynamically link OpenProcessToken, LookupPrivilegeValue and AdjustTokenPrivileges

    HMODULE hAdvapi32 = GetModuleHandle(TEXT("advapi32.dll"));

    if (!hAdvapi32)
        hAdvapi32 = LoadLibrary(TEXT("advapi32.dll"));

    auto OpenProcessToken_f =
      OpenProcessToken_t((void (*)()) GetProcAddress(hAdvapi32, "OpenProcessToken"));
    if (!OpenProcessToken_f)
        return fno();
    auto LookupPrivilegeValueA_f =
      LookupPrivilegeValueA_t((void (*)()) GetProcAddress(hAdvapi32, "LookupPrivilegeValueA"));
    if (!LookupPrivilegeValueA_f)
        return fno();
    auto AdjustTokenPrivileges_f =
      AdjustTokenPrivileges_t((void (*)()) GetProcAddress(hAdvapi32, "AdjustTokenPrivileges"));
    if (!AdjustTokenPrivileges_f)
        return fno();

    // We need SeLockMemoryPrivilege, so try to enable it for the process

    if (!OpenProcessToken_f(  // OpenProcessToken()
          GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hProcessToken))
        return fno();

    if (!LookupPrivilegeValueA_f(nullptr, "SeLockMemoryPrivilege", &luid))
        return fno();

    TOKEN_PRIVILEGES tp{};
    TOKEN_PRIVILEGES prevTp{};
    DWORD            prevTpLen = 0;

    tp.PrivilegeCount           = 1;
    tp.Privileges[0].Luid       = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Try to enable SeLockMemoryPrivilege. Note that even if AdjustTokenPrivileges()
    // succeeds, we still need to query GetLastError() to ensure that the privileges
    // were actually obtained.

    if (!AdjustTokenPrivileges_f(hProcessToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), &prevTp,
                                 &prevTpLen)
        || GetLastError() != ERROR_SUCCESS)
        return fno();

    auto&& ret = fyes(largePageSize);

    // Privilege no longer needed, restore previous state
    AdjustTokenPrivileges_f(hProcessToken, FALSE, &prevTp, 0, nullptr, nullptr);

    CloseHandle(hProcessToken);

    return std::forward<decltype(ret)>(ret);

    #endif
}

#endif

template<typename T, typename ByteT>
T load_as(const ByteT* buffer) {
    static_assert(std::is_trivially_copyable<T>::value, "Type must be trivially copyable");
    static_assert(sizeof(ByteT) == 1);

    if (reinterpret_cast<uintptr_t>(buffer) % alignof(T) != 0)
    {
        assert(false);
#ifdef __GNUC__
        __builtin_unreachable();
#endif
    }

    T value;
    std::memcpy(&value, buffer, sizeof(T));

    return value;
}

}  // namespace Stockfish

#endif  // #ifndef MEMORY_H_INCLUDED

// === sf_src/src/nnue/simd.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NNUE_SIMD_H_INCLUDED
#define NNUE_SIMD_H_INCLUDED

#if defined(USE_AVX2)
    #include <immintrin.h>

#elif defined(USE_SSE41)
    #include <smmintrin.h>

#elif defined(USE_SSSE3)
    #include <tmmintrin.h>

#elif defined(USE_SSE2)
    #include <emmintrin.h>

#elif defined(USE_NEON)
    #include <arm_neon.h>

#elif defined(USE_LASX)
    #include <lasxintrin.h>
    #include <lsxintrin.h>

#elif defined(USE_LSX)
    #include <lsxintrin.h>

#elif defined(USE_RVV)
    #include <riscv_vector.h>

#endif


namespace Stockfish::Eval::NNUE::SIMD {

#if defined(USE_AVX2)
    #define USE_PAIR_ACTIVATIONS
#endif

// On x86 with SSSE3 or better, two int8 weight rows can be widened and summed
// into int16 lanes with one unpack + one maddubs (against a vector of ones)
// instead of one sign-extension per row. See apply_feature_pair() in
// nnue_accumulator.cpp and FeatureTransformer::WideningPairOrder.
// Not used on wasm: emscripten emulates maddubs with several instructions.
#if defined(USE_SSSE3) && !defined(__wasm__)
    #define USE_PAIRED_WIDENING_ADD
#endif

// If vector instructions are enabled, we update and refresh the
// accumulator tile by tile such that each tile fits in the CPU's
// vector registers.
#define VECTOR

#ifdef USE_AVX512
using vec_t    = __m512i;
using vec_i8_t = __m256i;
    #define vec_load(a) _mm512_load_si512(a)
    #define vec_store(a, b) _mm512_store_si512(a, b)
    #define vec_convert_8_16(a) _mm512_cvtepi8_epi16(a)
    #define vec_add_16(a, b) _mm512_add_epi16(a, b)
    #define vec_sub_16(a, b) _mm512_sub_epi16(a, b)
    #define vec_mulhi_16(a, b) _mm512_mulhi_epi16(a, b)
    #define vec_zero() _mm512_setzero_epi32()
    #define vec_set_16(a) _mm512_set1_epi16(a)
    #define vec_max_16(a, b) _mm512_max_epi16(a, b)
    #define vec_min_16(a, b) _mm512_min_epi16(a, b)
    #define vec_slli_16(a, b) _mm512_slli_epi16(a, b)
    #define vec_srai_16(a, b) _mm512_srai_epi16(a, b)
    // Inverse permuted at load time
    #define vec_packus_16(a, b) _mm512_packus_epi16(a, b)
    #define vec_set_8(a) _mm512_set1_epi8(a)
    #define vec_unpacklo_8(a, b) _mm512_unpacklo_epi8(a, b)
    #define vec_unpackhi_8(a, b) _mm512_unpackhi_epi8(a, b)
    #define vec_maddubs_16(a, b) _mm512_maddubs_epi16(a, b)
    #define vec_xor(a, b) _mm512_xor_si512(a, b)

    #define vec_nnz(a) _mm512_cmpgt_epi32_mask(a, _mm512_setzero_si512())

    #define NumRegistersSIMD 16
    #define MaxChunkSize 64

#elif USE_AVX2
using vec_t    = __m256i;
using vec_i8_t = __m128i;
    #define vec_load(a) _mm256_load_si256(a)
    #define vec_store(a, b) _mm256_store_si256(a, b)
    #define vec_convert_8_16(a) _mm256_cvtepi8_epi16(a)
    #define vec_add_16(a, b) _mm256_add_epi16(a, b)
    #define vec_sub_16(a, b) _mm256_sub_epi16(a, b)
    #define vec_mulhi_16(a, b) _mm256_mulhi_epi16(a, b)
    #define vec_zero() _mm256_setzero_si256()
    #define vec_set_16(a) _mm256_set1_epi16(a)
    #define vec_max_16(a, b) _mm256_max_epi16(a, b)
    #define vec_min_16(a, b) _mm256_min_epi16(a, b)
    #define vec_slli_16(a, b) _mm256_slli_epi16(a, b)
    #define vec_srai_16(a, b) _mm256_srai_epi16(a, b)
    // Inverse permuted at load time
    #define vec_packus_16(a, b) _mm256_packus_epi16(a, b)
    #define vec_set_8(a) _mm256_set1_epi8(a)
    #define vec_unpacklo_8(a, b) _mm256_unpacklo_epi8(a, b)
    #define vec_unpackhi_8(a, b) _mm256_unpackhi_epi8(a, b)
    #define vec_maddubs_16(a, b) _mm256_maddubs_epi16(a, b)
    #define vec_xor(a, b) _mm256_xor_si256(a, b)

    #define vec_nnz(a) \
        _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(a, _mm256_setzero_si256())))

    #define NumRegistersSIMD 12
    #define MaxChunkSize 32

#elif USE_SSE2
using vec_t    = __m128i;
using vec_i8_t = u64;
    #define vec_load(a) (*(a))
    #define vec_store(a, b) *(a) = (b)
    #define vec_add_16(a, b) _mm_add_epi16(a, b)
    #define vec_sub_16(a, b) _mm_sub_epi16(a, b)
    #define vec_mulhi_16(a, b) _mm_mulhi_epi16(a, b)
    #define vec_zero() _mm_setzero_si128()
    #define vec_set_16(a) _mm_set1_epi16(a)
    #define vec_max_16(a, b) _mm_max_epi16(a, b)
    #define vec_min_16(a, b) _mm_min_epi16(a, b)
    #define vec_slli_16(a, b) _mm_slli_epi16(a, b)
    #define vec_srai_16(a, b) _mm_srai_epi16(a, b)
    #define vec_packus_16(a, b) _mm_packus_epi16(a, b)
    #define vec_set_8(a) _mm_set1_epi8(a)
    #define vec_unpacklo_8(a, b) _mm_unpacklo_epi8(a, b)
    #define vec_unpackhi_8(a, b) _mm_unpackhi_epi8(a, b)
    #define vec_xor(a, b) _mm_xor_si128(a, b)

    #ifdef USE_SSSE3
        #define vec_maddubs_16(a, b) _mm_maddubs_epi16(a, b)
        #define vec_nnz(a) \
            _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(a, _mm_setzero_si128())))
    #endif

    #ifdef __i386__
inline __m128i _mm_cvtsi64_si128(i64 val) {
    return _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&val));
}
    #endif

    #ifdef USE_SSE41
        #ifdef __wasm__
            #define vec_convert_8_16(a) wasm_i16x8_load8x8(reinterpret_cast<const void*>(&a))
        #else
            #define vec_convert_8_16(a) _mm_cvtepi8_epi16(_mm_cvtsi64_si128(static_cast<i64>(a)))
        #endif
    #else
// Credit: Yoshie2000
inline __m128i vec_convert_8_16(u64 x) {
    __m128i v8   = _mm_cvtsi64_si128(static_cast<i64>(x));
    __m128i sign = _mm_cmpgt_epi8(_mm_setzero_si128(), v8);
    return _mm_unpacklo_epi8(v8, sign);
}
    #endif

    #define NumRegistersSIMD (Is64Bit ? 12 : 6)
    #define MaxChunkSize 16

#elif USE_NEON
using vec_i8x8_t __attribute__((may_alias))  = int8x8_t;
using vec_i16x8_t __attribute__((may_alias)) = int16x8_t;
using vec_i8x16_t __attribute__((may_alias)) = int8x16_t;
using vec_u16x8_t __attribute__((may_alias)) = uint16x8_t;
using vec_i32x4_t __attribute__((may_alias)) = int32x4_t;

using vec_t __attribute__((may_alias))    = int16x8_t;
using vec_i8_t __attribute__((may_alias)) = int8x16_t;
    #define vec_load(a) (*(a))
    #define vec_store(a, b) *(a) = (b)
    #define vec_add_16(a, b) vaddq_s16(a, b)
    #define vec_sub_16(a, b) vsubq_s16(a, b)
    #define vec_mulhi_16(a, b) vqdmulhq_s16(a, b)
    #define vec_zero() vec_t{0}
    #define vec_set_16(a) vdupq_n_s16(a)
    #define vec_max_16(a, b) vmaxq_s16(a, b)
    #define vec_min_16(a, b) vminq_s16(a, b)
    #define vec_slli_16(a, b) vshlq_s16(a, vec_set_16(b))
    #define vec_packus_16(a, b) reinterpret_cast<vec_t>(vcombine_u8(vqmovun_s16(a), vqmovun_s16(b)))

    #define NumRegistersSIMD 16
    #define MaxChunkSize 16

    #ifndef __aarch64__
// Instructions that don't exist on 32-bit ARM
inline int16x8_t vaddw_high_s8(int16x8_t a, int8x16_t b) { return vaddw_s8(a, vget_high_s8(b)); }
inline int16x8_t vsubw_high_s8(int16x8_t a, int8x16_t b) { return vsubw_s8(a, vget_high_s8(b)); }
    #endif

#elif USE_LASX
using vec_t    = __m256i;
using vec_i8_t = __m128i;

inline __m256i lasx_load256(const __m256i* a) {
    return __lasx_xvld(reinterpret_cast<const void*>(a), 0);
}

inline void lasx_store256(__m256i* a, __m256i b) { __lasx_xvst(b, reinterpret_cast<void*>(a), 0); }

inline __m256i lasx_packus_16(__m256i a, __m256i b) {
    #if defined(__clang__) && defined(__has_builtin) && __has_builtin(__builtin_lasx_xvssrani_bu_h)
    return (__m256i) __builtin_lasx_xvssrani_bu_h((v32i8) b, (v32i8) a, 0);
    #else
    return __lasx_xvssrani_bu_h(b, a, 0);
    #endif
}

inline __m256i lasx_packus_32(__m256i a, __m256i b) {
    #if defined(__clang__) && defined(__has_builtin) && __has_builtin(__builtin_lasx_xvssrani_hu_w)
    return (__m256i) __builtin_lasx_xvssrani_hu_w((v16i16) b, (v16i16) a, 0);
    #else
    return __lasx_xvssrani_hu_w(b, a, 0);
    #endif
}

    #define vec_load(a) lasx_load256(a)
    #define vec_store(a, b) lasx_store256(a, b)
    #define vec_add_16(a, b) __lasx_xvadd_h(a, b)
    #define vec_sub_16(a, b) __lasx_xvsub_h(a, b)
    #define vec_mulhi_16(a, b) __lasx_xvmuh_h(a, b)
    #define vec_zero() __lasx_xvldi(0)
    #define vec_set_16(a) __lasx_xvreplgr2vr_h(a)
    #define vec_max_16(a, b) __lasx_xvmax_h(a, b)
    #define vec_min_16(a, b) __lasx_xvmin_h(a, b)
    #define vec_slli_16(a, b) __lasx_xvslli_h(a, b)
    // Inverse permuted at load time
    #define vec_packus_16(a, b) lasx_packus_16(a, b)
    #define vec_nnz(a) lasx_vec_nnz(a)
    #define vec_convert_8_16(a) lasx_cvtepi8_epi16(a)
    #define vec_mulhi_8 __lasx_xvmuh_bu
    #define vec_srli_8 __lasx_xvsrli_b

    #define NumRegistersSIMD 24
    #define MaxChunkSize 32

inline __m256i lasx_cvtepi8_epi16(__m128i a) {
    #if defined(__has_builtin) && __has_builtin(__builtin_lasx_cast_128)
    return __lasx_vext2xv_h_b(__lasx_cast_128(a));
    #elif defined(__GNUC__) && !defined(__clang__)
    __m256i out;
    __asm__("vext2xv.h.b %u0, %u1" : "=f"(out) : "f"(a));
    return out;
    #else
    i64     lo = (i64) __lsx_vpickve2gr_d(a, 0);
    i64     hi = (i64) __lsx_vpickve2gr_d(a, 1);
    __m256i v  = __lasx_xvldi(0);
    v          = __lasx_xvinsgr2vr_d(v, lo, 0);
    v          = __lasx_xvinsgr2vr_d(v, hi, 2);
    return __lasx_xvsllwil_h_b(v, 0);
    #endif
}

inline int lasx_vec_nnz(__m256i a) {
    const __m256i cmp = __lasx_xvslt_w(__lasx_xvldi(0), a);
    const __m256i msk = __lasx_xvmskltz_w(cmp);
    return ((int) __lasx_xvpickve2gr_w(msk, 0)) | (((int) __lasx_xvpickve2gr_w(msk, 4)) << 4);
}

#elif USE_LSX
using vec_t    = __m128i;
using vec_i8_t = u64;

inline __m128i lsx_packus_16(__m128i a, __m128i b) {
    #if defined(__clang__) && defined(__has_builtin) && __has_builtin(__builtin_lsx_vssrani_bu_h)
    return (__m128i) __builtin_lsx_vssrani_bu_h((v16i8) b, (v16i8) a, 0);
    #else
    return __lsx_vssrani_bu_h(b, a, 0);
    #endif
}

inline __m128i lsx_packus_32(__m128i a, __m128i b) {
    #if defined(__clang__) && defined(__has_builtin) && __has_builtin(__builtin_lsx_vssrani_hu_w)
    return (__m128i) __builtin_lsx_vssrani_hu_w((v8i16) b, (v8i16) a, 0);
    #else
    return __lsx_vssrani_hu_w(b, a, 0);
    #endif
}

    #define vec_load(a) (*(a))
    #define vec_store(a, b) *(a) = (b)
    #define vec_add_16(a, b) __lsx_vadd_h(a, b)
    #define vec_sub_16(a, b) __lsx_vsub_h(a, b)
    #define vec_mulhi_16(a, b) __lsx_vmuh_h(a, b)
    #define vec_zero() __lsx_vldi(0)
    #define vec_set_16(a) __lsx_vreplgr2vr_h(a)
    #define vec_max_16(a, b) __lsx_vmax_h(a, b)
    #define vec_min_16(a, b) __lsx_vmin_h(a, b)
    #define vec_slli_16(a, b) __lsx_vslli_h(a, b)
    // Inverse permuted at load time
    #define vec_packus_16(a, b) lsx_packus_16(a, b)

inline int lsx_vec_nnz(__m128i a) {
    const __m128i cmp = __lsx_vslt_w(__lsx_vldi(0), a);
    const __m128i msk = __lsx_vmskltz_w(cmp);
    return ((int) __lsx_vpickve2gr_w(msk, 0));
}
    #define vec_nnz(a) lsx_vec_nnz(a)

inline __m128i vec_convert_8_16(u64 x) {
    __m128i v = __lsx_vldrepl_d(reinterpret_cast<const void*>(&x), 0);
    return __lsx_vsllwil_h_b(v, 0);
}

    #define vec_mulhi_8 __lsx_vmuh_bu
    #define vec_srli_8 __lsx_vsrli_b

    #define NumRegistersSIMD 24
    #define MaxChunkSize 16

#else
    #undef VECTOR

#endif

#ifdef VECTOR
static_assert(MaxChunkSize == sizeof(vec_t), "MaxChunkSize must equal one vector.");
#endif

#if defined(USE_AVX512)

[[maybe_unused]] static int m512_hadd(__m512i sum, int bias) {
    return _mm512_reduce_add_epi32(sum) + bias;
}

[[maybe_unused]] static void m512_add_dpbusd_epi32(__m512i& acc, __m512i a, __m512i b) {

    #if defined(USE_VNNI)
    acc = _mm512_dpbusd_epi32(acc, a, b);
    #else
    __m512i product0 = _mm512_maddubs_epi16(a, b);
    product0         = _mm512_madd_epi16(product0, _mm512_set1_epi16(1));
    acc              = _mm512_add_epi32(acc, product0);
    #endif
}

#endif

#if defined(USE_AVX2)

[[maybe_unused]] static int m256_hadd(__m256i sum, int bias) {
    __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extracti128_si256(sum, 1));
    sum128         = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_BADC));
    sum128         = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_CDAB));
    return _mm_cvtsi128_si32(sum128) + bias;
}

[[maybe_unused]] static void m256_add_dpbusd_epi32(__m256i& acc, __m256i a, __m256i b) {

    #if defined(USE_VNNI)
    acc = _mm256_dpbusd_epi32(acc, a, b);
    #else
    __m256i product0 = _mm256_maddubs_epi16(a, b);
    product0         = _mm256_madd_epi16(product0, _mm256_set1_epi16(1));
    acc              = _mm256_add_epi32(acc, product0);
    #endif
}

#endif

#if defined(USE_SSSE3)

[[maybe_unused]] static int m128_hadd(__m128i sum, int bias) {
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x4E));  //_MM_PERM_BADC
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xB1));  //_MM_PERM_CDAB
    return _mm_cvtsi128_si32(sum) + bias;
}

[[maybe_unused]] static void m128_add_dpbusd_epi32(__m128i& acc, __m128i a, __m128i b) {
    #if defined(__wasm_relaxed_simd__)
    acc = wasm_i32x4_relaxed_dot_i8x16_i7x16_add(b, a, acc);
    #else
    __m128i product0 = _mm_maddubs_epi16(a, b);
    product0         = _mm_madd_epi16(product0, _mm_set1_epi16(1));
    acc              = _mm_add_epi32(acc, product0);
    #endif
}

#endif

#if defined(USE_NEON_DOTPROD)

[[maybe_unused]] static void
dotprod_m128_add_dpbusd_epi32(int32x4_t& acc, int8x16_t a, int8x16_t b) {

    acc = vdotq_s32(acc, a, b);
}
#endif

#if defined(USE_NEON)

[[maybe_unused]] static int neon_m128_reduce_add_epi32(int32x4_t s) {
    #if USE_NEON >= 8
    return vaddvq_s32(s);
    #else
    return s[0] + s[1] + s[2] + s[3];
    #endif
}

[[maybe_unused]] static int neon_m128_hadd(int32x4_t sum, int bias) {
    return neon_m128_reduce_add_epi32(sum) + bias;
}

#endif

#if USE_NEON >= 8
[[maybe_unused]] static void neon_m128_add_dpbusd_epi32(int32x4_t& acc, int8x16_t a, int8x16_t b) {

    int16x8_t product0 = vmull_s8(vget_low_s8(a), vget_low_s8(b));
    int16x8_t product1 = vmull_high_s8(a, b);
    int16x8_t sum      = vpaddq_s16(product0, product1);
    acc                = vpadalq_s16(acc, sum);
}
#endif

#if defined(USE_LASX)

[[maybe_unused]] static int lasx_m256_hadd(__m256i sum, int bias) {
    __m256i v  = sum;
    v          = __lasx_xvadd_w(v, __lasx_xvshuf4i_w(v, 0x4E));  // [C,D,A,B] per lane
    v          = __lasx_xvadd_w(v, __lasx_xvshuf4i_w(v, 0xB1));  // [B,A,D,C] per lane
    int lo_sum = (int) __lasx_xvpickve2gr_w(v, 0);
    int hi_sum = (int) __lasx_xvpickve2gr_w(v, 4);
    return lo_sum + hi_sum + bias;
}

[[maybe_unused]] static void lasx_m256_add_dpbusd_epi32(__m256i& acc, __m256i a, __m256i b) {
    __m256i tmp = __lasx_xvmulwev_h_bu_b(a, b);
    tmp         = __lasx_xvmaddwod_h_bu_b(tmp, a, b);
    acc         = __lasx_xvadd_w(acc, __lasx_xvhaddw_w_h(tmp, tmp));
}

#endif  // USE_LASX

#if defined(USE_LSX)

[[maybe_unused]] static int lsx_m128_hadd(__m128i sum, int bias) {
    sum = __lsx_vadd_w(sum, __lsx_vshuf4i_w(sum, 0x4E));  // [C,D,A,B]
    sum = __lsx_vadd_w(sum, __lsx_vshuf4i_w(sum, 0xB1));  // [B,A,D,C]
    return __lsx_vpickve2gr_w(sum, 0) + bias;
}

[[maybe_unused]] static void lsx_m128_add_dpbusd_epi32(__m128i& acc, __m128i a, __m128i b) {
    // tmp[i] = a[2i]*b[2i] + a[2i+1]*b[2i+1]
    __m128i tmp = __lsx_vmulwev_h_bu_b(a, b);
    tmp         = __lsx_vmaddwod_h_bu_b(tmp, a, b);
    acc         = __lsx_vadd_w(acc, __lsx_vhaddw_w_h(tmp, tmp));
}

#endif  // USE_LSX


// Compute optimal SIMD register count for feature transformer accumulation.
template<IndexType TransformedFeatureWidth, IndexType HalfDimensions>
class SIMDTiling {
#ifdef VECTOR
        // We use __m* types as template arguments, which causes GCC to emit warnings
        // about losing some attribute information. This is irrelevant to us as we
        // only take their size, so the following pragma are harmless.
    #if defined(__GNUC__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wignored-attributes"
    #endif

    template<typename SIMDRegisterType, typename LaneType, int NumLanes, int MaxRegisters>
    static constexpr int BestRegisterCount() {
        constexpr usize RegisterSize = sizeof(SIMDRegisterType);
        constexpr usize LaneSize     = sizeof(LaneType);

        static_assert(RegisterSize >= LaneSize);
        static_assert(MaxRegisters <= NumRegistersSIMD);
        static_assert(MaxRegisters > 0);
        static_assert(NumRegistersSIMD > 0);
        static_assert(RegisterSize % LaneSize == 0);
        static_assert((NumLanes * LaneSize) % RegisterSize == 0);

        const int ideal = (NumLanes * LaneSize) / RegisterSize;
        if (ideal <= MaxRegisters)
            return ideal;

        // Look for the largest divisor of the ideal register count that is smaller than MaxRegisters
        for (int divisor = MaxRegisters; divisor > 1; --divisor)
            if (ideal % divisor == 0)
                return divisor;

        return 1;
    }

    #if defined(__GNUC__)
        #pragma GCC diagnostic pop
    #endif

   public:
    static constexpr int NumRegs =
      BestRegisterCount<vec_t, WeightType, TransformedFeatureWidth, NumRegistersSIMD>();

    static constexpr IndexType TileHeight = NumRegs * sizeof(vec_t) / 2;

    static_assert(HalfDimensions % TileHeight == 0, "TileHeight must divide HalfDimensions");
#endif
};

}

#endif

/*
  This file contains the definition for a fully connected layer (aka affine transform).

    - expected use-case is for when PaddedInputDimensions == 32 and InputDimensions <= 32.
      - that's why AVX512 is hard to implement
    - expected use-case is small layers
    - inputs are processed in chunks of 4, weights are respectively transposed
    - accumulation happens directly to int32s
*/

namespace Stockfish::Eval::NNUE::Layers {

#if defined(USE_SSSE3) || defined(USE_NEON_DOTPROD) || defined(USE_LSX) || defined(USE_LASX)
    #define ENABLE_SEQ_OPT
#endif

// Fallback implementation for older/other architectures.
// Requires the input to be padded to at least 16 values.
#if !defined(ENABLE_SEQ_OPT) && !defined(USE_RVV)

template<IndexType InputDimensions, IndexType PaddedInputDimensions, IndexType OutputDimensions>
static void
affine_transform_non_ssse3(i32* output, const i8* weights, const i32* biases, const u8* input) {
    #if defined(USE_SSE2) || defined(USE_NEON)
        #if defined(USE_SSE2)
    // At least a multiple of 16, with SSE2.
    constexpr IndexType NumChunks   = ceil_to_multiple<IndexType>(InputDimensions, 16) / 16;
    const __m128i       Zeros       = _mm_setzero_si128();
    const auto          inputVector = reinterpret_cast<const __m128i*>(input);

        #elif defined(USE_NEON)
    constexpr IndexType NumChunks   = ceil_to_multiple<IndexType>(InputDimensions, 16) / 16;
    const auto          inputVector = reinterpret_cast<const int8x8_t*>(input);
        #endif

    for (IndexType i = 0; i < OutputDimensions; ++i)
    {
        const IndexType offset = i * PaddedInputDimensions;

        #if defined(USE_SSE2)
        __m128i    sumLo = _mm_cvtsi32_si128(biases[i]);
        __m128i    sumHi = Zeros;
        const auto row   = reinterpret_cast<const __m128i*>(&weights[offset]);
        for (IndexType j = 0; j < NumChunks; ++j)
        {
            __m128i row_j           = _mm_load_si128(&row[j]);
            __m128i input_j         = _mm_load_si128(&inputVector[j]);
            __m128i extendedRowLo   = _mm_srai_epi16(_mm_unpacklo_epi8(row_j, row_j), 8);
            __m128i extendedRowHi   = _mm_srai_epi16(_mm_unpackhi_epi8(row_j, row_j), 8);
            __m128i extendedInputLo = _mm_unpacklo_epi8(input_j, Zeros);
            __m128i extendedInputHi = _mm_unpackhi_epi8(input_j, Zeros);
            __m128i productLo       = _mm_madd_epi16(extendedRowLo, extendedInputLo);
            __m128i productHi       = _mm_madd_epi16(extendedRowHi, extendedInputHi);
            sumLo                   = _mm_add_epi32(sumLo, productLo);
            sumHi                   = _mm_add_epi32(sumHi, productHi);
        }
        __m128i sum           = _mm_add_epi32(sumLo, sumHi);
        __m128i sumHigh_64    = _mm_shuffle_epi32(sum, _MM_SHUFFLE(1, 0, 3, 2));
        sum                   = _mm_add_epi32(sum, sumHigh_64);
        __m128i sum_second_32 = _mm_shufflelo_epi16(sum, _MM_SHUFFLE(1, 0, 3, 2));
        sum                   = _mm_add_epi32(sum, sum_second_32);
        output[i]             = _mm_cvtsi128_si32(sum);

        #elif defined(USE_NEON)

        int32x4_t  sum = {biases[i]};
        const auto row = reinterpret_cast<const SIMD::vec_i8x8_t*>(&weights[offset]);
        for (IndexType j = 0; j < NumChunks; ++j)
        {
            int16x8_t product = vmull_s8(inputVector[j * 2], row[j * 2]);
            product           = vmlal_s8(product, inputVector[j * 2 + 1], row[j * 2 + 1]);
            sum               = vpadalq_s16(sum, product);
        }
        output[i] = SIMD::neon_m128_reduce_add_epi32(sum);

        #endif
    }
    #else
    std::memcpy(output, biases, sizeof(i32) * OutputDimensions);

    // Traverse weights in transpose order to take advantage of input sparsity
    for (IndexType i = 0; i < InputDimensions; ++i)
        if (input[i])
        {
            const i8* w  = &weights[i];
            const int in = input[i];
            for (IndexType j = 0; j < OutputDimensions; ++j)
                output[j] += w[j * PaddedInputDimensions] * in;
        }
    #endif
}

#endif  // !ENABLE_SEQ_OPT

template<IndexType InDims, IndexType OutDims>
class AffineTransform {
   public:
    // Input/output type
    using InputType  = u8;
    using OutputType = i32;

    // Number of input/output dimensions
    static constexpr IndexType InputDimensions  = InDims;
    static constexpr IndexType OutputDimensions = OutDims;

    static constexpr IndexType PaddedInputDimensions =
      ceil_to_multiple<IndexType>(InputDimensions, MaxSimdWidth);
    static constexpr IndexType PaddedOutputDimensions =
      ceil_to_multiple<IndexType>(OutputDimensions, MaxSimdWidth);

    using OutputBuffer = OutputType[PaddedOutputDimensions];

    // Hash value embedded in the evaluation file
    static constexpr u32 get_hash_value(u32 prevHash) {
        u32 hashValue = 0xCC03DAE4u;
        hashValue += OutputDimensions;
        hashValue ^= prevHash >> 1;
        hashValue ^= prevHash << 31;
        return hashValue;
    }

    static constexpr IndexType get_weight_index_scrambled(IndexType i) {
        IndexType inputIndex = i % PaddedInputDimensions;

#if defined(USE_PAIR_ACTIVATIONS) || defined(USE_LASX)
        // At load time, pre-permute the weights to match the per-128-bit-lane interleaving that
        // the previous layer produces, either via SqrClippedReLU::propagate_pair() or via the
        // separate SqrClippedReLU/ClippedReLU propagate() calls, so no shuffle is needed at runtime.
        const IndexType block = inputIndex / 32;
        const IndexType chunk = (inputIndex % 32) / 4;
    #if defined(USE_AVX512)
        inputIndex = block * 32 + ((chunk % 4) * 2 + chunk / 4) * 4 + inputIndex % 4;
    #else
        inputIndex = block * 32 + ((chunk % 2) * 4 + chunk / 2) * 4 + inputIndex % 4;
    #endif
#endif
        return inputIndex / 4 * OutputDimensions * 4 + i / PaddedInputDimensions * 4
             + inputIndex % 4;
    }

    static constexpr IndexType get_weight_index(IndexType i) {
#ifdef ENABLE_SEQ_OPT
        return get_weight_index_scrambled(i);
#else
        return i;
#endif
    }

    // Read network parameters
    bool read_parameters(std::istream& stream) {
        read_little_endian<BiasType>(stream, biases, OutputDimensions);
        for (IndexType i = 0; i < OutputDimensions * PaddedInputDimensions; ++i)
            weights[get_weight_index(i)] = read_little_endian<WeightType>(stream);

        return !stream.fail();
    }

    // Write network parameters
    bool write_parameters(std::ostream& stream) const {
        write_little_endian<BiasType>(stream, biases, OutputDimensions);

        for (IndexType i = 0; i < OutputDimensions * PaddedInputDimensions; ++i)
            write_little_endian<WeightType>(stream, weights[get_weight_index(i)]);

        return !stream.fail();
    }

    usize get_content_hash() const {
        usize h = 0;
        hash_combine(h, get_raw_data_hash(biases));
        hash_combine(h, get_raw_data_hash(weights));
        hash_combine(h, get_hash_value(0));
        return h;
    }

    // Forward propagation
    void propagate(const InputType* input, OutputType* output) const {

#ifdef ENABLE_SEQ_OPT

        if constexpr (OutputDimensions > 1)
        {
    #if defined(USE_AVX512)
            using vec_t = __m512i;
        #define vec_set_32 _mm512_set1_epi32
        #define vec_add_32 _mm512_add_epi32
        #define vec_add_dpbusd_32 SIMD::m512_add_dpbusd_epi32
    #elif defined(USE_AVX2)
            using vec_t = __m256i;
        #define vec_set_32 _mm256_set1_epi32
        #define vec_add_32 _mm256_add_epi32
        #define vec_add_dpbusd_32 SIMD::m256_add_dpbusd_epi32
    #elif defined(USE_SSSE3)
            using vec_t = __m128i;
        #define vec_set_32 _mm_set1_epi32
        #define vec_add_dpbusd_32 SIMD::m128_add_dpbusd_epi32
    #elif defined(USE_NEON_DOTPROD)
            using vec_t = int32x4_t;
        #define vec_set_32 vdupq_n_s32
        #define vec_add_dpbusd_32(acc, a, b) \
            SIMD::dotprod_m128_add_dpbusd_epi32(acc, vreinterpretq_s8_s32(a), \
                                                vreinterpretq_s8_s32(b))
    #elif defined(USE_LASX)
            using vec_t = __m256i;
        #define vec_set_32 __lasx_xvreplgr2vr_w
        #define vec_add_32 __lasx_xvadd_w
        #define vec_add_dpbusd_32 SIMD::lasx_m256_add_dpbusd_epi32
    #elif defined(USE_LSX)
            using vec_t = __m128i;
        #define vec_set_32 __lsx_vreplgr2vr_w
        #define vec_add_32 __lsx_vadd_w
        #define vec_add_dpbusd_32 SIMD::lsx_m128_add_dpbusd_epi32
    #endif
    #if defined(USE_LASX)
        #define vec_load_32(a) __lasx_xvldrepl_w(reinterpret_cast<const void*>(a), 0)
    #elif defined(USE_LSX)
        #define vec_load_32(a) __lsx_vldrepl_w(reinterpret_cast<const void*>(a), 0)
    #else
        #define vec_load_32(a) vec_set_32(load_as<i32>(a))
    #endif

            static constexpr IndexType OutputSimdWidth = sizeof(vec_t) / sizeof(OutputType);

            static_assert(OutputDimensions % OutputSimdWidth == 0);

            constexpr IndexType NumChunks = ceil_to_multiple<IndexType>(InputDimensions, 8) / 4;
            constexpr IndexType NumAccums = OutputDimensions / OutputSimdWidth;

    #if defined(USE_VNNI) || defined(USE_NEON_DOTPROD)
            constexpr IndexType NumRegs = 2 * NumAccums;
    #else
            constexpr IndexType NumRegs = NumAccums;
    #endif

            const vec_t* biasvec = reinterpret_cast<const vec_t*>(biases);
            vec_t        acc[NumRegs];
            UNROLLED_FOR (IndexType k = 0; k < NumAccums; ++k)
                acc[k] = biasvec[k];
            UNROLLED_FOR (IndexType k = NumAccums; k < NumRegs; ++k)
                acc[k] = vec_set_32(0);

            IndexType i = 0;
    #if defined(USE_VNNI) || defined(USE_NEON_DOTPROD)
            for (; i < NumChunks; i += 2)
            {
                const vec_t in0 = vec_load_32(input + i * sizeof(i32));
                const vec_t in1 = vec_load_32(input + (i + 1) * sizeof(i32));
                const auto  col0 =
                  reinterpret_cast<const vec_t*>(&weights[i * OutputDimensions * 4]);
                const auto col1 =
                  reinterpret_cast<const vec_t*>(&weights[(i + 1) * OutputDimensions * 4]);

                UNROLLED_FOR (IndexType k = 0; k < NumAccums; ++k)
                {
                    vec_add_dpbusd_32(acc[k], in0, col0[k]);
                    vec_add_dpbusd_32(acc[k + NumAccums], in1, col1[k]);
                }
            }

            UNROLLED_FOR (IndexType k = 0; k < NumAccums; ++k)
        #if defined(USE_NEON_DOTPROD)
                acc[k] = vaddq_s32(acc[k], acc[k + NumAccums]);
        #else
                acc[k] = vec_add_32(acc[k], acc[k + NumAccums]);
        #endif
    #endif
            for (; i < NumChunks; ++i)
            {
                const vec_t in0 = vec_load_32(input + i * sizeof(i32));
                const auto  col0 =
                  reinterpret_cast<const vec_t*>(&weights[i * OutputDimensions * 4]);

                UNROLLED_FOR (IndexType k = 0; k < NumAccums; ++k)
                    vec_add_dpbusd_32(acc[k], in0, col0[k]);
            }

            vec_t* outptr = reinterpret_cast<vec_t*>(output);
            UNROLLED_FOR (IndexType k = 0; k < NumAccums; ++k)
                outptr[k] = acc[k];

    #undef vec_set_32
    #undef vec_load_32
    #undef vec_add_dpbusd_32
        }
        else if constexpr (OutputDimensions == 1)
        {
    #if defined(USE_AVX2)
            using vec_t = __m256i;
        #define vec_setzero() _mm256_setzero_si256()
        #define vec_add_dpbusd_32 SIMD::m256_add_dpbusd_epi32
        #define vec_hadd SIMD::m256_hadd
    #elif defined(USE_SSSE3)
            using vec_t = __m128i;
        #define vec_setzero() _mm_setzero_si128()
        #define vec_add_dpbusd_32 SIMD::m128_add_dpbusd_epi32
        #define vec_hadd SIMD::m128_hadd
    #elif defined(USE_NEON_DOTPROD)
            using vec_t = int32x4_t;
        #define vec_setzero() vdupq_n_s32(0)
        #define vec_add_dpbusd_32(acc, a, b) \
            SIMD::dotprod_m128_add_dpbusd_epi32(acc, vreinterpretq_s8_s32(a), \
                                                vreinterpretq_s8_s32(b))
        #define vec_hadd SIMD::neon_m128_hadd
    #elif defined(USE_LASX)
            using vec_t = __m256i;
        #define vec_setzero() __lasx_xvldi(0)
        #define vec_add_dpbusd_32 SIMD::lasx_m256_add_dpbusd_epi32
        #define vec_hadd SIMD::lasx_m256_hadd
    #elif defined(USE_LSX)
            using vec_t = __m128i;
        #define vec_setzero() __lsx_vldi(0)
        #define vec_add_dpbusd_32 SIMD::lsx_m128_add_dpbusd_epi32
        #define vec_hadd SIMD::lsx_m128_hadd
    #endif

            const auto inputVector = reinterpret_cast<const vec_t*>(input);

            static constexpr IndexType InputSimdWidth = sizeof(vec_t) / sizeof(InputType);

            static_assert(PaddedInputDimensions % InputSimdWidth == 0);

            constexpr IndexType NumChunks = PaddedInputDimensions / InputSimdWidth;
            vec_t               sum0      = vec_setzero();
            const auto          row0      = reinterpret_cast<const vec_t*>(&weights[0]);

            for (int j = 0; j < int(NumChunks); ++j)
            {
                const vec_t in = inputVector[j];
                vec_add_dpbusd_32(sum0, in, row0[j]);
            }
            output[0] = vec_hadd(sum0, biases[0]);

    #undef vec_setzero
    #undef vec_add_dpbusd_32
    #undef vec_hadd
        }
#elif defined(USE_RVV)
        const i8* wIt = weights;

    #define RVV_PROPAGATE_SINGLE(m2, m1) \
        do \
        { \
            vint32m1_t     zero = __riscv_vmv_s_x_i32m1(0, 1); \
            usize          vl   = __riscv_vsetvl_e8##m1(InputDimensions); \
            vuint8##m1##_t in   = __riscv_vle8_v_u8##m1(input, vl); \
            for (IndexType i = 0; i < OutputDimensions; ++i, wIt += PaddedInputDimensions) \
            { \
                vint8##m1##_t  w    = __riscv_vle8_v_i8##m1(wIt, vl); \
                vint16##m2##_t prod = __riscv_vwmulsu(w, in, vl); \
                output[i]           = biases[i] + __riscv_vmv_x(__riscv_vwredsum(prod, zero, vl)); \
            } \
        } while (0)

        static usize VL1 = __riscv_vsetvlmax_e16m1();
        if (InputDimensions <= VL1)
            RVV_PROPAGATE_SINGLE(m1, mf2);
        else if (InputDimensions <= VL1 * 2)
            RVV_PROPAGATE_SINGLE(m2, m1);
        else if (InputDimensions <= VL1 * 4)
            RVV_PROPAGATE_SINGLE(m4, m2);
        else
        {
            for (IndexType i = 0; i < OutputDimensions; ++i, wIt += PaddedInputDimensions)
            {
                vint32m1_t sum = __riscv_vmv_s_x_i32m1(0, 1);
                for (IndexType vl, j = 0; j < InputDimensions; j += vl)
                {
                    vl              = __riscv_vsetvl_e8m2(InputDimensions - j);
                    vuint8m2_t in   = __riscv_vle8_v_u8m2(input + j, vl);
                    vint8m2_t  w    = __riscv_vle8_v_i8m2(wIt + j, vl);
                    vint16m4_t prod = __riscv_vwmulsu(w, in, vl);
                    sum             = __riscv_vwredsum(prod, sum, vl);
                }
                output[i] = biases[i] + __riscv_vmv_x(sum);
            }
        }

    #undef RVV_PROPAGATE_SINGLE
#else
        // Use old implementation for the other architectures.
        affine_transform_non_ssse3<InputDimensions, PaddedInputDimensions, OutputDimensions>(
          output, weights, biases, input);
#endif
    }

   private:
    using BiasType   = OutputType;
    using WeightType = i8;

    alignas(CacheLineSize) BiasType biases[OutputDimensions];
    alignas(CacheLineSize) WeightType weights[OutputDimensions * PaddedInputDimensions];
};

}  // namespace Stockfish::Eval::NNUE::Layers

#endif  // #ifndef NNUE_LAYERS_AFFINE_TRANSFORM_H_INCLUDED

// === sf_src/src/nnue/layers/affine_transform_sparse_input.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Definition of layer AffineTransformSparseInput of NNUE evaluation function

#ifndef NNUE_LAYERS_AFFINE_TRANSFORM_SPARSE_INPUT_H_INCLUDED
#define NNUE_LAYERS_AFFINE_TRANSFORM_SPARSE_INPUT_H_INCLUDED

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>


// === sf_src/src/nnue/nnz_helper.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NNZ_HELPER_H_INCLUDED
#define NNZ_HELPER_H_INCLUDED

#include <utility>


namespace Stockfish::Eval::NNUE {

template<usize Dimensions>
struct NNZInfo {

#if defined(USE_AVX512)
    unsigned count = 0;
    // indices of non-zero chunks
    u16 nnz[Dimensions / 4];

    #ifdef USE_AVX512ICL
    alignas(64) static constexpr auto Indices = []() {
        std::array<std::array<u16, 32>, 2> indices{};
        for (int i = 0; i < 2; ++i)
        {
            indices[i] = {0, 1, 2,  3,  16, 17, 18, 19, 4,  5,  6,  7,  20, 21, 22, 23,
                          8, 9, 10, 11, 24, 25, 26, 27, 12, 13, 14, 15, 28, 29, 30, 31};
            for (u16& m : indices[i])
                m += u16(i * Dimensions / 8);
        }
        return indices;
    }();
    #else
    alignas(64) static constexpr auto Indices = []() {
        std::array<std::array<u32, 16>, 2> indices{};
        for (int i = 0; i < 2; ++i)
        {
            indices[i] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
            for (u32& m : indices[i])
                m += i * Dimensions / 8;
        }
        return indices;
    }();
    #endif

    struct NNZCursor {
        NNZInfo& info;
        __m512i  indices;
        unsigned count;

        NNZCursor(NNZInfo& info_, bool perspective, unsigned count_) :
            info(info_),
            count(count_) {
            indices = _mm512_load_si512(&Indices[perspective]);
        }

        void record2(SIMD::vec_t neurons1, SIMD::vec_t neurons2) {
    #if defined(USE_AVX512ICL)
            const __m512i increment = _mm512_set1_epi16(32);

            // Get a bitmask and gather non zero indices
            const __m512i   inputV01 = _mm512_packs_epi32(neurons1, neurons2);
            const __mmask32 nnzMask  = _mm512_test_epi16_mask(inputV01, inputV01);

            // Avoid _mm512_mask_compressstoreu_epi16() as it's 256 uOps on Zen4
            __m512i nnzIndices = _mm512_maskz_compress_epi16(nnzMask, indices);
            _mm512_storeu_si512(info.nnz + count, nnzIndices);

            count += popcount(nnzMask);
            indices = _mm512_add_epi16(indices, increment);
    #else
            const __m512i increment = _mm512_set1_epi32(16);

            for (auto neurons : {neurons1, neurons2})
            {
                // Get a bitmask and gather non zero indices
                const __mmask16 nnzMask = _mm512_test_epi32_mask(neurons, neurons);
                const __m512i   nnzV    = _mm512_maskz_compress_epi32(nnzMask, indices);
                _mm512_mask_cvtepi32_storeu_epi16(info.nnz + count, 0xFFFF, nnzV);

                count += popcount(nnzMask);
                indices = _mm512_add_epi32(indices, increment);
            }
    #endif
        }

        ~NNZCursor() { info.count = count; }
    };

    NNZCursor make_cursor(bool perspective) { return {*this, perspective, count}; }
#elif defined(USE_RVV)
    usize count = 0;
    u16   nnz[Dimensions];  // indices of non-zero chunks
#else
    // Each 8-bit chunk
    alignas(8) u8 bitset[(Dimensions + 31) / 32];

    struct NNZCursor {
        u8* out;

        NNZCursor(NNZInfo& info, bool perspective) {
            out = info.bitset + perspective * Dimensions / 64;
        }

    #if (defined(USE_SSSE3) || defined(USE_LSX) || defined(USE_LASX) || (USE_NEON >= 8))
        void record2(SIMD::vec_t neurons1, SIMD::vec_t neurons2) {
            using namespace SIMD;

        #ifdef USE_NEON
            alignas(16) static constexpr u16 Mask8[8] = {1, 16, 2, 32, 4, 64, 8, 128};

            uint32x4_t n1 = vreinterpretq_u32_s16(neurons1);
            uint32x4_t n2 = vreinterpretq_u32_s16(neurons2);

            const uint32x4_t t1 = vtstq_u32(n1, n1);
            const uint32x4_t t2 = vtstq_u32(n2, n2);

            const uint16x8_t packed =
              vtrn1q_u16(vreinterpretq_u16_u32(t1), vreinterpretq_u16_u32(t2));
            const uint16x8_t bits = vandq_u16(packed, vld1q_u16(Mask8));

            *out++ = vaddvq_u16(bits);
        #elif defined(__wasm__)
            __m128i packed = _mm_packus_epi32(neurons1, neurons2);
            packed         = _mm_packs_epi16(packed, packed);
            *out++         = ~_mm_movemask_epi8(_mm_cmpeq_epi8(packed, _mm_setzero_si128()));
        #else
            auto m1 = vec_nnz(neurons1);
            auto m2 = vec_nnz(neurons2);

            if (sizeof(neurons1) == 16)
            {
                *out++ = m1 + (m2 << 4);
            }
            else
            {
                usize bytes = sizeof(neurons1) / 32;
                memcpy(out, &m1, bytes);
                out += bytes;
                memcpy(out, &m2, bytes);
                out += bytes;
            }
        #endif
        }
    #elif defined(VECTOR)
        void record2(SIMD::vec_t, SIMD::vec_t) {}
    #endif
    };

    NNZCursor make_cursor(bool perspective) { return {*this, perspective}; }
#endif
};
}  // namespace Stockfish::Eval::NNUE

#endif

/*
  This file contains the definition for a fully connected layer (aka affine transform) with block sparse input.
*/

namespace Stockfish::Eval::NNUE::Layers {

// Sparse input implementation
template<IndexType InDims, IndexType OutDims>
class AffineTransformSparseInput {
   public:
    // Input/output type
    using InputType  = u8;
    using OutputType = i32;

    // Number of input/output dimensions
    static constexpr IndexType InputDimensions  = InDims;
    static constexpr IndexType OutputDimensions = OutDims;

    static_assert(OutputDimensions % 16 == 0,
                  "Only implemented for OutputDimensions divisible by 16.");

    static constexpr IndexType PaddedInputDimensions =
      ceil_to_multiple<IndexType>(InputDimensions, MaxSimdWidth);
    static constexpr IndexType PaddedOutputDimensions =
      ceil_to_multiple<IndexType>(OutputDimensions, MaxSimdWidth);

#if (defined(USE_SSSE3) || defined(USE_LSX) || defined(USE_LASX) || (USE_NEON >= 8))
    static constexpr IndexType ChunkSize = 4;
#else
    static constexpr IndexType ChunkSize = 1;
#endif

    using OutputBuffer = OutputType[PaddedOutputDimensions];

    // Hash value embedded in the evaluation file
    static constexpr u32 get_hash_value(u32 prevHash) {
        u32 hashValue = 0xCC03DAE4u;
        hashValue += OutputDimensions;
        hashValue ^= prevHash >> 1;
        hashValue ^= prevHash << 31;
        return hashValue;
    }

    static constexpr IndexType get_weight_index_scrambled(IndexType i) {
        return (i / ChunkSize) % (PaddedInputDimensions / ChunkSize) * OutputDimensions * ChunkSize
             + i / PaddedInputDimensions * ChunkSize + i % ChunkSize;
    }

    static constexpr IndexType get_weight_index(IndexType i) {
#if (defined(USE_SSSE3) || defined(USE_LSX) || defined(USE_LASX) || (USE_NEON >= 8) \
     || defined(USE_RVV))
        return get_weight_index_scrambled(i);
#else
        return i;
#endif
    }

    // Read network parameters
    bool read_parameters(std::istream& stream) {
        read_little_endian<BiasType>(stream, biases, OutputDimensions);
        for (IndexType i = 0; i < OutputDimensions * PaddedInputDimensions; ++i)
            weights[get_weight_index(i)] = read_little_endian<WeightType>(stream);

        return !stream.fail();
    }

    // Write network parameters
    bool write_parameters(std::ostream& stream) const {
        write_little_endian<BiasType>(stream, biases, OutputDimensions);

        for (IndexType i = 0; i < OutputDimensions * PaddedInputDimensions; ++i)
            write_little_endian<WeightType>(stream, weights[get_weight_index(i)]);

        return !stream.fail();
    }

    usize get_content_hash() const {
        usize h = 0;
        hash_combine(h, get_raw_data_hash(biases));
        hash_combine(h, get_raw_data_hash(weights));
        hash_combine(h, get_hash_value(0));
        return h;
    }

    // Forward propagation
    void propagate(const InputType*                        input,
                   OutputType*                             output,
                   [[maybe_unused]] const NNZInfo<InDims>& nnzInfo) const {

#if (defined(USE_SSSE3) || defined(USE_LSX) || defined(USE_LASX) || (USE_NEON >= 8))
    #if defined(USE_AVX512)
        using invec_t  = __m512i;
        using outvec_t = __m512i;
        #define vec_add_32 _mm512_add_epi32
        #define vec_set_32 _mm512_set1_epi32
        #define vec_add_dpbusd_32 SIMD::m512_add_dpbusd_epi32
    #elif defined(USE_AVX2)
        using invec_t  = __m256i;
        using outvec_t = __m256i;
        #define vec_add_32 _mm256_add_epi32
        #define vec_set_32 _mm256_set1_epi32
        #define vec_add_dpbusd_32 SIMD::m256_add_dpbusd_epi32
    #elif defined(USE_SSSE3)
        using invec_t  = __m128i;
        using outvec_t = __m128i;
        #define vec_set_32 _mm_set1_epi32
        #define vec_add_dpbusd_32 SIMD::m128_add_dpbusd_epi32
    #elif defined(USE_NEON_DOTPROD)
        using invec_t  = int8x16_t;
        using outvec_t = int32x4_t;
        #define vec_set_32(a) vreinterpretq_s8_u32(vdupq_n_u32(a))
        #define vec_add_dpbusd_32 SIMD::dotprod_m128_add_dpbusd_epi32
    #elif defined(USE_NEON)
        using invec_t  = int8x16_t;
        using outvec_t = int32x4_t;
        #define vec_set_32(a) vreinterpretq_s8_u32(vdupq_n_u32(a))
        #define vec_add_dpbusd_32 SIMD::neon_m128_add_dpbusd_epi32
    #elif defined(USE_LASX)
        using invec_t  = __m256i;
        using outvec_t = __m256i;
        #define vec_add_32 __lasx_xvadd_w
        #define vec_set_32 __lasx_xvreplgr2vr_w
        #define vec_add_dpbusd_32 SIMD::lasx_m256_add_dpbusd_epi32
    #elif defined(USE_LSX)
        using invec_t  = __m128i;
        using outvec_t = __m128i;
        #define vec_add_32 __lsx_vadd_w
        #define vec_set_32 __lsx_vreplgr2vr_w
        #define vec_add_dpbusd_32 SIMD::lsx_m128_add_dpbusd_epi32
    #endif
    #if defined(USE_LASX)
        #define vec_load_32(a) __lasx_xvldrepl_w(reinterpret_cast<const void*>(a), 0)
    #elif defined(USE_LSX)
        #define vec_load_32(a) __lsx_vldrepl_w(reinterpret_cast<const void*>(a), 0)
    #else
        #define vec_load_32(a) vec_set_32(load_as<i32>(a))
    #endif
        constexpr IndexType OutputSimdWidth = sizeof(outvec_t) / sizeof(OutputType);
        constexpr IndexType NumAccums       = OutputDimensions / OutputSimdWidth;
        // If we're using high-latency dot product instructions, split the accumulators
        // into separate dependency chains and merge at the end
        constexpr IndexType NumRegs =
    #if (defined(USE_VNNI) && defined(USE_AVX512)) || defined(USE_NEON_DOTPROD)
          3 * NumAccums;
    #elif defined(USE_AVXVNNI)
          2 * NumAccums;
    #else
          NumAccums;
    #endif

        const outvec_t* biasvec = reinterpret_cast<const outvec_t*>(biases);
        outvec_t        acc[NumRegs];
        UNROLLED_FOR (IndexType k = 0; k < NumAccums; ++k)
            acc[k] = biasvec[k];

    #if defined(USE_AVXVNNI)
        UNROLLED_FOR (IndexType k = NumAccums; k < NumRegs; ++k)
            acc[k] = vec_set_32(0);
    #elif defined(USE_NEON_DOTPROD)
        UNROLLED_FOR (IndexType k = NumAccums; k < NumRegs; ++k)
            acc[k] = vdupq_n_s32(0);
    #endif

        // convince GCC to not do weird pointer arithmetic in the following loops
        const i8* weights_cp = weights;

    #if defined(USE_AVX512)
        const auto* start = nnzInfo.nnz;
        const auto* end   = nnzInfo.nnz + nnzInfo.count;

        for (IndexType k = NumAccums; k < NumRegs; ++k)
            acc[k] = vec_zero();
        #if defined(USE_VNNI)
        while (start < end - 2)
        {
            const isize   i0  = *start++;
            const isize   i1  = *start++;
            const isize   i2  = *start++;
            const invec_t in0 = vec_load_32(input + i0 * sizeof(i32));
            const invec_t in1 = vec_load_32(input + i1 * sizeof(i32));
            const invec_t in2 = vec_load_32(input + i2 * sizeof(i32));
            const auto    col0 =
              reinterpret_cast<const invec_t*>(&weights_cp[i0 * OutputDimensions * ChunkSize]);
            const auto col1 =
              reinterpret_cast<const invec_t*>(&weights_cp[i1 * OutputDimensions * ChunkSize]);
            const auto col2 =
              reinterpret_cast<const invec_t*>(&weights_cp[i2 * OutputDimensions * ChunkSize]);
            UNROLLED_FOR (IndexType k = 0; k < NumAccums; ++k)
            {
                vec_add_dpbusd_32(acc[k], in0, col0[k]);
                vec_add_dpbusd_32(acc[k + NumAccums], in1, col1[k]);
                vec_add_dpbusd_32(acc[k + 2 * NumAccums], in2, col2[k]);
            }
        }

        for (IndexType k = 0; k < NumAccums; ++k)
            acc[k] = vec_add_32(vec_add_32(acc[k], acc[k + NumAccums]), acc[k + 2 * NumAccums]);
        #endif

        while (start < end)
        {
            const isize   i  = *start++;
            const invec_t in = vec_load_32(input + i * sizeof(i32));
            const auto    col =
              reinterpret_cast<const invec_t*>(&weights_cp[i * OutputDimensions * ChunkSize]);
            UNROLLED_FOR (IndexType k = 0; k < NumAccums; ++k)
                vec_add_dpbusd_32(acc[k], in, col[k]);
        }
    #else
        static_assert(InputDimensions % 256 == 0);

        for (IndexType k = 0; k < InputDimensions / 256; ++k)
        {
            u64   bits = load_as<u64>(nnzInfo.bitset + k * 8);
            isize base = k * 64;

            auto* base_addr    = input + base * sizeof(i32);
            auto* weights_base = &weights_cp[base * OutputDimensions * ChunkSize];

        #if defined(USE_NEON_DOTPROD) && defined(__GNUC__) && !defined(__clang__)
            // GCC 15 pessimizes the following code on ARM64 by eliding the intermediate
            // computation of key pointers (base_addr, weights_base, col, input_addr), leading
            // to a lot of redundant indexing arithmetic in the while (bits) loop. The
            // optimization barriers force these pointers to be calculated and used.
            #if __GNUC__ >= 15
                #define FIX_GCC15_MISOPTIMIZATION
            #endif
        #endif

        #ifdef FIX_GCC15_MISOPTIMIZATION
            asm("" : "+r"(base_addr), "+r"(weights_base));  // opt barrier
        #endif

        #if defined(USE_AVXVNNI)
            while (bits)
            {
                const isize   i0   = pop_lsb(bits);
                const invec_t in0  = vec_load_32(base_addr + i0 * sizeof(i32));
                const auto    col0 = reinterpret_cast<const invec_t*>(
                  &weights_base[i0 * OutputDimensions * ChunkSize]);

                if (!bits)
                {
                    for (IndexType l = 0; l < NumAccums; ++l)
                        vec_add_dpbusd_32(acc[l], in0, col0[l]);
                    break;
                }

                const isize   i1   = pop_lsb(bits);
                const invec_t in1  = vec_load_32(base_addr + i1 * sizeof(i32));
                const auto    col1 = reinterpret_cast<const invec_t*>(
                  &weights_base[i1 * OutputDimensions * ChunkSize]);

                UNROLLED_FOR (IndexType l = 0; l < NumAccums; ++l)
                {
                    vec_add_dpbusd_32(acc[l], in0, col0[l]);
                    vec_add_dpbusd_32(acc[l + NumAccums], in1, col1[l]);
                }
            }
        #elif defined(USE_NEON_DOTPROD)
            while (bits)
            {
                const isize i0 = pop_lsb(bits);
                if (!bits)
                {
                    const invec_t in0  = vec_load_32(base_addr + i0 * sizeof(i32));
                    const auto    col0 = reinterpret_cast<const invec_t*>(
                      &weights_base[i0 * OutputDimensions * ChunkSize]);
                    UNROLLED_FOR (IndexType l = 0; l < NumAccums; ++l)
                        vec_add_dpbusd_32(acc[l], in0, col0[l]);
                    break;
                }

                const isize i1 = pop_lsb(bits);
                if (!bits)
                {
                    const invec_t in0  = vec_load_32(base_addr + i0 * sizeof(i32));
                    const invec_t in1  = vec_load_32(base_addr + i1 * sizeof(i32));
                    const auto    col0 = reinterpret_cast<const invec_t*>(
                      &weights_base[i0 * OutputDimensions * ChunkSize]);
                    const auto col1 = reinterpret_cast<const invec_t*>(
                      &weights_base[i1 * OutputDimensions * ChunkSize]);
                    UNROLLED_FOR (IndexType l = 0; l < NumAccums; ++l)
                    {
                        vec_add_dpbusd_32(acc[l], in0, col0[l]);
                        vec_add_dpbusd_32(acc[l + NumAccums], in1, col1[l]);
                    }
                    break;
                }

                const isize   i2   = pop_lsb(bits);
                const invec_t in0  = vec_load_32(base_addr + i0 * sizeof(i32));
                const invec_t in1  = vec_load_32(base_addr + i1 * sizeof(i32));
                const invec_t in2  = vec_load_32(base_addr + i2 * sizeof(i32));
                const auto    col0 = reinterpret_cast<const invec_t*>(
                  &weights_base[i0 * OutputDimensions * ChunkSize]);
                const auto col1 = reinterpret_cast<const invec_t*>(
                  &weights_base[i1 * OutputDimensions * ChunkSize]);
                const auto col2 = reinterpret_cast<const invec_t*>(
                  &weights_base[i2 * OutputDimensions * ChunkSize]);
                UNROLLED_FOR (IndexType l = 0; l < NumAccums; ++l)
                {
                    vec_add_dpbusd_32(acc[l], in0, col0[l]);
                    vec_add_dpbusd_32(acc[l + NumAccums], in1, col1[l]);
                    vec_add_dpbusd_32(acc[l + 2 * NumAccums], in2, col2[l]);
                }
            }
        #else
            while (bits)
            {
                isize       i          = pop_lsb(bits);
                const auto* input_addr = base_addr + i * sizeof(i32);
                auto        col =
                  reinterpret_cast<const invec_t*>(&weights_base[i * OutputDimensions * ChunkSize]);

            #ifdef FIX_GCC15_MISOPTIMIZATION
                asm("" : "+r"(col), "+r"(input_addr));
                #undef FIX_GCC15_MISOPTIMIZATION
            #endif

                const invec_t in = vec_load_32(input_addr);
                UNROLLED_FOR (IndexType l = 0; l < NumAccums; ++l)
                    vec_add_dpbusd_32(acc[l], in, col[l]);
            }
        #endif
        }

        #if defined(USE_AVXVNNI)
        UNROLLED_FOR (IndexType l = 0; l < NumAccums; ++l)
            acc[l] = vec_add_32(acc[l], acc[l + NumAccums]);
        #elif defined(USE_NEON_DOTPROD)
        UNROLLED_FOR (IndexType l = 0; l < NumAccums; ++l)
            acc[l] = vaddq_s32(vaddq_s32(acc[l], acc[l + NumAccums]), acc[l + 2 * NumAccums]);
        #endif
    #endif
        outvec_t* outptr = reinterpret_cast<outvec_t*>(output);
        UNROLLED_FOR (IndexType k = 0; k < NumAccums; ++k)
            outptr[k] = acc[k];

    #undef vec_set_32
    #undef vec_load_32
    #undef vec_add_dpbusd_32
    #ifdef vec_add_32
        #undef vec_add_32
    #endif
#elif defined(USE_RVV)
        // we don't support larger for now
        static_assert(OutputDimensions <= 128 * 8 / 32);

    #define RVV_SPARSE_PROPAGATE(m1, m2, m4) \
        do \
        { \
            usize          vl  = OutputDimensions; \
            vint32##m4##_t acc = __riscv_vle32_v_i32##m4(biases, vl); \
            IndexType      i   = 0; \
            for (; i + 2 <= nnzInfo.count; i += 2) \
            { \
                usize         idx0 = nnzInfo.nnz[i + 0]; \
                usize         idx1 = nnzInfo.nnz[i + 1]; \
                uint8_t       in0  = input[idx0]; \
                uint8_t       in1  = input[idx1]; \
                vint8##m1##_t w0   = __riscv_vle8_v_i8##m1(&weights[idx0 * OutputDimensions], vl); \
                vint8##m1##_t w1   = __riscv_vle8_v_i8##m1(&weights[idx1 * OutputDimensions], vl); \
                /* input is in [0:127], weights is in [-128:127], \
                 * so it's safe to accumulate into 16-bit twice */ \
                vint16##m2##_t prod = __riscv_vwmulsu(w0, in0, vl); \
                prod                = __riscv_vwmaccus(prod, in1, w1, vl); \
                acc                 = __riscv_vwadd_wv(acc, prod, vl); \
            } \
            if (i < nnzInfo.count) \
            { \
                usize          idx  = nnzInfo.nnz[i]; \
                uint8_t        in   = input[idx]; \
                vint8##m1##_t  w    = __riscv_vle8_v_i8##m1(&weights[idx * OutputDimensions], vl); \
                vint16##m2##_t prod = __riscv_vwmulsu(w, in, vl); \
                acc                 = __riscv_vwadd_wv(acc, prod, vl); \
            } \
            __riscv_vse32(output, acc, vl); \
        } while (0)

        // Select LMUL
        const usize VL1 = __riscv_vsetvlmax_e32m1();
        if (VL1 >= OutputDimensions)
            RVV_SPARSE_PROPAGATE(mf4, mf2, m1);
        else if (VL1 * 2 >= OutputDimensions)
            RVV_SPARSE_PROPAGATE(mf2, m1, m2);
        else if (VL1 * 4 >= OutputDimensions)
            RVV_SPARSE_PROPAGATE(m1, m2, m4);
        else
            RVV_SPARSE_PROPAGATE(m2, m4, m8);

    #undef RVV_SPARSE_PROPAGATE
#else
        // Use dense implementation for the other architectures.
        affine_transform_non_ssse3<InputDimensions, PaddedInputDimensions, OutputDimensions>(
          output, weights, biases, input);
#endif
    }

   private:
    using BiasType   = OutputType;
    using WeightType = i8;

    alignas(CacheLineSize) BiasType biases[OutputDimensions];
    alignas(CacheLineSize) WeightType weights[OutputDimensions * PaddedInputDimensions];
};

}  // namespace Stockfish::Eval::NNUE::Layers

#endif  // #ifndef NNUE_LAYERS_AFFINE_TRANSFORM_SPARSE_INPUT_H_INCLUDED

// === sf_src/src/nnue/layers/clipped_relu.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Definition of layer ClippedReLU of NNUE evaluation function

#ifndef NNUE_LAYERS_CLIPPED_RELU_H_INCLUDED
#define NNUE_LAYERS_CLIPPED_RELU_H_INCLUDED

#include <algorithm>
#include <cstdint>
#include <iosfwd>


namespace Stockfish::Eval::NNUE::Layers {

// Clipped ReLU
template<IndexType InDims, int WeightScaleBitsLocal = WeightScaleBits>
class ClippedReLU {
   public:
    // Input/output type
    using InputType  = i32;
    using OutputType = u8;

    // Number of input/output dimensions
    static constexpr IndexType InputDimensions  = InDims;
    static constexpr IndexType OutputDimensions = InputDimensions;
    static constexpr IndexType PaddedOutputDimensions =
      ceil_to_multiple<IndexType>(OutputDimensions, 32);

    using OutputBuffer = OutputType[PaddedOutputDimensions];

    // Hash value embedded in the evaluation file
    static constexpr u32 get_hash_value(u32 prevHash) {
        u32 hashValue = 0x538D24C7u;
        hashValue += prevHash;
        // TODO: consider including WeightScaleBitsLocal in the hash value.
        // For now omitted on purpose because not written by trainer (yet)
        return hashValue;
    }

    // Read network parameters
    bool read_parameters(std::istream&) { return true; }

    // Write network parameters
    bool write_parameters(std::ostream&) const { return true; }

    usize get_content_hash() const {
        usize h = 0;
        hash_combine(h, get_hash_value(0));
        return h;
    }

#if !defined(USE_PAIR_ACTIVATIONS)
    // Forward propagation
    void propagate(const InputType* input, OutputType* output) const {

    #if defined(USE_SSE2)
        constexpr IndexType NumChunks = InputDimensions / 16;

        #ifndef USE_SSE41
        const __m128i k0x80s = _mm_set1_epi8(-128);
        #endif

        const auto in  = reinterpret_cast<const __m128i*>(input);
        const auto out = reinterpret_cast<__m128i*>(output);
        for (IndexType i = 0; i < NumChunks; ++i)
        {
        #if defined(USE_SSE41)
            const __m128i words0 = _mm_srli_epi16(
              _mm_packus_epi32(_mm_load_si128(&in[i * 4 + 0]), _mm_load_si128(&in[i * 4 + 1])),
              WeightScaleBitsLocal);
            const __m128i words1 = _mm_srli_epi16(
              _mm_packus_epi32(_mm_load_si128(&in[i * 4 + 2]), _mm_load_si128(&in[i * 4 + 3])),
              WeightScaleBitsLocal);
            _mm_store_si128(&out[i], _mm_packs_epi16(words0, words1));
        #else
            const __m128i words0 = _mm_srai_epi16(
              _mm_packs_epi32(_mm_load_si128(&in[i * 4 + 0]), _mm_load_si128(&in[i * 4 + 1])),
              WeightScaleBitsLocal);
            const __m128i words1 = _mm_srai_epi16(
              _mm_packs_epi32(_mm_load_si128(&in[i * 4 + 2]), _mm_load_si128(&in[i * 4 + 3])),
              WeightScaleBitsLocal);
            const __m128i packedbytes = _mm_packs_epi16(words0, words1);
            _mm_store_si128(&out[i], _mm_subs_epi8(_mm_adds_epi8(packedbytes, k0x80s), k0x80s));
        #endif
        }
        constexpr IndexType Start = NumChunks * 16;

    #elif defined(USE_NEON)
        constexpr IndexType    NumChunks = InputDimensions / 8;
        const SIMD::vec_i8x8_t Zero      = {0};
        const auto             in        = reinterpret_cast<const SIMD::vec_i32x4_t*>(input);
        const auto             out       = reinterpret_cast<SIMD::vec_i8x8_t*>(output);
        for (IndexType i = 0; i < NumChunks; ++i)
        {
            int16x8_t  shifted;
            const auto pack = reinterpret_cast<int16x4_t*>(&shifted);
            pack[0]         = vqshrn_n_s32(in[i * 2 + 0], WeightScaleBitsLocal);
            pack[1]         = vqshrn_n_s32(in[i * 2 + 1], WeightScaleBitsLocal);
            out[i]          = vmax_s8(vqmovn_s16(shifted), Zero);
        }
        constexpr IndexType Start = NumChunks * 8;

    #elif defined(USE_LASX)
        constexpr IndexType NumChunks = InputDimensions / 32;
        const auto          in        = reinterpret_cast<const __m256i*>(input);
        const auto          out       = reinterpret_cast<__m256i*>(output);
        for (IndexType i = 0; i < NumChunks; ++i)
        {
            const __m256i packed0 = SIMD::lasx_packus_32(in[i * 4 + 0], in[i * 4 + 1]);
            const __m256i packed1 = SIMD::lasx_packus_32(in[i * 4 + 2], in[i * 4 + 3]);
            const __m256i packed  = __lasx_xvssrlni_b_h(packed1, packed0, WeightScaleBitsLocal);
            __lasx_xvst(packed, out + i, 0);
        }
        constexpr IndexType Start = NumChunks * 32;

    #elif defined(USE_LSX)
        constexpr IndexType NumChunks = InputDimensions / 16;
        const auto          in        = reinterpret_cast<const __m128i*>(input);
        const auto          out       = reinterpret_cast<__m128i*>(output);
        for (IndexType i = 0; i < NumChunks; ++i)
        {
            const __m128i packed0 = SIMD::lsx_packus_32(in[i * 4 + 0], in[i * 4 + 1]);
            const __m128i packed1 = SIMD::lsx_packus_32(in[i * 4 + 2], in[i * 4 + 3]);
            out[i]                = __lsx_vssrlni_b_h(packed1, packed0, WeightScaleBitsLocal);
        }
        constexpr IndexType Start = NumChunks * 16;

    #elif defined(USE_RVV)

        for (usize j = 0; j < InputDimensions;)
        {
            usize vl = __riscv_vsetvl_e32m4(InputDimensions - j);

            vint32m4_t in = __riscv_vle32_v_i32m4(&input[j], vl);
            in            = __riscv_vmax_vx_i32m4(in, 0, vl);

            vint16m2_t words =
              __riscv_vnclip_wx_i16m2(in, WeightScaleBitsLocal, __RISCV_VXRM_RDN, vl);
            vint8m1_t narrowed = __riscv_vnclip_wx_i8m1(words, 0, __RISCV_VXRM_RDN, vl);

            __riscv_vse8_v_u8m1(&output[j], __riscv_vreinterpret_v_i8m1_u8m1(narrowed), vl);
            j += vl;
        }
        constexpr IndexType Start = InputDimensions;

    #else
        constexpr IndexType Start = 0;
    #endif

        for (IndexType i = Start; i < InputDimensions; ++i)
        {
            output[i] =
              static_cast<OutputType>(std::clamp(input[i] >> WeightScaleBitsLocal, 0, 127));
        }
    }
#endif
};

}  // namespace Stockfish::Eval::NNUE::Layers

#endif  // NNUE_LAYERS_CLIPPED_RELU_H_INCLUDED

// === sf_src/src/nnue/layers/sqr_clipped_relu.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Definition of layer ClippedReLU of NNUE evaluation function

#ifndef NNUE_LAYERS_SQR_CLIPPED_RELU_H_INCLUDED
#define NNUE_LAYERS_SQR_CLIPPED_RELU_H_INCLUDED

#include <algorithm>
#include <cstdint>
#include <iosfwd>


namespace Stockfish::Eval::NNUE::Layers {

// Clipped ReLU
template<IndexType InDims, int WeightScaleBitsLocal = WeightScaleBits>
class SqrClippedReLU {
   public:
    // Input/output type
    using InputType  = i32;
    using OutputType = u8;

    // Number of input/output dimensions
    static constexpr IndexType InputDimensions  = InDims;
    static constexpr IndexType OutputDimensions = InputDimensions;
    static constexpr IndexType PaddedOutputDimensions =
      ceil_to_multiple<IndexType>(OutputDimensions, 32);

    using OutputBuffer = OutputType[PaddedOutputDimensions];

    // Hash value embedded in the evaluation file
    static constexpr u32 get_hash_value(u32 prevHash) {
        u32 hashValue = 0x538D24C7u;
        hashValue += prevHash;
        // TODO: consider including WeightScaleBitsLocal in the hash value.
        // For now omitted on purpose because not written by trainer (yet)
        return hashValue;
    }

    // Read network parameters
    bool read_parameters(std::istream&) { return true; }

    // Write network parameters
    bool write_parameters(std::ostream&) const { return true; }

    usize get_content_hash() const {
        usize h = 0;
        hash_combine(h, get_hash_value(0));
        return h;
    }

#if defined(USE_PAIR_ACTIVATIONS)
    // Produce the squared and linear clipped activations together, sharing the input loads and
    // the initial signed 32-to-16-bit saturating narrowing. No shuffle is needed to fix up the
    // pack lane order because the next layer's weights were already reverse pre-permuted to match
    // by AffineTransform::get_weight_index_scrambled().
    void propagate_pair(const InputType* input, OutputType* squared, OutputType* clipped) const {
        static_assert(WeightScaleBitsLocal >= 5 && WeightScaleBitsLocal <= 8,
                      "SqrClippedReLU only support WeightScaleBitsLocal between 5 and 8");
        static_assert(InputDimensions % 32 == 0,
                      "propagate_pair() needs a tail path if L2/L3 change to multiple of 16");

        constexpr IndexType NumChunks       = InputDimensions / 32;
        constexpr int       SimdShiftAmount = WeightScaleBitsLocal * 2 + 7 - 16;

    #if defined(USE_AVX512)
        const auto in      = reinterpret_cast<const __m512i*>(input);
        auto       sqrOut  = reinterpret_cast<__m256i*>(squared);
        auto       clipOut = reinterpret_cast<__m256i*>(clipped);

        const __m512i zero = _mm512_setzero_si512();

        for (IndexType i = 0; i < NumChunks; ++i)
        {
            const __m512i words = _mm512_packs_epi32(_mm512_load_si512(&in[i * 2 + 0]),
                                                     _mm512_load_si512(&in[i * 2 + 1]));
            const __m512i sqrWords =
              _mm512_srli_epi16(_mm512_mulhi_epi16(words, words), SimdShiftAmount);
            _mm256_store_si256(&sqrOut[i], _mm512_cvtsepi16_epi8(sqrWords));

            const __m512i clipWords =
              _mm512_srli_epi16(_mm512_max_epi16(words, zero), WeightScaleBitsLocal);
            _mm256_store_si256(&clipOut[i], _mm512_cvtsepi16_epi8(clipWords));
        }

    #elif defined(USE_AVX2)
        const auto in      = reinterpret_cast<const __m256i*>(input);
        auto       sqrOut  = reinterpret_cast<__m256i*>(squared);
        auto       clipOut = reinterpret_cast<__m256i*>(clipped);

        const __m256i zero = _mm256_setzero_si256();

        for (IndexType i = 0; i < NumChunks; ++i)
        {
            const __m256i words0 = _mm256_packs_epi32(_mm256_load_si256(&in[i * 4 + 0]),
                                                      _mm256_load_si256(&in[i * 4 + 1]));
            const __m256i words1 = _mm256_packs_epi32(_mm256_load_si256(&in[i * 4 + 2]),
                                                      _mm256_load_si256(&in[i * 4 + 3]));

            const __m256i sqr0 =
              _mm256_srli_epi16(_mm256_mulhi_epi16(words0, words0), SimdShiftAmount);
            const __m256i sqr1 =
              _mm256_srli_epi16(_mm256_mulhi_epi16(words1, words1), SimdShiftAmount);
            const __m256i sqrPacked = _mm256_packs_epi16(sqr0, sqr1);
            _mm256_store_si256(&sqrOut[i], sqrPacked);

            const __m256i clip0 =
              _mm256_srli_epi16(_mm256_max_epi16(words0, zero), WeightScaleBitsLocal);
            const __m256i clip1 =
              _mm256_srli_epi16(_mm256_max_epi16(words1, zero), WeightScaleBitsLocal);
            const __m256i clipPacked = _mm256_packs_epi16(clip0, clip1);
            _mm256_store_si256(&clipOut[i], clipPacked);
        }
    #else
        #error "propagate_pair() requires AVX2 or AVX512"
    #endif
    }
#endif

#if !defined(USE_PAIR_ACTIVATIONS)
    // Forward propagation
    void propagate(const InputType* input, OutputType* output) const {
        static_assert(WeightScaleBitsLocal >= 5 && WeightScaleBitsLocal <= 8,
                      "SqrClippedReLU only support WeightScaleBitsLocal between 5 and 8");
        // After squaring we need to shift by WeightScaleBitsLocal * 2 + 7
        // MulHi strips the lower 16 bits (i.e. shift by 16) so we need to shift out the remaining.
        [[maybe_unused]] constexpr int SimdShiftAmount = WeightScaleBitsLocal * 2 + 7 - 16;

    #if defined(USE_SSE2)
        constexpr IndexType NumChunks = InputDimensions / 16;
        const auto          in        = reinterpret_cast<const __m128i*>(input);
        const auto          out       = reinterpret_cast<__m128i*>(output);
        for (IndexType i = 0; i < NumChunks; ++i)
        {
            __m128i words0 =
              _mm_packs_epi32(_mm_load_si128(&in[i * 4 + 0]), _mm_load_si128(&in[i * 4 + 1]));
            __m128i words1 =
              _mm_packs_epi32(_mm_load_si128(&in[i * 4 + 2]), _mm_load_si128(&in[i * 4 + 3]));

            words0 = _mm_srli_epi16(_mm_mulhi_epi16(words0, words0), SimdShiftAmount);
            words1 = _mm_srli_epi16(_mm_mulhi_epi16(words1, words1), SimdShiftAmount);

            _mm_store_si128(&out[i], _mm_packs_epi16(words0, words1));
        }
        constexpr IndexType Start = NumChunks * 16;

    #elif defined(USE_LASX)
        constexpr IndexType NumChunks = InputDimensions / 32;
        const auto          in        = reinterpret_cast<const __m256i*>(input);
        const auto          out       = reinterpret_cast<__m256i*>(output);
        for (IndexType i = 0; i < NumChunks; ++i)
        {
            const __m256i words0 = __lasx_xvssrani_h_w(in[i * 4 + 1], in[i * 4 + 0], 0);
            const __m256i words1 = __lasx_xvssrani_h_w(in[i * 4 + 3], in[i * 4 + 2], 0);
            const __m256i sqr0   = __lasx_xvmuh_h(words0, words0);
            const __m256i sqr1   = __lasx_xvmuh_h(words1, words1);
            const __m256i packed = __lasx_xvssrlni_b_h(sqr1, sqr0, SimdShiftAmount);
            __lasx_xvst(packed, out + i, 0);
        }
        constexpr IndexType Start = NumChunks * 32;

    #elif defined(USE_LSX)
        constexpr IndexType NumChunks = InputDimensions / 16;
        const auto          in        = reinterpret_cast<const __m128i*>(input);
        const auto          out       = reinterpret_cast<__m128i*>(output);
        for (IndexType i = 0; i < NumChunks; ++i)
        {
            const __m128i words0 = __lsx_vssrani_h_w(in[i * 4 + 1], in[i * 4 + 0], 0);
            const __m128i words1 = __lsx_vssrani_h_w(in[i * 4 + 3], in[i * 4 + 2], 0);
            const __m128i sqr0   = __lsx_vmuh_h(words0, words0);
            const __m128i sqr1   = __lsx_vmuh_h(words1, words1);
            out[i]               = __lsx_vssrlni_b_h(sqr1, sqr0, SimdShiftAmount);
        }
        constexpr IndexType Start = NumChunks * 16;

    #elif defined(USE_NEON)
        constexpr IndexType NumChunks = InputDimensions / 16;
        const auto          in        = reinterpret_cast<const int32x4_t*>(input);
        const auto          out       = reinterpret_cast<int8x16_t*>(output);
        for (IndexType i = 0; i < NumChunks; ++i)
        {
            const int16x8_t words0 =
              vcombine_s16(vqmovn_s32(in[i * 4 + 0]), vqmovn_s32(in[i * 4 + 1]));
            const int16x8_t words1 =
              vcombine_s16(vqmovn_s32(in[i * 4 + 2]), vqmovn_s32(in[i * 4 + 3]));
            // Neon needs to shift by one more since the used simd instruction does
            // `Saturating Doubling Multiply High` (doubling before shift by 16).
            const int16x8_t r0 = vshrq_n_s16(vqdmulhq_s16(words0, words0), SimdShiftAmount + 1);
            const int16x8_t r1 = vshrq_n_s16(vqdmulhq_s16(words1, words1), SimdShiftAmount + 1);

            out[i] = vcombine_s8(vqmovn_s16(r0), vqmovn_s16(r1));
        }
        constexpr IndexType Start = NumChunks * 16;

    #elif defined(USE_RVV)

        for (usize j = 0; j < InputDimensions;)
        {
            usize      vl = __riscv_vsetvl_e32m4(InputDimensions - j);
            vint32m4_t in = __riscv_vle32_v_i32m4(&input[j], vl);

            vint16m2_t words   = __riscv_vnclip_wx_i16m2(in, 0, __RISCV_VXRM_RDN, vl);
            vint16m2_t sqr     = __riscv_vmulh_vv_i16m2(words, words, vl);
            vint8m1_t narrowed = __riscv_vnclip_wx_i8m1(sqr, SimdShiftAmount, __RISCV_VXRM_RDN, vl);

            __riscv_vse8_v_u8m1(&output[j], __riscv_vreinterpret_v_i8m1_u8m1(narrowed), vl);
            j += vl;
        }
        constexpr IndexType Start = InputDimensions;

    #else
        constexpr IndexType Start = 0;
    #endif

        for (IndexType i = Start; i < InputDimensions; ++i)
        {
            output[i] = static_cast<OutputType>(
              // Really should be /127 but we need to make it fast so we right-shift
              // by an extra 7 bits instead. Needs to be accounted for in the trainer.
              std::min(127ll,
                       ((long long) (input[i]) * input[i]) >> (2 * WeightScaleBitsLocal + 7)));
        }
    }
#endif
};

}  // namespace Stockfish::Eval::NNUE::Layers

#endif  // NNUE_LAYERS_SQR_CLIPPED_RELU_H_INCLUDED

namespace Stockfish::Eval::NNUE {

// Input features used in evaluation function
using ThreatFeatureSet = Features::FullThreats;
using PairFeatureSet   = Features::PP_3Wide;
using PSQFeatureSet    = Features::K32H3;

// Number of input feature dimensions after conversion
constexpr IndexType L1 = 1024;
constexpr int       L2 = 32;
constexpr int       L3 = 32;

constexpr IndexType RawLayerStacks = 192;
constexpr IndexType LayerStacks    = RawLayerStacks;

struct NetworkArchitecture {
    static constexpr IndexType TransformedFeatureDimensions = L1;
    static constexpr int       FC_0_OUTPUTS                 = L2;
    static constexpr int       FC_1_OUTPUTS                 = L3;

    Layers::AffineTransformSparseInput<TransformedFeatureDimensions, FC_0_OUTPUTS> fc_0;
    Layers::SqrClippedReLU<FC_0_OUTPUTS, WeightScaleBits + 1>                      ac_sqr_0;
    Layers::ClippedReLU<FC_0_OUTPUTS, WeightScaleBits + 1>                         ac_0;
    Layers::AffineTransform<FC_0_OUTPUTS * 2, FC_1_OUTPUTS>                        fc_1;
    Layers::SqrClippedReLU<FC_1_OUTPUTS, WeightScaleBits>                          ac_sqr_1;
    Layers::ClippedReLU<FC_1_OUTPUTS, WeightScaleBits>                             ac_1;
    Layers::AffineTransform<FC_0_OUTPUTS * 2 + FC_1_OUTPUTS * 2, 1>                fc_2;

    // Hash value embedded in the evaluation file
    static constexpr u32 get_hash_value() {
        // input slice hash
        u32 hashValue = 0xEC42E90Du;
        hashValue ^= TransformedFeatureDimensions * 2;

        hashValue = decltype(fc_0)::get_hash_value(hashValue);
        // TODO: consider including hash value of ac_sqr_0 in the overall hash value.
        // For now omitted on purpose because hash value is not written by trainer (yet)
        hashValue = decltype(ac_0)::get_hash_value(hashValue);
        hashValue = decltype(fc_1)::get_hash_value(hashValue);
        hashValue = decltype(ac_1)::get_hash_value(hashValue);
        hashValue = decltype(fc_2)::get_hash_value(hashValue);

        return hashValue;
    }

    // Read network parameters
    bool read_parameters(std::istream& stream) {
        return fc_0.read_parameters(stream) && ac_0.read_parameters(stream)
            && fc_1.read_parameters(stream) && ac_1.read_parameters(stream)
            && fc_2.read_parameters(stream);
    }

    // Write network parameters
    bool write_parameters(std::ostream& stream) const {
        return fc_0.write_parameters(stream) && ac_0.write_parameters(stream)
            && fc_1.write_parameters(stream) && ac_1.write_parameters(stream)
            && fc_2.write_parameters(stream);
    }

    i32 propagate(const TransformedFeatureType* transformedFeatures,
                  const NNZInfo<L1>&            nnzInfo) const {
        struct alignas(CacheLineSize) Buffer {
            alignas(CacheLineSize) typename decltype(fc_0)::OutputBuffer fc_0_out;
            alignas(CacheLineSize) typename decltype(ac_sqr_0)::OutputType
              concat_buffer[ceil_to_multiple<IndexType>(FC_0_OUTPUTS * 2 + FC_1_OUTPUTS * 2, 32)];
            alignas(CacheLineSize) typename decltype(fc_1)::OutputBuffer fc_1_out;
            alignas(CacheLineSize) typename decltype(fc_2)::OutputBuffer fc_2_out;
        };

        Buffer buffer;

        fc_0.propagate(transformedFeatures, buffer.fc_0_out, nnzInfo);
#if defined(USE_PAIR_ACTIVATIONS)
        ac_sqr_0.propagate_pair(buffer.fc_0_out, buffer.concat_buffer,
                                buffer.concat_buffer + FC_0_OUTPUTS);
#else
        ac_sqr_0.propagate(buffer.fc_0_out, buffer.concat_buffer);
        ac_0.propagate(buffer.fc_0_out, buffer.concat_buffer + FC_0_OUTPUTS);
#endif

        fc_1.propagate(buffer.concat_buffer, buffer.fc_1_out);
#if defined(USE_PAIR_ACTIVATIONS)
        ac_sqr_1.propagate_pair(buffer.fc_1_out, buffer.concat_buffer + FC_0_OUTPUTS * 2,
                                buffer.concat_buffer + FC_0_OUTPUTS * 2 + FC_1_OUTPUTS);
#else
        ac_sqr_1.propagate(buffer.fc_1_out, buffer.concat_buffer + FC_0_OUTPUTS * 2);
        ac_1.propagate(buffer.fc_1_out, buffer.concat_buffer + FC_0_OUTPUTS * 2 + FC_1_OUTPUTS);
#endif

        fc_2.propagate(buffer.concat_buffer, buffer.fc_2_out);

        static_assert(FC_0_OUTPUTS >= 2);
        i32 fwdOut = buffer.fc_2_out[0];
        i32 skip_0 = buffer.fc_0_out[FC_0_OUTPUTS - 2] - buffer.fc_0_out[FC_0_OUTPUTS - 1];
        fwdOut += skip_0;

        // fwdOut is such that 1.0 is equal to HiddenOneVal*(1<<WeightScaleBits)*2 in
        // quantized form, but we want 1.0 to be equal to 600*OutputScale
        // to make overflow impossible we cast to int64_t
        constexpr i64 multiplier = 600 * OutputScale;
        constexpr i64 denominator =
          static_cast<i64>(HiddenOneVal) * static_cast<i64>(1U << WeightScaleBits) * 2;

        i32 outputValue = static_cast<i32>((static_cast<i64>(fwdOut) * multiplier) / denominator);
        return outputValue;
    }

    usize get_content_hash() const {
        usize h = 0;
        hash_combine(h, fc_0.get_content_hash());
        hash_combine(h, ac_sqr_0.get_content_hash());
        hash_combine(h, ac_0.get_content_hash());
        hash_combine(h, fc_1.get_content_hash());
        // hash_combine(h, ac_sqr_1.get_content_hash()); TODO
        hash_combine(h, ac_1.get_content_hash());
        hash_combine(h, fc_2.get_content_hash());
        hash_combine(h, get_hash_value());
        return h;
    }
};

}  // namespace Stockfish::Eval::NNUE

template<>
struct std::hash<Stockfish::Eval::NNUE::NetworkArchitecture> {
    Stockfish::usize
    operator()(const Stockfish::Eval::NNUE::NetworkArchitecture& arch) const noexcept {
        return arch.get_content_hash();
    }
};

#endif  // #ifndef NNUE_ARCHITECTURE_H_INCLUDED

// === sf_src/src/nnue/nnue_feature_transformer.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// A class that converts the input features of the NNUE evaluation function

#ifndef NNUE_FEATURE_TRANSFORMER_H_INCLUDED
#define NNUE_FEATURE_TRANSFORMER_H_INCLUDED

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <iterator>


// === sf_src/src/position.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef POSITION_H_INCLUDED
#define POSITION_H_INCLUDED

#include <array>
#include <cassert>
#include <deque>
#include <iosfwd>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>


namespace Stockfish {

struct SharedHistories;

// StateInfo struct stores information needed to restore a Position object to
// its previous state when we retract a move. Whenever a move is made on the
// board (by calling Position::do_move), a StateInfo object must be passed.

struct StateInfo {

    // Copied when making a move
    Key    materialKey;
    Key    pawnKey;
    Key    minorPieceKey;
    Key    nonPawnKey[COLOR_NB];
    Value  nonPawnMaterial[COLOR_NB];
    int    castlingRights;
    int    rule50;
    int    pliesFromNull;
    Square epSquare;

    // Not copied when making a move (will be recomputed anyhow)
    Key                key;
    Bitboard           checkersBB;
    StateInfo*         previous;
    Bitboard           blockersForKing[COLOR_NB];
    Bitboard           pinners[COLOR_NB];
    Bitboard           checkSquares[PIECE_TYPE_NB];
    Bitboard           threats[PIECE_TYPE_NB];
    Piece              capturedPiece;
    int                repetition;
    std::array<u8, 32> piecePlacement;
};


// A list to keep track of the position states along the setup moves (from the
// start position to the position just before the search starts). Needed by
// 'draw by repetition' detection. Use a std::deque because pointers to
// elements are not invalidated upon list resizing.
using StateListPtr = std::unique_ptr<std::deque<StateInfo>>;

// This error should be used whenever a position is suspected to be unsupported
// by the engine. In particular positions that may cause hard errors like segmentation fault.
struct PositionSetError: std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Position class stores information regarding the board representation as
// pieces, side to move, hash keys, castling info, etc. Important methods are
// do_move() and undo_move(), used by the search to update node info when
// traversing the search tree.
class Position {
   public:
    Position()                           = default;
    Position(const Position&)            = delete;
    Position& operator=(const Position&) = delete;

    // FEN string input/output
    std::optional<PositionSetError> set(const std::string& fenStr, bool isChess960, StateInfo* si);
    std::optional<PositionSetError> set(const std::string& code, Color c, StateInfo* si);
    std::string                     fen() const;

    // Position representation
    Bitboard pieces() const;  // All pieces
    template<typename... PieceTypes>
    Bitboard pieces(PieceTypes... pts) const;
    Bitboard pieces(Color c) const;
    template<typename... PieceTypes>
    Bitboard                            pieces(Color c, PieceTypes... pts) const;
    Piece                               piece_on(Square s) const;
    const std::array<Piece, SQUARE_NB>& piece_array() const;
    Square                              ep_square() const;
    bool                                empty(Square s) const;
    template<PieceType Pt>
    int count(Color c) const;
    template<PieceType Pt>
    int count() const;
    template<PieceType Pt>
    Square square(Color c) const;

    // Castling
    bool   can_castle(CastlingRights cr) const;
    bool   castling_impeded(CastlingRights cr) const;
    Square castling_rook_square(CastlingRights cr) const;

    // Checking
    Bitboard checkers() const;
    Bitboard blockers_for_king(Color c) const;
    Bitboard check_squares(PieceType pt) const;
    Bitboard pinners(Color c) const;

    // Attacks to/from a given square
    Bitboard attackers_to(Square s) const;
    Bitboard attackers_to(Square s, Bitboard occupied) const;
    void     update_slider_blockers(Color c) const;
    Bitboard xattacks_by(PieceType pt, Color c, Bitboard occupied) const;
    Bitboard threats_by(PieceType pt) const;

    // Properties of moves
    bool  legal(Move m) const;
    bool  capture(Move m) const;
    bool  capture_stage(Move m) const;
    bool  gives_check(Move m) const;
    Piece moved_piece(Move m) const;
    Piece captured_piece() const;

    // Doing and undoing moves
    void do_move(Move m, StateInfo& newSt);
    void do_move(Move                   m,
                 StateInfo&             newSt,
                 bool                   givesCheck,
                 Dirties&               dirties,
                 const SharedHistories* worker = nullptr);
    void undo_move(Move m);
    void do_null_move(StateInfo& newSt);
    void undo_null_move();

    // Static Exchange Evaluation
    bool see_ge(Move m, int threshold = 0) const;

    // Accessing hash keys
    Key key() const;
    Key prefetch_key(Move m) const;
    Key material_key() const;
    Key pawn_key() const;
    Key minor_piece_key() const;
    Key non_pawn_key(Color c) const;

    // Other properties of the position
    Color side_to_move() const;
    int   game_ply() const;
    bool  is_chess960() const;
    bool  is_draw(int ply) const;
    bool  is_repetition(int ply) const;
    bool  upcoming_repetition(int ply) const;
    bool  has_repeated() const;
    int   rule50_count() const;
    Value non_pawn_material(Color c) const;
    Value non_pawn_material() const;
    bool  dtz_is_dtm() const;  // Pawnless && (3-men || 4-men-minors-only)

    // Position consistency check, for debugging
    bool                            pos_is_ok() const;
    bool                            material_key_is_ok() const;
    std::optional<PositionSetError> flip();

    StateInfo* state() const;

    void put_piece(Piece pc, Square s, DirtyThreats* const dts = nullptr);
    void remove_piece(Square s, DirtyThreats* const dts = nullptr);
    void swap_piece(Square s, Piece pc, DirtyThreats* const dts = nullptr);

   private:
    // Initialization helpers (used while setting up a position)
    void set_castling_right(Color c, Square rfrom);
    Key  compute_material_key() const;
    void set_state() const;
    void set_extra_bitboards() const;
    void compute_threats() const;

    // Other helpers
    template<bool ComputeRay = true>
    void update_piece_threats(Piece               pc,
                              bool                putPiece,
                              Square              s,
                              DirtyThreats* const dts,
                              Bitboard            noRaysContaining = -1ULL) const;
    void move_piece(Square from, Square to, DirtyThreats* const dts = nullptr);
    template<bool Do>
    void do_castling(Color               us,
                     Square              from,
                     Square&             to,
                     Square&             rfrom,
                     Square&             rto,
                     DirtyThreats* const dts = nullptr,
                     DirtyPiece* const   dp  = nullptr);
    template<bool AfterMove = false>
    Key adjust_key50(Key k) const;

    // Data members
    std::array<Piece, SQUARE_NB>        board;
    std::array<Bitboard, PIECE_TYPE_NB> byTypeBB;
    std::array<Bitboard, COLOR_NB>      byColorBB;

    int        pieceCount[PIECE_NB];
    int        castlingRightsMask[SQUARE_NB];
    Square     castlingRookSquare[CASTLING_RIGHT_NB];
    Bitboard   castlingPath[CASTLING_RIGHT_NB];
    StateInfo* st;
    int        gamePly;
    Color      sideToMove;
    bool       chess960;
    Dirties    scratchDirties;
};

std::ostream& operator<<(std::ostream& os, const Position& pos);

inline Color Position::side_to_move() const { return sideToMove; }

inline Piece Position::piece_on(Square s) const {
    assert(is_ok(s));
    return board[s];
}

inline const std::array<Piece, SQUARE_NB>& Position::piece_array() const { return board; }

inline bool Position::empty(Square s) const { return piece_on(s) == NO_PIECE; }

inline Piece Position::moved_piece(Move m) const { return piece_on(m.from_sq()); }

inline Bitboard Position::pieces() const { return byTypeBB[ALL_PIECES]; }

template<typename... PieceTypes>
inline Bitboard Position::pieces(PieceTypes... pts) const {
    return (byTypeBB[pts] | ...);
}

inline Bitboard Position::pieces(Color c) const { return byColorBB[c]; }

template<typename... PieceTypes>
inline Bitboard Position::pieces(Color c, PieceTypes... pts) const {
    return pieces(c) & pieces(pts...);
}

template<PieceType Pt>
inline int Position::count(Color c) const {
    return pieceCount[make_piece(c, Pt)];
}

template<PieceType Pt>
inline int Position::count() const {
    return count<Pt>(WHITE) + count<Pt>(BLACK);
}

template<PieceType Pt>
inline Square Position::square(Color c) const {
    assert(count<Pt>(c) == 1);
    return lsb(pieces(c, Pt));
}

inline Square Position::ep_square() const { return st->epSquare; }

inline bool Position::can_castle(CastlingRights cr) const { return st->castlingRights & cr; }

inline bool Position::castling_impeded(CastlingRights cr) const {
    assert(cr == WHITE_OO || cr == WHITE_OOO || cr == BLACK_OO || cr == BLACK_OOO);
    return pieces() & castlingPath[cr];
}

inline Square Position::castling_rook_square(CastlingRights cr) const {
    assert(cr == WHITE_OO || cr == WHITE_OOO || cr == BLACK_OO || cr == BLACK_OOO);
    return castlingRookSquare[cr];
}

inline Bitboard Position::attackers_to(Square s) const { return attackers_to(s, pieces()); }

// includes X-ray attacks through the king of color ~c
inline Bitboard Position::xattacks_by(PieceType pt, Color c, Bitboard occupied) const {

    if (pt == PAWN)
        return c == WHITE ? pawn_attacks_bb<WHITE>(pieces(WHITE, PAWN))
                          : pawn_attacks_bb<BLACK>(pieces(BLACK, PAWN));
    else
    {
        Bitboard threats   = 0;
        Bitboard attackers = pieces(c, pt);
        while (attackers)
            threats |= Attacks::attacks_bb(pt, pop_lsb(attackers), occupied);
        return threats;
    }
}

inline Bitboard Position::threats_by(PieceType pt) const {
    if (!st->threats[ALL_PIECES])
        compute_threats();
    return st->threats[pt];
}

inline Bitboard Position::checkers() const { return st->checkersBB; }

inline Bitboard Position::blockers_for_king(Color c) const { return st->blockersForKing[c]; }

inline Bitboard Position::pinners(Color c) const { return st->pinners[c]; }

inline Bitboard Position::check_squares(PieceType pt) const { return st->checkSquares[pt]; }

inline Key Position::key() const { return adjust_key50(st->key); }

template<bool AfterMove>
inline Key Position::adjust_key50(Key k) const {
    return st->rule50 < 14 - AfterMove ? k : k ^ make_key((st->rule50 - (14 - AfterMove)) / 8);
}

inline Key Position::pawn_key() const { return st->pawnKey; }

inline Key Position::material_key() const { return st->materialKey; }

inline Key Position::minor_piece_key() const { return st->minorPieceKey; }

inline Key Position::non_pawn_key(Color c) const { return st->nonPawnKey[c]; }

inline Value Position::non_pawn_material(Color c) const { return st->nonPawnMaterial[c]; }

inline Value Position::non_pawn_material() const {
    return non_pawn_material(WHITE) + non_pawn_material(BLACK);
}

inline int Position::game_ply() const { return gamePly; }

inline int Position::rule50_count() const { return st->rule50; }

inline bool Position::is_chess960() const { return chess960; }

inline bool Position::dtz_is_dtm() const {
    return !count<PAWN>()
        && (count<ALL_PIECES>() == 3 || (count<ALL_PIECES>() == 4 && !pieces(QUEEN, ROOK)));
}

inline bool Position::capture(Move m) const {
    assert(m.is_ok());

    const MoveType mt = m.type_of();

    if (mt == NORMAL || mt == PROMOTION)
        return !empty(m.to_sq());

    return mt == EN_PASSANT;
}

// Returns true if a move is generated from the capture stage, having also
// queen promotions covered, i.e. consistency with the capture stage move
// generation is needed to avoid the generation of duplicate moves.
inline bool Position::capture_stage(Move m) const {
    assert(m.is_ok());

    const MoveType mt = m.type_of();

    if (mt == NORMAL)
        return !empty(m.to_sq());

    if (mt == PROMOTION)
        return !empty(m.to_sq()) || m.promotion_type() == QUEEN;

    return mt == EN_PASSANT;
}

inline Piece Position::captured_piece() const { return st->capturedPiece; }

inline void Position::put_piece(Piece pc, Square s, DirtyThreats* const dts) {
    board[s] = pc;
    byTypeBB[ALL_PIECES] |= byTypeBB[type_of(pc)] |= s;
    byColorBB[color_of(pc)] |= s;
    pieceCount[pc]++;
    pieceCount[make_piece(color_of(pc), ALL_PIECES)]++;

    if (dts)
        update_piece_threats(pc, true, s, dts);
}

inline void Position::remove_piece(Square s, DirtyThreats* const dts) {
    Piece pc = board[s];

    if (dts)
        update_piece_threats(pc, false, s, dts);

    byTypeBB[ALL_PIECES] ^= s;
    byTypeBB[type_of(pc)] ^= s;
    byColorBB[color_of(pc)] ^= s;
    board[s] = NO_PIECE;
    pieceCount[pc]--;
    pieceCount[make_piece(color_of(pc), ALL_PIECES)]--;
}

inline void Position::move_piece(Square from, Square to, DirtyThreats* const dts) {
    Piece    pc     = board[from];
    Bitboard fromTo = from | to;

    if (dts)
        update_piece_threats(pc, false, from, dts, fromTo);

    byTypeBB[ALL_PIECES] ^= fromTo;
    byTypeBB[type_of(pc)] ^= fromTo;
    byColorBB[color_of(pc)] ^= fromTo;
    board[from] = NO_PIECE;
    board[to]   = pc;

    if (dts)
        update_piece_threats(pc, true, to, dts, fromTo);
}

inline void Position::swap_piece(Square s, Piece pc, DirtyThreats* const dts) {
    Piece old = board[s];

    remove_piece(s);

    if (dts)
        update_piece_threats<false>(old, false, s, dts);

    put_piece(pc, s);

    if (dts)
        update_piece_threats<false>(pc, true, s, dts);
}

inline void Position::do_move(Move m, StateInfo& newSt) {
    new (&scratchDirties.dirtyThreats) DirtyThreats;
    do_move(m, newSt, gives_check(m), scratchDirties);
}

inline StateInfo* Position::state() const { return st; }

}  // namespace Stockfish

#endif  // #ifndef POSITION_H_INCLUDED

// === sf_src/src/nnue/nnue_accumulator.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Class for difference calculation of NNUE evaluation function

#ifndef NNUE_ACCUMULATOR_H_INCLUDED
#define NNUE_ACCUMULATOR_H_INCLUDED

#include <array>
#include <cstddef>
#include <cstring>


namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE {

class FeatureTransformer;

// Class that holds the result of affine transformation of input features,
// combined HalfKA + Threats
struct alignas(CacheLineSize) Accumulator {
    std::array<std::array<i16, L1>, COLOR_NB> accumulation;
    std::array<bool, COLOR_NB>                computed = {};
};


// AccumulatorCaches struct provides per-thread accumulator caches, where each
// cache contains multiple entries for each of the possible king squares.
// When the accumulator needs to be refreshed, the cached entry is used to more
// efficiently update the accumulator, instead of rebuilding it from scratch.
// This idea was first described by Luecx (author of Koivisto) and
// is commonly referred to as "Finny Tables".
struct AccumulatorCaches {
    template<typename Network>
    AccumulatorCaches(const Network& network) {
        clear(network);
    }

    struct alignas(CacheLineSize) Entry {
        std::array<BiasType, L1>     accumulation;
        std::array<Piece, SQUARE_NB> pieces;
        Bitboard                     pieceBB;

        // To initialize a refresh entry, we set all its bitboards empty,
        // so we put the biases in the accumulation, without any weights on top
        void clear(const std::array<BiasType, L1>& biases) {
            accumulation = biases;
            std::memset(reinterpret_cast<std::byte*>(this) + offsetof(Entry, pieces), 0,
                        sizeof(Entry) - offsetof(Entry, pieces));
        }
    };

    template<typename Network>
    void clear(const Network& network) {
        for (auto& entries2D : entries)
            for (auto& entries1D : entries2D)
                for (auto& entry : entries1D)
                    entry.clear(network.featureTransformer.biases);
    }

    auto&       operator[](int psqBlock) { return entries[psqBlock]; }
    const auto& operator[](int psqBlock) const { return entries[psqBlock]; }

    std::array<std::array<std::array<Entry, COLOR_NB>, SQUARE_NB>, PSQFeatureSet::NumBlocks>
      entries;
};


struct AccumulatorState: public Accumulator, Dirties {
    PSQFeatureSet::BlockState blockBefore;
};

class AccumulatorStack {
   public:
    static constexpr usize MaxSize = MAX_PLY + 1;

    [[nodiscard]] const AccumulatorState& latest() const noexcept;

    void     reset() noexcept;
    Dirties& push(const Position& pos) noexcept;
    void     pop() noexcept;

    void evaluate(const Position&           pos,
                  const FeatureTransformer& featureTransformer,
                  // Silence spurious warning on GCC 10
                  [[maybe_unused]] AccumulatorCaches& cache) noexcept;

   private:
    [[nodiscard]] AccumulatorState& mut_latest() noexcept;

    void evaluate_side(Color                     perspective,
                       const Position&           pos,
                       const FeatureTransformer& featureTransformer,
                       // Silence spurious warning on GCC 10
                       [[maybe_unused]] AccumulatorCaches& cache,
                       usize                               last_usable_accum) noexcept;

    [[nodiscard]] usize find_last_usable_accumulator(Color           perspective,
                                                     const Position& pos) const noexcept;

    void forward_update_incremental(Color                     perspective,
                                    const Position&           pos,
                                    const FeatureTransformer& featureTransformer,
                                    const usize               begin) noexcept;

    void backward_update_incremental(Color                     perspective,
                                     const Position&           pos,
                                     const FeatureTransformer& featureTransformer,
                                     const usize               end) noexcept;

    void forward_update_incremental_both(const Position&           pos,
                                         const FeatureTransformer& featureTransformer,
                                         usize                     white_begin,
                                         usize                     black_begin) noexcept;

    std::array<AccumulatorState, MaxSize> accumulators;
    usize                                 size = 1;
};

}  // namespace Stockfish::Eval::NNUE

#endif  // NNUE_ACCUMULATOR_H_INCLUDED

namespace Stockfish::Eval::NNUE {

// Returns the inverse of a permutation
template<usize Len>
constexpr std::array<usize, Len> invert_permutation(const std::array<usize, Len>& order) {
    std::array<usize, Len> inverse{};
    for (usize i = 0; i < order.size(); i++)
        inverse[order[i]] = i;
    return inverse;
}

// Divide a byte region of size TotalSize to chunks of size
// BlockSize, and permute the blocks by a given order
template<usize BlockSize, typename T, usize N, usize OrderSize>
void permute(std::array<T, N>& data, const std::array<usize, OrderSize>& order) {
    constexpr usize TotalSize = N * sizeof(T);

    static_assert(TotalSize % (BlockSize * OrderSize) == 0,
                  "ChunkSize * OrderSize must perfectly divide TotalSize");

    constexpr usize ProcessChunkSize = BlockSize * OrderSize;

    std::array<std::byte, ProcessChunkSize> buffer{};

    std::byte* const bytes = reinterpret_cast<std::byte*>(data.data());

    for (usize i = 0; i < TotalSize; i += ProcessChunkSize)
    {
        std::byte* const values = &bytes[i];

        for (usize j = 0; j < OrderSize; j++)
        {
            auto* const buffer_chunk = &buffer[j * BlockSize];
            auto* const value_chunk  = &values[order[j] * BlockSize];

            std::copy(value_chunk, value_chunk + BlockSize, buffer_chunk);
        }

        std::copy(std::begin(buffer), std::end(buffer), values);
    }
}

// Input feature converter
class FeatureTransformer {
    // Number of output dimensions for one side
    static constexpr IndexType HalfDimensions = L1;

   public:
    // Output type
    using OutputType = TransformedFeatureType;

    // Number of input/output dimensions
    static constexpr IndexType ThreatInputDimensions = ThreatFeatureSet::Dimensions;
    static constexpr IndexType PairInputDimensions   = PairFeatureSet::Dimensions;
    static constexpr IndexType PsqDimensions         = PSQFeatureSet::Dimensions;
    static constexpr IndexType ThreatAndPpDimensions = ThreatInputDimensions + PairInputDimensions;
    static constexpr IndexType InputDimensions       = PsqDimensions + ThreatAndPpDimensions;
    static constexpr IndexType OutputDimensions      = HalfDimensions;
    static constexpr IndexType ThreatWeightSize      = ThreatInputDimensions * HalfDimensions;
    static constexpr IndexType PairWeightSize        = PairInputDimensions * HalfDimensions;
    static constexpr IndexType ThreatAndPpWeightSize = ThreatAndPpDimensions * HalfDimensions;

    using BiasesArray            = std::array<BiasType, HalfDimensions>;
    using WeightArray            = std::array<WeightType, HalfDimensions * PsqDimensions>;
    using ThreatAndPpWeightArray = std::array<ThreatWeightType, ThreatAndPpWeightSize>;

    // Size of forward propagation buffer
    static constexpr usize BufferSize = OutputDimensions * sizeof(OutputType);

    // Store the order by which 128-bit blocks of a 1024-bit data must
    // be permuted so that calling packus on adjacent vectors of 16-bit
    // integers loaded from the data results in the pre-permutation order
    static constexpr auto PackusEpi16Order = []() -> std::array<usize, 8> {
#if defined(USE_AVX512)
        // _mm512_packus_epi16 after permutation:
        // |   0   |   2   |   4   |   6   | // Vector 0
        // |   1   |   3   |   5   |   7   | // Vector 1
        // | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | // Packed Result
        return {0, 2, 4, 6, 1, 3, 5, 7};
#elif defined(USE_AVX2) || defined(USE_LASX)
        // _mm256_packus_epi16 after permutation:
        // |   0   |   2   |  |   4   |   6   | // Vector 0, 2
        // |   1   |   3   |  |   5   |   7   | // Vector 1, 3
        // | 0 | 1 | 2 | 3 |  | 4 | 5 | 6 | 7 | // Packed Result
        return {0, 2, 1, 3, 4, 6, 5, 7};
#else
        return {0, 1, 2, 3, 4, 5, 6, 7};
#endif
    }();

    static constexpr auto InversePackusEpi16Order = invert_permutation(PackusEpi16Order);

#if defined(USE_PAIRED_WIDENING_ADD)
    // The int8 weight rows are consumed one full vector register (sizeof(vec_t)
    // bytes) at a time: unpack{lo,hi}_epi8 interleave the bytes of two rows and
    // maddubs against a vector of ones sums each pair into an int16 lane. Both
    // unpacks work within 128-bit lanes, so the int16 results of a chunk come out
    // with its 8-byte blocks in the order {0, 2, 4, 6, ...} (lo) and {1, 3, 5, 7,
    // ...} (hi). Storing the 8-byte blocks of every chunk in this order at load
    // time makes the two output registers hold consecutive accumulator lanes, so
    // the accumulator layout is unchanged.
    static constexpr auto WideningPairOrder = []() {
        constexpr usize              NumBlocks = sizeof(SIMD::vec_t) / 8;
        std::array<usize, NumBlocks> order{};
        for (usize i = 0; i < NumBlocks; ++i)
            order[i] = i % 2 == 0 ? i / 2 : NumBlocks / 2 + i / 2;
        return order;
    }();

    static constexpr auto InverseWideningPairOrder = invert_permutation(WideningPairOrder);
#endif

    static constexpr u32 combine_hash(std::initializer_list<u32> hashes) {
        u32 hash = 0;
        for (const auto component_hash : hashes)
        {
            hash = (hash << 1) | (hash >> 31);
            hash ^= component_hash;
        }
        return hash;
    }

    // Hash value embedded in the evaluation file
    static constexpr u32 get_hash_value() {
        return combine_hash(
                 {ThreatFeatureSet::HashValue, PairFeatureSet::HashValue, PSQFeatureSet::HashValue})
             ^ (OutputDimensions * 2);
    }

    void permute_weights() {
        permute<16>(biases, PackusEpi16Order);
        permute<8>(weights, PackusEpi16Order);

        permute<8>(threatAndPpWeights, PackusEpi16Order);

#if defined(USE_PAIRED_WIDENING_ADD)
        // With 128-bit vectors the order is the identity
        if constexpr (sizeof(SIMD::vec_t) > 16)
        {
            permute<8>(weights, WideningPairOrder);
            permute<8>(threatAndPpWeights, WideningPairOrder);
        }
#endif
    }

    void unpermute_weights() {
#if defined(USE_PAIRED_WIDENING_ADD)
        if constexpr (sizeof(SIMD::vec_t) > 16)
        {
            permute<8>(weights, InverseWideningPairOrder);
            permute<8>(threatAndPpWeights, InverseWideningPairOrder);
        }
#endif

        permute<16>(biases, InversePackusEpi16Order);
        permute<8>(weights, InversePackusEpi16Order);
        permute<8>(threatAndPpWeights, InversePackusEpi16Order);
    }

    ThreatWeightType* threatWeightData() { return threatAndPpWeights.data(); }
    ThreatWeightType* pawnPairWeightData() { return threatWeightData() + ThreatWeightSize; }

    // Read network parameters
    bool read_parameters(std::istream& stream) {
        read_little_endian(stream, biases.data(), HalfDimensions);

        read_little_endian(stream, threatWeightData(), ThreatWeightSize);
        read_little_endian(stream, pawnPairWeightData(), PairWeightSize);

        read_little_endian(stream, weights.data(), weights.size());

        permute_weights();

        return !stream.fail();
    }

    // Write network parameters
    bool write_parameters(std::ostream& stream) const {
        std::unique_ptr<FeatureTransformer> copy = std::make_unique<FeatureTransformer>(*this);

        copy->unpermute_weights();

        write_little_endian(stream, copy->biases.data(), HalfDimensions);

        write_little_endian(stream, copy->threatWeightData(), ThreatWeightSize);
        write_little_endian(stream, copy->pawnPairWeightData(), PairWeightSize);

        write_little_endian(stream, copy->weights.data(), copy->weights.size());

        return !stream.fail();
    }

    usize get_content_hash() const {
        usize h = 0;

        hash_combine(h, get_raw_data_hash(biases));
        hash_combine(h, get_raw_data_hash(weights));

        hash_combine(h, get_raw_data_hash(threatAndPpWeights));

        hash_combine(h, get_hash_value());

        return h;
    }

    const WeightType* psq_weights(int block) const {
        return &weights[usize(PSQFeatureSet::block_offset(block)) * HalfDimensions];
    }

    // Convert input features
    void transform(const Position&                             pos,
                   AccumulatorStack&                           accumulatorStack,
                   AccumulatorCaches&                          cache,
                   OutputType*                                 output,
                   [[maybe_unused]] NNZInfo<OutputDimensions>& nnzInfo) const {
        accumulatorStack.evaluate(pos, *this, cache);
        const auto& accumulatorState = accumulatorStack.latest();

        const Color perspectives[2] = {pos.side_to_move(), ~pos.side_to_move()};
        const auto& accumulation    = accumulatorState.accumulation;

        for (IndexType p = 0; p < 2; ++p)
            transform_perspective(accumulation[perspectives[p]], output, p, nnzInfo);
    }

   private:
    static void transform_perspective(const std::array<i16, HalfDimensions>&      accumulation,
                                      OutputType*                                 output,
                                      IndexType                                   perspective,
                                      [[maybe_unused]] NNZInfo<OutputDimensions>& nnzInfo) {

        using namespace SIMD;
        const IndexType offset = (HalfDimensions / 2) * perspective;

#if defined(VECTOR)

        [[maybe_unused]] auto cursor = nnzInfo.make_cursor(perspective);

        constexpr IndexType OutputChunkSize = MaxChunkSize;
        static_assert((HalfDimensions / 2) % OutputChunkSize == 0);
        constexpr IndexType NumOutputChunks = HalfDimensions / 2 / OutputChunkSize;

        [[maybe_unused]] const vec_t   Zero  = vec_zero();
        [[maybe_unused]] const vec_t   FtMax = vec_set_16(FtMaxVal);
        [[maybe_unused]] constexpr int shift = 7;

        const vec_t* in0 = reinterpret_cast<const vec_t*>(&accumulation[0]);
        const vec_t* in1 = reinterpret_cast<const vec_t*>(&accumulation[HalfDimensions / 2]);
        vec_t*       out = reinterpret_cast<vec_t*>(output + offset);

        // Per the NNUE architecture, here we want to multiply pairs of
        // clipped elements and divide the product by 512. To do this,
        // we can naively perform min/max operation to clip each of the
        // four int16 vectors, mullo pairs together, then pack them into
        // one int8 vector. However, there exists a faster way.

        // The idea here is to use the implicit clipping from packus to
        // save us two vec_max_16 instructions. This clipping works due
        // to the fact that any int16 integer below zero will be zeroed
        // on packus.

        // Consider the case where the second element is negative.
        // If we do standard clipping, that element will be zero, which
        // means our pairwise product is zero. If we perform packus and
        // remove the lower-side clip for the second element, then our
        // product before packus will be negative, and is zeroed on pack.
        // The two operations produce equivalent results, but the second
        // one (using packus) saves one max operation per pair.

        // But here we run into a problem: mullo does not preserve the
        // sign of the multiplication. We can get around this by doing
        // mulhi, which keeps the sign. But that requires an additional
        // tweak.

        // mulhi cuts off the last 16 bits of the resulting product,
        // which is the same as performing a rightward shift of 16 bits.
        // We can use this to our advantage. Recall that we want to
        // divide the final product by 512, which is equivalent to a
        // 9-bit right shift. Intuitively, if we shift the clipped
        // value left by 7, and perform mulhi, which shifts the product
        // right by 16 bits, then we will net a right shift of 9 bits.

        for (IndexType j = 0; j < NumOutputChunks; j += 2)
        {
            vec_t packed[2];
            UNROLLED_FOR (IndexType k = 0; k < 2; ++k)
            {
                const IndexType i = (j + k) * 2;

                vec_t acc0a = in0[i + 0];
                vec_t acc0b = in0[i + 1];
                vec_t acc1a = in1[i + 0];
                vec_t acc1b = in1[i + 1];

                static_assert(FtMaxVal == 255);

    #if defined(USE_NEON)
                uint16x8_t mul0 = vmull_u8(vqmovun_s16(acc0a), vqmovun_s16(acc1a));
                uint16x8_t mul1 = vmull_u8(vqmovun_s16(acc0b), vqmovun_s16(acc1b));

                uint8x16x2_t uzp = vuzpq_u8(vreinterpretq_u8_u16(mul0), vreinterpretq_u8_u16(mul1));
                uint8x16_t   pab = vshrq_n_u8(uzp.val[1], 1);
                vec_t        result = reinterpret_cast<vec_t>(pab);
    #elif defined(USE_LSX) || defined(USE_LASX)
                vec_t pa = vec_packus_16(acc0a, acc0b);
                vec_t pb = vec_packus_16(acc1a, acc1b);

                vec_t hi     = vec_mulhi_8(pa, pb);
                vec_t result = vec_srli_8(hi, 1);
    #elif defined(__wasm__)
                // _mm_mulhi_epi16 is lowered to 32-bit multiplies, so we take
                // a similar approach as the NEON path.
                vec_t mul0 = vec_packus_16(acc0a, acc0b);
                vec_t mul1 = vec_packus_16(acc1a, acc1b);

                vec_t low = wasm_u16x8_extmul_low_u8x16(mul0, mul1);
                vec_t hi  = wasm_u16x8_extmul_high_u8x16(mul0, mul1);

                // equivalent to vuzp2_u8
                vec_t merged = wasm_i8x16_shuffle(low, hi, 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21,
                                                  23, 25, 27, 29, 31);
                vec_t result = wasm_u8x16_shr(merged, 1);
    #else
                vec_t sum0a = vec_slli_16(vec_max_16(vec_min_16(acc0a, FtMax), Zero), shift);
                vec_t sum0b = vec_slli_16(vec_max_16(vec_min_16(acc0b, FtMax), Zero), shift);
                vec_t sum1a = vec_min_16(acc1a, FtMax);
                vec_t sum1b = vec_min_16(acc1b, FtMax);

                vec_t pa = vec_mulhi_16(sum0a, sum1a);
                vec_t pb = vec_mulhi_16(sum0b, sum1b);

                vec_t result = vec_packus_16(pa, pb);
    #endif

                packed[k] = out[j + k] = result;
            }

            cursor.record2(packed[0], packed[1]);
        }

#elif defined(USE_RVV)

        usize       j  = 0;
        usize       VL = __riscv_vsetvlmax_e8m1();
        vuint8m1_t  vid8;
        vuint16m2_t vid16;
        if (VL <= 256)
            vid8 = __riscv_vid_v_u8m1(VL);
        else
            vid16 = __riscv_vid_v_u16m2(VL);

        for (usize vl; j < HalfDimensions / 2; j += vl)
        {
            vl = __riscv_vsetvl_e16m2(HalfDimensions / 2 - j);

            vint16m2_t acc0 = __riscv_vle16_v_i16m2(&accumulation[j], vl);
            vint16m2_t acc1 = __riscv_vle16_v_i16m2(&accumulation[j + HalfDimensions / 2], vl);

            acc0 = __riscv_vmax(acc0, 0, vl);
            acc1 = __riscv_vmax(acc1, 0, vl);

            vuint8m1_t pa = __riscv_vnclipu(__riscv_vreinterpret_u16m2(acc0), 0, 0, vl);
            vuint8m1_t pb = __riscv_vnclipu(__riscv_vreinterpret_u16m2(acc1), 0, 0, vl);

            vuint8m1_t hi     = __riscv_vmulhu(pa, pb, vl);
            vuint8m1_t result = __riscv_vsrl(hi, 1, vl);

            __riscv_vse8(&output[offset + j], result, vl);

            vbool8_t    m   = __riscv_vmsne(result, 0, vl);
            usize       cnt = __riscv_vcpop(m, vl);
            vuint16m2_t vidx;
            if (VL <= 256)
                vidx = __riscv_vzext_vf2(__riscv_vcompress(vid8, m, vl), cnt);
            else
                vidx = __riscv_vcompress(vid16, m, vl);
            __riscv_vse16(&nnzInfo.nnz[nnzInfo.count], __riscv_vadd(vidx, offset + j, cnt), cnt);
            nnzInfo.count += cnt;
        }

#else

        for (IndexType j = 0; j < HalfDimensions / 2; ++j)
        {
            BiasType sum0 = accumulation[j];
            BiasType sum1 = accumulation[j + HalfDimensions / 2];

            sum0 = std::clamp<BiasType>(sum0, 0, FtMaxVal);
            sum1 = std::clamp<BiasType>(sum1, 0, FtMaxVal);

            output[offset + j] = static_cast<OutputType>(unsigned(sum0 * sum1) / 512);
        }

#endif
    }

   public:
    alignas(CacheLineSize) BiasesArray biases;
    alignas(CacheLineSize) WeightArray weights;

    // Threats and pawn-pair features are concatenated into one array to allow for a single index to address either.
    // The first pawn-pair feature is at index ThreatFeatureSet::Dimensions.
    static_assert(PairFeatureSet::IndexBase == ThreatFeatureSet::Dimensions);

    alignas(CacheLineSize) ThreatAndPpWeightArray threatAndPpWeights;
};

}  // namespace Stockfish::Eval::NNUE

template<>
struct std::hash<Stockfish::Eval::NNUE::FeatureTransformer> {
    Stockfish::usize
    operator()(const Stockfish::Eval::NNUE::FeatureTransformer& ft) const noexcept {
        return ft.get_content_hash();
    }
};

#endif  // #ifndef NNUE_FEATURE_TRANSFORMER_H_INCLUDED

// === sf_src/src/nnue/nnue_misc.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NNUE_MISC_H_INCLUDED
#define NNUE_MISC_H_INCLUDED

#include <string>
#include <string_view>
#include <optional>
#include <filesystem>


namespace Stockfish {

class Position;

namespace Eval::NNUE {

// NNUE file metadata uses fixed string types so it stays trivially copyable and cheap to move
// around between the engine and the network loader.
struct EvalFile {
    // Default net name, will use one of the EvalFileDefaultName* macros defined
    // in evaluate.h
    constexpr static std::string_view defaultName = EvalFileDefaultName;
    // Selected net name, either via uci option or default
    std::optional<std::filesystem::path> current;
    // Net description extracted from the net file
    std::string netDescription;
};

struct NnueEvalTrace {
    Value positional[LayerStacks];
    usize correctBucket;
    usize layerStacks;
};

class Network;
struct AccumulatorCaches;

std::string trace(Position& pos, const Network& network, AccumulatorCaches& caches);

}  // namespace Stockfish::Eval::NNUE
}  // namespace Stockfish

#endif  // #ifndef NNUE_MISC_H_INCLUDED

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE {

class AccumulatorStack;
struct AccumulatorCaches;

// The network must be a trivial type, i.e. the memory must be in-line.
// This is required to allow sharing the network via shared memory, as
// there is no way to run destructors.
class Network {
   public:
    Network() = default;

    Network(const Network& other) = default;
    Network(Network&& other)      = default;

    Network& operator=(const Network& other) = default;
    Network& operator=(Network&& other)      = default;

    void load(const std::filesystem::path& rootDirectory,
              std::filesystem::path        evalfilePath,
              EvalFile&                    evalFile);
    bool save(const EvalFile& evalFile, const std::optional<std::filesystem::path>& filename) const;

    usize get_content_hash() const;

    Value evaluate(const Position&    pos,
                   AccumulatorStack&  accumulatorStack,
                   AccumulatorCaches& cache) const;


    void verify(const std::function<void(std::string_view)>& f,
                const EvalFile&                              evalFile,
                std::filesystem::path                        evalfilePath) const;

    NnueEvalTrace trace_evaluate(const Position&    pos,
                                 AccumulatorStack&  accumulatorStack,
                                 AccumulatorCaches& cache) const;

    void load_external(const std::filesystem::path&, const std::filesystem::path&, EvalFile&);
    void load_internal(EvalFile&);

   private:
    void initialize();

    bool                       save(std::ostream&, const std::string&) const;
    std::optional<std::string> load(std::istream&);

    bool read_header(std::istream&, u32*, std::string*) const;
    bool write_header(std::ostream&, u32, const std::string&) const;

    bool read_parameters(std::istream&, std::string&);
    bool write_parameters(std::ostream&, const std::string&) const;

    int layer_stack(const Position& pos) const;

    // Input feature converter
    FeatureTransformer featureTransformer;

    // Evaluation function
    NetworkArchitecture network[LayerStacks];
    u16                 layerStackFallback[RawLayerStacks];
    u16                 layerStackIndex[RawLayerStacks];
    IndexType           storedLayerStacks = 0;

    bool initialized = false;

    static constexpr u32 LayerStackTableHash = 0x5A7AB1E5u;

    // Hash value of evaluation function structure
    static constexpr u32 hash = FeatureTransformer::get_hash_value()
                              ^ NetworkArchitecture::get_hash_value() ^ LayerStackTableHash;

    friend struct AccumulatorCaches;
};


}  // namespace Stockfish::Eval::NNUE

template<>
struct std::hash<Stockfish::Eval::NNUE::Network> {
    Stockfish::usize operator()(const Stockfish::Eval::NNUE::Network& network) const noexcept {
        return network.get_content_hash();
    }
};

#endif

// === sf_src/src/uci.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UCI_H_INCLUDED
#define UCI_H_INCLUDED

#include <iostream>
#include <string>
#include <string_view>


// === sf_src/src/engine.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ENGINE_H_INCLUDED
#define ENGINE_H_INCLUDED

#include <functional>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>


// === sf_src/src/history.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef HISTORY_H_INCLUDED
#define HISTORY_H_INCLUDED

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>  // IWYU pragma: keep


namespace Stockfish {

constexpr int PAWN_HISTORY_BASE_SIZE   = 8192;  // has to be a power of 2
constexpr int UINT_16_HISTORY_SIZE     = std::numeric_limits<u16>::max() + 1;
constexpr int CORRHIST_BASE_SIZE       = UINT_16_HISTORY_SIZE;
constexpr int CORRECTION_HISTORY_LIMIT = 1024;
constexpr int LOW_PLY_HISTORY_SIZE     = 5;

static_assert((PAWN_HISTORY_BASE_SIZE & (PAWN_HISTORY_BASE_SIZE - 1)) == 0,
              "PAWN_HISTORY_BASE_SIZE has to be a power of 2");

static_assert((CORRHIST_BASE_SIZE & (CORRHIST_BASE_SIZE - 1)) == 0,
              "CORRHIST_BASE_SIZE has to be a power of 2");

// StatsEntry is the container of various numerical statistics. We use a class
// instead of a naked value to directly call history update operator<<() on
// the entry. The first template parameter T is the base type of the array,
// and the second template parameter D limits the range of updates in [-D, D]
// when we update values with the << operator
template<typename T, int D, bool Shared = false>
struct StatsEntry {
    static_assert(std::is_integral_v<T> && std::is_signed_v<T>, "Not a signed integer type");
    static_assert(D > 0 && D <= std::numeric_limits<T>::max()
                    && D <= std::numeric_limits<int>::max() / D,
                  "D can lead to overflows");

   private:
    std::conditional_t<Shared, RelaxedAtomic<T>, T> entry;

   public:
    void operator=(const T& v) { entry = v; }

    operator T() const { return entry; }

    void operator<<(int bonus) {
        // Make sure that bonus is in range [-D, D]
        int clampedBonus = std::clamp(bonus, -D, D);
        T   val          = *this;
        *this            = val + clampedBonus - val * std::abs(clampedBonus) / D;

        assert(std::abs(T(*this)) <= D);
    }
};

enum StatsType {
    NoCaptures,
    Captures
};

template<typename T, int D, usize... Sizes>
using Stats = MultiArray<StatsEntry<T, D>, Sizes...>;

template<typename T, int D, usize... Sizes>
using AtomicStats = MultiArray<StatsEntry<T, D, true>, Sizes...>;

// DynStats is a dynamically sized array of Stats, used for thread-shared histories
// which should scale with the total number of threads. The SizeMultiplier gives
// the per-thread allocation count of T.
template<typename T, int SizeMultiplier>
struct DynStats {
    explicit DynStats(usize s) :
        size(s * SizeMultiplier),
        data(make_unique_large_page<T[]>(size)) {}
    // Sets all values in the range to 0
    void clear_range(int value, usize threadIdx, usize numaTotal) {
        usize start = u64(threadIdx) * size / numaTotal;
        assert(start < size);
        usize end = threadIdx + 1 == numaTotal ? size : u64(threadIdx + 1) * size / numaTotal;

        while (start < end)
            data[start++].fill(value);
    }
    usize get_size() const { return size; }
    T&    operator[](usize index) {
        assert(index < size);
        return data[index];
    }
    const T& operator[](usize index) const {
        assert(index < size);
        return data[index];
    }

   private:
    usize             size;
    LargePagePtr<T[]> data;
};

// ButterflyHistory records how often quiet moves have been successful or unsuccessful
// during the current search, and is used for reduction and move ordering decisions.
// It uses 2 tables (one for each color) indexed by the move's from and to squares,
// see https://www.chessprogramming.org/Butterfly_Boards
using ButterflyHistory = Stats<i16, 7183, COLOR_NB, UINT_16_HISTORY_SIZE>;

// LowPlyHistory is addressed by ply and move's from and to squares, used
// to improve move ordering near the root
using LowPlyHistory = Stats<i16, 7183, LOW_PLY_HISTORY_SIZE, UINT_16_HISTORY_SIZE>;

// CapturePieceToHistory is addressed by a move's [piece][to][captured piece type]
using CapturePieceToHistory = Stats<i16, 10692, PIECE_NB, SQUARE_NB, PIECE_TYPE_NB>;

// PieceToHistory is like ButterflyHistory but is addressed by a move's [piece][to]
using PieceToHistory = AtomicStats<i16, 30000, PIECE_NB, SQUARE_NB>;

// ContinuationHistory is the combined history of a given pair of moves, usually
// the current one given a previous one. The nested history table is based on
// PieceToHistory instead of ButterflyBoards.
using ContinuationHistory = MultiArray<PieceToHistory, PIECE_NB, SQUARE_NB>;

// PawnHistory is addressed by the pawn structure and a move's [piece][to]
using PawnHistory = DynStats<AtomicStats<i16, 8192, PIECE_NB, SQUARE_NB>, PAWN_HISTORY_BASE_SIZE>;

// Correction histories record differences between the static evaluation of
// positions and their search score. It is used to improve the static evaluation
// used by some search heuristics.
// see https://www.chessprogramming.org/Static_Evaluation_Correction_History
enum CorrHistType {
    PieceTo,       // By [piece][to] move
    Continuation,  // Combined history of move pairs
};

template<typename T, int D>
struct CorrectionBundle {
    StatsEntry<T, D, true> pawn;
    StatsEntry<T, D, true> minor;
    StatsEntry<T, D, true> nonPawnWhite;
    StatsEntry<T, D, true> nonPawnBlack;

    void operator=(T val) {
        pawn         = val;
        minor        = val;
        nonPawnWhite = val;
        nonPawnBlack = val;
    }
};

namespace Detail {

template<CorrHistType>
struct CorrHistTypedef;

template<>
struct CorrHistTypedef<PieceTo> {
    using type = Stats<i16, CORRECTION_HISTORY_LIMIT, PIECE_NB, SQUARE_NB>;
};

template<>
struct CorrHistTypedef<Continuation> {
    using type = MultiArray<CorrHistTypedef<PieceTo>::type, PIECE_NB, SQUARE_NB>;
};

}

using UnifiedCorrectionHistory =
  DynStats<MultiArray<CorrectionBundle<i16, CORRECTION_HISTORY_LIMIT>, COLOR_NB>,
           CORRHIST_BASE_SIZE>;

template<CorrHistType T>
using CorrectionHistory = typename Detail::CorrHistTypedef<T>::type;

using TTMoveHistory = StatsEntry<i16, 8192>;

struct ContinuationHistoryBlock {
    ContinuationHistory table[2][2];
};

// Set of histories shared between groups of threads. To avoid excessive
// cross-node data transfer, histories are shared only between threads
// on a given NUMA node. The passed size must be a power of two to make
// the indexing more efficient.
struct SharedHistories {
    SharedHistories(usize threadCount) :
        correctionHistory(threadCount),
        continuationHistoryBlock(make_unique_large_page<ContinuationHistoryBlock>()),
        pawnHistory(threadCount) {
        assert((threadCount & (threadCount - 1)) == 0 && threadCount != 0);
        sizeMinus1         = correctionHistory.get_size() - 1;
        pawnHistSizeMinus1 = pawnHistory.get_size() - 1;
    }

    auto& continuationHistory() { return continuationHistoryBlock->table; }

    usize get_size() const { return sizeMinus1 + 1; }

    auto& pawn_entry(const Position& pos) {
        return pawnHistory[pos.pawn_key() & pawnHistSizeMinus1];
    }
    const auto& pawn_entry(const Position& pos) const {
        return pawnHistory[pos.pawn_key() & pawnHistSizeMinus1];
    }

    auto& pawn_correction_entry(const Position& pos) {
        return correctionHistory[pos.pawn_key() & sizeMinus1];
    }
    const auto& pawn_correction_entry(const Position& pos) const {
        return correctionHistory[pos.pawn_key() & sizeMinus1];
    }

    auto& minor_piece_correction_entry(const Position& pos) {
        return correctionHistory[pos.minor_piece_key() & sizeMinus1];
    }
    const auto& minor_piece_correction_entry(const Position& pos) const {
        return correctionHistory[pos.minor_piece_key() & sizeMinus1];
    }

    template<Color c>
    auto& nonpawn_correction_entry(const Position& pos) {
        return correctionHistory[pos.non_pawn_key(c) & sizeMinus1];
    }
    template<Color c>
    const auto& nonpawn_correction_entry(const Position& pos) const {
        return correctionHistory[pos.non_pawn_key(c) & sizeMinus1];
    }

    UnifiedCorrectionHistory               correctionHistory;
    LargePagePtr<ContinuationHistoryBlock> continuationHistoryBlock;
    PawnHistory                            pawnHistory;


   private:
    usize sizeMinus1, pawnHistSizeMinus1;
};

}  // namespace Stockfish

#endif  // #ifndef HISTORY_H_INCLUDED

// === sf_src/src/numa.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NUMA_H_INCLUDED
#define NUMA_H_INCLUDED

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <cstring>


// === sf_src/src/shm.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SHM_H_INCLUDED
#define SHM_H_INCLUDED

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#if (defined(__linux__) && !defined(__ANDROID__)) || defined(__APPLE__) || defined(__FreeBSD__) \
  || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    #define USE_UNIX_SHM

// === sf_src/src/shm_unix.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SHM_UNIX_H_INCLUDED
#define SHM_UNIX_H_INCLUDED

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <mutex>
#include <new>
#include <optional>
#include <pthread.h>
#include <string>
#include <inttypes.h>
#include <thread>
#include <type_traits>
#include <utility>

#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>
#include <limits.h>

#define SF_MAX_SEM_NAME_LEN NAME_MAX


#if defined(__linux__) && !defined(MADV_COLLAPSE)
    #define MADV_COLLAPSE 25
#endif

namespace Stockfish::shm {

namespace detail {

inline void* map_shared(int fd, usize size) noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
    return Stockfish::mmap_huge_aligned(size, MAP_SHARED, fd);
#else
    return mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
#endif
}

class SharedMemoryRegistry {
   private:
    inline static std::once_flag           register_atexit_once_;
    inline static std::mutex               socket_paths_mutex_;
    inline static std::vector<std::string> socket_paths_;

   public:
    static void register_socket_path(std::string path) {
        std::call_once(register_atexit_once_,
                       []() { std::atexit(SharedMemoryRegistry::cleanup_at_exit); });

        std::scoped_lock lock(socket_paths_mutex_);
        socket_paths_.push_back(std::move(path));
    }

    static void unlink_socket_path(const std::string& path) {
        std::scoped_lock lock(socket_paths_mutex_);
        unlink(path.c_str());
        socket_paths_.erase(std::remove(socket_paths_.begin(), socket_paths_.end(), path),
                            socket_paths_.end());
    }

    static void cleanup_at_exit() noexcept {
        std::scoped_lock lock(socket_paths_mutex_);
        // TODO: we litter .sock files upon an abnormal exit (e.g. Ctrl-C)
        // but it's unsafe to acquire the lock in a signal handler
        for (auto const& path : socket_paths_)
            unlink(path.c_str());
    }
};

}  // namespace detail


[[maybe_unused]] static void set_cloexec(int fd) {
    if (fd != -1)
        (void) fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
}

struct UniqueFd {
    UniqueFd() noexcept = default;
    explicit UniqueFd(int fd) noexcept :
        fd_{fd} {}
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept :
        fd_{other.release()} {}
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        std::swap(fd_, other.fd_);
        return *this;
    }
    ~UniqueFd() { reset(); }

    int  get() const noexcept { return fd_; }
    bool is_valid() const noexcept { return fd_ != -1; }

    int release() noexcept { return std::exchange(fd_, -1); }

    void reset(int fd = -1) noexcept {
        if (fd_ != -1)
            ::close(fd_);
        fd_ = fd;
    }

   private:
    int fd_ = -1;
};

struct TempRoot {
    // /tmp/stockfish-[uid], with appropriate permissions
    std::string prefix;

    static const std::optional<TempRoot>& get_temp_root() {
        static auto temp_root = []() -> std::optional<TempRoot> {
            auto proposed = std::string("/tmp/stockfish-") + std::to_string(getuid());

            if (mkdir(proposed.c_str(), 0700) == 0)
                return {{proposed}};

            if (errno == EEXIST)
            {
                // Temp root already exists, check perms
                struct stat st;
                if (lstat(proposed.c_str(), &st) == 0 && S_ISDIR(st.st_mode)
                    && st.st_uid == getuid() && (st.st_mode & 07777) == 0700)
                    return {{proposed}};
            }

            return std::nullopt;
        }();

        return temp_root;
    }
};

// Wrapper around flock() on a file
struct InitLock {
    InitLock() = default;

    ~InitLock() {
        if (lock_fd_.is_valid())
            flock(lock_fd_.get(), LOCK_UN);
    }

    static InitLock wait_for_init_lock(const std::string& path) noexcept {
        UniqueFd lock_fd(::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0666));
        if (!lock_fd.is_valid())
            return {};

        // Blocks here if another process is currently initializing
        while (flock(lock_fd.get(), LOCK_EX) == -1)
        {
            if (errno != EINTR)
                return {};  // failed to acquire
        }

        return InitLock(std::move(lock_fd));
    }

    bool is_valid() const { return lock_fd_.is_valid(); }

   private:
    UniqueFd lock_fd_;
    explicit InitLock(UniqueFd lock_fd) :
        lock_fd_(std::move(lock_fd)) {}
};

template<typename T>
class SharedMemory {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    static_assert(!std::is_pointer_v<T>, "T cannot be a pointer type");

   private:
    std::string name_;
    void*       mapped_ptr_ = nullptr;
    T*          data_ptr_   = nullptr;  // = mapped_ptr_

    // Fishes will put their .sock files in this folder, and each folder is associated with a single underlying
    // shared memfd. Therefore in a NUMA setting, we may have multiple such folders
    std::string shared_dir_;

    // Threads need to successfully and exclusively lock this file to initialize the memfd. If another process has
    // a lock on it, then we wait for it to finish initializing (or die) and release the lock
    std::string init_lock_path_;

    // serve requests for the shared segment on this .sock
    std::string  socket_path_;
    NativeThread server_thread_;
    UniqueFd     shutdown_;  // close to signal server thread shutdown

    static std::string make_sentinel_base(const std::string& name) {
        char buf[32];
        // Using std::to_string here causes non-deterministic PGO builds.
        // snprintf, being part of libc, is insensitive to the formatted values.
        std::snprintf(buf, sizeof(buf), "sfshm_%016" PRIu64, hash_string(name));
        return buf;
    }

   public:
    explicit SharedMemory(const std::string& name, const TempRoot& tempRoot) noexcept :
        name_(name),
        shared_dir_(tempRoot.prefix + "/" + make_sentinel_base(name)),
        init_lock_path_(shared_dir_ + "/init_lock"),
        socket_path_(shared_dir_ + "/" + std::to_string(getpid()) + ".sock") {}

    ~SharedMemory() noexcept {
        if (!socket_path_.empty())
            detail::SharedMemoryRegistry::unlink_socket_path(socket_path_);

        shutdown_.reset();
        if (server_thread_.joinable())
            server_thread_.join();

        if (mapped_ptr_)
            munmap(mapped_ptr_, sizeof(T));
    }

    SharedMemory(const SharedMemory&)            = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    SharedMemory(SharedMemory&& other) noexcept { swap(other); }

    SharedMemory& operator=(SharedMemory&& other) noexcept {
        swap(other);
        return *this;
    }

    bool is_mapped() const noexcept { return mapped_ptr_ != nullptr; }

    bool is_serving() const noexcept { return server_thread_.joinable(); }

    const std::string& name() const noexcept { return name_; }

    const T& get() const noexcept {
        assert(data_ptr_ != nullptr);

        return *data_ptr_;
    }

   private:
    void swap(SharedMemory& other) noexcept {
        std::swap(name_, other.name_);
        std::swap(mapped_ptr_, other.mapped_ptr_);
        std::swap(data_ptr_, other.data_ptr_);
        std::swap(shared_dir_, other.shared_dir_);
        std::swap(init_lock_path_, other.init_lock_path_);
        std::swap(socket_path_, other.socket_path_);
        std::swap(server_thread_, other.server_thread_);
        std::swap(shutdown_, other.shutdown_);
    }

    // Discover all peers in the shared dir
    std::vector<std::string> get_peer_sockets() noexcept {
        std::vector<std::string> peer_sockets;
        if (DIR* dir = opendir(shared_dir_.c_str()))
        {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr)
            {
                std::string name = entry->d_name;
                if (name.size() >= 5 && name.compare(name.size() - 5, 5, ".sock") == 0)
                    peer_sockets.push_back(shared_dir_ + "/" + name);
            }
            closedir(dir);
        }
        return peer_sockets;
    }

    static UniqueFd create_unix_socket() {
#ifdef SOCK_CLOEXEC
        UniqueFd fd(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
#else
        UniqueFd fd(socket(AF_UNIX, SOCK_STREAM, 0));
        set_cloexec(fd.get());
#endif
        return fd;
    }

    UniqueFd try_receive_memfd(const std::string& their_path) noexcept {
        auto peer_fd = create_unix_socket();
        if (!peer_fd.is_valid())
            return {};

        // 1-second timeout for connect and receive
        struct timeval tv = {1, 0};
        setsockopt(peer_fd.get(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        setsockopt(peer_fd.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        struct sockaddr_un addr = {};
        addr.sun_family         = AF_UNIX;
        std::strncpy(addr.sun_path, their_path.c_str(), sizeof(addr.sun_path) - 1);

        // Connect to peer socket and request access to the memfd
        int ret;
        do
            ret = connect(peer_fd.get(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        while (ret < 0 && errno == EINTR);

        if (ret == 0)
        {
            struct msghdr   msg                = {};
            const usize     space              = CMSG_SPACE(sizeof(int));
            constexpr usize alignment          = alignof(struct cmsghdr);
            auto            v                  = std::make_unique<std::byte[]>(space + alignment);
            std::byte*      control_msg_buffer = align_ptr_up<alignment>(v.get());

            char         buf[1];
            struct iovec iov[1];
            iov[0].iov_base = buf;
            iov[0].iov_len  = 1;

            msg.msg_iov        = iov;
            msg.msg_iovlen     = 1;
            msg.msg_control    = control_msg_buffer;
            msg.msg_controllen = space;

            ssize_t bytes_recv;
#ifdef MSG_CMSG_CLOEXEC
            int flags = MSG_CMSG_CLOEXEC;
#else
            int flags = 0;
#endif
            do
                bytes_recv = recvmsg(peer_fd.get(), &msg, flags);
            while (bytes_recv < 0 && errno == EINTR);

            if (bytes_recv > 0)
            {
                cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
                // Receive rights to the memfd from the peer; see make_server_thread
                if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS)
                {
                    int received_fd;
                    memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(received_fd));
#ifndef MSG_CMSG_CLOEXEC
                    set_cloexec(received_fd);
#endif
                    return UniqueFd(received_fd);
                }
            }
        }
        else if (errno == ECONNREFUSED || errno == ENOENT)
            // Failed to connect, clean up dead peer
            unlink(their_path.c_str());

        return {};
    }

    // Server thread:
    //  - Forwards the file descriptor fd
    //  - Exits when shutdown_receiver is hung up on
    //  - Listens on server_fd
    static NativeThread
    make_server_thread(UniqueFd fd, UniqueFd shutdown_receiver, UniqueFd server_fd) {
        return create_native_thread(
          NativeThreadOptions{},
          [fd = std::move(fd), shutdown_receiver = std::move(shutdown_receiver),
           server_fd = std::move(server_fd)]() {
              enum {
                  FdServer,
                  FdShutdown,
                  FdCount,
              };

              struct pollfd fds[FdCount];
              fds[FdServer].fd     = server_fd.get();
              fds[FdServer].events = POLLIN;

              fds[FdShutdown].fd     = shutdown_receiver.get();
              fds[FdShutdown].events = POLLIN;

              while (true)
              {
                  int ret = poll(fds, FdCount, -1);
                  if (ret < 0)
                  {
                      if (errno == EINTR)
                          continue;

                      break;
                  }

                  if (fds[FdShutdown].revents)
                      break;  // shutdown requested by main thread

                  if (fds[FdServer].revents & POLLIN)
                  {
                    // Another fish wants access
#if !defined(__APPLE__)
                      UniqueFd client_fd(accept4(server_fd.get(), nullptr, nullptr, SOCK_CLOEXEC));
#else
                      UniqueFd client_fd(accept(server_fd.get(), nullptr, nullptr));
                      set_cloexec(client_fd.get());
#endif
                      if (!client_fd.is_valid())
                          continue;  // including EINTR

                      struct msghdr   msg       = {};
                      const usize     space     = CMSG_SPACE(sizeof(int));
                      constexpr usize alignment = alignof(struct cmsghdr);
                      auto            v         = std::make_unique<std::byte[]>(space + alignment);
                      std::byte*      control_msg_buffer = align_ptr_up<alignment>(v.get());


                      char  buf[1] = {};
                      iovec iov[1];
                      iov[0].iov_base = buf;
                      iov[0].iov_len  = 1;

                      msg.msg_iov        = iov;
                      msg.msg_iovlen     = 1;
                      msg.msg_control    = control_msg_buffer;
                      msg.msg_controllen = space;

                      // Send over rights to the memfd (SCM_RIGHTS). The fd may be given a different number, but
                      // will refer to the same underlying file. Once it's mmapped then it will share physical memory
                      // between the processes.
                      // See https://man7.org/linux/man-pages/man7/unix.7.html for more information on SCM_RIGHTS
                      int             raw_fd = fd.get();
                      struct cmsghdr* cmsg   = CMSG_FIRSTHDR(&msg);
                      cmsg->cmsg_level       = SOL_SOCKET;
                      cmsg->cmsg_type        = SCM_RIGHTS;
                      cmsg->cmsg_len         = CMSG_LEN(sizeof(raw_fd));
                      memcpy(CMSG_DATA(cmsg), &raw_fd, sizeof(raw_fd));

#ifdef SO_NOSIGPIPE
                      int yes = 1;
                      setsockopt(client_fd.get(), SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif

#ifdef MSG_NOSIGNAL
                      int flags = MSG_NOSIGNAL;
#else
                      int flags = 0;
#endif
                      while (sendmsg(client_fd.get(), &msg, flags) < 0 && errno == EINTR)
                      {}
                  }
              }
          });
    }

   public:
    [[nodiscard]] bool open(const T& initial_value) noexcept {
        if (socket_path_.size() >= sizeof(sockaddr_un::sun_path))
            return false;

        if (mkdir(shared_dir_.c_str(), 0700) != 0 && errno != EEXIST)
            return false;

        {
            auto initLock = InitLock::wait_for_init_lock(init_lock_path_);
            if (!initLock.is_valid())
                return false;

            // Try to receive the shared memfd
            UniqueFd                 memfd;
            std::vector<std::string> peer_sockets = get_peer_sockets();
            for (const auto& sock_path : peer_sockets)
            {
                memfd = try_receive_memfd(sock_path);
                if (memfd.is_valid())
                    break;
            }

            const bool creator = !memfd.is_valid();  // We must create it

            if (creator)
            {
#if defined(MFD_CLOEXEC)
                // Failed to get it from a peer (no peers, or only dead peers), so create
                memfd.reset(memfd_create("replicated_data", MFD_CLOEXEC));
                if (!memfd.is_valid())
                    return false;
#else
                char temp_path[PATH_MAX];
                strncpy(temp_path, "/tmp/stockfish_replicated_data.XXXXXX", PATH_MAX);

                memfd.reset(mkstemp(temp_path));
                if (!memfd.is_valid())
                    return false;
                set_cloexec(memfd.get());
                unlink(temp_path);
#endif

                if (ftruncate(memfd.get(), sizeof(T)) != 0)
                    return false;
            }

            assert(memfd.is_valid());

            // Try to map the memfd
            T* mapped_mem = static_cast<T*>(detail::map_shared(memfd.get(), sizeof(T)));
            if (mapped_mem == MAP_FAILED)
                return false;

#ifdef MADV_HUGEPAGE
            (void) madvise(mapped_mem, sizeof(T), MADV_HUGEPAGE);
#endif

            if (creator)
            {
                // Creator is responsible for initialization
                *mapped_mem = initial_value;

#if defined(__linux__)
                (void) madvise(mapped_mem, sizeof(T), MADV_COLLAPSE);
#endif
            }

            mapped_ptr_ = data_ptr_ = mapped_mem;

            int shutdown_pipe[2];
#if !defined(__APPLE__)
            if (pipe2(shutdown_pipe, O_CLOEXEC) != 0)
                return false;
#else
            if (pipe(shutdown_pipe) != 0)
                return false;
            set_cloexec(shutdown_pipe[0]);
            set_cloexec(shutdown_pipe[1]);
#endif
            UniqueFd shutdown_receiver(shutdown_pipe[0]);
            shutdown_ = UniqueFd(shutdown_pipe[1]);

            auto server_fd = create_unix_socket();
            if (!server_fd.is_valid())
                return false;

            struct sockaddr_un addr = {};
            addr.sun_family         = AF_UNIX;
            strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

            detail::SharedMemoryRegistry::register_socket_path(socket_path_);
            unlink(socket_path_.c_str());
            if (bind(server_fd.get(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1
                || listen(server_fd.get(), 5) == -1)
                return false;

            // Don't release the init lock until we've actually made a socket that
            // other fishes can use.
            server_thread_ = make_server_thread(std::move(memfd), std::move(shutdown_receiver),
                                                std::move(server_fd));
            if (!server_thread_.joinable())
                return false;
        }

        return true;
    }
};

template<typename T>
[[nodiscard]] std::optional<SharedMemory<T>> create_shared(const std::string& name,
                                                           const T& initial_value) noexcept {
    auto tempRoot = TempRoot::get_temp_root();
    if (!tempRoot.has_value())
        return std::nullopt;
    SharedMemory<T> shm(name, *tempRoot);
    if (shm.open(initial_value))
        return shm;
    return std::nullopt;
}

}  // namespace Stockfish::shm

#endif  // #ifndef SHM_UNIX_H_INCLUDED
#endif



#if defined(_WIN32)

    #if _WIN32_WINNT < 0x0601
        #undef _WIN32_WINNT
        #define _WIN32_WINNT 0x0601  // Force to include needed API prototypes
    #endif

    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #include <windows.h>
#elif defined(__linux__)
    #include <cstring>
    #include <fcntl.h>
    #include <pthread.h>
    #include <semaphore.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif


#if defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <sys/syslimits.h>

#elif defined(__sun)
    #include <stdlib.h>

#elif defined(__FreeBSD__)
    #include <sys/sysctl.h>
    #include <sys/types.h>
    #include <unistd.h>

#elif defined(__NetBSD__) || defined(__DragonFly__) || defined(__linux__)
    #include <limits.h>
    #include <unistd.h>
#endif


namespace Stockfish {

// argv[0] CANNOT be used because we need to identify the executable.
// argv[0] contains the command used to invoke it, which does not involve the full path.
// Just using a path is not fully resilient either, as the executable could
// have changed if it wasn't locked by the OS. Ideally we would hash the executable
// but it's not really that important at this point.
// If the path is longer than 4095 bytes the hash will be computed from an unspecified
// amount of bytes of the path; in particular it can a hash of an empty string.

inline std::string getExecutablePathHash() {
    char  executable_path[4096] = {0};
    usize path_length           = 0;

#if defined(_WIN32)
    path_length = GetModuleFileNameA(NULL, executable_path, sizeof(executable_path));

#elif defined(__APPLE__)
    u32 size = sizeof(executable_path);
    if (_NSGetExecutablePath(executable_path, &size) == 0)
    {
        path_length = std::strlen(executable_path);
    }

#elif defined(__sun)  // Solaris
    const char* path = getexecname();
    if (path)
    {
        std::strncpy(executable_path, path, sizeof(executable_path) - 1);
        path_length = std::strlen(executable_path);
    }

#elif defined(__FreeBSD__)
    usize size   = sizeof(executable_path);
    int   mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    if (sysctl(mib, 4, executable_path, &size, NULL, 0) == 0)
    {
        path_length = std::strlen(executable_path);
    }

#elif defined(__NetBSD__) || defined(__DragonFly__)
    ssize_t len = readlink("/proc/curproc/exe", executable_path, sizeof(executable_path) - 1);
    if (len >= 0)
    {
        executable_path[len] = '\0';
        path_length          = len;
    }

#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", executable_path, sizeof(executable_path) - 1);
    if (len >= 0)
    {
        executable_path[len] = '\0';
        path_length          = len;
    }

#endif

    // In case of any error the path will be empty.
    return std::string(executable_path, path_length);
}

enum class SystemWideSharedConstantAllocationStatus {
    NoAllocation,
    LocalMemory,
    SharedMemory
};

#if defined(_WIN32)

inline std::string GetLastErrorAsString(DWORD error) {
    //Get the error message ID, if any.
    DWORD errorMessageID = error;
    if (errorMessageID == 0)
    {
        return std::string();  //No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    usize size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                                  | FORMAT_MESSAGE_IGNORE_INSERTS,
                                NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                (LPSTR) &messageBuffer, 0, NULL);

    //Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

// Utilizes shared memory to store the value. It is deduplicated system-wide (for the single user).
template<typename T>
class SharedMemoryBackend {
   public:
    enum class Status {
        Success,
        LargePageAllocationError,
        FileMappingError,
        MapViewError,
        MutexCreateError,
        MutexWaitError,
        MutexReleaseError,
        NotInitialized
    };

    static constexpr DWORD IS_INITIALIZED_VALUE = 1;

    SharedMemoryBackend() :
        status(Status::NotInitialized) {}

    SharedMemoryBackend(const std::string& shm_name, const T& value) :
        status(Status::NotInitialized) {

        initialize(shm_name, value);
    }

    bool is_valid() const { return status == Status::Success; }

    std::optional<std::string> get_error_message() const {
        switch (status)
        {
        case Status::Success :
            return std::nullopt;
        case Status::LargePageAllocationError :
            return "Failed to allocate large page memory";
        case Status::FileMappingError :
            return "Failed to create file mapping: " + last_error_message;
        case Status::MapViewError :
            return "Failed to map view: " + last_error_message;
        case Status::MutexCreateError :
            return "Failed to create mutex: " + last_error_message;
        case Status::MutexWaitError :
            return "Failed to wait on mutex: " + last_error_message;
        case Status::MutexReleaseError :
            return "Failed to release mutex: " + last_error_message;
        case Status::NotInitialized :
            return "Not initialized";
        default :
            return "Unknown error";
        }
    }

    void* get() const { return is_valid() ? pMap : nullptr; }

    ~SharedMemoryBackend() { cleanup(); }

    SharedMemoryBackend(const SharedMemoryBackend&)            = delete;
    SharedMemoryBackend& operator=(const SharedMemoryBackend&) = delete;

    SharedMemoryBackend(SharedMemoryBackend&& other) noexcept :
        pMap(other.pMap),
        hMapFile(other.hMapFile),
        status(other.status),
        last_error_message(std::move(other.last_error_message)) {

        other.pMap     = nullptr;
        other.hMapFile = 0;
        other.status   = Status::NotInitialized;
    }

    SharedMemoryBackend& operator=(SharedMemoryBackend&& other) noexcept {
        if (this != &other)
        {
            cleanup();
            pMap               = other.pMap;
            hMapFile           = other.hMapFile;
            status             = other.status;
            last_error_message = std::move(other.last_error_message);

            other.pMap     = nullptr;
            other.hMapFile = 0;
            other.status   = Status::NotInitialized;
        }
        return *this;
    }

    SystemWideSharedConstantAllocationStatus get_status() const {
        return status == Status::Success ? SystemWideSharedConstantAllocationStatus::SharedMemory
                                         : SystemWideSharedConstantAllocationStatus::NoAllocation;
    }

   private:
    void initialize(const std::string& shm_name, const T& value) {
        const usize total_size = sizeof(T) + sizeof(IS_INITIALIZED_VALUE);

        // Try allocating with large pages first.
        hMapFile = windows_try_with_large_page_privileges(
          [&](usize largePageSize) {
              const usize total_size_aligned =
                (total_size + largePageSize - 1) / largePageSize * largePageSize;

    #if defined(_WIN64)
              DWORD total_size_low  = total_size_aligned & 0xFFFFFFFFu;
              DWORD total_size_high = total_size_aligned >> 32u;
    #else
              DWORD total_size_low  = total_size_aligned;
              DWORD total_size_high = 0;
    #endif

              return CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                        PAGE_READWRITE | SEC_COMMIT | SEC_LARGE_PAGES,
                                        total_size_high, total_size_low, shm_name.c_str());
          },
          []() { return (void*) nullptr; });

        // Fallback to normal allocation if no large pages available.
        if (!hMapFile)
        {
            hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                          static_cast<DWORD>(total_size), shm_name.c_str());
        }

        if (!hMapFile)
        {
            const DWORD err    = GetLastError();
            last_error_message = GetLastErrorAsString(err);
            status             = Status::FileMappingError;
            return;
        }

        pMap = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, total_size);
        if (!pMap)
        {
            const DWORD err    = GetLastError();
            last_error_message = GetLastErrorAsString(err);
            status             = Status::MapViewError;
            cleanup();
            return;
        }

        // Use named mutex to ensure only one initializer
        std::string mutex_name = shm_name + "$mutex";
        HANDLE      hMutex     = CreateMutexA(NULL, FALSE, mutex_name.c_str());
        if (!hMutex)
        {
            const DWORD err    = GetLastError();
            last_error_message = GetLastErrorAsString(err);
            status             = Status::MutexCreateError;
            cleanup();
            return;
        }

        DWORD wait_result = WaitForSingleObject(hMutex, INFINITE);
        if (wait_result != WAIT_OBJECT_0)
        {
            const DWORD err    = GetLastError();
            last_error_message = GetLastErrorAsString(err);
            status             = Status::MutexWaitError;
            CloseHandle(hMutex);
            cleanup();
            return;
        }

        // Crucially, we place the object first to ensure alignment.
        volatile DWORD* is_initialized =
          std::launder(reinterpret_cast<DWORD*>(reinterpret_cast<char*>(pMap) + sizeof(T)));
        T* object = std::launder(reinterpret_cast<T*>(pMap));

        if (*is_initialized != IS_INITIALIZED_VALUE)
        {
            // First time initialization, message for debug purposes
            new (object) T{value};
            *is_initialized = IS_INITIALIZED_VALUE;
        }

        BOOL release_result = ReleaseMutex(hMutex);
        CloseHandle(hMutex);

        if (!release_result)
        {
            const DWORD err    = GetLastError();
            last_error_message = GetLastErrorAsString(err);
            status             = Status::MutexReleaseError;
            cleanup();
            return;
        }

        status = Status::Success;
    }

    void cleanup() {
        if (pMap != nullptr)
        {
            UnmapViewOfFile(pMap);
            pMap = nullptr;
        }
        if (hMapFile)
        {
            CloseHandle(hMapFile);
            hMapFile = 0;
        }
    }

    void*       pMap     = nullptr;
    HANDLE      hMapFile = 0;
    Status      status   = Status::NotInitialized;
    std::string last_error_message;
};

#elif defined(USE_UNIX_SHM)

template<typename T>
class SharedMemoryBackend {
   public:
    SharedMemoryBackend() = default;

    SharedMemoryBackend(const std::string& shm_name, const T& value) :
        shm1(shm::create_shared<T>(shm_name, value)) {}

    void* get() const {
        const T* ptr = &shm1->get();
        return reinterpret_cast<void*>(const_cast<T*>(ptr));
    }

    bool is_valid() const { return shm1 && shm1->is_mapped() && shm1->is_serving(); }

    SystemWideSharedConstantAllocationStatus get_status() const {
        return is_valid() ? SystemWideSharedConstantAllocationStatus::SharedMemory
                          : SystemWideSharedConstantAllocationStatus::NoAllocation;
    }

    std::optional<std::string> get_error_message() const {
        if (!shm1)
            return "Shared memory not initialized";

        if (!shm1->is_mapped())
            return "Shared memory is not mapped";

        if (!shm1->is_serving())
            return "Shared memory is not serving to other processes";

        return std::nullopt;
    }

   private:
    std::optional<shm::SharedMemory<T>> shm1;
};

#else

// For systems that don't have shared memory, or support is troublesome.
// The way fallback is done is that we need a dummy backend.

template<typename T>
class SharedMemoryBackend {
   public:
    SharedMemoryBackend() = default;

    SharedMemoryBackend([[maybe_unused]] const std::string& shm_name,
                        [[maybe_unused]] const T&           value) {}

    void* get() const { return nullptr; }

    bool is_valid() const { return false; }

    SystemWideSharedConstantAllocationStatus get_status() const {
        return SystemWideSharedConstantAllocationStatus::NoAllocation;
    }

    std::optional<std::string> get_error_message() const { return "Dummy SharedMemoryBackend"; }
};

#endif

template<typename T>
struct SharedMemoryBackendFallback {
    SharedMemoryBackendFallback() = default;

    SharedMemoryBackendFallback(const std::string&, const T& value) :
        fallback_object(make_unique_large_page<T>(value)) {}

    void* get() const { return fallback_object.get(); }

    SharedMemoryBackendFallback(const SharedMemoryBackendFallback&)            = delete;
    SharedMemoryBackendFallback& operator=(const SharedMemoryBackendFallback&) = delete;

    SharedMemoryBackendFallback(SharedMemoryBackendFallback&& other) noexcept :
        fallback_object(std::move(other.fallback_object)) {}

    SharedMemoryBackendFallback& operator=(SharedMemoryBackendFallback&& other) noexcept {
        fallback_object = std::move(other.fallback_object);
        return *this;
    }

    SystemWideSharedConstantAllocationStatus get_status() const {
        return fallback_object == nullptr ? SystemWideSharedConstantAllocationStatus::NoAllocation
                                          : SystemWideSharedConstantAllocationStatus::LocalMemory;
    }

    std::optional<std::string> get_error_message() const {
        if (fallback_object == nullptr)
            return "Not initialized";

        return "Shared memory not supported by the OS. Local allocation fallback.";
    }

   private:
    LargePagePtr<T> fallback_object;
};

// Platform-independent wrapper
template<typename T>
struct SystemWideSharedConstant {
   private:
    static std::string createHashString(const std::string& input) {
        char buf[1024];
        std::snprintf(buf, sizeof(buf), "%016" PRIx64, hash_string(input));
        return buf;
    }

   public:
    // We can't run the destructor because it may be in a completely different process.
    // The object stored must also be obviously in-line but we can't check for that, other than some basic checks that cover most cases.
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(std::is_trivially_move_constructible_v<T>);
    static_assert(std::is_trivially_copy_constructible_v<T>);

    SystemWideSharedConstant() = default;


    // Content is addressed by its hash. An additional discriminator can be added to account for differences
    // that are not present in the content, for example NUMA node allocation.
    SystemWideSharedConstant(const T& value, usize discriminator = 0) {
        usize content_hash    = std::hash<T>{}(value);
        usize executable_hash = hash_string(getExecutablePathHash());

        char buf[1024];
        std::snprintf(buf, sizeof(buf), "Local\\sf_%zu$%zu$%zu", content_hash, executable_hash,
                      discriminator);
        std::string shm_name = buf;

#if defined(USE_UNIX_SHM)
        // POSIX shared memory names must start with a slash
        shm_name = "/sf_" + createHashString(shm_name);

        // hash name and make sure it is not longer than SF_MAX_SEM_NAME_LEN
        if (shm_name.size() > SF_MAX_SEM_NAME_LEN)
        {
            shm_name = shm_name.substr(0, SF_MAX_SEM_NAME_LEN - 1);
        }
#endif

        SharedMemoryBackend<T> shm_backend(shm_name, value);

        if (shm_backend.is_valid())
        {
            backend = std::move(shm_backend);
        }
        else
        {
            backend = SharedMemoryBackendFallback<T>(shm_name, value);
        }
    }

    SystemWideSharedConstant(const SystemWideSharedConstant&)            = delete;
    SystemWideSharedConstant& operator=(const SystemWideSharedConstant&) = delete;

    SystemWideSharedConstant(SystemWideSharedConstant&& other) noexcept :
        backend(std::move(other.backend)) {}

    SystemWideSharedConstant& operator=(SystemWideSharedConstant&& other) noexcept {
        backend = std::move(other.backend);
        return *this;
    }

    const T& operator*() const { return *std::launder(reinterpret_cast<const T*>(get_ptr())); }

    bool operator==(std::nullptr_t) const noexcept { return get_ptr() == nullptr; }

    bool operator!=(std::nullptr_t) const noexcept { return get_ptr() != nullptr; }

    SystemWideSharedConstantAllocationStatus get_status() const {
        return std::visit(
          [](const auto& end) -> SystemWideSharedConstantAllocationStatus {
              if constexpr (std::is_same_v<std::decay_t<decltype(end)>, std::monostate>)
              {
                  return SystemWideSharedConstantAllocationStatus::NoAllocation;
              }
              else
              {
                  return end.get_status();
              }
          },
          backend);
    }

    std::optional<std::string> get_error_message() const {
        return std::visit(
          [](const auto& end) -> std::optional<std::string> {
              if constexpr (std::is_same_v<std::decay_t<decltype(end)>, std::monostate>)
              {
                  return std::nullopt;
              }
              else
              {
                  return end.get_error_message();
              }
          },
          backend);
    }

   private:
    auto get_ptr() const {
        return std::visit(
          [](const auto& end) -> void* {
              if constexpr (std::is_same_v<std::decay_t<decltype(end)>, std::monostate>)
              {
                  return nullptr;
              }
              else
              {
                  return end.get();
              }
          },
          backend);
    }

    std::variant<std::monostate, SharedMemoryBackend<T>, SharedMemoryBackendFallback<T>> backend;
};


}  // namespace Stockfish

#endif  // #ifndef SHM_H_INCLUDED

// We support linux very well, but we explicitly do NOT support Android,
// because there is no affected systems, not worth maintaining.
#if defined(__linux__) && !defined(__ANDROID__)
    #if !defined(_GNU_SOURCE)
        #define _GNU_SOURCE
    #endif
    #include <sched.h>
#elif defined(_WIN64)

    #if _WIN32_WINNT < 0x0601
        #undef _WIN32_WINNT
        #define _WIN32_WINNT 0x0601  // Force to include needed API prototypes
    #endif

    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #include <windows.h>
    #if defined small
        #undef small
    #endif

// https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadselectedcpusetmasks
using SetThreadSelectedCpuSetMasks_t = BOOL (*)(HANDLE, PGROUP_AFFINITY, USHORT);

// https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadselectedcpusetmasks
using GetThreadSelectedCpuSetMasks_t = BOOL (*)(HANDLE, PGROUP_AFFINITY, USHORT, PUSHORT);

#endif

namespace Stockfish {

using CpuIndex  = usize;
using NumaIndex = usize;

inline CpuIndex get_hardware_concurrency() {
    CpuIndex concurrency = std::thread::hardware_concurrency();

    // Get all processors across all processor groups on windows, since
    // hardware_concurrency() only returns the number of processors in
    // the first group, because only these are available to std::thread.
#ifdef _WIN64
    concurrency = std::max<CpuIndex>(concurrency, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
#endif

    return concurrency;
}

inline const CpuIndex SYSTEM_THREADS_NB = std::max<CpuIndex>(1, get_hardware_concurrency());

#if defined(_WIN64)

// On Windows each processor group can have up to 64 processors.
// https://learn.microsoft.com/en-us/windows/win32/procthread/processor-groups
static constexpr usize WIN_PROCESSOR_GROUP_SIZE = 64;

struct WindowsAffinity {
    std::optional<std::set<CpuIndex>> oldApi;
    std::optional<std::set<CpuIndex>> newApi;

    // We also provide diagnostic for when the affinity is set to nullopt
    // whether it was due to being indeterminate. If affinity is indeterminate
    // it is best to assume it is not set at all, so consistent with the meaning
    // of the nullopt affinity.
    bool isNewDeterminate = true;
    bool isOldDeterminate = true;

    std::optional<std::set<CpuIndex>> get_combined() const {
        if (!oldApi.has_value())
            return newApi;
        if (!newApi.has_value())
            return oldApi;

        std::set<CpuIndex> intersect;
        std::set_intersection(oldApi->begin(), oldApi->end(), newApi->begin(), newApi->end(),
                              std::inserter(intersect, intersect.begin()));
        return intersect;
    }

    // Since Windows 11 and Windows Server 2022 thread affinities can span
    // processor groups and can be set as such by a new WinAPI function. However,
    // we may need to force using the old API if we detect that the process has
    // affinity set by the old API already and we want to override that. Due to the
    // limitations of the old API we cannot detect its use reliably. There will be
    // cases where we detect not use but it has actually been used and vice versa.

    bool likely_used_old_api() const { return oldApi.has_value() || !isOldDeterminate; }
};

inline std::pair<BOOL, std::vector<USHORT>> get_process_group_affinity() {

    // GetProcessGroupAffinity requires the GroupArray argument to be
    // aligned to 4 bytes instead of just 2.
    static constexpr usize GroupArrayMinimumAlignment = 4;
    static_assert(GroupArrayMinimumAlignment >= alignof(USHORT));

    // The function should succeed the second time, but it may fail if the group
    // affinity has changed between GetProcessGroupAffinity calls. In such case
    // we consider this a hard error, as we Cannot work with unstable affinities
    // anyway.
    static constexpr int MAX_TRIES  = 2;
    USHORT               GroupCount = 1;
    for (int i = 0; i < MAX_TRIES; ++i)
    {
        auto GroupArray = std::make_unique<USHORT[]>(
          GroupCount + (GroupArrayMinimumAlignment / alignof(USHORT) - 1));

        USHORT* GroupArrayAligned = align_ptr_up<GroupArrayMinimumAlignment>(GroupArray.get());

        const BOOL status =
          GetProcessGroupAffinity(GetCurrentProcess(), &GroupCount, GroupArrayAligned);

        if (status == 0 && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            break;

        if (status != 0)
            return std::make_pair(status,
                                  std::vector(GroupArrayAligned, GroupArrayAligned + GroupCount));
    }

    return std::make_pair(0, std::vector<USHORT>());
}

// On Windows there are two ways to set affinity, and therefore 2 ways to get it.
// These are not consistent, so we have to check both. In some cases it is actually
// not possible to determine affinity. For example when two different threads have
// affinity on different processor groups, set using SetThreadAffinityMask, we cannot
// retrieve the actual affinities.
// From documentation on GetProcessAffinityMask:
//     > If the calling process contains threads in multiple groups,
//     > the function returns zero for both affinity masks.
// In such cases we just give up and assume we have affinity for all processors.
// nullopt means no affinity is set, that is, all processors are allowed
inline WindowsAffinity get_process_affinity() {
    HMODULE k32                            = GetModuleHandle(TEXT("Kernel32.dll"));
    auto    GetThreadSelectedCpuSetMasks_f = GetThreadSelectedCpuSetMasks_t(
      (void (*)()) GetProcAddress(k32, "GetThreadSelectedCpuSetMasks"));

    BOOL status = 0;

    WindowsAffinity affinity;

    if (GetThreadSelectedCpuSetMasks_f != nullptr)
    {
        USHORT RequiredMaskCount;
        status = GetThreadSelectedCpuSetMasks_f(GetCurrentThread(), nullptr, 0, &RequiredMaskCount);

        // We expect ERROR_INSUFFICIENT_BUFFER from GetThreadSelectedCpuSetMasks,
        // but other failure is an actual error.
        if (status == 0 && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            affinity.isNewDeterminate = false;
        else if (RequiredMaskCount > 0)
        {
            // If RequiredMaskCount then these affinities were never set, but it's
            // not consistent so GetProcessAffinityMask may still return some affinity.
            auto groupAffinities = std::make_unique<GROUP_AFFINITY[]>(RequiredMaskCount);

            status = GetThreadSelectedCpuSetMasks_f(GetCurrentThread(), groupAffinities.get(),
                                                    RequiredMaskCount, &RequiredMaskCount);

            if (status == 0)
                affinity.isNewDeterminate = false;
            else
            {
                std::set<CpuIndex> cpus;

                for (USHORT i = 0; i < RequiredMaskCount; ++i)
                {
                    const usize procGroupIndex = groupAffinities[i].Group;

                    for (usize j = 0; j < WIN_PROCESSOR_GROUP_SIZE; ++j)
                        if (groupAffinities[i].Mask & (KAFFINITY(1) << j))
                            cpus.insert(procGroupIndex * WIN_PROCESSOR_GROUP_SIZE + j);
                }

                affinity.newApi = std::move(cpus);
            }
        }
    }

    // NOTE: There is no way to determine full affinity using the old API if
    //       individual threads set affinity on different processor groups.

    DWORD_PTR proc, sys;
    status = GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys);

    // If proc == 0 then we cannot determine affinity because it spans processor groups.
    // On Windows 11 and Server 2022 it will instead
    //     > If, however, hHandle specifies a handle to the current process, the function
    //     > always uses the calling thread's primary group (which by default is the same
    //     > as the process' primary group) in order to set the
    //     > lpProcessAffinityMask and lpSystemAffinityMask.
    // So it will never be indeterminate here. We can only make assumptions later.
    if (status == 0 || proc == 0)
    {
        affinity.isOldDeterminate = false;
        return affinity;
    }

    // If SetProcessAffinityMask was never called the affinity must span
    // all processor groups, but if it was called it must only span one.

    std::vector<USHORT> groupAffinity;  // We need to capture this later and capturing
                                        // from structured bindings requires c++20.

    std::tie(status, groupAffinity) = get_process_group_affinity();
    if (status == 0)
    {
        affinity.isOldDeterminate = false;
        return affinity;
    }

    if (groupAffinity.size() == 1)
    {
        // We detect the case when affinity is set to all processors and correctly
        // leave affinity.oldApi as nullopt.
        if (GetActiveProcessorGroupCount() != 1 || proc != sys)
        {
            std::set<CpuIndex> cpus;

            const usize procGroupIndex = groupAffinity[0];

            const u64 mask = static_cast<u64>(proc);
            for (usize j = 0; j < WIN_PROCESSOR_GROUP_SIZE; ++j)
                if (mask & (KAFFINITY(1) << j))
                    cpus.insert(procGroupIndex * WIN_PROCESSOR_GROUP_SIZE + j);

            affinity.oldApi = std::move(cpus);
        }
    }
    else
    {
        // If we got here it means that either SetProcessAffinityMask was never set
        // or we're on Windows 11/Server 2022.

        // Since Windows 11 and Windows Server 2022 the behaviour of
        // GetProcessAffinityMask changed:
        //     > If, however, hHandle specifies a handle to the current process,
        //     > the function always uses the calling thread's primary group
        //     > (which by default is the same as the process' primary group)
        //     > in order to set the lpProcessAffinityMask and lpSystemAffinityMask.
        // In which case we can actually retrieve the full affinity.

        if (GetThreadSelectedCpuSetMasks_f != nullptr)
        {
            NativeThread th = create_native_thread(NativeThreadOptions{}, [&]() {
                std::set<CpuIndex> cpus;
                bool               isAffinityFull = true;

                for (auto procGroupIndex : groupAffinity)
                {
                    const int numActiveProcessors =
                      GetActiveProcessorCount(static_cast<WORD>(procGroupIndex));

                    // We have to schedule to two different processors
                    // and & the affinities we get. Otherwise our processor
                    // choice could influence the resulting affinity.
                    // We assume the processor IDs within the group are
                    // filled sequentially from 0.
                    u64 procCombined = std::numeric_limits<u64>::max();
                    u64 sysCombined  = std::numeric_limits<u64>::max();

                    for (int i = 0; i < std::min(numActiveProcessors, 2); ++i)
                    {
                        GROUP_AFFINITY GroupAffinity;
                        std::memset(&GroupAffinity, 0, sizeof(GROUP_AFFINITY));
                        GroupAffinity.Group = static_cast<WORD>(procGroupIndex);

                        GroupAffinity.Mask = static_cast<KAFFINITY>(1) << i;

                        status =
                          SetThreadGroupAffinity(GetCurrentThread(), &GroupAffinity, nullptr);
                        if (status == 0)
                        {
                            affinity.isOldDeterminate = false;
                            return;
                        }

                        SwitchToThread();

                        DWORD_PTR proc2, sys2;
                        status = GetProcessAffinityMask(GetCurrentProcess(), &proc2, &sys2);
                        if (status == 0)
                        {
                            affinity.isOldDeterminate = false;
                            return;
                        }

                        procCombined &= static_cast<u64>(proc2);
                        sysCombined &= static_cast<u64>(sys2);
                    }

                    if (procCombined != sysCombined)
                        isAffinityFull = false;

                    for (usize j = 0; j < WIN_PROCESSOR_GROUP_SIZE; ++j)
                        if (procCombined & (KAFFINITY(1) << j))
                            cpus.insert(procGroupIndex * WIN_PROCESSOR_GROUP_SIZE + j);
                }

                // We have to detect the case where the affinity was not set,
                // or is set to all processors so that we correctly produce as
                // std::nullopt result.
                if (!isAffinityFull)
                    affinity.oldApi = std::move(cpus);
            });

            th.join();
        }
    }

    return affinity;
}

// Type machinery used to emulate Cache->GroupCount

template<typename T, typename = void>
struct HasGroupCount: std::false_type {};

template<typename T>
struct HasGroupCount<T, std::void_t<decltype(std::declval<T>().Cache.GroupCount)>>: std::true_type {
};

template<typename T, typename Pred, std::enable_if_t<HasGroupCount<T>::value, bool> = true>
std::set<CpuIndex> readCacheMembers(const T* info, Pred&& is_cpu_allowed) {
    std::set<CpuIndex> cpus;
    // On Windows 10 this will read a 0 because GroupCount doesn't exist
    int groupCount = std::max(info->Cache.GroupCount, WORD(1));
    for (WORD procGroup = 0; procGroup < groupCount; ++procGroup)
    {
        for (BYTE number = 0; number < WIN_PROCESSOR_GROUP_SIZE; ++number)
        {
            WORD           groupNumber = info->Cache.GroupMasks[procGroup].Group;
            const CpuIndex c = static_cast<CpuIndex>(groupNumber) * WIN_PROCESSOR_GROUP_SIZE
                             + static_cast<CpuIndex>(number);
            if (!(info->Cache.GroupMasks[procGroup].Mask & (1ULL << number)) || !is_cpu_allowed(c))
                continue;
            cpus.insert(c);
        }
    }
    return cpus;
}

template<typename T, typename Pred, std::enable_if_t<!HasGroupCount<T>::value, bool> = true>
std::set<CpuIndex> readCacheMembers(const T* info, Pred&& is_cpu_allowed) {
    std::set<CpuIndex> cpus;
    for (BYTE number = 0; number < WIN_PROCESSOR_GROUP_SIZE; ++number)
    {
        WORD           groupNumber = info->Cache.GroupMask.Group;
        const CpuIndex c           = static_cast<CpuIndex>(groupNumber) * WIN_PROCESSOR_GROUP_SIZE
                         + static_cast<CpuIndex>(number);
        if (!(info->Cache.GroupMask.Mask & (1ULL << number)) || !is_cpu_allowed(c))
            continue;
        cpus.insert(c);
    }
    return cpus;
}

#endif

#if defined(__linux__) && !defined(__ANDROID__)

inline std::set<CpuIndex> get_process_affinity() {

    std::set<CpuIndex> cpus;

    // For unsupported systems, or in case of a soft error, we may assume
    // all processors are available for use.
    [[maybe_unused]] auto set_to_all_cpus = [&]() {
        for (CpuIndex c = 0; c < SYSTEM_THREADS_NB; ++c)
            cpus.insert(c);
    };

    // cpu_set_t by default holds 1024 entries. This may not be enough soon,
    // but there is no easy way to determine how many threads there actually
    // is. In this case we just choose a reasonable upper bound.
    static constexpr CpuIndex MaxNumCpus = 1024 * 64;

    cpu_set_t* mask = CPU_ALLOC(MaxNumCpus);
    if (mask == nullptr)
        std::exit(EXIT_FAILURE);

    const usize masksize = CPU_ALLOC_SIZE(MaxNumCpus);

    CPU_ZERO_S(masksize, mask);

    const int status = sched_getaffinity(0, masksize, mask);

    if (status != 0)
    {
        CPU_FREE(mask);
        std::exit(EXIT_FAILURE);
    }

    for (CpuIndex c = 0; c < MaxNumCpus; ++c)
        if (CPU_ISSET_S(c, masksize, mask))
            cpus.insert(c);

    CPU_FREE(mask);

    return cpus;
}

#endif

#if defined(__linux__) && !defined(__ANDROID__)

inline static const auto STARTUP_PROCESSOR_AFFINITY = get_process_affinity();

#elif defined(_WIN64)

inline static const auto STARTUP_PROCESSOR_AFFINITY = get_process_affinity();
inline static const auto STARTUP_USE_OLD_AFFINITY_API =
  STARTUP_PROCESSOR_AFFINITY.likely_used_old_api();

#endif

// We want to abstract the purpose of storing the numa node index somewhat.
// Whoever is using this does not need to know the specifics of the replication
// machinery to be able to access NUMA replicated memory.
class NumaReplicatedAccessToken {
   public:
    NumaReplicatedAccessToken() :
        n(0) {}

    explicit NumaReplicatedAccessToken(NumaIndex idx) :
        n(idx) {}

    NumaIndex get_numa_index() const { return n; }

   private:
    NumaIndex n;
};

struct L3Domain {
    NumaIndex          systemNumaIndex{};
    std::set<CpuIndex> cpus{};
};

// Use system NUMA nodes
struct SystemNumaPolicy {};
// Use system-reported L3 domains
struct L3DomainsPolicy {};
// Group system-reported L3 domains until they reach bundleSize
struct BundledL3Policy {
    usize bundleSize;
};

using NumaAutoPolicy = std::variant<SystemNumaPolicy, L3DomainsPolicy, BundledL3Policy>;

// Designed as immutable, because there is no good reason to alter an already
// existing config in a way that doesn't require recreating it completely, and
// it would be complex and expensive to maintain class invariants.
// The CPU (processor) numbers always correspond to the actual numbering used
// by the system. The NUMA node numbers MAY NOT correspond to the system's
// numbering of the NUMA nodes. In particular, by default, if the processor has
// non-uniform cache access within a NUMA node (i.e., a non-unified L3 cache structure),
// then L3 domains within a system NUMA node will be used to subdivide it
// into multiple logical NUMA nodes in the config. Additionally, empty nodes may
// be removed, or the user may create custom nodes.
//
// As a special case, when performing system-wide replication of read-only data
// (i.e., LazyNumaReplicatedSystemWide), the system NUMA node is used, rather than
// custom or L3-aware nodes. See that class's get_discriminator() function.
//
// It is guaranteed that NUMA nodes are NOT empty: every node exposed by NumaConfig
// has at least one processor assigned.
//
// We use startup affinities so as not to modify its own behaviour in time.
//
// Since Stockfish doesn't support exceptions all places where an exception
// should be thrown are replaced by std::exit.
class NumaConfig {
   public:
    NumaConfig() :
        highestCpuIndex(0),
        customAffinity(false) {
        const auto numCpus = SYSTEM_THREADS_NB;
        add_cpu_range_to_node(NumaIndex{0}, CpuIndex{0}, numCpus - 1);
    }

    // This function gets a NumaConfig based on the system's provided information.
    // The available policies are documented above.
    static NumaConfig from_system([[maybe_unused]] const NumaAutoPolicy& policy,
                                  bool respectProcessAffinity = true) {
        NumaConfig cfg = empty();

#if !((defined(__linux__) && !defined(__ANDROID__)) || defined(_WIN64))
        // Fallback for unsupported systems.
        for (CpuIndex c = 0; c < SYSTEM_THREADS_NB; ++c)
            cfg.add_cpu_to_node(NumaIndex{0}, c);
#else

    #if defined(_WIN64)

        std::optional<std::set<CpuIndex>> allowedCpus;

        if (respectProcessAffinity)
            allowedCpus = STARTUP_PROCESSOR_AFFINITY.get_combined();

        // The affinity cannot be determined in all cases on Windows,
        // but we at least guarantee that the number of allowed processors
        // is >= number of processors in the affinity mask. In case the user
        // is not satisfied they must set the processor numbers explicitly.
        auto is_cpu_allowed = [&allowedCpus](CpuIndex c) {
            return !allowedCpus.has_value() || allowedCpus->count(c) == 1;
        };

    #elif defined(__linux__) && !defined(__ANDROID__)

        std::set<CpuIndex> allowedCpus;

        if (respectProcessAffinity)
            allowedCpus = STARTUP_PROCESSOR_AFFINITY;

        auto is_cpu_allowed = [respectProcessAffinity, &allowedCpus](CpuIndex c) {
            return !respectProcessAffinity || allowedCpus.count(c) == 1;
        };

    #endif

        bool l3Success = false;
        if (!std::holds_alternative<SystemNumaPolicy>(policy))
        {
            usize l3BundleSize = 0;
            if (const auto* v = std::get_if<BundledL3Policy>(&policy))
                l3BundleSize = v->bundleSize;
            if (auto l3Cfg =
                  try_get_l3_aware_config(respectProcessAffinity, l3BundleSize, is_cpu_allowed))
            {
                cfg       = std::move(*l3Cfg);
                l3Success = true;
            }
        }
        if (!l3Success)
            cfg = from_system_numa(respectProcessAffinity, is_cpu_allowed);

    #if defined(_WIN64)
        // Split the NUMA nodes to be contained within a group if necessary.
        // This is needed between Windows 10 Build 20348 and Windows 11, because
        // the new NUMA allocation behaviour was introduced while there was
        // still no way to set thread affinity spanning multiple processor groups.
        // See https://learn.microsoft.com/en-us/windows/win32/procthread/numa-support
        // We also do this is if need to force old API for some reason.
        //
        // 2024-08-26: It appears that we need to actually always force this behaviour.
        // While Windows allows this to work now, such assignments have bad interaction
        // with the scheduler - in particular it still prefers scheduling on the thread's
        // "primary" node, even if it means scheduling SMT processors first.
        // See https://github.com/official-stockfish/Stockfish/issues/5551
        // See https://learn.microsoft.com/en-us/windows/win32/procthread/processor-groups
        //
        //     Each process is assigned a primary group at creation, and by default all
        //     of its threads' primary group is the same. Each thread's ideal processor
        //     is in the thread's primary group, so threads will preferentially be
        //     scheduled to processors on their primary group, but they are able to
        //     be scheduled to processors on any other group.
        //
        // used to be guarded by if (STARTUP_USE_OLD_AFFINITY_API)
        NumaConfig splitCfg = empty();

        NumaIndex splitNodeIndex = 0;
        for (const auto& cpus : cfg.nodes)
        {
            if (cpus.empty())
                continue;

            usize lastProcGroupIndex = *(cpus.begin()) / WIN_PROCESSOR_GROUP_SIZE;
            for (CpuIndex c : cpus)
            {
                const usize procGroupIndex = c / WIN_PROCESSOR_GROUP_SIZE;
                if (procGroupIndex != lastProcGroupIndex)
                {
                    splitNodeIndex += 1;
                    lastProcGroupIndex = procGroupIndex;
                }
                splitCfg.add_cpu_to_node(splitNodeIndex, c);
            }
            splitNodeIndex += 1;
        }

        cfg = std::move(splitCfg);
    #endif

#endif

        // We have to ensure no empty NUMA nodes persist.
        cfg.remove_empty_numa_nodes();

        // If the user explicitly opts out from respecting the current process affinity
        // then it may be inconsistent with the current affinity (obviously), so we
        // consider it custom.
        if (!respectProcessAffinity)
            cfg.customAffinity = true;

        return cfg;
    }

    // ':'-separated numa nodes
    // ','-separated cpu indices
    // supports "first-last" range syntax for cpu indices
    // For example "0-15,128-143:16-31,144-159:32-47,160-175:48-63,176-191"
    static std::optional<NumaConfig> from_string(const std::string& s) {
        NumaConfig cfg = empty();

        NumaIndex n = 0;
        for (auto&& nodeStr : split(s, ":"))
        {
            auto indices = indices_from_shortened_string(std::string(nodeStr));
            if (!indices.empty())
            {
                for (auto idx : indices)
                    if (!cfg.add_cpu_to_node(n, CpuIndex(idx)))
                        return std::nullopt;

                n += 1;
            }
        }

        if (n == 0)  // failed to parse any nodes
            return std::nullopt;

        cfg.customAffinity = true;

        return cfg;
    }

    NumaConfig(const NumaConfig&)            = delete;
    NumaConfig(NumaConfig&&)                 = default;
    NumaConfig& operator=(const NumaConfig&) = delete;
    NumaConfig& operator=(NumaConfig&&)      = default;

    bool is_cpu_assigned(CpuIndex n) const { return nodeByCpu.count(n) == 1; }

    NumaIndex num_numa_nodes() const { return nodes.size(); }

    CpuIndex num_cpus_in_numa_node(NumaIndex n) const {
        assert(n < nodes.size());
        return nodes[n].size();
    }

    CpuIndex num_cpus() const { return nodeByCpu.size(); }

    bool requires_memory_replication() const { return customAffinity || nodes.size() > 1; }

    std::string to_string() const {
        std::string str;

        bool isFirstNode = true;
        for (auto&& cpus : nodes)
        {
            if (!isFirstNode)
                str += ":";

            bool isFirstSet = true;
            auto rangeStart = cpus.begin();
            for (auto it = cpus.begin(); it != cpus.end(); ++it)
            {
                auto next = std::next(it);
                if (next == cpus.end() || *next != *it + 1)
                {
                    // cpus[i] is at the end of the range (may be of size 1)
                    if (!isFirstSet)
                        str += ",";

                    const CpuIndex last = *it;

                    if (it != rangeStart)
                    {
                        const CpuIndex first = *rangeStart;

                        str += std::to_string(first);
                        str += "-";
                        str += std::to_string(last);
                    }
                    else
                        str += std::to_string(last);

                    rangeStart = next;
                    isFirstSet = false;
                }
            }

            isFirstNode = false;
        }

        return str;
    }

    bool suggests_binding_threads(CpuIndex numThreads) const {
        // If we can reasonably determine that the threads cannot be contained
        // by the OS within the first NUMA node then we advise distributing
        // and binding threads. When the threads are not bound we can only use
        // NUMA memory replicated objects from the first node, so when the OS
        // has to schedule on other nodes we lose performance. We also suggest
        // binding if there's enough threads to distribute among nodes with minimal
        // disparity. We try to ignore small nodes, in particular the empty ones.

        // If the affinity set by the user does not match the affinity given by
        // the OS then binding is necessary to ensure the threads are running on
        // correct processors.
        if (customAffinity)
            return true;

        // We obviously cannot distribute a single thread, so a single thread
        // should never be bound.
        if (numThreads <= 1)
            return false;

        usize largestNodeSize = 0;
        for (auto&& cpus : nodes)
            if (cpus.size() > largestNodeSize)
                largestNodeSize = cpus.size();

        auto is_node_small = [largestNodeSize](const std::set<CpuIndex>& node) {
            static constexpr double SmallNodeThreshold = 0.6;
            return static_cast<double>(node.size()) / static_cast<double>(largestNodeSize)
                <= SmallNodeThreshold;
        };

        usize numNotSmallNodes = 0;
        for (auto&& cpus : nodes)
            if (!is_node_small(cpus))
                numNotSmallNodes += 1;

        return (numThreads > largestNodeSize / 2 || numThreads >= numNotSmallNodes * 4)
            && nodes.size() > 1;
    }

    std::vector<NumaIndex> distribute_threads_among_numa_nodes(CpuIndex numThreads) const {
        std::vector<NumaIndex> ns;

        if (nodes.size() == 1)
        {
            // Special case for when there's no NUMA nodes. This doesn't buy us
            // much, but let's keep the default path simple.
            ns.resize(numThreads, NumaIndex{0});
        }
        else
        {
            std::vector<usize> occupation(nodes.size(), 0);
            for (CpuIndex c = 0; c < numThreads; ++c)
            {
                NumaIndex bestNode{0};
                float     bestNodeFill = std::numeric_limits<float>::max();
                for (NumaIndex n = 0; n < nodes.size(); ++n)
                {
                    float fill =
                      static_cast<float>(occupation[n] + 1) / static_cast<float>(nodes[n].size());
                    // NOTE: Do we want to perhaps fill the first available node
                    //       up to 50% first before considering other nodes?
                    //       Probably not, because it would interfere with running
                    //       multiple instances. We basically shouldn't favor any
                    //       particular node.
                    if (fill < bestNodeFill)
                    {
                        bestNode     = n;
                        bestNodeFill = fill;
                    }
                }
                ns.emplace_back(bestNode);
                occupation[bestNode] += 1;
            }
        }

        return ns;
    }

    NumaReplicatedAccessToken bind_current_thread_to_numa_node(NumaIndex n) const {
        if (n >= nodes.size() || nodes[n].empty())
            std::exit(EXIT_FAILURE);

#if defined(__linux__) && !defined(__ANDROID__)

        cpu_set_t* mask = CPU_ALLOC(highestCpuIndex + 1);
        if (mask == nullptr)
            std::exit(EXIT_FAILURE);

        const usize masksize = CPU_ALLOC_SIZE(highestCpuIndex + 1);

        CPU_ZERO_S(masksize, mask);

        for (CpuIndex c : nodes[n])
            CPU_SET_S(c, masksize, mask);

        const int status = sched_setaffinity(0, masksize, mask);

        CPU_FREE(mask);

        if (status != 0)
            std::exit(EXIT_FAILURE);

        // We yield this thread just to be sure it gets rescheduled.
        // This is defensive, allowed because this code is not performance critical.
        sched_yield();

#elif defined(_WIN64)

        // Requires Windows 11. No good way to set thread affinity spanning
        // processor groups before that.
        HMODULE k32                            = GetModuleHandle(TEXT("Kernel32.dll"));
        auto    SetThreadSelectedCpuSetMasks_f = SetThreadSelectedCpuSetMasks_t(
          (void (*)()) GetProcAddress(k32, "SetThreadSelectedCpuSetMasks"));

        // We ALWAYS set affinity with the new API if available, because
        // there's no downsides, and we forcibly keep it consistent with
        // the old API should we need to use it. I.e. we always keep this
        // as a superset of what we set with SetThreadGroupAffinity.
        if (SetThreadSelectedCpuSetMasks_f != nullptr)
        {
            // Only available on Windows 11 and Windows Server 2022 onwards
            const USHORT numProcGroups = USHORT(
              ((highestCpuIndex + 1) + WIN_PROCESSOR_GROUP_SIZE - 1) / WIN_PROCESSOR_GROUP_SIZE);
            auto groupAffinities = std::make_unique<GROUP_AFFINITY[]>(numProcGroups);
            std::memset(groupAffinities.get(), 0, sizeof(GROUP_AFFINITY) * numProcGroups);
            for (WORD i = 0; i < numProcGroups; ++i)
                groupAffinities[i].Group = i;

            for (CpuIndex c : nodes[n])
            {
                const usize procGroupIndex     = c / WIN_PROCESSOR_GROUP_SIZE;
                const usize idxWithinProcGroup = c % WIN_PROCESSOR_GROUP_SIZE;
                groupAffinities[procGroupIndex].Mask |= KAFFINITY(1) << idxWithinProcGroup;
            }

            HANDLE hThread = GetCurrentThread();

            const BOOL status =
              SetThreadSelectedCpuSetMasks_f(hThread, groupAffinities.get(), numProcGroups);
            if (status == 0)
                std::exit(EXIT_FAILURE);

            // We yield this thread just to be sure it gets rescheduled.
            // This is defensive, allowed because this code is not performance critical.
            SwitchToThread();
        }

        // Sometimes we need to force the old API, but do not use it unless necessary.
        if (SetThreadSelectedCpuSetMasks_f == nullptr || STARTUP_USE_OLD_AFFINITY_API)
        {
            // On earlier windows version (since windows 7) we cannot run a single thread
            // on multiple processor groups, so we need to restrict the group.
            // We assume the group of the first processor listed for this node.
            // Processors from outside this group will not be assigned for this thread.
            // Normally this won't be an issue because windows used to assign NUMA nodes
            // such that they cannot span processor groups. However, since Windows 10
            // Build 20348 the behaviour changed, so there's a small window of versions
            // between this and Windows 11 that might exhibit problems with not all
            // processors being utilized.
            //
            // We handle this in NumaConfig::from_system by manually splitting the
            // nodes when we detect that there is no function to set affinity spanning
            // processor nodes. This is required because otherwise our thread distribution
            // code may produce suboptimal results.
            //
            // See https://learn.microsoft.com/en-us/windows/win32/procthread/numa-support
            GROUP_AFFINITY affinity;
            std::memset(&affinity, 0, sizeof(GROUP_AFFINITY));
            // We use an ordered set to be sure to get the smallest cpu number here.
            const usize forcedProcGroupIndex = *(nodes[n].begin()) / WIN_PROCESSOR_GROUP_SIZE;
            affinity.Group                   = static_cast<WORD>(forcedProcGroupIndex);
            for (CpuIndex c : nodes[n])
            {
                const usize procGroupIndex     = c / WIN_PROCESSOR_GROUP_SIZE;
                const usize idxWithinProcGroup = c % WIN_PROCESSOR_GROUP_SIZE;
                // We skip processors that are not in the same processor group.
                // If everything was set up correctly this will never be an issue,
                // but we have to account for bad NUMA node specification.
                if (procGroupIndex != forcedProcGroupIndex)
                    continue;

                affinity.Mask |= KAFFINITY(1) << idxWithinProcGroup;
            }

            HANDLE hThread = GetCurrentThread();

            const BOOL status = SetThreadGroupAffinity(hThread, &affinity, nullptr);
            if (status == 0)
                std::exit(EXIT_FAILURE);

            // We yield this thread just to be sure it gets rescheduled. This is
            // defensive, allowed because this code is not performance critical.
            SwitchToThread();
        }

#endif

        return NumaReplicatedAccessToken(n);
    }

    template<typename FuncT>
    void execute_on_numa_node(NumaIndex n, FuncT&& f) const {
        NativeThread th = create_native_thread(NativeThreadOptions{}, [this, &f, n]() {
            bind_current_thread_to_numa_node(n);
            std::forward<FuncT>(f)();
        });

        if (!th.joinable())
        {
            std::cerr << "Failed to execute function on NUMA node\n";
            std::exit(EXIT_FAILURE);
        }

        th.join();
    }

    std::vector<std::set<CpuIndex>> nodes;
    std::map<CpuIndex, NumaIndex>   nodeByCpu;

   private:
    CpuIndex highestCpuIndex;

    bool customAffinity;

    static NumaConfig empty() { return NumaConfig(EmptyNodeTag{}); }

    struct EmptyNodeTag {};

    NumaConfig(EmptyNodeTag) :
        highestCpuIndex(0),
        customAffinity(false) {}

    void remove_empty_numa_nodes() {
        std::vector<std::set<CpuIndex>> newNodes;
        for (auto&& cpus : nodes)
            if (!cpus.empty())
                newNodes.emplace_back(std::move(cpus));
        nodes = std::move(newNodes);
    }

    // Returns true if successful
    // Returns false if failed, i.e. when the cpu is already present
    //                          strong guarantee, the structure remains unmodified
    bool add_cpu_to_node(NumaIndex n, CpuIndex c) {
        if (is_cpu_assigned(c))
            return false;

        while (nodes.size() <= n)
            nodes.emplace_back();

        nodes[n].insert(c);
        nodeByCpu[c] = n;

        if (c > highestCpuIndex)
            highestCpuIndex = c;

        return true;
    }

    // Returns true if successful
    // Returns false if failed, i.e. when any of the cpus is already present
    //                          strong guarantee, the structure remains unmodified
    bool add_cpu_range_to_node(NumaIndex n, CpuIndex cfirst, CpuIndex clast) {
        for (CpuIndex c = cfirst; c <= clast; ++c)
            if (is_cpu_assigned(c))
                return false;

        while (nodes.size() <= n)
            nodes.emplace_back();

        for (CpuIndex c = cfirst; c <= clast; ++c)
        {
            nodes[n].insert(c);
            nodeByCpu[c] = n;
        }

        if (clast > highestCpuIndex)
            highestCpuIndex = clast;

        return true;
    }

    static std::vector<usize> indices_from_shortened_string(const std::string& s) {
        std::vector<usize> indices;

        if (s.empty())
            return indices;

        for (const auto& ss : split(s, ","))
        {
            if (ss.empty())
                continue;

            auto parts = split(ss, "-");
            if (parts.size() == 1)
            {
                auto c = str_to_size_t(std::string(parts[0]));
                if (c.has_value())
                    indices.emplace_back(*c);
            }
            else if (parts.size() == 2)
            {
                constexpr usize MaxIndices = 1 << 20;  // prevent oom

                auto cfirst = str_to_size_t(std::string(parts[0]));
                auto clast  = str_to_size_t(std::string(parts[1]));
                if (cfirst.has_value() && clast.has_value() && *clast - *cfirst < MaxIndices)
                    for (usize c = *cfirst; c <= *clast; ++c)
                        indices.emplace_back(c);
            }
        }

        return indices;
    }

    // This function queries the system for the mapping of processors to NUMA nodes.
    // On Linux we read from standardized kernel sysfs, with a fallback to single NUMA
    // node. On Windows we utilize GetNumaProcessorNodeEx, which has its quirks, see
    // comment for Windows implementation of get_process_affinity.
    template<typename Pred>
    static NumaConfig from_system_numa([[maybe_unused]] bool   respectProcessAffinity,
                                       [[maybe_unused]] Pred&& is_cpu_allowed) {
        NumaConfig cfg = empty();

#if defined(__linux__) && !defined(__ANDROID__)

        // On Linux things are straightforward, since there's no processor groups and
        // any thread can be scheduled on all processors.
        // We try to gather this information from the sysfs first
        // https://www.kernel.org/doc/Documentation/ABI/stable/sysfs-devices-node

        bool useFallback = false;
        auto fallback    = [&]() {
            useFallback = true;
            cfg         = empty();
        };

        // /sys/devices/system/node/online contains information about active NUMA nodes
        auto nodeIdsStr = read_file_to_string("/sys/devices/system/node/online");
        if (!nodeIdsStr.has_value() || nodeIdsStr->empty())
            fallback();
        else
        {
            remove_whitespace(*nodeIdsStr);
            for (usize n : indices_from_shortened_string(*nodeIdsStr))
            {
                // /sys/devices/system/node/node.../cpulist
                std::string path =
                  std::string("/sys/devices/system/node/node") + std::to_string(n) + "/cpulist";
                auto cpuIdsStr = read_file_to_string(path);
                // Now, we only bail if the file does not exist. Some nodes may be
                // empty, that's fine. An empty node still has a file that appears
                // to have some whitespace, so we need to handle that.
                if (!cpuIdsStr.has_value())
                {
                    fallback();
                    break;
                }
                else
                {
                    remove_whitespace(*cpuIdsStr);
                    for (usize c : indices_from_shortened_string(*cpuIdsStr))
                        if (is_cpu_allowed(c))
                            cfg.add_cpu_to_node(n, c);
                }
            }
        }

        if (useFallback)
        {
            for (CpuIndex c = 0; c < SYSTEM_THREADS_NB; ++c)
                if (is_cpu_allowed(c))
                    cfg.add_cpu_to_node(NumaIndex{0}, c);
        }

#elif defined(_WIN64)

        WORD numProcGroups = GetActiveProcessorGroupCount();
        for (WORD procGroup = 0; procGroup < numProcGroups; ++procGroup)
        {
            for (BYTE number = 0; number < WIN_PROCESSOR_GROUP_SIZE; ++number)
            {
                PROCESSOR_NUMBER procnum;
                procnum.Group    = procGroup;
                procnum.Number   = number;
                procnum.Reserved = 0;
                USHORT nodeNumber;

                const BOOL     status = GetNumaProcessorNodeEx(&procnum, &nodeNumber);
                const CpuIndex c      = static_cast<CpuIndex>(procGroup) * WIN_PROCESSOR_GROUP_SIZE
                                 + static_cast<CpuIndex>(number);
                if (status != 0 && nodeNumber != std::numeric_limits<USHORT>::max()
                    && is_cpu_allowed(c))
                    cfg.add_cpu_to_node(nodeNumber, c);
            }
        }

#else

        abort();  // should not reach here

#endif

        return cfg;
    }

    template<typename Pred>
    static std::optional<NumaConfig> try_get_l3_aware_config(
      bool respectProcessAffinity, usize bundleSize, [[maybe_unused]] Pred&& is_cpu_allowed) {
        // Get the normal system configuration so we know to which NUMA node
        // each L3 domain belongs.
        NumaConfig systemConfig =
          NumaConfig::from_system(SystemNumaPolicy{}, respectProcessAffinity);
        std::vector<L3Domain> l3Domains;

#if defined(__linux__) && !defined(__ANDROID__)

        std::set<CpuIndex> seenCpus;

        for (const auto& [next, _] : systemConfig.nodeByCpu)
        {
            if (seenCpus.count(next))
                continue;

            auto siblingsStr =
              read_file_to_string("/sys/devices/system/cpu/cpu" + std::to_string(next)
                                  + "/cache/index3/shared_cpu_list");

            if (!siblingsStr.has_value() || siblingsStr->empty())
                continue;

            L3Domain domain;
            for (usize c : indices_from_shortened_string(*siblingsStr))
            {
                if (is_cpu_allowed(c))
                {
                    domain.systemNumaIndex = systemConfig.nodeByCpu.at(c);
                    domain.cpus.insert(c);
                }
                seenCpus.insert(c);
            }
            if (!domain.cpus.empty())
                l3Domains.emplace_back(std::move(domain));
        }

#elif defined(_WIN64)

        DWORD bufSize = 0;
        GetLogicalProcessorInformationEx(RelationCache, nullptr, &bufSize);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return std::nullopt;

        std::vector<char> buffer(bufSize);
        auto info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
        if (!GetLogicalProcessorInformationEx(RelationCache, info, &bufSize))
            return std::nullopt;

        while (reinterpret_cast<char*>(info) < buffer.data() + bufSize)
        {
            info = std::launder(info);
            if (info->Relationship == RelationCache && info->Cache.Level == 3)
            {
                L3Domain domain{};
                domain.cpus = readCacheMembers(info, is_cpu_allowed);
                if (!domain.cpus.empty())
                {
                    domain.systemNumaIndex = systemConfig.nodeByCpu.at(*domain.cpus.begin());
                    l3Domains.push_back(std::move(domain));
                }
            }
            // Variable length data structure, advance to next
            info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
              reinterpret_cast<char*>(info) + info->Size);
        }
#endif

        if (!l3Domains.empty())
            return {NumaConfig::from_l3_info(std::move(l3Domains), bundleSize)};

        return std::nullopt;
    }


    static NumaConfig from_l3_info(std::vector<L3Domain>&& domains, usize bundleSize) {
        assert(!domains.empty());

        std::map<NumaIndex, std::vector<L3Domain>> list;
        for (auto& d : domains)
            list[d.systemNumaIndex].emplace_back(std::move(d));

        NumaConfig cfg = empty();
        NumaIndex  n   = 0;
        for (auto& [_, ds] : list)
        {
            bool changed;
            // Scan through pairs and merge them. With roughly equal L3 sizes, should give
            // a decent distribution.
            do
            {
                changed = false;
                for (usize j = 0; j + 1 < ds.size(); ++j)
                {
                    if (ds[j].cpus.size() + ds[j + 1].cpus.size() <= bundleSize)
                    {
                        changed = true;
                        ds[j].cpus.merge(ds[j + 1].cpus);
                        ds.erase(ds.begin() + j + 1);
                    }
                }
                // ds.size() has decreased if changed is true, so this loop will terminate
            } while (changed);
            for (const L3Domain& d : ds)
            {
                const NumaIndex dn = n++;
                for (CpuIndex cpu : d.cpus)
                    cfg.add_cpu_to_node(dn, cpu);
            }
        }
        return cfg;
    }
};

class NumaReplicationContext;

// Instances of this class are tracked by the NumaReplicationContext instance.
// NumaReplicationContext informs all tracked instances when NUMA configuration changes.
class NumaReplicatedBase {
   public:
    NumaReplicatedBase(NumaReplicationContext& ctx);

    NumaReplicatedBase(const NumaReplicatedBase&) = delete;
    NumaReplicatedBase(NumaReplicatedBase&& other) noexcept;

    NumaReplicatedBase& operator=(const NumaReplicatedBase&) = delete;
    NumaReplicatedBase& operator=(NumaReplicatedBase&& other) noexcept;

    virtual void on_numa_config_changed() = 0;
    virtual ~NumaReplicatedBase();

    const NumaConfig& get_numa_config() const;

   private:
    NumaReplicationContext* context;
};

// We force boxing with a unique_ptr. If this becomes an issue due to added
// indirection we may need to add an option for a custom boxing type. When the
// NUMA config changes the value stored at the index 0 is replicated to other nodes.
template<typename T>
class NumaReplicated: public NumaReplicatedBase {
   public:
    using ReplicatorFuncType = std::function<T(const T&)>;

    NumaReplicated(NumaReplicationContext& ctx) :
        NumaReplicatedBase(ctx) {
        replicate_from(T{});
    }

    NumaReplicated(NumaReplicationContext& ctx, T&& source) :
        NumaReplicatedBase(ctx) {
        replicate_from(std::move(source));
    }

    NumaReplicated(const NumaReplicated&) = delete;
    NumaReplicated(NumaReplicated&& other) noexcept :
        NumaReplicatedBase(std::move(other)),
        instances(std::exchange(other.instances, {})) {}

    NumaReplicated& operator=(const NumaReplicated&) = delete;
    NumaReplicated& operator=(NumaReplicated&& other) noexcept {
        NumaReplicatedBase::operator=(*this, std::move(other));
        instances = std::exchange(other.instances, {});

        return *this;
    }

    NumaReplicated& operator=(T&& source) {
        replicate_from(std::move(source));

        return *this;
    }

    ~NumaReplicated() override = default;

    const T& operator[](NumaReplicatedAccessToken token) const {
        assert(token.get_numa_index() < instances.size());
        return *(instances[token.get_numa_index()]);
    }

    const T& operator*() const { return *(instances[0]); }

    const T* operator->() const { return instances[0].get(); }

    template<typename FuncT>
    void modify_and_replicate(FuncT&& f) {
        auto source = std::move(instances[0]);
        std::forward<FuncT>(f)(*source);
        replicate_from(std::move(*source));
    }

    void on_numa_config_changed() override {
        // Use the first one as the source. It doesn't matter which one we use,
        // because they all must be identical, but the first one is guaranteed to exist.
        auto source = std::move(instances[0]);
        replicate_from(std::move(*source));
    }

   private:
    std::vector<std::unique_ptr<T>> instances;

    void replicate_from(T&& source) {
        instances.clear();

        const NumaConfig& cfg = get_numa_config();
        if (cfg.requires_memory_replication())
        {
            for (NumaIndex n = 0; n < cfg.num_numa_nodes(); ++n)
            {
                cfg.execute_on_numa_node(
                  n, [this, &source]() { instances.emplace_back(std::make_unique<T>(source)); });
            }
        }
        else
        {
            assert(cfg.num_numa_nodes() == 1);
            // We take advantage of the fact that replication is not required
            // and reuse the source value, avoiding one copy operation.
            instances.emplace_back(std::make_unique<T>(std::move(source)));
        }
    }
};

// We force boxing with a unique_ptr. If this becomes an issue due to added
// indirection we may need to add an option for a custom boxing type.
template<typename T>
class LazyNumaReplicated: public NumaReplicatedBase {
   public:
    using ReplicatorFuncType = std::function<T(const T&)>;

    LazyNumaReplicated(NumaReplicationContext& ctx) :
        NumaReplicatedBase(ctx) {
        prepare_replicate_from(T{});
    }

    LazyNumaReplicated(NumaReplicationContext& ctx, T&& source) :
        NumaReplicatedBase(ctx) {
        prepare_replicate_from(std::move(source));
    }

    LazyNumaReplicated(const LazyNumaReplicated&) = delete;
    LazyNumaReplicated(LazyNumaReplicated&& other) noexcept :
        NumaReplicatedBase(std::move(other)),
        instances(std::exchange(other.instances, {})) {}

    LazyNumaReplicated& operator=(const LazyNumaReplicated&) = delete;
    LazyNumaReplicated& operator=(LazyNumaReplicated&& other) noexcept {
        NumaReplicatedBase::operator=(*this, std::move(other));
        instances = std::exchange(other.instances, {});

        return *this;
    }

    LazyNumaReplicated& operator=(T&& source) {
        prepare_replicate_from(std::move(source));

        return *this;
    }

    ~LazyNumaReplicated() override = default;

    const T& operator[](NumaReplicatedAccessToken token) const {
        assert(token.get_numa_index() < instances.size());
        ensure_present(token.get_numa_index());
        return *(instances[token.get_numa_index()]);
    }

    const T& operator*() const { return *(instances[0]); }

    const T* operator->() const { return instances[0].get(); }

    template<typename FuncT>
    void modify_and_replicate(FuncT&& f) {
        auto source = std::move(instances[0]);
        std::forward<FuncT>(f)(*source);
        prepare_replicate_from(std::move(*source));
    }

    void on_numa_config_changed() override {
        // Use the first one as the source. It doesn't matter which one we use,
        // because they all must be identical, but the first one is guaranteed to exist.
        auto source = std::move(instances[0]);
        prepare_replicate_from(std::move(*source));
    }

   private:
    mutable std::vector<std::unique_ptr<T>> instances;
    mutable std::mutex                      mutex;

    void ensure_present(NumaIndex idx) const {
        assert(idx < instances.size());

        if (instances[idx] != nullptr)
            return;

        assert(idx != 0);

        std::unique_lock<std::mutex> lock(mutex);
        // Check again for races.
        if (instances[idx] != nullptr)
            return;

        const NumaConfig& cfg = get_numa_config();
        cfg.execute_on_numa_node(
          idx, [this, idx]() { instances[idx] = std::make_unique<T>(*instances[0]); });
    }

    void prepare_replicate_from(T&& source) {
        instances.clear();

        const NumaConfig& cfg = get_numa_config();
        if (cfg.requires_memory_replication())
        {
            assert(cfg.num_numa_nodes() > 0);

            // We just need to make sure the first instance is there.
            // Note that we cannot move here as we need to reallocate the data
            // on the correct NUMA node.
            cfg.execute_on_numa_node(
              0, [this, &source]() { instances.emplace_back(std::make_unique<T>(source)); });

            // Prepare others for lazy init.
            instances.resize(cfg.num_numa_nodes());
        }
        else
        {
            assert(cfg.num_numa_nodes() == 1);
            // We take advantage of the fact that replication is not required
            // and reuse the source value, avoiding one copy operation.
            instances.emplace_back(std::make_unique<T>(std::move(source)));
        }
    }
};

// Utilizes shared memory.
template<typename T>
class LazyNumaReplicatedSystemWide: public NumaReplicatedBase {
   public:
    using ReplicatorFuncType = std::function<T(const T&)>;

    LazyNumaReplicatedSystemWide(NumaReplicationContext& ctx, std::unique_ptr<T>&& source) :
        NumaReplicatedBase(ctx) {
        prepare_replicate_from(std::move(source));
    }

    LazyNumaReplicatedSystemWide(const LazyNumaReplicatedSystemWide&) = delete;
    LazyNumaReplicatedSystemWide(LazyNumaReplicatedSystemWide&& other) noexcept :
        NumaReplicatedBase(std::move(other)),
        instances(std::exchange(other.instances, {})) {}

    LazyNumaReplicatedSystemWide& operator=(const LazyNumaReplicatedSystemWide&) = delete;
    LazyNumaReplicatedSystemWide& operator=(LazyNumaReplicatedSystemWide&& other) noexcept {
        NumaReplicatedBase::operator=(*this, std::move(other));
        instances = std::exchange(other.instances, {});

        return *this;
    }

    LazyNumaReplicatedSystemWide& operator=(std::unique_ptr<T>&& source) {
        prepare_replicate_from(std::move(source));

        return *this;
    }

    ~LazyNumaReplicatedSystemWide() override = default;

    const T& operator[](NumaReplicatedAccessToken token) const {
        assert(token.get_numa_index() < instances.size());
        ensure_present(token.get_numa_index());
        return *(instances[token.get_numa_index()]);
    }

    const T& operator*() const { return *(instances[0]); }

    const T* operator->() const { return &*instances[0]; }

    std::vector<std::pair<SystemWideSharedConstantAllocationStatus, std::optional<std::string>>>
    get_status_and_errors() const {
        std::vector<std::pair<SystemWideSharedConstantAllocationStatus, std::optional<std::string>>>
          status;
        status.reserve(instances.size());

        for (const auto& instance : instances)
        {
            status.emplace_back(instance.get_status(), instance.get_error_message());
        }

        return status;
    }

    template<typename FuncT>
    void modify_and_replicate(FuncT&& f) {
        auto source = std::make_unique<T>(*instances[0]);
        std::forward<FuncT>(f)(*source);
        prepare_replicate_from(std::move(source));
    }

    void on_numa_config_changed() override {
        // Use the first one as the source. It doesn't matter which one we use,
        // because they all must be identical, but the first one is guaranteed to exist.
        auto source = std::make_unique<T>(*instances[0]);
        prepare_replicate_from(std::move(source));
    }

   private:
    mutable std::vector<SystemWideSharedConstant<T>> instances;
    mutable std::mutex                               mutex;

    usize get_discriminator(NumaIndex idx) const {
        const NumaConfig& cfg     = get_numa_config();
        const NumaConfig& cfg_sys = NumaConfig::from_system(SystemNumaPolicy{}, false);
        // as a discriminator, locate the hardware/system numadomain this cpuindex belongs to
        CpuIndex    cpu     = *cfg.nodes[idx].begin();  // get a CpuIndex from NumaIndex
        NumaIndex   sys_idx = cfg_sys.is_cpu_assigned(cpu) ? cfg_sys.nodeByCpu.at(cpu) : 0;
        std::string s       = cfg_sys.to_string() + "$" + std::to_string(sys_idx);
        return static_cast<usize>(hash_string(s));
    }

    void ensure_present(NumaIndex idx) const {
        assert(idx < instances.size());

        if (instances[idx] != nullptr)
            return;

        assert(idx != 0);

        std::unique_lock<std::mutex> lock(mutex);
        // Check again for races.
        if (instances[idx] != nullptr)
            return;

        const NumaConfig& cfg = get_numa_config();
        cfg.execute_on_numa_node(idx, [this, idx]() {
            instances[idx] = SystemWideSharedConstant<T>(*instances[0], get_discriminator(idx));
        });
    }

    void prepare_replicate_from(std::unique_ptr<T>&& source) {
        instances.clear();

        const NumaConfig& cfg = get_numa_config();
        // We just need to make sure the first instance is there.
        // Note that we cannot move here as we need to reallocate the data
        // on the correct NUMA node.
        // Even in the case of a single NUMA node we have to copy since it's shared memory.
        if (cfg.requires_memory_replication())
        {
            assert(cfg.num_numa_nodes() > 0);

            cfg.execute_on_numa_node(0, [this, &source]() {
                instances.emplace_back(SystemWideSharedConstant<T>(*source, get_discriminator(0)));
            });

            // Prepare others for lazy init.
            instances.resize(cfg.num_numa_nodes());
        }
        else
        {
            assert(cfg.num_numa_nodes() == 1);
            instances.emplace_back(SystemWideSharedConstant<T>(*source, get_discriminator(0)));
        }
    }
};

class NumaReplicationContext {
   public:
    NumaReplicationContext(NumaConfig&& cfg) :
        config(std::move(cfg)) {}

    NumaReplicationContext(const NumaReplicationContext&) = delete;
    NumaReplicationContext(NumaReplicationContext&&)      = delete;

    NumaReplicationContext& operator=(const NumaReplicationContext&) = delete;
    NumaReplicationContext& operator=(NumaReplicationContext&&)      = delete;

    ~NumaReplicationContext() {
        // The context must outlive replicated objects
        if (!trackedReplicatedObjects.empty())
            std::exit(EXIT_FAILURE);
    }

    void attach(NumaReplicatedBase* obj) {
        assert(trackedReplicatedObjects.count(obj) == 0);
        trackedReplicatedObjects.insert(obj);
    }

    void detach(NumaReplicatedBase* obj) {
        assert(trackedReplicatedObjects.count(obj) == 1);
        trackedReplicatedObjects.erase(obj);
    }

    // oldObj may be invalid at this point
    void move_attached([[maybe_unused]] NumaReplicatedBase* oldObj, NumaReplicatedBase* newObj) {
        assert(trackedReplicatedObjects.count(oldObj) == 1);
        assert(trackedReplicatedObjects.count(newObj) == 0);
        trackedReplicatedObjects.erase(oldObj);
        trackedReplicatedObjects.insert(newObj);
    }

    void set_numa_config(NumaConfig&& cfg) {
        config = std::move(cfg);
        for (auto&& obj : trackedReplicatedObjects)
            obj->on_numa_config_changed();
    }

    const NumaConfig& get_numa_config() const { return config; }

   private:
    NumaConfig config;

    // std::set uses std::less by default, which is required for pointer comparison
    std::set<NumaReplicatedBase*> trackedReplicatedObjects;
};

inline NumaReplicatedBase::NumaReplicatedBase(NumaReplicationContext& ctx) :
    context(&ctx) {
    context->attach(this);
}

inline NumaReplicatedBase::NumaReplicatedBase(NumaReplicatedBase&& other) noexcept :
    context(std::exchange(other.context, nullptr)) {
    context->move_attached(&other, this);
}

inline NumaReplicatedBase& NumaReplicatedBase::operator=(NumaReplicatedBase&& other) noexcept {
    context = std::exchange(other.context, nullptr);

    context->move_attached(&other, this);

    return *this;
}

inline NumaReplicatedBase::~NumaReplicatedBase() {
    if (context != nullptr)
        context->detach(this);
}

inline const NumaConfig& NumaReplicatedBase::get_numa_config() const {
    return context->get_numa_config();
}

}  // namespace Stockfish


#endif  // #ifndef NUMA_H_INCLUDED

// === sf_src/src/search.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <cstring>


// === sf_src/src/score.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SCORE_H_INCLUDED
#define SCORE_H_INCLUDED

#include <variant>
#include <utility>


namespace Stockfish {

class Position;

class Score {
   public:
    struct Mate {
        int plies;
    };

    struct Tablebase {
        int  plies;
        bool win;
    };

    struct InternalUnits {
        int value;
    };

    Score() = default;
    Score(Value v, const Position& pos);

    template<typename T>
    bool is() const {
        return std::holds_alternative<T>(score);
    }

    template<typename T>
    T get() const {
        return std::get<T>(score);
    }

    template<typename F>
    decltype(auto) visit(F&& f) const {
        return std::visit(std::forward<F>(f), score);
    }

   private:
    std::variant<Mate, Tablebase, InternalUnits> score;
};

}

#endif  // #ifndef SCORE_H_INCLUDED

// === sf_src/src/syzygy/tbprobe.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TBPROBE_H
#define TBPROBE_H

#include <functional>
#include <string>
#include <vector>


namespace Stockfish {
class Position;
class OptionsMap;

using Depth = int;

namespace Search {
struct RootMove;
using RootMoves = std::vector<RootMove>;
}
}

namespace Stockfish::Tablebases {

struct Config {
    int   cardinality = 0;
    bool  rootInTB    = false;
    bool  useRule50   = false;
    Depth probeDepth  = 0;
};

enum WDLScore {
    WDLLoss        = -2,  // Loss
    WDLBlessedLoss = -1,  // Loss, but draw under 50-move rule
    WDLDraw        = 0,   // Draw
    WDLCursedWin   = 1,   // Win, but draw under 50-move rule
    WDLWin         = 2,   // Win
};

// Possible states after a probing operation
enum ProbeState {
    FAIL              = 0,   // Probe failed (missing file table)
    OK                = 1,   // Probe successful
    CHANGE_STM        = -1,  // DTZ should check the other side
    ZEROING_BEST_MOVE = 2    // Best move zeroes DTZ (capture or pawn move)
};

extern int MaxCardinality;


void     init(const std::string& paths);
WDLScore probe_wdl(Position& pos, ProbeState* result);
int      probe_dtz(Position& pos, ProbeState* result);
bool     root_probe(Position&                    pos,
                    Search::RootMoves&           rootMoves,
                    bool                         rule50,
                    bool                         rankDTZ,
                    const std::function<bool()>& time_abort);
bool     root_probe_wdl(Position& pos, Search::RootMoves& rootMoves, bool rule50);
Config   rank_root_moves(
    const OptionsMap&            options,
    Position&                    pos,
    Search::RootMoves&           rootMoves,
    bool                         rankDTZ    = false,
    const std::function<bool()>& time_abort = []() { return false; });

}  // namespace Stockfish::Tablebases

#endif

// === sf_src/src/timeman.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TIMEMAN_H_INCLUDED
#define TIMEMAN_H_INCLUDED


#include <limits>


namespace Stockfish {

class OptionsMap;
enum Color : u8;

namespace Search {
struct LimitsType;
}

// The TimeManagement class computes the optimal time to think depending on
// the maximum available time, the game move number, and other parameters.
class TimeManagement {
   public:
    void init(Search::LimitsType& limits,
              Color               us,
              int                 ply,
              const OptionsMap&   options,
              double&             originalTimeAdjust);

    TimePoint optimum() const;
    TimePoint maximum() const;
    template<typename FUNC>
    TimePoint elapsed(FUNC nodes) const {
        return useNodesTime ? TimePoint(nodes()) : elapsed_time();
    }
    TimePoint elapsed_time() const { return now() - startTime; }

    void clear();
    void advance_nodes_time(i64 nodes);

   private:
    static constexpr TimePoint NoBound = std::numeric_limits<TimePoint>::max() / 2;

    TimePoint startTime;
    TimePoint optimumTime = NoBound;
    TimePoint maximumTime = NoBound;

    // Related to 'nodes as time' mode:
    bool useNodesTime      = false;
    i64  availableNodes    = -1;
    int  previousMovesToGo = 0;
    i64  cyclicBudget      = 0;
};

}  // namespace Stockfish

#endif  // #ifndef TIMEMAN_H_INCLUDED

namespace Stockfish {

// Different node types, used as a template parameter
enum NodeType {
    NonPV,
    PV,
    Root
};

class TranspositionTable;
class ThreadPool;
class OptionsMap;

namespace Eval::NNUE {
class Network;
}

namespace Search {

// syzygy_extend_pv() may lead to PVs longer than MAX_PLY
struct RootPVMoves: public std::vector<Move> {
    RootPVMoves() { reserve(MAX_PLY); }
};

struct PVMoves {
    Move  moves[MAX_PLY + 1];
    usize length = 0;

    Move*       begin() { return moves; }
    const Move* begin() const { return moves; }
    Move*       end() { return moves + length; }
    const Move* end() const { return moves + length; }

    Move&       operator[](usize index) { return moves[index]; }
    const Move& operator[](usize index) const { return moves[index]; }

    bool  empty() const { return length == 0; }
    usize size() const { return length; }

    void clear() { length = 0; }

    void push_back(Move move) {
        assert(length < MAX_PLY + 1);
        moves[length++] = move;
    }

    void resize(usize newSize) {
        assert(newSize <= length);
        length = newSize;
    }

    void update(Move move, const PVMoves* childPv) {
        assert(childPv == nullptr || childPv->size() <= MAX_PLY);
        length = childPv ? childPv->length : 0;

        if (childPv)
            std::memcpy(moves + 1, childPv->moves, length * sizeof(Move));

        moves[0] = move;
        ++length;
    }

    PVMoves& operator=(const RootPVMoves& rhs) {
        length = std::min(rhs.size(), usize(MAX_PLY));
        std::memcpy(moves, rhs.data(), length * sizeof(Move));
        return *this;
    }
};

// Stack struct keeps track of the information we need to remember from nodes
// shallower and deeper in the tree during the search. Each search thread has
// its own array of Stack objects, indexed by the current ply.
struct Stack {
    PVMoves*                    pv;
    PieceToHistory*             continuationHistory;
    CorrectionHistory<PieceTo>* continuationCorrectionHistory;
    int                         ply;
    Move                        currentMove;
    Move                        excludedMove;
    Value                       staticEval;
    int                         statScore;
    int                         moveCount;
    bool                        inCheck;
    bool                        ttPv;
    bool                        ttHit;
    bool                        followPV;
    int                         cutoffCnt;
    int                         reduction;
    int                         priorNMPFailHigh;
};


// RootMove struct is used for moves at the root of the tree. For each root move
// we store a score and a PV (really a refutation in the case of moves which
// fail low). Score is normally set at -VALUE_INFINITE for all non-pv moves.
struct RootMove {

    explicit RootMove(Move m) { pv.push_back(m); }
    bool extract_ponder_from_tt(const TranspositionTable& tt, Position& pos);
    bool is_inexact() const { return inexactLower || inexactUpper; }
    bool is_exact_loss() const {
        return score != -VALUE_INFINITE && is_loss(score) && !is_inexact();
    }
    void unset_inexact() { inexactLower = inexactUpper = false; }
    bool operator==(const Move& m) const { return pv[0] == m; }
    // Sort in descending order
    bool operator<(const RootMove& m) const {
        return m.score != score ? m.score < score : m.previousScore < previousScore;
    }

    u64         effort           = 0;
    Value       score            = -VALUE_INFINITE;
    Value       previousScore    = -VALUE_INFINITE;
    Value       averageScore     = -VALUE_INFINITE;
    Value       meanSquaredScore = -VALUE_INFINITE * VALUE_INFINITE;
    Value       uciScore         = -VALUE_INFINITE;
    bool        inexactLower     = false;  // By default root scores are exact, unless flagged as a
    bool        inexactUpper     = false;  // one-sided bound here. See also `enum Bound` in types.h
    bool        previousScoreExact = false;
    int         selDepth           = 0;
    int         tbRank             = 0;
    Value       tbScore;
    RootPVMoves pv, previousPV;
};

using RootMoves = std::vector<RootMove>;


// LimitsType struct stores information sent by the caller about the analysis required.
struct LimitsType {

    // Init explicitly due to broken value-initialization of non POD in MSVC
    LimitsType() {
        time[WHITE] = time[BLACK] = inc[WHITE] = inc[BLACK] = npmsec = movetime = TimePoint(0);
        movestogo = depth = mate = perft = infinite = 0;
        nodes                                       = 0;
        ponderMode                                  = false;
    }

    bool use_time_management() const { return time[WHITE] || time[BLACK]; }

    std::vector<std::string> searchmoves;
    TimePoint                time[COLOR_NB], inc[COLOR_NB], npmsec, movetime, startTime;
    int                      movestogo, depth, mate, perft, infinite;
    u64                      nodes;
    bool                     ponderMode;
};


// The UCI stores the uci options, thread pool, and transposition table.
// This struct is used to easily forward data to the Search::Worker class.
struct SharedState {
    SharedState(const OptionsMap&                                        optionsMap,
                ThreadPool&                                              threadPool,
                TranspositionTable&                                      transpositionTable,
                std::map<NumaIndex, SharedHistories>&                    sharedHists,
                const LazyNumaReplicatedSystemWide<Eval::NNUE::Network>& net) :
        options(optionsMap),
        threads(threadPool),
        tt(transpositionTable),
        sharedHistories(sharedHists),
        network(net) {}

    const OptionsMap&                                        options;
    ThreadPool&                                              threads;
    TranspositionTable&                                      tt;
    std::map<NumaIndex, SharedHistories>&                    sharedHistories;
    const LazyNumaReplicatedSystemWide<Eval::NNUE::Network>& network;
};

class Worker;

struct InfoShort {
    int   depth;
    Score score;
};

struct InfoFull: InfoShort {
    int              selDepth;
    usize            multiPV;
    std::string_view wdl;
    std::string_view bound;
    usize            timeMs;
    usize            nodes;
    usize            nps;
    usize            tbHits;
    std::string_view pv;
    int              hashfull;
};

struct InfoIteration {
    int              depth;
    std::string_view currmove;
    usize            currmovenumber;
};

// Skill structure is used to implement strength limit. If we have a UCI_Elo,
// we convert it to an appropriate skill level, anchored to the Stash engine.
// This method is based on a fit of the Elo results for games played between
// Stockfish at various skill levels and various versions of the Stash engine.
// Skill 0 .. 19 now covers CCRL Blitz Elo from 1320 to 3190, approximately
// Reference: https://github.com/vondele/Stockfish/commit/a08b8d4e9711c2
struct Skill {
    // Lowest and highest Elo ratings used in the skill level calculation
    constexpr static int LowestElo  = 1320;
    constexpr static int HighestElo = 3190;

    Skill(int skill_level, int uci_elo) {
        if (uci_elo)
        {
            double e = double(uci_elo - LowestElo) / (HighestElo - LowestElo);
            level = std::clamp((((37.2473 * e - 40.8525) * e + 22.2943) * e - 0.311438), 0.0, 19.0);
        }
        else
            level = double(skill_level);
    }
    bool enabled() const { return level < 20.0; }
    bool time_to_pick(Depth depth) const { return depth == 1 + int(level); }
    Move pick_best(const RootMoves&, usize multiPV);

    double level;
    Move   best = Move::none();
};

// SearchManager manages the search from the main thread. It is responsible for
// keeping track of the time, and storing data strictly related to the main thread.
class SearchManager {
   public:
    using UpdateShort    = std::function<void(const InfoShort&)>;
    using UpdateFull     = std::function<void(const InfoFull&)>;
    using UpdateIter     = std::function<void(const InfoIteration&)>;
    using UpdateBestmove = std::function<void(std::string_view, std::string_view)>;
    using UpdateStart    = std::function<void()>;

    struct UpdateContext {
        UpdateShort    onUpdateNoMoves;
        UpdateFull     onUpdateFull;
        UpdateIter     onIter;
        UpdateBestmove onBestmove;
        UpdateStart    onStart;
    };


    SearchManager(const UpdateContext& updateContext) :
        updates(updateContext) {}

    void check_time(Search::Worker& worker);

    void output_pv(Search::Worker&           worker,
                   const ThreadPool&         threads,
                   const TranspositionTable& tt,
                   Depth                     depth);

    Stockfish::TimeManagement tm;
    double                    originalTimeAdjust;
    int                       callsCnt;
    std::atomic_bool          ponder;

    std::array<Value, 4> iterValue;
    double               previousTimeReduction;
    Value                bestPreviousScore;
    Value                bestPreviousAverageScore;
    bool                 stopOnPonderhit;

    const UpdateContext& updates;
};

// Search::Worker is the class that does the actual search.
// It is instantiated once per thread, and it is responsible for keeping track
// of the search history, and storing data required for the search.
class Worker {
   public:
    Worker(
      SharedState&, std::unique_ptr<SearchManager>, usize, usize, usize, NumaReplicatedAccessToken);

    // Reset histories, usually before a new game.
    void clear();

    // Called when the program receives the UCI 'go' command.
    // It searches from the root position and outputs the "bestmove".
    void start_searching();

    bool is_mainthread() const { return threadIdx == 0; }

    void ensure_network_replicated();

    // Public because they need to be updatable by the stats
    ButterflyHistory mainHistory;
    LowPlyHistory    lowPlyHistory;

    CapturePieceToHistory           captureHistory;
    CorrectionHistory<Continuation> continuationCorrectionHistory;

    TTMoveHistory    ttMoveHistory;
    SharedHistories& sharedHistory;
    ContinuationHistory (&continuationHistory)[2][2];

   private:
    bool iterative_deepening();

    void do_move(Position&    pos,
                 const Move   move,
                 StateInfo&   st,
                 const bool   givesCheck,
                 const bool   capture,
                 Stack* const ss);
    void do_null_move(Position& pos, StateInfo& st, Stack* const ss);
    void undo_move(Position& pos, const Move move);
    void undo_null_move(Position& pos);

    // This is the main search function, for both PV and non-PV nodes
    template<NodeType nodeType>
    Value
    search(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, const bool cutNode);

    // Quiescence search function, which is called by the main search
    template<NodeType nodeType>
    Value qsearch(Position& pos, Stack* ss, Value alpha, Value beta);

    int reduction(bool i, Depth d, int mn) const;

    // Pointer to the search manager, only allowed to be called by the main thread
    SearchManager* main_manager() const {
        assert(threadIdx == 0);
        assert(manager.get() != nullptr);
        return manager.get();
    }

    TimePoint elapsed() const;

    Value evaluate(const Position&);

    LimitsType limits;

    usize              pvIdx, pvLast;
    RelaxedAtomic<u64> nodes, tbHits, bestMoveChanges;
    int                selDepth, nmpMinPly;

    Value optimism[COLOR_NB];

    Position  rootPos;
    StateInfo rootState;
    RootMoves rootMoves;
    Depth     rootDepth;
    bool      seekMate = false;

    PVMoves lastIterationIdxPV;

    usize                     threadIdx, numaThreadIdx, numaTotal;
    NumaReplicatedAccessToken numaAccessToken;

    // The main thread has a SearchManager, the others have a nullptr
    std::unique_ptr<SearchManager> manager;

    Tablebases::Config tbConfig;

    const OptionsMap&                                        options;
    ThreadPool&                                              threads;
    TranspositionTable&                                      tt;
    const LazyNumaReplicatedSystemWide<Eval::NNUE::Network>& network;

    // Used by NNUE
    Eval::NNUE::AccumulatorStack  accumulatorStack;
    Eval::NNUE::AccumulatorCaches refreshTable;

    friend class Stockfish::ThreadPool;
    friend class SearchManager;
};

struct ConthistBonus {
    int index;
    int weight;
};


}  // namespace Search

}  // namespace Stockfish

#endif  // #ifndef SEARCH_H_INCLUDED

// === sf_src/src/thread.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>


namespace Stockfish {


class OptionsMap;
using Value = int;

// Sometimes we don't want to actually bind the threads, but the recipient still
// needs to think it runs on *some* NUMA node, such that it can access structures
// that rely on NUMA node knowledge. This class encapsulates this optional process
// such that the recipient does not need to know whether the binding happened or not.
class OptionalThreadToNumaNodeBinder {
   public:
    OptionalThreadToNumaNodeBinder(NumaIndex n) :
        numaConfig(nullptr),
        numaId(n) {}

    OptionalThreadToNumaNodeBinder(const NumaConfig& cfg, NumaIndex n) :
        numaConfig(&cfg),
        numaId(n) {}

    NumaReplicatedAccessToken operator()() const {
        if (numaConfig != nullptr)
            return numaConfig->bind_current_thread_to_numa_node(numaId);
        else
            return NumaReplicatedAccessToken(numaId);
    }

   private:
    const NumaConfig* numaConfig;
    NumaIndex         numaId;
};

// Abstraction of a thread. It contains a pointer to the worker and a native thread.
// After construction, the native thread is started with idle_loop()
// waiting for a signal to start searching.
// When the signal is received, the thread starts searching and when
// the search is finished, it goes back to idle_loop() waiting for a new signal.
class Thread {
   public:
    Thread(Search::SharedState&,
           std::unique_ptr<Search::SearchManager>,
           usize,
           usize,
           usize,
           OptionalThreadToNumaNodeBinder);
    virtual ~Thread();

    void idle_loop();
    void start_searching();
    void clear_worker();
    void run_custom_job(std::function<void()> f);

    void ensure_network_replicated();

    // Thread has been slightly altered to allow running custom jobs, so
    // this name is no longer correct. However, this class (and ThreadPool)
    // require further work to make them properly generic while maintaining
    // appropriate specificity regarding search, from the point of view of an
    // outside user, so renaming of this function is left for whenever that happens.
    void  wait_for_search_finished();
    usize id() const { return idx; }

    LargePagePtr<Search::Worker> worker;
    std::function<void()>        jobFunc;

   private:
    std::mutex                mutex;
    std::condition_variable   cv;
    usize                     idx, idxInNuma, totalNuma;
    bool                      exit = false, searching = true;  // Set before starting std::thread
    NativeThread              stdThread;
    NumaReplicatedAccessToken numaAccessToken;
};


// ThreadPool struct handles all the threads-related stuff like init, starting,
// parking and, most importantly, launching a thread. All the access to threads
// is done through this class.
class ThreadPool {
   public:
    ThreadPool() {}

    ~ThreadPool() {
        // destroy any existing thread(s)
        if (threads.size() > 0)
        {
            main_thread()->wait_for_search_finished();

            threads.clear();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)      = delete;

    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

    void  start_thinking(const OptionsMap&, Position&, StateListPtr&, Search::LimitsType);
    void  run_on_thread(usize threadId, std::function<void()> f);
    void  wait_on_thread(usize threadId);
    usize num_threads() const;
    void  clear();
    void  set(const NumaConfig& numaConfig,
              Search::SharedState,
              const Search::SearchManager::UpdateContext&);

    Search::SearchManager* main_manager();
    Thread*                main_thread() const { return threads.front().get(); }
    u64                    nodes_searched() const;
    u64                    tb_hits() const;
    Thread*                get_best_thread() const;
    void                   start_searching();
    void                   wait_for_search_finished() const;

    std::vector<usize> get_bound_thread_to_numa_node() const;
    std::vector<usize> get_bound_thread_count_by_numa_node() const;
    usize              numa_nodes() const;

    void ensure_network_replicated();

    std::atomic_bool stop, increaseDepth;

    auto cbegin() const noexcept { return threads.cbegin(); }
    auto begin() noexcept { return threads.begin(); }
    auto end() noexcept { return threads.end(); }
    auto cend() const noexcept { return threads.cend(); }
    auto size() const noexcept { return threads.size(); }
    auto empty() const noexcept { return threads.empty(); }

   private:
    StateListPtr                         setupStates;
    std::vector<std::unique_ptr<Thread>> threads;
    std::vector<NumaIndex>               boundThreadToNumaNode;

    u64 accumulate(RelaxedAtomic<u64> Search::Worker::* member) const {

        u64 sum = 0;
        for (auto&& th : threads)
            sum += (th->worker.get()->*member).load(std::memory_order_relaxed);
        return sum;
    }
};

}  // namespace Stockfish

#endif  // #ifndef THREAD_H_INCLUDED

// === sf_src/src/tt.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TT_H_INCLUDED
#define TT_H_INCLUDED

#include <tuple>


namespace Stockfish {

class ThreadPool;
struct Cluster;

// There is only one global hash table for the engine and all its threads.
// For chess in particular, we even allow racy updates between threads to and
// from the TT, as taking the time to synchronize access would cost thinking
// time and thus Elo. As a hash table, collisions are possible and may cause
// chess playing issues (bizarre blunders, faulty mate reports, etc). Fixing
// these also loses Elo; however such risk decreases with larger TT size.
//
// We clearly separate TTData, a local copy of an entry, from TTWriter, which
// writes to the global table.


// A copy of the data already in an entry (possibly collided). Probes and reads
// are racy and non-atomic, possibly resulting in inconsistent data.
struct TTData {
    Move  move;
    Value value, eval;
    Depth depth;
    Bound bound;
    bool  is_pv;

    TTData() = delete;

    // clang-format off
    TTData(Move m, Value v, Value ev, Depth d, Bound b, bool pv) :
        move(m),
        value(v),
        eval(ev),
        depth(d),
        bound(b),
        is_pv(pv) {}
    // clang-format on
};


// This is used to make racy, non-atomic writes to the global TT. Writes are
// not "guaranteed": for chess reasons, we may decide the new data is less
// important than the old.
struct TTWriter {
   public:
    void write(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, u8 generation8);
    void penalize(int penalty);  // decrement stored depth by the penalty

   private:
    friend class TranspositionTable;
    Cluster* cluster;
    int      index;
    TTWriter(Cluster* cl, int idx);
};


class TranspositionTable {

   public:
    ~TranspositionTable() { aligned_large_pages_free(table); }

    // Set TT size in MiB
    void resize(usize mbSize, ThreadPool& threads);

    // Re-initialize memory, multithreaded
    void clear(ThreadPool& threads);

    // Must be called at the beginning of each root search to track entry aging
    void new_search();

    // The current age, used when writing new data to the TT
    u8 generation() const;

    // Approximate what fraction of entries (permille) have been written to
    // during this root search.
    int hashfull(int maxAge = 0) const;

    // `probe(key)` is the primary method: given a board position, we lookup
    //  its entry in the table, and return a tuple of:
    //   1) whether the entry already had data on this position
    //   2) a copy of the prior data, if any (may be self-inconsistent due to races)
    //   3) a writer object to the entry
    std::tuple<bool, TTData, TTWriter> probe(const Key key) const;

    // The hash function; its only external use is memory prefetching
    Cluster* get_cluster(const Key key) const;

   private:
    friend struct TTEntry;

    usize    clusterCount;
    Cluster* table = nullptr;

    u8 generation8 = 0;
};

}  // namespace Stockfish

#endif  // #ifndef TT_H_INCLUDED

// === sf_src/src/ucioption.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UCIOPTION_H_INCLUDED
#define UCIOPTION_H_INCLUDED

#include <functional>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>


namespace Stockfish {
// Define a custom comparator, because the UCI options should be case-insensitive
struct CaseInsensitiveLess {
    bool operator()(const std::string&, const std::string&) const;
};

class OptionsMap;

// The Option class implements each option as specified by the UCI protocol
class Option {
   public:
    using OnChange = std::function<std::optional<std::string>(const Option&)>;

    Option(OnChange = nullptr);
    Option(bool v, OnChange = nullptr);
    Option(const char* v, OnChange = nullptr);
    Option(int v, int minv, int maxv, OnChange = nullptr);
    Option(const char* v, const char* cur, OnChange = nullptr);

    Option& operator=(const std::string&);
    operator int() const;
    operator std::string() const;
    bool operator==(const char*) const;
    bool operator!=(const char*) const;

    friend std::ostream& operator<<(std::ostream&, const OptionsMap&);

    int operator<<(const Option&) = delete;

   private:
    friend class OptionsMap;
    friend class Engine;
    friend class Tune;


    std::string       defaultValue, currentValue, type;
    int               min = 0, max = 0;
    usize             idx = 0;
    OnChange          on_change;
    const OptionsMap* parent = nullptr;
};

class OptionsMap {
   public:
    using InfoListener = std::function<void(std::optional<std::string>)>;

    OptionsMap()                             = default;
    OptionsMap(const OptionsMap&)            = delete;
    OptionsMap(OptionsMap&&)                 = delete;
    OptionsMap& operator=(const OptionsMap&) = delete;
    OptionsMap& operator=(OptionsMap&&)      = delete;

    void add_info_listener(InfoListener&&);

    void setoption(std::istringstream&);

    const Option& operator[](const std::string&) const;

    void add(const std::string&, const Option& option);

    usize count(const std::string&) const;

   private:
    friend class Engine;
    friend class Option;

    friend std::ostream& operator<<(std::ostream&, const OptionsMap&);

    // The options container is defined as a std::map
    using OptionsStore = std::map<std::string, Option, CaseInsensitiveLess>;

    OptionsStore options_map;
    InfoListener info;
};

}
#endif  // #ifndef UCIOPTION_H_INCLUDED

namespace Stockfish {

constexpr int MaxHashMB = Is64Bit ? 33554432 : 2048;
extern int    MaxThreads;

class Engine {
   public:
    using InfoShort = Search::InfoShort;
    using InfoFull  = Search::InfoFull;
    using InfoIter  = Search::InfoIteration;

    Engine(std::optional<std::filesystem::path> path = std::nullopt);

    // Cannot be movable due to components holding backreferences to fields
    Engine(const Engine&)            = delete;
    Engine(Engine&&)                 = delete;
    Engine& operator=(const Engine&) = delete;
    Engine& operator=(Engine&&)      = delete;

    ~Engine() { wait_for_search_finished(); }

    std::variant<u64, PositionSetError> perft(const std::string& fen, Depth depth, bool isChess960);

    // non blocking call to start searching
    void go(Search::LimitsType&);
    // non blocking call to stop searching
    void stop();

    // blocking call to wait for search to finish
    void wait_for_search_finished();
    // set a new position, moves are in UCI format
    std::optional<PositionSetError> set_position(const std::string&              fen,
                                                 const std::vector<std::string>& moves);

    // modifiers

    bool set_numa_config_from_option(const std::string& o);
    void resize_threads();
    void set_tt_size(usize mb);
    void set_ponderhit(bool);
    void search_clear();

    void set_on_update_no_moves(std::function<void(const InfoShort&)>&&);
    void set_on_update_full(std::function<void(const InfoFull&)>&&);
    void set_on_iter(std::function<void(const InfoIter&)>&&);
    void set_on_bestmove(std::function<void(std::string_view, std::string_view)>&&);
    void set_on_start(std::function<void()>&&);
    void set_on_verify_network(std::function<void(std::string_view)>&&);

    // network related

    void                                 verify_network() const;
    std::unique_ptr<Eval::NNUE::Network> get_default_network();
    void                                 load_network(const std::filesystem::path& file);
    void save_network(const std::optional<std::filesystem::path>& file);

    // utility functions

    void trace_eval() const;

    const OptionsMap& get_options() const;
    OptionsMap&       get_options();

    int get_hashfull(int maxAge = 0) const;

    std::string                          fen() const;
    std::optional<PositionSetError>      flip();
    std::string                          visualize() const;
    std::vector<std::pair<usize, usize>> get_bound_thread_count_by_numa_node() const;
    std::string                          get_numa_config_as_string() const;
    std::string                          numa_config_information_as_string() const;
    std::string                          thread_allocation_information_as_string() const;
    std::string                          thread_binding_information_as_string() const;

   private:
    const std::filesystem::path binaryDirectory;

    NumaReplicationContext numaContext;

    Position     pos;
    StateListPtr states;

    OptionsMap                                        options;
    ThreadPool                                        threads;
    TranspositionTable                                tt;
    Eval::NNUE::EvalFile                              networkFile;
    LazyNumaReplicatedSystemWide<Eval::NNUE::Network> network;

    Search::SearchManager::UpdateContext  updateContext;
    std::function<void(std::string_view)> onVerifyNetwork;
    std::map<NumaIndex, SharedHistories>  sharedHists;
};

}  // namespace Stockfish


#endif  // #ifndef ENGINE_H_INCLUDED

namespace Stockfish {

class Position;
class Move;
class Score;
enum Square : u8;
using Value = int;

constexpr auto StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

class UCIEngine {
   public:
    UCIEngine(CommandLine cli);

    void loop();

    static int         to_cp(Value v, const Position& pos);
    static std::string format_score(const Score& s);
    static std::string square(Square s);
    static std::string move(Move m, bool chess960 = false);
    static std::string wdl(Value v, const Position& pos);
    static std::string to_lower(std::string str);
    static Move        to_move(const Position& pos, std::string str);

    Search::LimitsType parse_limits(std::istream& is);

    auto& engine_options() { return engine.get_options(); }

   private:
    Engine      engine;
    CommandLine cli;
    std::string currentCmd;

    static void print_info_string(std::string_view str);

    void go(std::istringstream& is);
    void bench(std::istream& args);
    void benchmark(std::istream& args);
    void position(std::istringstream& is);
    void setoption(std::istringstream& is);
    u64  perft(const Search::LimitsType&);

    static void on_update_no_moves(const Engine::InfoShort& info);
    static void on_update_full(const Engine::InfoFull& info, bool showWDL);
    static void on_iter(const Engine::InfoIter& info);
    static void on_bestmove(std::string_view bestmove, std::string_view ponder);

    void init_search_update_listeners();

    [[noreturn]] void terminate_on_critical_error(const std::string& message);
};

}  // namespace Stockfish

#endif  // #ifndef UCI_H_INCLUDED

namespace Stockfish {

static int simple_eval(const Position& pos) {
    const Color c = pos.side_to_move();
    return PawnValue * (pos.count<PAWN>(c) - pos.count<PAWN>(~c)) + pos.non_pawn_material(c)
         - pos.non_pawn_material(~c);
}

Value scale_evaluation(Value nnue, int optimism, const Position& pos);

Value Eval::evaluate(const Eval::NNUE::Network&     network,
                     const Position&                pos,
                     Eval::NNUE::AccumulatorStack&  accumulators,
                     Eval::NNUE::AccumulatorCaches& caches,
                     int                            optimism) {

    assert(!pos.checkers());
    Value nnue = network.evaluate(pos, accumulators, caches);
    return scale_evaluation(nnue, optimism, pos);
}

// Applies search-dependent scaling (optimism and rule50) to the raw NNUE eval
Value scale_evaluation(Value nnue, int optimism, const Position& pos) {
    Value se = simple_eval(pos);

    // Normalize the raw evaluations to [-1024, 1024] to measure their correlation.
    int se_norm   = (se * 1024) / (std::abs(se) + 1024);
    int nnue_norm = (nnue * 1024) / (std::abs(nnue) + 1024);
    // When NNUE and material agree (positive correlation), the position is straightforward;
    // otherwise (negative correlation) it involves complex compensation. In a representative
    // sample, correl averages -1 or so, i.e. it is well-centered in [-2048, 2048].
    int correl = (se_norm * nnue_norm) / 512;

    // When winning, we boost easy positions, and vice versa
    int v = nnue + (nnue + optimism) * correl / 65536;

    // Scale the combined evaluation by total material
    int material = 521 * pos.count<PAWN>() + pos.non_pawn_material();
    v += i64(material) * v / 90649;

    // Damp down the evaluation linearly when shuffling
    v -= v * pos.rule50_count() / 189;

    // Guarantee that the evaluation does not hit the tablebase range
    v = std::clamp(v, VALUE_TB_LOSS_IN_MAX_PLY + 1, VALUE_TB_WIN_IN_MAX_PLY - 1);

    return v;
}

// Like evaluate(), but instead of returning a value, it returns
// a string (suitable for outputting to stdout) that contains the detailed
// descriptions and values of each evaluation term. Useful for debugging.
// Trace scores are from white's point of view
std::string Eval::trace(Position& pos, const Eval::NNUE::Network& network) {

    if (pos.checkers())
        return "Final evaluation: none (in check)";

    auto accumulators = std::make_unique<Eval::NNUE::AccumulatorStack>();
    auto caches       = std::make_unique<Eval::NNUE::AccumulatorCaches>(network);

    std::stringstream ss;
    ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);
    ss << '\n' << NNUE::trace(pos, network, *caches) << '\n';

    ss << std::showpoint << std::showpos << std::fixed << std::setprecision(2) << std::setw(15);

    Value nnue = network.evaluate(pos, *accumulators, *caches);
    Value s_v  = scale_evaluation(nnue, VALUE_ZERO, pos);  // requires stm perspective

    ss << "NNUE evaluation          " << nnue << " (side to move, internal units)\n";

    nnue = pos.side_to_move() == WHITE ? nnue : -nnue;
    s_v  = pos.side_to_move() == WHITE ? s_v : -s_v;

    ss << "NNUE evaluation        " << 0.01 * UCIEngine::to_cp(nnue, pos) << " (white side)\n";
    ss << "Final evaluation      ";
    ss << 0.01 * UCIEngine::to_cp(s_v, pos) << " (white side)";
    ss << " [with scaled NNUE, ...]\n";

    return ss.str();
}

}  // namespace Stockfish

// === sf_src/src/misc.cpp ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <mutex>
#include <sstream>
#include <string_view>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <direct.h>
    #include <windows.h>
    #include <shellapi.h>
#endif


namespace Stockfish {

namespace fs = std::filesystem;

namespace {

// Version number or dev.
constexpr std::string_view version = "dev";

// Our fancy logging facility. The trick here is to replace cin.rdbuf() and
// cout.rdbuf() with two Tie objects that tie cin and cout to a file stream. We
// can toggle the logging of std::cout and std::cin at runtime whilst preserving
// usual I/O functionality, all without changing a single line of code!
// Idea from http://groups.google.com/group/comp.lang.c++/msg/1d941c0f26ea0d81

struct Tie: public std::streambuf {  // MSVC requires split streambuf for cin and cout

    Tie(std::streambuf* b, std::streambuf* l) :
        buf(b),
        logBuf(l) {}

    int sync() override { return logBuf->pubsync(), buf->pubsync(); }
    int overflow(int c) override { return log(buf->sputc(char(c)), "<< "); }
    int underflow() override { return buf->sgetc(); }
    int uflow() override { return log(buf->sbumpc(), ">> "); }

    std::streambuf *buf, *logBuf;

    int log(int c, const char* prefix) {

        static int last = '\n';  // Single log file

        if (last == '\n')
            logBuf->sputn(prefix, 3);

        return last = logBuf->sputc(char(c));
    }
};

class Logger {

    Logger() :
        in(std::cin.rdbuf(), file.rdbuf()),
        out(std::cout.rdbuf(), file.rdbuf()) {}
    ~Logger() { start(""); }

    std::ofstream file;
    Tie           in, out;

   public:
    static void start(const fs::path& fname) {

        static Logger l;

        if (l.file.is_open())
        {
            std::cout.rdbuf(l.out.buf);
            std::cin.rdbuf(l.in.buf);
            l.file.close();
        }

        if (!fname.empty())
        {
            l.file.open(fname, std::ifstream::out);

            if (!l.file.is_open())
            {
                std::cerr << "Unable to open debug log file " << fname << std::endl;
                exit(EXIT_FAILURE);
            }

            std::cin.rdbuf(&l.in);
            std::cout.rdbuf(&l.out);
        }
    }
};

}  // namespace


// Returns the full name of the current Stockfish version.
//
// For local dev compiles we try to append the commit SHA and
// commit date from git. If that fails only the local compilation
// date is set and "nogit" is specified:
//      Stockfish dev-YYYYMMDD-SHA
//      or
//      Stockfish dev-YYYYMMDD-nogit
//
// For releases (non-dev builds) we only include the version number:
//      Stockfish version
std::string engine_version_info() {
    std::stringstream ss;
    ss << "Stockfish " << version << std::setfill('0');

    if constexpr (version == "dev")
    {
        ss << "-";
#ifdef GIT_DATE
        ss << stringify(GIT_DATE);
#else
        constexpr std::string_view months("Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec");

        std::string       month, day, year;
        std::stringstream date(__DATE__);  // From compiler, format is "Sep 21 2008"

        date >> month >> day >> year;
        ss << year << std::setw(2) << std::setfill('0') << (1 + months.find(month) / 4)
           << std::setw(2) << std::setfill('0') << day;
#endif

#ifdef GIT_DIFFINDEX
        ss << "-m";
#endif

        ss << "-";

#ifdef GIT_SHA
        ss << stringify(GIT_SHA);
#else
        ss << "nogit";
#endif
    }

    return ss.str();
}

std::string engine_info(bool to_uci) {
    return engine_version_info() + (to_uci ? "\nid author " : " by ")
         + "the Stockfish developers (see AUTHORS file)";
}


// Returns a string trying to describe the compiler we use
std::string compiler_info() {

#define make_version_string(major, minor, patch) \
    stringify(major) "." stringify(minor) "." stringify(patch)

    // Predefined macros hell:
    //
    // __GNUC__                Compiler is GCC, Clang or ICX
    // __clang__               Compiler is Clang or ICX
    // __INTEL_LLVM_COMPILER   Compiler is ICX
    // _MSC_VER                Compiler is MSVC
    // _WIN32                  Building on Windows (any)
    // _WIN64                  Building on Windows 64 bit

    std::string compiler = "\nCompiled by                : ";

#if defined(__INTEL_LLVM_COMPILER)
    compiler += "ICX ";
    compiler += stringify(__INTEL_LLVM_COMPILER);
#elif defined(__clang__)
    compiler += "clang++ ";
    compiler += make_version_string(__clang_major__, __clang_minor__, __clang_patchlevel__);
#elif _MSC_VER
    compiler += "MSVC ";
    compiler += "(version ";
    compiler += stringify(_MSC_FULL_VER) "." stringify(_MSC_BUILD);
    compiler += ")";
#elif defined(__e2k__) && defined(__LCC__)
    #define dot_ver2(n) \
        compiler += char('.'); \
        compiler += char('0' + (n) / 10); \
        compiler += char('0' + (n) % 10);

    compiler += "MCST LCC ";
    compiler += "(version ";
    compiler += std::to_string(__LCC__ / 100);
    dot_ver2(__LCC__ % 100) dot_ver2(__LCC_MINOR__) compiler += ")";
#elif __GNUC__
    compiler += "g++ (GNUC) ";
    compiler += make_version_string(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
    compiler += "Unknown compiler ";
    compiler += "(unknown version)";
#endif

#if defined(__APPLE__)
    compiler += " on Apple";
#elif defined(__CYGWIN__)
    compiler += " on Cygwin";
#elif defined(__MINGW64__)
    compiler += " on MinGW64";
#elif defined(__MINGW32__)
    compiler += " on MinGW32";
#elif defined(__ANDROID__)
    compiler += " on Android";
#elif defined(__linux__)
    compiler += " on Linux";
#elif defined(_WIN64)
    compiler += " on Microsoft Windows 64-bit";
#elif defined(_WIN32)
    compiler += " on Microsoft Windows 32-bit";
#else
    compiler += " on unknown system";
#endif

    compiler += "\nCompilation architecture   : ";
#if defined(ARCH)
    compiler += stringify(ARCH);
#else
    compiler += "(undefined architecture)";
#endif

    compiler += "\nCompilation settings       : ";
    compiler += (Is64Bit ? "64bit" : "32bit");
#if defined(USE_AVX512ICL)
    compiler += " AVX512ICL";
#endif
#if defined(USE_VNNI)
    compiler += " VNNI";
#endif
#if defined(USE_AVX512)
    compiler += " AVX512";
#endif
    compiler += (HasPext ? " BMI2" : "");
#if defined(USE_AVX2)
    compiler += " AVX2";
#endif
#if defined(USE_SSE41)
    compiler += " SSE41";
#endif
#if defined(USE_SSSE3)
    compiler += " SSSE3";
#endif
#if defined(USE_SSE2)
    compiler += " SSE2";
#endif
#if defined(USE_NEON_DOTPROD)
    compiler += " NEON_DOTPROD";
#elif defined(USE_NEON)
    compiler += " NEON";
#endif
#if defined(USE_LASX)
    compiler += " LASX";
#endif
#if defined(USE_LSX)
    compiler += " LSX";
#endif
    compiler += (HasPopCnt ? " POPCNT" : "");

#if !defined(NDEBUG)
    compiler += " DEBUG";
#endif

    compiler += "\nCompiler __VERSION__ macro : ";
#ifdef __VERSION__
    compiler += __VERSION__;
#else
    compiler += "(undefined macro)";
#endif

    compiler += "\n";

    return compiler;
}


// Debug functions used mainly to collect run-time statistics
constexpr int MaxDebugSlots = 32;

namespace {

template<usize N>
struct DebugInfo {
    std::array<std::atomic<i64>, N> data = {0};

    [[nodiscard]] constexpr std::atomic<i64>& operator[](usize index) {
        assert(index < N);
        return data[index];
    }

    constexpr DebugInfo& operator=(const DebugInfo& other) {
        for (usize i = 0; i < N; i++)
            data[i].store(other.data[i].load());
        return *this;
    }
};

struct DebugExtremes: public DebugInfo<3> {
    DebugExtremes() {
        data[1] = std::numeric_limits<i64>::min();
        data[2] = std::numeric_limits<i64>::max();
    }
};

std::array<DebugInfo<2>, MaxDebugSlots>  hit;
std::array<DebugInfo<2>, MaxDebugSlots>  mean;
std::array<DebugInfo<3>, MaxDebugSlots>  stdev;
std::array<DebugInfo<6>, MaxDebugSlots>  correl;
std::array<DebugExtremes, MaxDebugSlots> extremes;

}  // namespace

bool dbg_hit_on(bool cond, int slot) {

    ++hit.at(slot)[0];
    if (cond)
        ++hit.at(slot)[1];

    return cond;
}

i64 dbg_mean_of(i64 value, int slot) {

    ++mean.at(slot)[0];
    mean.at(slot)[1] += value;

    return value;
}

i64 dbg_stdev_of(i64 value, int slot) {

    ++stdev.at(slot)[0];
    stdev.at(slot)[1] += value;
    stdev.at(slot)[2] += value * value;

    return value;
}

i64 dbg_extremes_of(i64 value, int slot) {
    ++extremes.at(slot)[0];

    i64 current_max = extremes.at(slot)[1].load();
    while (current_max < value && !extremes.at(slot)[1].compare_exchange_weak(current_max, value))
    {}

    i64 current_min = extremes.at(slot)[2].load();
    while (current_min > value && !extremes.at(slot)[2].compare_exchange_weak(current_min, value))
    {}

    return value;
}

void dbg_correl_of(i64 value1, i64 value2, int slot) {

    ++correl.at(slot)[0];
    correl.at(slot)[1] += value1;
    correl.at(slot)[2] += value1 * value1;
    correl.at(slot)[3] += value2;
    correl.at(slot)[4] += value2 * value2;
    correl.at(slot)[5] += value1 * value2;
}

void dbg_print() {

    i64  n;
    auto E   = [&n](i64 x) { return double(x) / n; };
    auto sqr = [](double x) { return x * x; };

    for (int i = 0; i < MaxDebugSlots; ++i)
        if ((n = hit[i][0]))
            std::cerr << "Hit #" << i << ": Total " << n << " Hits " << hit[i][1]
                      << " Hit Rate (%) " << 100.0 * E(hit[i][1]) << std::endl;

    for (int i = 0; i < MaxDebugSlots; ++i)
        if ((n = mean[i][0]))
            std::cerr << "Mean #" << i << ": Total " << n << " Mean " << E(mean[i][1]) << std::endl;

    for (int i = 0; i < MaxDebugSlots; ++i)
        if ((n = stdev[i][0]))
        {
            double r = sqrt(E(stdev[i][2]) - sqr(E(stdev[i][1])));
            std::cerr << "Stdev #" << i << ": Total " << n << " Stdev " << r << std::endl;
        }

    for (int i = 0; i < MaxDebugSlots; ++i)
        if ((n = extremes[i][0]))
            std::cerr << "Extremity #" << i << ": Total " << n << " Min " << extremes[i][2]
                      << " Max " << extremes[i][1] << std::endl;

    for (int i = 0; i < MaxDebugSlots; ++i)
        if ((n = correl[i][0]))
        {
            double r = (E(correl[i][5]) - E(correl[i][1]) * E(correl[i][3]))
                     / (sqrt(E(correl[i][2]) - sqr(E(correl[i][1])))
                        * sqrt(E(correl[i][4]) - sqr(E(correl[i][3]))));
            std::cerr << "Correl. #" << i << ": Total " << n << " Coefficient " << r << std::endl;
        }
}

void dbg_clear() {
    hit.fill({});
    mean.fill({});
    stdev.fill({});
    correl.fill({});
    extremes.fill({});
}

// Used to serialize access to std::cout
// to avoid multiple threads writing at the same time.
std::ostream& operator<<(std::ostream& os, SyncCout sc) {

    static std::mutex m;

    if (sc == IO_LOCK)
        m.lock();

    if (sc == IO_UNLOCK)
        m.unlock();

    return os;
}

void sync_cout_start() { std::cout << IO_LOCK; }
void sync_cout_end() { std::cout << IO_UNLOCK; }

// Hash function based on public domain MurmurHash64A, by Austin Appleby.
u64 hash_bytes(const char* data, usize size) {
    const u64 m = 0xc6a4a7935bd1e995ull;
    const int r = 47;

    u64 h = size * m;

    const char* end = data + (size & ~(usize) 7);

    for (const char* p = data; p != end; p += 8)
    {
        u64 k;
        std::memcpy(&k, p, sizeof(k));

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    if (size & 7)
    {
        u64 k = 0;
        for (int i = (size & 7) - 1; i >= 0; i--)
            k = (k << 8) | u8(end[i]);

        h ^= k;
        h *= m;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

// Trampoline helper to avoid moving Logger to misc.h
void start_logger(const fs::path& fname) { Logger::start(fname); }


// Convert a wide string to an UTF8 string
std::string utf8_from_wstring(std::wstring_view s) {
#ifdef _WIN32
    if (s.empty())
        return {};

    int size =
      WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return {};

    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), out.data(), size, nullptr, nullptr);
    return out;
#else
    return std::string(s.begin(), s.end());
#endif
}


// This utility allows us to handle filenames and paths with UTF8 encoding
fs::path path_from_utf8(const std::string& path) {
#ifdef _WIN32
    int u8len = static_cast<int>(path.size());

    // First attempt UTF-8, then fall back to ANSI for old GUIs like Arena
    constexpr int CodePages[2] = {CP_UTF8, CP_ACP};
    for (int cp : CodePages)
    {
        int flags = cp == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
        int wlen  = MultiByteToWideChar(cp, flags, path.c_str(), u8len, NULL, 0);
        if (wlen > 0)
        {
            std::wstring wstr(static_cast<usize>(wlen), L'\0');
            MultiByteToWideChar(cp, 0, path.c_str(), u8len, wstr.data(), wlen);
            return {wstr};
        }
    }

    return {path};
#else
    return {path};
#endif
}


// Constructor for the CommandLine class
CommandLine::CommandLine(int _argc, char** _argv) :
    argc(_argc),
    argv(_argv) {
#ifdef _WIN32
    // Convert any non-ANSI characters passed on the command line to UTF-8
    int wargc = 0;
    if (LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc))
    {
        for (int i = 0; i < wargc; ++i)
            argv_storage.push_back(utf8_from_wstring(wargv[i]));
        LocalFree(wargv);

        for (std::string& s : argv_storage)
            argv_utf8.push_back(s.data());
        argv_utf8.push_back(nullptr);

        argc = wargc;
        argv = argv_utf8.data();
    }
#endif
}


// Read a numerical value from the command line
std::optional<usize> str_to_size_t(const std::string& s) {
    if (s.empty() || s[0] == '-')
        return std::nullopt;
    errno                           = 0;
    char*                    endptr = nullptr;
    const unsigned long long value  = std::strtoull(s.c_str(), &endptr, 10);
    if (errno == ERANGE || (*endptr != '\0' && !std::isspace(static_cast<unsigned char>(*endptr)))
        || value > std::numeric_limits<usize>::max())
        return std::nullopt;
    return static_cast<usize>(value);
}

// Read the given file as bytes (returns std::nullopt if the file does not exist)
std::optional<std::string> read_file_to_string(const std::string& path) {
    std::ifstream f(path, std::ios_base::binary);
    if (!f)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

// Remove all spaces from a string
void remove_whitespace(std::string& s) {
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); }),
            s.end());
}

// Test if a string has only spaces
bool is_whitespace(std::string_view s) {
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
}


// Return the directory where our Stockfish binary sits. This is useful,
// because when the NNUE network is not embedded in the binary, this directory
// is one of the locations where we look for the NNUE file.
fs::path CommandLine::get_binary_directory(fs::path argv0) {

#ifdef _WIN32
    // Prefer the executable path reported by Windows. Unlike _get_wpgmptr,
    // this does not depend on whether the CRT used a narrow or wide entry
    // point. Windows paths cannot exceed 32767 characters, so a fixed
    // buffer is always sufficient. Falls back to argv0 if the API fails.
    constexpr DWORD MaxPath = 32768;
    wchar_t         path[MaxPath];

    if (const DWORD length = GetModuleFileNameW(nullptr, path, MaxPath))
        argv0 = fs::path(path, path + length);
#endif

    auto binaryDirectory = argv0.parent_path();
    if (binaryDirectory.empty())
        binaryDirectory = fs::path(".");
    return binaryDirectory;
}


// On Windows, tell the console to use UTF8 encoding
void set_console_utf8() {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
}

}  // namespace Stockfish

// === sf_src/src/movegen.cpp ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


// === sf_src/src/movegen.h ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MOVEGEN_H_INCLUDED
#define MOVEGEN_H_INCLUDED

#include <algorithm>  // IWYU pragma: keep


namespace Stockfish {

class Position;

enum GenType {
    CAPTURES,
    QUIETS,
    EVASIONS,
    ALL
};

struct ExtMove: public Move {
    int value;

    void operator=(Move m) { data = m.raw(); }

    // Inhibit unwanted implicit conversions to Move
    // with an ambiguity that yields to a compile error.
    operator float() const = delete;
};

inline bool operator<(const ExtMove& f, const ExtMove& s) { return f.value < s.value; }

template<GenType>
Move* generate(const Position& pos, Move* moveList);

// The MoveList struct wraps the generate() function and returns a convenient
// list of moves. Using MoveList is sometimes preferable to directly calling
// the lower level generate() function.
template<GenType T>
struct MoveList {

    explicit MoveList(const Position& pos) :
        last(generate<T>(pos, moveList)) {}
    const Move* begin() const { return moveList; }
    const Move* end() const { return last; }
    usize       size() const { return last - moveList; }
    bool        contains(Move move) const { return std::find(begin(), end(), move) != end(); }

   private:
    Move moveList[MAX_MOVES], *last;
};

}  // namespace Stockfish

#endif  // #ifndef MOVEGEN_H_INCLUDED

#include <cassert>
#include <initializer_list>


#if defined(USE_AVX512ICL)
    #include <immintrin.h>
#endif

namespace Stockfish {

namespace {

#if defined(USE_AVX512ICL)

template<Direction offset>
inline Move* splat_pawn_moves(Move* moveList, Bitboard to_bb) {
    assert(popcount(to_bb) <= 8);  // <= 8 pawns per side

    const __m128i toSquares =
      _mm_cvtepi8_epi16(_mm512_castsi512_si128(_mm512_maskz_compress_epi8(to_bb, AllSquares)));
    const __m128i fromSquares = _mm_subs_epi16(toSquares, _mm_set1_epi16(offset));
    const __m128i moves       = _mm_or_si128(_mm_slli_epi16(fromSquares, Move::FromSqShift),
                                             _mm_slli_epi16(toSquares, Move::ToSqShift));

    _mm_storeu_si128(reinterpret_cast<__m128i*>(moveList), moves);
    return moveList + popcount(to_bb);
}

inline Move* splat_moves(Move* moveList, Square from, Bitboard to_bb) {
    assert(popcount(to_bb) <= 32);  // Q can attack up to 27 squares

    const __m512i fromVec = _mm512_set1_epi16(Move(from, SQUARE_ZERO).raw());
    const __m512i toSquares =
      _mm512_cvtepi8_epi16(_mm512_castsi512_si256(_mm512_maskz_compress_epi8(to_bb, AllSquares)));
    const __m512i moves = _mm512_or_si512(fromVec, _mm512_slli_epi16(toSquares, Move::ToSqShift));

    _mm512_storeu_si512(moveList, moves);
    return moveList + popcount(to_bb);
}

#else

template<Direction offset>
inline Move* splat_pawn_moves(Move* moveList, Bitboard to_bb) {
    while (to_bb)
    {
        Square to   = pop_lsb(to_bb);
        *moveList++ = Move(to - offset, to);
    }
    return moveList;
}

inline Move* splat_moves(Move* moveList, Square from, Bitboard to_bb) {
    while (to_bb)
        *moveList++ = Move(from, pop_lsb(to_bb));
    return moveList;
}

#endif

template<GenType Type, Direction D, bool Enemy>
Move* make_promotions(Move* moveList, [[maybe_unused]] Square to) {

    constexpr bool          all  = Type == EVASIONS || Type == ALL;
    [[maybe_unused]] Square from = to - D;

    if constexpr (Type == CAPTURES || all)
        *moveList++ = Move::make<PROMOTION>(from, to, QUEEN);

    if constexpr ((Type == CAPTURES && Enemy) || (Type == QUIETS && !Enemy) || all)
    {
        *moveList++ = Move::make<PROMOTION>(from, to, ROOK);
        *moveList++ = Move::make<PROMOTION>(from, to, BISHOP);
        *moveList++ = Move::make<PROMOTION>(from, to, KNIGHT);
    }

    return moveList;
}


template<Color Us, GenType Type>
Move* generate_pawn_moves(const Position& pos, Move* moveList, Bitboard target) {

    static_assert(Type != ALL, "Unsupported type in generate_pawn_moves()");

    constexpr Color     Them     = ~Us;
    constexpr Bitboard  TRank7BB = (Us == WHITE ? Rank7BB : Rank2BB);
    constexpr Bitboard  TRank3BB = (Us == WHITE ? Rank3BB : Rank6BB);
    constexpr Direction Up       = pawn_push(Us);
    constexpr Direction UpRight  = (Us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction UpLeft   = (Us == WHITE ? NORTH_WEST : SOUTH_EAST);

    const Bitboard emptySquares = ~pos.pieces();
    const Bitboard enemies      = Type == EVASIONS ? pos.checkers() : pos.pieces(Them);
    const Bitboard pinned       = pos.blockers_for_king(Us);
    const Square   ksq          = pos.square<KING>(Us);

    const Bitboard pawnsOn7        = pos.pieces(Us, PAWN) & TRank7BB;
    const Bitboard pawnsNotOn7     = pos.pieces(Us, PAWN) & ~TRank7BB;
    const Bitboard pushable        = ~pinned | file_bb(ksq);
    const Bitboard canCaptureLeft  = ~pinned | Attacks::antidiag_bb(ksq);
    const Bitboard canCaptureRight = ~pinned | Attacks::diag_bb(ksq);

    // Single and double pawn pushes, no promotions
    if constexpr (Type != CAPTURES)
    {
        Bitboard b1 = shift(pawnsNotOn7 & pushable, Up) & emptySquares;
        Bitboard b2 = shift(b1 & TRank3BB, Up) & emptySquares;

        if constexpr (Type == EVASIONS)  // Consider only blocking squares
        {
            b1 &= target;
            b2 &= target;
        }

        moveList = splat_pawn_moves<Up>(moveList, b1);
        moveList = splat_pawn_moves<Up + Up>(moveList, b2);
    }

    // Promotions and underpromotions
    if (pawnsOn7)
    {
        Bitboard b1 = shift(pawnsOn7 & canCaptureRight, UpRight) & enemies;
        Bitboard b2 = shift(pawnsOn7 & canCaptureLeft, UpLeft) & enemies;
        Bitboard b3 = shift(pawnsOn7 & pushable, Up) & emptySquares;

        if constexpr (Type == EVASIONS)
            b3 &= target;

        while (b1)
            moveList = make_promotions<Type, UpRight, true>(moveList, pop_lsb(b1));

        while (b2)
            moveList = make_promotions<Type, UpLeft, true>(moveList, pop_lsb(b2));

        while (b3)
            moveList = make_promotions<Type, Up, false>(moveList, pop_lsb(b3));
    }

    // Standard and en passant captures
    if constexpr (Type == CAPTURES || Type == EVASIONS)
    {
        Bitboard b1 = shift(pawnsNotOn7 & canCaptureRight, UpRight) & enemies;
        Bitboard b2 = shift(pawnsNotOn7 & canCaptureLeft, UpLeft) & enemies;

        moveList = splat_pawn_moves<UpRight>(moveList, b1);
        moveList = splat_pawn_moves<UpLeft>(moveList, b2);

        const Square epSq = pos.ep_square();

        if (epSq != SQ_NONE
            // An en passant capture cannot resolve a discovered check
            && !(Type == EVASIONS && (target & (epSq + Up))))
        {
            assert(rank_of(epSq) == relative_rank(Us, RANK_6));

            b1 = pawnsNotOn7
               & ((canCaptureRight & shift(square_bb(epSq), -UpRight))
                  | (canCaptureLeft & shift(square_bb(epSq), -UpLeft)));
            assert(b1);

            while (b1)
                *moveList++ = Move::make<EN_PASSANT>(pop_lsb(b1), epSq);
        }
    }

    return moveList;
}


template<PieceType Pt>
Move* generate_moves(const Position& pos, Move* moveList, Bitboard target, Color us) {

    static_assert(Pt != KING && Pt != PAWN, "Unsupported piece type in generate_moves()");

    const Square   ksq    = pos.square<KING>(us);
    const Bitboard pinned = pos.blockers_for_king(us);

    Bitboard bb = pos.pieces(us, Pt) & ~pinned;
    while (bb)
    {
        Square   from = pop_lsb(bb);
        Bitboard b    = Attacks::attacks_bb(Pt, from, pos.pieces()) & target;

        moveList = splat_moves(moveList, from, b);
    }

    if constexpr (Pt != KNIGHT)  // pinned knights cannot move
    {
        bb = pos.pieces(us, Pt) & pinned;
        while (bb)
        {
            Square   from = pop_lsb(bb);
            Bitboard b    = Attacks::attacks_bb(Pt, from, pos.pieces()) & target;
            b &= Attacks::line_bb(ksq, from);

            moveList = splat_moves(moveList, from, b);
        }
    }

    return moveList;
}

}  // namespace


// <CAPTURES>     Generates all legal captures plus queen promotions
// <QUIETS>       Generates all legal non-captures and underpromotions
// <EVASIONS>     Generates all legal check evasions
//
// Returns a pointer to the end of the move list.
template<GenType Type>
Move* generate(const Position& pos, Move* moveList) {

    static_assert(Type != ALL, "Unsupported type in generate()");
    assert((Type == EVASIONS) == bool(pos.checkers()));

    const Color  us  = pos.side_to_move();
    const Square ksq = pos.square<KING>(us);
    Bitboard     target;

    // Skip generating non-king moves when in double check
    if (Type != EVASIONS || !more_than_one(pos.checkers()))
    {
        target = Type == EVASIONS ? Attacks::between_bb(ksq, lsb(pos.checkers()))
               : Type == CAPTURES ? pos.pieces(~us)
                                  : ~pos.pieces();  // QUIETS

        moveList = us == WHITE ? generate_pawn_moves<WHITE, Type>(pos, moveList, target)
                               : generate_pawn_moves<BLACK, Type>(pos, moveList, target);
        moveList = generate_moves<KNIGHT>(pos, moveList, target, us);
        moveList = generate_moves<BISHOP>(pos, moveList, target, us);
        moveList = generate_moves<ROOK>(pos, moveList, target, us);
        moveList = generate_moves<QUEEN>(pos, moveList, target, us);
    }

    if constexpr (Type == EVASIONS)
        target = ~pos.pieces(us);

    Bitboard b = Attacks::attacks_bb(KING, ksq) & target;
    if (b)
        moveList = splat_moves(moveList, ksq, b & ~pos.threats_by(ALL_PIECES));

    if (Type == QUIETS && pos.can_castle(us & ANY_CASTLING))
    {
        for (CastlingRights cr : {us & KING_SIDE, us & QUEEN_SIDE})
        {
            if (!pos.can_castle(cr) || pos.castling_impeded(cr))
                continue;

            Square rookSquare = pos.castling_rook_square(cr);
            Square to         = relative_square(us, rookSquare > ksq ? SQ_G1 : SQ_C1);

            if (!(Attacks::between_bb(ksq, to) & pos.threats_by(ALL_PIECES))
                && !(pos.blockers_for_king(us) & rookSquare))
                *moveList++ = Move::make<CASTLING>(ksq, rookSquare);
        }
    }

    return moveList;
}

// Explicit template instantiations
template Move* generate<CAPTURES>(const Position&, Move*);
template Move* generate<QUIETS>(const Position&, Move*);
template Move* generate<EVASIONS>(const Position&, Move*);

// generate<ALL> generates all the legal moves in the given position

template<>
Move* generate<ALL>(const Position& pos, Move* moveList) {
    return pos.checkers() ? generate<EVASIONS>(pos, moveList)
                          : generate<QUIETS>(pos, generate<CAPTURES>(pos, moveList));
}

}  // namespace Stockfish

// === sf_src/src/position.cpp ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>


using std::string;

namespace Stockfish {

using namespace Attacks;

namespace Zobrist {

// constexpr-generated Zobrist constants
struct Keys {
    Key psq[PIECE_NB][SQUARE_NB];
    Key enpassant[FILE_NB];
    Key castling[CASTLING_RIGHT_NB];
    Key side, noPawns;
};

constexpr Keys generate_keys() {
    Keys k{};
    u64  s    = 1070372;
    auto next = [&s] {
        s ^= s >> 12;
        s ^= (s << 25) & 0xFFFFFFFFFFFFFFFFULL;
        s ^= s >> 27;
        return Key((s * 2685821657736338717ULL) & 0xFFFFFFFFFFFFFFFFULL);
    };
    for (Piece pc : {W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING, B_PAWN, B_KNIGHT,
                     B_BISHOP, B_ROOK, B_QUEEN, B_KING})
        for (Square sq = SQ_A1; sq <= SQ_H8; ++sq)
            k.psq[pc][sq] = next();
    // Pawns on these squares will promote: keep the impossible placements zero
    for (Square sq = SQ_A8; sq <= SQ_H8; ++sq)
        k.psq[W_PAWN][sq] = Key(0);
    for (Square sq = SQ_A1; sq <= SQ_H1; ++sq)
        k.psq[B_PAWN][sq] = Key(0);
    for (auto& e : k.enpassant)
        e = next();
    for (auto& c : k.castling)
        c = next();
    k.side    = next();
    k.noPawns = next();
    return k;
}

constexpr Keys keys = generate_keys();

constexpr auto& psq       = keys.psq;
constexpr auto& enpassant = keys.enpassant;
constexpr auto& castling  = keys.castling;
constexpr auto& side      = keys.side;
constexpr auto& noPawns   = keys.noPawns;

// No combination of ep, castling and stm key components may cancel, so that a
// matching piecePlacement and Zobrist key guarantee a unique position.
static_assert(
  [] {
      for (int a = 0; a < CASTLING_RIGHT_NB; ++a)
          for (int b = 0; b < CASTLING_RIGHT_NB; ++b)
              for (int f1 = 0; f1 <= FILE_NB; ++f1)      // 0 means no ep component
                  for (int f2 = 0; f2 <= FILE_NB; ++f2)  // 0 means no ep component
                      for (int stm = 0; stm < 2; ++stm)
                      {
                          Key cr   = castling[a] ^ castling[b];
                          Key ep   = (f1 ? enpassant[f1 - 1] : 0) ^ (f2 ? enpassant[f2 - 1] : 0);
                          Key stmd = stm ? side : 0;
                          if (cr == 0 && ep == 0 && stmd == 0)
                              continue;  // identical components
                          if ((cr ^ ep ^ stmd) == 0)
                              return false;  // a cancellation exists
                      }
      return true;
  }(),
  "Zobrist ep/castling/stm keys must not cancel: matching piecePlacement "
  "and key guarantee a unique position");

}

namespace {

constexpr std::string_view PieceToChar(" PNBRQK  pnbrqk");

static constexpr Piece Pieces[] = {W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
                                   B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING};

// Pack pieces into nibbles: lower half of the board to even nibbles, upper half to odd nibbles
inline std::array<u8, 32> pack_pieces(const std::array<Piece, SQUARE_NB>& arr) {
    std::array<u8, 32> p;
    for (int k = 0; k < 32; k += 8)
    {
        u64 a, b;
        memcpy(&a, &arr[k], 8);
        memcpy(&b, &arr[k + 32], 8);
        a |= b << 4;
        memcpy(&p[k], &a, 8);
    }
    return p;
}

inline void swap_nibble(std::array<u8, 32>& p, Square s1, Square s2) {
    int i1 = int(s1) & 31, sh1 = (int(s1) & 0x20) >> 3;
    int i2 = int(s2) & 31, sh2 = (int(s2) & 0x20) >> 3;
    int swap = ((p[i1] >> sh1) ^ (p[i2] >> sh2)) & 0xF;
    p[i1] ^= swap << sh1;
    p[i2] ^= swap << sh2;
}

}  // namespace


// Returns an ASCII representation of the position
std::ostream& operator<<(std::ostream& os, const Position& pos) {

    os << "\n +---+---+---+---+---+---+---+---+\n";

    for (Rank r = RANK_8;; --r)
    {
        for (File f = FILE_A; f <= FILE_H; ++f)
            os << " | " << PieceToChar[pos.piece_on(make_square(f, r))];

        os << " | " << (1 + r) << "\n +---+---+---+---+---+---+---+---+\n";

        if (r == RANK_1)
            break;
    }

    os << "   a   b   c   d   e   f   g   h\n"
       << "\nFen: " << pos.fen() << "\nKey: " << std::hex << std::uppercase << std::setfill('0')
       << std::setw(16) << pos.key() << std::setfill(' ') << std::dec << "\nCheckers: ";

    for (Bitboard b = pos.checkers(); b;)
        os << UCIEngine::square(pop_lsb(b)) << " ";

    if (Tablebases::MaxCardinality >= popcount(pos.pieces()) && !pos.can_castle(ANY_CASTLING))
    {
        StateInfo st;

        Position p;
        p.set(pos.fen(), pos.is_chess960(), &st);
        Tablebases::ProbeState s1, s2;
        Tablebases::WDLScore   wdl = Tablebases::probe_wdl(p, &s1);
        int                    dtz = Tablebases::probe_dtz(p, &s2);
        os << "\nTablebases WDL: " << std::setw(4) << wdl << " (" << s1 << ")"
           << "\nTablebases DTZ: " << std::setw(4) << dtz << " (" << s2 << ")";
    }

    return os;
}


// Implements Marcel van Kervinck's cuckoo algorithm to detect repetition of positions
// for 3-fold repetition draws. The algorithm uses two hash tables with Zobrist hashes
// to allow fast detection of recurring positions. For details see:
// http://web.archive.org/web/20201107002606/https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf

// First and second hash functions for indexing the cuckoo tables
constexpr int H1(Key h) { return h & 0x1fff; }
constexpr int H2(Key h) { return (h >> 16) & 0x1fff; }

// constexpr-generated cuckoo tables with Zobrist hashes of valid reversible
// moves, and the moves themselves.
struct CuckooTables {
    std::array<Key, 8192>  keys;
    std::array<Move, 8192> moves;
    int                    count;
};

constexpr CuckooTables generate_cuckoo() {
    std::array<Key, 8192>  c{};
    std::array<Move, 8192> m{};
    int                    count = 0;
    for (Piece pc : Pieces)
        for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
            for (Square s2 = Square(s1 + 1); s2 <= SQ_H8; ++s2)
                // only non-pawn moves are reversible relocations; note that
                // PseudoAttacks puns slots 0-1 for the pawn-by-color attacks
                if (type_of(pc) != PAWN && (PseudoAttacks[type_of(pc)][s1] & s2))
                {
                    Move move = Move(s1, s2);
                    Key  key  = Zobrist::psq[pc][s1] ^ Zobrist::psq[pc][s2] ^ Zobrist::side;
                    int  i    = H1(key);
                    while (true)
                    {
                        swap_constexpr(c[i], key);
                        swap_constexpr(m[i], move);
                        if (move == Move::none())  // Arrived at empty slot?
                            break;
                        i = (i == H1(key)) ? H2(key) : H1(key);  // Push victim to alternative slot
                    }
                    count++;
                }
    return {c, m, count};
}

constexpr CuckooTables cuckooTables = generate_cuckoo();

static_assert(cuckooTables.count == 3668, "cuckoo table insertion count must be stable");

constexpr auto& cuckoo     = cuckooTables.keys;
constexpr auto& cuckooMove = cuckooTables.moves;

// Initializes the position object with the given FEN string.
// The FEN string is strictly validated; if it is invalid or inconsistent,
// a PositionSetError describing the problem is returned, otherwise std::nullopt.
std::optional<PositionSetError>
Position::set(const string& fenStr, bool isChess960, StateInfo* si) {
    /*
   A FEN string defines a particular position using only the ASCII character set.

   A FEN string contains six fields separated by a space. The fields are:

   1) Piece placement (from white's perspective). Each rank is described, starting
      with rank 8 and ending with rank 1. Within each rank, the contents of each
      square are described from file A through file H. Following the Standard
      Algebraic Notation (SAN), each piece is identified by a single letter taken
      from the standard English names. White pieces are designated using upper-case
      letters ("PNBRQK") whilst Black uses lowercase ("pnbrqk"). Blank squares are
      noted using digits 1 through 8 (the number of blank squares), and "/"
      separates ranks.

   2) Active color. "w" means white moves next, "b" means black.

   3) Castling availability. If neither side can castle, this is "-". Otherwise,
      this has one or more letters: "K" (White can castle kingside), "Q" (White
      can castle queenside), "k" (Black can castle kingside), and/or "q" (Black
      can castle queenside).

   4) En passant target square (in algebraic notation). If there's no en passant
      target square, this is "-". If a pawn has just made a 2-square move, this
      is the position "behind" the pawn. Following X-FEN standard, this is recorded
      only if there is a pawn in position to make an en passant capture, and if
      there really is a pawn that might have advanced two squares.

   5) Halfmove clock. This is the number of halfmoves since the last pawn advance
      or capture. This is used to determine if a draw can be claimed under the
      fifty-move rule.

   6) Fullmove number. The number of the full move. It starts at 1, and is
      incremented after Black's move.
*/

    unsigned char      token;
    std::istringstream ss(fenStr);

    std::memset(reinterpret_cast<char*>(this), 0, sizeof(Position));
    std::memset(si, 0, sizeof(StateInfo));
    st = si;

    ss >> std::noskipws;

    int numPieces = 0;
    int file      = FILE_A;
    int rank      = RANK_8;

    // 1. Piece placement
    for (;;)
    {
        if (!(ss >> token))
            return PositionSetError("Invalid FEN. Unexpected end of stream.");

        if (isspace(token))
            break;

        if (isdigit(token))
        {
            const int diff = (token - '0');
            if (diff < 1 || diff > 8)
                return PositionSetError("Invalid FEN. Invalid number of squares to skip.");

            file += diff;
            if (file > FILE_NB)
                return PositionSetError("Invalid FEN. Invalid file reached.");
        }
        else if (token == '/')
        {
            if (file != FILE_NB)
                return PositionSetError(
                  "Invalid FEN. Trying to end rank when not at the end of it.");

            --rank;
            file = FILE_A;

            if (rank < RANK_1)
                return PositionSetError("Invalid FEN. Invalid rank reached.");
        }
        else
        {
            if (file >= FILE_NB)
                return PositionSetError("Invalid FEN. Invalid file reached.");

            const usize idx = PieceToChar.find(token);
            if (idx == string::npos)
                return PositionSetError(std::string("Invalid FEN. Invalid piece: ")
                                        + std::string(1, token));

            if (++numPieces > 32)
                return PositionSetError("Invalid FEN. More than 32 pieces on the board.");

            const Square sq = make_square(File(file), Rank(rank));
            put_piece(Piece(idx), sq);

            ++file;
        }
    }

    if (rank != RANK_1 || file != FILE_NB)
        return PositionSetError("Invalid FEN. Board state encoding ended but cursor not at end.");

    if (pieces(PAWN) & (Rank1BB | Rank8BB))
        return PositionSetError("Unsupported position. Pawns on the first or eighth rank.");

    if (count<KING>(WHITE) != 1 || count<KING>(BLACK) != 1)
        return PositionSetError("Unsupported position. Incorrect number of kings.");

    for (Color c : {WHITE, BLACK})
    {
        if (count<PAWN>(c) > 8)
            return PositionSetError(std::string("Unsupported position. ")
                                    + (c == WHITE ? "WHITE" : "BLACK") + " has more than 8 pawns.");

        int additional = std::max(count<KNIGHT>(c) - 2, 0) + std::max(count<BISHOP>(c) - 2, 0)
                       + std::max(count<ROOK>(c) - 2, 0) + std::max(count<QUEEN>(c) - 1, 0);
        if (additional > 8 - count<PAWN>(c))
            return PositionSetError(std::string("Unsupported position. Too many pieces for ")
                                    + (c == WHITE ? "WHITE." : "BLACK."));
    }

    // 2. Active color
    if (!(ss >> token))
        return PositionSetError("Invalid FEN. Unexpected end of stream.");
    if (token != 'w' && token != 'b')
        return PositionSetError(std::string("Invalid FEN. Invalid side to move: ")
                                + std::string(1, token));
    sideToMove = (token == 'w' ? WHITE : BLACK);
    if (!(ss >> token) || !isspace(token) || ss.eof())
        return PositionSetError("Invalid FEN. Expected whitespace after side to move.");

    // 3. Castling availability. Compatible with 3 standards: Normal FEN standard,
    // Shredder-FEN that uses the letters of the columns on which the rooks began
    // the game instead of KQkq and also X-FEN standard that, in case of Chess960,
    // if an inner rook is associated with the castling right, the castling tag is
    // replaced by the file letter of the involved rook, as for the Shredder-FEN.
    //
    // NOTE: Due to the prevalence of incorrect (or missing) castling rights the
    // validation is less strict. However, incorrect castling rights are still sanitized.
    int num_castling_rights = 0;
    for (;;)
    {
        if (!(ss >> token))
            break;

        if (isspace(token))
            break;

        if (num_castling_rights == 0 && token == '-')
        {
            ss >> std::ws;
            break;
        }

        if (++num_castling_rights > 4)
            return PositionSetError("Invalid FEN. Maximum of 4 castling rights can be specified.");

        Square rsq  = SQ_NONE;
        Square ksq  = SQ_NONE;
        Color  c    = islower(token) ? BLACK : WHITE;
        Piece  rook = make_piece(c, ROOK);
        Piece  king = make_piece(c, KING);

        token = char(toupper(token));

        if (token == 'K' || token == 'Q')
        {
            const int dir = token == 'K' ? -1 : 1;
            Square    sq  = relative_square(c, token == 'K' ? SQ_H1 : SQ_A1);
            // Look for a rook and a king for the castling. King must come later.
            // Only the first rook is noted.
            // If the castling rights are available the king must always be between files 2 and 7 inclusive
            // so there is no need to check the last square.
            for (int i = 0; i < 7; ++i, sq = Square(sq + dir))
            {
                const Piece pc = piece_on(sq);
                if (pc == king)
                {
                    ksq = sq;
                    break;
                }
                else if (pc == rook && rsq == SQ_NONE)
                    rsq = sq;
            }
        }
        else if (token >= 'A' && token <= 'H')
        {
            const Square rsqCandidate = make_square(File(token - 'A'), relative_rank(c, RANK_1));
            if (piece_on(rsqCandidate) == rook)
                rsq = rsqCandidate;

            // If the castling rights are available the king must always be between files 2 and 7 inclusive.
            Square sq = square<KING>(c);
            if (relative_rank(c, sq) == RANK_1 && file_of(sq) >= FILE_B && file_of(sq) <= FILE_G)
                ksq = sq;
        }
        else
            return PositionSetError(std::string("Invalid FEN. Expected castling rights. Got: ")
                                    + std::string(1, token));

        // Only apply castling rights if they can be valid.
        if (ksq != SQ_NONE && rsq != SQ_NONE)
            set_castling_right(c, rsq);
    }

    // 4. En passant square.
    // Ignore if square is invalid or not on side to move relative rank 6.
    bool          legalEP = false;
    unsigned char col     = '-', row;
    ss >> col;
    if (col != '-')
    {
        if (!(ss >> row))
            return PositionSetError("Invalid FEN. Unexpected end of stream.");

        if ((col >= 'a' && col <= 'h') && (row == (sideToMove == WHITE ? '6' : '3')))
        {
            st->epSquare = make_square(File(col - 'a'), Rank(row - '1'));

            Bitboard pawns = attacks_bb(PAWN, st->epSquare, ~sideToMove) & pieces(sideToMove, PAWN);
            Bitboard target = (pieces(~sideToMove, PAWN) & (st->epSquare + pawn_push(~sideToMove)));
            Bitboard occ    = pieces() ^ target ^ st->epSquare;

            // En passant square will be considered only if
            // a) side to move have a pawn threatening epSquare
            // b) there is an enemy pawn in front of epSquare
            // c) there is no piece on epSquare or behind epSquare
            const bool enpassant =
              pawns && target
              && !(pieces() & (st->epSquare | (st->epSquare + pawn_push(sideToMove))));

            // If no pawn can execute the en passant capture without leaving the king in check, don't record the epSquare
            while (enpassant && pawns && !legalEP)
                legalEP = !(attackers_to(square<KING>(sideToMove), occ ^ pop_lsb(pawns))
                            & pieces(~sideToMove) & ~target);
        }
        else
            return PositionSetError("Invalid FEN. Invalid en-passant square.");
    }

    if (!legalEP)
        st->epSquare = SQ_NONE;

    // 5-6. Halfmove clock and fullmove number
    ss >> std::skipws >> st->rule50 >> gamePly;

    // Normally values larger than 99 would be pointless but we do support ignoring 50 move rule for TB purposes.
    // Limit at 2**15 as it's used multiplicatively with position evaluation during search.
    if (st->rule50 < 0 || st->rule50 > 32767)
        return PositionSetError("Unsupported position. Rule50 counter out of range.");

    if (gamePly < 0 || gamePly > 100000)
        return PositionSetError("Unsupported position. Game ply out of range.");

    // Convert from fullmove starting from 1 to gamePly starting from 0,
    // handle also common incorrect FEN with fullmove = 0.
    gamePly = std::max(2 * (gamePly - 1), 0) + (sideToMove == BLACK);

    chess960 = isChess960;
    set_state();

    if (attackers_to(square<KING>(~sideToMove)) & pieces(sideToMove))
        return PositionSetError("Unsupported position. King can be captured.");

    assert(pos_is_ok());

    return std::nullopt;
}


// Helper function used to set castling
// rights given the corresponding color and the rook starting square.
void Position::set_castling_right(Color c, Square rfrom) {

    Square         kfrom = square<KING>(c);
    CastlingRights cr    = c & (kfrom < rfrom ? KING_SIDE : QUEEN_SIDE);

    st->castlingRights |= cr;
    castlingRightsMask[kfrom] |= cr;
    castlingRightsMask[rfrom] |= cr;
    castlingRookSquare[cr] = rfrom;

    Square kto = relative_square(c, cr & KING_SIDE ? SQ_G1 : SQ_C1);
    Square rto = relative_square(c, cr & KING_SIDE ? SQ_F1 : SQ_D1);

    castlingPath[cr] = (between_bb(rfrom, rto) | between_bb(kfrom, kto)) & ~(kfrom | rfrom);
}


// Sets pinners and where stm can give check. Squares threatened by ~stm are
// computed lazily by compute_threats().
void Position::set_extra_bitboards() const {

    update_slider_blockers(WHITE);
    update_slider_blockers(BLACK);

    Square ksq                              = square<KING>(~sideToMove);
    const auto [bishopAttacks, rookAttacks] = both_attacks_bb(ksq, pieces());

    st->checkSquares[PAWN]   = attacks_bb(PAWN, ksq, ~sideToMove);
    st->checkSquares[KNIGHT] = attacks_bb(KNIGHT, ksq);
    st->checkSquares[BISHOP] = bishopAttacks;
    st->checkSquares[ROOK]   = rookAttacks;
    st->checkSquares[QUEEN]  = st->checkSquares[BISHOP] | st->checkSquares[ROOK];
    st->checkSquares[KING]   = 0;

    st->threats[ALL_PIECES] = 0;
}


void Position::compute_threats() const {

    Bitboard occ            = pieces() ^ square<KING>(sideToMove);
    st->threats[ALL_PIECES] = st->threats[PAWN]    = xattacks_by(PAWN, ~sideToMove, occ);
    st->threats[ALL_PIECES] |= st->threats[KNIGHT] = xattacks_by(KNIGHT, ~sideToMove, occ);
    st->threats[ALL_PIECES] |= st->threats[BISHOP] = xattacks_by(BISHOP, ~sideToMove, occ);
    st->threats[ALL_PIECES] |= st->threats[ROOK]   = xattacks_by(ROOK, ~sideToMove, occ);
    st->threats[ALL_PIECES] |= st->threats[QUEEN]  = xattacks_by(QUEEN, ~sideToMove, occ);
    st->threats[ALL_PIECES] |= st->threats[KING]   = xattacks_by(KING, ~sideToMove, occ);
}


// Computes the hash keys of the position, and other
// data that once computed is updated incrementally as moves are made.
// The function is only used when a new position is set up
void Position::set_state() const {

    st->key               = 0;
    st->minorPieceKey     = 0;
    st->nonPawnKey[WHITE] = st->nonPawnKey[BLACK] = 0;
    st->pawnKey                                   = Zobrist::noPawns;
    st->nonPawnMaterial[WHITE] = st->nonPawnMaterial[BLACK] = VALUE_ZERO;
    st->checkersBB = attackers_to(square<KING>(sideToMove)) & pieces(~sideToMove);

    set_extra_bitboards();

    for (Bitboard b = pieces(); b;)
    {
        Square s  = pop_lsb(b);
        Piece  pc = piece_on(s);
        st->key ^= Zobrist::psq[pc][s];

        if (type_of(pc) == PAWN)
            st->pawnKey ^= Zobrist::psq[pc][s];

        else
        {
            st->nonPawnKey[color_of(pc)] ^= Zobrist::psq[pc][s];

            if (type_of(pc) != KING)
            {
                st->nonPawnMaterial[color_of(pc)] += PieceValue[pc];

                if (type_of(pc) <= BISHOP)
                    st->minorPieceKey ^= Zobrist::psq[pc][s];
            }
        }
    }

    if (st->epSquare != SQ_NONE)
        st->key ^= Zobrist::enpassant[file_of(st->epSquare)];

    if (sideToMove == BLACK)
        st->key ^= Zobrist::side;

    st->key ^= Zobrist::castling[st->castlingRights];
    st->materialKey = compute_material_key();

    st->piecePlacement = pack_pieces(board);
}

Key Position::compute_material_key() const {
    Key k = 0;
    for (Piece pc : Pieces)
        for (int cnt = 0; cnt < pieceCount[pc]; ++cnt)
            k ^= Zobrist::psq[pc][8 + cnt];
    return k;
}


// Overload to initialize the position object with the given endgame code string
// like "KBPKN". It's mainly a helper to get the material key out of an endgame code.
std::optional<PositionSetError> Position::set(const string& code, Color c, StateInfo* si) {

    assert(code[0] == 'K');

    string sides[] = {code.substr(code.find('K', 1)),                                // Weak
                      code.substr(0, std::min(code.find('v'), code.find('K', 1)))};  // Strong

    assert(sides[0].length() > 0 && sides[0].length() < 8);
    assert(sides[1].length() > 0 && sides[1].length() < 8);

    std::transform(sides[c].begin(), sides[c].end(), sides[c].begin(), tolower);

    string fenStr = "8/" + sides[0] + char(8 - sides[0].length() + '0') + "/8/8/8/8/" + sides[1]
                  + char(8 - sides[1].length() + '0') + "/8 w - - 0 10";

    return set(fenStr, false, si);
}


// Returns a FEN representation of the position. In case of
// Chess960 the Shredder-FEN notation is used. This is mainly a debugging function.
string Position::fen() const {

    int                emptyCnt;
    std::ostringstream ss;

    for (Rank r = RANK_8;; --r)
    {
        for (File f = FILE_A; f <= FILE_H; ++f)
        {
            for (emptyCnt = 0; f <= FILE_H && empty(make_square(f, r)); ++f)
                ++emptyCnt;

            if (emptyCnt)
                ss << emptyCnt;

            if (f <= FILE_H)
                ss << PieceToChar[piece_on(make_square(f, r))];
        }

        if (r == RANK_1)
            break;
        ss << '/';
    }

    ss << (sideToMove == WHITE ? " w " : " b ");

    if (can_castle(WHITE_OO))
        ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OO))) : 'K');

    if (can_castle(WHITE_OOO))
        ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OOO))) : 'Q');

    if (can_castle(BLACK_OO))
        ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OO))) : 'k');

    if (can_castle(BLACK_OOO))
        ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OOO))) : 'q');

    if (!can_castle(ANY_CASTLING))
        ss << '-';

    ss << (ep_square() == SQ_NONE ? " - " : " " + UCIEngine::square(ep_square()) + " ")
       << st->rule50 << " " << 1 + (gamePly - (sideToMove == BLACK)) / 2;

    return ss.str();
}

// Calculates st->blockersForKing[c] and st->pinners[~c],
// which store respectively the pieces preventing king of color c from being in check
// and the slider pieces of color ~c pinning pieces of color c to the king.
void Position::update_slider_blockers(Color c) const {

    Square ksq = square<KING>(c);

    st->blockersForKing[c] = 0;
    st->pinners[~c]        = 0;

    // Snipers are sliders that attack 's' when a piece and other snipers are removed
    Bitboard snipers = ((attacks_bb(ROOK, ksq) & pieces(QUEEN, ROOK))
                        | (attacks_bb(BISHOP, ksq) & pieces(QUEEN, BISHOP)))
                     & pieces(~c);
    Bitboard occupancy = pieces() ^ snipers;

    while (snipers)
    {
        Square   sniperSq = pop_lsb(snipers);
        Bitboard b        = between_bb(ksq, sniperSq) & occupancy;

        if (b && !more_than_one(b))
        {
            st->blockersForKing[c] |= b;
            if (b & pieces(c))
                st->pinners[~c] |= sniperSq;
        }
    }
}


// Computes a bitboard of all pieces which attack a given square.
// Slider attacks use the occupied bitboard to indicate occupancy.
Bitboard Position::attackers_to(Square s, Bitboard occupied) const {

    const auto [bishopAttacks, rookAttacks] = both_attacks_bb(s, occupied);

    return (rookAttacks & pieces(ROOK, QUEEN)) | (bishopAttacks & pieces(BISHOP, QUEEN))
         | (attacks_bb(PAWN, s, BLACK) & pieces(WHITE, PAWN))
         | (attacks_bb(PAWN, s, WHITE) & pieces(BLACK, PAWN))
         | (attacks_bb(KNIGHT, s) & pieces(KNIGHT)) | (attacks_bb(KING, s) & pieces(KING));
}

// Takes a random move and tests whether the move is legal. It is used to validate moves
// from TT that can be corrupted due to SMP concurrent access or hash position key aliasing.
bool Position::legal(Move m) const {

    Color  us   = sideToMove;
    Square from = m.from_sq();
    Square to   = m.to_sq();
    Piece  pc   = moved_piece(m);

    // Use a slower but simpler function for uncommon cases
    if (m.type_of() != NORMAL)
        return MoveList<ALL>(*this).contains(m);

    // Is not a promotion, so the promotion piece must be empty
    assert(m.promotion_type() - KNIGHT == NO_PIECE_TYPE);

    // If the 'from' square is not occupied by a piece belonging to the side to
    // move, the move is obviously not legal.
    if (pc == NO_PIECE || color_of(pc) != us)
        return false;

    // The destination square cannot be occupied by a friendly piece
    if (pieces(us) & to)
        return false;

    // Handle the special case of a pawn move
    if (type_of(pc) == PAWN)
    {
        // We have already handled promotion moves, so destination cannot be on the 8th/1st rank
        if ((Rank8BB | Rank1BB) & to)
            return false;

        // Check if it's a valid capture, single push, or double push
        const bool isCapture    = bool(attacks_bb(PAWN, from, us) & pieces(~us) & to);
        const bool isSinglePush = (from + pawn_push(us) == to) && empty(to);
        const bool isDoublePush = (from + 2 * pawn_push(us) == to)
                               && (relative_rank(us, from) == RANK_2) && empty(to)
                               && empty(to - pawn_push(us));

        if (!(isCapture || isSinglePush || isDoublePush))
            return false;
    }
    else if (!(attacks_bb(type_of(pc), from, pieces()) & to))
        return false;


    if (type_of(pc) != KING)
    {
        if (checkers())
        {
            // In double check, only a king move can evade
            if (more_than_one(checkers()))
                return false;

            // The move must block the check or capture the checker
            if (!(between_bb(square<KING>(us), lsb(checkers())) & to))
                return false;
        }
        // When not in check, a non-king move is legal if and only if it is not pinned
        // or it is moving along the ray towards or away from the king.
        return !(blockers_for_king(us) & from) || line_bb(from, to) & pieces(us, KING);
    }
    else
        // If the moving piece is a king, check whether the destination square is
        // attacked by the opponent.
        return !(to & threats_by(ALL_PIECES));
}


// Tests whether a legal move gives a check
bool Position::gives_check(Move m) const {

    assert(m.is_ok());
    assert(color_of(moved_piece(m)) == sideToMove);

    Square from = m.from_sq();
    Square to   = m.to_sq();

    // Is there a direct check?
    if (check_squares(type_of(piece_on(from))) & to)
        return true;

    // Is there a discovered check?
    if (blockers_for_king(~sideToMove) & from)
        return !(line_bb(from, to) & pieces(~sideToMove, KING)) || m.type_of() == CASTLING;

    switch (m.type_of())
    {
    case NORMAL :
        return false;

    case PROMOTION :
        return attacks_bb(m.promotion_type(), to, pieces() ^ from) & pieces(~sideToMove, KING);

    // En passant capture with check? We have already handled the case of direct
    // checks and ordinary discovered check, so the only case we need to handle
    // is the unusual case of a discovered check through the captured pawn.
    case EN_PASSANT : {
        Square   capsq                          = make_square(file_of(to), rank_of(from));
        Bitboard b                              = (pieces() ^ from ^ capsq) | to;
        const auto [bishopAttacks, rookAttacks] = both_attacks_bb(square<KING>(~sideToMove), b);

        return (rookAttacks & pieces(sideToMove, QUEEN, ROOK))
             | (bishopAttacks & pieces(sideToMove, QUEEN, BISHOP));
    }
    default :  //CASTLING
    {
        // Castling is encoded as 'king captures the rook'
        Square rto = relative_square(sideToMove, to > from ? SQ_F1 : SQ_D1);

        return check_squares(ROOK) & rto;
    }
    }
}


// Makes a move, and saves all information necessary
// to a StateInfo object. The move is assumed to be legal.
// If a pointer to the shared histories is passed, their entries for the
// new position will be prefetched.
void Position::do_move(
  Move m, StateInfo& newSt, bool givesCheck, Dirties& dirties, const SharedHistories* history) {

    assert(m.is_ok());
    assert(&newSt != st);

    Key k = st->key ^ Zobrist::side;

    // Copy some fields of the old state to our new StateInfo object except the
    // ones which are going to be recalculated from scratch anyway and then switch
    // our state pointer to point to the new (ready to be updated) state.
    std::memcpy(&newSt, st, offsetof(StateInfo, key));
    newSt.previous = st;
    st             = &newSt;

    // Increment ply counters. In particular, rule50 will be reset to zero later on
    // in case of a capture or a pawn move.
    ++gamePly;
    ++st->rule50;
    ++st->pliesFromNull;

    auto& dpps = dirties.dirtyPawnPairs;
    auto& dts  = dirties.dirtyThreats;
    auto& dp   = dirties.dirtyPiece;

    dpps.before[WHITE] = pieces(WHITE, PAWN);
    dpps.before[BLACK] = pieces(BLACK, PAWN);

    Color  us       = sideToMove;
    Color  them     = ~us;
    Square from     = m.from_sq();
    Square to       = m.to_sq();
    Piece  pc       = piece_on(from);
    Piece  captured = m.type_of() == EN_PASSANT ? make_piece(them, PAWN) : piece_on(to);

    dp.pc     = pc;
    dp.from   = from;
    dp.to     = to;
    dp.add_sq = SQ_NONE;

    assert(color_of(pc) == us);
    assert(captured == NO_PIECE || color_of(captured) == (m.type_of() != CASTLING ? them : us));
    assert(type_of(captured) != KING);

    if (m.type_of() == CASTLING)
    {
        assert(pc == make_piece(us, KING));
        assert(captured == make_piece(us, ROOK));

        Square rfrom, rto;
        do_castling<true>(us, from, to, rfrom, rto, &dts, &dp);

        k ^= Zobrist::psq[captured][rfrom] ^ Zobrist::psq[captured][rto];
        st->nonPawnKey[us] ^= Zobrist::psq[captured][rfrom] ^ Zobrist::psq[captured][rto];
        captured = NO_PIECE;
    }
    else if (captured)
    {
        Square capsq = to;

        // If the captured piece is a pawn, update pawn hash key, otherwise
        // update non-pawn material.
        if (type_of(captured) == PAWN)
        {
            if (m.type_of() == EN_PASSANT)
            {
                capsq -= pawn_push(us);

                assert(pc == make_piece(us, PAWN));
                assert(to == st->epSquare);
                assert(relative_rank(us, to) == RANK_6);
                assert(piece_on(to) == NO_PIECE);
                assert(piece_on(capsq) == make_piece(them, PAWN));

                // Update board and piece lists in ep case, normal captures are updated later
                remove_piece(capsq, &dts);
            }

            st->pawnKey ^= Zobrist::psq[captured][capsq];
        }
        else
        {
            st->nonPawnMaterial[them] -= PieceValue[captured];
            st->nonPawnKey[them] ^= Zobrist::psq[captured][capsq];

            if (type_of(captured) <= BISHOP)
                st->minorPieceKey ^= Zobrist::psq[captured][capsq];
        }

        dp.remove_pc = captured;
        dp.remove_sq = capsq;

        k ^= Zobrist::psq[captured][capsq];
        st->materialKey ^=
          Zobrist::psq[captured][8 + pieceCount[captured] - (m.type_of() != EN_PASSANT)];

        // Reset rule 50 counter
        st->rule50 = 0;
    }
    else
        dp.remove_sq = SQ_NONE;

    // Update hash key
    k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

    // Reset en passant square
    if (st->epSquare != SQ_NONE)
    {
        k ^= Zobrist::enpassant[file_of(st->epSquare)];
        st->epSquare = SQ_NONE;
    }

    // Update castling rights.
    k ^= Zobrist::castling[st->castlingRights];
    st->castlingRights &= ~(castlingRightsMask[from] | castlingRightsMask[to]);
    k ^= Zobrist::castling[st->castlingRights];

    // If the moving piece is a pawn do some special extra work
    if (type_of(pc) == PAWN)
    {
        // Check if the en passant square needs to be set. Accurate e.p. info is needed
        // for correct zobrist key generation and 3-fold checking.
        if ((int(to) ^ int(from)) == 16)
        {
            Square   epSquare = to - pawn_push(us);
            Bitboard pawns    = attacks_bb(PAWN, epSquare, us) & pieces(them, PAWN);

            // If there are no pawns attacking the ep square, ep is not possible.
            if (pawns)
            {
                Square   ksq         = square<KING>(them);
                Bitboard notBlockers = ~st->previous->blockersForKing[them];
                bool     noDiscovery = (from & notBlockers) || file_of(from) == file_of(ksq);

                // If the pawn gives discovered check, ep is never legal. Else, if at least one
                // pawn was not a blocker for the enemy king or lies on the same line as the
                // enemy king and en passant square, a legal capture exists.
                if (noDiscovery && (pawns & (notBlockers | line_bb(epSquare, ksq))))
                {
                    st->epSquare = epSquare;
                    k ^= Zobrist::enpassant[file_of(epSquare)];
                }
            }
        }

        else if (m.type_of() == PROMOTION)
        {
            PieceType pt        = m.promotion_type();
            Piece     promotion = make_piece(us, pt);

            assert(relative_rank(us, to) == RANK_8);
            assert(pt >= KNIGHT && pt <= QUEEN);

            dp.add_pc = promotion;
            dp.add_sq = to;
            dp.to     = SQ_NONE;

            // Update hash keys
            // Zobrist::psq[pc][to] is zero, so we don't need to clear it
            k ^= Zobrist::psq[promotion][to];
            st->materialKey ^= Zobrist::psq[promotion][8 + pieceCount[promotion]]
                             ^ Zobrist::psq[pc][8 + pieceCount[pc] - 1];
            st->nonPawnKey[us] ^= Zobrist::psq[promotion][to];

            if (pt <= BISHOP)
                st->minorPieceKey ^= Zobrist::psq[promotion][to];

            // Update material
            st->nonPawnMaterial[us] += PieceValue[promotion];
        }

        // Update pawn hash key
        st->pawnKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

        // Reset rule 50 draw counter
        st->rule50 = 0;
    }

    else
    {
        st->nonPawnKey[us] ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

        if (type_of(pc) <= BISHOP)
            st->minorPieceKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    }

    // Update the key with the final value
    st->key = k;

    if (history)
    {
        prefetch(&history->pawn_entry(*this)[pc][to]);
        prefetch(&history->pawn_correction_entry(*this));
        prefetch(&history->minor_piece_correction_entry(*this));
        prefetch(&history->nonpawn_correction_entry<WHITE>(*this));
        prefetch(&history->nonpawn_correction_entry<BLACK>(*this));
    }

    // Move the piece. The tricky Chess960 castling is handled earlier
    if (m.type_of() != CASTLING)
    {
        const Piece toPc = m.type_of() == PROMOTION ? make_piece(us, m.promotion_type()) : pc;

        if (captured && m.type_of() != EN_PASSANT)
        {
            remove_piece(from, &dts);
            swap_piece(to, toPc, &dts);
        }
        else if (pc == toPc)
            move_piece(from, to, &dts);
        else
        {
            remove_piece(from, &dts);
            put_piece(toPc, to, &dts);
        }
    }

    // Set capture piece
    st->capturedPiece = captured;

    // Calculate checkers bitboard (if move gives check)
    st->checkersBB = givesCheck ? attackers_to(square<KING>(them)) & pieces(us) : 0;

    sideToMove = ~sideToMove;

    // Update king attacks used for fast check detection
    set_extra_bitboards();

    st->piecePlacement = pack_pieces(board);

    // Calculate the repetition info. It is the ply distance from the previous
    // occurrence of the same position, negative in the 3-fold case, or zero
    // if the position was not repeated.
    st->repetition = 0;
    int end        = std::min(st->rule50, st->pliesFromNull);
    if (end >= 4)
    {
        StateInfo* stp = st->previous->previous;
        for (int i = 4; i <= end; i += 2)
        {
            stp = stp->previous->previous;
            // The piecePlacement, in addition to the Zobrist key, provides accurate
            // repetition detection.
            if (stp->key == st->key && stp->piecePlacement == st->piecePlacement)
            {
                st->repetition = stp->repetition ? -i : i;
                break;
            }
        }
    }

    assert(pos_is_ok());

    dpps.after[WHITE] = pieces(WHITE, PAWN);
    dpps.after[BLACK] = pieces(BLACK, PAWN);

    assert(dp.pc != NO_PIECE);
    assert(!(bool(captured) || m.type_of() == CASTLING) ^ (dp.remove_sq != SQ_NONE));
    assert(dp.from != SQ_NONE);
    assert(!(dp.add_sq != SQ_NONE) ^ (m.type_of() == PROMOTION || m.type_of() == CASTLING));
}


// Unmakes a move. When it returns, the position should
// be restored to exactly the same state as before the move was made.
void Position::undo_move(Move m) {

    assert(m.is_ok());

    sideToMove = ~sideToMove;

    Color  us   = sideToMove;
    Square from = m.from_sq();
    Square to   = m.to_sq();
    Piece  pc   = piece_on(to);

    assert(empty(from) || m.type_of() == CASTLING);
    assert(type_of(st->capturedPiece) != KING);

    if (m.type_of() == PROMOTION)
    {
        assert(relative_rank(us, to) == RANK_8);
        assert(type_of(pc) == m.promotion_type());
        assert(type_of(pc) >= KNIGHT && type_of(pc) <= QUEEN);

        pc = make_piece(us, PAWN);
        swap_piece(to, pc);
    }

    if (m.type_of() == CASTLING)
    {
        Square rfrom, rto;
        do_castling<false>(us, from, to, rfrom, rto);
    }
    else
    {
        move_piece(to, from);  // Put the piece back at the source square

        if (st->capturedPiece)
        {
            Square capsq = to;

            if (m.type_of() == EN_PASSANT)
            {
                capsq -= pawn_push(us);

                assert(type_of(pc) == PAWN);
                assert(to == st->previous->epSquare);
                assert(relative_rank(us, to) == RANK_6);
                assert(piece_on(capsq) == NO_PIECE);
                assert(st->capturedPiece == make_piece(~us, PAWN));
            }

            put_piece(st->capturedPiece, capsq);  // Restore the captured piece
        }
    }

    // Finally point our state pointer back to the previous state
    st = st->previous;
    --gamePly;

    assert(pos_is_ok());
}

inline void add_dirty_threat(DirtyThreats* const dts,
                             bool                putPiece,
                             Piece               pc,
                             Piece               threatened,
                             Square              s,
                             Square              threatenedSq) {
    dts->list.push_back({pc, threatened, s, threatenedSq, putPiece});
}

#ifdef USE_AVX512ICL
// Given a DirtyThreat template and bit offsets to insert the piece type and square, write the threats
// present at the given bitboard.
template<int SqShift, int PcShift>
void write_multiple_dirties(const Position& p,
                            Bitboard        mask,
                            DirtyThreat     dt_template,
                            DirtyThreats*   dts) {
    static_assert(sizeof(DirtyThreat) == 4);

    const __m512i board    = _mm512_loadu_si512(p.piece_array().data());
    const int     dt_count = popcount(mask);
    assert(dt_count <= 16);

    const __m512i template_v = _mm512_set1_epi32(dt_template.raw());
    auto*         write      = dts->list.make_space(dt_count);

    // Extract the list of squares and upconvert to 32 bits. There are never more than 16
    // incoming threats so this is sufficient.
    __m512i threat_squares = _mm512_maskz_compress_epi8(mask, AllSquares);
    threat_squares         = _mm512_cvtepi8_epi32(_mm512_castsi512_si128(threat_squares));

    __m512i threat_pieces =
      _mm512_maskz_permutexvar_epi8(0x1111111111111111ULL, threat_squares, board);

    // Shift the piece and square into place
    threat_squares = _mm512_slli_epi32(threat_squares, SqShift);
    threat_pieces  = _mm512_slli_epi32(threat_pieces, PcShift);

    const __m512i dirties =
      _mm512_ternarylogic_epi32(template_v, threat_squares, threat_pieces, 254 /* A | B | C */);
    _mm512_storeu_si512(write, dirties);
}
#endif

constexpr bool can_slider_threat(Piece pc, Piece slider) {
    return type_of(pc) != QUEEN || type_of(slider) == QUEEN;
}

template<bool ComputeRay>
void Position::update_piece_threats(Piece               pc,
                                    bool                putPiece,
                                    Square              s,
                                    DirtyThreats* const dts,
                                    // Silence spurious warning on GCC 10
                                    [[maybe_unused]] Bitboard noRaysContaining) const {
    const Bitboard occupied         = pieces();
    const auto [bAttacks, rAttacks] = both_attacks_bb(s, occupied);
    const Bitboard  sliderAttacks   = bAttacks | rAttacks;
    const Bitboard  occupiedNoK     = occupied ^ pieces(KING);
    const PieceType pt              = type_of(pc);
    const Bitboard  sliders = (pieces(BISHOP, QUEEN) & bAttacks) | (pieces(ROOK, QUEEN) & rAttacks);

    auto process_sliders = [&](bool addDirectAttacks) {
        Bitboard b = sliders;
        while (b)
        {
            Square sliderSq = pop_lsb(b);
            Piece  slider   = piece_on(sliderSq);

            const Bitboard ray        = ray_pass_bb(sliderSq, s);
            const Bitboard discovered = ray & sliderAttacks & occupiedNoK;

            assert(!more_than_one(discovered));
            if (discovered && (ray & noRaysContaining) != noRaysContaining)
            {
                const Square threatenedSq = lsb(discovered);
                const Piece  threatenedPc = piece_on(threatenedSq);
                if (can_slider_threat(threatenedPc, slider))
                    add_dirty_threat(dts, !putPiece, slider, threatenedPc, sliderSq, threatenedSq);
            }

            if (addDirectAttacks && can_slider_threat(pc, slider))
                add_dirty_threat(dts, putPiece, slider, pc, sliderSq, s);
        }
    };

    // Kings emit no direct threats
    if (pt == KING)
    {
        if constexpr (ComputeRay)
            process_sliders(false);
        return;
    }

    const Bitboard threatTargets = pt == PAWN                 ? pieces(KNIGHT, ROOK)
                                 : pt == BISHOP || pt == ROOK ? pieces(PAWN, KNIGHT, BISHOP, ROOK)
                                                              : occupiedNoK;
    Bitboard       threatened    = (pt == BISHOP  ? bAttacks
                                    : pt == ROOK  ? rAttacks
                                    : pt == QUEEN ? sliderAttacks
                                    : pt == PAWN  ? PseudoAttacks[color_of(pc)][s]
                                                  : PseudoAttacks[pt][s])
                        & threatTargets;
    Bitboard incomingThreats = PseudoAttacks[KNIGHT][s] & pieces(KNIGHT);

    if (pt == KNIGHT || pt == ROOK)
        incomingThreats |= (attacks_bb(PAWN, s, WHITE) & pieces(BLACK, PAWN))
                         | (attacks_bb(PAWN, s, BLACK) & pieces(WHITE, PAWN));

#ifdef USE_AVX512ICL
    write_multiple_dirties<DirtyThreat::ThreatenedSqOffset, DirtyThreat::ThreatenedPcOffset>(
      *this, threatened, {pc, NO_PIECE, s, Square(0), putPiece}, dts);

    const Bitboard directSliders = pt == QUEEN ? sliders & pieces(QUEEN) : sliders;
    write_multiple_dirties<DirtyThreat::PcSqOffset, DirtyThreat::PcOffset>(
      *this, directSliders | incomingThreats, {NO_PIECE, pc, Square(0), s, putPiece}, dts);

    // For ICL, direct threats were written above
    if constexpr (ComputeRay)
        process_sliders(false);
#else
    while (threatened)
    {
        Square threatenedSq = pop_lsb(threatened);
        Piece  threatenedPc = piece_on(threatenedSq);
        assert(threatenedSq != s);
        assert(threatenedPc != NO_PIECE);

        add_dirty_threat(dts, putPiece, pc, threatenedPc, s, threatenedSq);
    }

    if constexpr (ComputeRay)
        process_sliders(true);
    else
        incomingThreats |= pt == QUEEN ? sliders & pieces(QUEEN) : sliders;

    while (incomingThreats)
    {
        Square srcSq = pop_lsb(incomingThreats);
        Piece  srcPc = piece_on(srcSq);
        assert(srcSq != s);
        assert(srcPc != NO_PIECE);

        add_dirty_threat(dts, putPiece, srcPc, pc, srcSq, s);
    }
#endif
}

Key Position::prefetch_key(Move m) const {
    Square from     = m.from_sq();
    Square to       = m.to_sq();
    Piece  pc       = piece_on(from);
    Piece  captured = piece_on(to);
    Key    k        = st->key ^ Zobrist::side;

    k ^= Zobrist::psq[captured][to] ^ Zobrist::psq[pc][to] ^ Zobrist::psq[pc][from];

    if (captured || type_of(pc) == PAWN)
        return k;

    return adjust_key50<true>(k);
}

// Helper used to do/undo a castling move. This is a bit
// tricky in Chess960 where from/to squares can overlap.
template<bool Do>
void Position::do_castling(Color                              us,
                           Square                             from,
                           Square&                            to,
                           Square&                            rfrom,
                           Square&                            rto,
                           DirtyThreats* const                dts,
                           [[maybe_unused]] DirtyPiece* const dp) {

    bool kingSide = to > from;
    rfrom         = to;  // Castling is encoded as "king captures friendly rook"
    rto           = relative_square(us, kingSide ? SQ_F1 : SQ_D1);
    to            = relative_square(us, kingSide ? SQ_G1 : SQ_C1);

    assert(!Do || dp);

    if constexpr (Do)
    {
        dp->to        = to;
        dp->remove_pc = dp->add_pc = make_piece(us, ROOK);
        dp->remove_sq              = rfrom;
        dp->add_sq                 = rto;
    }

    // Remove both pieces first since squares could overlap in Chess960
    remove_piece(Do ? from : to, dts);
    remove_piece(Do ? rfrom : rto, dts);
    put_piece(make_piece(us, KING), Do ? to : from, dts);
    put_piece(make_piece(us, ROOK), Do ? rto : rfrom, dts);
}


// Used to do a "null move": it flips
// the side to move without executing any move on the board.
void Position::do_null_move(StateInfo& newSt) {

    assert(!checkers());
    assert(&newSt != st);

    // Remaining fields are recomputed, invalidated or overwritten below.
    std::memcpy(&newSt, st, offsetof(StateInfo, blockersForKing));
    newSt.piecePlacement = st->piecePlacement;

    newSt.previous = st;
    st             = &newSt;

    if (st->epSquare != SQ_NONE)
    {
        st->key ^= Zobrist::enpassant[file_of(st->epSquare)];
        st->epSquare = SQ_NONE;
    }

    st->key ^= Zobrist::side;

    st->pliesFromNull = 0;

    st->capturedPiece = NO_PIECE;

    sideToMove = ~sideToMove;

    set_extra_bitboards();

    st->repetition = 0;

    assert(pos_is_ok());
}


// Must be used to undo a "null move"
void Position::undo_null_move() {

    assert(!checkers());

    st         = st->previous;
    sideToMove = ~sideToMove;
}


// Tests if the SEE (Static Exchange Evaluation)
// value of the move is greater or equal to the given threshold. We'll use an
// algorithm similar to alpha-beta pruning with a null window.
bool Position::see_ge(Move m, int threshold) const {

    assert(m.is_ok());

    // Only deal with normal moves, assume others pass a simple SEE
    if (m.type_of() != NORMAL)
        return VALUE_ZERO >= threshold;

    Square from = m.from_sq(), to = m.to_sq();

    assert(piece_on(from) != NO_PIECE);

    int swap = PieceValue[piece_on(to)] - threshold;
    if (swap < 0)
        return false;

    swap = PieceValue[piece_on(from)] - swap;
    if (swap <= 0)
        return true;

    assert(color_of(piece_on(from)) == sideToMove);
    Bitboard occupied  = pieces() ^ from ^ to;  // xoring to is important for pinned piece logic
    Color    stm       = sideToMove;
    Bitboard attackers = attackers_to(to, occupied);
    Bitboard stmAttackers, bb;
    int      res = 1;

    while (true)
    {
        stm = ~stm;
        attackers &= occupied;

        // If stm has no more attackers then give up: stm loses
        if (!(stmAttackers = attackers & pieces(stm)))
            break;

        // Don't allow pinned pieces to attack as long as there are
        // pinners on their original square.
        if (pinners(~stm) & occupied)
        {
            stmAttackers &= ~blockers_for_king(stm);

            if (!stmAttackers)
                break;
        }

        res ^= 1;

        // Locate and remove the next least valuable attacker, and add to
        // the bitboard 'attackers' any X-ray attackers behind it.
        if ((bb = stmAttackers & pieces(PAWN)))
        {
            if ((swap = PawnValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);

            attackers |= attacks_bb(BISHOP, to, occupied) & pieces(BISHOP, QUEEN);
        }

        else if ((bb = stmAttackers & pieces(KNIGHT)))
        {
            if ((swap = KnightValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);
        }

        else if ((bb = stmAttackers & pieces(BISHOP)))
        {
            if ((swap = BishopValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);

            attackers |= attacks_bb(BISHOP, to, occupied) & pieces(BISHOP, QUEEN);
        }

        else if ((bb = stmAttackers & pieces(ROOK)))
        {
            if ((swap = RookValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);

            attackers |= attacks_bb(ROOK, to, occupied) & pieces(ROOK, QUEEN);
        }

        else if ((bb = stmAttackers & pieces(QUEEN)))
        {
            swap = QueenValue - swap;
            //  implies that the previous recapture was done by a higher rated piece than a Queen (King is excluded)
            assert(swap >= res);
            occupied ^= least_significant_square_bb(bb);

            const auto [bishopAttacks, rookAttacks] = both_attacks_bb(to, occupied);
            attackers |=
              (bishopAttacks & pieces(BISHOP, QUEEN)) | (rookAttacks & pieces(ROOK, QUEEN));
        }

        else  // KING
              // If we "capture" with the king but the opponent still has attackers,
              // reverse the result.
            return (attackers & ~pieces(stm)) ? res ^ 1 : res;
    }

    return bool(res);
}

// Tests whether the position is drawn by 50-move rule
// or by repetition. It does not detect stalemates.
bool Position::is_draw(int ply) const {

    if (st->rule50 > 99 && (!checkers() || MoveList<ALL>(*this).size()))
        return true;

    return is_repetition(ply);
}

// Return a draw score if a position repeats once earlier but strictly
// after the root, or repeats twice before or at the root.
bool Position::is_repetition(int ply) const { return st->repetition && st->repetition < ply; }

// Tests whether there has been at least one repetition
// of positions since the last capture or pawn move.
bool Position::has_repeated() const {

    StateInfo* stc = st;
    int        end = std::min(st->rule50, st->pliesFromNull);
    while (end-- >= 4)
    {
        if (stc->repetition)
            return true;

        stc = stc->previous;
    }
    return false;
}


// Tests if the position has a move which draws by repetition.
// This function accurately matches the outcome of is_draw() over all legal moves.
bool Position::upcoming_repetition(int ply) const {

    int end = std::min(st->rule50, st->pliesFromNull);

    if (end < 3)
        return false;

    Key        originalKey = st->key;
    StateInfo* stp         = st->previous;
    Key        other       = originalKey ^ stp->key ^ Zobrist::side;

    for (int i = 3; i <= end; i += 2)
    {
        stp = stp->previous;
        other ^= stp->key ^ stp->previous->key ^ Zobrist::side;
        stp = stp->previous;

        if (other != 0)
            continue;

        Key moveKey = originalKey ^ stp->key;

        if (int j = H1(moveKey); cuckoo[j] == moveKey || (j = H2(moveKey), cuckoo[j] == moveKey))
        {
            Move   move = cuckooMove[j];
            Square s1   = move.from_sq();
            Square s2   = move.to_sq();

            if (!((between_bb(s1, s2) ^ s2) & pieces()))
            {
                auto b = st->piecePlacement;
                swap_nibble(b, s1, s2);

                if (b == stp->piecePlacement)
                {
                    // For nodes before or at the root, check that the move is a
                    // repetition rather than a move to the current position.
                    if (ply > i || stp->repetition)
                        return true;
                }
            }
        }
    }
    return false;
}


// Flips position with the white and black sides reversed. This
// is only useful for debugging e.g. for finding evaluation symmetry bugs.
std::optional<PositionSetError> Position::flip() {

    string            f, token;
    std::stringstream ss(fen());

    for (Rank r = RANK_8;; --r)  // Piece placement
    {
        std::getline(ss, token, r > RANK_1 ? '/' : ' ');
        f.insert(0, token + (f.empty() ? " " : "/"));

        if (r == RANK_1)
            break;
    }

    ss >> token;                        // Active color
    f += (token == "w" ? "B " : "W ");  // Will be lowercased later

    ss >> token;  // Castling availability
    f += token + " ";

    std::transform(f.begin(), f.end(), f.begin(),
                   [](unsigned char c) { return char(islower(c) ? toupper(c) : tolower(c)); });

    ss >> token;  // En passant square
    f += (token == "-" ? token : token.replace(1, 1, token[1] == '3' ? "6" : "3"));

    std::getline(ss, token);  // Half and full moves
    f += token;

    return set(f, is_chess960(), st);
}


bool Position::material_key_is_ok() const { return compute_material_key() == st->materialKey; }


// Performs some consistency checks for the position object
// and raise an assert if something wrong is detected.
// This is meant to be helpful when debugging.
bool Position::pos_is_ok() const {

    if ((sideToMove != WHITE && sideToMove != BLACK) || piece_on(square<KING>(WHITE)) != W_KING
        || piece_on(square<KING>(BLACK)) != B_KING
        || (ep_square() != SQ_NONE && relative_rank(sideToMove, ep_square()) != RANK_6))
        assert(0 && "pos_is_ok: Default");

    if (count<KING>(WHITE) != 1 || count<KING>(BLACK) != 1
        || attackers_to(square<KING>(~sideToMove)) & pieces(sideToMove))
        assert(0 && "pos_is_ok: Kings");

    if ((pieces(PAWN) & (Rank1BB | Rank8BB)) || count<PAWN>(WHITE) > 8 || count<PAWN>(BLACK) > 8)
        assert(0 && "pos_is_ok: Pawns");


    if (ep_square() != SQ_NONE)
    {
        Square ksq = square<KING>(sideToMove);

        Bitboard captured = (ep_square() + pawn_push(~sideToMove)) & pieces(~sideToMove, PAWN);
        Bitboard pawns    = attacks_bb(PAWN, ep_square(), ~sideToMove) & pieces(sideToMove, PAWN);
        Bitboard potentialCheckers = pieces(~sideToMove) ^ captured;

        if (!captured || !pawns
            || ((attackers_to(ksq, pieces() ^ captured ^ ep_square() ^ lsb(pawns))
                 & potentialCheckers)
                && (attackers_to(ksq, pieces() ^ captured ^ ep_square() ^ msb(pawns))
                    & potentialCheckers)))
            assert(0 && "pos_is_ok: En passant square");
    }

    if ((pieces(WHITE) & pieces(BLACK)) || (pieces(WHITE) | pieces(BLACK)) != pieces()
        || popcount(pieces(WHITE)) > 16 || popcount(pieces(BLACK)) > 16)
        assert(0 && "pos_is_ok: Bitboards");

    for (PieceType p1 = PAWN; p1 <= KING; ++p1)
        for (PieceType p2 = PieceType(p1 + 1); p2 <= KING; ++p2)
            if (pieces(p1) & pieces(p2))
                assert(0 && "pos_is_ok: Bitboards");

    for (Piece pc : Pieces)
        if (pieceCount[pc] != popcount(pieces(color_of(pc), type_of(pc)))
            || pieceCount[pc] != std::count(board.begin(), board.end(), pc))
            assert(0 && "pos_is_ok: Pieces");

    for (Color c : {WHITE, BLACK})
        for (CastlingRights cr : {c & KING_SIDE, c & QUEEN_SIDE})
        {
            if (!can_castle(cr))
                continue;

            if (piece_on(castling_rook_square(cr)) != make_piece(c, ROOK)
                || castlingRightsMask[castlingRookSquare[cr]] != cr
                || (castlingRightsMask[square<KING>(c)] & cr) != cr)
                assert(0 && "pos_is_ok: Castling");
        }

    assert(material_key_is_ok() && "pos_is_ok: materialKey");

    return true;
}

}  // namespace Stockfish

// === sf_src/src/nnue/network.cpp ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <type_traits>
#include <vector>
#include <filesystem>

#define INCBIN_SILENCE_BITCODE_WARNING

// === sf_src/src/incbin/incbin.h ===
/**
 * @file incbin.h
 * @author Dale Weiler
 * @brief Utility for including binary files
 *
 * Facilities for including binary files into the current translation unit and
 * making use from them externally in other translation units.
 */
#ifndef INCBIN_HDR
#define INCBIN_HDR
#include <limits.h>
#if   defined(__AVX512BW__) || \
      defined(__AVX512CD__) || \
      defined(__AVX512DQ__) || \
      defined(__AVX512ER__) || \
      defined(__AVX512PF__) || \
      defined(__AVX512VL__) || \
      defined(__AVX512F__)
# define INCBIN_ALIGNMENT_INDEX 6
#elif defined(__AVX__)      || \
      defined(__AVX2__)
# define INCBIN_ALIGNMENT_INDEX 5
#elif defined(__SSE__)      || \
      defined(__SSE2__)     || \
      defined(__SSE3__)     || \
      defined(__SSSE3__)    || \
      defined(__SSE4_1__)   || \
      defined(__SSE4_2__)   || \
      defined(__neon__)     || \
      defined(__ARM_NEON)   || \
      defined(__ALTIVEC__)
# define INCBIN_ALIGNMENT_INDEX 4
#elif ULONG_MAX != 0xffffffffu
# define INCBIN_ALIGNMENT_INDEX 3
# else
# define INCBIN_ALIGNMENT_INDEX 2
#endif

/* Lookup table of (1 << n) where `n' is `INCBIN_ALIGNMENT_INDEX' */
#define INCBIN_ALIGN_SHIFT_0 1
#define INCBIN_ALIGN_SHIFT_1 2
#define INCBIN_ALIGN_SHIFT_2 4
#define INCBIN_ALIGN_SHIFT_3 8
#define INCBIN_ALIGN_SHIFT_4 16
#define INCBIN_ALIGN_SHIFT_5 32
#define INCBIN_ALIGN_SHIFT_6 64

/* Actual alignment value */
#define INCBIN_ALIGNMENT \
    INCBIN_CONCATENATE( \
        INCBIN_CONCATENATE(INCBIN_ALIGN_SHIFT, _), \
        INCBIN_ALIGNMENT_INDEX)

/* Stringize */
#define INCBIN_STR(X) \
    #X
#define INCBIN_STRINGIZE(X) \
    INCBIN_STR(X)
/* Concatenate */
#define INCBIN_CAT(X, Y) \
    X ## Y
#define INCBIN_CONCATENATE(X, Y) \
    INCBIN_CAT(X, Y)
/* Deferred macro expansion */
#define INCBIN_EVAL(X) \
    X
#define INCBIN_INVOKE(N, ...) \
    INCBIN_EVAL(N(__VA_ARGS__))
/* Variable argument count for overloading by arity */
#define INCBIN_VA_ARG_COUNTER(_1, _2, _3, N, ...) N
#define INCBIN_VA_ARGC(...) INCBIN_VA_ARG_COUNTER(__VA_ARGS__, 3, 2, 1, 0)

/* Green Hills uses a different directive for including binary data */
#if defined(__ghs__)
#  if (__ghs_asm == 2)
#    define INCBIN_MACRO ".file"
/* Or consider the ".myrawdata" entry in the ld file */
#  else
#    define INCBIN_MACRO "\tINCBIN"
#  endif
#else
#  define INCBIN_MACRO ".incbin"
#endif

#ifndef _MSC_VER
#  define INCBIN_ALIGN \
    __attribute__((aligned(INCBIN_ALIGNMENT)))
#else
#  define INCBIN_ALIGN __declspec(align(INCBIN_ALIGNMENT))
#endif

#if defined(__arm__) || /* GNU C and RealView */ \
    defined(__arm) || /* Diab */ \
    defined(_ARM) /* ImageCraft */
#  define INCBIN_ARM
#endif

#ifdef __GNUC__
/* Utilize .balign where supported */
#  define INCBIN_ALIGN_HOST ".balign " INCBIN_STRINGIZE(INCBIN_ALIGNMENT) "\n"
#  define INCBIN_ALIGN_BYTE ".balign 1\n"
#elif defined(INCBIN_ARM)
/*
 * On arm assemblers, the alignment value is calculated as (1 << n) where `n' is
 * the shift count. This is the value passed to `.align'
 */
#  define INCBIN_ALIGN_HOST ".align " INCBIN_STRINGIZE(INCBIN_ALIGNMENT_INDEX) "\n"
#  define INCBIN_ALIGN_BYTE ".align 0\n"
#else
/* We assume other inline assembler's treat `.align' as `.balign' */
#  define INCBIN_ALIGN_HOST ".align " INCBIN_STRINGIZE(INCBIN_ALIGNMENT) "\n"
#  define INCBIN_ALIGN_BYTE ".align 1\n"
#endif

/* INCBIN_CONST is used by incbin.c generated files */
#if defined(__cplusplus)
#  define INCBIN_EXTERNAL extern "C"
#  define INCBIN_CONST    extern const
#else
#  define INCBIN_EXTERNAL extern
#  define INCBIN_CONST    const
#endif

/**
 * @brief Optionally override the linker section into which size and data is
 * emitted.
 * 
 * @warning If you use this facility, you might have to deal with
 * platform-specific linker output section naming on your own.
 */
#if !defined(INCBIN_OUTPUT_SECTION)
#  if defined(__APPLE__)
#    define INCBIN_OUTPUT_SECTION ".const_data"
#  else
#    define INCBIN_OUTPUT_SECTION ".rodata"
#  endif
#endif

/**
 * @brief Optionally override the linker section into which data is emitted.
 *
 * @warning If you use this facility, you might have to deal with
 * platform-specific linker output section naming on your own.
 */
#if !defined(INCBIN_OUTPUT_DATA_SECTION)
#  define INCBIN_OUTPUT_DATA_SECTION INCBIN_OUTPUT_SECTION
#endif

/**
 * @brief Optionally override the linker section into which size is emitted.
 *
 * @warning If you use this facility, you might have to deal with
 * platform-specific linker output section naming on your own.
 * 
 * @note This is useful for Harvard architectures where program memory cannot
 * be directly read from the program without special instructions. With this you
 * can chose to put the size variable in RAM rather than ROM.
 */
#if !defined(INCBIN_OUTPUT_SIZE_SECTION)
#  define INCBIN_OUTPUT_SIZE_SECTION INCBIN_OUTPUT_SECTION
#endif

#if defined(__APPLE__)
#  include "TargetConditionals.h"
#  if defined(TARGET_OS_IPHONE) && !defined(INCBIN_SILENCE_BITCODE_WARNING)
#    warning "incbin is incompatible with bitcode. Using the library will break upload to App Store if you have bitcode enabled. Add `#define INCBIN_SILENCE_BITCODE_WARNING` before including this header to silence this warning."
#  endif
/* The directives are different for Apple branded compilers */
#  define INCBIN_SECTION         INCBIN_OUTPUT_SECTION "\n"
#  define INCBIN_GLOBAL(NAME)    ".globl " INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME "\n"
#  define INCBIN_INT             ".long "
#  define INCBIN_MANGLE          "_"
#  define INCBIN_BYTE            ".byte "
#  define INCBIN_TYPE(...)
#else
#  define INCBIN_SECTION         ".section " INCBIN_OUTPUT_SECTION "\n"
#  define INCBIN_GLOBAL(NAME)    ".global " INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME "\n"
#  if defined(__ghs__)
#    define INCBIN_INT           ".word "
#  else
#    define INCBIN_INT           ".int "
#  endif
#  if defined(__USER_LABEL_PREFIX__)
#    define INCBIN_MANGLE        INCBIN_STRINGIZE(__USER_LABEL_PREFIX__)
#  else
#    define INCBIN_MANGLE        ""
#  endif
#  if defined(INCBIN_ARM)
/* On arm assemblers, `@' is used as a line comment token */
#    define INCBIN_TYPE(NAME)    ".type " INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME ", %object\n"
#  elif defined(__MINGW32__) || defined(__MINGW64__)
/* Mingw doesn't support this directive either */
#    define INCBIN_TYPE(NAME)
#  else
/* It's safe to use `@' on other architectures */
#    define INCBIN_TYPE(NAME)    ".type " INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME ", @object\n"
#  endif
#  define INCBIN_BYTE            ".byte "
#endif

/* List of style types used for symbol names */
#define INCBIN_STYLE_CAMEL 0
#define INCBIN_STYLE_SNAKE 1

/**
 * @brief Specify the prefix to use for symbol names.
 *
 * @note By default this is "g".
 *
 * @code
 * #define INCBIN_PREFIX incbin
 * #include "incbin.h"
 * INCBIN(Foo, "foo.txt");
 *
 * // Now you have the following symbols instead:
 * // const unsigned char incbinFoo<data>[];
 * // const unsigned char *const incbinFoo<end>;
 * // const unsigned int incbinFoo<size>;
 * @endcode
 */
#if !defined(INCBIN_PREFIX)
#  define INCBIN_PREFIX g
#endif

/**
 * @brief Specify the style used for symbol names.
 *
 * Possible options are
 * - INCBIN_STYLE_CAMEL "CamelCase"
 * - INCBIN_STYLE_SNAKE "snake_case"
 *
 * @note By default this is INCBIN_STYLE_CAMEL
 *
 * @code
 * #define INCBIN_STYLE INCBIN_STYLE_SNAKE
 * #include "incbin.h"
 * INCBIN(foo, "foo.txt");
 *
 * // Now you have the following symbols:
 * // const unsigned char <prefix>foo_data[];
 * // const unsigned char *const <prefix>foo_end;
 * // const unsigned int <prefix>foo_size;
 * @endcode
 */
#if !defined(INCBIN_STYLE)
#  define INCBIN_STYLE INCBIN_STYLE_CAMEL
#endif

/* Style lookup tables */
#define INCBIN_STYLE_0_DATA Data
#define INCBIN_STYLE_0_END End
#define INCBIN_STYLE_0_SIZE Size
#define INCBIN_STYLE_1_DATA _data
#define INCBIN_STYLE_1_END _end
#define INCBIN_STYLE_1_SIZE _size

/* Style lookup: returning identifier */
#define INCBIN_STYLE_IDENT(TYPE) \
    INCBIN_CONCATENATE( \
        INCBIN_STYLE_, \
        INCBIN_CONCATENATE( \
            INCBIN_EVAL(INCBIN_STYLE), \
            INCBIN_CONCATENATE(_, TYPE)))

/* Style lookup: returning string literal */
#define INCBIN_STYLE_STRING(TYPE) \
    INCBIN_STRINGIZE( \
        INCBIN_STYLE_IDENT(TYPE)) \

/* Generate the global labels by indirectly invoking the macro with our style
 * type and concatenating the name against them. */
#define INCBIN_GLOBAL_LABELS(NAME, TYPE) \
    INCBIN_INVOKE( \
        INCBIN_GLOBAL, \
        INCBIN_CONCATENATE( \
            NAME, \
            INCBIN_INVOKE( \
                INCBIN_STYLE_IDENT, \
                TYPE))) \
    INCBIN_INVOKE( \
        INCBIN_TYPE, \
        INCBIN_CONCATENATE( \
            NAME, \
            INCBIN_INVOKE( \
                INCBIN_STYLE_IDENT, \
                TYPE)))

/**
 * @brief Externally reference binary data included in another translation unit.
 *
 * Produces three external symbols that reference the binary data included in
 * another translation unit.
 *
 * The symbol names are a concatenation of `INCBIN_PREFIX' before *NAME*; with
 * "Data", as well as "End" and "Size" after. An example is provided below.
 *
 * @param TYPE Optional array type. Omitting this picks a default of `unsigned char`.
 * @param NAME The name given for the binary data
 *
 * @code
 * INCBIN_EXTERN(Foo);
 *
 * // Now you have the following symbols:
 * // extern const unsigned char <prefix>Foo<data>[];
 * // extern const unsigned char *const <prefix>Foo<end>;
 * // extern const unsigned int <prefix>Foo<size>;
 * @endcode
 * 
 * You may specify a custom optional data type as well as the first argument.
 * @code
 * INCBIN_EXTERN(custom_type, Foo);
 * 
 * // Now you have the following symbols:
 * // extern const custom_type <prefix>Foo<data>[];
 * // extern const custom_type *const <prefix>Foo<end>;
 * // extern const unsigned int <prefix>Foo<size>;
 * @endcode
 */
#define INCBIN_EXTERN(...) \
    INCBIN_CONCATENATE(INCBIN_EXTERN_, INCBIN_VA_ARGC(__VA_ARGS__))(__VA_ARGS__)
#define INCBIN_EXTERN_1(NAME, ...) \
    INCBIN_EXTERN_2(unsigned char, NAME)
#define INCBIN_EXTERN_2(TYPE, NAME) \
    INCBIN_EXTERNAL const INCBIN_ALIGN TYPE \
        INCBIN_CONCATENATE( \
            INCBIN_CONCATENATE(INCBIN_PREFIX, NAME), \
            INCBIN_STYLE_IDENT(DATA))[]; \
    INCBIN_EXTERNAL const INCBIN_ALIGN TYPE *const \
    INCBIN_CONCATENATE( \
        INCBIN_CONCATENATE(INCBIN_PREFIX, NAME), \
        INCBIN_STYLE_IDENT(END)); \
    INCBIN_EXTERNAL const unsigned int \
        INCBIN_CONCATENATE( \
            INCBIN_CONCATENATE(INCBIN_PREFIX, NAME), \
            INCBIN_STYLE_IDENT(SIZE))

/**
 * @brief Externally reference textual data included in another translation unit.
 *
 * Produces three external symbols that reference the textual data included in
 * another translation unit.
 *
 * The symbol names are a concatenation of `INCBIN_PREFIX' before *NAME*; with
 * "Data", as well as "End" and "Size" after. An example is provided below.
 *
 * @param NAME The name given for the textual data
 *
 * @code
 * INCBIN_EXTERN(Foo);
 *
 * // Now you have the following symbols:
 * // extern const char <prefix>Foo<data>[];
 * // extern const char *const <prefix>Foo<end>;
 * // extern const unsigned int <prefix>Foo<size>;
 * @endcode
 */
#define INCTXT_EXTERN(NAME) \
    INCBIN_EXTERN_2(char, NAME)

/**
 * @brief Include a binary file into the current translation unit.
 *
 * Includes a binary file into the current translation unit, producing three symbols
 * for objects that encode the data and size respectively.
 *
 * The symbol names are a concatenation of `INCBIN_PREFIX' before *NAME*; with
 * "Data", as well as "End" and "Size" after. An example is provided below.
 *
 * @param TYPE Optional array type. Omitting this picks a default of `unsigned char`.
 * @param NAME The name to associate with this binary data (as an identifier.)
 * @param FILENAME The file to include (as a string literal.)
 *
 * @code
 * INCBIN(Icon, "icon.png");
 *
 * // Now you have the following symbols:
 * // const unsigned char <prefix>Icon<data>[];
 * // const unsigned char *const <prefix>Icon<end>;
 * // const unsigned int <prefix>Icon<size>;
 * @endcode
 * 
 * You may specify a custom optional data type as well as the first argument.
 * These macros are specialized by arity.
 * @code
 * INCBIN(custom_type, Icon, "icon.png");
 *
 * // Now you have the following symbols:
 * // const custom_type <prefix>Icon<data>[];
 * // const custom_type *const <prefix>Icon<end>;
 * // const unsigned int <prefix>Icon<size>;
 * @endcode
 *
 * @warning This must be used in global scope
 * @warning The identifiers may be different if INCBIN_STYLE is not default
 *
 * To externally reference the data included by this in another translation unit
 * please @see INCBIN_EXTERN.
 */
#ifdef _MSC_VER
#  define INCBIN(NAME, FILENAME) \
      INCBIN_EXTERN(NAME)
#else
#  define INCBIN(...) \
     INCBIN_CONCATENATE(INCBIN_, INCBIN_VA_ARGC(__VA_ARGS__))(__VA_ARGS__)
#  if defined(__GNUC__)
#    define INCBIN_1(...) _Pragma("GCC error \"Single argument INCBIN not allowed\"")
#  elif defined(__clang__)
#    define INCBIN_1(...) _Pragma("clang error \"Single argument INCBIN not allowed\"")
#  else
#    define INCBIN_1(...) /* Cannot do anything here */
#  endif
#  define INCBIN_2(NAME, FILENAME) \
      INCBIN_3(unsigned char, NAME, FILENAME)
#  define INCBIN_3(TYPE, NAME, FILENAME) INCBIN_COMMON(TYPE, NAME, FILENAME, /* No terminator for binary data */)
#  define INCBIN_COMMON(TYPE, NAME, FILENAME, TERMINATOR) \
    __asm__(INCBIN_SECTION \
            INCBIN_GLOBAL_LABELS(NAME, DATA) \
            INCBIN_ALIGN_HOST \
            INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(DATA) ":\n" \
            INCBIN_MACRO " \"" FILENAME "\"\n" \
                TERMINATOR \
            INCBIN_GLOBAL_LABELS(NAME, END) \
            INCBIN_ALIGN_BYTE \
            INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(END) ":\n" \
                INCBIN_BYTE "1\n" \
            INCBIN_GLOBAL_LABELS(NAME, SIZE) \
            INCBIN_ALIGN_HOST \
            INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(SIZE) ":\n" \
                INCBIN_INT INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(END) " - " \
                           INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(DATA) "\n" \
            INCBIN_ALIGN_HOST \
            ".text\n" \
    ); \
    INCBIN_EXTERN(TYPE, NAME)
#endif

/**
 * @brief Include a textual file into the current translation unit.
 * 
 * This behaves the same as INCBIN except it produces char compatible arrays
 * and implicitly adds a null-terminator byte, thus the size of data included
 * by this is one byte larger than that of INCBIN.
 *
 * Includes a textual file into the current translation unit, producing three
 * symbols for objects that encode the data and size respectively.
 *
 * The symbol names are a concatenation of `INCBIN_PREFIX' before *NAME*; with
 * "Data", as well as "End" and "Size" after. An example is provided below.
 *
 * @param NAME The name to associate with this binary data (as an identifier.)
 * @param FILENAME The file to include (as a string literal.)
 *
 * @code
 * INCTXT(Readme, "readme.txt");
 *
 * // Now you have the following symbols:
 * // const char <prefix>Readme<data>[];
 * // const char *const <prefix>Readme<end>;
 * // const unsigned int <prefix>Readme<size>;
 * @endcode
 *
 * @warning This must be used in global scope
 * @warning The identifiers may be different if INCBIN_STYLE is not default
 *
 * To externally reference the data included by this in another translation unit
 * please @see INCBIN_EXTERN.
 */
#if defined(_MSC_VER)
#  define INCTXT(NAME, FILENAME) \
     INCBIN_EXTERN(NAME)
#else
#  define INCTXT(NAME, FILENAME) \
     INCBIN_COMMON(char, NAME, FILENAME, INCBIN_BYTE "0\n")
#endif

#endif

// Macro to embed the default efficiently updatable neural network (NNUE) file
// data in the engine binary (using incbin.h, by Dale Weiler).
// This macro invocation will declare the following three variables
//     const unsigned char        gEmbeddedNNUEData[];  // a pointer to the embedded data
//     const unsigned char *const gEmbeddedNNUEEnd;     // a marker to the end
//     const unsigned int         gEmbeddedNNUESize;    // the size of the embedded file
// Note that this does not work in Microsoft Visual Studio.
#if !defined(UNIVERSAL_BINARY) && !defined(_MSC_VER) && !defined(NNUE_EMBEDDING_OFF)
INCBIN(EmbeddedNNUE, EvalFileDefaultName);
#elif defined(UNIVERSAL_BINARY_MACOS_X86_SLICE)
// Determined at runtime, see universal/nnue_embed.cpp
extern const unsigned char* const gEmbeddedNNUEData;
extern const unsigned int         gEmbeddedNNUESize;
#elif defined(UNIVERSAL_BINARY)
extern const unsigned char gEmbeddedNNUEData[];
extern const unsigned int  gEmbeddedNNUESize;
#else
const unsigned char gEmbeddedNNUEData[1] = {0x0};
const unsigned int  gEmbeddedNNUESize    = 1;
#endif


namespace Stockfish::Eval::NNUE {

namespace fs = std::filesystem;

namespace Detail {

// Read evaluation function parameters
template<typename T>
bool read_parameters(std::istream& stream, T& reference) {

    u32 header;
    header = read_little_endian<u32>(stream);
    if (!stream || header != T::get_hash_value())
        return false;
    return reference.read_parameters(stream);
}

// Write evaluation function parameters
template<typename T>
bool write_parameters(std::ostream& stream, const T& reference) {

    write_little_endian<u32>(stream, T::get_hash_value());
    return reference.write_parameters(stream);
}

}  // namespace Detail

void Network::load(const fs::path& rootDirectory, fs::path evalfilePath, EvalFile& evalFile) {
#if defined(DEFAULT_NNUE_DIRECTORY)
    std::vector<fs::path> dirs = {fs::path{}, rootDirectory,
                                  fs::path(stringify(DEFAULT_NNUE_DIRECTORY))};
#else
    std::vector<fs::path> dirs = {fs::path{}, rootDirectory};
#endif

    if (evalfilePath.empty())
        evalfilePath = evalFile.defaultName;

    if (evalFile.current != evalfilePath && evalfilePath == evalFile.defaultName)
        load_internal(evalFile);

    for (const auto& directory : dirs)
        if (evalFile.current != evalfilePath)
            load_external(directory, evalfilePath, evalFile);
}

bool Network::save(const EvalFile& evalFile, const std::optional<fs::path>& filename) const {
    if (!evalFile.current.has_value())
    {
        sync_cout << "Failed to export a net. No network file is currently loaded. "
                     "Please load a network file first."
                  << sync_endl;
        return false;
    }

    if (!filename.has_value() && evalFile.current != evalFile.defaultName)
    {
        sync_cout << "Failed to export a net. A non-embedded net can only be "
                     "saved if the filename is specified"
                  << sync_endl;
        return false;
    }

    fs::path      actualFilename = filename.value_or(evalFile.defaultName);
    std::ofstream stream(actualFilename, std::ios_base::binary);

    bool saved = save(stream, evalFile.netDescription);

    sync_cout << (saved ? "Network saved successfully to " + actualFilename.string()
                        : "Failed to export a net")
              << sync_endl;

    return saved;
}

int Network::layer_stack(const Position& pos) const {
    const Color stm    = pos.side_to_move();
    const int   band   = (pos.count<ALL_PIECES>() - 1) / 4;
    const int   queens = 2 * bool(pos.pieces(stm, QUEEN)) + bool(pos.pieces(~stm, QUEEN));

    // Pawn layout: 0 if pawns stand on both board halves, 1 if on one half only, 2 if there are no pawns
    constexpr Bitboard QueenSide = FileABB | FileBBB | FileCBB | FileDBB;
    const Bitboard     pawns     = pos.pieces(PAWN);
    const int          layout    = !pawns ? 2 : (pawns & QueenSide) && (pawns & ~QueenSide) ? 0 : 1;

    // Officers are the pieces other than pawns and kings
    const auto officers = [&](Color c) {
        return popcount(pos.pieces(c) & ~pos.pieces(PAWN, KING));
    };
    const bool many = std::max(officers(WHITE), officers(BLACK)) >= 2;

    return layerStackIndex[((band * 4 + queens) * 3 + layout) * 2 + many];
}

Value Network::evaluate(const Position&    pos,
                        AccumulatorStack&  accumulatorStack,
                        AccumulatorCaches& cache) const {

    constexpr u64 alignment = CacheLineSize;

    alignas(alignment) TransformedFeatureType transformedFeatures[FeatureTransformer::BufferSize];

    ASSERT_ALIGNED(transformedFeatures, alignment);

    NNZInfo<L1> nnzInfo;

    const int bucket = layer_stack(pos);

    featureTransformer.transform(pos, accumulatorStack, cache, transformedFeatures, nnzInfo);
    const auto positional = network[bucket].propagate(transformedFeatures, nnzInfo);
    return static_cast<Value>(positional / OutputScale);
}


void Network::verify(const std::function<void(std::string_view)>& f,
                     const EvalFile&                              evalFile,
                     fs::path                                     evalfilePath) const {
    if (evalfilePath.empty())
        evalfilePath = evalFile.defaultName;

    if (evalFile.current != evalfilePath)
    {
        if (f)
        {
            std::string msg1 =
              "Network evaluation parameters compatible with the engine must be available.";
            std::string msg2 =
              "The network file " + evalfilePath.string() + " was not loaded successfully.";
            std::string msg3 = "The UCI option EvalFile might need to specify the full path, "
                               "including the directory name, to the network file.";
            std::string msg4 = "The default net can be downloaded from: "
                               "https://tests.stockfishchess.org/api/nn/"
                             + std::string(evalFile.defaultName);
            std::string msg5 = "The engine will be terminated now.";

            std::string msg = "ERROR: " + msg1 + '\n' + "ERROR: " + msg2 + '\n' + "ERROR: " + msg3
                            + '\n' + "ERROR: " + msg4 + '\n' + "ERROR: " + msg5 + '\n';

            f(msg);
        }

        exit(EXIT_FAILURE);
    }

    if (f)
    {
        usize size = sizeof(featureTransformer) + sizeof(NetworkArchitecture) * storedLayerStacks;
        f("NNUE evaluation using " + evalfilePath.string() + " ("
          + std::to_string(size / (1024 * 1024)) + "MiB, ("
          + std::to_string(featureTransformer.InputDimensions) + ", "
          + std::to_string(network[0].TransformedFeatureDimensions) + ", "
          + std::to_string(network[0].FC_0_OUTPUTS) + ", " + std::to_string(network[0].FC_1_OUTPUTS)
          + ", 1))");
    }
}


NnueEvalTrace Network::trace_evaluate(const Position&    pos,
                                      AccumulatorStack&  accumulatorStack,
                                      AccumulatorCaches& cache) const {

    constexpr u64 alignment = CacheLineSize;

    alignas(alignment) TransformedFeatureType transformedFeatures[FeatureTransformer::BufferSize];

    ASSERT_ALIGNED(transformedFeatures, alignment);

    const int bucket = layer_stack(pos);

    NnueEvalTrace t{};
    t.correctBucket = bucket;
    t.layerStacks   = storedLayerStacks;

    NNZInfo<L1> nnzInfo;
    featureTransformer.transform(pos, accumulatorStack, cache, transformedFeatures, nnzInfo);

    for (IndexType b = 0; b < storedLayerStacks; ++b)
    {
        const auto positional = network[b].propagate(transformedFeatures, nnzInfo);
        t.positional[b]       = static_cast<Value>(positional / OutputScale);
    }

    return t;
}


void Network::load_external(const fs::path& dir, const fs::path& evalfilePath, EvalFile& evalFile) {
    std::ifstream stream(dir / evalfilePath, std::ios::binary);
    auto          description = load(stream);

    if (description.has_value())
    {
        evalFile.current        = evalfilePath;
        evalFile.netDescription = description.value();
    }
}


void Network::load_internal(EvalFile& evalFile) {
    // C++ way to prepare a buffer for a memory stream
    class MemoryBuffer: public std::basic_streambuf<char> {
       public:
        MemoryBuffer(char* p, usize n) {
            setg(p, p, p + n);
            setp(p, p + n);
        }
    };

#ifdef UNIVERSAL_BINARY_MACOS_X86_SLICE
    if (gEmbeddedNNUEData == nullptr)  // failed embedded load
        return;
#endif

    MemoryBuffer buffer(const_cast<char*>(reinterpret_cast<const char*>(gEmbeddedNNUEData)),
                        usize(gEmbeddedNNUESize));

    std::istream stream(&buffer);
    auto         description = load(stream);

    if (description.has_value())
    {
        evalFile.current        = evalFile.defaultName;
        evalFile.netDescription = description.value();
    }
}


void Network::initialize() { initialized = true; }


bool Network::save(std::ostream& stream, const std::string& netDescription) const {
    return write_parameters(stream, netDescription);
}


std::optional<std::string> Network::load(std::istream& stream) {
    initialize();
    std::string description;

    return read_parameters(stream, description) ? std::make_optional(description) : std::nullopt;
}


usize Network::get_content_hash() const {
    if (!initialized)
        return 0;

    usize h = 0;
    hash_combine(h, featureTransformer);
    for (const u16 f : layerStackFallback)
        hash_combine(h, f);
    for (usize i = 0; i < storedLayerStacks; ++i)
        hash_combine(h, network[i]);
    return h;
}

// Read network header
bool Network::read_header(std::istream& stream, u32* hashValue, std::string* desc) const {
    u32 version, size;

    version    = read_little_endian<u32>(stream);
    *hashValue = read_little_endian<u32>(stream);
    size       = read_little_endian<u32>(stream);
    if (!stream || version != Version)
        return false;

    constexpr u32 Chunk = 4096;
    char          buf[Chunk];

    desc->clear();
    for (u32 remaining = size; remaining > 0;)
    {
        const u32 want = std::min(remaining, Chunk);
        stream.read(buf, want);
        const u32 got = u32(stream.gcount());
        desc->append(buf, got);
        if (got != want)
            return false;
        remaining -= want;
    }
    return !stream.fail();
}


// Write network header
bool Network::write_header(std::ostream& stream, u32 hashValue, const std::string& desc) const {
    write_little_endian<u32>(stream, Version);
    write_little_endian<u32>(stream, hashValue);
    write_little_endian<u32>(stream, u32(desc.size()));
    stream.write(&desc[0], desc.size());
    return !stream.fail();
}


bool Network::read_parameters(std::istream& stream, std::string& netDescription) {
    u32 hashValue;
    if (!read_header(stream, &hashValue, &netDescription))
        return false;
    if (hashValue != Network::hash)
        return false;
    if (read_little_endian<u32>(stream) != LayerStackTableHash
        || read_little_endian<u32>(stream) != RawLayerStacks)
        return false;
    storedLayerStacks = read_little_endian<u32>(stream);
    if (storedLayerStacks == 0 || storedLayerStacks > LayerStacks)
        return false;
    for (u16& f : layerStackFallback)
        if ((f = read_little_endian<u16>(stream)) >= RawLayerStacks)
            return false;
    IndexType stored = 0;
    for (IndexType s = 0; s < RawLayerStacks; ++s)
        if (layerStackFallback[s] == s)
            layerStackIndex[s] = u16(stored++);
    if (stored != storedLayerStacks)
        return false;
    for (IndexType s = 0; s < RawLayerStacks; ++s)
    {
        const u16 f = layerStackFallback[s];
        if (layerStackFallback[f] != f)
            return false;
        layerStackIndex[s] = layerStackIndex[f];
    }
    if (!Detail::read_parameters(stream, featureTransformer))
        return false;
    for (usize i = 0; i < storedLayerStacks; ++i)
    {
        if (!Detail::read_parameters(stream, network[i]))
            return false;
    }
    return stream && stream.peek() == std::ios::traits_type::eof();
}


bool Network::write_parameters(std::ostream& stream, const std::string& netDescription) const {
    if (!write_header(stream, Network::hash, netDescription))
        return false;
    write_little_endian<u32>(stream, LayerStackTableHash);
    write_little_endian<u32>(stream, RawLayerStacks);
    write_little_endian<u32>(stream, storedLayerStacks);
    for (const u16 f : layerStackFallback)
        write_little_endian<u16>(stream, f);
    if (!Detail::write_parameters(stream, featureTransformer))
        return false;
    for (usize i = 0; i < storedLayerStacks; ++i)
    {
        if (!Detail::write_parameters(stream, network[i]))
            return false;
    }
    return bool(stream);
}

}  // namespace Stockfish::Eval::NNUE

// === sf_src/src/nnue/nnue_accumulator.cpp ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <algorithm>
#include <array>
#include <cassert>
#include <new>
#include <type_traits>  // IWYU pragma: keep


namespace Stockfish::Eval::NNUE {

using namespace SIMD;

namespace {

template<bool Forward>
void update_accumulator_incremental(Color                     perspective,
                                    const FeatureTransformer& featureTransformer,
                                    const Square              ksq,
                                    int                       psqBlock,
                                    AccumulatorState&         target_state,
                                    const AccumulatorState&   computed);

void update_accumulator_incremental_both(const FeatureTransformer& featureTransformer,
                                         Square                    white_ksq,
                                         Square                    black_ksq,
                                         int                       whiteBlock,
                                         int                       blackBlock,
                                         AccumulatorState&         target_state,
                                         const AccumulatorState&   computed);

void update_accumulator_refresh_cache(Color                     perspective,
                                      const FeatureTransformer& featureTransformer,
                                      const Position&           pos,
                                      AccumulatorState&         accumulatorState,
                                      AccumulatorCaches&        cache);

void update_accumulator_hybrid(Color                     perspective,
                               const Position&           pos,
                               const FeatureTransformer& featureTransformer,
                               AccumulatorState&         target,
                               const AccumulatorState&   computed,
                               AccumulatorCaches&        cache);

}

const AccumulatorState& AccumulatorStack::latest() const noexcept { return accumulators[size - 1]; }

AccumulatorState& AccumulatorStack::mut_latest() noexcept { return accumulators[size - 1]; }

void AccumulatorStack::reset() noexcept {
    accumulators[0].dirtyPiece = {};
    new (&accumulators[0].dirtyThreats) DirtyThreats;
    new (&accumulators[0].dirtyPawnPairs) DirtyPawnPairs;
    accumulators[0].computed.fill(false);
    size = 1;
}

Dirties& AccumulatorStack::push(const Position& pos) noexcept {
    assert(size < MaxSize);
    auto& st = accumulators[size];
    st.computed.fill(false);
    st.blockBefore = PSQFeatureSet::block_state(pos);
    new (&st.dirtyThreats) DirtyThreats;
    new (&st.dirtyPawnPairs) DirtyPawnPairs;
    size++;
    return st;
}

void AccumulatorStack::pop() noexcept {
    assert(size > 1);
    size--;
}

void AccumulatorStack::evaluate(const Position&           pos,
                                const FeatureTransformer& featureTransformer,
                                // Silence spurious warning on GCC 10
                                [[maybe_unused]] AccumulatorCaches& cache) noexcept {
    const usize last_white = find_last_usable_accumulator(WHITE, pos);
    const usize last_black = find_last_usable_accumulator(BLACK, pos);

    if (accumulators[last_white].computed[WHITE] && accumulators[last_black].computed[BLACK])
        forward_update_incremental_both(pos, featureTransformer, last_white, last_black);
    else
    {
        evaluate_side(WHITE, pos, featureTransformer, cache, last_white);
        evaluate_side(BLACK, pos, featureTransformer, cache, last_black);
    }
}

void AccumulatorStack::evaluate_side(Color                     perspective,
                                     const Position&           pos,
                                     const FeatureTransformer& featureTransformer,
                                     AccumulatorCaches&        cache,
                                     usize                     last_usable_accum) noexcept {

    constexpr int MIN_PC_COUNT_HYBRID = 15;

    if (accumulators[last_usable_accum].computed[perspective])
        forward_update_incremental(perspective, pos, featureTransformer, last_usable_accum);

    else
    {
        const auto& dirtyPiece = latest().dirtyPiece;

        if (dirtyPiece.pc == make_piece(perspective, KING)
            && latest().blockBefore[perspective] == PSQFeatureSet::block(pos, perspective)
            && accumulators[size - 2].computed[perspective]
            && pos.count<ALL_PIECES>() >= MIN_PC_COUNT_HYBRID
            && ((int(dirtyPiece.from) & 0b100) == (int(dirtyPiece.to) & 0b100))
            && dirtyPiece.add_sq == SQ_NONE)  // excludes castling
        {
            update_accumulator_hybrid(perspective, pos, featureTransformer, mut_latest(),
                                      accumulators[size - 2], cache);
            return;
        }

        update_accumulator_refresh_cache(perspective, featureTransformer, pos, mut_latest(), cache);
        backward_update_incremental(perspective, pos, featureTransformer, last_usable_accum);
    }
}

// Find the earliest usable accumulator, this can either be a computed accumulator or the accumulator
// state just after a change that requires full refresh.
usize AccumulatorStack::find_last_usable_accumulator(Color           perspective,
                                                     const Position& pos) const noexcept {

    // All traversed states must use the current block.
    const int psqBlock = PSQFeatureSet::block(pos, perspective);

    for (usize curr_idx = size - 1; curr_idx > 0; curr_idx--)
    {
        if (accumulators[curr_idx].computed[perspective])
            return curr_idx;

        const auto& dirtyPiece = accumulators[curr_idx].dirtyPiece;
        const int   before     = accumulators[curr_idx].blockBefore[perspective];

        // Threat feature set refreshes require a king move across the center, i.e.,
        // a subset of halfka refreshes
        if (PSQFeatureSet::requires_refresh(dirtyPiece, perspective, before, psqBlock))
            return curr_idx;
    }

    return 0;
}

void AccumulatorStack::forward_update_incremental(Color                     perspective,
                                                  const Position&           pos,
                                                  const FeatureTransformer& featureTransformer,
                                                  const usize               begin) noexcept {

    assert(begin < accumulators.size());
    assert(accumulators[begin].computed[perspective]);

    const Square ksq      = pos.square<KING>(perspective);
    const int    psqBlock = PSQFeatureSet::block(pos, perspective);

    for (usize next = begin + 1; next < size; next++)
        update_accumulator_incremental<true>(perspective, featureTransformer, ksq, psqBlock,
                                             accumulators[next], accumulators[next - 1]);

    assert(latest().computed[perspective]);
}

void AccumulatorStack::backward_update_incremental(Color                     perspective,
                                                   const Position&           pos,
                                                   const FeatureTransformer& featureTransformer,
                                                   const usize               end) noexcept {

    assert(end < accumulators.size());
    assert(end < size);
    assert(latest().computed[perspective]);

    const Square ksq      = pos.square<KING>(perspective);
    const int    psqBlock = PSQFeatureSet::block(pos, perspective);

    for (i64 next = i64(size) - 2; next >= i64(end); next--)
        update_accumulator_incremental<false>(perspective, featureTransformer, ksq, psqBlock,
                                              accumulators[next], accumulators[next + 1]);

    assert(accumulators[end].computed[perspective]);
}

void AccumulatorStack::forward_update_incremental_both(const Position&           pos,
                                                       const FeatureTransformer& featureTransformer,
                                                       usize                     white_begin,
                                                       usize black_begin) noexcept {

    assert(white_begin < size);
    assert(black_begin < size);
    assert(accumulators[white_begin].computed[WHITE]);
    assert(accumulators[black_begin].computed[BLACK]);

    const Square white_ksq    = pos.square<KING>(WHITE);
    const Square black_ksq    = pos.square<KING>(BLACK);
    const int    whiteBlock   = PSQFeatureSet::block(pos, WHITE);
    const int    blackBlock   = PSQFeatureSet::block(pos, BLACK);
    const usize  shared_begin = std::max(white_begin, black_begin);

    // Catch up the lagging perspective, then traverse the common suffix once.
    for (usize next = white_begin + 1; next <= shared_begin; ++next)
        update_accumulator_incremental<true>(WHITE, featureTransformer, white_ksq, whiteBlock,
                                             accumulators[next], accumulators[next - 1]);
    for (usize next = black_begin + 1; next <= shared_begin; ++next)
        update_accumulator_incremental<true>(BLACK, featureTransformer, black_ksq, blackBlock,
                                             accumulators[next], accumulators[next - 1]);

    for (usize next = shared_begin + 1; next < size; ++next)
        update_accumulator_incremental_both(featureTransformer, white_ksq, black_ksq, whiteBlock,
                                            blackBlock, accumulators[next], accumulators[next - 1]);

    assert(latest().computed[WHITE]);
    assert(latest().computed[BLACK]);
}

namespace {

constexpr IndexType Dimensions = FeatureTransformer::OutputDimensions;

#ifdef USE_RVV

struct Tiling {
    static constexpr int NumRegs = 1;
};

using Tile = vint16m8_t;

sf_always_inline Tile load_tile(IndexType j, const i16* data) {
    usize vl = __riscv_vsetvl_e16m8(Dimensions - j);
    return __riscv_vle16_v_i16m8(data + j, vl);
}

sf_always_inline void store_tile(IndexType j, i16* dest, Tile acc) {
    usize vl = __riscv_vsetvl_e16m8(Dimensions - j);
    __riscv_vse16_v_i16m8(dest + j, acc, vl);
}

sf_always_inline void increment_index(IndexType& j) { j += __riscv_vsetvl_e16m8(Dimensions - j); }

template<int sign>
sf_always_inline Tile apply(IndexType j, Tile acc, const i16* data) {
    static_assert(sign == +1 || sign == -1);
    usize      vl       = __riscv_vsetvl_e16m8(Dimensions - j);
    vint16m8_t data_vec = __riscv_vle16_v_i16m8(data, vl);
    if constexpr (sign == +1)
        acc = __riscv_vadd_vv_i16m8(acc, data_vec, vl);
    else
        acc = __riscv_vsub_vv_i16m8(acc, data_vec, vl);
    return acc;
}

template<int sign>
sf_always_inline Tile apply_feature_single(IndexType j, Tile acc, const i8* data) {
    static_assert(sign == +1 || sign == -1);
    usize     vl         = __riscv_vsetvl_e16m8(Dimensions - j);
    vint8m4_t weight_vec = __riscv_vle8_v_i8m4(data, vl);
    if constexpr (sign == +1)
        acc = __riscv_vwadd_wv_i16m8(acc, weight_vec, vl);
    else
        acc = __riscv_vwsub_wv_i16m8(acc, weight_vec, vl);
    return acc;
}

#else  // VECTOR or non VECTOR

    #ifdef VECTOR

using Tiling = SIMDTiling<Dimensions, Dimensions>;

    #else

// Treat scalar impl as degenerate size-1 vector
struct Tiling {
    static constexpr int NumRegs    = 1;
    static constexpr int TileHeight = 1;
};

using vec_t    = i16;
using vec_i8_t = i8;

        #define vec_add_16(a, b) ((a) + (b))
        #define vec_sub_16(a, b) ((a) - (b))
        #define vec_convert_8_16(a) (i16(a))

    #endif

struct Tile {
    vec_t inner[Tiling::NumRegs];
    auto& operator[](int i) { return inner[i]; }
};

sf_always_inline Tile load_tile(IndexType j, const i16* data) {
    Tile  acc;
    auto* column = reinterpret_cast<const vec_t*>(&data[j]);
    UNROLLED_FOR (IndexType k = 0; k < Tiling::NumRegs; ++k)
        acc[k] = column[k];
    return acc;
}

sf_always_inline void store_tile(IndexType j, i16* dest, Tile acc) {
    auto* column = reinterpret_cast<vec_t*>(&dest[j]);
    UNROLLED_FOR (IndexType k = 0; k < Tiling::NumRegs; ++k)
        column[k] = acc[k];
}

sf_always_inline void increment_index(IndexType& j) { j += Tiling::TileHeight; }

template<int sign>
sf_always_inline Tile apply([[maybe_unused]] IndexType j, Tile acc, const i16* data) {
    const auto* column = reinterpret_cast<const vec_t*>(data);
    UNROLLED_FOR (IndexType k = 0; k < Tiling::NumRegs; ++k)
        if constexpr (sign == +1)
            acc[k] = vec_add_16(acc[k], column[k]);
        else
            acc[k] = vec_sub_16(acc[k], column[k]);
    return acc;
}

// The tile loops below walk all changed weight rows together, one tile at a time,
// and the hardware prefetcher doesn't keep up with that many rows. Prefetch the
// next tile of a row while its current tile is being added. On a row's last tile
// this hits the next row, which is harmless since prefetches never fault. Rows of
// only two tiles (AVX-512) get no lead time from this and are faster without it,
// and so are AMD Zen 1/2, which only get builds without pext.
    #if defined(USE_PEXT)
sf_always_inline void prefetch_next_tile(const i8* data) {
    if constexpr (Dimensions / Tiling::TileHeight > 2)
        for (IndexType k = 0; k < Tiling::TileHeight; k += CacheLineSize)
            prefetch(data + Tiling::TileHeight + k);
}
    #else
[[maybe_unused]] void prefetch_next_tile(const i8*) {}
    #endif

template<int sign>
sf_always_inline Tile apply_feature_single([[maybe_unused]] IndexType j, Tile acc, const i8* data) {
    static_assert(sign == +1 || sign == -1);
    [[maybe_unused]] auto* column = reinterpret_cast<const vec_i8_t*>(data);
    #ifdef USE_NEON
    UNROLLED_FOR (IndexType k = 0; k < Tiling::NumRegs; k += 2)
    {
        if constexpr (sign == +1)
        {
            acc[k]     = vaddw_s8(acc[k], vget_low_s8(column[k / 2]));
            acc[k + 1] = vaddw_high_s8(acc[k + 1], column[k / 2]);
        }
        else
        {
            acc[k]     = vsubw_s8(acc[k], vget_low_s8(column[k / 2]));
            acc[k + 1] = vsubw_high_s8(acc[k + 1], column[k / 2]);
        }
    }
    #elif defined(USE_LSX) && !defined(USE_LASX)
    UNROLLED_FOR (IndexType k = 0; k < Tiling::NumRegs; k += 2)
    {
        if constexpr (sign == +1)
        {
            const __m128i weight = __lsx_vld(reinterpret_cast<const void*>(&column[k]), 0);
            acc[k]               = vec_add_16(acc[k], __lsx_vsllwil_h_b(weight, 0));
            acc[k + 1]           = vec_add_16(acc[k + 1], __lsx_vexth_h_b(weight));
        }
        else
        {
            const __m128i weight = __lsx_vld(reinterpret_cast<const void*>(&column[k]), 0);
            acc[k]               = vec_sub_16(acc[k], __lsx_vsllwil_h_b(weight, 0));
            acc[k + 1]           = vec_sub_16(acc[k + 1], __lsx_vexth_h_b(weight));
        }
    }
    #elif defined(USE_PAIRED_WIDENING_ADD)
    static_assert(Tiling::NumRegs % 2 == 0);
    prefetch_next_tile(data);
    const vec_t zero = vec_zero();
    UNROLLED_FOR (IndexType k = 0; k < Tiling::NumRegs; k += 2)
    {
        const vec_t weight = vec_load(reinterpret_cast<const vec_t*>(data) + k / 2);
        const vec_t lo     = vec_srai_16(vec_unpacklo_8(zero, weight), 8);
        const vec_t hi     = vec_srai_16(vec_unpackhi_8(zero, weight), 8);
        if constexpr (sign == +1)
        {
            acc[k]     = vec_add_16(acc[k], lo);
            acc[k + 1] = vec_add_16(acc[k + 1], hi);
        }
        else
        {
            acc[k]     = vec_sub_16(acc[k], lo);
            acc[k + 1] = vec_sub_16(acc[k + 1], hi);
        }
    }
    #else
    UNROLLED_FOR (IndexType k = 0; k < Tiling::NumRegs; ++k)
        if constexpr (sign == +1)
            acc[k] = vec_add_16(acc[k], vec_convert_8_16(column[k]));
        else
            acc[k] = vec_sub_16(acc[k], vec_convert_8_16(column[k]));
    #endif
    return acc;
}

    #if defined(USE_PAIRED_WIDENING_ADD)

template<int sign>
sf_always_inline Tile apply_feature_pair(Tile acc, const i8* data0, const i8* data1) {
    static_assert(sign == +1 || sign == -1);
    static_assert(Tiling::NumRegs % 2 == 0);
    prefetch_next_tile(data0);
    prefetch_next_tile(data1);
    const vec_t ones = vec_set_8(1);
    UNROLLED_FOR (IndexType k = 0; k < Tiling::NumRegs; k += 2)
    {
        const vec_t w0 = vec_load(reinterpret_cast<const vec_t*>(data0) + k / 2);
        const vec_t w1 = vec_load(reinterpret_cast<const vec_t*>(data1) + k / 2);
        const vec_t lo = vec_maddubs_16(ones, vec_unpacklo_8(w0, w1));
        const vec_t hi = vec_maddubs_16(ones, vec_unpackhi_8(w0, w1));
        if constexpr (sign == +1)
        {
            acc[k]     = vec_add_16(acc[k], lo);
            acc[k + 1] = vec_add_16(acc[k + 1], hi);
        }
        else
        {
            acc[k]     = vec_sub_16(acc[k], lo);
            acc[k + 1] = vec_sub_16(acc[k + 1], hi);
        }
    }
    return acc;
}

// acc += add - sub in one maddubs per half. Bias both rows to u8 so maddubs' unsigned
// operand is valid; the bias cancels and |add - sub| <= 255 can't saturate.
sf_always_inline Tile apply_feature_diff(Tile acc, const i8* add, const i8* sub) {
    static_assert(Tiling::NumRegs % 2 == 0);
    prefetch_next_tile(add);
    prefetch_next_tile(sub);
    const vec_t bias  = vec_set_8(-128);
    const vec_t signs = vec_set_16(-255);  // i8 pairs (+1, -1)
        #if defined(__clang__)
            #pragma clang loop unroll(full)
        #elif defined(__GNUC__)
            #pragma GCC unroll(Tiling::NumRegs / 2)
        #endif
    for (IndexType k = 0; k < Tiling::NumRegs; k += 2)
    {
        const vec_t ua = vec_xor(vec_load(reinterpret_cast<const vec_t*>(add) + k / 2), bias);
        const vec_t us = vec_xor(vec_load(reinterpret_cast<const vec_t*>(sub) + k / 2), bias);
        acc[k]         = vec_add_16(acc[k], vec_maddubs_16(vec_unpacklo_8(ua, us), signs));
        acc[k + 1]     = vec_add_16(acc[k + 1], vec_maddubs_16(vec_unpackhi_8(ua, us), signs));
    }
    return acc;
}

    #endif

#endif

// The weight-table base already includes the tile offset.
template<int sign>
sf_always_inline Tile
apply_features(IndexType j, Tile acc, const u16* indices, int count, const i8* weights) {
    static_assert(sign == +1 || sign == -1);
    int i = 0;
#if defined(USE_PAIRED_WIDENING_ADD)
    for (; i + 1 < count; i += 2)
        acc = apply_feature_pair<sign>(acc, &weights[usize(indices[i]) * Dimensions],
                                       &weights[usize(indices[i + 1]) * Dimensions]);
#endif
    for (; i < count; ++i)
        acc = apply_feature_single<sign>(j, acc, &weights[usize(indices[i]) * Dimensions]);
    return acc;
}

// Applies one sign's psq and threat/pp rows. With paired widening, an odd row is held back
// in `odd` so it can be combined with the other sign's odd row.
template<int sign, typename PsqRows>
sf_always_inline Tile apply_features_but_odd(IndexType                 j,
                                             Tile                      acc,
                                             const PsqRows&            psq,
                                             const u16*                thr,
                                             int                       thrCount,
                                             const FeatureTransformer& ft,
                                             const WeightType*         psqBase,
                                             const i8*&                odd) {
    const auto* thrWeights = &ft.threatAndPpWeights[j];
    const int   psqCount   = int(psq.size());
    const auto  psqRow     = [&](int i) {
        if constexpr (std::is_same_v<PsqRows, PSQFeatureSet::IndexList>)
            return &psqBase[usize(psq[i]) * Dimensions + j];
        else
            return psq[i] + j;
    };
    odd   = nullptr;
    int i = 0;
#if defined(USE_PAIRED_WIDENING_ADD)
    for (; i + 1 < psqCount; i += 2)
        acc = apply_feature_pair<sign>(acc, psqRow(i), psqRow(i + 1));

    // Pair the odd psq row with the first threat/pp row of the same sign, if any.
    if (i < psqCount)
    {
        if (thrCount == 0)
        {
            odd = psqRow(i);
            return acc;
        }
        acc = apply_feature_pair<sign>(acc, psqRow(i), &thrWeights[usize(thr[0]) * Dimensions]);
        ++thr;
        --thrCount;
    }

    if (thrCount % 2 != 0)
        odd = &thrWeights[usize(thr[--thrCount]) * Dimensions];
#else
    for (; i < psqCount; ++i)
        acc = apply_feature_single<sign>(j, acc, psqRow(i));
#endif
    return apply_features<sign>(j, acc, thr, thrCount, thrWeights);
}

sf_always_inline Tile apply_odd_features(IndexType j,
                                         Tile      acc,
                                         const i8* removed,
                                         const i8* added) {
#if defined(USE_PAIRED_WIDENING_ADD)
    if (removed && added)
        return apply_feature_diff(acc, added, removed);
#endif
    if (removed)
        acc = apply_feature_single<-1>(j, acc, removed);
    if (added)
        acc = apply_feature_single<+1>(j, acc, added);
    return acc;
}

sf_always_inline Tile apply_psq_features(IndexType                       j,
                                         Tile                            acc,
                                         const PSQFeatureSet::IndexList& removed,
                                         const PSQFeatureSet::IndexList& added,
                                         const FeatureTransformer&       ft,
                                         const WeightType*               psqWeights) {
    const i8 *oddRemoved, *oddAdded;
    acc = apply_features_but_odd<-1>(j, acc, removed, nullptr, 0, ft, psqWeights, oddRemoved);
    acc = apply_features_but_odd<+1>(j, acc, added, nullptr, 0, ft, psqWeights, oddAdded);
    return apply_odd_features(j, acc, oddRemoved, oddAdded);
}

template<int sign>
sf_always_inline Tile apply_threat_features(IndexType                          j,
                                            Tile                               acc,
                                            const ThreatFeatureSet::IndexList& list,
                                            const FeatureTransformer&          ft) {
    return apply_features<sign>(j, acc, list.begin(), list.ssize(), &ft.threatAndPpWeights[j]);
}

template<typename PsqRemoved, typename PsqAdded>
sf_always_inline Tile apply_combined_features(IndexType                          j,
                                              Tile                               acc,
                                              const PsqRemoved&                  psqRemoved,
                                              const ThreatFeatureSet::IndexList& thrRemoved,
                                              const PsqAdded&                    psqAdded,
                                              const ThreatFeatureSet::IndexList& thrAdded,
                                              const FeatureTransformer&          ft,
                                              const WeightType*                  psqBase) {
    const i8 *oddRemoved, *oddAdded;
    acc = apply_features_but_odd<-1>(j, acc, psqRemoved, thrRemoved.begin(), thrRemoved.ssize(), ft,
                                     psqBase, oddRemoved);
    acc = apply_features_but_odd<+1>(j, acc, psqAdded, thrAdded.begin(), thrAdded.ssize(), ft,
                                     psqBase, oddAdded);
    return apply_odd_features(j, acc, oddRemoved, oddAdded);
}

template<int RemovedCount, int AddedCount>
void apply_combined(Color                              perspective,
                    const FeatureTransformer&          featureTransformer,
                    const AccumulatorState&            from,
                    AccumulatorState&                  to,
                    const PSQFeatureSet::IndexList&    psqAdded,
                    const PSQFeatureSet::IndexList&    psqRemoved,
                    const ThreatFeatureSet::IndexList& thrAdded,
                    const ThreatFeatureSet::IndexList& thrRemoved,
                    const WeightType*                  psqWeights) {

    static_assert(RemovedCount == 1 || RemovedCount == 2);
    static_assert(AddedCount == 1 || AddedCount == 2);

    const auto& fromAcc = from.accumulation[perspective];
    auto&       toAcc   = to.accumulation[perspective];

    std::array<const i8*, RemovedCount> removedWeights;
    std::array<const i8*, AddedCount>   addedWeights;
    for (int i = 0; i < RemovedCount; ++i)
        removedWeights[i] = &psqWeights[usize(psqRemoved[i]) * Dimensions];
    for (int i = 0; i < AddedCount; ++i)
        addedWeights[i] = &psqWeights[usize(psqAdded[i]) * Dimensions];

    for (IndexType j = 0; j < Dimensions; increment_index(j))
    {
        Tile acc = load_tile(j, fromAcc.data());

        acc = apply_combined_features(j, acc, removedWeights, thrRemoved, addedWeights, thrAdded,
                                      featureTransformer, psqWeights);

        store_tile(j, toAcc.data(), acc);
    }
}

void apply_combined(Color                              perspective,
                    const FeatureTransformer&          featureTransformer,
                    const AccumulatorState&            from,
                    AccumulatorState&                  to,
                    const PSQFeatureSet::IndexList&    psqAdded,
                    const PSQFeatureSet::IndexList&    psqRemoved,
                    const ThreatFeatureSet::IndexList& thrAdded,
                    const ThreatFeatureSet::IndexList& thrRemoved,
                    const WeightType*                  psqWeights) {
    assert(psqRemoved.size() == 1 || psqRemoved.size() == 2);
    assert(psqAdded.size() == 1 || psqAdded.size() == 2);

    // Dispatch once; feature counts and row addresses stay fixed across tiles.
    if (psqRemoved.size() == 1)
    {
        if (psqAdded.size() == 1)
            apply_combined<1, 1>(perspective, featureTransformer, from, to, psqAdded, psqRemoved,
                                 thrAdded, thrRemoved, psqWeights);
        else
            apply_combined<1, 2>(perspective, featureTransformer, from, to, psqAdded, psqRemoved,
                                 thrAdded, thrRemoved, psqWeights);
    }
    else
    {
        if (psqAdded.size() == 1)
            apply_combined<2, 1>(perspective, featureTransformer, from, to, psqAdded, psqRemoved,
                                 thrAdded, thrRemoved, psqWeights);
        else
            apply_combined<2, 2>(perspective, featureTransformer, from, to, psqAdded, psqRemoved,
                                 thrAdded, thrRemoved, psqWeights);
    }
}

template<bool Forward>
void update_accumulator_incremental(Color                     perspective,
                                    const FeatureTransformer& featureTransformer,
                                    const Square              ksq,
                                    int                       psqBlock,
                                    AccumulatorState&         target_state,
                                    const AccumulatorState&   computed) {

    assert(computed.computed[perspective]);
    assert(!target_state.computed[perspective]);

    // The size must be enough to contain the largest possible update.
    // That might depend on the feature set and generally relies on the
    // feature set's update cost calculation to be correct and never allow
    // updates with more added/removed features than MaxActiveDimensions.
    PSQFeatureSet::IndexList    psqRemoved, psqAdded;
    ThreatFeatureSet::IndexList thrRemoved, thrAdded;

    const auto& dirtyPiece     = Forward ? target_state.dirtyPiece : computed.dirtyPiece;
    const auto& dirtyThreats   = Forward ? target_state.dirtyThreats : computed.dirtyThreats;
    const auto& dirtyPawnPairs = Forward ? target_state.dirtyPawnPairs : computed.dirtyPawnPairs;

    // Used solely for prefetching
    const auto* threatPpBase = &featureTransformer.threatAndPpWeights[0];
    IndexType   pfStride     = FeatureTransformer::OutputDimensions;

    if constexpr (Forward)
    {
        ThreatFeatureSet::append_changed_indices(perspective, ksq, dirtyThreats, thrRemoved,
                                                 thrAdded, threatPpBase, pfStride);
        PairFeatureSet::append_changed_indices(perspective, ksq, dirtyPawnPairs, thrRemoved,
                                               thrAdded, threatPpBase, pfStride);
        PSQFeatureSet::append_changed_indices(perspective, ksq, dirtyPiece, psqRemoved, psqAdded);
    }
    else
    {
        ThreatFeatureSet::append_changed_indices(perspective, ksq, dirtyThreats, thrAdded,
                                                 thrRemoved, threatPpBase, pfStride);
        PairFeatureSet::append_changed_indices(perspective, ksq, dirtyPawnPairs, thrAdded,
                                               thrRemoved, threatPpBase, pfStride);
        PSQFeatureSet::append_changed_indices(perspective, ksq, dirtyPiece, psqAdded, psqRemoved);
    }

    apply_combined(perspective, featureTransformer, computed, target_state, psqAdded, psqRemoved,
                   thrAdded, thrRemoved, featureTransformer.psq_weights(psqBlock));

    target_state.computed[perspective] = true;
}

void update_accumulator_incremental_both(const FeatureTransformer& featureTransformer,
                                         Square                    white_ksq,
                                         Square                    black_ksq,
                                         int                       whiteBlock,
                                         int                       blackBlock,
                                         AccumulatorState&         target_state,
                                         const AccumulatorState&   computed) {

    assert(computed.computed[WHITE]);
    assert(computed.computed[BLACK]);
    assert(!target_state.computed[WHITE]);
    assert(!target_state.computed[BLACK]);

    PSQFeatureSet::IndexList    psq_removed[COLOR_NB], psq_added[COLOR_NB];
    ThreatFeatureSet::IndexList thr_removed[COLOR_NB], thr_added[COLOR_NB];

    const auto* threat_pp_base = &featureTransformer.threatAndPpWeights[0];
    const auto  pf_stride      = FeatureTransformer::OutputDimensions;

    ThreatFeatureSet::append_changed_indices_both(
      white_ksq, black_ksq, target_state.dirtyThreats, thr_removed[WHITE], thr_added[WHITE],
      thr_removed[BLACK], thr_added[BLACK], threat_pp_base, pf_stride);
    PairFeatureSet::append_changed_indices_both(
      white_ksq, black_ksq, target_state.dirtyPawnPairs, thr_removed[WHITE], thr_added[WHITE],
      thr_removed[BLACK], thr_added[BLACK], threat_pp_base, pf_stride);
    PSQFeatureSet::append_changed_indices(WHITE, white_ksq, target_state.dirtyPiece,
                                          psq_removed[WHITE], psq_added[WHITE]);
    PSQFeatureSet::append_changed_indices(BLACK, black_ksq, target_state.dirtyPiece,
                                          psq_removed[BLACK], psq_added[BLACK]);

    apply_combined(WHITE, featureTransformer, computed, target_state, psq_added[WHITE],
                   psq_removed[WHITE], thr_added[WHITE], thr_removed[WHITE],
                   featureTransformer.psq_weights(whiteBlock));
    apply_combined(BLACK, featureTransformer, computed, target_state, psq_added[BLACK],
                   psq_removed[BLACK], thr_added[BLACK], thr_removed[BLACK],
                   featureTransformer.psq_weights(blackBlock));

    target_state.computed[WHITE] = true;
    target_state.computed[BLACK] = true;
}

Bitboard get_changed_pieces(const std::array<Piece, SQUARE_NB>& oldPieces,
                            const std::array<Piece, SQUARE_NB>& newPieces) {
#if defined(USE_AVX2)
    static_assert(sizeof(Piece) == 1);
    Bitboard sameBB = 0;

    for (int i = 0; i < 64; i += 32)
    {
        const __m256i old_v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&oldPieces[i]));
        const __m256i new_v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&newPieces[i]));
        const __m256i cmpEqual  = _mm256_cmpeq_epi8(old_v, new_v);
        const u32     equalMask = _mm256_movemask_epi8(cmpEqual);
        sameBB |= static_cast<Bitboard>(equalMask) << i;
    }
    return ~sameBB;
#elif defined(USE_LASX)
    static_assert(sizeof(Piece) == 1);

    Bitboard changed = 0;

    for (int i = 0; i < 64; i += 32)
    {
        const __m256i old_v = __lasx_xvld(reinterpret_cast<const void*>(&oldPieces[i]), 0);
        const __m256i new_v = __lasx_xvld(reinterpret_cast<const void*>(&newPieces[i]), 0);
        const __m256i diff  = __lasx_xvxor_v(old_v, new_v);
        const __m256i mask  = __lasx_xvmsknz_b(diff);
        const auto    lo    = __lasx_xvpickve2gr_d(mask, 0);
        const auto    hi    = __lasx_xvpickve2gr_d(mask, 2);

        changed |= (static_cast<Bitboard>(lo) | (static_cast<Bitboard>(hi) << 16)) << i;
    }

    return changed;
#elif defined(USE_LSX)
    static_assert(sizeof(Piece) == 1);

    Bitboard changed = 0;

    for (int i = 0; i < 64; i += 16)
    {
        const __m128i old_v = __lsx_vld(reinterpret_cast<const void*>(&oldPieces[i]), 0);
        const __m128i new_v = __lsx_vld(reinterpret_cast<const void*>(&newPieces[i]), 0);
        const __m128i diff  = __lsx_vxor_v(old_v, new_v);
        const __m128i mask  = __lsx_vmsknz_b(diff);

        changed |= static_cast<Bitboard>(__lsx_vpickve2gr_d(mask, 0)) << i;
    }

    return changed;
#elif defined(USE_NEON)
    uint8x16x4_t old_v = vld4q_u8(reinterpret_cast<const u8*>(oldPieces.data()));
    uint8x16x4_t new_v = vld4q_u8(reinterpret_cast<const u8*>(newPieces.data()));
    auto         cmp   = [=](const int i) { return vceqq_u8(old_v.val[i], new_v.val[i]); };

    uint8x16_t cmp0_1 = vsriq_n_u8(cmp(1), cmp(0), 1);
    uint8x16_t cmp2_3 = vsriq_n_u8(cmp(3), cmp(2), 1);
    uint8x16_t merged = vsriq_n_u8(cmp2_3, cmp0_1, 2);
    merged            = vsriq_n_u8(merged, merged, 4);
    uint8x8_t sameBB  = vshrn_n_u16(vreinterpretq_u16_u8(merged), 4);

    return ~vget_lane_u64(vreinterpret_u64_u8(sameBB), 0);
#elif defined(USE_SSE2)
    Bitboard sameBB = 0;

    for (int i = 0; i < 64; i += 16)
    {
        const __m128i old_v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&oldPieces[i]));
        const __m128i new_v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&newPieces[i]));
        const __m128i same  = _mm_cmpeq_epi8(old_v, new_v);

        sameBB |= static_cast<Bitboard>(_mm_movemask_epi8(same)) << i;
    }

    return ~sameBB;
#elif defined(USE_RVV)

    #define IMPL(mx, bx) \
        return __riscv_vmv_x_s_u64m1_u64(__riscv_vreinterpret_v_u8m1_u64m1( \
          __riscv_vreinterpret_v_b##bx##_u8m1(__riscv_vmsne_vv_i8m##mx##_b##bx( \
            __riscv_vle8_v_i8m##mx(reinterpret_cast<const i8*>(oldPieces.data()), 64), \
            __riscv_vle8_v_i8m##mx(reinterpret_cast<const i8*>(newPieces.data()), 64), 64))))


    usize vl = __riscv_vsetvlmax_e8m1();
    if (vl >= 64)
        IMPL(1, 8);
    else if (vl == 32)
        IMPL(2, 4);
    else
        IMPL(4, 2);

    #undef IMPL

#else
    Bitboard changed = 0;

    for (Square sq = SQUARE_ZERO; sq < SQUARE_NB; ++sq)
        changed |= static_cast<Bitboard>(oldPieces[sq] != newPieces[sq]) << sq;

    return changed;
#endif
}

// Append the PSQ feature indices that differ between two piece arrays, given
// the bitboards of the pieces present before and after the change.
sf_always_inline void append_changed_psq_indices(Color                               perspective,
                                                 Square                              ksq,
                                                 const std::array<Piece, SQUARE_NB>& oldPieces,
                                                 Bitboard                            oldBB,
                                                 const std::array<Piece, SQUARE_NB>& newPieces,
                                                 Bitboard                            newBB,
                                                 PSQFeatureSet::IndexList&           removed,
                                                 PSQFeatureSet::IndexList&           added) {
    const Bitboard changedBB = get_changed_pieces(oldPieces, newPieces);
    const Bitboard removedBB = changedBB & oldBB;
    const Bitboard addedBB   = changedBB & newBB;

#if defined(USE_AVX512ICL)
    PSQFeatureSet::write_indices(oldPieces, newPieces, removedBB, addedBB, perspective, ksq,
                                 removed, added);
#else
    Bitboard r = removedBB;
    while (r)
    {
        const Square sq = pop_lsb(r);
        removed.push_back(PSQFeatureSet::make_index(perspective, sq, oldPieces[sq], ksq));
    }

    Bitboard a = addedBB;
    while (a)
    {
        const Square sq = pop_lsb(a);
        added.push_back(PSQFeatureSet::make_index(perspective, sq, newPieces[sq], ksq));
    }
#endif
}

// Updates accumulator for a king move, and also updates the accumulator cache
// for the new king position.
void update_accumulator_hybrid(Color                     perspective,
                               const Position&           pos,
                               const FeatureTransformer& featureTransformer,
                               AccumulatorState&         target,
                               const AccumulatorState&   computed,
                               AccumulatorCaches&        cache) {
    const auto& dirtyPiece = target.dirtyPiece;

    assert(dirtyPiece.pc == make_piece(perspective, KING));
    assert(dirtyPiece.to != SQ_NONE);
    assert((int(dirtyPiece.from) & 0b100) == (int(dirtyPiece.to) & 0b100));
    assert(computed.computed[perspective]);
    assert(!target.computed[perspective]);

    const Square oldKsq = dirtyPiece.from;
    const Square newKsq = dirtyPiece.to;
    assert(oldKsq != newKsq);

    const auto& currentPieces  = pos.piece_array();
    auto        previousPieces = currentPieces;  // copies 64 bytes!

    Bitboard previousPieceBB = pos.pieces();

    assert(previousPieces[newKsq] == dirtyPiece.pc);

    if (dirtyPiece.remove_sq != SQ_NONE)
    {
        assert(dirtyPiece.remove_sq == newKsq);
        previousPieces[newKsq] = dirtyPiece.remove_pc;
    }
    else
    {
        previousPieces[newKsq] = NO_PIECE;
        previousPieceBB &= ~square_bb(newKsq);
    }

    assert(previousPieces[oldKsq] == NO_PIECE);
    previousPieces[oldKsq] = make_piece(perspective, KING);
    previousPieceBB |= square_bb(oldKsq);

    const int   psqBlock   = PSQFeatureSet::block(pos, perspective);
    const auto& oldEntry   = cache[psqBlock][oldKsq][perspective];
    auto&       newEntry   = cache[psqBlock][newKsq][perspective];
    const auto* psqWeights = featureTransformer.psq_weights(psqBlock);

    // "Remove" means we need to remove them from the cache entry,
    // "Add" means add them to the entry to get the accumulator we want
    PSQFeatureSet::IndexList oldRemove, oldAdd, newRemove, newAdd;

    append_changed_psq_indices(perspective, oldKsq, oldEntry.pieces, oldEntry.pieceBB,
                               previousPieces, previousPieceBB, oldRemove, oldAdd);
    append_changed_psq_indices(perspective, newKsq, newEntry.pieces, newEntry.pieceBB,
                               currentPieces, pos.pieces(), newRemove, newAdd);

    ThreatFeatureSet::IndexList thrRemoved, thrAdded;  // also contain pp indices
    const auto*                 threatPpBase = &featureTransformer.threatAndPpWeights[0];
    IndexType                   pfStride     = FeatureTransformer::OutputDimensions;
    ThreatFeatureSet::append_changed_indices(perspective, newKsq, target.dirtyThreats, thrRemoved,
                                             thrAdded, threatPpBase, pfStride);
    PairFeatureSet::append_changed_indices(perspective, newKsq, target.dirtyPawnPairs, thrRemoved,
                                           thrAdded, threatPpBase, pfStride);

    const auto& fromAcc = computed.accumulation[perspective];
    auto&       toAcc   = target.accumulation[perspective];

    for (IndexType j = 0; j < Dimensions; increment_index(j))
    {
        Tile acc = load_tile(j, newEntry.accumulation.data());

        acc = apply_psq_features(j, acc, newRemove, newAdd, featureTransformer, psqWeights);

        store_tile(j, newEntry.accumulation.data(), acc);

        // adding the old accumulator adds (most of) the threats and pp weights that we need
        acc = apply<+1>(j, acc, &fromAcc[j]);
        // But we have added a whole bunch of psq weights for the wrong king bucket which
        // we need to remove
        // first we remove the cached psq accumulation for the old king position...
        acc = apply<-1>(j, acc, &oldEntry.accumulation[j]);

        // ... then we adjust
        // The old entry's removed psq features have to be added back, its added ones removed
        acc = apply_combined_features(j, acc, oldAdd, thrRemoved, oldRemove, thrAdded,
                                      featureTransformer, psqWeights);

        store_tile(j, toAcc.data(), acc);
    }

    newEntry.pieces  = currentPieces;
    newEntry.pieceBB = pos.pieces();

    target.computed[perspective] = true;
}

// HalfKA data comes from the Finny table entry, while the threats are built
// from the active threat features
void update_accumulator_refresh_cache(Color                     perspective,
                                      const FeatureTransformer& featureTransformer,
                                      const Position&           pos,
                                      AccumulatorState&         accumulator,
                                      AccumulatorCaches&        cache) {

    const int                psqBlock   = PSQFeatureSet::block(pos, perspective);
    const Square             ksq        = pos.square<KING>(perspective);
    auto&                    entry      = cache[psqBlock][ksq][perspective];
    const auto*              psqWeights = featureTransformer.psq_weights(psqBlock);
    PSQFeatureSet::IndexList removed, added;

    append_changed_psq_indices(perspective, ksq, entry.pieces, entry.pieceBB, pos.piece_array(),
                               pos.pieces(), removed, added);

    entry.pieceBB = pos.pieces();
    entry.pieces  = pos.piece_array();

    ThreatFeatureSet::IndexList active;
    ThreatFeatureSet::append_active_indices(perspective, pos, active);
    PairFeatureSet::append_active_indices(perspective, pos, active);

    accumulator.computed[perspective] = true;

    for (IndexType j = 0; j < Dimensions; increment_index(j))
    {
        Tile acc = load_tile(j, entry.accumulation.data());

        acc = apply_psq_features(j, acc, removed, added, featureTransformer, psqWeights);

        store_tile(j, &entry.accumulation[0], acc);

        acc = apply_threat_features<+1>(j, acc, active, featureTransformer);

        store_tile(j, accumulator.accumulation[perspective].data(), acc);
    }
}

}

}

// === sf_src/src/nnue/nnue_misc.cpp ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Code for calculating NNUE evaluation function


#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iosfwd>
#include <iostream>
#include <memory>
#include <sstream>


namespace Stockfish::Eval::NNUE {


namespace {


// Converts a Value into pawns, always keeping two decimals
void format_cp_aligned_dot(Value v, std::stringstream& stream, const Position& pos) {

    const double pawns = std::abs(0.01 * UCIEngine::to_cp(v, pos));

    stream << (v < 0   ? '-'
               : v > 0 ? '+'
                       : ' ')
           << std::setiosflags(std::ios::fixed) << std::setw(6) << std::setprecision(2) << pawns;
}
}


// Returns a string with the table for evaluation values bucket by bucket.
std::string
trace(Position& pos, const Eval::NNUE::Network& network, Eval::NNUE::AccumulatorCaches& caches) {

    std::stringstream ss;

    auto accumulators = std::make_unique<AccumulatorStack>();
    accumulators->reset();

    auto t = network.trace_evaluate(pos, *accumulators, caches);

    ss << "NNUE network contributions (Normalized, "
       << (pos.side_to_move() == WHITE ? "White to move)" : "Black to move)") << std::endl
       << "+------------+------------+\n"
       << "|   Bucket   | Evaluation |\n"
       << "+------------+------------+\n";

    for (usize bucket = 0; bucket < t.layerStacks; ++bucket)
    {
        ss << "|  " << std::left << std::setw(10) << bucket << std::right << "|  ";
        format_cp_aligned_dot(t.positional[bucket], ss, pos);
        ss << "   |";
        if (bucket == t.correctBucket)
            ss << " <-- this bucket is used";
        ss << '\n';
    }

    ss << "+------------+------------+\n";

    return ss.str();
}


}  // namespace Stockfish::Eval::NNUE

// === sf_src/src/nnue/features/full_threats.cpp ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//Definition of input features FullThreats of NNUE evaluation function


#include <array>
#include <cstdint>
#include <initializer_list>


namespace Stockfish::Eval::NNUE::Features {

struct alignas(64) ThreatLUTs {
    // [attacker][attacked][from < to]
    std::array<std::array<std::array<u32, 2>, PIECE_NB>, PIECE_NB> lut1{};
    // [pt][from][to]
    std::array<std::array<std::array<u16, SQUARE_NB>, SQUARE_NB>, PIECE_TYPE_NB> lut2{};
};

constexpr std::array<Piece, 12> AllPieces = {
  W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
  B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
};

constexpr auto init_threat_luts() {
    ThreatLUTs                     tables{};
    std::array<u16, PIECE_TYPE_NB> offsets{};

    for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING})
    {
        u16 offset = 0;
        for (Square from = SQ_A1; from <= SQ_H8; ++from)
        {
            for (Square to = SQ_A1; to <= SQ_H8; ++to)
            {
                Bitboard attacks = (pt != PAWN) ? Attacks::PseudoAttacks[pt][from]
                                 : (to < from)  ? Attacks::PseudoAttacks[BLACK][from]
                                                : Attacks::PseudoAttacks[WHITE][from];
                tables.lut2[pt][from][to] =
                  offset + constexpr_popcount(((1ULL << to) - 1) & attacks);
            }

            if (pt != PAWN)
                offset += constexpr_popcount(Attacks::PseudoAttacks[pt][from]);
            else if (from >= SQ_A2 && from <= SQ_H7)
                offset += constexpr_popcount(Attacks::PseudoAttacks[WHITE][from]);
        }

        offsets[pt] = offset;
    }

    IndexType cumulativeOffset = 0;
    for (Piece attacker : AllPieces)
    {
        const PieceType attackerType          = type_of(attacker);
        const IndexType cumulativePieceOffset = offsets[attackerType];
        const IndexType validTargets          = numValidTargets[attacker];

        for (Piece attacked : AllPieces)
        {
            bool      enemy        = (attacker ^ attacked) == 8;
            PieceType attackedType = type_of(attacked);

            int  map           = FullThreats::map[attackerType - 1][attackedType - 1];
            bool semi_excluded = attackerType == attackedType && (enemy || attackerType != PAWN);
            IndexType feature =
              cumulativeOffset
              + (color_of(attacked) * (validTargets / 2) + map) * cumulativePieceOffset;

            bool excluded                      = map < 0;
            tables.lut1[attacker][attacked][0] = excluded ? FullThreats::Dimensions : feature;
            tables.lut1[attacker][attacked][1] =
              excluded || semi_excluded ? FullThreats::Dimensions : feature;
        }

        cumulativeOffset += validTargets * cumulativePieceOffset;
    }

    return tables;
}

constexpr ThreatLUTs threat_luts = init_threat_luts();

// Index of a feature for a given king position and another piece on some square
sf_always_inline IndexType FullThreats::make_index(
  Color perspective, Piece attacker, Square from, Square to, Piece attacked, Square ksq) {
    const u8 orientation   = OrientTBL[ksq] ^ (56 * perspective);
    unsigned from_oriented = u8(from) ^ orientation;
    unsigned to_oriented   = u8(to) ^ orientation;

    const u8 swap              = 8 * perspective;
    unsigned attacker_oriented = attacker ^ swap;
    unsigned attacked_oriented = attacked ^ swap;

    return threat_luts.lut1[attacker_oriented][attacked_oriented][from_oriented < to_oriented]
         + threat_luts.lut2[type_of(attacker)][from_oriented][to_oriented];
}

// Get a list of indices for active features in ascending order

void FullThreats::append_active_indices(Color perspective, const Position& pos, IndexList& active) {
    const Square   ksq                = pos.square<KING>(perspective);
    const Bitboard occupied           = pos.pieces();
    const Bitboard pawnTargets        = pos.pieces(KNIGHT, ROOK);
    const Bitboard minorSliderTargets = pos.pieces(PAWN, KNIGHT, BISHOP, ROOK);
    const Bitboard queenTargets       = pos.pieces(PAWN, KNIGHT, BISHOP, ROOK, QUEEN);

    auto process_pawn_attacks = [&](Color c, Direction attkDir) {
        const Bitboard cPawns  = pos.pieces(c, PAWN);
        Bitboard       attacks = shift(cPawns, attkDir) & pawnTargets;
        while (attacks)
        {
            Square    to       = pop_lsb(attacks);
            Square    from     = to - attkDir;
            Piece     attacked = pos.piece_on(to);
            Piece     attacker = make_piece(c, PAWN);
            IndexType index    = make_index(perspective, attacker, from, to, attacked, ksq);
            active.push_back_if_lt(index, Dimensions);
        }
    };

    process_pawn_attacks(WHITE, NORTH_EAST);
    process_pawn_attacks(WHITE, NORTH_WEST);

    process_pawn_attacks(BLACK, SOUTH_WEST);
    process_pawn_attacks(BLACK, SOUTH_EAST);

    for (Color c : {WHITE, BLACK})
    {
        for (PieceType pt = KNIGHT; pt < KING; ++pt)
        {
            Piece    attacker = make_piece(c, pt);
            Bitboard bb       = pos.pieces(c, pt);
            Bitboard targets  = pt == KNIGHT || pt == QUEEN ? queenTargets : minorSliderTargets;
            while (bb)
            {
                Square   from    = pop_lsb(bb);
                Bitboard attacks = Attacks::attacks_bb(pt, from, occupied) & targets;
                while (attacks)
                {
                    Square    to       = pop_lsb(attacks);
                    Piece     attacked = pos.piece_on(to);
                    IndexType index    = make_index(perspective, attacker, from, to, attacked, ksq);
                    active.push_back_if_lt(index, Dimensions);
                }
            }
        }
    }
}

// Get a list of indices for recently changed features

void FullThreats::append_changed_indices(Color                   perspective,
                                         Square                  ksq,
                                         const DiffType&         diff,
                                         IndexList&              removed,
                                         IndexList&              added,
                                         const ThreatWeightType* prefetchBase,
                                         IndexType               prefetchStride) {
    const unsigned orient    = OrientTBL[ksq] ^ (56 * perspective);
    const unsigned pair_swap = 0x88 * perspective;
    const auto*    lut1_flat = &threat_luts.lut1[0][0];

    for (const auto& dirty : diff.list)
    {
        const u32      raw   = dirty.raw();
        const unsigned from  = u8(raw) ^ orient;
        const unsigned to    = u8(raw >> 8) ^ orient;
        const unsigned pair  = u8(raw >> 16);
        const auto&    plut2 = threat_luts.lut2[(pair >> 4) & 7];

        const IndexType index = lut1_flat[pair ^ pair_swap][from < to] + plut2[from][to];

        if (prefetchBase)
            prefetch<PrefetchRw::READ, PrefetchLoc::LOW>(reinterpret_cast<const void*>(
              reinterpret_cast<uintptr_t>(prefetchBase) + uintptr_t(index) * prefetchStride));

        if (raw >> 31)
            added.push_back_if_lt(index, Dimensions);
        else
            removed.push_back_if_lt(index, Dimensions);
    }
}

void FullThreats::append_changed_indices_both(Square                  white_ksq,
                                              Square                  black_ksq,
                                              const DiffType&         diff,
                                              IndexList&              white_removed,
                                              IndexList&              white_added,
                                              IndexList&              black_removed,
                                              IndexList&              black_added,
                                              const ThreatWeightType* prefetchBase,
                                              IndexType               prefetchStride) {
    const unsigned w_orient  = OrientTBL[white_ksq];
    const unsigned b_orient  = OrientTBL[black_ksq] ^ 56;
    const auto*    lut1_flat = &threat_luts.lut1[0][0];

    for (const auto& dirty : diff.list)
    {
        const u32      raw   = dirty.raw();
        const unsigned from  = u8(raw);
        const unsigned to    = u8(raw >> 8);
        const unsigned pair  = u8(raw >> 16);
        const auto&    plut2 = threat_luts.lut2[(pair >> 4) & 7];

        const unsigned  w_from      = from ^ w_orient;
        const unsigned  w_to        = to ^ w_orient;
        const IndexType white_index = lut1_flat[pair][w_from < w_to] + plut2[w_from][w_to];

        const unsigned  b_from      = from ^ b_orient;
        const unsigned  b_to        = to ^ b_orient;
        const IndexType black_index = lut1_flat[pair ^ 0x88][b_from < b_to] + plut2[b_from][b_to];

        if (prefetchBase)
        {
            prefetch<PrefetchRw::READ, PrefetchLoc::LOW>(reinterpret_cast<const void*>(
              reinterpret_cast<uintptr_t>(prefetchBase) + uintptr_t(white_index) * prefetchStride));
            prefetch<PrefetchRw::READ, PrefetchLoc::LOW>(reinterpret_cast<const void*>(
              reinterpret_cast<uintptr_t>(prefetchBase) + uintptr_t(black_index) * prefetchStride));
        }

        if (raw >> 31)
        {
            white_added.push_back_if_lt(white_index, Dimensions);
            black_added.push_back_if_lt(black_index, Dimensions);
        }
        else
        {
            white_removed.push_back_if_lt(white_index, Dimensions);
            black_removed.push_back_if_lt(black_index, Dimensions);
        }
    }
}

}  // namespace Stockfish::Eval::NNUE::Features

// === sf_src/src/nnue/features/k32h3.cpp ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//Definition of input features K32H3 of NNUE evaluation function


#include <array>

#if defined(USE_AVX512ICL)
#endif

namespace Stockfish::Eval::NNUE::Features {

#if defined(USE_AVX512ICL)
void K32H3::write_indices(const std::array<Piece, SQUARE_NB>& oldPieces,
                          const std::array<Piece, SQUARE_NB>& newPieces,
                          Bitboard                            removedBB,
                          Bitboard                            addedBB,
                          Color                               perspective,
                          Square                              ksq,
                          IndexList&                          removed,
                          IndexList&                          added) {

    auto* write_removed = removed.make_space(popcount(removedBB));
    auto* write_added   = added.make_space(popcount(addedBB));

    const __m512i vecOldPieces = _mm512_loadu_si512(oldPieces.data());
    const __m512i vecNewPieces = _mm512_loadu_si512(newPieces.data());

    // PieceSquareIndex and KingBuckets are multiples of 64, while s and orient use only the low
    // six bits. Therefore no carry crosses bit 6, and
    // (s ^ orient) + psi[pc] + kingBucket == s ^ (psi[pc] + kingBucket + orient),
    // allowing the orientation to be folded into the per-piece lookup offset.
    const u16     flip       = 56 * perspective;
    const u16     orient     = u16(OrientTBL[ksq]) ^ flip;
    const u16     kingBucket = u16(KingBuckets[int(ksq) ^ flip]);
    const __m512i psi =
      _mm512_castsi256_si512(_mm256_loadu_si256((const __m256i*) PieceSquareIndex[perspective]));
    const __m512i psi_plus_offset =
      _mm512_add_epi16(psi, _mm512_set1_epi16(u16(kingBucket + orient)));

    __m512i removed_squares = _mm512_maskz_compress_epi8(removedBB, AllSquares);
    __m512i added_squares   = _mm512_maskz_compress_epi8(addedBB, AllSquares);
    __m512i removed_pieces  = _mm512_maskz_compress_epi8(removedBB, vecOldPieces);
    __m512i added_pieces    = _mm512_maskz_compress_epi8(addedBB, vecNewPieces);

    removed_squares = _mm512_cvtepi8_epi16(_mm512_castsi512_si256(removed_squares));
    added_squares   = _mm512_cvtepi8_epi16(_mm512_castsi512_si256(added_squares));
    removed_pieces  = _mm512_cvtepi8_epi16(_mm512_castsi512_si256(removed_pieces));
    added_pieces    = _mm512_cvtepi8_epi16(_mm512_castsi512_si256(added_pieces));

    const __m512i removed_indices =
      _mm512_xor_si512(removed_squares, _mm512_permutexvar_epi16(removed_pieces, psi_plus_offset));
    const __m512i added_indices =
      _mm512_xor_si512(added_squares, _mm512_permutexvar_epi16(added_pieces, psi_plus_offset));

    _mm512_storeu_si512(write_removed, removed_indices);
    _mm512_storeu_si512(write_added, added_indices);
}
#endif

// Index of a feature within its block for a given king position and another piece on
// some square

IndexType K32H3::make_index(Color perspective, Square s, Piece pc, Square ksq) {
    alignas(64) static constexpr auto offsets = [] {
        std::array<std::array<u16, PIECE_NB>, COLOR_NB * SQUARE_NB> table{};
        for (int c = 0; c < COLOR_NB; ++c)
            for (int sq = 0; sq < SQUARE_NB; ++sq)
            {
                const u16 flip       = 56 * c;
                const u16 orient     = u16(OrientTBL[sq]) ^ flip;
                const u16 kingBucket = u16(KingBuckets[sq ^ flip]);
                for (int pieceIndex = 0; pieceIndex < PIECE_NB; ++pieceIndex)
                    table[c * SQUARE_NB + sq][pieceIndex] =
                      PieceSquareIndex[c][pieceIndex] + kingBucket + orient;
            }
        return table;
    }();

    return IndexType(s) ^ offsets[perspective * SQUARE_NB + ksq][pc];
}

// Get a list of indices for recently changed features

void K32H3::append_changed_indices(
  Color perspective, Square ksq, const DiffType& diff, IndexList& removed, IndexList& added) {
    removed.push_back(make_index(perspective, diff.from, diff.pc, ksq));
    if (diff.to != SQ_NONE)
        added.push_back(make_index(perspective, diff.to, diff.pc, ksq));

    if (diff.remove_sq != SQ_NONE)
        removed.push_back(make_index(perspective, diff.remove_sq, diff.remove_pc, ksq));

    if (diff.add_sq != SQ_NONE)
        added.push_back(make_index(perspective, diff.add_sq, diff.add_pc, ksq));
}

int K32H3::block(const Position& pos, Color perspective) {
    return pos.count<QUEEN>(~perspective) ? 2 : bool(pos.pieces(ROOK, QUEEN));
}

K32H3::BlockState K32H3::block_state(const Position& pos) {
    return {u8(block(pos, WHITE)), u8(block(pos, BLACK))};
}

bool K32H3::requires_refresh(const DiffType& diff,
                             Color           perspective,
                             int             blockBefore,
                             int             blockAfter) {
    return diff.pc == make_piece(perspective, KING) || blockBefore != blockAfter;
}

}  // namespace Stockfish::Eval::NNUE::Features

// === sf_src/src/nnue/features/pp_3wide.cpp ===
/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <algorithm>
#include <cassert>
#include <cstdint>


namespace Stockfish::Eval::NNUE::Features {

static constexpr IndexType make_pawn_id(Color color, Square square) {
    assert(square >= SQ_A2 && square <= SQ_H7);
    return 48 * int(color) + square - SQ_A2;
}

static constexpr IndexType pp_idx(IndexType a, IndexType b) {
    const IndexType hi = std::max(a, b);
    const IndexType lo = std::min(a, b);
    return hi * (hi - 1) / 2 + lo + PP_3Wide::IndexBase;
}

#ifdef USE_AVX512ICL
static inline __m256i pp_idx_epi16(__m256i a, __m256i b) {
    const __m256i hi   = _mm256_max_epu16(a, b);
    const __m256i lo   = _mm256_min_epu16(a, b);
    const __m256i prod = _mm256_mullo_epi16(hi, _mm256_sub_epi16(hi, _mm256_set1_epi16(1)));
    return _mm256_add_epi16(_mm256_add_epi16(_mm256_srli_epi16(prod, 1), lo),
                            _mm256_set1_epi16(i16(PP_3Wide::IndexBase)));
}
#endif

sf_always_inline IndexType PP_3Wide::make_index(
  Color perspective, Color color, Square from, Square to, Color pairedColor, Square ksq) {
    const u8 orientation   = OrientTBL[ksq] ^ (56 * perspective);
    unsigned from_oriented = u8(from) ^ orientation;
    unsigned to_oriented   = u8(to) ^ orientation;

    Color color_oriented       = Color(color ^ perspective);
    Color pairedColor_oriented = Color(pairedColor ^ perspective);

    assert(from_oriented >= SQ_A2 && from_oriented <= SQ_H7);
    assert(to_oriented >= SQ_A2 && to_oriented <= SQ_H7);

    const IndexType idA = make_pawn_id(color_oriented, Square(from_oriented));
    const IndexType idB = make_pawn_id(pairedColor_oriented, Square(to_oriented));
    return pp_idx(idA, idB);
}

template<usize N>
static void append_indices_impl(Color perspective,
                                const Square (&ksq)[N],
                                Bitboard updated,
                                Bitboard pawnsW,
                                Bitboard pawnsB,
                                PP_3Wide::IndexList* const (&out)[N],
                                [[maybe_unused]] const ThreatWeightType* prefetchBase,
                                [[maybe_unused]] IndexType               prefetchStride) {
    static_assert(N == 1 || N == 2);

    if (!updated)
        return;

    u8 orientation[N];
    for (usize i = 0; i < N; ++i)
        orientation[i] = OrientTBL[ksq[i]] ^ u8(56 * (perspective ^ i));

#ifdef USE_AVX512ICL
    __m512i ids[N];
    for (usize i = 0; i < N; ++i)
    {
        const Bitboard enemy    = (perspective ^ i) == WHITE ? pawnsB : pawnsW;
        const __m512i  adjusted = _mm512_sub_epi8(
          _mm512_xor_si512(AllSquares, _mm512_set1_epi8(orientation[i])), _mm512_set1_epi8(8));
        ids[i] = _mm512_mask_add_epi8(adjusted, enemy, adjusted, _mm512_set1_epi8(48));
    }
#endif

    // Enumerate changed pawns and their partners once for all requested perspectives.
    const Bitboard unchanged = (pawnsW | pawnsB) & ~updated;
    for (Bitboard u = updated; u;)
    {
        const Square   a        = pop_lsb(u);
        const Bitboard partners = pawn_pair_bb(a) & (unchanged | u);
        if (!partners)
            continue;

#ifdef USE_AVX512ICL
        const int n = popcount(partners);
        assert(n <= 16);

        for (usize i = 0; i < N; ++i)
        {
            const Bitboard enemy = (perspective ^ i) == WHITE ? pawnsB : pawnsW;
            const u16      aId   = u16(((u8(a) ^ orientation[i]) - 8) + ((enemy & a) ? 48 : 0));
            const __m256i  pids  = _mm256_cvtepu8_epi16(
              _mm512_castsi512_si128(_mm512_maskz_compress_epi8(partners, ids[i])));
            const __m256i feats = pp_idx_epi16(_mm256_set1_epi16(aId), pids);
            // Position::set material limits bound refresh lists to 232 entries:
            // 80 + 8 * (16 - P) + 2 * P + P * (P - 1) / 2, for P <= 16 pawns.
            // Updates have at most 96 threats + 120 pairs. Both fit in 256 entries
            // including the 15 extra lanes written by an unmasked store.
            _mm256_storeu_epi16(out[i]->make_space(n), feats);
        }
#else
        const Color aCol = (pawnsB & a) ? BLACK : WHITE;
        IndexType   aId[N];
        for (usize i = 0; i < N; ++i)
            aId[i] = make_pawn_id(Color(aCol ^ perspective ^ i), Square(u8(a) ^ orientation[i]));

        auto push = [&](Square to, Color pairedColor) {
            for (usize i = 0; i < N; ++i)
            {
                const IndexType bId   = make_pawn_id(Color(pairedColor ^ perspective ^ i),
                                                     Square(u8(to) ^ orientation[i]));
                const IndexType index = pp_idx(aId[i], bId);
                if (prefetchBase)
                    prefetch<PrefetchRw::READ, PrefetchLoc::LOW>(
                      reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(prefetchBase)
                                                    + uintptr_t(index) * prefetchStride));
                out[i]->push_back(index);
            }
        };

        for (Bitboard pb = pawnsB & partners; pb;)
            push(pop_lsb(pb), BLACK);
        for (Bitboard pw = pawnsW & partners; pw;)
            push(pop_lsb(pw), WHITE);
#endif
    }
}

template<usize N>
static void append_changed_indices_impl(Color perspective,
                                        const Square (&ksq)[N],
                                        const PP_3Wide::DiffType& diff,
                                        PP_3Wide::IndexList* const (&removed)[N],
                                        PP_3Wide::IndexList* const (&added)[N],
                                        const ThreatWeightType* prefetchBase,
                                        IndexType               prefetchStride) {
    const Bitboard whiteBefore = diff.before[WHITE];
    const Bitboard blackBefore = diff.before[BLACK];
    const Bitboard whiteAfter  = diff.after[WHITE];
    const Bitboard blackAfter  = diff.after[BLACK];

    if (whiteBefore == whiteAfter && blackBefore == blackAfter)
        return;

    append_indices_impl<N>(perspective, ksq,
                           (whiteAfter & ~whiteBefore) | (blackAfter & ~blackBefore), whiteAfter,
                           blackAfter, added, prefetchBase, prefetchStride);
    append_indices_impl<N>(perspective, ksq,
                           (whiteBefore & ~whiteAfter) | (blackBefore & ~blackAfter), whiteBefore,
                           blackBefore, removed, prefetchBase, prefetchStride);
}

void PP_3Wide::append_active_indices(Color perspective, const Position& pos, IndexList& active) {
    const Bitboard white = pos.pieces(WHITE, PAWN);
    const Bitboard black = pos.pieces(BLACK, PAWN);
    append_indices_impl<1>(perspective, {pos.square<KING>(perspective)}, white | black, white,
                           black, {&active}, nullptr, 0);
}

void PP_3Wide::append_changed_indices(Color                   perspective,
                                      Square                  ksq,
                                      const DiffType&         diff,
                                      IndexList&              removed,
                                      IndexList&              added,
                                      const ThreatWeightType* prefetchBase,
                                      IndexType               prefetchStride) {
    append_changed_indices_impl<1>(perspective, {ksq}, diff, {&removed}, {&added}, prefetchBase,
                                   prefetchStride);
}

void PP_3Wide::append_changed_indices_both(Square                  white_ksq,
                                           Square                  black_ksq,
                                           const DiffType&         diff,
                                           IndexList&              white_removed,
                                           IndexList&              white_added,
                                           IndexList&              black_removed,
                                           IndexList&              black_added,
                                           const ThreatWeightType* prefetchBase,
                                           IndexType               prefetchStride) {
    append_changed_indices_impl<2>(WHITE, {white_ksq, black_ksq}, diff,
                                   {&white_removed, &black_removed}, {&white_added, &black_added},
                                   prefetchBase, prefetchStride);
}

}  // namespace Stockfish::Eval::NNUE::Features

// Minimal stubs for unused trace/debug symbols referenced by position.cpp & evaluate.cpp
namespace Stockfish {
    std::string UCIEngine::square(Square s) {
        return std::string{char('a' + file_of(s)), char('1' + rank_of(s))};
    }
    int UCIEngine::to_cp(Value v, const Position&) { return int(v); }
    namespace Tablebases {
        int MaxCardinality = 0;
        WDLScore probe_wdl(Position&, ProbeState* r) { *r = FAIL; return WDLDraw; }
        int probe_dtz(Position&, ProbeState* r) { *r = FAIL; return 0; }
    }
}


#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <array>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <bit>

namespace V8 {

namespace SF = Stockfish;
using U64 = uint64_t;
using U32 = uint32_t;
using U16 = uint16_t;
using U8  = uint8_t;

constexpr int MAX_PLY     = 128;
constexpr int MAX_MOVES   = 256;
constexpr int INF_SCORE   = 32000;
constexpr int MATE_SCORE  = 31000;
constexpr int MATE_IN_MAX = MATE_SCORE - MAX_PLY;
constexpr int VAL_NONE    = 32500;
constexpr int SEE_VAL[8]  = {0, 100, 320, 330, 500, 900, 20000, 0};

struct TuneParams {
    int rfp_margin = 74, rfp_improving = 55, razor_margin = 260, probcut_margin = 190;
    int fut_base = 115, fut_mult = 95, see_quiet = 65, see_cap = 28, lmr_base = 75, lmr_div = 215;
} TUNE;

std::unique_ptr<SF::Eval::NNUE::Network> g_net;

inline void init_engine() {
    SF::Attacks::init();
    g_net = std::make_unique<SF::Eval::NNUE::Network>();
    SF::Eval::NNUE::EvalFile ef{"nn-bddd9d57eb87.nnue", "nn-bddd9d57eb87.nnue"};
    g_net->load_internal(ef);
}

inline int eval_pos(const SF::Position& pos, SF::Eval::NNUE::AccumulatorStack& acc,
                    SF::Eval::NNUE::AccumulatorCaches& caches, int optimism = 0) {
    int raw = int(SF::Eval::evaluate(*g_net, pos, acc, caches, optimism));
    return (raw * 100) / 328;
}

inline bool see_ge_cp(const SF::Position& pos, SF::Move m, int cp_thresh) {
    return pos.see_ge(m, (cp_thresh * 208) / 100);
}

inline SF::PieceType cap_or_promo_pt(const SF::Position& pos, SF::Move m) {
    if (m.type_of() == SF::EN_PASSANT) return SF::PAWN;
    SF::Piece cap = pos.piece_on(m.to_sq());
    if (cap != SF::NO_PIECE) return SF::type_of(cap);
    return (m.type_of() == SF::PROMOTION) ? m.promotion_type() : SF::NO_PIECE_TYPE;
}

inline bool is_tactical(const SF::Position& pos, SF::Move m) {
    return pos.capture_stage(m) || m.type_of() == SF::PROMOTION;
}

inline bool is_legal_move(const SF::Position& pos, SF::Move m) {
    if (!m.is_ok()) return false;
    SF::Square from = m.from_sq(), to = m.to_sq();
    SF::Piece pc = pos.piece_on(from);
    SF::Color us = pos.side_to_move();
    if (pc == SF::NO_PIECE || SF::color_of(pc) != us) return false;
    if (!pos.checkers() && m.type_of() == SF::NORMAL) {
        if (pos.pieces(us) & SF::square_bb(to)) return false;
        if (SF::type_of(pc) != SF::PAWN) {
            if (!(pos.attackers_to(to, pos.pieces()) & SF::square_bb(from))) return false;
            return pos.legal(m);
        } else {
            int diff = int(to) - int(from);
            int push = (us == SF::WHITE) ? 8 : -8;
            if (diff == push)
                return pos.empty(to) && SF::rank_of(to) != SF::RANK_1 && SF::rank_of(to) != SF::RANK_8 && pos.legal(m);
            if (diff == 2 * push) {
                SF::Rank sr = (us == SF::WHITE) ? SF::RANK_2 : SF::RANK_7;
                return SF::rank_of(from) == sr && pos.empty(SF::Square(int(from) + push)) && pos.empty(to) && pos.legal(m);
            }
            if ((SF::Attacks::attacks_bb(SF::PAWN, from, us) & SF::square_bb(to)) &&
                (pos.pieces(~us) & SF::square_bb(to)) &&
                SF::rank_of(to) != SF::RANK_1 && SF::rank_of(to) != SF::RANK_8)
                return pos.legal(m);
            return false;
        }
    }
    if (pos.checkers()) {
        for (const auto& lm : SF::MoveList<SF::EVASIONS>(pos))
            if (lm == m && pos.legal(lm)) return true;
    } else {
        for (const auto& lm : SF::MoveList<SF::ALL>(pos))
            if (lm == m && pos.legal(lm)) return true;
    }
    return false;
}
// ============================================================================
// Transposition Table (4-entry Bucketed + ARM64 Acquire/Release)
// ============================================================================
enum Bound : U8 { BOUND_NONE = 0, BOUND_UPPER = 1, BOUND_LOWER = 2, BOUND_EXACT = 3 };

struct TTEntry {
    U32 key32 = 0;
    U16 move_raw = 0;
    int16_t score = VAL_NONE, eval = VAL_NONE;
    int8_t depth = -1;
    U8 gen_pv_bound = 0;
};
struct alignas(64) TTBucket { TTEntry entries[4]; };
struct TTData {
    SF::Move move = SF::Move::none();
    int score = VAL_NONE, eval = VAL_NONE, depth = -1;
    Bound bound = BOUND_NONE;
    bool pv = false, hit = false;
};

class TranspositionTable {
    std::vector<TTBucket> table;
    size_t mask = 0;
    U8 generation = 0;
public:
    TranspositionTable() { resize(32); }
    void resize(size_t mb) {
        size_t count = std::bit_floor(std::max<size_t>(1, mb) * 1048576ULL / sizeof(TTBucket));
        table.assign(count, TTBucket{});
        mask = count - 1;
    }
    void clear() { std::fill(table.begin(), table.end(), TTBucket{}); generation = 0; }
    void new_search() { generation = (generation + 1) & 0x1F; }
    inline void prefetch(U64 key) const { __builtin_prefetch(&table[key & mask]); }
    static inline int score_to_tt(int s, int ply) { return s >= MATE_IN_MAX ? s + ply : (s <= -MATE_IN_MAX ? s - ply : s); }
    static inline int score_from_tt(int s, int ply) { return s == VAL_NONE ? VAL_NONE : (s >= MATE_IN_MAX ? s - ply : (s <= -MATE_IN_MAX ? s + ply : s)); }

    TTData probe(U64 key, int ply) const {
        const TTBucket& b = table[key & mask];
        U32 k32 = U32(key >> 32);
        for (int i = 0; i < 4; ++i) {
            const TTEntry& e = b.entries[i];
            if (e.key32 == k32 && e.gen_pv_bound != 0) {
                std::atomic_thread_fence(std::memory_order_acquire);
                return {SF::Move(e.move_raw), score_from_tt(e.score, ply), e.eval, e.depth,
                        Bound(e.gen_pv_bound & 3), (e.gen_pv_bound & 4) != 0, true};
            }
        }
        return TTData{};
    }

    void store(U64 key, SF::Move m, int score, int raw_eval, int depth, Bound b, int ply, bool is_pv = false) {
        TTBucket& bkt = table[key & mask];
        U32 k32 = U32(key >> 32);
        TTEntry* rep = &bkt.entries[0];
        int worst = 999999;
        for (int i = 0; i < 4; ++i) {
            TTEntry& e = bkt.entries[i];
            if (e.gen_pv_bound == 0 || e.key32 == k32) { rep = &e; break; }
            int val = int(e.depth) - ((32 + int(generation) - int(e.gen_pv_bound >> 3)) & 0x1F) * 8;
            if (val < worst) { worst = val; rep = &e; }
        }
        bool same = (rep->key32 == k32 && rep->gen_pv_bound != 0);
        if (same && !m.is_ok()) m = SF::Move(rep->move_raw);
        if (!same || b == BOUND_EXACT || U8(rep->gen_pv_bound >> 3) != generation || depth + 4 >= int(rep->depth)) {
            rep->key32 = 0;
            rep->move_raw = m.raw();
            rep->score = int16_t(std::clamp(score_to_tt(score, ply), -32000, 32000));
            rep->eval  = (raw_eval == VAL_NONE) ? int16_t(VAL_NONE) : int16_t(std::clamp(raw_eval, -32000, 32000));
            rep->depth = int8_t(depth);
            rep->gen_pv_bound = U8((generation << 3) | (is_pv ? 4 : 0) | (b & 3));
            std::atomic_thread_fence(std::memory_order_release);
            rep->key32 = k32;
        }
    }
};
inline TranspositionTable TT;
// ============================================================================
// History Tables & Native MovePicker
// ============================================================================
constexpr int HIST_MAX  = 16384;
constexpr int CORR_SIZE = 16384;

inline void update_stat(int16_t& entry, int bonus) {
    int clamped = std::clamp(bonus, -HIST_MAX, HIST_MAX);
    entry += clamped - (int(entry) * std::abs(clamped)) / HIST_MAX;
}

struct PrevMove {
    SF::Piece pc = SF::NO_PIECE;
    SF::Square to = SF::SQ_A1;
    bool ok() const { return pc != SF::NO_PIECE; }
};

struct SearchHistory {
    int16_t main_hist[2][64][64][2][2], pawn_hist[512][16][64], low_ply_hist[4][64][64];
    int16_t cap_hist[16][64][8], cont_hist[16][64][16][64];
    int16_t corr_hist[2][CORR_SIZE], minor_corr[2][CORR_SIZE], non_pawn_corr[2][2][CORR_SIZE], cont_corr[16][64][16][64];
    SF::Move killers[MAX_PLY + 4][2], countermoves[16][64];

    void clear() {
        std::memset(main_hist, 0, sizeof(main_hist));
        std::memset(pawn_hist, 0, sizeof(pawn_hist));
        std::memset(corr_hist, 0, sizeof(corr_hist));
        std::memset(minor_corr, 0, sizeof(minor_corr));
        std::memset(non_pawn_corr, 0, sizeof(non_pawn_corr));
        std::memset(low_ply_hist, 0, sizeof(low_ply_hist));
        std::memset(cap_hist, 0, sizeof(cap_hist));
        std::memset(cont_hist, 0, sizeof(cont_hist));
        std::memset(cont_corr, 0, sizeof(cont_corr));
        for (int i = 0; i < MAX_PLY + 4; ++i) killers[i][0] = killers[i][1] = SF::Move::none();
        for (int p = 0; p < 16; ++p) for (int s = 0; s < 64; ++s) countermoves[p][s] = SF::Move::none();
    }

    int get_quiet_score(const SF::Position& pos, U64 opp_threats, SF::Move m, int ply,
                        PrevMove p1, PrevMove p2, PrevMove p4, PrevMove p6) const {
        SF::Piece pc = pos.moved_piece(m);
        int from = int(m.from_sq()), to = int(m.to_sq());
        int ft = (opp_threats >> from) & 1, tt = (opp_threats >> to) & 1;
        int sc = main_hist[pos.side_to_move()][from][to][ft][tt] + pawn_hist[pos.pawn_key() & 511][pc][to];
        if (ply < 4) sc += low_ply_hist[ply][from][to] * 2;
        if (p1.ok()) sc += cont_hist[p1.pc][p1.to][pc][to];
        if (p2.ok()) sc += cont_hist[p2.pc][p2.to][pc][to];
        if (p4.ok()) sc += cont_hist[p4.pc][p4.to][pc][to] / 2;
        if (p6.ok()) sc += cont_hist[p6.pc][p6.to][pc][to] / 2;
        return sc;
    }

    void update_quiet_histories(const SF::Position& pos, U64 opp_threats, SF::Move best,
                                const SF::Move* quiets, int qc, int depth, int ply,
                                PrevMove p1, PrevMove p2, PrevMove p4, PrevMove p6) {
        int bonus = std::min(2100, depth * depth * 16 + depth * 32);
        auto apply = [&](SF::Move m, int val) {
            SF::Piece pc = pos.moved_piece(m);
            int from = int(m.from_sq()), to = int(m.to_sq());
            int ft = (opp_threats >> from) & 1, tt = (opp_threats >> to) & 1;
            update_stat(main_hist[pos.side_to_move()][from][to][ft][tt], val);
            update_stat(pawn_hist[pos.pawn_key() & 511][pc][to], val);
            if (ply < 4) update_stat(low_ply_hist[ply][from][to], val);
            if (p1.ok()) update_stat(cont_hist[p1.pc][p1.to][pc][to], val);
            if (p2.ok()) update_stat(cont_hist[p2.pc][p2.to][pc][to], val);
            if (p4.ok()) update_stat(cont_hist[p4.pc][p4.to][pc][to], val / 2);
            if (p6.ok()) update_stat(cont_hist[p6.pc][p6.to][pc][to], val / 2);
        };
        apply(best, bonus);
        for (int i = 0; i < qc; ++i) if (quiets[i] != best) apply(quiets[i], -bonus);
    }

    void update_capture_histories(const SF::Position& pos, SF::Move best, const SF::Move* caps, int cc, int depth) {
        int bonus = std::min(2000, depth * depth * 14 + 32);
        if (best.is_ok() && is_tactical(pos, best)) {
            update_stat(cap_hist[pos.moved_piece(best)][best.to_sq()][cap_or_promo_pt(pos, best)], bonus);
        }
        for (int i = 0; i < cc; ++i) if (caps[i] != best && is_tactical(pos, caps[i])) {
            update_stat(cap_hist[pos.moved_piece(caps[i])][caps[i].to_sq()][cap_or_promo_pt(pos, caps[i])], -bonus);
        }
    }

    int correct_eval(const SF::Position& pos, int raw, PrevMove p1 = {}, PrevMove p2 = {}) const {
        SF::Color c = pos.side_to_move();
        constexpr U64 m = CORR_SIZE - 1;
        int corr = corr_hist[c][pos.pawn_key() & m] * 10 + minor_corr[c][pos.minor_piece_key() & m] * 6
                 + (non_pawn_corr[c][0][pos.non_pawn_key(SF::WHITE) & m] +
                    non_pawn_corr[c][1][pos.non_pawn_key(SF::BLACK) & m]) * 7;
        if (p1.ok() && p2.ok()) corr += cont_corr[p1.pc][p1.to][p2.pc][p2.to] * 6;
        return std::clamp(raw + corr / 1024, -MATE_IN_MAX + 1, MATE_IN_MAX - 1);
    }

    void update_correction(const SF::Position& pos, int depth, int diff, PrevMove p1 = {}, PrevMove p2 = {}) {
        SF::Color c = pos.side_to_move();
        constexpr U64 m = CORR_SIZE - 1;
        int bonus = std::clamp(diff * depth / 4, -512, 512);
        update_stat(corr_hist[c][pos.pawn_key() & m], bonus);
        update_stat(minor_corr[c][pos.minor_piece_key() & m], bonus);
        update_stat(non_pawn_corr[c][0][pos.non_pawn_key(SF::WHITE) & m], bonus);
        update_stat(non_pawn_corr[c][1][pos.non_pawn_key(SF::BLACK) & m], bonus);
        if (p1.ok() && p2.ok()) update_stat(cont_corr[p1.pc][p1.to][p2.pc][p2.to], bonus);
    }
};

struct ScoredMove { SF::Move move; int score; };
enum PickerStage : int { STAGE_TT=0, STAGE_GEN_CAPS, STAGE_GOOD_CAPS, STAGE_KILLER_1, STAGE_KILLER_2, STAGE_COUNTER, STAGE_GEN_QUIETS, STAGE_QUIETS, STAGE_BAD_CAPS, STAGE_DONE };

struct MovePicker {
    const SF::Position& pos;
    const SearchHistory& hist;
    SF::Move tt_move, k1 = SF::Move::none(), k2 = SF::Move::none(), cm = SF::Move::none();
    PrevMove prev1, prev2, prev4, prev6;
    int ply = 0;
    bool tactical_only = false;
    int stage = STAGE_TT;
    int last_score = 0;
    bool has_last_score = false;
    U64& opp_threats;
    bool& threats_ready;
    ScoredMove good_caps[MAX_MOVES], quiets[MAX_MOVES], bad_caps[64];
    int gc_cnt = 0, gc_idx = 0, bc_cnt = 0, bc_idx = 0, q_cnt = 0, q_idx = 0;

    MovePicker(const SF::Position& p_pos, const SearchHistory& h, SF::Move ttm, int p,
               PrevMove p1, PrevMove p2, PrevMove p4, PrevMove p6,
               U64& ot, bool& tr, bool tac = false)
        : pos(p_pos), hist(h), tt_move(ttm), prev1(p1), prev2(p2), prev4(p4), prev6(p6),
          ply(p), tactical_only(tac), opp_threats(ot), threats_ready(tr) {
        if (!tac && ply < MAX_PLY) {
            k1 = hist.killers[ply][0];
            k2 = hist.killers[ply][1];
            if (p1.ok()) cm = hist.countermoves[p1.pc][p1.to];
        }
    }

    static inline ScoredMove pick_best(ScoredMove* arr, int idx, int count) {
        int bi = idx;
        for (int i = idx + 1; i < count; ++i) if (arr[i].score > arr[bi].score) bi = i;
        std::swap(arr[idx], arr[bi]);
        return arr[idx];
    }

    SF::Move next() {
        has_last_score = false;
        while (stage != STAGE_DONE) {
            switch (stage) {
                case STAGE_TT:
                    stage = STAGE_GEN_CAPS;
                    if (tt_move.is_ok() && (!tactical_only || is_tactical(pos, tt_move)) && is_legal_move(pos, tt_move))
                        return tt_move;
                    break;
                case STAGE_GEN_CAPS: {
                    auto add_cap = [&](SF::Move m) {
                        if (m == tt_move) return;
                        SF::PieceType pt = cap_or_promo_pt(pos, m);
                        int sc = SEE_VAL[pt] * 16 - int(SF::type_of(pos.moved_piece(m)))
                               + hist.cap_hist[pos.moved_piece(m)][m.to_sq()][pt];
                        good_caps[gc_cnt++] = {m, sc};
                    };
                    if (pos.checkers()) {
                        for (const auto& m : SF::MoveList<SF::EVASIONS>(pos))
                            if (pos.legal(m) && is_tactical(pos, m)) add_cap(m);
                    } else {
                        for (const auto& m : SF::MoveList<SF::CAPTURES>(pos)) add_cap(m);
                    }
                    stage = STAGE_GOOD_CAPS;
                    break;
                }
                case STAGE_GOOD_CAPS:
                    while (gc_idx < gc_cnt) {
                        ScoredMove sm = pick_best(good_caps, gc_idx++, gc_cnt);
                        if (tactical_only || see_ge_cp(pos, sm.move, -108)) return sm.move;
                        if (bc_cnt < 64) bad_caps[bc_cnt++] = sm;
                    }
                    stage = tactical_only ? STAGE_DONE : STAGE_KILLER_1;
                    break;
                case STAGE_KILLER_1:
                    stage = STAGE_KILLER_2;
                    if (k1.is_ok() && k1 != tt_move && !is_tactical(pos, k1) && is_legal_move(pos, k1)) return k1;
                    break;
                case STAGE_KILLER_2:
                    stage = STAGE_COUNTER;
                    if (k2.is_ok() && k2 != tt_move && k2 != k1 && !is_tactical(pos, k2) && is_legal_move(pos, k2)) return k2;
                    break;
                case STAGE_COUNTER:
                    stage = STAGE_GEN_QUIETS;
                    if (cm.is_ok() && cm != tt_move && cm != k1 && cm != k2 && !is_tactical(pos, cm) && is_legal_move(pos, cm)) return cm;
                    break;
                case STAGE_GEN_QUIETS: {
                    if (!threats_ready) { opp_threats = pos.threats_by(SF::ALL_PIECES); threats_ready = true; }
                    auto add_q = [&](SF::Move m) {
                        if (m == tt_move || m == k1 || m == k2 || m == cm) return;
                        quiets[q_cnt++] = {m, hist.get_quiet_score(pos, opp_threats, m, ply, prev1, prev2, prev4, prev6)};
                    };
                    if (pos.checkers()) {
                        for (const auto& m : SF::MoveList<SF::EVASIONS>(pos))
                            if (pos.legal(m) && !is_tactical(pos, m)) add_q(m);
                    } else {
                        for (const auto& m : SF::MoveList<SF::QUIETS>(pos)) add_q(m);
                    }
                    stage = STAGE_QUIETS;
                    break;
                }
                case STAGE_QUIETS:
                    if (q_idx < q_cnt) {
                        ScoredMove sm = pick_best(quiets, q_idx++, q_cnt);
                        last_score = sm.score;
                        has_last_score = true;
                        return sm.move;
                    }
                    stage = STAGE_BAD_CAPS;
                    break;
                case STAGE_BAD_CAPS:
                    if (bc_idx < bc_cnt) return bad_caps[bc_idx++].move;
                    stage = STAGE_DONE;
                    break;
                default: stage = STAGE_DONE; break;
            }
        }
        return SF::Move::none();
    }
};// ============================================================================
// SearchWorker (PVS, QSearch, LMR, Pruning)
// ============================================================================
inline int LMR_TABLE[MAX_PLY][MAX_MOVES];
inline void init_lmr() {
    for (int d = 0; d < MAX_PLY; ++d)
        for (int m = 0; m < MAX_MOVES; ++m)
            LMR_TABLE[d][m] = (d == 0 || m == 0) ? 0
                : int(1024.0 * ((TUNE.lmr_base / 100.0) + std::log(double(d)) * std::log(double(m)) / (TUNE.lmr_div / 100.0)));
}

struct SearchStack {
    SF::Move pv[MAX_PLY];
    int pv_len = 0;
    PrevMove cur_move;
    SF::Move excluded_move = SF::Move::none();
    int static_eval = VAL_NONE;
    int double_extensions = 0;
    int cutoff_cnt = 0;
    int reduction = 0;
};

struct SearchLimits {
    int depth = MAX_PLY - 1, movetime = 0, wtime = 0, btime = 0, winc = 0, binc = 0, movestogo = 0;
    U64 max_nodes = 0ULL;
    bool infinite = false;
    int64_t soft_limit_ms = 0, hard_limit_ms = 0;
};

struct alignas(64) SearchWorker {
    int thread_id = 0;
    SF::Position pos;
    SF::StateInfo st_stack[1024];
    int sp = 0;
    SF::Eval::NNUE::AccumulatorStack acc_stack;
    std::unique_ptr<SF::Eval::NNUE::AccumulatorCaches> caches;
    SearchHistory hist;
    SearchStack ss[MAX_PLY + 8];
    U64 nodes = 0ULL;
    U64 root_move_nodes[64][64] = {{0ULL}};
    int seldepth = 0, nmp_min_ply = 0;
    int optimism[2] = {0, 0};
    SF::Move best_root_move = SF::Move::none();
    int best_root_score = 0;
    int completed_depth = 0;
    int16_t tt_move_hist = 0;
    std::atomic<bool>* stop_flag = nullptr;
    std::chrono::steady_clock::time_point start_time;
    SearchLimits limits;

    SearchWorker() { hist.clear(); }

    void set_position(const std::string& fen, const std::vector<SF::Move>& moves) {
        if (!caches && g_net) caches = std::make_unique<SF::Eval::NNUE::AccumulatorCaches>(*g_net);
        sp = 0;
        st_stack[0] = SF::StateInfo{};
        pos.set(fen, false, &st_stack[0]);
        for (SF::Move m : moves) {
            sp++;
            pos.do_move(m, st_stack[sp]);
        }
        acc_stack.reset();
    }

    inline void make_move(SF::Move m) {
        sp++;
        SF::Dirties& dirties = acc_stack.push(pos);
        pos.do_move(m, st_stack[sp], pos.gives_check(m), dirties);
    }
    inline void unmake_move(SF::Move m) {
        pos.undo_move(m);
        acc_stack.pop();
        sp--;
    }
    inline void make_null() { sp++; pos.do_null_move(st_stack[sp]); }
    inline void unmake_null() { pos.undo_null_move(); sp--; }

    inline int64_t elapsed_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
    }
    inline bool check_stop() {
        if (stop_flag->load(std::memory_order_relaxed)) return true;
        if (limits.max_nodes && nodes >= limits.max_nodes) { stop_flag->store(true, std::memory_order_relaxed); return true; }
        if (thread_id == 0 && !limits.infinite && (nodes & 1023ULL) == 0ULL && elapsed_ms() >= limits.hard_limit_ms) {
            stop_flag->store(true, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    int qsearch(int alpha, int beta, int ply) {
        if (check_stop()) return 0;
        nodes++;
        if (ply > seldepth) seldepth = ply;
        if (pos.is_draw(ply)) return 0;
        if (ply >= MAX_PLY - 2) return eval_pos(pos, acc_stack, *caches, optimism[pos.side_to_move()]);

        bool pv_node = (beta - alpha > 1);
        bool in_check = (pos.checkers() != 0ULL);
        TTData tt = TT.probe(pos.key(), ply);
        bool tt_pv = pv_node || (tt.hit && tt.pv);
        bool tt_capture = tt.move.is_ok() && is_tactical(pos, tt.move);
        if (!pv_node && tt.hit && tt.score != VAL_NONE) {
            if (tt.bound == BOUND_EXACT || (tt.bound == BOUND_LOWER && tt.score >= beta) || (tt.bound == BOUND_UPPER && tt.score <= alpha))
                return tt.score;
        }

        int best_score = -INF_SCORE, raw_eval = VAL_NONE;
        PrevMove p1 = (ply >= 1) ? ss[ply - 1].cur_move : PrevMove{};
        PrevMove p2 = (ply >= 2) ? ss[ply - 2].cur_move : PrevMove{};
        PrevMove p4 = (ply >= 4) ? ss[ply - 4].cur_move : PrevMove{};
        PrevMove p6 = (ply >= 6) ? ss[ply - 6].cur_move : PrevMove{};

        if (!in_check) {
            raw_eval = (tt.hit && tt.eval != VAL_NONE) ? tt.eval : eval_pos(pos, acc_stack, *caches, optimism[pos.side_to_move()]);
            int stand_pat = hist.correct_eval(pos, raw_eval, p1, p2);
            best_score = stand_pat;
            if (stand_pat >= beta) return stand_pat;
            if (stand_pat > alpha) alpha = stand_pat;
        }

        U64 opp_threats = 0ULL;
        bool threats_ready = false;
        MovePicker mp(pos, hist, tt.move, ply, p1, p2, p4, p6, opp_threats, threats_ready, !in_check);

        SF::Move best_move = SF::Move::none();
        Bound bound = BOUND_UPPER;
        int move_count = 0;

        while (true) {
            SF::Move m = mp.next();
            if (!m.is_ok()) break;
            if (!in_check) {
                int gain = SEE_VAL[cap_or_promo_pt(pos, m)] + (m.type_of() == SF::PROMOTION ? SEE_VAL[m.promotion_type()] : 0) + 180;
                if (best_score + gain < alpha) continue;
            }
            if (!pos.legal(m)) continue;
            if (!in_check && !see_ge_cp(pos, m, (!pv_node && best_score + 140 <= alpha) ? 1 : 0)) continue;

            move_count++;
            ss[ply].cur_move = {pos.moved_piece(m), m.to_sq()};
            make_move(m);
            int score = -qsearch(-beta, -alpha, ply + 1);
            unmake_move(m);
            if (stop_flag->load(std::memory_order_relaxed)) return 0;

            if (score > best_score) {
                best_score = score;
                best_move = m;
                if (score > alpha) {
                    alpha = score;
                    bound = BOUND_EXACT;
                    if (score >= beta) { bound = BOUND_LOWER; break; }
                }
            }
        }
        if (in_check && move_count == 0) return -MATE_SCORE + ply;
        TT.store(pos.key(), (bound == BOUND_UPPER ? SF::Move::none() : best_move), best_score, raw_eval, 0, bound, ply, tt_pv);
        return best_score;
    }

    int pvs(int alpha, int beta, int depth, int ply, bool cutnode) {
        ss[ply].pv_len = 0;
        if (check_stop()) return 0;
        bool root_node = (ply == 0), pv_node = (beta - alpha > 1), all_node = !pv_node && !cutnode;
        if (ply > seldepth) seldepth = ply;

        if (!root_node) {
            if (pos.is_draw(ply)) return 0;
            if (ply >= MAX_PLY - 2) return eval_pos(pos, acc_stack, *caches, optimism[pos.side_to_move()]);
            alpha = std::max(alpha, -MATE_SCORE + ply);
            beta  = std::min(beta,   MATE_SCORE - ply - 1);
            if (alpha >= beta) return alpha;
        }
        if (depth <= 0) return qsearch(alpha, beta, ply);
        nodes++;

        bool in_check = (pos.checkers() != 0ULL);
        SF::Move excluded = ss[ply].excluded_move;
        TTData tt = excluded.is_ok() ? TTData{} : TT.probe(pos.key(), ply);
        bool tt_pv = pv_node || (tt.hit && tt.pv);

        if (!pv_node && tt.hit && tt.depth >= depth && tt.score != VAL_NONE) {
            if (tt.bound == BOUND_EXACT || (tt.bound == BOUND_LOWER && tt.score >= beta) || (tt.bound == BOUND_UPPER && tt.score <= alpha))
                return tt.score;
        }
        if (root_node && best_root_move.is_ok()) tt.move = best_root_move;
        if (!all_node && depth >= 6 && !tt.move.is_ok() && !excluded.is_ok()) depth--;

        PrevMove p1 = (ply >= 1) ? ss[ply - 1].cur_move : PrevMove{};
        PrevMove p2 = (ply >= 2) ? ss[ply - 2].cur_move : PrevMove{};
        PrevMove p4 = (ply >= 4) ? ss[ply - 4].cur_move : PrevMove{};
        PrevMove p6 = (ply >= 6) ? ss[ply - 6].cur_move : PrevMove{};

        int raw_eval = VAL_NONE, static_eval = VAL_NONE, eval = VAL_NONE;
        bool improving = false;
        bool opp_worsening = false;
        if (in_check) {
            ss[ply].static_eval = VAL_NONE;
        } else if (excluded.is_ok()) {
            static_eval = eval = ss[ply].static_eval;
        } else {
            raw_eval = (tt.hit && tt.eval != VAL_NONE) ? tt.eval : eval_pos(pos, acc_stack, *caches, optimism[pos.side_to_move()]);
            static_eval = hist.correct_eval(pos, raw_eval, p1, p2);
            ss[ply].static_eval = static_eval;
            eval = static_eval;
            if (tt.hit && tt.score != VAL_NONE &&
                (tt.bound == BOUND_EXACT || (tt.bound == BOUND_LOWER && tt.score > eval) || (tt.bound == BOUND_UPPER && tt.score < eval)))
                eval = tt.score;
            if (ply >= 2 && ss[ply - 2].static_eval != VAL_NONE) improving = (static_eval > ss[ply - 2].static_eval);
            else if (ply >= 4 && ss[ply - 4].static_eval != VAL_NONE) improving = (static_eval > ss[ply - 4].static_eval);
            if (ply >= 1 && ss[ply - 1].static_eval != VAL_NONE) {
                opp_worsening = (static_eval + ss[ply - 1].static_eval > 0);
                if (ss[ply - 1].reduction >= 2 && depth >= 2 && static_eval + ss[ply - 1].static_eval > 166) depth--;
            }
        }

        if (!pv_node && !in_check && !excluded.is_ok()) {
            if (all_node && eval + 342 * depth < alpha) {
                int r_score = qsearch(alpha, beta, ply);
                if (r_score <= alpha) return r_score;
            }

            bool tt_capture = tt.move.is_ok() && is_tactical(pos, tt.move);
            int rfp_mult = std::min(45 + depth * 4, TUNE.rfp_margin);
            int rfp_margin = depth * rfp_mult - (improving ? TUNE.rfp_improving : 0);
            if (!tt_pv && depth <= 14 && eval - rfp_margin >= beta && (!tt.move.is_ok() || tt_capture) && std::abs(beta) < MATE_IN_MAX) return eval;

            if (depth >= 3 && ply >= nmp_min_ply && eval >= beta && p1.ok() && pos.non_pawn_material(pos.side_to_move()) != 0) {
                int R = 3 + depth / 3 + std::min(3, (eval - beta) / 190);
                ss[ply].cur_move = {};
                make_null();
                int null_score = -pvs(-beta, -beta + 1, depth - R, ply + 1, !cutnode);
                unmake_null();
                if (stop_flag->load(std::memory_order_relaxed)) return 0;
                if (null_score >= beta) {
                    if (null_score >= MATE_IN_MAX) null_score = beta;
                    if (depth < 12) return null_score;
                    nmp_min_ply = ply + 3 * (depth - R) / 4;
                    int v_score = pvs(beta - 1, beta, depth - R, ply, false);
                    nmp_min_ply = 0;
                    if (v_score >= beta) return null_score;
                }
            }

            int prob_beta = beta + TUNE.probcut_margin + 51 - 64 * int(improving);
            if (depth >= 3 && std::abs(beta) < MATE_IN_MAX && (!tt.hit || tt.depth + 3 < depth || tt.score >= prob_beta)) {
                U64 ot = 0ULL; bool tr = false;
                MovePicker pc_mp(pos, hist, tt.move, ply, p1, p2, p4, p6, ot, tr, true);
                while (true) {
                    SF::Move m = pc_mp.next();
                    if (!m.is_ok()) break;
                    if (!see_ge_cp(pos, m, prob_beta - eval) || !pos.legal(m)) continue;
                    ss[ply].cur_move = {pos.moved_piece(m), m.to_sq()};
                    make_move(m);
                    int q_val = -qsearch(-prob_beta, -prob_beta + 1, ply + 1);
                    if (q_val >= prob_beta) q_val = -pvs(-prob_beta, -prob_beta + 1, depth - 4, ply + 1, !cutnode);
                    unmake_move(m);
                    if (stop_flag->load(std::memory_order_relaxed)) return 0;
                    if (q_val >= prob_beta) {
                        TT.store(pos.key(), m, q_val, raw_eval, depth - 3, BOUND_LOWER, ply);
                        return q_val;
                    }
                }
            }
        }

        U64 opp_threats = 0ULL;
        bool threats_ready = false;
        MovePicker mp(pos, hist, tt.move, ply, p1, p2, p4, p6, opp_threats, threats_ready, false);

        SF::Move quiets_tried[MAX_MOVES], caps_tried[MAX_MOVES];
        int q_tried = 0, c_tried = 0, best_score = -INF_SCORE, move_count = 0;
        SF::Move best_move = SF::Move::none();
        Bound bound = BOUND_UPPER;
        ss[ply + 1].cutoff_cnt = 0;

        while (true) {
            SF::Move m = mp.next();
            if (!m.is_ok()) break;
            if (m == excluded) continue;

            bool is_quiet = !is_tactical(pos, m);
            if (is_quiet && !threats_ready) { opp_threats = pos.threats_by(SF::ALL_PIECES); threats_ready = true; }
            int hist_score = is_quiet ? (mp.has_last_score ? mp.last_score : hist.get_quiet_score(pos, opp_threats, m, ply, p1, p2, p4, p6)) : 0;

            if (!root_node && best_score > -MATE_IN_MAX) {
                int lmp_limit = (3 + depth * depth) / (2 - int(improving));
                if (is_quiet) {
                    if (depth <= 8 && move_count >= lmp_limit) continue;
                    int lmr_depth = std::max(0, depth - (LMR_TABLE[std::min(depth, MAX_PLY - 1)][std::min(move_count + 1, MAX_MOVES - 1)] >> 10));
                    if (!in_check && lmr_depth <= 6 && eval + TUNE.fut_base + TUNE.fut_mult * lmr_depth + hist_score / 32 <= alpha) continue;
                    if (!in_check && depth <= 4 && hist_score < -3200 * depth) continue;
                }
                int see_margin = is_quiet ? (-TUNE.see_quiet * depth) : (-TUNE.see_cap * depth * depth);
                if (depth <= 8 && !see_ge_cp(pos, m, see_margin)) continue;
            }
            if (!pos.legal(m)) continue;

            int extension = 0;
            if (!root_node && depth >= 7 && m == tt.move && !excluded.is_ok() &&
                tt.depth >= depth - 3 && (tt.bound & BOUND_LOWER) && std::abs(tt.score) < MATE_IN_MAX) {
                int s_beta = tt.score - 2 * depth, s_depth = (depth - 1) / 2;
                ss[ply].excluded_move = m;
                int s_score = pvs(s_beta - 1, s_beta, s_depth, ply, cutnode);
                ss[ply].excluded_move = SF::Move::none();
                if (s_score < s_beta) {
                    extension = 1;
                    if (!pv_node && s_score < s_beta - 24 && ss[ply].double_extensions <= 6) extension = 2;
                } else if (s_beta >= beta) return s_beta;
                else if (tt.score >= beta || cutnode) extension = -1;
            } else if (in_check && (depth <= 6 || (depth <= 12 && see_ge_cp(pos, m, 0)))) {
                extension = 1;
            }
            ss[ply + 1].double_extensions = ss[ply].double_extensions + (extension == 2 ? 1 : 0);

            U64 nodes_before = nodes;
            move_count++;
            SF::Piece moved_pc = pos.moved_piece(m);
            SF::PieceType cpt = is_quiet ? SF::NO_PIECE_TYPE : cap_or_promo_pt(pos, m);
            ss[ply].cur_move = {moved_pc, m.to_sq()};
            TT.prefetch(pos.prefetch_key(m));
            make_move(m);

            int new_depth = depth - 1 + extension, score = 0;
            if (depth >= 2 && move_count > 1 + int(root_node)) {
                int r = LMR_TABLE[std::min(depth, MAX_PLY - 1)][std::min(move_count, MAX_MOVES - 1)];
                if (is_quiet) {
                    if (!tt_pv) r += 1024;
                    if (opp_worsening) r -= 256;
                    if (tt_move_hist < 0) r += 512;
                    if (cutnode) r += 1280;
                    if (!improving) r += 768;
                    if (tt.move.is_ok() && is_tactical(pos, tt.move)) r += 1024;
                    if (ss[ply + 1].cutoff_cnt > 3) r += 1024;
                    if (pos.checkers() != 0ULL) r -= 1024;
                    r -= std::clamp(hist_score / 14, -2048, 2048);
                } else {
                    r = r / 2;
                    if (!improving) r += 512;
                    if (cutnode) r += 512;
                    if (pos.checkers() != 0ULL) r -= 1024;
                    r -= std::clamp(int(hist.cap_hist[moved_pc][m.to_sq()][cpt]) / 16, -1024, 1024);
                }
                int red = std::max(0, r >> 10);
                int d = std::clamp(new_depth - red, 1, new_depth);
                ss[ply].reduction = new_depth - d;
                score = -pvs(-alpha - 1, -alpha, d, ply + 1, true);
                ss[ply].reduction = 0;
                if (score > alpha && d < new_depth) {
                    bool do_deeper = score > (best_score + 55 + 2 * new_depth);
                    bool do_shallower = score < (best_score + new_depth);
                    int re_depth = new_depth + int(do_deeper) - int(do_shallower);
                    if (re_depth > d) score = -pvs(-alpha - 1, -alpha, re_depth, ply + 1, !cutnode);
                }
            } else if (!pv_node || move_count > 1) {
                score = -pvs(-alpha - 1, -alpha, new_depth, ply + 1, !cutnode);
            }
            if (pv_node && (move_count == 1 || score > alpha))
                score = -pvs(-beta, -alpha, new_depth, ply + 1, false);

            unmake_move(m);
            if (stop_flag->load(std::memory_order_relaxed)) return 0;
            if (root_node) root_move_nodes[m.from_sq()][m.to_sq()] += (nodes - nodes_before);

            if (score > best_score) {
                best_score = score;
                best_move = m;
                if (root_node && (score > alpha || depth == 1)) { best_root_move = m; best_root_score = score; }
                if (score > alpha) {
                    alpha = score;
                    bound = BOUND_EXACT;
                    ss[ply].pv[0] = m;
                    std::memcpy(&ss[ply].pv[1], &ss[ply + 1].pv[0], ss[ply + 1].pv_len * sizeof(SF::Move));
                    ss[ply].pv_len = ss[ply + 1].pv_len + 1;
                    if (score >= beta) { bound = BOUND_LOWER; ss[ply].cutoff_cnt++; break; }
                }
            }
            if (is_quiet && q_tried < MAX_MOVES) quiets_tried[q_tried++] = m;
            else if (!is_quiet && c_tried < MAX_MOVES) caps_tried[c_tried++] = m;
        }

        if (move_count == 0) return excluded.is_ok() ? alpha : (in_check ? (-MATE_SCORE + ply) : 0);

        if (bound == BOUND_UPPER && p1.ok() && ply >= 1) {
            int bonus = std::min(1400, depth * depth * 12 + 32);
            if (p2.ok()) update_stat(hist.cont_hist[p2.pc][p2.to][p1.pc][p1.to], bonus);
            if (p4.ok()) update_stat(hist.cont_hist[p4.pc][p4.to][p1.pc][p1.to], bonus / 2);
        }
        if (bound == BOUND_LOWER && best_move.is_ok()) {
            if (tt.move.is_ok()) update_stat(tt_move_hist, best_move == tt.move ? 512 : -512);
            if (!is_tactical(pos, best_move)) {
                if (hist.killers[ply][0] != best_move) { hist.killers[ply][1] = hist.killers[ply][0]; hist.killers[ply][0] = best_move; }
                if (p1.ok()) hist.countermoves[p1.pc][p1.to] = best_move;
                if (!threats_ready) opp_threats = pos.threats_by(SF::ALL_PIECES);
                hist.update_quiet_histories(pos, opp_threats, best_move, quiets_tried, q_tried, depth, ply, p1, p2, p4, p6);
            }
            hist.update_capture_histories(pos, best_move, caps_tried, c_tried, depth);
        }

        if (!in_check && !excluded.is_ok() && std::abs(best_score) < MATE_IN_MAX &&
            (!best_move.is_ok() || !is_tactical(pos, best_move)) &&
            (bound == BOUND_EXACT || (bound == BOUND_LOWER && best_score > static_eval) || (bound == BOUND_UPPER && best_score < static_eval)))
            hist.update_correction(pos, depth, best_score - static_eval, p1, p2);

        if (!excluded.is_ok())
            TT.store(pos.key(), (bound == BOUND_UPPER ? SF::Move::none() : best_move), best_score, raw_eval, depth, bound, ply, tt_pv);

        return best_score;
    }
};// ============================================================================
// ThreadPool & UCI
// ============================================================================
inline std::string move_to_uci(SF::Move m) {
    if (!m.is_ok()) return "0000";
    SF::Square from = m.from_sq(), to = m.to_sq();
    if (m.type_of() == SF::CASTLING)
        to = SF::make_square(to > from ? SF::FILE_G : SF::FILE_C, SF::rank_of(from));
    std::string s;
    s += char('a' + SF::file_of(from)); s += char('1' + SF::rank_of(from));
    s += char('a' + SF::file_of(to));   s += char('1' + SF::rank_of(to));
    if (m.type_of() == SF::PROMOTION) {
        const char* p = "  nbrq";
        s += p[m.promotion_type()];
    }
    return s;
}

inline SF::Move parse_uci_move(const SF::Position& pos, const std::string& str) {
    if (pos.checkers()) {
        for (const auto& m : SF::MoveList<SF::EVASIONS>(pos))
            if (pos.legal(m) && move_to_uci(m) == str) return m;
    } else {
        for (const auto& m : SF::MoveList<SF::ALL>(pos))
            if (pos.legal(m) && move_to_uci(m) == str) return m;
    }
    return SF::Move::none();
}

class ThreadPool {
public:
    std::vector<std::unique_ptr<SearchWorker>> workers;
    std::vector<std::thread> threads;
    std::atomic<bool> stop_flag{false};
    int num_threads = 1;

    ThreadPool() { set_threads(1); }
    void set_threads(int n) {
        num_threads = std::clamp(n, 1, 256);
        workers.clear();
        for (int i = 0; i < num_threads; ++i) {
            auto w = std::make_unique<SearchWorker>();
            w->thread_id = i;
            w->stop_flag = &stop_flag;
            workers.push_back(std::move(w));
        }
    }
    void clear_histories() { for (auto& w : workers) w->hist.clear(); }
    U64 total_nodes() const { U64 s = 0; for (const auto& w : workers) s += w->nodes; return s; }

    void run_worker(SearchWorker* w) {
        int prev_score = 0, stability = 0;
        SF::Move prev_best = SF::Move::none();
        std::vector<SF::Move> root_legals;
        if (w->pos.checkers()) {
            for (const auto& m : SF::MoveList<SF::EVASIONS>(w->pos)) if (w->pos.legal(m)) root_legals.push_back(m);
        } else {
            for (const auto& m : SF::MoveList<SF::ALL>(w->pos)) if (w->pos.legal(m)) root_legals.push_back(m);
        }
        if (root_legals.empty()) return;
        w->best_root_move = root_legals[0];

        for (int depth = 1 + (w->thread_id % 2); depth <= w->limits.depth; ++depth) {
            w->seldepth = 0;
            int delta = 14 + (w->thread_id % 4) * 3;
            int alpha = -INF_SCORE, beta = INF_SCORE;
            if (depth >= 5) { alpha = std::max(-INF_SCORE, prev_score - delta); beta = std::min(INF_SCORE, prev_score + delta); }
            int score = 0, asp_depth = depth;
            while (true) {
                score = w->pvs(alpha, beta, asp_depth, 0, false);
                if (stop_flag.load(std::memory_order_relaxed)) break;
                if (score <= alpha) { beta = (alpha + beta) / 2; alpha = std::max(-INF_SCORE, score - delta); asp_depth = depth; }
                else if (score >= beta) { beta = std::min(INF_SCORE, score + delta); asp_depth = std::max(1, asp_depth - 1); }
                else break;
                delta += delta / 2 + 4;
            }
            if (stop_flag.load(std::memory_order_relaxed)) break;

            int score_drop = (depth >= 6) ? (prev_score - score) : 0;
            prev_score = score;
            w->completed_depth = depth;
            int opt = 135 * prev_score / (std::abs(prev_score) + 185);
            w->optimism[w->pos.side_to_move()] = opt;
            w->optimism[~w->pos.side_to_move()] = -opt;

            if (w->best_root_move == prev_best) stability = std::min(10, stability + 1);
            else { stability = 0; prev_best = w->best_root_move; }

            if (w->thread_id == 0) {
                int64_t ms = std::max<int64_t>(1, w->elapsed_ms());
                U64 tot = total_nodes(), nps = (tot * 1000ULL) / ms;
                std::cout << "info depth " << depth << " seldepth " << w->seldepth;
                if (std::abs(score) >= MATE_IN_MAX)
                    std::cout << " score mate " << (score > 0 ? (MATE_SCORE - std::abs(score) + 1) / 2 : -(MATE_SCORE - std::abs(score) + 1) / 2);
                else
                    std::cout << " score cp " << score;
                std::cout << " nodes " << tot << " nps " << nps << " time " << ms << " pv";
                for (int i = 0; i < w->ss[0].pv_len; ++i) std::cout << " " << move_to_uci(w->ss[0].pv[i]);
                if (w->ss[0].pv_len == 0 && w->best_root_move.is_ok()) std::cout << " " << move_to_uci(w->best_root_move);
                std::cout << std::endl;

                if (!w->limits.infinite && w->limits.soft_limit_ms > 0 && depth >= 6) {
                    SF::Move bm = w->best_root_move;
                    double frac = double(w->root_move_nodes[bm.from_sq()][bm.to_sq()]) / double(std::max<U64>(1ULL, w->nodes));
                    double falling = std::clamp(1.0 + 0.012 * score_drop, 0.75, 1.55);
                    double scale = (1.35 - 0.65 * frac) * (1.15 - 0.04 * stability) * falling;
                    if (ms >= int64_t(w->limits.soft_limit_ms * std::clamp(scale, 0.50, 2.00)) || root_legals.size() == 1) {
                        stop_flag.store(true, std::memory_order_relaxed);
                        break;
                    }
                }
            }
        }
    }

    void start_search(const std::string& fen, const std::vector<SF::Move>& moves, SF::Color stm, SearchLimits lim) {
        stop_flag.store(false, std::memory_order_relaxed);
        TT.new_search();
        if (lim.movetime > 0) { lim.soft_limit_ms = 0; lim.hard_limit_ms = lim.movetime; }
        else {
            int tl = (stm == SF::WHITE) ? lim.wtime : lim.btime;
            int inc = (stm == SF::WHITE) ? lim.winc : lim.binc;
            if (tl > 0) {
                int mtg = lim.movestogo > 0 ? lim.movestogo : 28;
                int avail = std::max(1, tl - 15);
                lim.soft_limit_ms = std::max<int64_t>(1, avail / mtg + (inc * 3) / 4);
                lim.hard_limit_ms = std::max<int64_t>(2, std::min<int64_t>(avail * 4 / 10, lim.soft_limit_ms * 4));
            } else lim.infinite = true;
        }
        auto now = std::chrono::steady_clock::now();
        for (int i = 0; i < num_threads; ++i) {
            workers[i]->set_position(fen, moves);
            workers[i]->nodes = 0ULL;
            workers[i]->completed_depth = 0;
            workers[i]->best_root_score = -INF_SCORE;
            std::memset(workers[i]->root_move_nodes, 0, sizeof(workers[i]->root_move_nodes));
            workers[i]->start_time = now;
            workers[i]->limits = lim;
        }
        threads.clear();
        for (int i = 1; i < num_threads; ++i) threads.emplace_back(&ThreadPool::run_worker, this, workers[i].get());
        run_worker(workers[0].get());
        stop_flag.store(true, std::memory_order_relaxed);
        for (auto& t : threads) if (t.joinable()) t.join();

        SF::Move best = workers[0]->best_root_move;
        if (num_threads > 1) {
            int min_sc = INF_SCORE;
            for (const auto& w : workers) if (w->completed_depth > 0) min_sc = std::min(min_sc, w->best_root_score);
            int64_t best_vote = -1;
            for (const auto& w : workers) {
                if (w->completed_depth <= 0 || !w->best_root_move.is_ok()) continue;
                int64_t vote = 0;
                for (const auto& o : workers) {
                    if (o->completed_depth > 0 && o->best_root_move == w->best_root_move) {
                        int64_t wt = int64_t(o->best_root_score - min_sc + 14) * o->completed_depth;
                        if (o->thread_id == 0) wt += wt / 4;
                        vote += wt;
                    }
                }
                if (vote > best_vote) { best_vote = vote; best = w->best_root_move; }
            }
        }
        std::cout << "bestmove " << move_to_uci(best) << std::endl;
    }
};

} // namespace V8

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    V8::init_lmr();
    V8::init_engine();

    const std::string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    std::string cur_fen = START_FEN;
    std::vector<Stockfish::Move> cur_moves;
    Stockfish::Position root_pos;
    Stockfish::StateInfo root_st[1024];
    int root_sp = 0;
    root_pos.set(cur_fen, false, &root_st[0]);

    V8::ThreadPool pool;
    std::thread search_thread;
    auto join_search = [&]() {
        pool.stop_flag.store(true, std::memory_order_relaxed);
        if (search_thread.joinable()) search_thread.join();
    };

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream ss(line);
        std::string token;
        if (!(ss >> token)) continue;
        for (char& c : token) c = char(std::tolower((unsigned char)c));

        if (token == "uci") {
            std::cout << "id name V8-Native\n"
                      << "id author Tejdeep\n"
                      << "option name Hash type spin default 32 min 1 max 4096\n"
                      << "option name Threads type spin default 1 min 1 max 256\n"
                      << "option name rfp_margin type spin default 74 min 40 max 130\n"
                      << "option name rfp_improving type spin default 55 min 15 max 110\n"
                      << "option name razor_margin type spin default 260 min 150 max 400\n"
                      << "option name probcut_margin type spin default 190 min 100 max 320\n"
                      << "option name fut_base type spin default 115 min 50 max 220\n"
                      << "option name fut_mult type spin default 95 min 40 max 180\n"
                      << "option name see_quiet type spin default 65 min 25 max 130\n"
                      << "option name see_cap type spin default 28 min 10 max 70\n"
                      << "option name lmr_base type spin default 75 min 30 max 140\n"
                      << "option name lmr_div type spin default 215 min 130 max 320\n"
                      << "uciok" << std::endl;
        } else if (token == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (token == "setoption") {
            join_search();
            std::string name_tok, opt_name, val_tok, opt_val;
            ss >> name_tok >> opt_name >> val_tok;
            for (char& c : opt_name) c = char(std::tolower((unsigned char)c));
            std::getline(ss >> std::ws, opt_val);
            if (opt_name == "hash") V8::TT.resize(std::stoull(opt_val));
            else if (opt_name == "threads") pool.set_threads(std::stoi(opt_val));
            else if (opt_name == "rfp_margin") V8::TUNE.rfp_margin = std::stoi(opt_val);
            else if (opt_name == "rfp_improving") V8::TUNE.rfp_improving = std::stoi(opt_val);
            else if (opt_name == "razor_margin") V8::TUNE.razor_margin = std::stoi(opt_val);
            else if (opt_name == "probcut_margin") V8::TUNE.probcut_margin = std::stoi(opt_val);
            else if (opt_name == "fut_base") V8::TUNE.fut_base = std::stoi(opt_val);
            else if (opt_name == "fut_mult") V8::TUNE.fut_mult = std::stoi(opt_val);
            else if (opt_name == "see_quiet") V8::TUNE.see_quiet = std::stoi(opt_val);
            else if (opt_name == "see_cap") V8::TUNE.see_cap = std::stoi(opt_val);
            else if (opt_name == "lmr_base") { V8::TUNE.lmr_base = std::stoi(opt_val); V8::init_lmr(); }
            else if (opt_name == "lmr_div") { V8::TUNE.lmr_div = std::stoi(opt_val); V8::init_lmr(); }
        } else if (token == "ucinewgame") {
            join_search();
            V8::TT.clear();
            pool.clear_histories();
            cur_fen = START_FEN; cur_moves.clear(); root_sp = 0;
            root_pos.set(cur_fen, false, &root_st[0]);
        } else if (token == "position") {
            join_search();
            std::string type; ss >> type;
            std::string fen;
            if (type == "startpos") { fen = START_FEN; ss >> token; }
            else if (type == "fen") {
                while (ss >> token && token != "moves") { if (!fen.empty()) fen += " "; fen += token; }
            }
            cur_fen = fen; cur_moves.clear(); root_sp = 0;
            root_st[0] = Stockfish::StateInfo{};
            root_pos.set(cur_fen, false, &root_st[0]);
            if (token == "moves") {
                while (ss >> token) {
                    Stockfish::Move m = V8::parse_uci_move(root_pos, token);
                    if (m.is_ok()) {
                        root_sp++;
                        root_pos.do_move(m, root_st[root_sp]);
                        cur_moves.push_back(m);
                    }
                }
            }
        } else if (token == "go") {
            join_search();
            V8::SearchLimits lim;
            std::string sub;
            while (ss >> sub) {
                if (sub == "depth") ss >> lim.depth;
                else if (sub == "movetime") ss >> lim.movetime;
                else if (sub == "wtime") ss >> lim.wtime;
                else if (sub == "btime") ss >> lim.btime;
                else if (sub == "winc") ss >> lim.winc;
                else if (sub == "binc") ss >> lim.binc;
                else if (sub == "movestogo") ss >> lim.movestogo;
                else if (sub == "nodes") ss >> lim.max_nodes;
                else if (sub == "infinite") lim.infinite = true;
            }
            Stockfish::Color stm = root_pos.side_to_move();
            search_thread = std::thread([&pool, cur_fen, cur_moves, stm, lim]() {
                pool.start_search(cur_fen, cur_moves, stm, lim);
            });
        } else if (token == "stop") { join_search(); }
        else if (token == "quit") { join_search(); break; }
    }
    join_search();
    return 0;
}
