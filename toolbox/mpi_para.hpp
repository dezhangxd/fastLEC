#pragma once

#include <mpi.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <algorithm>

extern "C"
{
#include "../deps/kissat/src/kissat.h"
    struct kissat;
}

class CNF
{
public:
    int n_vars, n_clauses;
    std::vector<int> lits;
    std::vector<bool> mask;

    void build_mask(const std::string &cnf_file, int cube_size)
    {

        mask.resize(n_vars + 1, false);

        std::vector<int> cubes;
        size_t last_slash = cnf_file.find_last_of("/\\");
        std::string filename = (last_slash == std::string::npos)
            ? cnf_file
            : cnf_file.substr(last_slash + 1);
        size_t last_dot = filename.find_last_of('.');
        std::string base_name = (last_dot == std::string::npos)
            ? filename
            : filename.substr(0, last_dot);
        std::vector<std::string> parts;
        std::stringstream ss(base_name);
        std::string item;
        while (std::getline(ss, item, '_'))
            parts.push_back(item);
        for (int i = int(parts.size()) - 1; i >= 0; --i)
        {
            std::string &token = parts[i];
            if (!token.empty() &&
                (std::isdigit(token[0]) ||
                 (token[0] == '-' && token.size() > 1 &&
                  std::isdigit(token[1]))))
            {
                bool valid = true;
                for (size_t j = (token[0] == '-' ? 1 : 0); j < token.size();
                     ++j)
                {
                    if (!std::isdigit(token[j]))
                    {
                        valid = false;
                        break;
                    }
                }
                if (valid)
                {
                    cubes.push_back(std::stoi(token));
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
        std::reverse(cubes.begin(), cubes.end());

        if (cube_size > int(cubes.size()))
        {
            printf("c [build_mask] ERROR: cube_size(%d) > parsed "
                   "cubes.size(%zu)\n",
                   cube_size,
                   cubes.size());
            exit(1);
        }
        if (cube_size > 0)
        {
            cubes = std::vector<int>(cubes.end() - cube_size, cubes.end());
        }
        else
        {
            cubes.clear();
        }

        for (size_t i = 0; i < cubes.size(); ++i)
        {
            mask[abs(cubes[i])] = true;
        }
    }

    void build(const std::string &cnf_file, int cube_size)
    {
        std::ifstream fin(cnf_file);
        std::string line;
        while (getline(fin, line))
        {
            if (line.empty() || line[0] == 'c')
                continue;
            if (line[0] == 'p')
            {
                std::istringstream iss(line);
                std::string tmp_p, tmp_cnf;
                iss >> tmp_p >> tmp_cnf >> n_vars >> n_clauses;
                build_mask(cnf_file, cube_size);
            }
            else
            {
                std::istringstream iss(line);
                int lit;
                while (iss >> lit)
                {
                    lits.push_back(lit);
                }
            }
        }
        fin.close();
    }
};

// 全局退出函数
void global_exit(int exit_code)
{
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (world_rank == 0)
    {
        std::cout << "Manager: Initiating global exit..." << std::endl;
        // 广播退出信号给所有进程
        int exit_signal = -1;
        MPI_Bcast(&exit_signal, 1, MPI_INT, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    exit(exit_code);
}

enum Status
{
    BUSY,
    FREE,
};

class Manager
{
    std::vector<Status> worker_status; //
    unsigned num_running;              // BUSY的worker

    int next_task_index;                    // 下一个待求解的任务索引
    std::vector<std::vector<int>> tasks;    // 存储补充的cube
    std::vector<int> task_worker;           // 记录每个任务是由哪个worker求解的
    std::vector<MPI_Request> send_requests; // 存储发送请求
    std::vector<MPI_Status> send_statuses;  // 存储发送状态

    int world_size;
    int world_rank;
    CNF cnf;

public:
    Manager(int world_size, int world_rank)
    {
        this->world_size = world_size;
        this->world_rank = world_rank;
        this->num_running = 0;
        this->next_task_index = 0;
        worker_status.resize(world_size, Status::FREE);
        send_requests.resize(world_size); // 为每个worker分配一个发送请求
        send_statuses.resize(world_size); // 为每个worker分配一个发送状态
    }

    ~Manager()
    {
        // 清理资源
        tasks.clear();
        task_worker.clear();
        worker_status.clear();
        send_requests.clear();
        send_statuses.clear();
    }

    void Bcast_CNF()
    {
        // 广播 CNF 数据给所有 worker，并更新
        int cnf_size = cnf.lits.size();
        MPI_Bcast(&cnf_size, 1, MPI_INT, world_rank, MPI_COMM_WORLD);
        MPI_Bcast(
            cnf.lits.data(), cnf_size, MPI_INT, world_rank, MPI_COMM_WORLD);
    }

    int checking(CNF cnf)
    {
        this->cnf = cnf;
        Bcast_CNF();

        tasks.push_back({});       // 初始任务
        task_worker.push_back(-1); // 没有分配
        for (int i = 1; i <= cnf.n_vars; i++)
        {
            if (cnf.mask[i])
            {
                continue;
            }

            // pos cube
            tasks.push_back({i});
            task_worker.push_back(-1);

            // neg cube
            tasks.push_back({-i});
            task_worker.push_back(-1);
        }

        int completed_tasks = 0;
        bool found_counterexample = false;

        while (completed_tasks < tasks.size() && !found_counterexample)
        {
            // 检查是否有完成的任务
            MPI_Status status;
            int flag;
            MPI_Iprobe(
                MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);

            if (flag)
            {
                // 有任务完成，接收结果
                int result;
                MPI_Recv(&result,
                         1,
                         MPI_INT,
                         status.MPI_SOURCE,
                         status.MPI_TAG,
                         MPI_COMM_WORLD,
                         &status);

                int worker_rank = status.MPI_SOURCE;
                int task_id = status.MPI_TAG;

                worker_status[worker_rank] = Status::FREE;
                num_running--;
                completed_tasks++;

                if (result == 1)
                { // 如果找到反例
                    found_counterexample = true;
                    stop_all(worker_rank);
                    return 10;
                }
            }

            // 尝试分配新任务
            if (next_task_index < tasks.size() && num_running < world_size)
            {
                if (assign_task(next_task_index) != -1)
                {
                    next_task_index++;
                }
            }
        }

        return 20; // 所有任务都完成且没有找到反例
    }

    int assign_task(int task_id)
    {
        // 找一个Free的worker，分配task_id
        for (int i = 1; i < world_size; i++)
        {
            if (worker_status[i] == Status::FREE)
            {
                worker_status[i] = Status::BUSY;
                num_running++;
                task_worker[task_id] = i;

                // 使用非阻塞发送任务给worker
                MPI_Isend(&task_id,
                          1,
                          MPI_INT,
                          i,
                          0,
                          MPI_COMM_WORLD,
                          &send_requests[i]);

                // 发送cube数据
                int cube_size = tasks[task_id].size();
                MPI_Send(&cube_size, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                MPI_Send(tasks[task_id].data(),
                         cube_size,
                         MPI_INT,
                         i,
                         0,
                         MPI_COMM_WORLD);

                return i;
            }
        }
        return -1;
    }

    void stop(int worker_rank, int task_id)
    {
        int stop_signal = 1;
        MPI_Send(
            &stop_signal, 1, MPI_INT, worker_rank, task_id, MPI_COMM_WORLD);
    }

    void stop_all(int worker_rank)
    {
        int stop_signal = 2;
        MPI_Send(&stop_signal, 1, MPI_INT, worker_rank, 0, MPI_COMM_WORLD);
    }

    void terminate()
    {
        // 发送终止信号给所有worker
        for (int i = 1; i < world_size; i++)
        {
            int terminate_signal = -1;
            MPI_Send(&terminate_signal, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    }
};

class Worker
{
private:
    int world_rank, world_size;
    int current_task_id;   // 当前正在求解的任务ID,没有任务则为0
    std::vector<int> CNF;  // 存储CNF公式
    std::vector<int> cube; // 存储cube
    bool should_stop;      // 是否应该停止当前任务

public:
    std::string log_file;
    Worker(int world_rank, int world_size)
        : world_rank(world_rank), world_size(world_size), current_task_id(0),
          should_stop(false)
    {
    }

    void start()
    {
        // 接收manager的CNF
        int cnf_size;
        MPI_Bcast(&cnf_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
        CNF.resize(cnf_size);
        MPI_Bcast(CNF.data(), cnf_size, MPI_INT, 0, MPI_COMM_WORLD);

        while (true)
        {
            // 等待接收manager的任务
            MPI_Status status;
            int task_id;
            MPI_Recv(
                &task_id, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            // 检查是否是终止信号
            if (task_id == -1)
            {
                break;
            }

            // 接收假设
            int cube_size;
            MPI_Recv(&cube_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
            cube.resize(cube_size);
            MPI_Recv(
                cube.data(), cube_size, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);

            current_task_id = task_id;
            should_stop = false;

            // 求解任务
            int result = solve_task();

            // 发送结果给manager
            MPI_Send(&result, 1, MPI_INT, 0, task_id, MPI_COMM_WORLD);
        }
    }

    void stop_task_ID(int task_id)
    {
        if (current_task_id == task_id)
        {
            should_stop = true;
        }
    }

private:
    int solve_task()
    {
        auto start = std::chrono::high_resolution_clock::now();

        kissat *solver = kissat_init();

        // 首先添加CNF子句
        for (auto &lit : CNF)
        {
            if (should_stop)
            {
                kissat_release(solver);
                return 0;
            }
            kissat_add(solver, lit);
        }

        std::stringstream ss;

        // 然后添加cube作为单子句
        ss << "{ ";
        for (auto &lit : cube)
        {
            kissat_add(solver, lit);
            ss << lit << " ";
            kissat_add(solver, 0); // 添加子句结束标记
        }
        ss << "} ";

        int res = kissat_solve(solver);

        if (res == 10)
        {
            ss << "[ SAT ] ";
        }
        else if (res == 20)
        {
            ss << "[ UNSAT ] ";
        }

        kissat_release(solver);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double time_cost = duration.count() / 1000000.0;
        ss << std::fixed << std::setprecision(3) << "(time = " << time_cost
           << " seconds)";

        std::cout << ss.str() << std::endl;
        std::ofstream fout(log_file, std::ios::app);
        fout << ss.str() << std::endl;
        fout.close();
        return res;
    }
};
