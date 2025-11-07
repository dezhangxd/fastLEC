rm mpi_para

mpic++ -o mpi_para -O3 -std=c++17 mpi_para.cpp -I../deps/kissat/src -L../deps/kissat/build -lkissat

# README for mpi_para
# param1: input CNF file
# param2: cube size
# param2: output runtime log file

mpirun -np 8 ./mpi_para /Users/zhangxd/EC/fastLEC/vis/test_11_TOP6_-54_190.cnf 2 /Users/zhangxd/EC/fastLEC/vis/test_11_TOP6_-54_190_-370.log