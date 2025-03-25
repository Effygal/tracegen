#include <boost/program_options.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <iostream>
#include <random>
#include "tracegen-utils.h"

namespace po = boost::program_options;

struct tadr {
    i64 ird;
    i64 addr;
};

bool tadr_cmp(const tadr &a, const tadr &b) { return a.ird > b.ird; }

tadr heappop(vec<tadr>& heap) {
    std::pop_heap(heap.begin(), heap.end(), tadr_cmp);
    tadr min = heap.back();
    heap.pop_back();
    return min;
}

void heappush(vec<tadr>& heap, i64 ird, i64 addr) {
    heap.push_back({ird, addr});
    std::push_heap(heap.begin(), heap.end(), tadr_cmp);
}

vec<i64> td_gen(i64 addrs, i64 length, f64 p_irm, dist d_ird, dist d_irm, std::mt19937_64 &rng) {
    vec<tadr> irds;
    for (i64 a = 0; a < addrs; a++)
        irds.push_back({d_ird(rng), a});
    std::make_heap(irds.begin(), irds.end(), tadr_cmp);
    std::uniform_real_distribution<> d_is_irm(0, 1);
    vec<i64> trace;
    for (i64 i = 0; i < length; i++) {
        if (d_is_irm(rng) < p_irm) {
            auto addr = d_irm(rng);
            assert(addr < addrs);
            trace.push_back(addr);
        } else {
            auto ird_sample = d_ird(rng);
            assert(ird_sample >= 0 && ird_sample < addrs);
            tadr min_ird = heappop(irds);
            trace.push_back(min_ird.addr);
            heappush(irds, min_ird.ird + ird_sample, min_ird.addr);
        }
    }
    return trace;
}

int main(int argc, char **argv) {
    i64 length, num_addrs, seed, blocksize;
    f64 p_irm, frac_read;
    str ird_arg, irm_arg, sizedist_arg;
    
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Produce this message")
        ("addresses,m", po::value<i64>(&num_addrs)->required(), "Footprint size (number of unique addresses)")
        ("length,n", po::value<i64>(&length)->required(), "Length of trace (in addresses)")
        ("p_irm,p", po::value<f64>(&p_irm)->required(), "Probability of IRM")
        ("seed,s", po::value<i64>(&seed)->default_value(42), "RNG seed")
        ("blocksize,b", po::value<i64>(&blocksize)->default_value(4096), "Block size in bytes")
        ("ird,f", po::value<str>(&ird_arg)->default_value("b"), "IRD distribution")
        ("irm,g", po::value<str>(&irm_arg)->default_value("zipf:1.2,20"), "IRM distribution")
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
    
    fmt::print("Generating trace with parameters:\nAddresses: {}\nLength: {}\nProbability of IRM: {}\nSeed: {}\n", 
               num_addrs, length, p_irm, seed);
    
    std::mt19937_64 rng(seed);
    auto ird = parse_ird(ird_arg);
    auto irm = parse_irm(irm_arg, num_addrs, false);
    auto sizedist = parse_request_sizes(sizedist_arg);
    auto addrs = td_gen(num_addrs, length, p_irm, ird, irm, rng);
    
    std::uniform_real_distribution<> d_is_read(0, 1);
    for (auto addr : addrs) {
        auto is_read = (d_is_read(rng) < frac_read);
        auto size = sizedist(rng);
        fmt::print("{:d} {} {}\n", !is_read, size * blocksize, addr * blocksize);
    }
    
    return 0;
}
