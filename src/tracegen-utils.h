#ifndef TRACEGEN_UTILS_H
#define TRACEGEN_UTILS_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include "utils.h"  // defines vec<T>, str, split(), normalise_vec(), ensure_fatal(), log_info(), log_fatal()

inline vec<std::uniform_int_distribution<i64>> get_intervals(i64 classes, i64 max) {
    assert(classes > 0 && max > 0 && classes <= max);
    vec<std::uniform_int_distribution<i64>> intervals;
    auto width = max / classes;
    for (i64 i = 1; i <= classes; i++) {
        auto lower = (i - 1) * width;
        auto upper = i * width - 1;
        upper = std::min(upper, max - 1);
        intervals.push_back(std::uniform_int_distribution<i64>(lower, upper));
    }
    return intervals;
}

inline dist normal_dist(f64 mean, f64 stddev, i64 max) {
    std::normal_distribution<f64> dis(mean, stddev);
    return [dis, max](std::mt19937_64 &rng) mutable -> i64 {
        auto sample = dis(rng);
        if (sample < 0)
            return 0;
        if (sample > max)
            return max;
        return (i64)std::round(sample);
    };
}

inline dist zipf_dist(f64 alpha, i64 classes, i64 max) {
    assert(alpha > 0 && classes > 0);
    fmt::print("IRM: zipf: alpha: {} n: {}\n", alpha, classes);
    auto intervals = get_intervals(classes, max);
    vec<f64> weights;
    for (f64 i = 1; i <= classes; i++)
        weights.push_back(1.0 / std::pow(i, alpha));
    normalise_vec(weights);
    assert(weights.size() == intervals.size());
    std::discrete_distribution<i64> dis(weights.begin(), weights.end());
    return [dis, ivs = std::move(intervals)](std::mt19937_64 &rng) mutable {
        auto idx = dis(rng);
        return ivs[idx](rng);
    };
}

inline dist uniform_dist(i64 max) {
    std::uniform_int_distribution<i64> dis(0, max - 1);
    return [dis](std::mt19937_64 &rng) mutable { return dis(rng); };
}

inline dist pareto_dist(f64 xm, f64 alpha, i64 classes, i64 max) {
    assert(xm > 0 && alpha > 0 && classes > 0);
    fmt::print("IRM: pareto: xm: {} n: {}\n", xm, classes);
    auto intervals = get_intervals(classes, max);
    vec<f64> weights;
    for (f64 i = 1; i <= classes; i++)
        weights.push_back(std::pow(xm / i, alpha));
    normalise_vec(weights);
    assert(weights.size() == intervals.size());
    std::discrete_distribution<i64> dis(weights.begin(), weights.end());
    return [dis, ivs = std::move(intervals)](std::mt19937_64 &rng) mutable {
        auto idx = dis(rng);
        return ivs[idx](rng);
    };
}

inline dist sequential_dist() {
    fmt::print("IRM: sequential\n");
    i64 i = 0;
    return [i](std::mt19937_64 &rng) mutable { return i++; };
}

inline dist irdgen(i64 k, f64 epsilon, vec<i64> spikes) {
    vec<f64> weights;
    for (i64 i = 0; i < k; i++)
        weights.push_back(epsilon);
    for (auto s : spikes)
        weights[s] = 1 - epsilon;
    normalise_vec(weights);
    fmt::print("IRD: k: {} epsilon: {} spikes: ", k, epsilon);
    for (auto s : spikes)
        fmt::print("{} ", s);
    fmt::print("\n");
    std::discrete_distribution<i64> dis(weights.begin(), weights.end());
    return [dis](std::mt19937_64 &rng) mutable { return dis(rng); };
}

inline dist parse_request_sizes(str arg) {
    vec<str> parts = split(arg, ":");
    ensure_fatal(parts.size() == 2, "Invalid size dist string: {}", arg);
    vec<str> weights_str = split(parts[0], ",");
    vec<str> sizes_str = split(parts[1], ",");
    ensure_fatal(weights_str.size() == sizes_str.size(), "Unequal number of weights and sizes: {}", arg);
    vec<f64> w;
    vec<i64> s;
    for (auto &x : weights_str)
        w.push_back(std::stod(x));
    for (auto &x : sizes_str)
        s.push_back(std::stoi(x));
    normalise_vec(w);
    std::discrete_distribution<i64> dis(w.begin(), w.end());
    return [dis, s](std::mt19937_64 &rng) mutable {
        auto idx = dis(rng);
        return s[idx];
    };
}

inline vec<double> parse_probabilities(const str &s) {
    vec<str> parts = split(s, ",");
    vec<double> probs;
    for (auto &part : parts)
        probs.push_back(std::stod(part));
    normalise_vec(probs);
    return probs;
}

