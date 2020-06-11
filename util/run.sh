cd ..

project_path=".PP"

# Simple
flow_file="scratch/${project_path}/data/simple_flow.txt"
topo_file="scratch/${project_path}/data/simple_topo.txt"
sim_time=10

# Complicated
# flow_file="${project_path}/data/complicated_flow.txt"
# topo_file="${project_path}/data/complicated_topo.txt"
# sim_time=10

./waf --run "scratch/PersonalProject --flow_file=${flow_file} --topo_file=${topo_file} --sim_time=${sim_time}" 2>scratch/${project_path}/out/log.out

cd scratch/${project_path}

awk '$4 == "thr" && $5 == "0(tcp)" {print $1 "\t" $6}' out/log.out > out/thr0.dat
awk '$4 == "thr" && $5 == "1(udp)" {print $1 "\t" $6}' out/log.out > out/thr1.dat
awk '$4 == "delay" && $5 == "1(udp)" {print $1 "\t" $6}' out/log.out > out/delay1.dat

gnuplot util/thr.plot
gnuplot util/delay.plot

code out/thr.png
code out/delay.png
