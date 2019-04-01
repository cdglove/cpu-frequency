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
#ifndef CPUFREQUENCY_HPP
#define CPUFREQUENCY_HPP
#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

class semaphore {
 public:
  void notify() {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    ++count_;
    condition_.notify_one();
  }

  void notify(unsigned int count) {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    count_ += count;
    condition_.notify_all();
  }

  void wait() {
    std::unique_lock<decltype(mutex_)> lock(mutex_);
    while(!count_) {
      condition_.wait(lock);
    }
    --count_;
  }

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  unsigned long count_ = 0; // Initialized as locked.
};

class cpu_frequency {
 public:
  cpu_frequency(int spin_count)
      : spin_count_(spin_count) {
  }

  cpu_frequency(cpu_frequency const&) = delete;
  cpu_frequency& operator=(cpu_frequency const&) = delete;

  void start_threads(int num_threads);
  void stop_threads();
  void sample();

  int thread_count() {
    return thread_count_;
  }

  float mhz(int i) {
    return thread_data_[i].mhz;
  }

 private:
  struct thread_data {
    float mhz;
  };
  void sample_thread(thread_data* data);

  std::vector<std::thread> threads_;
  std::vector<thread_data> thread_data_;
  std::atomic<bool> cancel_{false};
  semaphore start_work_;
  semaphore work_complete_;
  int spin_count_            = 0;
  unsigned int thread_count_ = 0;
};
#endif // CPUFREQUENCY_HPP