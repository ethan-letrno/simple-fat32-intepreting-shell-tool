// Project 3
// COP4610 Operating Systems
// Ethan Letourneau, Stephen Hightower, Sarah Jiwani

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//STRUCTS AND DEFS

typedef struct {
	char BS_jmpBoot[3];
	char BS_OEMName[8]; 
	unsigned short BPB_BytsPerSec;
	unsigned char BPB_SecPerClus;
	unsigned short BPB_RsvdSecCnt;
	unsigned char BPB_NumFATs;
	unsigned short BPB_RootEntCnt;
	unsigned short BPB_TotSec16;
	unsigned char BPB_Media;
	unsigned short BPB_FATSz16;
	unsigned short BPB_SecPerTrk;
	unsigned short BPB_NumHeads;
	unsigned int BPB_HiddSec;
	unsigned int BPB_TotSec32;
	unsigned int BPB_FatSz32;
	unsigned short BPB_ExtFlags;
	unsigned short BPB_FSVer;
	unsigned int BPB_RootClus;
	unsigned short BPB_FSInfo;
	unsigned short BPB_BkBootSec;
	char BPB_Reserved[12];	
	unsigned char BS_DrvNum;
	unsigned char BS_Reserved1;
	unsigned char BS_BootSig;
	unsigned int BS_VolID;
	char BS_VolLab[11];
	char BS_FilSysType[8];
} __attribute__((packed)) Fat32BootSector;

typedef struct {
	char DIR_Name[11];
	unsigned char DIR_Attr; 
	unsigned char DIR_NTRes;
	unsigned char DIR_CrtTimeTenth;
	unsigned short DIR_CrtTime;
	unsigned short DIR_CrtDate;
	unsigned short DIR_LstAccDate;
	unsigned short DIR_FstClusHI;
	unsigned short DIR_WrtTime;
	unsigned short DIR_WrtDate;
	unsigned short DIR_FstClusLO;
	unsigned int DIR_FileSize;
} __attribute__((packed)) Fat32DirectoryEntry;


typedef struct
{
	char** tokens;
	int numTokens;
} instruction;

//linked list
struct fileTable
{
	char file_name[256];
	char file_mode[5];
	unsigned int cluster_num;
	struct fileTable* next;
}; 

//GLOBAL VARS
char* arguments[10];
int cwd;
Fat32BootSector bootsector;
Fat32DirectoryEntry root;
unsigned long FirstDataSector;
unsigned long RootDirSectors;
int FirstRootByte;
int FATFirstByte;
char * prompt_string;
struct fileTable* rootNode = NULL;

//COMMAND FUNCTION PROTOTYPES
void info(void);
void ls(FILE* img, int cwd_addr);
//NOT NEEDED... void cd(FILE* img, char* directory);
void size(FILE* img, char* filename, int cwd_addr);
void open(FILE* img, char* filename, const char* mode, int cwd_addr);
void closeFile(FILE* img, char *filename, int cwd_addr);
void creat(FILE* img, char* filename, int cwd_addr, int dir); //creat and mkdir use this, flag on "dir" to differentiate
void removedir(FILE* img, char* filename, int cwd_addr);
void removefile(FILE* img, char* filename, int cwd_addr);
void readFile(FILE* img, char* filename, unsigned int offset, unsigned int size, int cwd_addr);
void writefile(FILE* img, char* filename, int cwd_addr, int offset, int size, char* input_string);

//HELPER FUNCTION PROTOTYPES
void addToken(instruction* instr_ptr, char* tok);
void printTokens(instruction* instr_ptr);
void clearInstruction(instruction* instr_ptr);
int getBootInfo(FILE* img);
int isDir(Fat32DirectoryEntry entry);
int getFirstSectorNumber(int clusterNum);
Fat32DirectoryEntry getEntryInfo(FILE* img, int pos);
unsigned int getNextCluster(FILE* img, int cluster_num);
void add2FileTable(char *filename, const char* mode, unsigned int clusternum);
void rmFromFileTable(char *filename, unsigned int clusterNum);
unsigned int getDirectoryAddress(FILE* img, char* filename, int cwd_addr);
void realignDirectory(FILE* img, int cwd_addr);

