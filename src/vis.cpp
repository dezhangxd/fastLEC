#include "vis.hpp"
#include "basic.hpp"
#include "parser.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <limits.h>
#include <vector>
#include <string>
#include <iostream>
#include <mutex>
#include <iomanip>
#include <algorithm>

#if defined(_WIN32)
#include <io.h>
#define F_OK 0
#define access _access
#include <windows.h>
#else
#include <unistd.h>
#include <libgen.h>
#endif

void fastLEC::Visualizer::re_compute_scores(dot_data &dot_data)
{
    this->xag->compute_XOR_chains(
        dot_data.mask, dot_data.XOR_chains, dot_data.important_nodes);

    dot_data.is_important.clear();
    dot_data.is_important.resize(cnf->num_vars, false);
    for (auto &d : dot_data.important_nodes)
        for (auto v : d)
            dot_data.is_important[v] = true;

    this->xag->compute_distance(dot_data.mask,
                                dot_data.in_degree,
                                dot_data.out_degree,
                                dot_data.idis,
                                dot_data.odis);

    dot_data.scores.clear();
    dot_data.scores.resize(this->cnf->num_vars + 1, 1.0);

    for (unsigned i = 0; i < dot_data.XOR_chains.size(); i++)
    {
        for (auto v : dot_data.XOR_chains[i])
        {
            double alpha = 0.6;
            double dis =
                alpha * dot_data.odis[v] + (1 - alpha) * dot_data.idis[v] + 1.0;
            dot_data.scores[v] = std::pow(dot_data.XOR_chains[i].size(), 2.0) *
                log2(this->cnf->num_vars) / dis;
        }
        for (auto v : dot_data.important_nodes[i])
        {
            dot_data.scores[v] *= 5.0;
        }
    }

    int prop_lev = 2;
    double beta = 10.0;
    for (int i = 0; i < prop_lev; i++)
    {
        std::vector<double> score_bac = dot_data.scores;
        for (int v = 1; v <= this->cnf->num_vars; v++)
        {
            if (!dot_data.mask[v])
            {
                double sum = 0.0;
                double cnt = 0;
                for (auto u : this->xag->v_usr[v])
                {
                    if (!dot_data.mask[u])
                    {
                        sum += score_bac[u];
                        cnt += 1;
                    }
                }
                int i1 = aiger_var(this->xag->gates[v].inputs[0]);
                int i2 = aiger_var(this->xag->gates[v].inputs[1]);
                if (!dot_data.mask[i1])
                {
                    sum += score_bac[i1];
                    cnt += 1;
                }
                if (!dot_data.mask[i2])
                {
                    sum += score_bac[i2];
                    cnt += 1;
                }

                sum += beta * cnt * score_bac[v];
                cnt += beta * cnt;

                dot_data.scores[v] = sum / cnt;
            }
        }
    }

    // Calculate the rank of each variable according to scores and store in
    // dot_data.score_rank
    int n_vars = this->cnf->num_vars;
    dot_data.score_rank.clear();
    dot_data.score_rank.resize(n_vars + 1, 0);

    // Create vector of pairs (score, variable id)
    std::vector<std::pair<double, int>> score_with_idx;
    for (int v = 1; v <= n_vars; v++)
    {
        score_with_idx.emplace_back(dot_data.scores[v], v);
    }

    // Sort descending by score (higher score = higher rank)
    std::sort(
        score_with_idx.begin(),
        score_with_idx.end(),
        [](const std::pair<double, int> &a, const std::pair<double, int> &b)
        {
            return a.first > b.first;
        });

    // Assign rank: rank 1 for highest score, 2 for next, etc.
    for (int rank = 0; rank < (int)score_with_idx.size(); ++rank)
    {
        int v = score_with_idx[rank].second;
        dot_data.score_rank[v] = rank + 1;
    }

    // Calculate the rank of each variable according to speedups and store in
    // dot_data.speedup_rank
    dot_data.speedup_rank.clear();
    dot_data.speedup_rank.resize(n_vars + 1, 0);

    // Create vector of pairs (speedup, variable id)
    std::vector<std::pair<double, int>> speedup_with_idx;
    for (int v = 1; v <= n_vars; v++)
    {
        speedup_with_idx.emplace_back(
            (dot_data.neg_runtimes[v] > 0.0 && dot_data.pos_runtimes[v] > 0.0)
                ? (dot_data.base_runtime /
                   std::max(dot_data.pos_runtimes[v], dot_data.neg_runtimes[v]))
                : -1e9,
            v);
    }

    // Sort descending by speedup (higher speedup = higher rank)
    std::sort(
        speedup_with_idx.begin(),
        speedup_with_idx.end(),
        [](const std::pair<double, int> &a, const std::pair<double, int> &b)
        {
            return a.first > b.first;
        });

    // Assign rank: rank 1 for highest speedup, 2 for next, etc.
    for (int rank = 0; rank < (int)speedup_with_idx.size(); ++rank)
    {
        int v = speedup_with_idx[rank].second;
        dot_data.speedup_rank[v] = rank + 1;
    }
}

