#include "fastLEC.hpp"
#include "parser.hpp"
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <string>

using namespace fastLEC;

void Sweeper::clear()
{
    next_class_idx = 0;
    class_index.clear();
    eql_classes.clear();
    skip_pairs.clear();
    proved_pairs.clear();
    rejected_pairs.clear();
}

// ---------------------------------------------------
// logic simulation
// ---------------------------------------------------
fastLEC::ret_vals fastLEC::Sweeper::logic_simulation()
{
    double start_time = ResMgr::get().get_runtime();
    ret_vals ret = ret_vals::ret_UNK;

    this->clear();
    std::vector<int> class_index(2 * (this->xag->max_var + 1), -1);

    unsigned pre_round = 0;
    unsigned round = 0;
    unsigned logic_sim_round = (unsigned)Param::get().custom_params.ls_round;
    unsigned bv_width = (unsigned)Param::get().custom_params.ls_bv_bits;
    for (; round < logic_sim_round; round++)
    {
        // ---------------------------------------------------------------------
        // step 1: perform logic simulation
        // ---------------------------------------------------------------------
        std::vector<BitVector> states(2 * (this->xag->max_var + 1));

        for (unsigned i = 0; i < this->xag->PI.size(); i++)
        {
            int lit = this->xag->PI[i];
            states[lit].resize(1llu << (bv_width - 1));
            states[lit].random();
        }

        for (auto &gate : this->xag->gates)
        {
            if (gate.type == fastLEC::GateType::AND2 ||
                gate.type == fastLEC::GateType::XOR2)
            {
                int rhs0 = gate.inputs[0];
                int rhs1 = gate.inputs[1];
                if (aiger_sign(rhs0) && states[rhs0].size() == 0)
                    states[rhs0] = ~states[aiger_not(rhs0)];
                if (aiger_sign(rhs1) && states[rhs1].size() == 0)
                    states[rhs1] = ~states[aiger_not(rhs1)];
                if (gate.type == fastLEC::GateType::AND2)
                {
                    states[gate.output] = states[rhs0] & states[rhs1];
                }
                else
                {
                    states[gate.output] = states[rhs0] ^ states[rhs1];
                }
                states[aiger_not(gate.output)] = ~states[gate.output];
            }
        }
        if (states[this->xag->PO].has_one())
        {
            ret = ret_vals::ret_SAT;
            break;
        }

        // ---------------------------------------------------------------------
        // step 2: perform classification
        // ---------------------------------------------------------------------

        std::unordered_map<BitVector, std::vector<int>> classification;
        classification.clear();
        if (round == 0)
        {
            for (unsigned lit = 2; lit < states.size(); lit += 2)
            {
                if (states[lit].size() == 0)
                    continue;

                classification[states[lit]].push_back(lit);
            }
        }
        else
        {
            for (unsigned lit = 2; lit < states.size(); lit += 2)
            {
                if (class_index[lit] == -1)
                    continue;

                class_index[lit] = -1;
                classification[states[lit]].push_back(lit);
            }
        }

        eql_classes.clear();
        for (auto &pair : classification)
        {
            const auto &indices = pair.second;
            if (indices.size() <= 1)
                continue;

            for (auto lit : indices)
                class_index[lit] = eql_classes.size();

            eql_classes.emplace_back(indices);
        }

        if (round > 0)
        {
            if (pre_round == eql_classes.size())
                break;
        }
        pre_round = eql_classes.size();
    }

    if (ret == ret_vals::ret_SAT)
    {
        printf("c [logSim] round %d: Find bugs\n", round);
        return ret;
    }

    printf(
        "c [logSim] round %d: Find %lu classes\n", round, eql_classes.size());

    // ---------------------------------------------------------------------
    // step 3: perform topological sorting
    // ---------------------------------------------------------------------
    this->xag->topological_sort();

    int tmp_ct = 0;
    auto tmp_eql_classes = eql_classes;
    eql_classes.clear();
    for (auto &cls : tmp_eql_classes)
    {
        printf("c [class %5d] vars:", ++tmp_ct);
        for (auto &lit : cls)
            printf(" %d", lit / 2);
        std::cout << std::endl;

        if (cls.size() > 2)
        {
            for (unsigned i = 0; i < cls.size() - 1; i++)
            {
                for (unsigned j = i + 1; j < cls.size(); j++)
                {
                    int l1 = cls[i];
                    int l2 = cls[j];
                    eql_classes.emplace_back(std::vector<int>{l1, l2});
                }
            }
        }
        else
        {
            eql_classes.emplace_back(cls);
        }
    }
    std::vector<int> &topo = this->xag->topo_idx;

    std::sort(eql_classes.begin(),
              eql_classes.end(),
              [topo](const std::vector<int> &a, const std::vector<int> &b)
              {
                  int v11 = aiger_var(a[1]);
                  int v21 = aiger_var(b[1]);
                  int v10 = aiger_var(a[0]);
                  int v20 = aiger_var(b[0]);
                  int topo_v1 = std::max(topo[v11], topo[v10]);
                  int topo_v2 = std::max(topo[v21], topo[v20]);
                  return topo_v1 < topo_v2;
              });

    // fast structrual hashing
    bool strash_prune_enabled = true;
    if (strash_prune_enabled)
    {
        int deleted_ct = 0;
        xag->fast_compute_varcone_sizes();

        if (Param::get().verbose > 2)
        {
            for (unsigned i = 0; i < eql_classes.size(); i++)
            {
                printf("c [id: %5i] l{%5d, %5d}, v{%5d, %5d}"
                       " -> cone={%5d, %5d}\n",
                       i,
                       eql_classes[i][0],
                       eql_classes[i][1],
                       eql_classes[i][0] / 2,
                       eql_classes[i][1] / 2,
                       xag->varcone_sizes[eql_classes[i][0] / 2],
                       xag->varcone_sizes[eql_classes[i][1] / 2]);
            }
        }

        int id = 0;
        for (unsigned i = 0; i < eql_classes.size() - 1; i++)
        {
            int u1 = eql_classes[i][0];
            int u2 = eql_classes[i][1];
            printf("c [id: %5i] l{%5d, %5d}, v{%5d, %5d} "
                   "-> cone={%5d, %5d} |del| ",
                   ++id,
                   eql_classes[i][0],
                   eql_classes[i][1],
                   eql_classes[i][0] / 2,
                   eql_classes[i][1] / 2,
                   xag->varcone_sizes[eql_classes[i][0] / 2],
                   xag->varcone_sizes[eql_classes[i][1] / 2]);

            int prt_cnt = 0;
            for (unsigned j = i + 1; j < eql_classes.size(); j++)
            {
                int v1 = eql_classes[j][0];
                int v2 = eql_classes[j][1];
                if ((xag->strash_prune(u1, v1) && xag->strash_prune(u2, v2)) ||
                    (xag->strash_prune(u1, v2) && xag->strash_prune(u2, v1)))
                {
                    this->skip_pairs[i].push_back({v1, v2});
                    if (++prt_cnt == 11)
                    {
                        printf("\nc%75c", ' ');
                        prt_cnt = 1;
                    }
                    printf("v{%5d, %5d} ", v1 / 2, v2 / 2);

                    if (Param::get().verbose > 2)
                    {
                        printf("c [netlist] strash class{%d, %d} and class{%d, "
                               "%d}\n",
                               u1,
                               u2,
                               v1,
                               v2);
                    }
                    eql_classes.erase(eql_classes.begin() + j);
                    j--;
                    deleted_ct++;
                }
            }
            printf("\n");
        }
        printf("c [strash] deleted %d classes by strash, remain %lu "
               "potentail-eql classes\n",
               deleted_ct,
               eql_classes.size());
    }

    this->xag->init_var_replace();

    printf("c [netlist] logic simulation done. ret = %d [time = %.2f]\n",
           ret,
           ResMgr::get().get_runtime() - start_time);
    fflush(stdout);

    return ret;
}

