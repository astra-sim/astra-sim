#! /bin/bash
cd network
if [ -d "gem5_astra" ]
then
	echo "You have already downloaded gem5"
	cd ..
	exit 1
elif [ -d "ns3_astra" ]
then
	echo "You have already downloaded ns3"
	cd ..
	exit 1
fi

read -p "Enter the back-end you want to download, options are gem5 or ns3: " backend

if [ "$backend" == "gem5" ]
then
	git clone https://github.com/georgia-tech-synergy-lab/gem5_astra.git
	echo "gem5 has been successfully downloaded"
	cd .. 
	exit 1
elif [ "$backend" == "ns3" ]
then
	git clone https://github.com/astra-sim/ns3_astra.git
	cd ns3_astra
	./init.sh
	echo "ns3 has been successfully downloaded"
	cd ..
	cd ..
	exit 1
fi
