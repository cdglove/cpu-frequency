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
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

struct Options {
  enum Mode {
    Monitor,
    Instrument,
  };

  Mode mode      = Monitor;
  int threads    = std::thread::hardware_concurrency();
  bool want_help = false;
  int samples    = 1000;
};

void print_usage() {
  // clang-format off
  std::cout << "  Usage: \n"
               "    --mode (monitor, instrument)\n"
               "      Selects the mode to run in. Possible values are:\n"
               "        monitor    In this mode, the application passively monitors the speed\n"
               "                   of all cores without affecting the speed of the cpu (much)\n"
               "  \n"
               "        instrument In this mode, the application spins the cpus up to max in order\n"
               "                   measure their max capability.\n"
               "  \n"
               "    --threads int\n"
               "      Sets the number of threads to use. Leave blank to auto select based on detected\n"
               "      core count."
               << std::endl;
  // clang-format on
}

Options parse_options(int argc, char** argv) {
  auto has_another_arg      = [argc](int i) { return i < (argc - 1); };
  auto try_read_next_option = [&](int i, auto& value) {
    if(!has_another_arg(i)) {
      std::cout << argv[i] << " requires an argument" << std::endl;
      print_usage();
      throw std::runtime_error("error parsing arguments.");
    }
    ++i;
    std::stringstream s;
    s.str(argv[i]);
    s >> value;
    if(s.bad()) {
      std::cout << "failed to parse " << argv[i] << " argument" << std::endl;
      print_usage();
      throw std::runtime_error("error parsing arguments.");
    }
  };

  Options options;
  std::stringstream opt;
  for(int i = 0; i < argc; ++i) {
    if(!std::strcmp(argv[i], "--mode")) {
      if(!has_another_arg(i)) {
        std::cout << "--mode requires an argument" << std::endl;
        print_usage();
        throw std::runtime_error("error parsing arguments.");
      }

      ++i;
      if(!std::strcmp(argv[i], "monitor")) {
        options.mode = Options::Monitor;
      }
      else if(!std::strcmp(argv[i], "instrument")) {
        options.mode = Options::Instrument;
      }
      else {
        std::cout << "failed to parse mode argument" << std::endl;
        print_usage();
        throw std::runtime_error("error parsing arguments.");
      }
    }
    else if(!std::strcmp(argv[i], "--threads")) {
      try_read_next_option(i, options.threads);
    }
    else if(!std::strcmp(argv[i], "--samples")) {
      try_read_next_option(i, options.samples);
    }
    else if(!std::strcmp(argv[i], "--help")) {
      options.want_help = true;
    }
  }

  return options;
}

namespace sc = std::chrono;

class FrequencyTimer {
 public:
  FrequencyTimer(double frequency_hz)
      : cycle_(frequency_hz > 0 ? (1 / frequency_hz) : 0)
      , start_(sc::steady_clock::now()) {
  }

  bool expired() const {
    return remaining() <= sc::milliseconds(0);
  }

  sc::milliseconds remaining() const {
    auto next = start_ + cycle_;
    return sc::duration_cast<sc::milliseconds>(next - sc::steady_clock::now());
  }

  void reset() {
    // cglover-todo: this will cause drift;
    start_ = sc::steady_clock::now();
  }

 private:
  sc::duration<double> cycle_;
  sc::time_point<sc::steady_clock> start_;
};

int main(int argc, char** argv) {
  Options options = parse_options(argc, argv);
  if(options.want_help) {
    print_usage();
    return 0;
  }

  CpuFrequency cpu_freq_mon(options.samples);

  if(options.mode == Options::Monitor) {
    std::cout << "Monitoring CPU frequencies on " << options.threads
              << " threads." << std::endl;
  }
  else {
    std::cout << "Instrumenting CPU frequencies on " << options.threads
              << " threads." << std::endl;
  }

  cpu_freq_mon.start_threads(options.threads);

  using namespace std::literals;

  FrequencyTimer print_timer(1);
  FrequencyTimer sample_timer(options.mode == Options::Monitor ? 1 : 0);

  while(true) {
    cpu_freq_mon.sample();
    if(print_timer.expired()) {
      print_timer.reset();
      std::cout << std::fixed << std::setprecision(2) << std::setw(10);
      for(int i = 0; i < cpu_freq_mon.thread_count(); ++i) {
        std::cout << cpu_freq_mon.mhz(i) << "  ";
      }
      std::cout << std::endl;
    }

    auto remaining = sample_timer.remaining();
    if(remaining > 10ms) {
      std::this_thread::sleep_for(remaining);
      sample_timer.reset();
    }
  }

  cpu_freq_mon.stop_threads();
  return 0;
}