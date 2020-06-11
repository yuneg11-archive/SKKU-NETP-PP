set terminal png
set output "out/thr.png"

set style line 1 lt 2 lw 2 lc rgb "red"
set style line 2 lt 1 lw 2 lc rgb "blue"

set xlab 'Time (sec)'
set ylab 'Throughput (Kbps)'

plot "out/thr0.dat" using 1:2 title "0(Tcp)" with lines ls 1,\
     "out/thr1.dat" using 1:2 title "1(Udp)" with lines ls 2