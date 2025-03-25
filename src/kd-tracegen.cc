#include <boost/program_options.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <iostream>
#include <random>
#include "tracegen-utils.h"

namespace po = boost::program_options;

struct group_tadr {
    i64 ird;   
    i64 addr;   
    int group;  // group index
};

bool group_tadr_cmp(const group_tadr &a, const group_tadr &b) {
    return a.ird > b.ird;
}

group_tadr heappop(vec<group_tadr> &heap) {
    std::pop_heap(heap.begin(), heap.end(), group_tadr_cmp);
    group_tadr min = heap.back();
    heap.pop_back();
    return min;
}

void heappush(vec<group_tadr> &heap, i64 ird, i64 addr, int group) {
    heap.push_back({ird, addr, group});
    std::push_heap(heap.begin(), heap.end(), group_tadr_cmp);
}

/**
 * kd_gen:
 *   - addrs: number of unique addresses.
 *   - length: number of trace entries.
 *   - irds: vector of IRD functions (one per group), parsed via parse_ird().
 *   - pop: vector of popularity weights (one per group).
 *   - rng: random number generator.
 *
 * Each address is assigned to a group by equal partitioning. For each address, we sample an initial IRD
 * from the group's IRD function, scale it by dividing by the popularity weight (rounding to an integer),
 * and schedule it in a min‑heap. Then, for each access, we pop the item with the smallest IRD, record its address,
 * sample a new IRD from the same group, add it (scaled) to the current IRD, and push it back.
 */
vec<i64> kd_gen(i64 addrs, i64 length, const vec<dist> &irds, const vec<double> &pop, std::mt19937_64 &rng)
{
    int groups = irds.size();
    i64 group_size = addrs / groups;
    vec<group_tadr> heap;
    heap.reserve(addrs);

    for (i64 a = 0; a < addrs; a++) {
        int group = a / group_size;
        if (group >= groups)
            group = groups - 1;
        int raw_ird = irds[group](rng);
        double scaled = (pop[group] == 0.0 ? raw_ird : (double)raw_ird / pop[group]);
        i64 init_ird = (i64)std::llround(scaled);
        if (init_ird < 0)
            init_ird = 0;
        heap.push_back({init_ird, a, group});
    }
    std::make_heap(heap.begin(), heap.end(), group_tadr_cmp);

    vec<i64> trace;
    trace.reserve(length);
    for (i64 i = 0; i < length; i++) {
        auto entry = heappop(heap);
        trace.push_back(entry.addr);

        int raw_ird = irds[entry.group](rng);
        double scaled = (pop[entry.group] == 0.0 ? raw_ird : (double)raw_ird / pop[entry.group]);
        i64 add_ird = (i64)std::llround(scaled);
        if (add_ird < 0)
            add_ird = 0;
        entry.ird += add_ird;
        heappush(heap, entry.ird, entry.addr, entry.group);
    }
    return trace;
}

int main(int argc, char **argv) {
    i64 length, num_addrs, seed, blocksize;
    f64 frac_read;
    str ird_arg, irm_arg, sizedist_arg;
    int groups;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Produce this message")
        ("addresses,m", po::value<i64>(&num_addrs)->required(), "Footprint size (number of unique addresses)")
        ("length,n", po::value<i64>(&length)->required(), "Length of trace (in addresses)")
        ("seed,s", po::value<i64>(&seed)->default_value(42), "RNG seed")
        ("blocksize,b", po::value<i64>(&blocksize)->default_value(4096), "Block size in bytes")
        ("groups,k", po::value<int>(&groups)->required(), "Number of groups")
        ("ird,f", po::value<str>(&ird_arg)->required(),
         "Semicolon-separated IRD distributions (each a comma-separated probability vector). "
         "E.g. \"fgen:10000:0.00001:3,5,10,20;fgen:100:0.005:3,5,10,20\"")
        ("irm,g", po::value<str>(&irm_arg)->required(),
         "Single popularity specification for all groups. "
         "Either a canonical spec (e.g. \"zipf:1.2,2\") or a comma-separated list (e.g. \"2,8\").")
        ("rwratio,r", po::value<f64>(&frac_read)->default_value(1), "Fraction of addresses that are reads")
        ("sizedist,z", po::value<str>(&sizedist_arg)->default_value("1:1"), "Request size distribution")
    ;

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 1;
        }
    } catch (std::exception &e) {
        fmt::print("Error: {}\n", e.what());
        std::cout << desc << std::endl;
        return 1;
    }
    po::notify(vm);

    fmt::print("Generating trace:\n  addresses={} length={} groups={} seed={}\n",
               num_addrs, length, groups, seed);

    std::mt19937_64 rng(seed);
    vec<str> ird_parts = split(ird_arg, ";");
    ensure_fatal(ird_parts.size() == (size_t)groups, "Expected {} IRD specs, got {}", groups, ird_parts.size());
    vec<dist> irds;
    for (auto &spec : ird_parts) {
    irds.push_back(parse_ird(spec));
    }
    dist irm_dist = parse_irm(irm_arg, num_addrs, true);
    vec<double> pop;
    for (int i = 0; i < groups; i++) {
    i64 sample = irm_dist(rng);
    pop.push_back((double)sample / 10000.0);
    }
    auto sizedist = parse_request_sizes(sizedist_arg);
    auto addrs = kd_gen(num_addrs, length, irds, pop, rng);

    std::uniform_real_distribution<> d_is_read(0, 1);
    for (auto addr : addrs) {
        bool is_read = (d_is_read(rng) < frac_read);
        i64 size_in_blocks = sizedist(rng);
        fmt::print("{:d} {} {}\n",
                   is_read ? 0 : 1,
                   size_in_blocks * blocksize,
                   addr * blocksize);
    }

    return 0;
}
