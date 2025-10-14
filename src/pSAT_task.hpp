#pragma once

#include <queue>
#include <atomic>
#include <vector>
#include <condition_variable>

namespace fastLEC
{

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
    UNKNOWN,       // be cutted
};
// ----------------------------------------------------------------------------

class Task
{
public:
    Task();
    ~Task() = default;

    // states state;
    std::atomic<task_states> state;

    int id, cpu;                      // task id, cpu id
    std::vector<int> cube;            // task cubes
    std::vector<int> propagated_lits; // propagated lits
    unsigned new_cube_lit_cnt;        // new cube lit count
    bool is_propagated;

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
    bool is_sat() const { return state == SATISFIABLE; }
    bool is_unsat() const { return state == UNSATISFIABLE; }

    void set_state(task_states s) { state.store(s, std::memory_order_relaxed); }

    friend std::ostream &operator<<(std::ostream &os, const Task &t);
};
std::ostream &operator<<(std::ostream &os, const Task &t);

} // namespace fastLEC