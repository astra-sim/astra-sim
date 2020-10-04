#! /bin/bash -v
cpus=(128)
commScale=(1)
workload=(DLRM_HybridParallel) #Transformer_HybridParallel_Fwd_In_Bckwd
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
        filename="workload-$work-nodes-${cpus[$i]}-commScale-$inj"
        echo "--comm-scale=$inj , --host-count=${cpus[$i]} , --total-stats=$tot_stat_row , --stat-row=$current_row , --path=$mypath/ , --run-name=$filename"
		./build.sh -r \
        --system-configuration=sample_3dim_sys \
        --workload-configuration="$work" \
        --topology-name=intel \
        --bandwidth="250 25 12.5" \
        --link-latency="500 500 500" \
        --nic-latency=1 \
        --switch-latency=1 \
        --print-topology-log=true \
        --num-passes=2 \
        --num-queues-per-dim=1 \
        --comm-scale=$inj \
        --compute-scale=1 \
        --injection-scale=1 \
        --rendezvous-protocol=false \
        --host-count="${cpus[$i]}" \
        --total-stat-rows="$tot_stat_row" \
        --stat-row="$current_row" \
        --path="$mypath/" \
        --run-name=$filename>>"result/$mypath/$filename.txt"
        sleep 1
    done
  done
done
