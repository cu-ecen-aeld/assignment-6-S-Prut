#!/bin/sh
#--------------------------------------------------------
#Script to find a string in files of specified directory
#--------------------------------------------------------
# It has to accept the following runtime arguments:
#   1st arg - a full path to a file (including filename) on the filesystem [writefile]
#   2nd arg - a text string which will be written within these file [writestr]
#--------------------------------------------------------
#Author: Sergiy Prutyanyy
#Date:   04/30/2025

#write_dir_path="~/test"

echo "This script writes a string to a file in a specified directory"
echo "Author: Sergiy Prutyanyy (as part of exercises in the AESD lectures)"
echo "syntax: ./finder.sh arg1 arg2"
echo " arg1 - a full path to a file on the filesystem including a filename that shall be written, e.g. ~/path/to/directory/filename.txt"
echo " arg2 - a string to be written, e.g. my_search_string (the string doesnot include a space or any specific character)"
echo

echo -n "number of added arguments: $#"
if [ $# -lt 2 ]
then
    echo " - failed: invalid number of arguments"
    exit 1
else
  echo " - Ok"
fi

success_result="file created/changed successful !"
unsuccess_result="!!! failed: file creation and write were incomplet !!!"
filefullpath=$1
writestr=$2

#path_items=$(echo $filefullpath | grep -o ".[^/]*")
#for i in $path_items
#do
#  echo $i
#  if [ -d "$i" ]
#  then pwd; cd "$i"; echo "The path " $i "exists"
#  else echo "The path ${i} doesnot exists!"
#  fi
#done
filename=$(echo $filefullpath | grep -o "[A-Za-z0-9._%+-]*$")
#filename=$(echo $writepath | grep -o "[^/]*$")
filepath=$(echo $filefullpath | grep -o ".*/")
echo "To be created file: " $filename
echo "... in the directory: " $filepath

if [ ! -d "$filepath" ]
then
  echo "directory ${filepath} does not exists, creating directory"
  mkdir -p "$filepath"
fi
if [ $? -eq 0 ]; then 
  if [ -f "$filefullpath" ]
  then
    echo "file exists, appends the string"
    echo ${writestr} >> ${filefullpath}
  else
    echo "file does not exists, creating file"
    echo ${writestr} > ${filefullpath}
  fi
else
  echo "Failed to create a folder!"
fi

if [ $? -eq 0 ] ; then
  echo "${success_result}"
  exit 0
else
  echo "${unsuccess_result}"
  exit 1
fi
