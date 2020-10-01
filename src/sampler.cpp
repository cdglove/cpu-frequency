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
#include "cpuhz/sampler.hpp"

#include <cassert>
#include <chrono>
#include <ratio>
#include <sstream>

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
  auto result = SetThreadAffinityMask(handle, 1LL << core);
  if(result == 0) {
    auto last = GetLastError();
    std::stringstream s;
    s << "Error calling SetThreadAffinityMask: " << result
      << ", GetLastError: " << last;
    throw std::runtime_error(s.str());
  }
}

void set_thread_priority_max(HANDLE handle) {
  auto result = SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
  if(result == 0) {
    auto last = GetLastError();
    std::stringstream s;
    s << "Error calling SetThreadPriority: " << result
      << ", GetLastError: " << last;
    throw std::runtime_error(s.str());
  }
}

HANDLE get_current_thread_handle() {
  return GetCurrentThread();
}

int get_current_thread_core() {
  return GetCurrentProcessorNumber();
}

#else

class HighResolutionTimer {
 public:
  HighResolutionTimer() {
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_);
  }

  double elapsed_seconds() const noexcept {
    timespec end;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
    auto timer = diff(start_, end);
    double ret = timer.tv_nsec;
    ret /= std::nano::den * 1.0;
    ret += timer.tv_sec;
    return ret;
  }

 private:
  static timespec diff(timespec start, timespec end) {
    timespec temp;
    if((end.tv_nsec - start.tv_nsec) < 0) {
      temp.tv_sec  = end.tv_sec - start.tv_sec - 1;
      temp.tv_nsec = std::nano::den + end.tv_nsec - start.tv_nsec;
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
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  auto error = pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
  if(error != 0) {
    std::stringstream s;
    s << "Error calling pthread_setaffinity_np: " << error;
    throw std::runtime_error(s.str());
  }
}

void set_thread_priority_max(pthread_t handle) {
  sched_param params;
  params.sched_priority = sched_get_priority_max(SCHED_OTHER);
  auto error            = pthread_setschedparam(handle, SCHED_OTHER, &params);
  if(error != 0) {
    std::stringstream s;
    s << "Error calling pthread_setschedparam: " << error;
    throw std::runtime_error(s.str());
  }
}

int get_current_thread_core() {
  return sched_getcpu();
}

pthread_t get_current_thread_handle() {
  return pthread_self();
}

#endif // _WIN32

// Calculate the frequency of the current CPU in MHz, one time.
float measure_frequency_once(int spin_count) {

  // By construction of execute_exact_clocks.asm
  int const kClocksPerSpin = 50;
 
  // Try to sleep immediately before measuring so we're more likely to 
  // avoid interruption.
  std::this_thread::sleep_for(std::chrono::microseconds(1));

  HighResolutionTimer t;

  // Spin for kSpinsPerLoop * kSpinCount cycles. This should
  // be fast enough to usually avoid any interrupts.
  execute_exact_clocks(spin_count);
  double const elapsed = t.elapsed_seconds();
  assert(elapsed > 0);

  // Calculate the frequency in MHz.
  int clocks           = kClocksPerSpin * spin_count;
  auto const frequency = (clocks / elapsed) / 1e6;
  return static_cast<float>(frequency);
}

void configure_monitor_thread(int index) {
  auto handle = get_current_thread_handle();
  set_thread_affinity(handle, index);
  set_thread_priority_max(handle);
}

float measure_frequency(int attempts, int spin_count) {
  float max_frequency = measure_frequency_once(spin_count);
  for(int i = 1; i < attempts; ++i) {
    float frequency = measure_frequency_once(spin_count);
    if(frequency > max_frequency) {
      max_frequency = frequency;
    }
  }

  return max_frequency;
}

} // namespace

namespace cpuhz {
void Sampler::start_threads(int num_monitor_threads) {
  thread_count_ = num_monitor_threads;
  thread_data_.resize(num_monitor_threads);
  threads_.reserve(thread_count_);
  for(int i = 0; i < num_monitor_threads; ++i) {
    threads_.emplace_back(&Sampler::sample_thread, this, &thread_data_[i]);
  }
}

void Sampler::stop_threads() {
  cancel_ = true;
  sample();
  for(auto&& t : threads_) {
    t.join();
  }
}

void Sampler::sample() {
  std::fill(thread_data_.begin(), thread_data_.end(), thread_data());
  start_work_.notify(thread_count_);
  work_complete_.wait(thread_count_);
  end_work_.notify(thread_count_);
}

void Sampler::sample_thread(thread_data* data) {
  auto thread_index = static_cast<int>(data - thread_data_.data());
  configure_monitor_thread(thread_index);
  while(!cancel_) {
    start_work_.wait();
    data->mhz = measure_frequency(25, spin_count_);

    auto current_core = get_current_thread_core();
    if(thread_index != current_core) {
      std::stringstream s;
      s << "Error: thread " << thread_index << " running on core "
        << current_core;
      throw std::runtime_error(s.str());
    }

    work_complete_.notify();
    end_work_.wait();
  }
}

} // namespace cpuhz