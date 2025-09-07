#include "AIG.hpp"
#include "parser.hpp"

using namespace fastLEC;

void AIG::set(std::unique_ptr<aiger, std::function<void(aiger *)>> aig)
{
    this->aig = std::move(aig);
}

std::unique_ptr<aiger, std::function<void(aiger *)>> AIG::move()
{
    return std::move(this->aig);
}

auto AIG::create() -> std::unique_ptr<aiger, std::function<void(aiger *)>>
{
    auto deleter = [](aiger *aig)
    {
        if (aig != nullptr)
            aiger_reset(aig);
        aig = nullptr;
    };
    return std::unique_ptr<aiger, std::function<void(aiger *)>>(aiger_init(), deleter);
}

void AIG::rewrite()
{
    std::vector<int> mp(2 * (aig.get()->maxvar + 1), 0);
    for (int i = 0; i < (int)mp.size(); ++i)
        mp[i] = i;
    for (int i = 0; i < (int)aig.get()->num_ands; ++i)
    {
        int lhs = aig.get()->ands[i].lhs, rhs0 = aig.get()->ands[i].rhs0, rhs1 = aig.get()->ands[i].rhs1;
        rhs0 = mp[rhs0];
        rhs1 = mp[rhs1];

        if (rhs0 == 0 || rhs1 == 0)
            mp[lhs] = 0, mp[lhs ^ 1] = 1;
        else if (rhs0 == 1)
            mp[lhs] = rhs1, mp[lhs ^ 1] = rhs1 ^ 1;
        else if (rhs1 == 1)
            mp[lhs] = rhs0, mp[lhs ^ 1] = rhs0 ^ 1;

        aig->ands[i].rhs0 = rhs0;
        aig->ands[i].rhs1 = rhs1;
    }
    for (int i = 0; i < (int)aig->num_outputs; ++i)
        aig->outputs[i].lit = mp[aig->outputs[i].lit];
}

bool AIG::log(const std::string &filename)
{
    std::string aig_log;
    if (filename.empty())
        aig_log = "temp.aig";
    else
        aig_log = filename;
    return aiger_open_and_write_to_file(aig.get(), aig_log.c_str());
}

bool AIG::construct(const std::string &filename)
{
    if (this->aig.get() != nullptr)
    {
        printf("c [AIG] warning: this aiger is already initialized\n");
        this->aig.reset();
    }

    this->aig = this->create();
    if (this->aig.get() == nullptr)
    {
        printf("c [AIG] Failed to initialize AIGER\n");
        return false;
    }

    // read the file
    FILE *fp = nullptr;
    fp = fopen(filename.c_str(), "r");
    if (fp == nullptr)
    {
        fprintf(stderr, "c [AIG] Failed to open AIGER file: %s\n", filename.c_str());
        return false;
    }

    const char *msg = aiger_read_from_file(aig.get(), fp);
    if (msg)
    {
        fprintf(stderr, "c [AIG] error: load AIGER file: %s, msg: %s\n", filename.c_str(), msg);
        return false;
    }

    if (aig->num_latches > 0)
    {
        fprintf(stderr, "c [AIG] error: can not handle latches\n");
        return false;
    }
    if (aig->num_bad > 0)
    {
        fprintf(stderr, "c [AIG] error: can not handle bad state properties\n");
        return false;
    }
    if (aig->num_constraints > 0)
    {
        fprintf(stderr, "c [AIG] error: can not handle environment constraints\n");
        return false;
    }
    if (aig->num_outputs == 0)
    {
        fprintf(stderr, "c [AIG] error: no output\n");
        return false;
    }
    if (aig->num_outputs > 1)
    {
        fprintf(stderr, "c [AIG] error: more than one output\n");
        return false;
    }
    if (aig->num_justice > 0)
    {
        fprintf(stderr, "c [AIG] warning: ignoring justice properties\n");
    }
    if (aig->num_fairness > 0)
    {
        fprintf(stderr, "c [AIG] warning: ignoring fairness constraints\n");
    }

    fclose(fp);

    aiger_reencode(aig.get());
    rewrite();

    if (fastLEC::Param::get().verbose > 0)
    {
        printf("c [AIG] successfully load AIGER file with MILOA: %u %u %u %u %u\n", aig->maxvar, aig->num_inputs, aig->num_latches, aig->num_outputs, aig->num_ands);
        fflush(stdout);
    }
    return true;
}

int fastLEC::aiger_var(const unsigned &aiger_lit)
{
    return aiger_lit2var(aiger_lit);
}

int fastLEC::aiger_pos_lit(const unsigned &aiger_var)
{
    return aiger_var2lit(aiger_var);
}

int fastLEC::aiger_neg_lit(const unsigned &aiger_var)
{
    return aiger_not(aiger_var2lit(aiger_var));
}

bool fastLEC::aiger_value(const unsigned &aiger_lit, const bool &aiger_var_val)
{
    if (aiger_var_val)
        return !aiger_sign(aiger_lit);
    else
        return aiger_sign(aiger_lit);
}

bool fastLEC::aiger_has_same_var(const unsigned &aiger_lit0, const unsigned &aiger_lit1)
{
    return aiger_strip(aiger_lit0) == aiger_strip(aiger_lit1);
}

aiger_and *AIG::is_and(unsigned lit) const
{
    return aiger_is_and(aig.get(), lit);
}

bool AIG::is_xor(unsigned lit, unsigned *rhs0ptr, unsigned *rhs1ptr) const
{
    aiger_and *and_gate = is_and(lit);
    if (!and_gate)
        return false;
    if (!aiger_sign(and_gate->rhs0) || !aiger_sign(and_gate->rhs1))
        return false;
    aiger_and *left = is_and(and_gate->rhs0);
    if (!left)
        return false;
    aiger_and *right = is_and(and_gate->rhs1);
    if (!right)
        return false;
    unsigned left_rhs0 = left->rhs0, left_rhs1 = left->rhs1;
    unsigned right_rhs0 = right->rhs0, right_rhs1 = right->rhs1;
    unsigned not_right_rhs0 = aiger_not(right_rhs0);
    unsigned not_right_rhs1 = aiger_not(right_rhs1);
    //      (!l0 | !l1) & (!r0 | !r1)   
    // (A): ( r0 |  r1) & (!r0 | !r1)
    // (B): ( r1 |  r0) & (!r0 | !r1)
    //        r0 ^  r1                 // used
    //        l0 ^  l1                 // not used
    if ((left_rhs0 == not_right_rhs0 && left_rhs1 == not_right_rhs1) || //(A)
        (left_rhs0 == not_right_rhs1 && left_rhs1 == not_right_rhs0))
    { //(B)
        const unsigned rhs0 = left_rhs0, rhs1 = left_rhs1;
        if (aiger_has_same_var(rhs0, rhs1))
            return false;
        if (rhs0ptr)
            *rhs0ptr = rhs0;
        if (rhs1ptr)
            *rhs1ptr = rhs1;
        return true;
    }
    return false;
}