//MAIN
int main(int argc, char* argv[]){

	char* token = NULL;
	char* temp = NULL;
	instruction instr;
	instr.tokens = NULL;
	instr.numTokens = 0;
	int not_found = 0;

	//take path to image file as arg, open it.
	if(argc != 2){
		printf("ERROR: %s\n","Please enter single path to FAT image as arg.");
		return 0;
	}

	FILE* img_file = fopen(argv[1], "rb+");
	
	if(img_file == NULL){
		printf("ERROR: %s\n","Failed to open FAT image file.");
		return 0;	
	}	

	arguments[0] = "\0";
	

	//Read information from Boot Sector.
	if(getBootInfo(img_file) != 1){
		printf("ERROR: %s\n","Failed to read from Boot Sector.");
		return 0;
	}

	//Set root to be CWD, prompt string to be '/'.
	cwd = FirstRootByte;
	prompt_string = (char*)malloc((strlen("/")+1) * sizeof(char));
	prompt_string[0] = '/';
	prompt_string[strlen(prompt_string)] = '\0';
	
	//If all info read correctly, begin shell-like interface.
	while (1) {
		printf("%s> ",prompt_string);

		do {			
			
			scanf( "%ms", &token);	
	    	temp = (char*)malloc((strlen(token)+1) * sizeof(char));	//allocate temp variable with same size as token
		
			memcpy(temp, token, strlen(token));
			temp[strlen(token)] = '\0';
			addToken(&instr, temp);			
		
			//free and reset variables
			free(token);
			free(temp);
			token = NULL;
			temp = NULL;
		
		} while ('\n' != getchar());    //until end of line is reached
		
		//If the number of tokens is over 5, it's a write command and we need to concatenate all the tokens after
		//The fifth into the fifth to be written to the file a single character array.
		if(instr.numTokens > 5){

			strcat(instr.tokens[4], " ");
			instr.tokens[4] = realloc(instr.tokens[4], (strlen(instr.tokens[4])+1) * sizeof(char));
			for(int x = 1; x <= instr.numTokens-5; x++){
				instr.tokens[4] = realloc(instr.tokens[4], (strlen(instr.tokens[4]) + strlen(instr.tokens[4+x])+2) * sizeof(char));
				strcat(instr.tokens[4], instr.tokens[4+x]);
				if(x != instr.numTokens-5){ strcat(instr.tokens[4], " "); }
			}
		}
		


	/*** EXIT ***/
	if(strcmp(instr.tokens[0], "exit") == 0 || strcmp(instr.tokens[0], "EXIT") == 0){
		printf("%s\n", "Exiting...");
		fclose(img_file);
		clearInstruction(&instr);
		return 1;
	}
	
	/*** INFO ***/
	else if(strcmp(instr.tokens[0], "info") == 0 || strcmp(instr.tokens[0], "INFO") == 0){
		info();
	}
	
	/*** LS ***/
	else if(strcmp(instr.tokens[0], "ls") == 0 || strcmp(instr.tokens[0], "LS") == 0){
		
		//If it's just "ls" or "ls .", print ls for cwd.
		if(instr.numTokens == 1 || (instr.tokens[1] != NULL && strcmp(instr.tokens[1], ".") == 0)){ ls(img_file, cwd); }
		else if(instr.tokens[1] != NULL && strcmp(instr.tokens[1], ".") != 0 ){ 
			
			//If the address given is not 0 or start of data section, run LS on address.
			if(getDirectoryAddress(img_file, instr.tokens[1], cwd) != 0 && 
				getDirectoryAddress(img_file, instr.tokens[1], cwd) != 0x100000){
				ls(img_file, getDirectoryAddress(img_file, instr.tokens[1], cwd));
			}
			//Else if start of data gotten, LS on root.
			else if(getDirectoryAddress(img_file, instr.tokens[1], cwd) == 0x100000){ 
				ls(img_file, FirstRootByte);
			}
			//Else if == 0, directory wasn't found.
			else{ printf("ERROR: Directory '%s' not found in current working directory.\n", instr.tokens[1]); }
		}		
		
	}

	/*** SIZE ***/
	else if(strcmp(instr.tokens[0], "size") == 0 || strcmp(instr.tokens[0], "SIZE") == 0){
		
		if(instr.tokens[1] != NULL){ size(img_file, instr.tokens[1], cwd); }
	}
	
	/*** OPEN ***/
	else if(strcmp(instr.tokens[0], "open") == 0 || strcmp(instr.tokens[0], "OPEN") == 0){
		
		if(instr.tokens[1] != NULL && instr.tokens[2] != NULL){ 
			open(img_file, instr.tokens[1], instr.tokens[2], cwd); }
		else
		{
			printf("ERROR: Command requires filename and mode.\n");
		}
		
	}

	/*** CLOSE ***/
	else if(strcmp(instr.tokens[0], "close") == 0 || strcmp(instr.tokens[0], "CLOSE") == 0){
		
		if(instr.tokens[1] != NULL){ closeFile(img_file, instr.tokens[1], cwd); }
		
	}

	/*** CD ***/
	else if(strcmp(instr.tokens[0], "cd") == 0 || strcmp(instr.tokens[0], "CD") == 0){
		
		//get directory address of directory trying to go to, set cwd to that:
		if(instr.tokens[1] != NULL && strcmp(instr.tokens[1], ".") != 0){
			
			//If the address given is not 0 or start of data section, run LS on address.
			if(getDirectoryAddress(img_file, instr.tokens[1], cwd) != 0 && 
				getDirectoryAddress(img_file, instr.tokens[1], cwd) != 0x100000){
				cwd = getDirectoryAddress(img_file, instr.tokens[1], cwd);
				not_found = 0;
			}
			//Else if start of data gotten, LS on root.
			else if(getDirectoryAddress(img_file, instr.tokens[1], cwd) == 0x100000){ 
				cwd = FirstRootByte;
				not_found = 0;
			}
			//Else if == 0, directory wasn't found.
			else{ 
				printf("ERROR: Directory '%s' not found in current working directory.\n", instr.tokens[1]); 
				not_found = 1;
			}
			
			//add the name if != "..", remove last one if == ".." IF GETDIRECTORY != 0 
			if(strcmp(instr.tokens[1], "..") == 0 && not_found == 0){

				for(int x = strlen(prompt_string); x >= 0; x--){
					if(prompt_string[x] == '/' && x != 0){
						prompt_string[x] = '\0';
						break;
					}
					else if(prompt_string[x] == '/' && x == 0){
						prompt_string[x+1] = '\0';
						break;
					}				
				}

			}
			else if(not_found == 0){
				if(prompt_string[1] == '\0'){				
					strcat(prompt_string, instr.tokens[1]);	
				}
				else{
					strcat(prompt_string, "/");
					strcat(prompt_string, instr.tokens[1]);
				}			
			}

		}
		else if(strcmp(instr.tokens[1], ".") == 0){ /* do nothing*/}
		else{ printf("ERROR: Command requires directory name.\n"); };
		
	}

	/*** CREAT ***/
	else if(strcmp(instr.tokens[0], "creat") == 0 || strcmp(instr.tokens[0], "CREAT") == 0){
		
		if(instr.numTokens == 2 && instr.tokens[1] != NULL){ creat(img_file, instr.tokens[1], cwd, 0); }
		else{ printf("ERROR: Creat command needs to be of form \"creat FILENAME\""); }
	}

	/*** MKDIR ***/
	else if(strcmp(instr.tokens[0], "mkdir") == 0 || strcmp(instr.tokens[0], "MKDIR") == 0){
		
		if(instr.numTokens == 2 && instr.tokens[1] != NULL){ creat(img_file, instr.tokens[1], cwd, 1); }
		else{ printf("ERROR: Mkdir command needs to be of form \"mdkir DIRNAME\""); }
	}

	/*** RMDIR ***/
	else if(strcmp(instr.tokens[0], "rmdir") == 0 || strcmp(instr.tokens[0], "RMDIR") == 0){
		
		if(instr.numTokens == 2 && instr.tokens[1] != NULL){ 
			removedir(img_file, instr.tokens[1], cwd); 
			realignDirectory(img_file, cwd);
		}
		else{ printf("ERROR: Rmdir command needs to be of form \"rmdir DIRNAME\""); }
	}

	/*** RM ***/
	else if(strcmp(instr.tokens[0], "rm") == 0 || strcmp(instr.tokens[0], "RM") == 0){
		
		if(instr.numTokens == 2 && instr.tokens[1] != NULL){ 
			removefile(img_file, instr.tokens[1], cwd); 
			realignDirectory(img_file, cwd); 
		}
		else{ printf("ERROR: Rm command needs to be of form \"rm FILENAME\"\n"); }
	}

	/*** READ ***/
	else if(strcmp(instr.tokens[0], "read") == 0 || strcmp(instr.tokens[0], "READ") == 0){
		
		if(instr.tokens[1] != NULL && instr.tokens[2] != NULL && instr.tokens[3] != NULL){ 
			unsigned int offset;
			unsigned int size;
			offset = atoi(instr.tokens[2]);
			size = atoi(instr.tokens[3]);
			readFile(img_file, instr.tokens[1], offset, size, cwd);
			printf("\n");		
		 }
		else
		{
			printf("ERROR: Command requires filename, offset, and size.\n");
		}
		
	}	

	/*** WRITE ***/
	else if(strcmp(instr.tokens[0], "write") == 0 || strcmp(instr.tokens[0], "WRITE") == 0){
		
		if(instr.numTokens > 4 && instr.tokens[1] != NULL && instr.tokens[2] != NULL && instr.tokens[3] != NULL && instr.tokens[4] != NULL){ 
			unsigned int offset;
			unsigned int size;
			offset = atoi(instr.tokens[2]);
			size = atoi(instr.tokens[3]);
			writefile(img_file, instr.tokens[1], cwd, offset, size, instr.tokens[4]);

		 }
		else
		{
			printf("ERROR: Command requires filename, offset, size, and input string.\n");
		}
		
	}	

	/*** UNRECOGNIZED COMMAND ***/
	else{
		printf("ERROR: Command '%s' unrecognized.\n", instr.tokens[0]);
	}

	//printTokens(&instr);
	clearInstruction(&instr);
		
	}

	return 1;
	
}


//HELPER FUNCTION DEFS

int getBootInfo(FILE* img){

	//Parse and store boot sector...
	fseek(img, 512*0, SEEK_SET);
	fread(&bootsector, sizeof(Fat32BootSector), 1, img);
	
	//Calculate important numbers from that data...
	RootDirSectors = ((bootsector.BPB_RootEntCnt * 32) + (bootsector.BPB_BytsPerSec - 1)) / bootsector.BPB_BytsPerSec;
	FirstDataSector = bootsector.BPB_RsvdSecCnt + (bootsector.BPB_NumFATs * bootsector.BPB_FatSz32) + RootDirSectors;
	FirstRootByte = FirstDataSector * bootsector.BPB_BytsPerSec;
	FATFirstByte = bootsector.BPB_RsvdSecCnt * bootsector.BPB_BytsPerSec;
	return 1;
}


//reallocates instruction array to hold another token
//allocates for new token within instruction array
void addToken(instruction* instr_ptr, char* tok)
{
	//extend token array to accomodate an additional token
	if (instr_ptr->numTokens == 0)
		instr_ptr->tokens = (char**)malloc(sizeof(char*));
	else
		instr_ptr->tokens = (char**)realloc(instr_ptr->tokens, (instr_ptr->numTokens+1) * sizeof(char*));

	//allocate char array for new token in new slot
	instr_ptr->tokens[instr_ptr->numTokens] = (char *)malloc((strlen(tok)+1) * sizeof(char));
	strcpy(instr_ptr->tokens[instr_ptr->numTokens], tok);

	instr_ptr->numTokens++;
	
}

