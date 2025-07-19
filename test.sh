clear;
clear;

# rm -rf build

rm build/bin/fastLEC

mkdir -p build
cd build
cmake ..
make -j
cd ..

# clear

# ./build/bin/fastLEC -i /home/zhangxd/Experiment/EC/ins/dp2_pairs/test_11_TOP5_0.aiger -m ES -p use_ies 1
# ./build/bin/fastLEC -i ../ins/miter_error/bit12/miter12o12.aig -m ES -p use_ies 1 -p ies_u64 1
./build/bin/fastLEC -i ../ins/all/test_11_TOP5.aiger -m ES -p use_ies 1 -p ies_u64 0
# ./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_right/bit14/mul14o14.aig -m ES -p use_ies 1
# ./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_error/bit19/miter19o19.aig -m ES -p use_ies 0
# ./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_right/bit12/mul12o12.aig -m ES -p use_ies 0
# ./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_right/bit8/mul8o08.aig -m BDD
# ./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_error/bit12/miter12o12.aig -m ES -p use_ies 1
# -c 1 -t 3600 -v 1 -m BDD -p max_iterations 1000000 -p seed 2

# dot -O -Tpdf ./logs_bdd/graph.dot