#include "fastLEC.hpp"
#include "parser.hpp"
#include "basic.hpp"
#include "simu.hpp"

using namespace fastLEC;

ret_vals fastLEC::Prove_Task::seq_es_org()
{
    fastLEC::Simulator simu(xag);
    simu.run();

    return ret_vals::ret_UNK;
}