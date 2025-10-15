#pragma once

#include <queue>
#include <vector>
#include <mutex>
#include <thread>
#include <memory>
#include <atomic>
#include <shared_mutex>

#include "XAG.hpp"
#include "CNF.hpp"
#include "basic.hpp"
#include "pSAT_task.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

#include "../deps/kissat/src/kissat.h"

#ifdef __cplusplus
}
#endif

extern std::shared_mutex _prt_mtx;

namespace fastLEC
{

// a thread pool based SAT solver
class PartitionSAT
{
private:
    std::shared_ptr<fastLEC::XAG> xag;
    std::shared_ptr<fastLEC::CNF> root_cnf;

private:
    unsigned n_threads;

    std::vector<std::thread> workers;
    std::vector<int> cpu_task_ids;
    std::vector<std::unique_ptr<std::mutex>> mutexes;
    std::vector<std::shared_ptr<kissat>> solvers;

    SQueue<int> q_wait_ids;                       // waiting to be added tasks
    SQueue<int> q_prop_ids;                       // newly solved tasks
    std::vector<std::shared_ptr<Task>> all_tasks; // vector to save all tasks

    std::atomic<bool> stop;
    std::atomic<int> running_cpu_cnt;

    std::thread timeout_thread;
    std::atomic<bool> timeout_thread_running;

    void worker_func(int cpu_id);

    std::atomic<bool> states_updated;
    std::atomic<bool> all_task_terminated;

    std::mutex split_mutex; //

    void timeout_monitor_func();

    // ------------------------------------------------------------
    std::vector<double> scores;
    // ------------------------------------------------------------

public:
    PartitionSAT(std::shared_ptr<fastLEC::XAG> xag, unsigned n_threads);
    ~PartitionSAT();

    std::shared_ptr<Task> get_task_by_cpu(int cpu_id);
    std::shared_ptr<Task> get_task_by_id(int task_id);

    bool propagate_task(std::shared_ptr<Task> task,
                        std::vector<bool> &forbidden_vars,
                        std::vector<int> &new_decision_lits,
                        std::vector<int> &new_propagated_lits);
    int submit_task(std::shared_ptr<Task> task);

    void terminate_task_by_id(int task_id);
    void terminate_task_by_cpu(int cpu_id);
    void terminate_all_tasks();

    std::shared_ptr<fastLEC::Task> pick_split_task();
    std::vector<int> pick_split_vars(std::shared_ptr<fastLEC::Task> father);
    bool compute_scores();
    int decide_split_vars();
    bool split_task_and_submit(std::shared_ptr<fastLEC::Task> father);

    std::shared_ptr<kissat> get_solver_by_cpu(int cpu_id);
    unsigned num_tasks() const { return all_tasks.size(); }

    void show_unsolved_tasks();
    void show_detailed_tasks();
    void show_pool();
    friend std::ostream &operator<<(std::ostream &os, const PartitionSAT &ps);

    ret_vals prop_task_status();

    ret_vals check();
};

std::ostream &operator<<(std::ostream &os, const PartitionSAT &ps);

} // namespace fastLEC