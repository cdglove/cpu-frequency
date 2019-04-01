// Copyright (c) 2018 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "cpu_frequency.hpp"
#include <cassert>
#include <chrono>
#include <ratio>

#ifdef _WIN32
#  include <Windows.h>
#else
#  include <pthread.h>
#endif

extern "C" int execute_exact_clocks(int spin_count);

namespace {

#ifdef _WIN32
// class HighResolutionTimer {
// public:
//  HighResolutionTimer() noexcept {
//    QueryPerformanceCounter(&start_);
//  }
//  double ElapsedSeconds() const noexcept {
//    LARGE_INTEGER stop;
//    QueryPerformanceCounter(&stop);
//    LARGE_INTEGER frequency;
//    QueryPerformanceFrequency(&frequency);
//    return (stop.QuadPart - start_.QuadPart) /
//           static_cast<double>(frequency.QuadPart);
//  }
//
// private:
//  LARGE_INTEGER start_;
//};

void set_thread_affinity(HANDLE handle, int core) {
  SetThreadAffinityMask(handle, 1LL << core);
}

void set_thread_priority_max(HANDLE handle) {
  SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
}

#else

class HighResolutionTimer {
 public:
  double ElapsedSeconds() const noexcept {
    using namespace std::chrono;
    auto end = clock::now();
    return duration_cast<microseconds>(end - start_).count() / 1e6;
  }

 private:
  using clock = std::chrono::high_resolution_clock;

  std::chrono::time_point<clock> start_ = clock::now();
};

void set_thread_affinity(pthread_t handle, int core) {
  cpu_set_t cpuset;
  pthread_t thread;
  thread = pthread_self();
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
}

void set_thread_priority_max(pthread_t handle) {
  sched_param param;
  int policy;
  pthread_getschedparam(handle, &policy, &param);
  param.sched_priority = -10;
  pthread_setschedparam(handle, policy, &param);
}

#endif // _WIN32

class HighResolutionTimer {
 public:
  double ElapsedSeconds() const noexcept {
    using namespace std::chrono;
    auto end = clock::now();
    return duration_cast<microseconds>(end - start_).count() / 1e6;
  }

 private:
  using clock = std::chrono::system_clock;

  std::chrono::time_point<clock> start_ = clock::now();
};

// Calculate the frequency of the current CPU in MHz, one time.
float measure_frequency_once(int spin_count) {

  // By construction of execute_exact_clocks.asm
  int const kClocksPerSpin = 50;

  // Spin for kSpinsPerLoop * kSpinCount cycles. This should
  // be fast enough to usually avoid any interrupts.
  HighResolutionTimer t;
  execute_exact_clocks(spin_count);
  double const elapsed = t.ElapsedSeconds();
  assert(elapsed > 0);

  // Calculate the frequency in MHz.
  int clocks = kClocksPerSpin * spin_count;
  auto const frequency = (clocks / elapsed) / 1e6;
  return static_cast<float>(frequency);
}

template <typename Handle>
void configure_thread(Handle h, int index) {
#ifdef _WIN32
  HANDLE handle = reinterpret_cast<HANDLE>(h);
#else
  auto handle = h;
#endif
  set_thread_affinity(handle, 1LL << index);
  set_thread_priority_max(handle);
}

float measure_frequency(int attempts, int spin_count) {
  float max_frequency = 0.f;
  for(int i = 0; i < attempts; ++i) {
    float frequency = measure_frequency_once(spin_count);
    if(frequency > max_frequency) {
      max_frequency = frequency;
    }
  }

  return max_frequency;
}

} // namespace

void CpuFrequency::start_threads(int num_threads) {
  thread_count_ = num_threads;
  thread_data_.resize(num_threads);
  threads_.reserve(num_threads);
  for(int i = 0; i < num_threads; ++i) {
    threads_.emplace_back(
        &CpuFrequency::sample_thread, this, &thread_data_[i]);
    configure_thread(threads_.back().native_handle(), i);
  }
}

void CpuFrequency::stop_threads() {
  cancel_ = true;
  sample();
  for(auto&& t : threads_) {
    t.join();
  }
}

void CpuFrequency::sample() {
  start_work_.notify(thread_count_);
  for(auto i = thread_count_; i > 0; --i) {
    work_complete_.wait();
  }
}

void CpuFrequency::sample_thread(thread_data* data) {
  while(!cancel_) {
    start_work_.wait();
    data->mhz = measure_frequency(5, spin_count_);
    work_complete_.notify();
  }
}