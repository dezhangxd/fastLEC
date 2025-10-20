#include "fastLEC.hpp"
#include "simu.hpp"
#include "parser.hpp"
#include "basic.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>

using namespace fastLEC;

unsigned fastLEC::Simulator::cal_pes_threads(unsigned n_t)
{
    if (n_t <= 0)
        return 0;
    unsigned para_bits = log2(n_t);
    return 1 << para_bits;
}

fastLEC::ret_vals fastLEC::Simulator::run_pbits_pes(unsigned n_t)
{
    double start_time = ResMgr::get().get_runtime();
    if (is == nullptr)
    {
        is = std::make_unique<fastLEC::ISimulator>();
        is->init_glob_ES(xag);
    }

    cal_es_bits(n_t);

    std::vector<std::thread> threads;
    std::atomic<bool> found_sat(false);
    std::atomic<unsigned long long> found_sat_round(0);
    std::atomic<bool> cutted(false);

    double time_resources = Param::get().timeout - ResMgr::get().get_runtime();

    auto worker = [&](uint64_t para_idx)
    {
        auto st = std::chrono::high_resolution_clock::now();
        std::vector<BitVector> loc_mem(is->glob_es.mem_sz,
                                       BitVector(1 << this->bv_bits));

        unsigned long long round_num = 1llu << batch_bits;
        unsigned long long round = 0;
        for (; round < round_num; round++)
        {
            if (Param::get().verbose > 1 && round % 1000 == 0 && para_idx == 0)
            {
                printf("c [iES] %6.2f%% : round %lld / %llu \n",
                       (double)round / round_num * 100,
                       round,
                       round_num);
                fflush(stdout);
            }
            if (round % 100 == 0 &&
                (std::chrono::duration_cast<std::chrono::duration<double>>(
                     std::chrono::high_resolution_clock::now() - st)
                         .count() > time_resources ||
                 global_solved_for_PPE.load()))
            {
                cutted.store(true);
                return;
            }

            loc_mem[0].reset();
            loc_mem[1].set();

            unsigned long long i = 0;
            unsigned bvb = std::min(is->glob_es.PI_num, this->bv_bits);
            for (i = 0; i < bvb; i++)
                loc_mem[i + 2].u64_pi(i);

            for (; i < bvb + para_bits; i++)
            {
                int k = (para_idx >> (i - bvb)) & 1ull;
                if (k == 1)
                    loc_mem[i + 2].set();
                else
                    loc_mem[i + 2].reset();
            }
            for (i = bvb + para_bits; i < is->glob_es.PI_num; i++)
            {
                int k = (round >> (i - bvb - para_bits)) & 1ull;
                if (k == 1)
                    loc_mem[i + 2].set();
                else
                    loc_mem[i + 2].reset();
            }

            for (i = 0; i < is->glob_es.n_ops; i++)
            {
                operation *op = &is->glob_es.ops[i];

                if (op->type == OP_AND)
                    loc_mem[op->addr1] =
                        loc_mem[op->addr2] & loc_mem[op->addr3];
                else if (op->type == OP_XOR)
                    loc_mem[op->addr1] =
                        loc_mem[op->addr2] ^ loc_mem[op->addr3];
                else if (op->type == OP_NOT)
                    loc_mem[op->addr1] = ~loc_mem[op->addr2];
            }

            if (loc_mem[is->glob_es.PO_lit].has_one())
            {
                found_sat.store(true);
                found_sat_round.store(round);
                return;
            }
        }
    };

    try
    {
        fflush(stdout);
        for (uint64_t i = 0; i < (1llu << para_bits); i++)
            threads.emplace_back(worker, i);

        for (auto &t : threads)
            t.join();
    }
    catch (const std::exception &e)
    {
        printf("c [piEPS(pbits)] exception: %s\n", e.what());
    }

    int res = 0;
    if (!cutted.load())
    {
        if (found_sat.load())
            res = 10;
        else
            res = 20;
    }

    printf("c [piES] result = %d [bv:pbits=%d:%d] [bv_w = %3d]"
           " [nGates = %5lu] [nPI = %3lu] [nBV = %u /t.] [time = %.2f]\n",
           res,
           this->bv_bits,
           this->batch_bits,
           Param::get().custom_params.es_bv_bits,
           xag.used_gates.size(),
           xag.PI.size(),
           is->glob_es.mem_sz,
           ResMgr::get().get_runtime() - start_time);
    fflush(stdout);

    return ret_vals(res);
}