void printTokens(instruction* instr_ptr)
{
	int i;
	printf("Tokens:\n");
	for (i = 0; i < instr_ptr->numTokens; i++)
	{
		if ((instr_ptr->tokens)[i] != NULL)
			printf("#%s#\n", (instr_ptr->tokens)[i]);
	}
}

void clearInstruction(instruction* instr_ptr)
{
	int i;
	for (i = 0; i < instr_ptr->numTokens; i++)
		free(instr_ptr->tokens[i]);
	free(instr_ptr->tokens);

	instr_ptr->tokens = NULL;
	instr_ptr->numTokens = 0;
}

int isDir(Fat32DirectoryEntry entry){
	if(entry.DIR_Attr == 0x10){ return 1; }
	else return 0;
}

int getFirstSectorNumber(int clusterNum){
	return ((clusterNum - 2) * bootsector.BPB_SecPerClus) + FirstDataSector;
}

Fat32DirectoryEntry getEntryInfo(FILE* img, int pos){
	
	Fat32DirectoryEntry entry;	
	
	fseek(img, pos, SEEK_SET);
	fread(&entry, sizeof(Fat32DirectoryEntry), 1, img);

	return entry;

	

}

unsigned int getNextCluster(FILE* img, int cluster_num){
	
	unsigned int next_cluster;

	fseek(img, FATFirstByte + cluster_num*4, SEEK_SET);
	fread(&next_cluster, sizeof(unsigned int), 1, img);
	return next_cluster;
	

}

unsigned int getDirectoryAddress(FILE* img, char* filename, int cwd_addr){

	//Calculate the cluster number from the address given for lookup into FAT
	unsigned int current_byte = cwd_addr;
	unsigned int cluster_num = (current_byte - 0x100000) / 0x200;
	
	int counter = 0;

	//read in first directoryEntry @current_byte
	Fat32DirectoryEntry entry = getEntryInfo(img, current_byte);

	//While the attribute is not zero indicating the end of the directory, keep reading in.
	while(entry.DIR_Attr != 0){
		
		entry.DIR_Name[10] = '\0';
		
		for(int x = 0; x < strlen(filename); x++){ filename[x] = tolower(filename[x]); }
		for(int x = 0; x < strlen(entry.DIR_Name); x++){ entry.DIR_Name[x] = tolower( entry.DIR_Name[x]); }
		
		//If name given matches a directory entry name AND that entry is a directory, return the address of the dir.
		if(entry.DIR_Attr == 0x10 && strncmp(entry.DIR_Name, filename, strlen(filename)) == 0){
			
			//get upper and lower bytes of first cluster, put them together and switch endianness.
			cluster_num = entry.DIR_FstClusHI << 16 | entry.DIR_FstClusLO;
			current_byte = (cluster_num * 0x200) + 0x100000;
			return current_byte;
		}

		counter += 1;	
		current_byte += 32; //each address is +32 of previous
		
		//If we reach 16 loops (end of sector, 512 bytes), need to go into FAT to look up next cluster, go to that addr
		if(counter % 16 == 0){

			cluster_num = getNextCluster(img, cluster_num);

			//If getNextCluster returns -1, it's end of cluster. Time to stop
			if(cluster_num == 0x0FFFFFFF){
				return 0;
			}			

			current_byte = (cluster_num * 0x200) + 0x100000;
		}

		//read in next entry
		entry = getEntryInfo(img, current_byte);		
				
	}

	return 0;

}

void add2FileTable(char *filename, const char* mode, unsigned int clusternum){

	//printf("Adding to the table...\n");

	//allocate memory 
	//void *calloc(n, element-size)
	struct fileTable* entry = calloc(1, sizeof(struct fileTable));

	//add file info to entry
	entry->cluster_num = clusternum;
	strcpy(entry->file_name, filename);
	strcpy(entry->file_mode, mode);
	entry->next = NULL;

	//if file is already opened, throw error
	for (struct fileTable* itr = rootNode; itr != NULL; itr = itr->next)
	{
		if (strcmp(itr->file_name, filename) == 0){
			printf("ERROR: File is already open.\n");
			return;
		}
	}

	//if root is empty, set entry to root
	if (rootNode == NULL){rootNode = entry; printf("File opened.\n");}
	else{
		//iterate to the end and add entry
		struct fileTable* itr = rootNode;
		while (itr->next != NULL){itr = itr->next;}
		itr->next = entry;
		printf("File opened.\n");
	}
	
	//Print File Table 
/*	printf("\nCURRENT FILE TABLE: \n");
	if (rootNode != NULL) {
		struct fileTable* itr = rootNode;
		printf("File name: %s\tFile mode: %s\tCluster number: %x\n", rootNode->file_name, rootNode->file_mode, rootNode->cluster_num);
		while (itr->next != NULL) {
			printf("File name: %s\tFile mode: %s\tCluster number: %x\n", itr->next->file_name, itr->next->file_mode, itr->next->cluster_num);
			itr = itr->next;
		}
	}
	printf("\n");*/
}


void rmFromFileTable(char *filename, unsigned int clusterNum){

	//printf("(In-function) Remove cluster #%x\n", clusterNum);

	struct fileTable* itr1 = rootNode;
	struct fileTable* temp = itr1, *itr2;

	if (temp != NULL && temp->cluster_num == clusterNum && (strcmp(temp->file_name, filename) == 0))
	{
		rootNode = temp->next;
		free(temp);
		printf("File closed.\n");
	}
	while (temp!=NULL && temp->cluster_num != clusterNum)
	{
		itr2 = temp;
		temp = temp->next;
	}
	if (temp == NULL){
		printf("ERROR: File has not been opened.\n");
		return;
	}

	itr2->next = temp->next;
	free(temp);
	printf("File closed.\n");

/*	//Print File Table 

	if (rootNode != NULL) {
		printf("\nCURRENT FILE TABLE: \n");
		struct fileTable* itr = rootNode;
		printf("File name: %s\tFile mode: %s\tCluster number: %x\n", rootNode->file_name, rootNode->file_mode, rootNode->cluster_num);
		while (itr->next != NULL) {
			printf("File name: %s\tFile mode: %s\tCluster number: %x\n", itr->next->file_name, itr->next->file_mode, itr->next->cluster_num);
			itr = itr->next;
		}
	}
	else 
		printf("\nFILE TABLE IS EMPTY.\n");
	printf("\n");*/

}


