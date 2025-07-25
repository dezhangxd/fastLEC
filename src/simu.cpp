#include "fastLEC.hpp"
#include "parser.hpp"
#include "basic.hpp"
#include "simu.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <thread>
#include <atomic>

using namespace fastLEC;

bvec_t festivals[6] = {
    0xAAAAAAAAAAAAAAAA,
    0xCCCCCCCCCCCCCCCC,
    0xF0F0F0F0F0F0F0F0,
    0xFF00FF00FF00FF00,
    0xFFFF0000FFFF0000,
    0xFFFFFFFF00000000};

bvec_ts festivals_s[5] = {
    0xAAAAAAAA,
    0xCCCCCCCC,
    0xF0F0F0F0,
    0xFF00FF00,
    0xFFFF0000};

void bvec_set(bvec_t *vec)
{
    *vec = ~0ull;
}

void bvec_reset(bvec_t *vec)
{
    *vec = 0ull;
}

void bvec_set_s(bvec_ts *vec)
{
    *vec = ~0u;
}

void bvec_reset_s(bvec_ts *vec)
{
    *vec = 0u;
}

void fastLEC::ISimulator::prt_bvec(bvec_t *vec)
{
    for (bvec_t i = 0; i < BVEC_SIZE; i++)
    {
        printf("%ld", (*vec >> i) & 1);
    }
    printf("\n");
}

void fastLEC::ISimulator::prt_op(operation *op)
{
    if (op->type == OP_AND)
        printf("AND %u \t%u \t%u\n", op->addr1, op->addr2, op->addr3);
    else if (op->type == OP_XOR)
        printf("XOR %u \t%u \t%u\n", op->addr1, op->addr2, op->addr3);
    else if (op->type == OP_NOT)
        printf("NOT %u \t%u\n", op->addr1, op->addr2);
    else
        printf("UNKNOWN \n");
}

