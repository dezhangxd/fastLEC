#pragma once

#include <queue>
#include <vector>
#include <mutex>
#include <thread>
#include <future>
#include <memory>
#include <atomic>
#include <functional>
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

namespace fastLEC
{

class Task;
class ThreadPool;
class TaskManager;

// extern std::shared_mutex _prt_mtx;

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
    Task(std::shared_ptr<TaskManager> tm);
    ~Task() = default;

    std::shared_ptr<TaskManager> task_manager;
    // states state;
    std::atomic<task_states> state;
    std::atomic<bool> can_terminate;
    std::atomic<bool> paused;
    // std::atomic<int> is_adding;
    std::shared_ptr<kissat> solver;

    int id, cpu;

    double create_time, start_time, stop_time;
    double runtime() const;

    void terminate();

    int father;
    std::vector<std::vector<int>> sons; // vectors of Twins, Quadruplets, ...
    std::vector<int> cubes;

    unsigned new_cube_lits;
    unsigned level;
    unsigned split_ct() const { return sons.size(); }

    bool is_root() const { return id == ID_ROOT; }

    void set_state(task_states s) { state.store(s, std::memory_order_release); }

    friend std::ostream &operator<<(std::ostream &os, const Task &t);
};

class pSAT
{
    std::shared_ptr<fastLEC::XAG> xag;
    std::shared_ptr<fastLEC::CNF> cnf;

    unsigned n_threads;

public:
    pSAT(std::shared_ptr<fastLEC::XAG> xag, unsigned n_threads);
    ~pSAT() = default;

    fastLEC::ret_vals check_xag();
};

} // namespace fastLEC