void realignDirectory(FILE* img, int cwd_addr){

	//Calculate the cluster number from the address given for lookup into FAT
	unsigned int current_byte = cwd_addr;
	unsigned int cluster_num = (current_byte - 0x100000) / 0x200;
	unsigned int cluster_num_to_shift = 0;
	unsigned int byte_to_shift = 0;
	unsigned char zero_byte = 0;
	
	int counter = 0;

	//read in first directoryEntry @current_byte
	Fat32DirectoryEntry entry = getEntryInfo(img, current_byte);
	Fat32DirectoryEntry entry_to_shift;

	//Read entry by entry in cwd
	while(1){
		//printf("Current byte = 0x%x\n", current_byte);
		//if find an empty entry, look-ahead to read next one
		if(entry.DIR_Attr == 0x0){
		
			//Case 1: Counter % 15 != 0, meaning going to the next 32 bytes would NOT be out of the current cluster.
			if(counter % 15 != 0){
				
				entry_to_shift = getEntryInfo(img, current_byte+32);
			
				//if looked-ahead entry also empty, it's end of file so we return
				if(entry_to_shift.DIR_Attr == 0x0){
					return;
				}
				//else if not empty, write to current empty entry, delete the look-ahead entry.
				else{
					fseek(img, current_byte, SEEK_SET);
					fwrite(&entry_to_shift, sizeof(Fat32DirectoryEntry), 1, img);
	
					for(int z = 0; z < 32; z++){				
						fseek(img, current_byte+32+z, SEEK_SET);
						fwrite(&zero_byte, sizeof(unsigned char), 1, img);
					}
				}	
			
			}
			//Case 2: If next 32 bytes would be out of current cluster, look to next cluster.
			else{
				cluster_num_to_shift = getNextCluster(img, cluster_num);

				//If getNextCluster returns -1, it's end of cluster. 
				//So, Means there's just one empty slot in single cluster.
				if(cluster_num_to_shift == 0x0FFFFFFF || cluster_num_to_shift == 0x0FFFFFF8){
					return;
				}			

				byte_to_shift = (cluster_num_to_shift * 0x200) + 0x100000;

				entry_to_shift = getEntryInfo(img, byte_to_shift);
			
				//if looked-ahead entry also empty, it's end of file so we return
				if(entry_to_shift.DIR_Attr == 0x0){
					return;
				}
				//else if not empty, write to current empty entry, delete the look-ahead entry.
				else{
					fseek(img, current_byte, SEEK_SET);
					fwrite(&entry_to_shift, sizeof(Fat32DirectoryEntry), 1, img);
	
					for(int z = 0; z < 32; z++){				
						fseek(img, byte_to_shift+z, SEEK_SET);
						fwrite(&zero_byte, sizeof(unsigned char), 1, img);
					}
				}

			}
		}

		counter += 1;	
		current_byte += 32; //each address is +32 of previous
		
		//If we reach 16 loops (end of sector, 512 bytes), need to go into FAT to look up next cluster, go to that addr
		if(counter % 16 == 0){
			cluster_num = getNextCluster(img, cluster_num);

			//If getNextCluster returns -1, it's end of cluster. Time to stop
			if(cluster_num == 0x0FFFFFFF){
				return;
			}			

			current_byte = (cluster_num * 0x200) + 0x100000;
		}

		//read in next entry
		entry = getEntryInfo(img, current_byte);		
				
	}

	return;

}


//COMMAND FUNCTION DEFS

void info(void){

	printf("%s\n", "--- Filesystem Info ---");
	printf("Bytes Per Sector: %d\n", bootsector.BPB_BytsPerSec);
	printf("Sectors Per Cluster: %d\n", bootsector.BPB_SecPerClus);
	printf("Num Reserved Sects: %d\n", bootsector.BPB_RsvdSecCnt);
	printf("Num FATs: %d\n", bootsector.BPB_NumFATs);
	printf("Root Cluster: %d\n", bootsector.BPB_RootClus);
	printf("SecPerTrk: %d\n", bootsector.BPB_SecPerTrk);	
	printf("NumHeads: %d\n", bootsector.BPB_NumHeads);
	printf("HiddenSec: %d\n", bootsector.BPB_HiddSec);
	printf("Total Sec 32: %d\n", bootsector.BPB_TotSec32);
	printf("Bytes Per FAT: %lx\n", bootsector.BPB_FatSz32);
	printf("Number of Sectors Occupied by Root: %lu\n", RootDirSectors);
	printf("First Cluster of Root: %lu\n\n", getFirstSectorNumber(bootsector.BPB_RootClus));

}


void ls(FILE* img, int cwd_addr){

	//Calculate the cluster number from the address given for lookup into FAT
	unsigned int current_byte = cwd_addr;
	unsigned int cluster_num = (current_byte - 0x100000) / 0x200;
	
	int counter = 0;

	//read in first directoryEntry @current_byte
	Fat32DirectoryEntry entry = getEntryInfo(img, current_byte);

	//While the attribute is not zero indicating the end of the directory, keep reading in.
	while(entry.DIR_Attr != 0){
	
		//print if valid entry
		if(entry.DIR_Attr == 1 || entry.DIR_Attr == 2 || entry.DIR_Attr == 4 || 
			entry.DIR_Attr == 8 || entry.DIR_Attr == 0x10 || entry.DIR_Attr == 0x20){
		
			entry.DIR_Name[10] = '\0';

			//Change color blue/green depending on if it's directory/file
			if( entry.DIR_Attr == 0x10 ){ printf("\033[0;36m%s\n\033[0m", entry.DIR_Name); }
			else if( entry.DIR_Attr == 0x20 ){ printf("\033[0;32m%s\n\033[0m", entry.DIR_Name); }
			
		}

		counter += 1;	
		current_byte += 32; //each address is +32 of previous
		
		//If we reach 16 loops (end of sector, 512 bytes), need to go into FAT to look up next cluster, go to that addr
		if(counter % 16 == 0){
			cluster_num = getNextCluster(img, cluster_num);

			//If getNextCluster returns -1, it's end of cluster. Time to stop
			if(cluster_num == 0x0FFFFFFF){
				return;
			}			

			current_byte = (cluster_num * 0x200) + 0x100000;
		}

		//read in next entry
		entry = getEntryInfo(img, current_byte);		
				
	}

	return;

}

void size(FILE* img, char* filename, int cwd_addr){

	//Calculate the cluster number from the address given for lookup into FAT
	unsigned int current_byte = cwd_addr;
	unsigned int cluster_num = (current_byte - 0x100000) / 0x200;
	
	int counter = 0;

	//read in first directoryEntry @current_byte
	Fat32DirectoryEntry entry = getEntryInfo(img, current_byte);

	//While the attribute is not zero indicating the end of the directory, keep reading in.
	while(entry.DIR_Attr != 0){
	
		entry.DIR_Name[10] = '\0';

		//Set each string to be lowercase
		for(int x = 0; x < strlen(filename); x++){ filename[x] = tolower(filename[x]); }
		for(int x = 0; x < strlen(entry.DIR_Name); x++){ entry.DIR_Name[x] = tolower( entry.DIR_Name[x]); }

		//print size if filename == name give entry
		if(strncmp(entry.DIR_Name, filename, strlen(filename)) == 0){
	 		printf("Size: %d\n", entry.DIR_FileSize); 
			return;
		}

		counter += 1;	
		current_byte += 32; //each address is +32 of previous
		
		//If we reach 16 loops (end of sector, 512 bytes), need to go into FAT to look up next cluster, go to that addr
		if(counter % 16 == 0){
			cluster_num = getNextCluster(img, cluster_num);
			
			//If getNextCluster returns -1, it's end of cluster. Time to stop
			if(cluster_num == 0x0FFFFFFF){
				printf("%s\n", "ERROR: Filename not found.");
				return;
			}			

			current_byte = (cluster_num * 0x200) + 0x100000;
		}

		//read in next entry
		entry = getEntryInfo(img, current_byte);		
				
	}

	printf("%s\n", "ERROR: Filename not found.");
	return;


}

