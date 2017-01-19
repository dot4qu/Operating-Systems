/*
Written by Brian Team (dot4qu)
Date: 11/21/16
This C++ source file implements a barebones FAT16 filesystem controller.
It supports the cd, ls, cpin, and cpout commands.
*/

#include "fat.h"

//GLOBALS
int root_dir_start;						/* holds the offset within the filesystem file for the root directory */
int fat_start;							/* holds the offset within the filesystem file for the FAT */
int data_start;							/* holds the offset within the filesystem file for the data section */
unsigned char sectors_per_cluster;		/* the number of sectors per cluster */
unsigned short bytes_per_sector;		/* the number of bytes per sector */

FILE* read_boot_sector(Fat16BootSector *bs, char *data_file) {
	FILE *raw = fopen(data_file, "rb+");
	fread(bs, sizeof(*bs), 1, raw);
	return raw;
}

void fix_filename(unsigned char *buf, unsigned char *new_buf) {
	int i = 0;
	//iterate along filename copying all letters into new buf. Needs index check b/c if filename 8 bytes, keeps going since buf is passed as pointer
	while (buf[i] != ' ' && i < 8) {
		new_buf[i] = buf[i];
		i++;
	}
	//replace first space with null terminator
	new_buf[i] = '\0';
}

void fix_extension(unsigned char *buf, unsigned char *new_buf) {
	int i = 0;
	//iterate along filename copying all letters into new buf. Needs index check b/c if filename 8 bytes, keeps going since buf is passed as pointer
	while (buf[i] != ' ' && i < 3) {
		new_buf[i] = buf[i];
		i++;
	}
	//replace first space with null terminator
	new_buf[i] = '\0';
}

int read_dir_entries(string filename, Fat16Entry **entries_arr, FILE *in) {
	int i = 0;
	unsigned char name_buf[9];
	int temp;

	if (filename == "/") {
		//root dir case
		fseek(in, root_dir_start, SEEK_SET);
		 temp = ftell(in);
	} else {
		while (entries_arr[i] != NULL) {
			fix_filename(entries_arr[i]->filename, name_buf);
			if (!strcmp(filename.c_str(), (char*) name_buf)) {
				//this is the entry of the dir we're looking at, need to go to it on disk
				fseek(in, data_start + (entries_arr[i]->starting_cluster - 2) * sectors_per_cluster * bytes_per_sector, SEEK_SET);
				temp = ftell(in);
				break;
			} 
			i++; 
		}
	}

	i = 0;
	while(true) {
		Fat16Entry *new_entry = new Fat16Entry;
		fread(new_entry, sizeof(Fat16Entry), 1, in);
		
		//first byte of filename is zero, that means we're done reading files for this directory
		if (new_entry->filename[0] == 0x00) {
			//just read in a blank entry, need to rewind 32 before returning for copy_file_in's use of filepointer for new entry addition
			fseek(in, -32, SEEK_CUR);
			break;
		}
		//checking for long filename attr. If it is, toss this entry , don't need it
		if (new_entry->attributes == 0x0F)
			continue;
		//set current index of array to address of new_entry (since the variable is a pointer)
		entries_arr[i] = new_entry;
		//increment index in entry array
		i++;
	}

	return i;
}

string parse_input(vector<string> &out) {
	int end = 0;
	int start = 0;
	int arr_idx = 0;
	string ret_dir;
	string input_line;

	out.clear();

	//read in user input line
	getline(cin, input_line);

	while (input_line[end] != '\0') {
		if (input_line[end] == ' ') {
			out.push_back(input_line.substr(start, end - start));
			end++;
			start = end;
			continue;
		}
		end++;
	}
	//push back last dir b/c while loop kicks out at \0 w/o printing
	out.push_back(input_line.substr(start, end - start));
	end++;

	if (out.size() <= 1) {
		ret_dir = "";
	} else if (out.at(0) == "ls") {
		ret_dir = out.at(1);
	} else if (out.at(0) == "cd") {
		ret_dir = out.at(1);
	} else if (out.at(0) == "cpin") {
		//for copy in, we return the local filepath we want
		return out.at(2);
	} else if (out.at(0) == "cpout") {
		//for copy out, we return the local filepath to copy out
		return out.at(1);
	}

	return ret_dir;
}

