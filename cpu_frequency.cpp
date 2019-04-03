// Copyright (c) 2019 Google Inc.
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
class HighResolutionTimer {
 public:
  HighResolutionTimer() noexcept {
    QueryPerformanceCounter(&start_);
  }

  double elapsed_seconds() const noexcept {
    LARGE_INTEGER stop;
    QueryPerformanceCounter(&stop);
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return (stop.QuadPart - start_.QuadPart) /
           static_cast<double>(frequency.QuadPart);
  }

 private:
  LARGE_INTEGER start_;
};

void set_thread_affinity(HANDLE handle, int core) {
  SetThreadAffinityMask(handle, 1LL << core);
}

void set_thread_priority_max(HANDLE handle) {
  SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
}

#else

class HighResolutionTimer {
 public:
  HighResolutionTimer() {
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_);
  }

  double elapsed_seconds() const noexcept {
    timespec end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    auto timer = diff(start_, end);
    double ret = timer.tv_sec;
    ret += timer.tv_nsec / 1e9;
    return ret;
  }

 private:
  static timespec diff(timespec start, timespec end) {
    timespec temp;
    if((end.tv_nsec - start.tv_nsec) < 0) {
      temp.tv_sec  = end.tv_sec - start.tv_sec - 1;
      temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    }
    else {
      temp.tv_sec  = end.tv_sec - start.tv_sec;
      temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
  }

  timespec start_;
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

// Calculate the frequency of the current CPU in MHz, one time.
float measure_frequency_once(int spin_count) {

  // By construction of execute_exact_clocks.asm
  int const kClocksPerSpin = 50;

  // Spin for kSpinsPerLoop * kSpinCount cycles. This should
  // be fast enough to usually avoid any interrupts.
  HighResolutionTimer t;
  execute_exact_clocks(spin_count);
  double const elapsed = t.elapsed_seconds();
  assert(elapsed > 0);

  // Calculate the frequency in MHz.
  int clocks           = kClocksPerSpin * spin_count;
  auto const frequency = (clocks / elapsed) / 1e6;
  return static_cast<float>(frequency);
}

template <typename Handle>
void configure_monitor_thread(Handle h, int index) {
#ifdef _WIN32
  HANDLE handle = reinterpret_cast<HANDLE>(h);
#else
  auto handle = h;
#endif
  set_thread_affinity(handle, 1LL << index);
  set_thread_priority_max(handle);
}

template <typename Handle>
void configure_busy_thread(Handle h, int index) {
#ifdef _WIN32
  HANDLE handle = reinterpret_cast<HANDLE>(h);
#else
  auto handle = h;
#endif
  set_thread_affinity(handle, 1LL << index);
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

void CpuFrequency::start_threads(
    int num_monitor_threads, int num_worker_threads) {
  thread_count_ = num_monitor_threads;
  thread_data_.resize(num_monitor_threads);
  threads_.reserve(thread_count_);
  for(int i = 0; i < num_monitor_threads; ++i) {
    threads_.emplace_back(&CpuFrequency::sample_thread, this, &thread_data_[i]);
    configure_monitor_thread(threads_.back().native_handle(), i);
  }

  for(int i = 0; i < num_worker_threads; ++i) {
    threads_.emplace_back(&CpuFrequency::busy_thread, this);
    configure_busy_thread(threads_.back().native_handle(), i);
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

std::size_t CpuFrequency::busy_thread() {
  std::size_t i = 0;
  while(!cancel_) {
    HighResolutionTimer t;
    while(!cancel_) {
      ++i;
      if(t.elapsed_seconds() > 0.5) {
        break;
      }
    }
    sched_yield();
  }
  return i;
}