void open(FILE* img, char* filename, const char* mode, int cwd_addr){
	
	//check if mode is entered correctly 
	if (strcmp(mode, "r") != 0 && strcmp(mode, "w") != 0 && strcmp(mode, "rw") != 0 && strcmp(mode, "wr") != 0)
    	printf("ERROR: Invalid mode.\n");

	//check filesystem for the file 
	else{
		unsigned int current_byte = cwd_addr;
		//get cluster number
		unsigned int cluster_num = (current_byte - 0x100000) / 0x200;	
		int counter = 0;

		//read in first directoryEntry @current_byte
		Fat32DirectoryEntry entry = getEntryInfo(img, current_byte);
		int clusternum = entry.DIR_FstClusHI << 16 | entry.DIR_FstClusLO;

		//While the attribute is not zero (indicating the end of the directory) keep reading in
		while(entry.DIR_Attr != 0){
	
			entry.DIR_Name[10] = '\0';

			//Set each string to be lowercase
			for(int x = 0; x < strlen(filename); x++){ filename[x] = tolower(filename[x]); }
			for(int x = 0; x < strlen(entry.DIR_Name); x++){ entry.DIR_Name[x] = tolower( entry.DIR_Name[x]); }

			//if filename is found & mode is valid, add to table of opened files  
			if(strncmp(entry.DIR_Name, filename, strlen(filename)) == 0){

				if (entry.DIR_Attr == 0x10)
					{
						printf("ERROR: Cannot open directory.\n");
					}
				else if (entry.DIR_Attr == 0x01 && ((strcmp(mode, "w") == 0) || (strcmp(mode, "rw") == 0) || (strcmp(mode, "wr") == 0)))
					printf("ERROR: File is read-only.\n");
				else	
					add2FileTable(filename, mode, entry.DIR_FstClusHI << 16 | entry.DIR_FstClusLO);
					
				return;
			}

			counter += 1;	
			current_byte += 32; //each address is +32 of previous
		
			//If we reach 16 loops (end of sector, 512 bytes), need to go into FAT to look up next cluster, go to that addr
			if(counter % 15 == 0){
				cluster_num = getNextCluster(img, cluster_num);
			
				//If getNextCluster returns -1, it's end of cluster. Time to stop
				if(cluster_num == 0x0FFFFFFF){
					printf("%s\n", "ERROR: Filename not found.");
					return;
				}			

				current_byte = (cluster_num * 0x200) + 0x100000;
			}
		//read in next entry
		entry = getEntryInfo(img, current_byte);					
	}

	printf("%s\n", "ERROR: Filename not found.");
	return;
	}
	
}


void closeFile(FILE* img, char *filename, int cwd_addr){

		unsigned int current_byte = cwd_addr;
		//get cluster number
		unsigned int cluster_num = (current_byte - 0x100000) / 0x200;	
		int counter = 0;

		//read in first directoryEntry @current_byte
		Fat32DirectoryEntry entry = getEntryInfo(img, current_byte);
		int clusternum = entry.DIR_FstClusHI << 16 | entry.DIR_FstClusLO;

		//While the attribute is not zero (indicating the end of the directory) keep reading in
		while(entry.DIR_Attr != 0){
	
			entry.DIR_Name[10] = '\0';

			//Set each string to be lowercase
			for(int x = 0; x < strlen(filename); x++){ filename[x] = tolower(filename[x]); }
			for(int x = 0; x < strlen(entry.DIR_Name); x++){ entry.DIR_Name[x] = tolower( entry.DIR_Name[x]); }

			//if filename is found & mode is valid, remove from table of opened files  
			if(strncmp(entry.DIR_Name, filename, strlen(filename)) == 0){

				if (entry.DIR_Attr == 0x10)
					{
						printf("ERROR: Cannot open directory.\n");
					}
				else
					rmFromFileTable(filename, entry.DIR_FstClusHI << 16 | entry.DIR_FstClusLO);
					
				return;
			}

			counter += 1;	
			current_byte += 32; //each address is +32 of previous
		
			//If we reach 16 loops (end of sector, 512 bytes), need to go into FAT to look up next cluster, go to that addr
			if(counter % 15 == 0){
				cluster_num = getNextCluster(img, cluster_num);
			
				//If getNextCluster returns -1, it's end of cluster. Time to stop
				if(cluster_num == 0x0FFFFFFF){
					printf("%s\n", "ERROR: Filename not found.");
					return;
				}			

				current_byte = (cluster_num * 0x200) + 0x100000;
			}
		//read in next entry
		entry = getEntryInfo(img, current_byte);					
	}

	printf("%s\n", "ERROR: Filename not found.");
	return;
}



void creat(FILE* img, char* filename, int cwd_addr, int dir){

	//Check cwd for file of same name, if found return error This also gets us to the first available space in the
	//directory to put our new file.
	unsigned int current_byte = cwd_addr;
	unsigned int cluster_num = (current_byte - 0x100000) / 0x200;

	unsigned int fat_cursor = 0;
	unsigned int empty_cluster_num = 0;
	unsigned int end_of_cluster = 0x0FFFFFFF;
	
	int counter = 0;

	//read in first directoryEntry @current_byte
	Fat32DirectoryEntry entry = getEntryInfo(img, current_byte);

	//While the attribute is not zero indicating the end of the directory, keep reading in.
	while(entry.DIR_Attr != 0){
		entry.DIR_Name[10] = '\0';
		for(int x = 0; x < strlen(filename); x++){ filename[x] = tolower(filename[x]); }
		for(int x = 0; x < strlen(entry.DIR_Name); x++){ entry.DIR_Name[x] = tolower( entry.DIR_Name[x]); }

		//if filename == name, return error.  
		if(strncmp(entry.DIR_Name, filename, strlen(filename)) == 0){
			printf("ERROR: File of same name exists in current directory.\n");
			return;
		}
		
		counter += 1;	
		current_byte += 32; //each address is +32 of previous
		
		//If we reach 16 loops (end of sector, 512 bytes), need to go into FAT to look up next cluster, go to that addr
		if(counter % 16 == 0){
			cluster_num = getNextCluster(img, cluster_num);

			//If getNextCluster returns -1, it's end of cluster. Need to alloc a new one.
			if(cluster_num == 0x0FFFFFFF || cluster_num == 0x0FFFFFF8){
				
				// Go into FAT and find first empty cluster, and put in 0x0FFFFFF.
				for(int x = 0; x < bootsector.BPB_FatSz32; x++){
					fseek(img, FATFirstByte + x*4, SEEK_SET);
					fread(&fat_cursor, sizeof(unsigned int), 1, img);
				
					//Once found, Set cluster to be 0x0FFFFFFF and grab the first byte of the cluster
					if(fat_cursor == 0){
						//printf("NEW DIR LOC: Free cluster found at 0x%x  (%dth cluster)\n",FATFirstByte + x*4, x);
						fseek(img, FATFirstByte + x*4, SEEK_SET);
						fwrite(&end_of_cluster, sizeof(unsigned int), 1, img);
						empty_cluster_num = x;
						break;
					}
				}	
				
				// set data at cluster_num to point to that cluster number
				fseek(img, FATFirstByte + ((cwd - 0x100000) / 0x200)*4, SEEK_SET);	
				fwrite(&empty_cluster_num, sizeof(unsigned int), 1, img);
	
				//set cluster_num to be the newly allocated cluster
				cluster_num = empty_cluster_num;
			}			

			current_byte = (cluster_num * 0x200) + 0x100000;
		}

		entry = getEntryInfo(img, current_byte);					
	}
	
	//If no file with same name, parse FAT until an empty cluster is found (00 00 00 00 in FAT)
	fat_cursor = 0;
	empty_cluster_num = 0;


	for(int x = 0; x < bootsector.BPB_FatSz32; x++){
		fseek(img, FATFirstByte + x*4, SEEK_SET);
		fread(&fat_cursor, sizeof(unsigned int), 1, img);
		
		//Once found, Set cluster to be 0x0FFFFFFF and grab the first byte of the cluster
		if(fat_cursor == 0){
			//printf("Free cluster found at 0x%x  (%dth cluster)\n",FATFirstByte + x*4, x);
			fseek(img, FATFirstByte + x*4, SEEK_SET);
			fwrite(&end_of_cluster, sizeof(unsigned int), 1, img);
			empty_cluster_num = x;
			break;
		}
	}	
	
	if(empty_cluster_num == 0){ printf("ERROR: Could not create new file. No free clusters, FAT is full.\n"); return; }

	// Split into two (ie, 0000 & 0b41), and reverse endianness (0000, 410b).
	unsigned short cluster_high = empty_cluster_num >> 16;
	unsigned short cluster_low = (empty_cluster_num << 16) >> 16;
	unsigned char attribute = 0x20;	
	char file_to_insert[11];

	for(int x = 0; x < 11; x++){
		if(filename[x] != NULL){ file_to_insert[x] = filename[x]; }
		else{ file_to_insert[x] = ' '; }
	}		

	//printf("HI: 0x%x, LO: 0x%x", cluster_high, cluster_low);

	if(dir == 1){
		attribute = 0x10;
	}

	//In cwd, store new directory file at first available space with name, cluster nums, 0x10 attribute, zero size, etc.
	fseek(img, current_byte, SEEK_SET);	
	fwrite(&file_to_insert, sizeof(char), 11, img); //DIR_Name;
	
	fseek(img, current_byte+11, SEEK_SET);	
	fwrite(&attribute, sizeof(unsigned char), 1, img); //DIR_Attr;

	fseek(img, current_byte+20, SEEK_SET);	
	fwrite(&cluster_high, sizeof(unsigned short), 1, img); //DIR_FstClusHI;
	
	fseek(img, current_byte+26, SEEK_SET);	
	fwrite(&cluster_low, sizeof(unsigned short), 1, img); //DIR_FstClusLO;

	//If the object we're wiritng is a directory, must initialize '.' and '..' in the actual directory.
	if(dir == 1){
		//Go to the first cluster number, write directory entries for '.' and '..'
		char curdir[11] = {'.',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};
		char pardir[11] = {'.','.',' ',' ',' ',' ',' ',' ',' ',' ',' '};
		unsigned short cluster_high_parent = ((cwd_addr - 0x100000) / 0x200) >> 16;
		unsigned short cluster_low_parent = (((cwd_addr - 0x100000) / 0x200) << 16) >> 16;		

		//Current Directory '.'
		fseek(img, 0x100000 + (bootsector.BPB_BytsPerSec*empty_cluster_num), SEEK_SET);	
		fwrite(&curdir, sizeof(char), 11, img); //DIR_Name;

		fseek(img, 0x100000 + (bootsector.BPB_BytsPerSec*empty_cluster_num) + 11, SEEK_SET);	
		fwrite(&attribute, sizeof(unsigned char), 1, img); //DIR_Attr;

		fseek(img, 0x100000 + (bootsector.BPB_BytsPerSec*empty_cluster_num)+20, SEEK_SET);	
		fwrite(&cluster_high, sizeof(unsigned short), 1, img); //DIR_FstClusHI;
	
		fseek(img, 0x100000 + (bootsector.BPB_BytsPerSec*empty_cluster_num)+26, SEEK_SET);	
		fwrite(&cluster_low, sizeof(unsigned short), 1, img); //DIR_FstClusLO;

		//Parent Directory '..'
		fseek(img, 0x100000 + (bootsector.BPB_BytsPerSec*empty_cluster_num) + 32, SEEK_SET);	
		fwrite(&pardir, sizeof(char), 11, img); //DIR_Name;

		fseek(img, 0x100000 + (bootsector.BPB_BytsPerSec*empty_cluster_num) + 32 + 11, SEEK_SET);	
		fwrite(&attribute, sizeof(unsigned char), 1, img); //DIR_Attr;

		fseek(img, 0x100000 + (bootsector.BPB_BytsPerSec*empty_cluster_num) + 32 +20, SEEK_SET);	
		fwrite(&cluster_high_parent, sizeof(unsigned short), 1, img); //DIR_FstClusHI;
	
		fseek(img, 0x100000 + (bootsector.BPB_BytsPerSec*empty_cluster_num) + 32 +26, SEEK_SET);	
		fwrite(&cluster_low_parent, sizeof(unsigned short), 1, img); //DIR_FstClusLO;
	
	}

}


