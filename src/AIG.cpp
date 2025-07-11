#include "AIG.hpp"
#include "parser.hpp"

using namespace fastLEC;

void AIG::set(std::shared_ptr<aiger> aig)
{
    this->aig = aig;
}

std::shared_ptr<aiger> AIG::get()
{
    return this->aig;
}

std::shared_ptr<aiger> AIG::create()
{
    return std::shared_ptr<aiger>(aiger_init(), [](aiger *aig)
                                  {
                                      if (aig != nullptr)
                                          aiger_reset(aig);
                                      aig = nullptr; });
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

    if (fastLEC::Param::get().verbose > 0)
        printf("c [AIG] load AIGER file with MILOA: %u %u %u %u %u\n", aig->maxvar, aig->num_inputs, aig->num_latches, aig->num_outputs, aig->num_ands);

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
        printf("c [AIG] successfully load AIGER file: %s\n", filename.c_str());

    return true;
}