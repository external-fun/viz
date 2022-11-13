#!/bin/bash

select CMD in "list files" "delete file" "rename file" "exit"; do
  case $CMD in
    "list files")
      ls .
      ;;
    "delete file")
      echo "Enter file name:"
      read -r file
      if [ ! "$file" ]; then
        continue
      fi
      if [ -f "$file" ]; then
        rm "$file"
      else
        echo "File with name '${file}' doesn't exist"
      fi
      ;;
    "rename file")
      echo "Enter old file name:"
      read -r old_file
      if [ ! -f "$old_file" ] || [ ! "$old_file" ]; then
        echo "Old file name doesn't exist; try again: "
        read -r old_file
        if [ ! -f "$old_file" ] || [ ! "$old_file" ]; then
          echo "File with name '${old_file}' doesn't exist"
          continue
        fi
      fi
      echo "Enter new file name:"
      read -r new_file
      if [ "$old_file" = "$new_file" ]; then
        echo "Trying to rename the same file"
        continue
      fi
      if [ -f "$new_file" ]; then
        echo "File with name ${new_file} already exists"
        continue
      fi
      mv "${old_file}" "${new_file}"
      ;;
    "exit")
      echo "Stopping the script"
      break
      ;;
    *)
      echo "Invalid option $REPLY"
      ;;
  esac
done