std::shared_ptr<fastLEC::XAG> Sweeper::next_sub_graph()
{
    std::shared_ptr<fastLEC::XAG> sub_xag = nullptr;
    sub_graph_string = "";
    if (this->next_class_idx == this->eql_classes.size())
    {
        sub_xag = this->xag->extract_sub_graph({this->xag->PO});
        std::ostringstream oss;
        oss << "c*[ Final ] PO-lit{" << std::to_string(this->xag->PO)
            << "}, v{ " << aiger_var(this->xag->PO) << "}, cone={ "
            << xag->varcone_sizes[aiger_var(this->xag->PO)] << "}"
            << ", PI= " << sub_xag->PI.size();
        sub_graph_string += oss.str();
    }
    else if (this->next_class_idx < this->eql_classes.size())
    {
        sub_xag = this->xag->extract_sub_graph(
            this->eql_classes[this->next_class_idx]);
        std::ostringstream oss;
        oss << "c*[" << std::setw(4) << (next_class_idx + 1) << "/"
            << std::setw(4) << eql_classes.size() << "] l{" << std::setw(5)
            << eql_classes[this->next_class_idx][0] << ", " << std::setw(5)
            << eql_classes[this->next_class_idx][1] << "}, v={" << std::setw(5)
            << (eql_classes[this->next_class_idx][0] / 2) << ", "
            << std::setw(5) << (eql_classes[this->next_class_idx][1] / 2)
            << "}, cone={" << std::setw(5)
            << xag->varcone_sizes[eql_classes[this->next_class_idx][0] / 2]
            << ", " << std::setw(5)
            << xag->varcone_sizes[eql_classes[this->next_class_idx][1] / 2]
            << "}, PI= " << sub_xag->PI.size();
        sub_graph_string += oss.str();
    }

    this->next_class_idx++;

    if (sub_xag && Param::get().custom_params.log_sub_aiger)
    {
        this->tmp_next_graph = sub_xag;
        this->log_next_sub_aiger();
        this->tmp_next_graph = nullptr;
    }
    if (sub_xag && Param::get().custom_params.log_sub_cnfs)
    {
        this->tmp_next_graph = sub_xag;
        this->log_next_sub_cnfs();
        this->tmp_next_graph = nullptr;
    }

    return sub_xag;
}

