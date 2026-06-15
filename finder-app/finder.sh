#!/bin/sh
#--------------------------------------------------------
#Script to find a string in files of specified directory
#--------------------------------------------------------
# It has to accept the following runtime arguments:
#   1st arg - a path to a directory on the filesystem [filesdir]
#   2nd arg - a text string which will be searched within these files [searchstr]
#--------------------------------------------------------
#Author: Sergiy Prutyanyy
#Date:   04/25/2025

echo "This script searchs a string in all files in a specified directory"
echo "Author: Sergiy Prutyanyy (as part of exercises in the AESD lectures)"
echo "syntax: ./finder.sh arg1 arg2"
echo " arg1 - a path to a directory on the filesystem in which files should be searched, e.g. ~/path/to/directory"
echo " arg2 - a string to be searched, e.g. my_search_string (the string doesnot include a space or any specific character)"
echo

echo "number of added arguments: $#"
if [ $# -lt 2 ]
then
    echo " - failed: invalid number of arguments"
    exit 1
fi

success_result="search was successful !"
unsuccess_result="!!! failed: search was unsuccessfully !!!"
filesdir=$1
searchstr=$2
matching_files_result=$(grep -rl "${searchstr}" "${filesdir}"/)
#tst=$(grep -rl "${searchstr}" "${filesdir}" | grep -co ".*$"); num_files=0; for i in $tst; do num_files=$((num_files+i)); done
num_files=$(grep -rl "${searchstr}" "${filesdir}" | wc -l)
#num_files=$(find "${filesdir}" -type f | wc -l)

matching_lines_result=$(grep -r "${searchstr}" "${filesdir}"/)
#tst=$(grep -Rc "${searchstr}" "${filesdir}" | grep -o "[0-9]*$"); num_lines=0; for i in $tst; do num_lines=$((num_lines+i)); done
num_lines=$(grep -r "${searchstr}" "${filesdir}" | wc -l)

if [ $? -eq 0 ] ; then
    echo "${success_result}"
    #echo "${matching_files_result}"
    #echo "${matching_lines_result}"
    echo "The number of files are ${num_files} and the number of matching lines are ${num_lines}"
    exit 0
else
    echo "${unsuccess_result}"
    exit 1
fi
