echo "Creating desired hierarchy"
./create_hierarchy.sh $1 $2
echo "Done!!"
rm -rf create_processes
gcc -g -Wall create_processes.c -o create_processes -lm
echo "Creating desired no of processes"
./create_processes $1 $2 $3
echo "Done!!"
echo "Moving created processes into systemd directory"
./move_processes.sh
echo "Done!!"
echo "Moving desired processes into tasks file of sub directories"
./move_processes_to_sub_dir.sh $1 $2 $3
echo "Done!!"
echo "Checking for unique processes in tasks file of all sub directories"
./check_for_unique_processes.sh $1 $2
echo "Done!!"
echo "Moving all the processes from all sub directories into systemd directory"
./remove_created_hierarchy.sh $1 $2
echo "Done!! and removed created hierarchy"
echo "Test Result: PASS"