void removedir(FILE* img, char* filename, int cwd_addr){

	//Calculate the cluster number from the address given for lookup into FAT
	unsigned int current_byte = cwd_addr;
	unsigned int cluster_num = (current_byte - 0x100000) / 0x200;
	unsigned int byte_of_entry = 0;
	
	unsigned char shouldBeZero = 0;
	
	int counter = 0;

	//read in first directoryEntry @current_byte
	Fat32DirectoryEntry entry = getEntryInfo(img, current_byte);

	//While the attribute is not zero indicating the end of the directory, keep reading in.
	while(entry.DIR_Attr != 0){
	
		entry.DIR_Name[10] = '\0';

		//Set each string to be lowercase
		for(int x = 0; x < strlen(filename); x++){ filename[x] = tolower(filename[x]); }
		for(int x = 0; x < strlen(entry.DIR_Name); x++){ entry.DIR_Name[x] = tolower( entry.DIR_Name[x]); }

		//We find the directory to delete
		if(strncmp(entry.DIR_Name, filename, strlen(filename)) == 0 && entry.DIR_Attr == 0x10){

			byte_of_entry = current_byte;

			//get upper and lower bytes of first cluster, put them together and switch endianness.
			cluster_num = entry.DIR_FstClusHI << 16 | entry.DIR_FstClusLO;
			current_byte = (cluster_num * 0x200) + 0x100000;

			//Loop through directory and check it's empty after '.' and '..'
			for(int x = 65; x < 512; x+=32){
				fseek(img, current_byte+x, SEEK_SET);	
				fread(&shouldBeZero, sizeof(unsigned char), 1, img);
	
				if(shouldBeZero != 0){
					printf("ERROR: Cannot remove directory that is not empty.\n");
					return;
				}
			}
			
			shouldBeZero = 0;

			//Delete directory contents
			//printf("Deleting 64 bytes at address 0x%X (. & .. in dir)\n", current_byte);
			for(int x = 0; x < 64; x++){			
				fseek(img, current_byte+x, SEEK_SET);	
				fwrite(&shouldBeZero, sizeof(unsigned char), 1, img);
			}
			
			//Remove from FAT	
			//printf("Deleting 4 bytes at address 0x%X (FAT Entry)\n", FATFirstByte + (cluster_num*4));			
			for(int y = 0; y < 4; y++){	
				fseek(img, FATFirstByte + (cluster_num*4) + y, SEEK_SET);	
				fwrite(&shouldBeZero, sizeof(unsigned char), 1, img);
			}
	
			//Remove from cwd
			//printf("Deleting 32 bytes at address 0x%X (Dir entry in parent directory)\n", byte_of_entry);	
			for(int z = 0; z < 32; z++){				
				fseek(img, byte_of_entry+z, SEEK_SET);
				fwrite(&shouldBeZero, sizeof(unsigned char), 1, img);
			}
			
			return;

		}

		counter += 1;	
		current_byte += 32; //each address is +32 of previous
		
		//If we reach 16 loops (end of sector, 512 bytes), need to go into FAT to look up next cluster, go to that addr
		if(counter % 16 == 0){
			cluster_num = getNextCluster(img, cluster_num);
			
			//If getNextCluster returns -1, it's end of cluster. Time to stop
			if(cluster_num == 0x0FFFFFFF){
				printf("%s\n", "ERROR: Filename not found.");
				return;
			}			

			current_byte = (cluster_num * 0x200) + 0x100000;
		}

		//read in next entry
		entry = getEntryInfo(img, current_byte);		
				
	}

	printf("%s\n", "ERROR: Directory not found.");
	return;

}

