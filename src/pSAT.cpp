#include "pSAT.hpp"
#include "parser.hpp"

#include <iomanip>
#include <cassert>

// ----------------------------------------------------------------------------
// related to SQueue
// ----------------------------------------------------------------------------

// std::shared_mutex _prt_mtx;

template <typename T> bool fastLEC::SQueue<T>::empty() const
{
    std::lock_guard<std::mutex> lock(_mtx);
    return q.empty();
}

template <typename T> unsigned fastLEC::SQueue<T>::size() const
{
    std::lock_guard<std::mutex> lock(_mtx);
    return q.size();
}

template <typename T> void fastLEC::SQueue<T>::emplace(T &&item)
{
    {
        std::lock_guard<std::mutex> lock(_mtx);
        q.emplace(std::move(item));
    }
    _cv.notify_one();
}

template <typename T> T fastLEC::SQueue<T>::pop()
{
    std::unique_lock<std::mutex> lock(_mtx);
    _cv.wait(lock,
             [this]()
             {
                 return !q.empty();
             });

    T item = std::move(q.front());
    q.pop();
    return item;
}

template <typename T> bool fastLEC::SQueue<T>::try_pop(T &item)
{
    std::lock_guard<std::mutex> lock(_mtx);
    if (q.empty())
        return false;

    item = std::move(q.front());
    q.pop();
    return true;
}

// ----------------------------------------------------------------------------
// Task class implementation
// ----------------------------------------------------------------------------

fastLEC::Task::Task()
    : state(WAITING), id(ID_NONE), cpu(CPU_NONE), new_cube_lit_cnt(0),
      father(ID_NONE), level(0)
{
    create_time = fastLEC::ResMgr::get().get_runtime();
    start_time = 0.0;
    stop_time = 0.0;
}

double fastLEC::Task::runtime() const
{
    if (stop_time > 0.0)
        return stop_time - start_time;
    else
        return fastLEC::ResMgr::get().get_runtime() - start_time;
}

void fastLEC::Task::terminate_info_upd()
{
    task_states s = state.load(std::memory_order_relaxed);
    while (s == WAITING || s == ADDING || s == RUNNING)
    {
        if (state.compare_exchange_weak(s, UNKNOW, std::memory_order_relaxed))
            break;
    }
    if (stop_time == 0.0)
        stop_time = fastLEC::ResMgr::get().get_runtime();
}

std::ostream &fastLEC::operator<<(std::ostream &os, const Task &t)
{
    os << "T[";
    if (t.state == task_states::WAITING)
        os << "-";
    else if (t.state == task_states::ADDING)
        os << "A";
    else if (t.state == task_states::RUNNING)
        os << "R";
    else if (t.state == task_states::SATISFIABLE)
        os << "S";
    else if (t.state == task_states::UNSATISFIABLE)
        os << "U";
    else
        os << "?";

    os << ",";
    os << std::left << t.id;
    os << ",C";
    os << std::left << t.cpu;
    os << ",L";
    os << std::left << t.level;
    os << "]";

    os << "{";
    std::string father_id = "";
    if (t.is_root())
        father_id = "N/A ";
    else if (t.father == ID_ROOT)
        father_id = "ROOT";
    else
        father_id = "F" + std::to_string(t.father);

    os << "+";
    for (unsigned i = t.cubes.size() - t.new_cube_lit_cnt; i < t.cubes.size();
         i++)
    {
        os << t.cubes[i];
        if (i != t.cubes.size() - 1)
            os << ",";
    }
    os << "}";

    os << "," << std::right << std::setw(7) << std::fixed
       << std::setprecision(2) << t.runtime() << "s";
    return os;
}

// ----------------------------------------------------------------------------
// ThreadPool class implementation
// ----------------------------------------------------------------------------