void Sweeper::log_next_sub_cnfs()
{
    auto cnf = tmp_next_graph->construct_cnf_from_this_xag();

    std::string log_file = Param::get().custom_params.log_dir;
    log_file += "/" + Param::get().filename;
    log_file += "_" + std::to_string(this->next_class_idx) + ".cnf";
    printf("c [log] log file: %s\n", log_file.c_str());
    cnf->log_cnf(log_file);
}

void Sweeper::log_next_sub_aiger()
{
    auto aig = tmp_next_graph->construct_aig_from_this_xag();

    std::string log_file = Param::get().custom_params.log_dir;
    log_file += "/" + Param::get().filename;
    log_file += "_" + std::to_string(this->next_class_idx) + ".aig";
    printf("c [log] log file: %s\n", log_file.c_str());

    aig->log(log_file);
}

void Sweeper::post_proof(fastLEC::ret_vals ret)
{
    if (ret == ret_vals::ret_UNK &&
        (Param::get().custom_params.log_sub_aiger ||
         Param::get().custom_params.log_sub_cnfs))
        ret = ret_vals::ret_UNS;

    unsigned last_id = this->next_class_idx - 1;
    if (last_id >= this->eql_classes.size())
        return;

    int l1 = this->eql_classes[last_id][0];
    int l2 = this->eql_classes[last_id][1];
    std::pair<int, int> pair = std::make_pair(l1, l2);
    if (ret == ret_vals::ret_UNS)
    {
        this->proved_pairs.emplace_back(pair);
        for (auto &p : this->skip_pairs[last_id])
            this->proved_pairs.emplace_back(p);

        std::function<int(int)> find_f = [&](int v) -> int
        {
            if (this->xag->var_replace[v] != v)
                this->xag->var_replace[v] = find_f(this->xag->var_replace[v]);

            return this->xag->var_replace[v];
        };

        // merge the larger topological order to the smaller one
        int rootX = find_f(aiger_var(l1));
        int rootY = find_f(aiger_var(l2));
        // if (netlist.topo_idx[rootX] < netlist.topo_idx[rootY])
        if (rootX < rootY)
            this->xag->var_replace[rootY] = rootX;
        else
            this->xag->var_replace[rootX] = rootY;

        for (auto &p : this->skip_pairs[last_id])
        {
            rootX = find_f(aiger_var(p.first));
            rootY = find_f(aiger_var(p.second));
            // if (netlist.topo_idx[rootX] < netlist.topo_idx[rootY])
            if (rootX < rootY)
                this->xag->var_replace[rootY] = rootX;
            else
                this->xag->var_replace[rootX] = rootY;
        }
    }
    else if (ret == ret_vals::ret_SAT)
    {
        this->rejected_pairs.emplace_back(pair);
        for (auto &p : this->skip_pairs[last_id])
            this->rejected_pairs.emplace_back(p);
    }
    else
    {
        printf("c error or timeout: [post proof] unknown result\n");
        fflush(stdout);
    }
}