void print_dir(int num_entries, Fat16Entry **entries) {
	int i = 0;
	unsigned char name_buf[9];
	unsigned char ext_buf[4];

	for (i = 0; i < num_entries; i++) {
		//strips unneccesary trailing spaces if filename not 8 chars
		fix_filename(entries[i]->filename, name_buf);

		if (entries[i]->attributes & (1 << 4)) {
			//directory
			printf("D %s\n", name_buf);
		} else if (entries[i]->attributes & (1 << 3) || entries[i]->attributes & (1 << 1)) {
			//volume label or hidden file, dont print
		} else {
			//file
			fix_extension(entries[i]->ext, ext_buf);
			printf("F %s.%s\n", name_buf, ext_buf);
		}
	}
}

int find_dir(string dir, Fat16Entry **entries, FILE *file) {
	int start = 0;
	int end = 0;
	int i = 0;
	int num_entries = 0;
	string temp_dir;
	unsigned char name_buf[9];

	while(true) {
		if (dir[end] == '/') {
			//just pulled in another dirname, need to read its entries into array
			if (start == end)
				//case of pulling in first slash of absolute path
				temp_dir = dir[start];
			else 
				temp_dir = dir.substr(start, end - start);
			//update our entries array for this dir
			num_entries = read_dir_entries(temp_dir, entries, file);
			end++;
			start = end;
		} else if (dir[end] == '\0') {
			if (start != end) {
				//update our entries array for the final dir (start != end checks case of only finding root dir) then break out
				temp_dir = dir.substr(start, end - start);
				num_entries = read_dir_entries(temp_dir, entries, file);
			}
			break;
		} else {
			//still moving through dirname
			end++;
		}
	}
	return num_entries;
}

string remove_ext(string filename) {
	int i = filename.length() - 1;
	while (filename[i] != '.') {
		i--;
	}
	return filename.substr(0, i);
}

void copy_file_out(Fat16Entry *entry, string file_out, FILE *in) {
	//holds two byte value pulled in from current fat location
	unsigned char fat_value[2] = {0, 0};
	const short fat_eof = 0xFFFF;
	//hold the number representation of current fat index used for seeking that much from the fat_start
	short fat_offset = entry->starting_cluster;
	
	//open file on path within host
	FILE *out = fopen(file_out.c_str(), "wb");
	chmod(file_out.c_str(), 0777);
	//printf("Error: %s\n", strerror(errno));
	
	unsigned int remaining_filesize = entry->file_size;
	//declare buffer to hold read bytes before we write them
	char *filebuf = new char[4096];
	//seek and read in first value of this file within fat. offset by starting cluster from fat_start
	do {
		//seek to beginning of files actual data
		fseek(in, data_start + (fat_offset - 2) * sectors_per_cluster * bytes_per_sector, SEEK_SET);
		int temp = ftell(in);
		if (remaining_filesize >= 4096) {
			fread(filebuf, 4096, 1, in);
			fwrite(filebuf, 4096, 1, out);
			remaining_filesize -=4096;
		} else {
			fread(filebuf, remaining_filesize, 1, in);
			fwrite(filebuf, remaining_filesize, 1, out);
			remaining_filesize = 0;
		}

		//reading current entries fat value for end of do-while check
		fseek(in, fat_start + fat_offset * 2, SEEK_SET);
		temp = ftell(in);
		//updates fat_offset with value at current index. either FFFF or will point to next cluster to read data from for re-iteration of this loop
		fread(&fat_offset, sizeof(fat_offset), 1, in);
		
		//fat_offset = fat_value;
	} while (fat_offset != fat_eof)/*while (fat_value[0] != fat_eof[0] && fat_value[1] != fat_eof[1])*/;
	
	delete[] filebuf;
	fclose(out);
}

