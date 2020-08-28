#! /bin/bash

cd astra-sim
# Download network backend
cd network
if [ -d "gem5_astra" ]
then
    echo "You have already downloaded gem5"
    cd ..
elif [ -d "ns3" ]
then
    echo "You have already downloaded ns3"
    cd ..
else
    read -p "Enter the back-end you want to download, options are gem5 or ns3: " backend

    if [ "$backend" == "gem5" ]
    then
        git clone https://github.com/georgia-tech-synergy-lab/gem5_astra.git
        echo "gem5 has been successfully downloaded"
    elif [ "$backend" == "ns3" ]
    then
        git clone
        echo "ns3 has been successfully downloaded"
    fi
    cd ..
fi

# Download compute backend
cd compute
if [ -d "SCALE-Sim" ]
then
    echo "You have already downloaded SCALE-Sim"
    cd ..
    exit 1
fi

git clone https://github.com/ARM-software/SCALE-Sim.git
echo "SCALE-Sim has been successfully downloaded"
cd ..

exit 1
