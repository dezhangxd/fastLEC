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

# ./build/bin/fastLEC -i /home/zhangxd/Experiment/EC/ins/dp2_pairs/test_11_TOP5_0.aiger -m ES
# ./build/bin/fastLEC -i ../ins/all/test_11_TOP4.aiger -m ES -p use_ies 0
./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_right/bit13/mul13o13.aig -m ES -p es_bv_bits 12
# ./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_error/bit19/miter19o19.aig -m ES -p use_ies 0
# ./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_right/bit12/mul12o12.aig -m ES -p use_ies 0
# ./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_right/bit8/mul8o08.aig -m BDD
# ./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_error/bit8/miter8o08.aig -m BDD
# -c 1 -t 3600 -v 1 -m BDD -p max_iterations 1000000 -p seed 2

# dot -O -Tpdf ./logs_bdd/graph.dot