unsigned short find_open_fat_entry(FILE *in) {
	int previous_location = ftell(in);				/* save previous fpointer location */
	unsigned short fat_cluster = 1;					/* holds the index of the fatval we're currently reading in */
	short fat_val;									/* hold the values that we're iterating through in the fat until we find the first open (0x0000) entry */

	//skip over first two reserved fat entries to read first possible, sector 2
	fseek(in, fat_start + 4, SEEK_SET);
	do {
		fat_cluster++;
		fread(&fat_val, sizeof(short), 1, in);
	} while (fat_val != 0x0000);

	//move fpointer back to where it was when we started
	fseek(in, previous_location, SEEK_SET);

	return fat_cluster;
}

void build_entry(Fat16Entry *new_entry, string filename, int file_size, FILE *in) {
		int new_entry_location = ftell(in);				/* need to save where were saving entry because we need to seek to the fat to find first open cluster */

	//strip ext 
	string filename_no_ext = remove_ext(filename);
	int padding = 0;
	//copies each char of filename into new entry, then pads remaining space for 8 byte array with spaces if needed
	for (padding = 0; padding < filename_no_ext.length(); padding++) {
		new_entry->filename[padding] = filename_no_ext[padding];
	}
	for ( ; padding < 8; padding++) {
		new_entry->filename[padding] = ' ';
	}

	//iterate from end until hit period, then strip only the following text for ext
	int ext_idx = filename.length() - 1;
	while (filename[ext_idx] != '.') {
		ext_idx--;
	}
	string ext = filename.substr(ext_idx + 1);
	for (padding = 0; padding < ext.length(); padding++) {
		new_entry->ext[padding] = ext[padding];
	}
	for ( ; padding < 3; padding++) {
		new_entry->ext[padding] = ' ';
	}

	//hardcoded file (archive) attrs
	new_entry->attributes = 0x20;

	unsigned char reserved_bytes[10];
	*new_entry->reserved = *reserved_bytes;

	time_t curr_time = time(0);
	//hardcoded, not necessary
	new_entry->modify_time = 0;
	new_entry->modify_date = 0;

	//returns index of next available fat entry
	new_entry->starting_cluster = find_open_fat_entry(in);

	//rewind fpointer to where we're going to save this entry
	fseek(in, new_entry_location, SEEK_SET);

	new_entry->file_size = file_size;
}

void write_data(Fat16Entry *entry, FILE *in, FILE *source) {
	const short fat_eoc = 0xFFFF;
	//temporary filebuffer to read data into
	char filebuf[entry->file_size];
	fseek(in, fat_start + entry->starting_cluster * 2, SEEK_SET);
	fwrite(&fat_eoc, sizeof(short), 1, in);

	//seek to beginning of files actual data
	fseek(in, data_start + (entry->starting_cluster - 2) * sectors_per_cluster * bytes_per_sector, SEEK_SET);
	int data_location = ftell(in);				/* saves location of data for multicluster writes when we seek to fat to write in current clusters value at previous clusters location */

	if (entry->file_size <= 4096) {
		//single cluster data write
		//pulling in file data from host disk
		fread(filebuf, entry->file_size, 1, source);
		fwrite(filebuf, entry->file_size, 1, in);
	} else {
		//multi cluster data write
		int remaining_filesize = entry->file_size;
		unsigned short previous_fat_cluster = entry->starting_cluster;
		unsigned short current_fat_cluster;

		while(remaining_filesize > 0) {
			if (remaining_filesize <= 4096) {
				//copy only remaining filesize
				//pulling in file data from host disk
				fread(filebuf, remaining_filesize, 1, source);
				fwrite(filebuf, remaining_filesize, 1, in);
				//write EOC value into last fat slot
				fseek(in, fat_start + previous_fat_cluster * 2, SEEK_SET);
				fwrite(&fat_eoc, sizeof(short), 1, in);
				remaining_filesize = 0;
			} else {
				//pulling in file data from host disk
				fread(filebuf, 4096, 1, source);
				//copy full allotment of 4096		
				fwrite(filebuf, 4096, 1, in);
				data_location = ftell(in);
				//find next open fat spot and save as 'current'
				current_fat_cluster = find_open_fat_entry(in);
				//seek to location of previous fat spot
				fseek(in, fat_start + previous_fat_cluster * 2, SEEK_SET);
				//write value of newly saved current into previous' slot for the next hop
				fwrite(&current_fat_cluster, sizeof(short), 1, in);
				//now seek to current and write EOC value so our next iterations call to find_open_fat_entry doesnt grab the same one
				fseek(in, fat_start + current_fat_cluster * 2, SEEK_SET);
				fwrite(&fat_eoc, sizeof(short), 1, in);
				//set current as the previous cluster for next iteration
				previous_fat_cluster = current_fat_cluster;
				//seek back to proper data writing location
				fseek(in, data_location, SEEK_SET);
				//reduce remaining filesize
				remaining_filesize -= 4096;
			}
		}
	}
}

