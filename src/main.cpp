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
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

struct Options {
  int threads = std::thread::hardware_concurrency();
  bool want_help = false;
  int samples = 2500;
};

void print_usage() {
  // clang-format off
  std::cout << " Usage: \n"
               "   --threads int\n"
               "       Sets the number of threads to use.\n" 
               "       Leave blank to auto select based on detected core count.\n"
               "   --samples int\n"
               "       Sets the number of samples to take when timing.\n"
               "       It's imprtant to selct the smallest value possible\n." 
               "       Larger values are necessary on very fast processors to be long enough to measure,\n"
               "       but larger values can cause the frequency to increase and are also likely\n"
               "       to be interrupted by the scheduler, reducing the measured frequency.\n"
               "       Good values are typically around 1 per CPU MHz, or 1000 per GHz.\n"
               "       Example: A 2.2Ghz CPU could use a value around 2200.\n"
               "       The default is 2500."
               << std::endl;
  // clang-format on
}

Options parse_options(int argc, char** argv) {
  auto has_another_arg = [argc](int i) { return i < (argc - 1); };
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
    if(!std::strcmp(argv[i], "--threads")) {
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
    start_ = sc::time_point_cast<sc::steady_clock::duration>(start_ + cycle_);
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

  cpuhz::Sampler cpu_freq_mon(options.samples);
  std::cout << "Monitoring CPU frequencies on " << options.threads
            << " threads." << std::endl;

  cpu_freq_mon.start_threads(options.threads);

  using namespace std::literals;

  FrequencyTimer print_timer(1);

  while(true) {
    cpu_freq_mon.sample();
    if(print_timer.expired()) {
      print_timer.reset();
      std::cout << std::fixed << std::setprecision(2);
      for(int i = 0; i < cpu_freq_mon.thread_count(); ++i) {
        auto mhz = cpu_freq_mon.mhz(i);
        std::cout << std::setw(9) << mhz << "  ";
      }
      std::cout << std::endl;
    }

    auto remaining = print_timer.remaining();
    if(remaining > 10ms) {
      std::this_thread::sleep_for(remaining);
    }
  }

  cpu_freq_mon.stop_threads();
  return 0;
}