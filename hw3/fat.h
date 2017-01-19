/*
Written by Brian Team (dot4qu)
Date: 11/21/16
This header file is responsible for listing the structs, 
macros, included files, and function prototypes for fat.cpp
*/

#include <stdlib.h> 
#include <stdio.h>
#include <string>
#include <iostream>
#include <vector>
#include <cstring>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctime>

using namespace std;

#define BOOT_SECTOR_SIZE 512

typedef struct {
    unsigned char jmp[3];
    char oem[8];
    unsigned short bytes_per_sector;
    unsigned char sectors_per_cluster;
    unsigned short reserved_sectors;
    unsigned char number_of_fats;
    unsigned short root_entry_count;
    unsigned short total_sectors_short; // if zero, later field is used
    unsigned char media_descriptor;
    unsigned short fat_size_sectors;
    unsigned short sectors_per_track;
    unsigned short number_of_heads;
    unsigned int hidden_sectors;
    unsigned int total_sectors_long;
    
    unsigned char drive_number;
    unsigned char current_head;
    unsigned char boot_signature;
    unsigned int volume_id;
    char volume_label[11];
    char fs_type[8];
    char boot_code[448];
    unsigned short boot_sector_signature;
} __attribute((packed)) Fat16BootSector;
 
typedef struct {
    unsigned char filename[8];
    unsigned char ext[3];
    unsigned char attributes;
    unsigned char reserved[10];
    unsigned short modify_time;
    unsigned short modify_date;
    unsigned short starting_cluster;
    unsigned int file_size;
} __attribute((packed)) Fat16Entry;

FILE* read_boot_sector(Fat16BootSector *bs);						/* helper function to read in the bootsector into the Fat16BootSector struct and return a file pointer to the filesystem .raw file */
void fix_filename(unsigned char *buf, unsigned char *new_buf);		/* strips trailing spaces off of filenames less than 8 characters */
void fix_extension(unsigned char *buf, unsigned char *new_buf);		/* strips trailing spaces off of file extensions less than 3 chars */
int read_dir_entries(string filename, Fat16Entry **entries_arr, FILE *in);	/* takes in a dir name and array of entries and iterates along them building array for searching through */
string parse_input(vector<string> &out);							/* reads in users input string, parses into a vector of terms, and returns the command given */
void print_dir(int num_entries, Fat16Entry **entries);				/* helper function to iterate through dir entries printing in the correct format */
int find_dir(string dir, Fat16Entry **entries, FILE *file);			/* takes absolute or relative filepath and iterates searching through each dir for the next to get the final set of entries */
string remove_ext(string filename);									/* strips ext of of input filename */
void copy_file_out(Fat16Entry *entry, string file_out, FILE *in);	/* major workhorse function to repeatedly copy filedata out until final cluster reached */
unsigned short find_open_fat_entry(FILE *in);						/* iterates through fat until first open cluster is found and returns index */
void build_entry(Fat16Entry *new_entry, string filename, int file_size, FILE *in);	/* builds up entry information when cpin'ing a file */
void write_data(Fat16Entry *entry, FILE *in, FILE *source);			/* writes actual file data to data section while iterating through fat values */
void copy_file_in(string host_read_str, string local_write_str, string cwd, FILE *in, Fat16Entry **entries);	/* major workhorse function to handle setting up entry and writing data for a cpin */