void copy_file_in(string host_read_str, string local_write_str, string cwd, FILE *in, Fat16Entry **entries) {
	FILE *host_read = fopen(host_read_str.c_str(), "rb");
	int file_size;
	int num_entries = 0;
	int entries_end_addr;

	//seek to file end to calculate filesize
	fseek(host_read, 0, SEEK_END);
	file_size = ftell(host_read);
	//seek back to beginnging for following fread of data
	fseek(host_read, 0, SEEK_SET);

	int index = local_write_str.length() - 1;
	string filename, filepath;
	//move index backwards from end until it hits space or /.
	//everything from [0] to index is filepath, everything from index to end is filename
	while (local_write_str[index] != '/' && index != 0) {
		index--;
	}
	if (index != 0) {
		//on a slash somewhere in the filename most likely
		filename = local_write_str.substr(index + 1);
		filepath = local_write_str.substr(0, index);
	} else {
		//were at zero index
		if (local_write_str[index] == '/') {
			//if were on a slash, substr from index + 1
			filename = local_write_str.substr(index + 1);
		} else {
			//not on a slas hbut still at index 0, so single filename of cwd given
			filename = local_write_str.substr(index);
		}
		filepath = cwd;
	}

	num_entries = find_dir(filepath, entries, in);
	//find dir calls read_dir_entries, which will end up reading the entries for our final dir (end of dir string)
	//this leaves the file pointer point at the null bytes directly after the last entry has been read
	entries_end_addr = ftell(in);

	Fat16Entry *new_entry = new Fat16Entry();
	build_entry(new_entry, filename, file_size, in);
	//writes new entry directly following final entry read since the fpointer is still sitting there from 'find_dir'
	fwrite(new_entry, sizeof(Fat16Entry), 1, in);

	//handles writing data to correct clusters and fat entries
	write_data(new_entry, in, host_read);
	fflush(in);

	entries_end_addr = ftell(in);
	delete[] new_entry;
}