inline dist parse_fgen(vec<str> args) {
    assert(args[0] == "fgen");
    ensure_fatal(args.size() == 4, "fgen requires 3 arguments - fgen:k:epsilon:spikes");
    i64 k = std::stoi(args[1]);
    f64 epsilon = std::stod(args[2]);
    str spikes_str = args[3];
    vec<str> spikes = split(spikes_str, ",");
    vec<i64> spike_idxs;
    for (auto &s : spikes)
        spike_idxs.push_back(std::stoi(s));
    return irdgen(k, epsilon, spike_idxs);
}

inline dist parse_ird(str s) {
    if (s == "b")
        return irdgen(20, 0.005, {0, 3});
    if (s == "c")
        return irdgen(20, 0.005, {2, 9});
    if (s == "d")
        return irdgen(5, 0.01, {0, 4});
    if (s == "e")
        return irdgen(20, 0.005, {1});
    if (s == "f")
        return irdgen(20, 0.01, {2});
    vec<str> args = split(s, ":");
    if (args[0] == "fgen")
        return parse_fgen(args);
    std::cerr << "Invalid dist string: " << s << std::endl;
    exit(1);
}

inline dist parse_irm(str dist_str, i64 max, bool pop_mode = false) {
    const double scale = 10000.0; // fixed–point scale used in popularity mode
    // Non-canonical specification: no colon.
    if(dist_str.find(":") == str::npos) {
      vec<str> tokens = split(dist_str, ",");
      // Convert tokens to doubles.
      vec<double> vals;
      for(auto &t : tokens)
        vals.push_back(std::stod(t));
      normalise_vec(vals);
      if(!pop_mode) {
        // Address mode: partition the M addresses into bins.
        // Compute the bin boundaries.
        vec<i64> boundaries;
        boundaries.push_back(0);
        double sum = 0;
        for (size_t i = 0; i < vals.size(); i++) {
          sum += vals[i];
          // Use floor (or round) to determine the boundary.
          boundaries.push_back((i64)std::floor(sum * max));
        }
        // Create a discrete distribution using the normalized weights.
        std::discrete_distribution<int> bin_dis(vals.begin(), vals.end());
        return [bin_dis, boundaries](std::mt19937_64 &rng) mutable -> i64 {
          int bin = bin_dis(rng);
          i64 start = boundaries[bin];
          i64 end = boundaries[bin+1] - 1;
          std::uniform_int_distribution<i64> u(start, end);
          return u(rng);
        };
      } else {
        // Popularity mode: return a distribution that samples a weight.
        std::discrete_distribution<int> dis(vals.begin(), vals.end());
        return [dis, vals, scale](std::mt19937_64 &rng) mutable -> i64 {
          int idx = dis(rng);
          double v = vals[idx]; // normalized weight in [0,1]
          return (i64) std::llround(v * scale);
        };
      }
    }
    // Canonical specification: parse as before.
    vec<str> args = split(dist_str, ":");
    ensure_fatal(args.size() == 2, "Invalid dist string: {} ", dist_str);
    str dist_type = args[0];
    vec<str> dist_args = split(args[1], ",");
    if (dist_type == "pareto") {
      ensure_fatal(dist_args.size() == 3, "Pareto dist requires 3 args");
      f64 xm = std::stod(dist_args[0]);
      f64 alpha = std::stod(dist_args[1]);
      i64 n = std::stoi(dist_args[2]);
      log_info("Pareto dist: xm: {} alpha: {} n: {}", xm, alpha, n);
      return pareto_dist(xm, alpha, n, max);
    }
    if (dist_type == "zipf") {
      ensure_fatal(dist_args.size() == 2, "Zipf dist requires 2 args");
      f64 alpha = std::stod(dist_args[0]);
      i64 n = std::stoi(dist_args[1]);
      log_info("Zipf dist: alpha: {} n: {}", alpha, n);
      return zipf_dist(alpha, n, max);
    }
    if (dist_type == "uniform") {
      log_info("Uniform dist: max: {}", max);
      return uniform_dist(max);
    }
    if (dist_type == "normal") {
      ensure_fatal(dist_args.size() == 2, "Normal dist requires 2 args");
      f64 mu = std::stod(dist_args[0]);
      f64 sigma = std::stod(dist_args[1]);
      log_info("Normal dist: mu: {} sigma: {}", mu, sigma);
      return normal_dist(mu, sigma, max);
    }
    log_fatal("Invalid dist type: {}", dist_type);
    exit(1);
  }
  
#endif // TRACEGEN_UTILS_H
