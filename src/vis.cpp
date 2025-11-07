#include "vis.hpp"
#include "basic.hpp"
#include "parser.hpp"

#include <cstdio>
#include <fstream>
#include <cstdlib>
#include <limits.h>
#include <vector>
#include <string>
#include <iostream>
#include <mutex>

#if defined(_WIN32)
#include <io.h>
#define F_OK 0
#define access _access
#include <windows.h>
#else
#include <unistd.h>
#include <libgen.h>
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
            // 计算可用线程数：保留一半CPU核心给其他任务，减去当前使用的线程数
            available_threads = static_cast<int>(cpu_cores) / 2 - nthread;
            // 确保至少为1，但不超过200
            if (available_threads <= 0)
                available_threads = 1;
            if (available_threads > 200)
                available_threads = 200;
        }
#endif

        // 查找 mpi_para 可执行文件的路径
        std::string mpi_para_path = "";
        bool found_mpi_para = false;

        // 尝试多个可能的路径（按优先级排序）
        std::vector<std::string> possible_paths;

        // 1. 首先尝试从环境变量获取项目根目录
        const char *project_root = std::getenv("FASTLEC_ROOT");
        if (project_root != nullptr)
        {
            std::string env_path =
                std::string(project_root) + "/toolbox/mpi_para";
            possible_paths.push_back(env_path);
        }

        // 2. 尝试相对路径（从项目根目录，假设从build/bin运行）
        possible_paths.push_back("../../toolbox/mpi_para");
        possible_paths.push_back("../toolbox/mpi_para");

        // 3. 尝试从当前工作目录
        possible_paths.push_back("./toolbox/mpi_para");
        possible_paths.push_back("toolbox/mpi_para");

        // 4. 尝试绝对路径（如果toolbox在项目根目录）
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr)
        {
            std::string cwd_str(cwd);
            // 如果当前在build/bin目录，向上两级到项目根
            if (cwd_str.find("/build/bin") != std::string::npos)
            {
                size_t pos = cwd_str.find("/build/bin");
                std::string root = cwd_str.substr(0, pos);
                possible_paths.push_back(root + "/toolbox/mpi_para");
            }
            // 如果当前在build目录，向上一级到项目根
            else if (cwd_str.find("/build") != std::string::npos &&
                     cwd_str.find("/build/bin") == std::string::npos)
            {
                size_t pos = cwd_str.find("/build");
                std::string root = cwd_str.substr(0, pos);
                possible_paths.push_back(root + "/toolbox/mpi_para");
            }
            // 如果当前在项目根目录
            else
            {
                possible_paths.push_back(cwd_str + "/toolbox/mpi_para");
            }
        }
#endif

        // 检查每个可能的路径
        for (const auto &path : possible_paths)
        {
            if (access(path.c_str(), F_OK) == 0)
            {
                // 检查是否为可执行文件
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
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
}