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
# ./build/bin/fastLEC -i ../ins/miter_error/bit12/miter12o12.aig -m pES -c 10 |gnomon
# ./build/bin/fastLEC -i ../ins/all/test_11_TOP5.aiger -m pES -c 1 |gnomon
./build/bin/fastLEC -i ../ins//miter_right/bit12/mul12o12.aig -m gpuES -c 1 |gnomon
# ./build/bin/fastLEC -i ../ins/miter_error/bit12/miter12o12.aig -m gpuES -c 1 |gnomon
# ./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_error/bit12/miter12o12.aig -m ES -p use_ies 1
# -c 1 -t 3600 -v 1 -m BDD -p max_iterations 1000000 -p seed 2

# dot -O -Tpdf ./logs_bdd/graph.dot