void fastLEC::ISimulator::init_glob_ES(fastLEC::XAG &xag)
{

    std::vector<unsigned> ref_cts(2 * (xag.max_var + 1), 0);
    std::vector<unsigned> mem_addr(2 * (xag.max_var + 1), NOT_ALLOC);
    unsigned max_mems = 0;
    std::vector<unsigned> free_mems_stack;
    std::vector<operation> ops;

    std::function<unsigned(void)> alloc_mem = [&]() -> unsigned
    {
        assert(max_mems <= UINT16_MAX);
        if (free_mems_stack.empty())
            return max_mems++;
        unsigned addr = free_mems_stack.back();
        free_mems_stack.pop_back();
        return addr;
    };

    std::function<void(unsigned)> free_mems = [&](unsigned addr)
    {
        free_mems_stack.push_back(addr);
    };

    // Calculate reference counts
    for (auto &gate : xag.gates)
    {
        if (gate.type == fastLEC::GateType::AND2 || gate.type == fastLEC::GateType::XOR2)
        {
            unsigned rhs0 = gate.inputs[0];
            unsigned rhs1 = gate.inputs[1];
            ref_cts[rhs0]++;
            ref_cts[rhs1]++;
        }
    }
    int olit = xag.PO;
    ref_cts[olit]++;

    // considering not gates
    for (unsigned i = 1; i < (unsigned)xag.max_var + 1; i++)
    {
        unsigned lit = aiger_neg_lit(i);
        if (ref_cts[lit] > 0)
            ref_cts[aiger_not(lit)]++;
    }

    mem_addr[0] = alloc_mem();
    mem_addr[1] = alloc_mem();
    assert(mem_addr[0] == const0_addr);
    assert(mem_addr[1] == const1_addr);

    for (unsigned i = 0; i < xag.PI.size(); i++)
    {
        unsigned lit = xag.PI[i];
        if (ref_cts[lit] == 0)
        {
            printf("c [ERROR] ies glb_es construct: input lit=%u not used in the circuit\n", lit);
            exit(0);
        }
        mem_addr[lit] = alloc_mem();
        assert(lit == i + 2);
    }

    if (ref_cts[0] == 0)
        free_mems(const0_addr);
    if (ref_cts[1] == 0)
        free_mems(const1_addr);

    // Process gates
    for (auto &gate : xag.gates)
    {
        if (gate.type == fastLEC::GateType::AND2 || gate.type == fastLEC::GateType::XOR2)
        {
            unsigned rhs0 = gate.inputs[0];
            unsigned rhs1 = gate.inputs[1];
            unsigned not_rhs0 = aiger_not(rhs0);
            unsigned not_rhs1 = aiger_not(rhs1);
            unsigned out = gate.output;

            // Process rhs0
            if (mem_addr[rhs0] == NOT_ALLOC)
            {
                if (mem_addr[not_rhs0] != NOT_ALLOC) // first use the neg symbol
                {
                    ref_cts[not_rhs0]--;
                    operation op;
                    op.type = OP_NOT;
                    if (ref_cts[not_rhs0] == 0)
                        mem_addr[rhs0] = mem_addr[not_rhs0];
                    else
                        mem_addr[rhs0] = alloc_mem();
                    op.addr1 = mem_addr[rhs0];
                    op.addr2 = mem_addr[not_rhs0];
                    ops.push_back(op);
                }
                else
                {
                    printf("c [ERROR] ies glb_es left construct: rhs0=%u, not_rhs0=%u not found\n", rhs0, not_rhs0);
                    exit(0);
                }
            }

            // Process rhs1
            if (mem_addr[rhs1] == NOT_ALLOC)
            {
                if (mem_addr[not_rhs1] != NOT_ALLOC) // first use the neg symbol
                {
                    ref_cts[not_rhs1]--;
                    operation op;
                    op.type = OP_NOT;
                    if (ref_cts[not_rhs1] == 0)
                        mem_addr[rhs1] = mem_addr[not_rhs1];
                    else
                        mem_addr[rhs1] = alloc_mem();
                    op.addr1 = mem_addr[rhs1];
                    op.addr2 = mem_addr[not_rhs1];
                    ops.push_back(op);
                }
                else
                {
                    printf("c [ERROR] ies glb_es right construct: rhs1=%u, not_rhs1=%u not found\n", rhs1, not_rhs1);
                    exit(0);
                }
            }

            ref_cts[rhs0]--;
            if (ref_cts[rhs0] == 0)
                free_mems(mem_addr[rhs0]);

            ref_cts[rhs1]--;
            if (ref_cts[rhs1] == 0)
                free_mems(mem_addr[rhs1]);

            operation op;

            if (gate.type == fastLEC::GateType::AND2)
                op.type = OP_AND;
            else
                op.type = OP_XOR;

            op.addr1 = mem_addr[out] = alloc_mem();
            op.addr2 = mem_addr[rhs0];
            op.addr3 = mem_addr[rhs1];

            // printf("c [op] ");
            // prt_op(&op);
            // printf("c [op]     %u <- %u %u\n", out, rhs0, rhs1);
            // printf("--------------------------------\n");

            ops.push_back(op);
        }
    }

    // Process output
    unsigned po_lit = xag.PO;
    if (mem_addr[po_lit] == NOT_ALLOC)
    {
        unsigned not_po_lit = aiger_not(po_lit);
        assert(mem_addr[not_po_lit] != NOT_ALLOC);
        operation op;
        ref_cts[not_po_lit]--;
        op.type = OP_NOT;
        op.addr1 = op.addr2 = mem_addr[po_lit] = mem_addr[not_po_lit];
        ops.push_back(op);
    }

    // Set up fast_ES structure
    glob_es.PI_num = xag.PI.size();
    glob_es.PO_lit = mem_addr[po_lit];
    glob_es.mem_sz = max_mems;

    glob_es.n_ops = ops.size();
    glob_es.ops = (operation *)malloc(ops.size() * sizeof(operation));
    if (!glob_es.ops)
    {
        printf("c [ERROR] translate_XAG_to_fast_ES: failed to allocate memory for ops\n");
        exit(0);
    }

    for (unsigned i = 0; i < ops.size(); i++)
        glob_es.ops[i] = ops[i];
}

void fastLEC::ISimulator::init_gpu_ES(fastLEC::XAG &xag, glob_ES **ges)
{
    this->init_glob_ES(xag);

    if ((*ges) == nullptr)
        (*ges) = gpu_init();

    (*ges)->PI_num = glob_es.PI_num;
    (*ges)->PO_lit = glob_es.PO_lit;
    (*ges)->mem_sz = glob_es.mem_sz;
    (*ges)->n_ops = glob_es.n_ops;
    (*ges)->ops = (operation *)malloc(glob_es.n_ops * sizeof(operation));
    memcpy((*ges)->ops, glob_es.ops, glob_es.n_ops * sizeof(operation));
}

