#include "fastLEC.hpp"
#include "parser.hpp"
#include "basic.hpp"
#include "simu.hpp"

#include <algorithm>

using namespace fastLEC;

void fastLEC::Simulator::cal_es_bits(unsigned threads_for_es)
{
    unsigned max_bv_bits = Param::get().custom_params.es_bv_bits;
    unsigned n_pi = this->xag.PI.size();

    para_bits = batch_bits = 0;

    if (n_pi <= static_cast<unsigned>(sizeof(fastLEC::bv_unit_t)))
    {
        bv_bits = static_cast<unsigned>(sizeof(fastLEC::bv_unit_t));
    }
    else if (n_pi <= max_bv_bits)
    {
        bv_bits = n_pi;
    }
    else if (n_pi - max_bv_bits <= log2(threads_for_es))
    {
        bv_bits = max_bv_bits;
        para_bits = n_pi - max_bv_bits;
        batch_bits = 0;
    }
    else
    {
        bv_bits = max_bv_bits;
        para_bits = log2(threads_for_es);
        batch_bits = n_pi - max_bv_bits - para_bits;
    }
}

bool fastLEC::Simulator::construct_isimu()
{
    return true;
}

ret_vals fastLEC::Simulator::run_ies()
{

    return ret_vals::ret_UNK;
}

ret_vals fastLEC::Simulator::run_es()
{
    double start_time = ResMgr::get().get_runtime();

    cal_es_bits(1);

    std::vector<BitVector> states(2 * (xag.max_var + 1));

    for (uint64_t i = 0; i < std::min((unsigned)xag.PI.size(), this->bv_bits); i++)
    {
        int lit = xag.PI[i];
        states[lit].resize(1llu << this->bv_bits);
        states[lit].cycle_festival(1llu << (i));
    }

    uint64_t round_num = 1llu << batch_bits;
    uint64_t round = 0;
    for (; round < round_num; round++)
    {
        std::vector<int> refs(2 * (xag.max_var + 1), 0);
        for (uint64_t i = 0; i < std::min((unsigned)xag.PI.size(), this->bv_bits); i++)
            refs[xag.PI[i]] = true;

        // exact count eps number based on the round number;
        // printf("c [EPS] round %8lu: ", round);
        int ct = round;
        for (unsigned i = this->bv_bits; i < xag.PI.size(); i++)
        {
            int lit = xag.PI[i];
            states[lit].resize(1llu << this->bv_bits);
            refs[lit] = true;
            if (ct % 2 == 0)
                states[lit].reset();
            else
                states[lit].set();
            // printf("%d ", ct % 2);
            ct /= 2;
        }
        // printf("\n");

        for (auto &gate : xag.gates)
        {
            if (gate.type == fastLEC::GateType::AND2 || gate.type == fastLEC::GateType::XOR2)
            {
                int rhs0 = gate.inputs[0];
                int rhs1 = gate.inputs[1];
                if (aiger_sign(rhs0) && !refs[rhs0])
                    states[rhs0] = ~states[aiger_not(rhs0)], refs[rhs0] = true;
                if (aiger_sign(rhs1) && !refs[rhs1])
                    states[rhs1] = ~states[aiger_not(rhs1)], refs[rhs1] = true;
                if (gate.type == GateType::AND2)
                    states[gate.output] = states[rhs0] & states[rhs1];
                else
                    states[gate.output] = states[rhs0] ^ states[rhs1];
                refs[gate.output] = true;
            }
        }

        int olit = xag.PO;
        if (!refs[olit])
            states[olit] = ~states[aiger_not(olit)], refs[olit] = true;
        // std::cout << "olit: " << olit << std::endl;
        // std::cout << "states[olit]: " << states[olit] << std::endl;
        if (states[olit].has_one())
        {
            printf("c [EPS] result = 10 [round %lu] [time = %.2f]\n", round, ResMgr::get().get_runtime() - start_time);
            return ret_vals::ret_SAT;
        }
        if (ResMgr::get().get_runtime() > Param::get().timeout)
        {
            printf("c [EPS] round %8lu: %lf %lf\n", round, start_time, ResMgr::get().get_runtime() - start_time);
            break;
        }
    }

    if (round < round_num)
    {
        printf("c [EPS] result = 0 [bv:para:batch=%d:%d:%d] [bv_w = %3d] [nGates = %5lu] [nPI = %3lu] [time = %.2f]\n",
               this->bv_bits, this->para_bits, this->batch_bits,
               Param::get().custom_params.es_bv_bits,
               xag.used_gates.size(), xag.PI.size(),
               ResMgr::get().get_runtime() - start_time);
        return ret_vals::ret_UNK;
    }
    else
    {
        printf("c [EPS] result = 20 [bv:para:batch=%d:%d:%d] [bv_w = %3d] [nGates = %5lu] [nPI = %3lu] [time = %.2f]\n",
               this->bv_bits, this->para_bits, this->batch_bits,
               Param::get().custom_params.es_bv_bits,
               xag.used_gates.size(), xag.PI.size(),
               ResMgr::get().get_runtime() - start_time);
        return ret_vals::ret_UNS;
    }
}

ret_vals fastLEC::Prove_Task::seq_es()
{

    double start_time = ResMgr::get().get_runtime();

    fastLEC::Simulator simu(*xag);

    std::string tag = "ES";

    ret_vals ret = ret_vals::ret_UNK;
    if (Param::get().custom_params.use_ies)
    {
        tag = "iES";
        if (!simu.construct_isimu())
        {
            ret = ret_vals::ret_UNK;
            printf("c [iES] error: failed to construct instruction simulator\n");
        }
        else
            ret = simu.run_ies();
    }
    else
    {
        tag = "ES";
        ret = simu.run_es();
    }

    double end_time = ResMgr::get().get_runtime();
    printf("c [%s] runtime: %f seconds\n", tag.c_str(), end_time - start_time);
    return ret;
}