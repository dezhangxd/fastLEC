#include "mpi_para.hpp"

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    std::string cnf_file(argv[1]);
    int cube_size = atoi(argv[2]);
    std::string log_file(argv[3]);

    CNF cnf;
    cnf.build(cnf_file, cube_size);

    // 一次求解尝试。
    if (world_rank == 0)
    {
        // Manager process
        Manager manager(world_size, world_rank);
        int res = manager.checking(cnf);
        // std::cout << "Manager: Result = " << res << std::endl;
        manager.terminate();
    }
    else
    {
        // Worker process
        Worker worker(world_rank, world_size);
        worker.log_file = log_file;
        worker.start();
    }

    MPI_Finalize();
    return 0;
}