fastLEC::Visualizer::Visualizer(std::shared_ptr<fastLEC::XAG> xag) : xag(xag)
{
    cnf = xag->construct_cnf_from_this_xag();
    cnf->build_watches();
}

void fastLEC::Visualizer::generate_dot(dot_data &dot_data)
{
    std::ofstream fout(dot_data.dot_filename);
    fout << "digraph G {\n";
    fout << "  rankdir=LR;\n";
    fout << "  node [shape=box, style=filled];\n";
    fout << "  edge [arrowhead=vee];\n\n";
    dot_data.scores.resize(cnf->num_vars + 1, 0.0);
    dot_data.speedups.resize(cnf->num_vars + 1, 0.0);
    this->re_compute_scores(dot_data);
    this->xag->compute_n_step_XOR_cnt(
        dot_data.mask, dot_data.n_step_XOR_cnt, dot_data.n_step_Gates_cnt, 10);

    // -----------------------------
    // update visualization data
    std::vector<bool> should_vis(cnf->num_vars + 1, false);
    for (int v = 1; v <= cnf->num_vars; v++)
    {
        if (((dot_data.n_step_XOR_cnt[0][v] +
              (dot_data.n_step_Gates_cnt[0][v] -
               dot_data.n_step_XOR_cnt[0][v]) /
                  4) >= 1) &&
            ((dot_data.n_step_XOR_cnt[1][v] +
              (dot_data.n_step_Gates_cnt[1][v] -
               dot_data.n_step_XOR_cnt[1][v]) /
                  4) >= 2) &&
            ((dot_data.n_step_XOR_cnt[2][v] +
              (dot_data.n_step_Gates_cnt[2][v] -
               dot_data.n_step_XOR_cnt[2][v]) /
                  4) >= 3))
        {
            should_vis[v] = true;
        }
    }
    // --------------------------------

    for (int v = 1; v <= cnf->num_vars; v++)
    {
        std::stringstream ss;
        double speedup_value = dot_data.base_runtime /
            std::max(dot_data.pos_runtimes[v], dot_data.neg_runtimes[v]);
        dot_data.speedups[v] = speedup_value;
        ss << std::fixed << std::setprecision(3) << speedup_value;
        std::string speedup = ss.str();

        std::string fillcolor = "white";
        if (speedup_value < 1.0)
            fillcolor = "#FFFFFF"; // while
        else if (speedup_value >= 1.05 && speedup_value < 1.5)
            fillcolor = "#FFD700"; // yellow
        else if (speedup_value >= 1.5 && speedup_value < 2.0)
            fillcolor = "#90EE90"; // light green
        else if (speedup_value >= 2.0 && speedup_value < 3.0)
            fillcolor = "#87CEEB"; // sky blue
        else if (speedup_value >= 3.0 && speedup_value < 4.0)
            fillcolor = "#FF6B6B"; // red
        else if (speedup_value >= 4.0)
            fillcolor = "#8B008B"; // deep purple

        std::string prefix = "";
        double penwidth = 5.0;
        std::string style = "filled,bold";
        std::string fontcolor = "black";
        std::string shape = "box";
        std::string bordercolor = "black";

        if (xag->gates[v].type == fastLEC::GateType::XOR2)
        {
            prefix = "XOR_";
            penwidth = 6.0;
            style = "filled,bold";
        }
        else if (xag->gates[v].type == fastLEC::GateType::AND2)
        {
            prefix = "AND_";
            penwidth = 2.0;
            style = "filled,bold,dashed";
        }
        else if (xag->gates[v].type == fastLEC::GateType::PI)
        {
            prefix = "PI_";
            penwidth = 1.0;
            style = "filled,bold,rounded,dashed";
        }

        if (dot_data.score_rank[v] <= 10)
        {
            bordercolor = "red";
        }

        if (should_vis[v] == false)
        {
            style = "invis";
        }

        int i1 = aiger_var(xag->gates[v].inputs[0]);
        int i2 = aiger_var(xag->gates[v].inputs[1]);

        std::stringstream ts;
        if (xag->gates[v].type == fastLEC::GateType::PI)
            ts << prefix << v << "\\n";
        else
            ts << prefix << v << "<-"
               << std::to_string(i1) + "," + std::to_string(i2) << "\\n";

        ts << "sp:" << std::fixed << std::setprecision(2)
           << dot_data.speedups[v] << "x, rk=" << dot_data.speedup_rank[v]
           << "\\n";

        ts << "scr:" << std::fixed << std::setprecision(2) << dot_data.scores[v]
           << ", rk=" << dot_data.score_rank[v] << "\\n";

        ts << "X/G:";
        for (int step = 0; step <= 5; step++)
        {
            ts << dot_data.n_step_XOR_cnt[step][v] << "/"
               << dot_data.n_step_Gates_cnt[step][v] << ",";
        }
        ts << "\\n";

        ts << "cut-point ? " << (dot_data.is_important[v] ? "Y" : "N") << "\\n";

        ts << "IO-dis=" << dot_data.idis[v] << "+" << dot_data.odis[v] << "="
           << dot_data.idis[v] + dot_data.odis[v]
           << ", deg=" << dot_data.out_degree[v] << "\\n";

        fout << "  " << v << " [label=\"" << ts.str() << "\""
             << ", style=\"" << style << "\""
             << ", fillcolor=\"" << fillcolor << "\""
             << ", color=\"" << bordercolor << "\""
             << ", penwidth=" << penwidth << ", fontcolor=\"" << fontcolor
             << "\""
             << ", fontname=\"bold\""
             << ", shape=\"" << shape << "\"];\n";
    }

    fout << "\n";

    // Add edges between gates with negative indicators
    for (int gid : xag->used_gates)
    {
        Gate &gate = xag->gates[gid];
        if (gate.type != fastLEC::GateType::PI)
        {
            // First input

            int v_i1 = aiger_var(gate.inputs[0]);
            int v_i2 = aiger_var(gate.inputs[1]);
            int v_o = aiger_var(gate.output);

            int l_i1 = aiger_sign(gate.inputs[0]) ? -v_i1 : v_i1;
            int l_i2 = aiger_sign(gate.inputs[1]) ? -v_i2 : v_i2;

            fout << "  " << v_i1 << " -> " << v_o;
            if (l_i1 < 0)
            {
                if (should_vis[v_i1] == false || should_vis[v_o] == false)
                    fout
                        << " [headlabel=\"●\", labeldistance=0.5, style=invis]";
                else
                    fout << " [headlabel=\"●\", labeldistance=0.5]";
            }
            else
            {
                if (should_vis[v_i1] == false || should_vis[v_o] == false)
                    fout << " [labeldistance=0.5, style=invis]";
                else
                    fout << " [labeldistance=0.5]";
            }
            fout << ";\n";

            // Second input
            fout << "  " << v_i2 << " -> " << v_o;
            if (l_i2 < 0)
            {
                if (should_vis[v_i2] == false || should_vis[v_o] == false)
                    fout
                        << " [headlabel=\"●\", labeldistance=0.5, style=invis]";
                else
                    fout << " [headlabel=\"●\", labeldistance=0.5]";
            }
            else
            {
                if (should_vis[v_i2] == false || should_vis[v_o] == false)
                    fout << " [labeldistance=0.5, style=invis]";
                else
                    fout << " [labeldistance=0.5]";
            }
            fout << ";\n";
        }
    }

    fout << "}\n";

    fout.close();
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

    // 2. log the right cnf;
    // check if the file already exists (cross-platform)
    bool should_log_cnf = true;
    if (access((basefilename + ".cnf").c_str(), F_OK) != -1)
    {
        should_log_cnf = false;
    }
    if (should_log_cnf)
    {
        std::ofstream file(basefilename + ".cnf");
        file << "p cnf " << cnf->num_vars << " "
             << cnf->num_clauses() + unit_clauses.size() << std::endl;
        int start_pos = 0, end_pos = 0;
        for (int i = 0; i < cnf->num_clauses(); i++)
        {
            end_pos = cnf->cls_end_pos[i];
            for (int j = start_pos; j < end_pos; j++)
                file << cnf->lits[j] << " ";
            start_pos = end_pos;
            file << "0" << std::endl;
        }
        for (int l : unit_clauses)
        {
            file << l << " 0" << std::endl;
        }
        file.close();
    }

    // 3. run_cnf and gen logfiles
    bool should_run_cnf = true;
    if (access((basefilename + ".log").c_str(), F_OK) != -1)
    {
        should_run_cnf = false;
    }
    if (should_run_cnf)
    {
        int nthread = Param::get().n_threads;
        int available_threads = 200;
#if defined(__unix__) || defined(__APPLE__)
        long cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
        if (cpu_cores > 0)
        {
            // calculate available threads: keep half CPU cores for other tasks,
            // minus current threads
            available_threads = static_cast<int>(cpu_cores) / 2 - nthread;
            // make sure at least 1 and no more than 200
            if (available_threads <= 0)
                available_threads = 1;
            if (available_threads > 200)
                available_threads = 200;
        }
#endif

        // find the path to mpi_para executable
        std::string mpi_para_path = "";
        bool found_mpi_para = false;

        // try multiple possible paths (by priority)
        std::vector<std::string> possible_paths;

        // 1. try from environment variable project root first
        const char *project_root = std::getenv("FASTLEC_ROOT");
        if (project_root != nullptr)
        {
            std::string env_path =
                std::string(project_root) + "/toolbox/mpi_para";
            possible_paths.push_back(env_path);
        }

        // 2. try relative paths (from project root, assuming run from
        // build/bin)
        possible_paths.push_back("../../toolbox/mpi_para");
        possible_paths.push_back("../toolbox/mpi_para");

        // 3. try from current working directory
        possible_paths.push_back("./toolbox/mpi_para");
        possible_paths.push_back("toolbox/mpi_para");

        // 4. try absolute path if toolbox is in project root
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr)
        {
            std::string cwd_str(cwd);
            // if in /build/bin, go up two levels to project root
            if (cwd_str.find("/build/bin") != std::string::npos)
            {
                size_t pos = cwd_str.find("/build/bin");
                std::string root = cwd_str.substr(0, pos);
                possible_paths.push_back(root + "/toolbox/mpi_para");
            }
            // if in /build, go up one level to project root
            else if (cwd_str.find("/build") != std::string::npos &&
                     cwd_str.find("/build/bin") == std::string::npos)
            {
                size_t pos = cwd_str.find("/build");
                std::string root = cwd_str.substr(0, pos);
                possible_paths.push_back(root + "/toolbox/mpi_para");
            }
            // if in project root directory
            else
            {
                possible_paths.push_back(cwd_str + "/toolbox/mpi_para");
            }
        }
#endif

        // check each possible path
        for (const auto &path : possible_paths)
        {
            if (access(path.c_str(), F_OK) == 0)
            {
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
                // check if executable
                if (access(path.c_str(), X_OK) == 0)
                {
                    mpi_para_path = path;
                    found_mpi_para = true;
                    break;
                }
#else
                mpi_para_path = path;
                found_mpi_para = true;
                break;
#endif
            }
        }

        if (!found_mpi_para)
        {
            std::cerr << "[Error] mpi_para executable not found. Please ensure "
                         "it is built in toolbox/ directory."
                      << std::endl;
            std::cerr << "[Error] Tried paths:" << std::endl;
            for (const auto &path : possible_paths)
            {
                std::cerr << "  - " << path << std::endl;
            }
            std::cerr << "[Error] You can set FASTLEC_ROOT environment "
                         "variable to specify project root."
                      << std::endl;
            std::cerr << "[Error] Example: export "
                         "FASTLEC_ROOT=/mnt/home/zhangxd/EC/fastLEC"
                      << std::endl;
            return;
        }

        std::string command = "";
        command += "mpirun -np " + std::to_string(available_threads);
        command += " " + mpi_para_path;

        command += " " + basefilename + ".cnf";
        command += " " + std::to_string(unit_clauses.size());
        command += " " + basefilename + ".log";

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
        static std::mutex system_mutex;
        {
            std::lock_guard<std::mutex> lock(system_mutex);
            system(command.c_str());
        }
#elif defined(_WIN32)
        // Windows does not support mpirun and system may behave differently.
        // Consider alternative implementation or show a warning.
        std::cerr << "[Warning] CNF runtime generation via mpirun is not "
                     "supported on Windows."
                  << std::endl;
#else
        std::cerr << "[Warning] CNF runtime generation is not supported on "
                     "this platform."
                  << std::endl;
#endif
    }

    // 4. read log file and get the runtimes
    double base_runtime = -1.0;
    std::vector<double> pos_runtimes(cnf->num_vars + 1, -1.0);
    std::vector<double> neg_runtimes(cnf->num_vars + 1, -1.0);
    std::ifstream log_file(basefilename + ".log");
    std::string line;
    while (std::getline(log_file, line))
    {
        // find { ... }
        size_t lbrace = line.find('{');
        size_t rbrace = line.find('}');
        if (lbrace == std::string::npos || rbrace == std::string::npos ||
            rbrace <= lbrace)
            continue;
        std::string content = line.substr(lbrace + 1, rbrace - lbrace - 1);
        // find (time = ... seconds)
        size_t time_pos = line.find("time =");
        size_t seconds_pos = line.find("seconds", time_pos);
        double runtime = -1.0;
        if (time_pos != std::string::npos && seconds_pos != std::string::npos)
        {
            std::string time_str =
                line.substr(time_pos + 6, seconds_pos - (time_pos + 6));
            try
            {
                runtime = std::stod(time_str);
            }
            catch (...)
            {
                runtime = -1.0;
            }
        }
        // if {} empty, then this is base
        if (content.find_first_not_of(" \t") == std::string::npos)
        {
            base_runtime = runtime;
        }
        else
        {
            // find the first number
            int lit = 0;
            size_t first_non_space = content.find_first_not_of(" \t");
            if (first_non_space != std::string::npos)
            {
                size_t next_space =
                    content.find_first_of(" \t", first_non_space);
                std::string lit_str = content.substr(
                    first_non_space, next_space - first_non_space);
                try
                {
                    lit = std::stoi(lit_str);
                    if (lit > 0 && lit < static_cast<int>(pos_runtimes.size()))
                    {
                        pos_runtimes[lit] = runtime;
                    }
                    else if (lit < 0 &&
                             -lit < static_cast<int>(neg_runtimes.size()))
                    {
                        neg_runtimes[-lit] = runtime;
                    }
                }
                catch (...)
                {
                    // ignore parse error
                }
            }
        }
    }
    log_file.close();

    // 5. generate dot file
    dot_data dot_data;
    dot_data.dot_filename = basefilename + ".dot";
    dot_data.base_runtime = base_runtime;
    dot_data.pos_runtimes = pos_runtimes;
    dot_data.neg_runtimes = neg_runtimes;
    dot_data.mask = mask;
    generate_dot(dot_data);

    // 6. vis dot file
    std::string pdf_filename = basefilename + ".pdf";
    int ret = system(
        ("dot -Tpdf " + dot_data.dot_filename + " -o " + pdf_filename).c_str());
    if (ret != 0)
    {
        std::cerr << "Failed to generate PDF file" << std::endl;
    }

    // 7. exit?
    exit(0);
}