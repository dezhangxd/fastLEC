#pragma once

#include <queue>
#include <vector>
#include <mutex>
#include <thread>
#include <memory>
#include <atomic>
#include <shared_mutex>
#include <condition_variable>

#include "XAG.hpp"
#include "CNF.hpp"
#include "basic.hpp"

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

class Task;
class PartitionSAT;

// ----------------------------------------------------------------------------
// Safe Queue
// ----------------------------------------------------------------------------
template <typename T> class SQueue
{
    std::queue<T> q;
    mutable std::mutex _mtx;
    std::condition_variable _cv;

public:
    bool empty() const;
    unsigned size() const;
    void emplace(T &&item);
    bool try_pop(T &item);
    T pop();
};

// ----------------------------------------------------------------------------
// task status
#define ID_ROOT 0
#define ID_NONE -1
#define CPU_NONE -1

// running task_states
// WAITING: initial sate
// waiting -> adding    [by run a task, adding clauses]
// waiting -> unsat     [by BCP, unsat propagation]
// waiting -> sat       [by solving]
// waiting -> unknown   [by being cut]
// adding  -> running   [by call kissat_solve()]
// adding  -> unknown   [by being cut]
// running -> sat       [by solving]
// running -> unsat     [by solving, unsat propagation]
// running -> unknown   [by being cut]
// ========================================================
//            x(term)
// waiting -> adding -> running ------------+-> SAT/UNSAT
//  |            |           \             /
//  + -----------+------------+--> unknown
enum task_states
{
    WAITING,       // waiting in the pool
    ADDING,        // construct the cnf
    RUNNING,       // start SAT solving
    SATISFIABLE,   // get a SAT result
    UNSATISFIABLE, // get a UNSAT result
    UNKNOW,        // be cutted
};
// ----------------------------------------------------------------------------

class Task
{
public:
    Task();
    ~Task() = default;

    // states state;
    std::atomic<task_states> state;

    int id, cpu;               // task id, cpu id
    std::vector<int> cubes;    // task cubes
    unsigned new_cube_lit_cnt; // new cube lit count

    double create_time, // create time
        start_time,     // start solving time
        stop_time;      // stop solving time
    double runtime() const;

    int father;                         // father task id
    std::vector<std::vector<int>> sons; // vectors of Twins, Quadruplets, ...
    unsigned split_ct() const { return sons.size(); }

    unsigned level;

    void terminate_info_upd();

    bool is_root() const { return id == ID_ROOT; }
    bool is_solved() const
    {
        return state == SATISFIABLE || state == UNSATISFIABLE;
    }

    void set_state(task_states s) { state.store(s, std::memory_order_relaxed); }

    friend std::ostream &operator<<(std::ostream &os, const Task &t);
};

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
    std::atomic<bool> running_cpu_cnt;

    std::thread timeout_thread;
    std::atomic<bool> timeout_thread_running;

    void worker_func(int cpu_id);

    std::atomic<bool> states_updated;
    std::atomic<bool> all_task_terminated;
    void timeout_monitor_func();

public:
    PartitionSAT(std::shared_ptr<fastLEC::XAG> xag, unsigned n_threads);
    ~PartitionSAT();

    std::shared_ptr<Task> get_task_by_cpu(int cpu_id);
    std::shared_ptr<Task> get_task_by_id(int task_id);

    void submit_task(std::shared_ptr<Task> task);

    void terminate_task_by_id(int task_id);
    void terminate_task_by_cpu(int cpu_id);
    void terminate_all_tasks();

    std::shared_ptr<fastLEC::Task> pick_split_task();
    bool split_task_and_submit(std::shared_ptr<fastLEC::Task> xag);

    std::shared_ptr<kissat> get_solver_by_cpu(int cpu_id);
    unsigned num_tasks() const { return all_tasks.size(); }

    std::string show_unsolved_tasks();
    std::string show_pool();
    friend std::ostream &operator<<(std::ostream &os, const PartitionSAT &ps);

    ret_vals check();
};

class pSAT
{
    std::shared_ptr<fastLEC::XAG> xag;
    std::shared_ptr<fastLEC::CNF> cnf;

    unsigned n_threads;
    std::unique_ptr<PartitionSAT> tp;

public:
    pSAT(std::shared_ptr<fastLEC::XAG> xag, unsigned n_threads);
    ~pSAT() = default;

    fastLEC::ret_vals check();
};

} // namespace fastLEC