#!/bin/bash
#set -vx

WIDTH=$1;
DEPTH=$2;
cd /sys/fs/cgroup/systemd/

create_sub_dir () {
	local i;
	for (( i=1; i<=$WIDTH; i++ ))
	do
		/bin/mkdir $i;
	done
	let DEPTH=$DEPTH-1
}

create_childs() {
	local j;
	cd $1;
	create_sub_dir
	if [ $DEPTH -eq 0 ]; then
		cd ..
	else
		for (( j=1; j<=$WIDTH; j++ ))
		do
			create_childs $j
		done
		cd ..
	fi
	DEPTH=$DEPTH+1
}

/bin/mkdir sai
cd sai
create_sub_dir
for (( k=1; k<=$WIDTH; k++ ))
do
	create_childs $k
done
cd ..
