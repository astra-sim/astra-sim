#! /bin/bash -v
cpus=(4 16 36 64 81 100 144 256 400 900 1600)
commScale=(1)
workload=(medium_DLRM) #Transformer_HybridParallel_Fwd_In_Bckwd
current_row=-1
tot_stat_row=`expr ${#cpus[@]} \* ${#commScale[@]} \* ${#workload[@]}`
mypath="result/$1-torus"
rm -rf $mypath
mkdir $mypath
mypath="$1-torus"
for i in "${!cpus[@]}"; do
  for inj in "${commScale[@]}"; do
    for work in "${workload[@]}"; do
        current_row=`expr $current_row + 1`
        filename="workload-$work-npus-${cpus[$i]}-commScale-$inj"
        echo "--comm-scale=$inj , --host-count=${cpus[$i]} , --total-stats=$tot_stat_row , --stat-row=$current_row , --path=$mypath/ , --run-name=$filename"
		./build.sh -r \
        --system-configuration sample_torus_sys \
        --workload-configuration "$work" \
        --topology-name torus \
        --dims-count 1 \
        --nodes-per-dim "${cpus[$i]}" \
        --link-bandwidth 25 \
        --link-latency 500 \
        --nic-latency 0 \
        --router-latency 0 \
        --hbm-bandwidth 13 \
        --hbm-latency 500 \
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