void removefile(FILE* img, char* filename, int cwd_addr){

	//Calculate the cluster number from the address given for lookup into FAT
	unsigned int current_byte = cwd_addr;
	unsigned int cluster_num = (current_byte - 0x100000) / 0x200;
	unsigned int byte_of_entry = 0;
	unsigned int next_cluster = 0;
	unsigned char zero_byte = 0;
	
	int counter = 0;

	//read in first directoryEntry @current_byte
	Fat32DirectoryEntry entry = getEntryInfo(img, current_byte);

	//While the attribute is not zero indicating the end of the directory, keep reading in.
	while(entry.DIR_Attr != 0){
	
		entry.DIR_Name[10] = '\0';

		//Set each string to be lowercase
		for(int x = 0; x < strlen(filename); x++){ filename[x] = tolower(filename[x]); }
		for(int x = 0; x < strlen(entry.DIR_Name); x++){ entry.DIR_Name[x] = tolower( entry.DIR_Name[x]); }

		//printf("Filenames: '%s' '%s'\n",filename, entry.DIR_Name);

		//We find the directory to delete
		if(strncmp(entry.DIR_Name, filename, strlen(filename)) == 0 && entry.DIR_Attr != 0x10){

			byte_of_entry = current_byte;

			//get upper and lower bytes of first cluster, put them together and switch endianness.
			cluster_num = entry.DIR_FstClusHI << 16 | entry.DIR_FstClusLO;
			
			//If cluster number is zero, we simply remove the entry from the directory.
			if(cluster_num == 0){ 
				for(int z = 0; z < 32; z++){				
					fseek(img, byte_of_entry+z, SEEK_SET);
					fwrite(&zero_byte, sizeof(unsigned char), 1, img);
				}
				return; 
			}
			
			current_byte = (cluster_num * 0x200) + 0x100000;

			//Once we find the first cluster of file to be deleted, go through each cluster of that file and delete
			//its contents.
			while(1){			
				
				//printf("current_byte = 0x%x", current_byte);				
		
				//Loop through file's cluster and delete everything in it.
				for(int x = 0; x < 512; x++){
					fseek(img, current_byte+x, SEEK_SET);	
					fwrite(&zero_byte, sizeof(unsigned char), 1, img);
				}
				
				//If next cluster is end of cluster, finished deleting entire file contents.
				if(getNextCluster(img, cluster_num) == 0x0FFFFFFF){
					for(int y = 0; y < 4; y++){
						fseek(img, FATFirstByte + (cluster_num*4)+y, SEEK_SET);	
						fwrite(&zero_byte, sizeof(unsigned char), 1, img);
					}
					break;
				}
				//Otherwise need to zero out current cluster, move on to next to start deleting its data.
				else{
					next_cluster = getNextCluster(img, cluster_num);
					for(int y = 0; y < 4; y++){
						fseek(img, FATFirstByte + (cluster_num*4)+y, SEEK_SET);	
						fwrite(&zero_byte, sizeof(unsigned char), 1, img);
					}
					
					cluster_num = next_cluster;
					current_byte = (cluster_num * 0x200) + 0x100000;	
				}	
			
			}

			//Remove from cwd
			//printf("Deleting 32 bytes at address 0x%X (Dir entry in parent directory)\n", byte_of_entry);	
			for(int z = 0; z < 32; z++){				
				fseek(img, byte_of_entry+z, SEEK_SET);
				fwrite(&zero_byte, sizeof(unsigned char), 1, img);
			}
			
			return;

		}

		counter += 1;	
		current_byte += 32; //each address is +32 of previous
		
		//If we reach 16 loops (end of sector, 512 bytes), need to go into FAT to look up next cluster, go to that addr
		if(counter % 16 == 0){
			cluster_num = getNextCluster(img, cluster_num);
			
			//If getNextCluster returns -1, it's end of cluster. Time to stop
			if(cluster_num == 0x0FFFFFFF || cluster_num == 0x0FFFFFF8){
				printf("%s\n", "ERROR: Filename not found.");
				return;
			}			

			current_byte = (cluster_num * 0x200) + 0x100000;
		}

		//read in next entry
		entry = getEntryInfo(img, current_byte);				
	}

	printf("%s\n", "ERROR: File not found.");
	return;

}


void readFile(FILE* img, char* filename, unsigned int offset, unsigned int size, int cwd_addr){
	int found = 0;
	int filenamesize = sizeof(filename);
	int clusterOffset = offset / bootsector.BPB_BytsPerSec;
	int byteOffset = offset % bootsector.BPB_BytsPerSec;

	//printf("filename: %s\noffset: %d\nfilename size: %d\n\n", filename, offset, filenamesize);

	//check if file is open
	for (struct fileTable* itr = rootNode; itr != NULL; itr = itr->next)
	{
		if (strcmp(itr->file_name, filename) == 0){
			//printf("File was found (open).\n");
			found = 1;

		//check for read permissions
		if (strcmp(itr->file_mode, "r") != 0 && strcmp(itr->file_mode, "rw") != 0 && strcmp(itr->file_mode, "wr") != 0){
    		printf("ERROR: Cannot read file.\n");
			return;
			}
		}
	}

	//exit if file not opened
	if (found == 0){
		printf("ERROR: File is not open.\n");
		return;}

	/* -------------------- look through clusters of the file to find its contents -----------------------*/

		unsigned int current_byte = cwd_addr;
		//get cluster number
		unsigned int cluster_num = (current_byte - 0x100000) / 0x200;	
		int counter = 0;
		int clustNum, beginAddr;
		int i = 0;
        	int x = 0;
		unsigned char buffer[512];
        	char theChar;

		//read in first directoryEntry @current_byte
		Fat32DirectoryEntry entry = getEntryInfo(img, current_byte);

		//While the attribute is not zero (indicating the end of the directory) keep reading in
		while(entry.DIR_Attr != 0){
	
			entry.DIR_Name[10] = '\0';

			//Set each string to be lowercase
			for(int x = 0; x < strlen(filename); x++){ filename[x] = tolower(filename[x]); }
			for(int x = 0; x < strlen(entry.DIR_Name); x++){ entry.DIR_Name[x] = tolower( entry.DIR_Name[x]); }

			//if filename is found  
			if(strncmp(entry.DIR_Name, filename, strlen(filename)) == 0){

			if (offset > entry.DIR_FileSize){
				printf("ERROR: Exceeds file size.\n");
			return;}

                        if (size > entry.DIR_FileSize){
                            size = entry.DIR_FileSize;
			}
                        //calculate bytes to read in this cluster
                        int bytesToRead = bootsector.BPB_BytsPerSec - offset;
						//get cluster # from the file 
						clustNum = (entry.DIR_FstClusHI << 16 | entry.DIR_FstClusLO);

                        for (x = 0; x < clusterOffset; x++){
                            clustNum = getNextCluster(img, clustNum);
                            }
						beginAddr = (clustNum * bootsector.BPB_BytsPerSec + 0x100000) + byteOffset;


                        int remainingSize = size;
                        while(remainingSize > 0){
                	    fseek(img, beginAddr, SEEK_SET);
          
                            fread(&theChar, sizeof(unsigned char), 1, img);
                            printf("%c", theChar);
                            beginAddr++;
                            bytesToRead--;
                            remainingSize--;

                            if (bytesToRead == 0){
                                clustNum = getNextCluster(img, clustNum);
                                bytesToRead = bootsector.BPB_BytsPerSec;
                                beginAddr = (clustNum * bootsector.BPB_BytsPerSec + 0x100000); 
                            }
                            if (remainingSize == 0)
                                return;
                        }
    
			return;
			}

			/* ----------------------------- finished reading in contents --------------------------------*/

			counter += 1;	
			current_byte += 32; //each address is +32 of previous
		
			//If we reach 16 loops (end of sector, 512 bytes), need to go into FAT to look up next cluster, go to that addr
			if(counter % 15 == 0){
				cluster_num = getNextCluster(img, cluster_num);
			
				//If getNextCluster returns -1, it's end of cluster. Time to stop
				if(cluster_num == 0x0FFFFFFF){
					printf("%s\n", "ERROR: Filename not found.");
					return;
				}			

				current_byte = (cluster_num * 0x200) + 0x100000;
			}
		//read in next entry
		entry = getEntryInfo(img, current_byte);					
	}
	
}


