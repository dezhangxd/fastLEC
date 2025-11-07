rm mpi_para

mpic++ -o mpi_para -O3 -std=c++17 mpi_para.cpp -I../deps/kissat/src -L../deps/kissat/build -lkissat

# README for mpi_para
# param1: input CNF file
# param2: cube size
# param2: output runtime log file

mpirun -np 200 ./mpi_para ../vis/test_11_TOP6.cnf 0 ../vis/test_11_TOP6.log