int main(int argc, char **argv) {
	int curr_file_pos = 0;
	int num_entries = 0;
	int i = 0;
	string cwd, temp_cwd, local_copy, host_copy;
	vector<string> input;

	//pull in data_file name as only arg given (arg[0]) holds program filanme
	char *data_file = argv[1];

	Fat16BootSector *bs = new Fat16BootSector;
	FILE *in;

	//opens file and reads in bootsector bytes located at beginning of disk
	in = read_boot_sector(bs, data_file);

	//saving global values for use in other functions w/o passing bs reference
	sectors_per_cluster = bs->sectors_per_cluster;
	bytes_per_sector = bs->bytes_per_sector;

	//calculating global values used for seeking to offsets throughout fs
	root_dir_start = BOOT_SECTOR_SIZE + (bs->reserved_sectors - 1 + bs->fat_size_sectors * bs->number_of_fats) * bs->bytes_per_sector;
	fat_start = bs->bytes_per_sector * bs->reserved_sectors;
	data_start = root_dir_start + (bs->root_entry_count * 32);
	//Pointer is at end of boot sector hence the minus one for reserved sectors
	//This seeks past FATs and to the beginning of the root directory
	fseek(in, root_dir_start, SEEK_SET);

	//allocate array for root dir entries
	Fat16Entry *entries[bs->root_entry_count];
	//sets initial directory to root
	cwd = "/";		
	//reads and moves file pointer through entire root directory
	num_entries = read_dir_entries(cwd, entries, in);

	//reserve spots for 4 strings
	input.reserve(4);

	while (true) {
		printf(":%s>", &cwd[0]);
		//reads in the user input string and delimits by spaces and places each string in the input vector
		temp_cwd = parse_input(input);

		if (input.at(0) == "ls") {

			if (temp_cwd != "") {
				//only get temp_cwd entries if its not an empty string
				if (temp_cwd == ".") {
					//if '.', just reset it to cwd
					temp_cwd = cwd;
				}
				num_entries = find_dir(temp_cwd, entries, in);
			} else {
				//temp_cwd was empty so we just get it for ourselves (single ls command w/ no path)
				num_entries = find_dir(cwd, entries, in);
			}
			print_dir(num_entries, entries);

		} else if (input.at(0) == "cd") {

			if (temp_cwd != "") {
				//only get temp_cwd entries if its not an empty string
				if (temp_cwd == ".") {
					//if '.', just reset it to cwd
					temp_cwd = cwd;
				} else if (temp_cwd == "..") {
					//need to substring cwd to second to last dir
					int index = cwd.length() - 1;
					//move index backwards from end until it hits space or /.
					//everything from [0] to index is new cwd
					while (cwd[index] != '/' && index != 0) {
						//c--;
						index--;
					}
					if (index == 0)
						temp_cwd = "/";
					else 
						temp_cwd = cwd.substr(0, index);
				}

				num_entries = find_dir(temp_cwd, entries, in);
			} else {
				//temp_cwd was empty so we just get it for ourselves (single ls command w/ no path)
				num_entries = find_dir(cwd, entries, in);
			}

			if (temp_cwd[0] != '/') {
				//relative path, append to cwd
				if (cwd == "/") {
					cwd += temp_cwd;
				} else {
					cwd += "/" + temp_cwd;
				}
			} else {
				//absolute path, replace it
				cwd = temp_cwd;
			}

		} else if (input.at(0) == "cpin") {
			//string holding path of file within open fs
			local_copy = input.at(2);
			//string holding desired copy-to path in host fs
			host_copy = input.at(1);

			//function handler for all single or multi cluster data in copying
			copy_file_in(host_copy, local_copy, cwd, in, entries);

		} else if (input.at(0) == "cpout") {
			if (input.size() < 3) {
				exit(-1);
			}
			//string holding path of file within open fs
			local_copy = input.at(1);
			//string holding desired copy-to path in host fs
			host_copy = input.at(2);
			//temp_cwd holding path to local file to copy out
			//need to substring temp_cwd to seperate filename from path to dir holding file
			int index = local_copy.length() - 1;
			string filename, filepath;
			//move index backwards from end until it hits space or /.
			//everything from [0] to index is filepath, everything from index to end is filename
			while (local_copy[index] != '/' && index != 0) {
				index--;
			}

			if (index != 0) {
				//on a slash somewhere in the filename
				filename = local_copy.substr(index + 1);
				filepath = local_copy.substr(0, index);
			} else {
				//were at zero index
				if (local_copy[index] == '/') {
					//if were on a slash, substr from index + 1
					filename = local_copy.substr(index + 1);
				} else {
					//not on a slash but still at index 0, so single filename of cwd given
					filename = local_copy.substr(index);
				}
				filepath = cwd;
			}

			//strip ext b/c comparision with file in entries only does filename w/o ext
			string filename_no_ext = remove_ext(filename);
			//need to call find_dir on filepath to load in entries array with file info
			num_entries = find_dir(filepath, entries, in);

			unsigned char name_buf[9];
			int i = 0;
			int temp;

			while (i < num_entries) {
				fix_filename(entries[i]->filename, name_buf);
				if (!strcmp(filename_no_ext.c_str(), (char*) name_buf)) {
					break;
				} 
				i++; 
			}
			//copies file while checking and moving fat entries until FFFF is reached
			copy_file_out(entries[i], host_copy, in);

		} else if (input.at(0) == "exit") {
			if (in != NULL) {
				fclose(in);
			}
			return 0;
		} else {
			//not a recognized command, don't print anything 
			//printf("Unrecognized command\n");
		}

	}

	return 0;
}