void fastLEC::Simulator::cal_es_bits(unsigned threads_for_es)
{
    unsigned max_bv_bits = Param::get().custom_params.es_bv_bits;
    unsigned n_pi = this->xag.PI.size();

    para_bits = batch_bits = 0;

    const unsigned bv_unit_t_bit_len = 8 * sizeof(fastLEC::bv_unit_t);
    const unsigned log_bit = log2(bv_unit_t_bit_len);

    if (n_pi <= log_bit)
    {
        bv_bits = log_bit;
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

int fastLEC::ISimulator::run_ies_round(uint64_t r)
{
    unsigned i, j, k;
    operation *op;
    bvec_t *local_mems;
    local_mems = (bvec_t *)malloc(glob_es.mem_sz * sizeof(bvec_t));

    bvec_reset(&local_mems[0]);
    bvec_reset(&local_mems[1]);
    i = 2;

    if (glob_es.PI_num < BVEC_BIT_WIDTH)
        memcpy(local_mems + i, festivals, glob_es.PI_num * sizeof(bvec_t));
    else
    {
        memcpy(local_mems + i, festivals, BVEC_BIT_WIDTH * sizeof(bvec_t));
        i += BVEC_BIT_WIDTH;

        for (j = BVEC_BIT_WIDTH; j < glob_es.PI_num; j++)
        {
            k = (r >> (j - BVEC_BIT_WIDTH)) & 1ull;
            if (k == 1)
                bvec_set(&local_mems[i]);
            else
                bvec_reset(&local_mems[i]);
            i++;
        }
    }

    // start to simulation
    for (i = 0; i < glob_es.n_ops; i++)
    {
        op = &glob_es.ops[i];

        // printf("c [op] ");
        // prt_op(op);
        // printf("c [op] addr1: %u, addr2: %u, addr3: %u\n", op->addr1, op->addr2, op->addr3);
        // if (op->type != OP_SAVE)
        //     printf("addr2: "), prt_bvec(&local_mems[op->addr2]);
        // if (op->type != OP_SAVE && op->type != OP_NOT)
        //     printf("addr3: "), prt_bvec(&local_mems[op->addr3]);

        if (op->type == OP_AND)
            local_mems[op->addr1] = local_mems[op->addr2] & local_mems[op->addr3];
        else if (op->type == OP_XOR)
            local_mems[op->addr1] = local_mems[op->addr2] ^ local_mems[op->addr3];
        else if (op->type == OP_NOT)
            local_mems[op->addr1] = ~local_mems[op->addr2];

        // printf("addr1: "), prt_bvec(&local_mems[op->addr1]);
        // printf("--------------------------------\n");
    }

    // check result
    int res = 0;
    if (local_mems[glob_es.PO_lit] != 0u)
        res = 10;
    else
        res = 20;

    free(local_mems);

    return res;
}

int fastLEC::ISimulator::run_ies()
{
    // load inputs
    unsigned mem_cost = 0;
    mem_cost += (glob_es.mem_sz) * sizeof(bvec_t);
    mem_cost += sizeof(unsigned) * 3;
    mem_cost += sizeof(operation *);

    if (Param::get().verbose > 0)
    {
        printf("c [iES] Each Thread mems_cost: %d bytes\n", mem_cost);
        fflush(stdout);
    }
    unsigned es_bits = BVEC_BIT_WIDTH;
    unsigned long long round_num = 0;
    if (glob_es.PI_num >= es_bits)
        round_num = 1llu << (glob_es.PI_num - es_bits);
    else
        round_num = 1llu;

    for (unsigned long long r = 0; r < round_num; r++)
    {
        int res = 0;
        if (Param::get().verbose > 1 && r % 100000 == 0){
            printf("c [iES] %6.2f%% : round %lld / %llu \n", (double)r / round_num * 100, r, round_num);
            fflush(stdout);
        }
        res = run_ies_round(r);
        if (res == 10)
            return 10;
    }

    return 20;
}

ret_vals fastLEC::Simulator::run_ies()
{
    double start_time = ResMgr::get().get_runtime();

    assert(is == nullptr);
    is = std::make_unique<fastLEC::ISimulator>();
    is->init_glob_ES(xag);

    int res = 0;
    if (Param::get().custom_params.ies_u64)
    {
        this->bv_bits = 6;
        this->para_bits = 0;
        this->batch_bits = is->glob_es.PI_num > this->bv_bits ? is->glob_es.PI_num - this->bv_bits : 0;
        res = is->run_ies();
    }
    else
    {
        cal_es_bits(1);

        std::vector<BitVector> loc_mem(is->glob_es.mem_sz, BitVector(1 << this->bv_bits));

        unsigned long long round_num = 1llu << batch_bits;
        unsigned long long round = 0;
        for (; round < round_num; round++)
        {
            if (Param::get().verbose > 1 && round % 100000 == 0){
                printf("c [iES] %6.2f%% : round %lld / %llu \n", (double)round / round_num * 100, round, round_num);
                fflush(stdout);
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
                    loc_mem[op->addr1] = loc_mem[op->addr2] & loc_mem[op->addr3];
                else if (op->type == OP_XOR)
                    loc_mem[op->addr1] = loc_mem[op->addr2] ^ loc_mem[op->addr3];
                else if (op->type == OP_NOT)
                    loc_mem[op->addr1] = ~loc_mem[op->addr2];
            }

            if (loc_mem[is->glob_es.PO_lit].has_one())
            {
                res = 10;
                break;
            }
        }
        if (res == 0)
            res = 20;
    }

    fastLEC::ret_vals ret = ret_vals::ret_UNK;
    if (res == 10)
        ret = ret_vals::ret_SAT;
    else if (res == 20)
        ret = ret_vals::ret_UNS;
    else
        ret = ret_vals::ret_UNK;

    printf("c [iES] result = %d [bv:para:batch=%d:%d:%d] [bv_w = %3d] [nGates = %5lu] [nPI = %3lu] [num_bv = %u] [time = %.2f]\n",
           res,
           this->bv_bits, this->para_bits, this->batch_bits,
           Param::get().custom_params.es_bv_bits,
           xag.used_gates.size(), xag.PI.size(),
           is->glob_es.mem_sz,
           ResMgr::get().get_runtime() - start_time);
    return ret;
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
            ct /= 2;
        }

        for (int i : xag.used_gates)
        {
            auto &gate = xag.gates[i];
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
    fastLEC::Simulator simu(*xag);

    ret_vals ret = ret_vals::ret_UNK;
    if (Param::get().custom_params.use_ies)
    {
        ret = simu.run_ies();
    }
    else
    {
        ret = simu.run_es();
    }
    return ret;
}

unsigned fastLEC::Simulator::cal_pes_threads(unsigned n_thread)
{
    if (n_thread <= 0)
        return 0;
    unsigned para_bits = log2(n_thread);
    return 1 << para_bits;
}

fastLEC::ret_vals fastLEC::Simulator::run_pbits_pes(unsigned n_t)
{
    double start_time = ResMgr::get().get_runtime();
    assert(is == nullptr);
    is = std::make_unique<fastLEC::ISimulator>();
    is->init_glob_ES(xag);

    cal_es_bits(n_t);

    std::vector<std::thread> threads;
    std::atomic<bool> found_sat(false);
    std::atomic<bool> cutted(false);

    auto worker = [&](uint64_t para_idx)
    {
        std::vector<BitVector> loc_mem(is->glob_es.mem_sz, BitVector(1 << this->bv_bits));

        unsigned long long round_num = 1llu << batch_bits;
        unsigned long long round = 0;
        for (; round < round_num; round++)
        {
            if (Param::get().verbose > 1 && round % 1000 == 0 && para_idx == 0){
                printf("c [iES] %6.2f%% : round %lld / %llu \n", (double)round / round_num * 100, round, round_num);
                fflush(stdout);
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
                    loc_mem[op->addr1] = loc_mem[op->addr2] & loc_mem[op->addr3];
                else if (op->type == OP_XOR)
                    loc_mem[op->addr1] = loc_mem[op->addr2] ^ loc_mem[op->addr3];
                else if (op->type == OP_NOT)
                    loc_mem[op->addr1] = ~loc_mem[op->addr2];
            }

            if (loc_mem[is->glob_es.PO_lit].has_one())
            {
                found_sat.store(true);
                break;
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
        printf("c [pEPS] exception: %s\n", e.what());
    }

    int res = 0;
    if (!cutted.load())
    {
        if (found_sat.load())
            res = 10;
        else
            res = 20;
    }

    printf("c [piES] result = %d [bv:para:batch=%d:%d:%d] [bv_w = %3d] [nGates = %5lu] [nPI = %3lu] [num_bv = %u] [time = %.2f]\n",
           res,
           this->bv_bits, this->para_bits, this->batch_bits,
           Param::get().custom_params.es_bv_bits,
           xag.used_gates.size(), xag.PI.size(),
           is->glob_es.mem_sz,
           ResMgr::get().get_runtime() - start_time);

    if (res == 0)
        return ret_vals::ret_UNK;
    else if (res == 10)
        return ret_vals::ret_SAT;
    else
        return ret_vals::ret_UNS;
}

fastLEC::ret_vals fastLEC::Simulator::run_round_pes(unsigned n_t)
{
    double start_time = ResMgr::get().get_runtime();
    assert(is == nullptr);
    is = std::make_unique<fastLEC::ISimulator>();
    is->init_glob_ES(xag);
    cal_es_bits(1);

    std::vector<std::thread> threads;
    std::atomic<bool> found_sat(false);
    std::atomic<bool> cutted(false);

    auto worker = [&](unsigned long long start_round, unsigned long long end_round)
    {
        std::vector<BitVector> loc_mem(is->glob_es.mem_sz, BitVector(1 << this->bv_bits));

        for (unsigned long long round = start_round; round < end_round; round++)
        {
            if (Param::get().verbose > 1 && (round - start_round) % 1000 == 0 && start_round == 0){
                printf("c [iES] %6.2f%% : round %lld / %llu \n", 
                    (double)(round - start_round) / (end_round - start_round) * 100, round - start_round, end_round - start_round);
                fflush(stdout);
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
                    loc_mem[op->addr1] = loc_mem[op->addr2] & loc_mem[op->addr3];
                else if (op->type == OP_XOR)
                    loc_mem[op->addr1] = loc_mem[op->addr2] ^ loc_mem[op->addr3];
                else if (op->type == OP_NOT)
                    loc_mem[op->addr1] = ~loc_mem[op->addr2];
            }

            if (loc_mem[is->glob_es.PO_lit].has_one())
            {
                found_sat.store(true);
                break;
            }
        }
    };

    try
    {
        fflush(stdout);
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
        printf("c [pEPS] exception: %s\n", e.what());
    }

    int res = 0;
    if (!cutted.load())
    {
        if (found_sat.load())
            res = 10;
        else
            res = 20;
    }

    printf("c [piES] result = %d [bv:round:batch=%d:%d] [bv_w = %3d] [nGates = %5lu] [nPI = %3lu] [num_bv = %u] [time = %.2f]\n",
           res,
           this->bv_bits, this->batch_bits,
           Param::get().custom_params.es_bv_bits,
           xag.used_gates.size(), xag.PI.size(),
           is->glob_es.mem_sz,
           ResMgr::get().get_runtime() - start_time);

    if (res == 0)
        return ret_vals::ret_UNK;
    else if (res == 10)
        return ret_vals::ret_SAT;
    else
        return ret_vals::ret_UNS;
}

fastLEC::ret_vals fastLEC::Prove_Task::para_es(int n_thread)
{
    fastLEC::Simulator simu(*xag);

    if (Param::get().custom_params.use_pes_pbit)
    {
        int n_t = simu.cal_pes_threads(n_thread);
        return simu.run_pbits_pes(n_t);
    }
    else
    {
        return simu.run_round_pes(n_thread);
    }

    return ret_vals::ret_UNK;
}

fastLEC::ret_vals fastLEC::Simulator::run_ges()
{
    double start_time = ResMgr::get().get_runtime();
    assert(is == nullptr);
    is = std::make_unique<fastLEC::ISimulator>();
    glob_ES *ges = nullptr;
    is->init_gpu_ES(xag, &ges);

    int res = gpu_run(ges, Param::get().verbose);
    printf("c [gpuES] result = %d [time = %.2f]\n", res, ResMgr::get().get_runtime() - start_time);
    if (ges != nullptr)
        free_gpu(ges);

    if (res == 20)
        return ret_vals::ret_UNS;
    else if (res == 10)
        return ret_vals::ret_SAT;
    else
        return ret_vals::ret_UNK;
}

fastLEC::ret_vals fastLEC::Prove_Task::gpu_es()
{
    fastLEC::Simulator simu(*xag);
    return simu.run_ges();
}
