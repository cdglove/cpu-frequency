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
#include <iomanip>
#include <iostream>

int main() {
  using namespace std::chrono_literals;

  CpuFrequency cpu_freq_mon(1000);
  cpu_freq_mon.start_threads(std::thread::hardware_concurrency());
  while(true) {
    cpu_freq_mon.sample();
    std::cout << std::fixed << std::setprecision(2) << std::setw(10);
    for(int i = 0; i < cpu_freq_mon.thread_count(); ++i) {
      std::cout << cpu_freq_mon.mhz(i) << "  ";
    }
    std::cout << std::endl;
    std::this_thread::sleep_for(1s);
  }
  cpu_freq_mon.stop_threads();
  return 0;
}