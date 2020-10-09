#! /bin/bash -v
nodes=(2 4 8 16 32 64 128)
commScale=(1)
workload=(medium_DLRM)
current_row=-1
tot_stat_row=`expr ${#nodes[@]} \* ${#commScale[@]} \* ${#workload[@]}`
mypath="result/$1-3dim"
rm -rf $mypath
mkdir $mypath
mypath="$1-3dim"
for i in "${!nodes[@]}"; do
  for inj in "${commScale[@]}"; do
    for work in "${workload[@]}"; do
        current_row=`expr $current_row + 1`
        totalNPUs=`expr ${nodes[$i]} \* 16`
        filename="workload-$work-npus-$totalNPUs-commScale-$inj"
        echo "--comm-scale=$inj , --host-count=${nodes[$i]} , --total-stats=$tot_stat_row , --stat-row=$current_row , --path=$mypath/ , --run-name=$filename"
		./build.sh -r \
        --system-configuration sample_3dim_sys \
        --workload-configuration "$work" \
        --topology-name Ring_AllToAll_Switch \
        --dims-count 3 \
        --nodes-per-dim 2 8 "${nodes[$i]}" \
        --link-bandwidth 250 25 12.5 \
        --link-latency 500 500 500 \
        --nic-latency 0 0 1 \
        --router-latency 0 0 1 \
        --hbm-bandwidth 370 370 13 \
        --hbm-latency 500 500 500 \
        --hbm-scale 1 \
        --num-passes 2 \
        --num-queues-per-dim 1 \
        --comm-scale $inj \
        --compute-scale 1 \
        --injection-scale 1 \
        --rendezvous-protocol false \
        --total-stat-rows "$tot_stat_row" \
        --stat-row "$current_row" \
        --path "$mypath/" \
        --run-name $filename>>"result/$mypath/$filename.txt"
        sleep 1
    done
  done
done
