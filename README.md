# A Simple FAT32 Interpreting Shell Tool


Author:
-------
Ethan Letourneau


With Help From:
---------------
Stephen Hightower, Sarah Jiwani


Description:
------------
This is an implementation of a simple shell-like utility that is capable of interpreting a FAT32 file system image, written in C. 
The executable takes the image file path name (provided is file "fat32.7z" which compresses "fat32.img") as an argument and reads and writes to it according to different commands. 
The list of commands the program recognizes (and short descriptions of what each command does) are as followed:

exit - Closes the program and frees up any allocated resources.

info - Parses the boot sector, prints the field name and corresponding values for each entry.

ls <directory name> - Prints the name field for the directories within the contents of <directory name>.

cd <directory name> - Changes current working directory to <directory name>.

size <file name> - Prints size in bytes of <file name> in current working directory.

creat <file name> - Creates a file in the current working directsory with a size of 0 bytes and with a name of <file name>.

mkdir <directory name> - Creates a new directory in the current working directory with the name <directory name>.

open <file name> <mode> - Opens file named  <file name> in current working directory. File must be opened before being read/written to. <mode> must be one of the following: 
	- "r", read
	- "w", write
	- "rw", read and write
	- "wr", write and read

close <file name> - Closes file named <file name>.

read <file name> <offset> <size> - Reads out data from file in current working directory named <file name>, starting from <offset> bytes into the file and stopping after <size> bytes.

write <file name> <offset> <size> "<string>" - Writes <string> to file in current working directory named <file name>, starting from <offset> bytes into the file and stopping after <size> bytes.

rm <file name> - Deletes file named <file name> from CWD. 

rmdir <directory name> - Deletes directory named <directory name> from CWD, as well as any data it points to.


Assumptions Made in Operation:
------------------------------
- File and directory names cannot contain spaces.
- File and directory names cannot be paths, only names in the CWD.
- Long directory names are unsupported.
- "." and ".." are valid directory names.


Compilation Instructions: 
-------------------------
To compile, run “make” in the directory containing both the makefile and the filesys.c file. Then to run, use command “filesys.x”.

