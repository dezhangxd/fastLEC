#include "vis.hpp"
#include "basic.hpp"
#include "parser.hpp"
#include <cstdio>
#if defined(_WIN32)
#include <io.h>
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#endif

fastLEC::Visualizer::Visualizer(std::shared_ptr<fastLEC::XAG> xag) : xag(xag)
{
    cnf = xag->construct_cnf_from_this_xag();
    cnf->build_watches();
}

std::string fastLEC::Visualizer::gen_basename_str()
{
    std::string dir_file = Param::get().custom_params.log_dir;
    fastLEC::check_dir_and_create(dir_file);
    std::string str = "";
    str += Param::get().filename;
    return dir_file + "/" + str;
}

std::string fastLEC::Visualizer::gen_basename_str(std::vector<int> unit_clauses)
{
    std::string dir_file = Param::get().custom_params.log_dir;
    fastLEC::check_dir_and_create(dir_file);
    std::string str = "";
    str += Param::get().filename;
    for (auto lit : unit_clauses)
    {
        str += "_" + std::to_string(lit);
    }
    return dir_file + "/" + str;
}

void fastLEC::Visualizer::visualize(std::vector<int> unit_clauses)
{

    // 1. BCP and extend unit-clauses & mask;
    std::vector<bool> mask, val;
    mask.resize(cnf->num_vars + 1, false);
    val.resize(cnf->num_vars + 1, false);
    for (auto lit : unit_clauses)
    {
        mask[abs(lit)] = true;
        val[abs(lit)] = (lit > 0);
    }
    std::vector<int> new_lits;

    bool res = cnf->perform_bcp(mask, val, unit_clauses, {}, {}, new_lits);

    if (!res)
    {
        printf("c [vis] this CNF is unsat by propagate\n");
        fflush(stdout);
    }
    for (int l : new_lits)
        unit_clauses.push_back(l);

    std::string basefilename = gen_basename_str();
    for (int l : unit_clauses)
    {
        basefilename += "_" + std::to_string(l);
    }

    // check if the file already exists (cross-platform)
    if (access(basefilename.c_str(), F_OK) != -1)
    {
        printf("c [vis] the file %s already exists\n", basefilename.c_str());
        fflush(stdout);
        return;
    }
    printf("c [vis] the base filename is %s [sz = %zu]\n",
           basefilename.c_str(),
           (size_t)unit_clauses.size());
    fflush(stdout);

    // 2. log the right cnf;

    // 3. run_cnf
}