fastLEC::ret_vals fastLEC::Simulator::run_round_pes(unsigned n_t)
{
    double start_time = ResMgr::get().get_runtime();
    if (is == nullptr)
    {
        is = std::make_unique<fastLEC::ISimulator>();
        is->init_glob_ES(xag);
    }

    std::vector<std::thread> threads;
    std::atomic<bool> found_sat(false);
    std::atomic<unsigned long long> found_sat_round(0);
    std::atomic<bool> cutted(false);

    double time_resources = Param::get().timeout - ResMgr::get().get_runtime();

    auto worker =
        [&](unsigned long long start_round, unsigned long long end_round)
    {
        auto st = std::chrono::high_resolution_clock::now();
        std::vector<BitVector> loc_mem(is->glob_es.mem_sz,
                                       BitVector(1 << this->bv_bits));

        for (unsigned long long round = start_round; round < end_round; round++)
        {
            if (Param::get().verbose > 1 && (round - start_round) % 1000 == 0 &&
                start_round == 0)
            {
                printf("c [piES] %6.2f%% : round %lld / %llu \n",
                       (double)(round - start_round) /
                           (end_round - start_round) * 100,
                       round - start_round,
                       end_round - start_round);
                fflush(stdout);
            }
            if ((round - start_round) % 100 == 0 &&
                (std::chrono::duration_cast<std::chrono::duration<double>>(
                     std::chrono::high_resolution_clock::now() - st)
                         .count() > time_resources ||
                 global_solved_for_PPE.load()))
            {
                cutted.store(true);
                return;
            }

            loc_mem[0].reset();
            loc_mem[1].set();

            unsigned long long i = 0;
            unsigned bvb = std::min(is->glob_es.PI_num, this->bv_bits);
            for (i = 0; i < bvb; i++)
                loc_mem[i + 2].u64_pi(i);

            for (i = bvb; i < is->glob_es.PI_num; i++)
            {
                int k = (round >> (i - bvb)) & 1ull;
                if (k == 1)
                    loc_mem[i + 2].set();
                else
                    loc_mem[i + 2].reset();
            }

            for (i = 0; i < is->glob_es.n_ops; i++)
            {
                operation *op = &is->glob_es.ops[i];

                if (op->type == OP_AND)
                    loc_mem[op->addr1] =
                        loc_mem[op->addr2] & loc_mem[op->addr3];
                else if (op->type == OP_XOR)
                    loc_mem[op->addr1] =
                        loc_mem[op->addr2] ^ loc_mem[op->addr3];
                else if (op->type == OP_NOT)
                    loc_mem[op->addr1] = ~loc_mem[op->addr2];
            }

            if (loc_mem[is->glob_es.PO_lit].has_one())
            {
                found_sat.store(true);
                found_sat_round.store(round);
                return;
            }
        }
    };

    try
    {
        cal_es_bits(1);
        unsigned long long round_num = 1llu << batch_bits;
        for (uint64_t i = 0; i < n_t; i++)
        {
            unsigned long long start_round = i * round_num / n_t;
            unsigned long long end_round;
            if (i == n_t - 1)
                end_round = round_num;
            else
                end_round = (i + 1) * round_num / n_t;
            threads.emplace_back(worker, start_round, end_round);
        }

        for (auto &t : threads)
            t.join();
    }
    catch (const std::exception &e)
    {
        printf("c [piEPS(round)] exception: %s\n", e.what());
    }

    int res = 0;
    if (!cutted.load())
    {
        if (found_sat.load())
            res = 10;
        else
            res = 20;
    }

    printf("c [piES] result = %d [bv:pbits=%d:%d] [bv_w = %3d]"
           " [nGates = %5lu] [nPI = %3lu] [nBV = %u /t.] [time = %.2f]\n",
           res,
           this->bv_bits,
           this->batch_bits,
           Param::get().custom_params.es_bv_bits,
           xag.used_gates.size(),
           xag.PI.size(),
           is->glob_es.mem_sz,
           ResMgr::get().get_runtime() - start_time);
    fflush(stdout);

    return ret_vals(res);
}

// ---------------------------------------------------
// ES methods from Prover class
// ---------------------------------------------------

fastLEC::ret_vals fastLEC::Prover::para_ES(std::shared_ptr<fastLEC::XAG> xag,
                                           int n_t)
{
    fastLEC::Simulator simu(*xag);

    // default use_pes_pbit = false
    if (Param::get().custom_params.use_pes_pbit)
    {
        int t = simu.cal_pes_threads(n_t);
        return simu.run_pbits_pes(t);
    }
    else
    {
        return simu.run_round_pes(n_t);
    }

    return ret_vals::ret_UNK;
}