void writefile(FILE * img, char * filename, int cwd_addr, int offset, int size, char * input_string) {

    int found = 0;
    int clusterOffset = offset / bootsector.BPB_BytsPerSec;
    int byteOffset = offset % bootsector.BPB_BytsPerSec;
    int fat_cursor = 0;
    int empty_cluster_num = 0;
    unsigned int end_of_cluster = 0x0FFFFFFF;

    //First check if file is open and is writable.
    for (struct fileTable * itr = rootNode; itr != NULL; itr = itr -> next) {
        if (strcmp(itr -> file_name, filename) == 0) {
            //printf("File was found (open).\n");
            found = 1;

            //check for read permissions
            if (strcmp(itr -> file_mode, "w") != 0 && strcmp(itr -> file_mode, "rw") != 0 && strcmp(itr -> file_mode, "wr") != 0) {
                printf("ERROR: Cannot read file.\n");
                return;
            }
        }
    }

    //exit if file not opened/not writable.
    if (found == 0) {
        printf("ERROR: File is not open.\n");
        return;
    }

    //Once file is found and is open, look for it in the cwd and write to it if found.
    unsigned int current_byte = cwd_addr;

    unsigned int cluster_num = (current_byte - 0x100000) / 0x200;
    int counter = 0;
    int clustNum, beginAddr;
    int i = 0;
    int x = 0;
    unsigned char buffer[512];
    unsigned char zero_byte = 0;
    int finalCluster = 0;

    //read in first directoryEntry @current_byte
    Fat32DirectoryEntry entry = getEntryInfo(img, current_byte);

    //While the attribute is not zero (indicating the end of the directory) keep reading in
    while (entry.DIR_Attr != 0) {

        entry.DIR_Name[10] = '\0';

        //Set each string to be lowercase
        for (int x = 0; x < strlen(filename); x++) {
            filename[x] = tolower(filename[x]);
        }
        for (int x = 0; x < strlen(entry.DIR_Name); x++) {
            entry.DIR_Name[x] = tolower(entry.DIR_Name[x]);
        }

        //if filename is found  
        if (strncmp(entry.DIR_Name, filename, strlen(filename)) == 0) {

            if (offset > entry.DIR_FileSize && entry.DIR_FileSize != 0) {
                printf("ERROR: Exceeds file size.\n");
                return;
            }

            //calculate bytes to write in this cluster
            int bytesToPrint = bootsector.BPB_BytsPerSec - byteOffset;
            //get cluster # from the file 
            clustNum = (entry.DIR_FstClusHI << 16 | entry.DIR_FstClusLO);

            //Get to the right cluster
            for (x = 0; x < clusterOffset; x++) {
                clustNum = getNextCluster(img, clustNum);
            }

            //Set beginAddr to the first byte + byte_offset in the right cluster.
            beginAddr = (clustNum * bootsector.BPB_BytsPerSec + 0x100000) + byteOffset;

			//printf("Beginning cluster address is %X\n",beginAddr);

            int char_counter = 0;
            int remainingSize = size;
			int end_of_file = 0;
			int next_cluster = 0;

            while (end_of_file == 0) {
                
				fseek(img, beginAddr, SEEK_SET);

                if (char_counter < strlen(input_string)) {
                    fwrite( & input_string[char_counter], sizeof(char), 1, img);
                }
                //Once we start writing zeroes, make note of that cluster number to deallocate after once we've written.
                else if (char_counter == strlen(input_string)) {
                    finalCluster = clustNum;
                    fwrite( & zero_byte, sizeof(unsigned char), 1, img);
                }
                //if size > strlen, print the string and size-strlent 0s, then fill rest of file with 0s. 
                else {
                    fwrite( & zero_byte, sizeof(unsigned char), 1, img);
                }
				
								
			
                beginAddr++;
                bytesToPrint--;
                remainingSize--;
                char_counter++;
			
				//Get to end of current cluster, need to go to next cluster and continue writing.
                if (bytesToPrint == 0) {
                    if (getNextCluster(img, clustNum) != 0x0FFFFFFF) {
                        clustNum = getNextCluster(img, clustNum);
                        bytesToPrint = bootsector.BPB_BytsPerSec;
                        beginAddr = (clustNum * bootsector.BPB_BytsPerSec + 0x100000);
                    }
                    //if end of cluster value returned on a get next cluster, allocate new one and write there.
                    else if (getNextCluster(img, clustNum) == 0x0FFFFFFF && remainingSize > 0) {

                        // Go into FAT and find first empty cluster, and put in 0x0FFFFFF.
                        for (int x = 0; x < bootsector.BPB_FatSz32; x++) {
                            fseek(img, FATFirstByte + x * 4, SEEK_SET);
                            fread( & fat_cursor, sizeof(unsigned int), 1, img);

                            //Once found, Set cluster to be 0x0FFFFFFF and grab the first byte of the cluster
                            if (fat_cursor == 0) {
                                fseek(img, FATFirstByte + x * 4, SEEK_SET);
                                fwrite( & end_of_cluster, sizeof(unsigned int), 1, img);
                                empty_cluster_num = x;
                                break;
                            }
                        }

                        // set data at cluster_num to point to that cluster number
                        fseek(img, FATFirstByte + clustNum * 4, SEEK_SET);
                        fwrite( & empty_cluster_num, sizeof(unsigned int), 1, img);

                        //set cluster_num to be the newly allocated cluster
                        clustNum = empty_cluster_num;
                        bytesToPrint = bootsector.BPB_BytsPerSec;
                        beginAddr = (clustNum * bootsector.BPB_BytsPerSec + 0x100000);

                        fat_cursor = 0;
                        empty_cluster_num = 0;

                    }
					else { end_of_file = 1; }
                }

            }

			//Once we finish wiritng, go to noted cluster num in FAT, run through and empty out FAT clusters if more exist after..
			if(getNextCluster(img, finalCluster) != 0x0FFFFFFF){

				clustNum = getNextCluster(img, finalCluster);

				fseek(img, FATFirstByte + (finalCluster*4), SEEK_SET);	
				fwrite(&end_of_cluster, sizeof(unsigned int), 1, img);


				while(1){

					//If next cluster is end of cluster, finished deleting entire file contents.
					if(getNextCluster(img, clustNum) == 0x0FFFFFFF){
						for(int y = 0; y < 4; y++){
							fseek(img, FATFirstByte + (clustNum*4)+y, SEEK_SET);	
							fwrite(&zero_byte, sizeof(unsigned char), 1, img);
						}
						break;
					}
					//Otherwise need to zero out current cluster, move on to next to start deleting its data.
					else{
						next_cluster = getNextCluster(img, clustNum);

						for(int y = 0; y < 4; y++){
							fseek(img, FATFirstByte + (clustNum*4)+y, SEEK_SET);	
							fwrite(&zero_byte, sizeof(unsigned char), 1, img);
						}
						
						clustNum = next_cluster;

					}	
				
				}
			
			}

			//Lastly, caluclate new size and fwrite it to the entry in the directory.
			unsigned int new_size = 0;

			if(size > strlen(input_string)){
				new_size = offset + strlen(input_string);
			}
			else{ 
				new_size =  offset + size;
			}

			fseek(img, current_byte+28, SEEK_SET);	
			fwrite(&new_size, sizeof(unsigned int), 1, img);
			
            return;

        }

        /* ----------------------------- finished reading in contents --------------------------------*/

        counter += 1;
        current_byte += 32; //each address is +32 of previous

        //If we reach 16 loops (end of sector, 512 bytes), need to go into FAT to look up next cluster, go to that addr
        if (counter % 15 == 0) {
            cluster_num = getNextCluster(img, cluster_num);

            //If getNextCluster returns -1, it's end of cluster. Time to stop
            if (cluster_num == 0x0FFFFFFF) {
                printf("%s\n", "ERROR: Filename not found.");
                return;
            }

            current_byte = (cluster_num * 0x200) + 0x100000;
        }
        //read in next entry
        entry = getEntryInfo(img, current_byte);
    }  

}
