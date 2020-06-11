set terminal png
set output "out/delay.png"

set style line 1 lt 1 lw 2 lc rgb "blue"

set xlab 'Time (sec)'
set ylab 'Delay (ms)'

plot "out/delay1.dat" using 1:2 title "1(Udp)" with lines ls 1