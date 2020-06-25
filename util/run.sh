#!/bin/sh

# Put project files in "scratch/.PP"
# Put run.sh file in "scratch"

cd ..

project_path="scratch/.PP"

sim_time=100

# Simple
flow_file="${project_path}/data/simple_flow.txt"
topo_file="${project_path}/data/simple_topo.txt"

# Complicated
# flow_file="${project_path}/data/complicated_flow.txt"
# topo_file="${project_path}/data/complicated_topo.txt"

./waf --run "scratch/PersonalProject --flow_file=${flow_file} --topo_file=${topo_file} --sim_time=${sim_time}" 2>scratch/log.out
# ./waf --run "scratch/PersonalProject" --command-template="gdb --args %s --flow_file=${flow_file} --topo_file=${topo_file} --sim_time=${sim_time}" 2>scratch/log.out