fastLEC::ThreadPool::ThreadPool(unsigned n_threads,
                                std::shared_ptr<fastLEC::CNF> cnf)
    : n_threads(n_threads), root_cnf(cnf), stop(false)
{
    solvers.resize(n_threads, nullptr);

    std::shared_ptr<Task> root_task = std::make_shared<Task>();
    root_task->id = ID_ROOT;
    root_task->father = ID_NONE;
    root_task->level = 0;
    root_task->new_cube_lit_cnt = 0;
    submit_task(root_task);

    cpu_task_ids.resize(n_threads, ID_NONE);

    mutexes.reserve(n_threads);
    for (unsigned i = 0; i < n_threads; ++i)
        mutexes.emplace_back(std::make_unique<std::mutex>());

    for (unsigned i = 0; i < n_threads; ++i)
        workers.emplace_back(&ThreadPool::worker_func, this, i);
}

fastLEC::ThreadPool::~ThreadPool()
{
    stop.store(true);

    for (auto &worker : workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

void fastLEC::ThreadPool::worker_func(int cpu_id)
{
    int free_cnt = 0;
    while (!stop.load())
    {
        int id = ID_NONE;

        if (!q_wait_ids.try_pop(id))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            free_cnt++;
            if (free_cnt > 100)
            {
                free_cnt = 0;
                std::shared_ptr<Task> task = pick_split_task();
                if (task)
                    split_task_and_submit(task);
                
            }
            continue;
        }

        std::shared_ptr<Task> task = get_task_by_id(id);

        task->cpu = cpu_id;
        task->start_time = fastLEC::ResMgr::get().get_runtime();

        {
            std::lock_guard<std::mutex> lock(*mutexes[cpu_id]);
            cpu_task_ids[cpu_id] = task->id;
        }

        task->set_state(ADDING);

        int result = 0;

        assert(solvers[cpu_id] == nullptr);
        solvers[cpu_id] =
            std::shared_ptr<kissat>(kissat_init(), kissat_release);

        int start_pos = 0, end_pos = 0;
        for (int i = 0; i < root_cnf->num_clauses(); i++)
        {
            end_pos = root_cnf->cls_end_pos[i];
            for (int j = start_pos; j < end_pos; j++)
            {
                kissat_add(solvers[cpu_id].get(), root_cnf->lits[j]);
            }
            kissat_add(solvers[cpu_id].get(), 0);
            start_pos = end_pos;
        }

        for (int l : task->cubes)
        {
            kissat_add(solvers[cpu_id].get(), l);
            kissat_add(solvers[cpu_id].get(), 0);
        }

        if (!stop.load())
        {
            task->set_state(RUNNING);
            result = kissat_solve(solvers[cpu_id].get());
            printf("c [pSAT] Worker %d solving task %d result = %d\n",
                   cpu_id,
                   task->id,
                   result);
            fflush(stdout);

            if (result == 0)
            {
                q_prop_ids.emplace(std::move(id));
                task->set_state(UNKNOW);
            }
            else
            {
                if (result == 10)
                    task->set_state(SATISFIABLE);
                else if (result == 20)
                    task->set_state(UNSATISFIABLE);
                else
                    task->set_state(UNKNOW);

                if (task->id == ID_ROOT)
                {
                    terminate_all_tasks();
                    stop.store(true);
                    printf("c [pSAT] Worker %d stopping\n", cpu_id);
                    fflush(stdout);
                }
            }
        }
        task->terminate_info_upd();
        {
            std::lock_guard<std::mutex> lock(*mutexes[cpu_id]);
            cpu_task_ids[cpu_id] = ID_NONE;
        }
    }
}

std::shared_ptr<fastLEC::Task> fastLEC::ThreadPool::get_task_by_cpu(int cpu_id)
{
    if (cpu_id < 0 || cpu_id >= static_cast<int>(n_threads))
        return nullptr;

    std::lock_guard<std::mutex> lock(*mutexes[cpu_id]);
    return get_task_by_id(cpu_task_ids[cpu_id]);
}

std::shared_ptr<fastLEC::Task> fastLEC::ThreadPool::get_task_by_id(int task_id)
{
    if (task_id < 0 || task_id > (int)all_tasks.size())
        return nullptr;
    return all_tasks[task_id];
}

void fastLEC::ThreadPool::submit_task(std::shared_ptr<Task> task)
{
    if (task)
    {
        task->create_time = fastLEC::ResMgr::get().get_runtime();
        int task_id = task->id = (int)all_tasks.size();
        all_tasks.push_back(std::move(task));
        q_wait_ids.emplace(std::move(task_id));
    }
}

std::shared_ptr<kissat> fastLEC::ThreadPool::get_solver_by_cpu(int cpu_id)
{
    if (cpu_id < 0 || cpu_id >= static_cast<int>(n_threads))
    {
        return nullptr;
    }
    return solvers[cpu_id];
}

void fastLEC::ThreadPool::terminate_task_by_id(int task_id)
{

    std::shared_ptr<Task> task = get_task_by_id(task_id);
    if (task == nullptr || task->cpu == CPU_NONE)
        return;

    std::lock_guard<std::mutex> lock(*mutexes[task->cpu]);
    if (cpu_task_ids[task->cpu] == task_id)
    {
        std::shared_ptr<kissat> p = solvers[task->cpu];
        if (p)
            kissat_terminate(p.get());
        cpu_task_ids[task->cpu] = ID_NONE;
    }
}

void fastLEC::ThreadPool::terminate_task_by_cpu(int cpu_id)
{
    std::lock_guard<std::mutex> lock(*mutexes[cpu_id]);
    if (cpu_task_ids[cpu_id] != ID_NONE)
    {
        std::shared_ptr<kissat> p = solvers[cpu_id];
        if (p)
            kissat_terminate(p.get());
        cpu_task_ids[cpu_id] = ID_NONE;
    }
}

void fastLEC::ThreadPool::terminate_all_tasks()
{
    stop.store(true);

    for (unsigned i = 0; i < n_threads; i++)
        terminate_task_by_cpu(i);

    for (auto &t : all_tasks)
        t->terminate_info_upd();
}

fastLEC::ret_vals fastLEC::ThreadPool::check()
{
    for (auto &worker : workers)
    {
        if (worker.joinable())
            worker.join();
    }

    if (all_tasks[0]->state == SATISFIABLE)
        return ret_vals::ret_SAT;
    else if (all_tasks[0]->state == UNSATISFIABLE)
        return ret_vals::ret_UNS;
    else
        return ret_vals::ret_UNK;
}

std::shared_ptr<fastLEC::Task> fastLEC::ThreadPool::pick_split_task()
{
    return get_task_by_id(0);
}

bool fastLEC::ThreadPool::split_task_and_submit(
    std::shared_ptr<fastLEC::Task> task)
{
    int split_v =
        (fastLEC::ResMgr::get().random(0, root_cnf->num_vars) + 1) - 1;
    std::shared_ptr<fastLEC::Task> new_task1 =
        std::make_shared<fastLEC::Task>();
    std::shared_ptr<fastLEC::Task> new_task2 =
        std::make_shared<fastLEC::Task>();

    new_task1->father = task->id;
    task->sons.push_back({(int)num_tasks(), (int)num_tasks() + 1});
    new_task1->level = task->level + 1;
    new_task1->cubes = task->cubes;
    new_task1->cubes.push_back(split_v);
    new_task1->new_cube_lit_cnt = 1;
    submit_task(std::move(new_task1));

    new_task2->father = task->id;
    new_task2->level = task->level + 1;
    new_task2->cubes = task->cubes;
    new_task2->cubes.push_back(-split_v);
    new_task2->new_cube_lit_cnt = 1;
    submit_task(std::move(new_task2));
    return true;
}

// ----------------------------------------------------------------------

fastLEC::pSAT::pSAT(std::shared_ptr<fastLEC::XAG> xag, unsigned n_threads)
    : xag(xag), n_threads(n_threads)
{
    cnf = xag->construct_cnf_from_this_xag();
    tp = std::make_unique<ThreadPool>(n_threads, cnf);
}

fastLEC::ret_vals fastLEC::pSAT::check_xag()
{
    double start_time = fastLEC::ResMgr::get().get_runtime();

    ret_vals ret = tp->check();

    if (fastLEC::Param::get().verbose > 0)
    {
        printf("c [pSAT] result = %d [var = %d, clause = %d, lit = %d]"
               "[time = %.2f]\n",
               ret,
               this->cnf->num_vars,
               this->cnf->num_clauses(),
               this->cnf->num_lits(),
               fastLEC::ResMgr::get().get_runtime() - start_time);
        fflush(stdout);
    }

    return ret;
}
