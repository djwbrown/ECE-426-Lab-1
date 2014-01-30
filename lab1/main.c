//  main.c
//  lab1
//
//  Created by Dylan Brown on 1/9/14.
//  Copyright (c) 2014 Dylan Brown. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define MAXFILEENTRIES 64
#define MAXCLUSTERS 1024
#define END_OF_FILE 0xFFFF

/* Initialize the file allocation table (FAT) as an array of struct types.
 * With two byte addresses, there are a maximum of 16^4 or 64k
 * entries in FAT-16. We are asked to implement 1k or 1024 bytes only.
 * unsigned short type gives an address in the range 0x0000-0xFFFF.
 */
struct node {
    unsigned short clusterAddress;
    struct node *next;
};
struct node fat[MAXCLUSTERS] = {0x0000, NULL};

/* Create an array fileDirectory with MAXFILEENTRIES entries, each 32 bytes (one fileMetadata struct) */
struct fileMetadata {
    char name[8];
    char ext[3];
    short date;
    short time;
    int fileSize;
    unsigned short startAddress;
};
struct fileMetadata fileDirectory[MAXFILEENTRIES];
const struct fileMetadata emptyFile = {0};

/* Create an array of structs of size MAXCLUSTERS which simulates a disk drive in main memory */
struct cluster {
    char disk_data[512];
};
struct cluster disk[MAXCLUSTERS] = {'\0'};

void usage(void);
void create_data(char*, int);
int create_file(char [], int);
void delete_file(char []);
void write_file(char [], int, char *);
void read_file(char []);
void printClusters(char []);
void printFileMetadata(char []);

int main(int argc, const char * argv[])
{
    /* Initialize data for several 'files' of varying size */
    char file1[1000];
    char file2[2000];
    char file3[3000];
    char file4[4000];
    create_data(file1, sizeof(file1));
    create_data(file2, sizeof(file2));
    create_data(file3, sizeof(file3));
    create_data(file4, sizeof(file4));
    
    /* Check for proper CLI usage */
    if (argc > 1) usage();
    
    /* Drive the filesystem test */
    write_file("file1.txt", sizeof(file1), file1);
    write_file("file2.txt", sizeof(file2), file2);
    write_file("file3.txt", sizeof(file3), file3);
    delete_file("file2.txt");
    write_file("file4.txt", sizeof(file4), file4);
    
    printClusters("file1.txt");
    printClusters("file2.txt");
    printClusters("file3.txt");
    printClusters("file4.txt");
    
    fprintf(stdout, "-- FILE DIRECTORY --\n");
    printFileMetadata("file1.txt");
    printFileMetadata("file2.txt");
    printFileMetadata("file3.txt");
    printFileMetadata("file4.txt");
    fprintf(stdout, "\n");
    
    read_file("file1.txt");
    read_file("file2.txt");
    read_file("file3.txt");
    read_file("file4.txt");
    fprintf(stdout, "\n");
    
    return 0;
}

int getFileIndex(char filename[])
{
    /* Purpose: To search the fileDirectory for the index of a file.
     * Inputs:  char filename[]     a C-string containing the full filename (with extension).
     * Outputs: int                 the index of the file in the range [0, MAXFILEENTRIES) OR -1 for file not found.
     *
     * Note carefully the string tokenization with C stdlib, which is somewhat tricky.
     */
    
    char *mutableFilename = strdup(filename);
    char *name = strsep(&mutableFilename, ".");
    for (int idx = 0; idx < MAXFILEENTRIES; idx++) {
        if (strcmp(fileDirectory[idx].name, name) == 0) {
            return idx; // strcmp returning 0 indicates an exact match
        }
    }
    return -1;
}

void getDateString(char filename[], char dateString[], size_t maxlength)
{
    /* Purpose: To modify a given date string to match the file modification date of a file.
     * Inputs:  char filename[]     a C-string containing the full filename (with extension).
     *          char dateString[]   a char array buffer long enough for strftime().
     *          size_t maxlength    the size of the char array buffer sizeof(dateString).
     * Outputs: int                 the index of the file in the range [0, MAXFILEENTRIES) OR -1 for file not found.
     */
    
    int fileIndex = getFileIndex(filename);
    
    /* The bitwise operations below are used to decompress the file's date metadata */
    unsigned short fileDate = fileDirectory[fileIndex].date;
    unsigned short fileTime = fileDirectory[fileIndex].time;
    time_t emptyTime;
    struct tm *uncompressedTime = localtime(&emptyTime);
    
    uncompressedTime->tm_mday = fileDate & 0x1F;
    fileDate = fileDate >> 5;
    uncompressedTime->tm_mon = fileDate & 0x0F;
    fileDate = fileDate >> 4;
    uncompressedTime->tm_year = fileDate + 80;
    
    uncompressedTime->tm_sec = (fileTime & 0x1F) * 2;
    fileTime = fileTime >> 5;
    uncompressedTime->tm_min = fileTime & 0x3F;
    fileTime = fileTime >> 6;
    uncompressedTime->tm_hour = fileTime;
    
    /* Finally, use strftime() to format the given dateString in human readable form */
    strftime(dateString, maxlength, "%D %T", uncompressedTime);
}

void usage(void)
{
    /* Print the correct usage to stderr */
    fprintf(stderr, "usage: lab1 [no options...]\n");
    exit(1);
}

void create_data(char *file, int numberBytes)
{
    /* Purpose: To modify a given char array to contain a repeating alphabet lowercase 'a'-'z'.
     * Inputs:  char *file      a pointer to the first element of a large char array.
     *          int numberBytes the number of bytes in this array sizeof(file).
     * Outputs: void
     */
    
    char initializer_char = 'a';
    for (int i = 0; i < numberBytes; i++) {
        file[i] = initializer_char;
        initializer_char++;
        if (initializer_char > 'z') initializer_char = 'a';
    };
}

int create_file(char filename[], int numberBytes)
{
    /* Purpose: To modify a given date string to match the file modification date of a file.
     * Inputs:  char filename[]     a C-string containing the full filename (with extension).
     *          int numberBytes     the number of bytes in this file.
     * Outputs: int                 the index of the file in the range [0, MAXFILEENTRIES) OR -1 for too many files.
     */
    for (int idx = 0; idx < MAXFILEENTRIES; idx++) {
        if (fileDirectory[idx].startAddress == 0) {
            
            /* Write the filename and extension to the file directory */
            char *mutableFilename = strdup(filename);
            char *name = strsep(&mutableFilename, ".");
            char *ext = strsep(&mutableFilename, ".");
            strncpy(fileDirectory[idx].name, name, sizeof(fileDirectory[idx].name));
            strncpy(fileDirectory[idx].ext, ext, sizeof(fileDirectory[idx].ext));
            
            /* Get the creation time and date */
            time_t rawtime;
            time(&rawtime);
            struct tm *localTime = localtime(&rawtime);
            int fileYear = localTime->tm_year - 80; // Years since 1980
            int fileMonth = localTime->tm_mon;
            int fileDay = localTime->tm_mday;
            int fileHour = localTime->tm_hour;
            int fileMin = localTime->tm_min;
            int fileSec = localTime->tm_sec / 2;
            
            unsigned short compressedDate = (unsigned short)fileYear;
            compressedDate = compressedDate << 4;
            compressedDate = compressedDate | (unsigned short)fileMonth;
            compressedDate = compressedDate << 5;
            compressedDate = compressedDate | fileDay;
            fileDirectory[idx].date = compressedDate;
            
            unsigned short compressedTime = (unsigned short)fileHour;
            compressedTime = compressedTime << 6;
            compressedTime = compressedTime | fileMin;
            compressedTime = compressedTime << 5;
            compressedTime = compressedTime | fileSec;
            fileDirectory[idx].time = compressedTime;
            
            fileDirectory[idx].fileSize = numberBytes;
            
            return idx;
        }
    }
    
    /* Failed to create file */
    fprintf(stderr, "Exceeded the maximum number of files!\n");
    return -1;
}

void delete_file(char filename[])
{
    /* Purpose: To delete a file.
     * Inputs:  char filename[] a C-string containing the full filename (with extension).
     * Outputs: void
     */
    
    /* Traverse the linked list and set all nodes back to 0x0001 */
    int fileIndex = getFileIndex(filename);
    struct node *this_node = &fat[fileDirectory[fileIndex].startAddress];
    while (this_node->clusterAddress != END_OF_FILE) {
        this_node->clusterAddress = 0x0001;
        this_node = this_node->next;
    }
    this_node->clusterAddress = 0x0001; // Finally, remove the EOF
    
    /* Alert the user */
    fprintf(stdout, "Deleting file '%s.%s'\n\n", fileDirectory[fileIndex].name, fileDirectory[fileIndex].ext);
    
    /* Remove the entry in the fileDirectory */
    fileDirectory[fileIndex] = emptyFile;
}

void write_file(char filename[], int numberBytesRemaining, char *data)
{
    /* Purpose: To write a file to disk and update the FAT.
     * Inputs:  char filename[]             a C-string containing the full filename (with extension).
     *          int numberBytesRemaining    the number of bytes in the file remaining to write.
     *          char *data                  a pointer to the first element of the actual data to be written.
     * Outputs: void
     */
    
    /* Use this index to keep track of our position in the file */
    int data_idx = 0;
    
    /* Determine if we need to create the fileDirectory entry */
    int fileIndex = getFileIndex(filename);
    if (fileIndex < 0) fileIndex = create_file(filename, numberBytesRemaining);
    
    /* Find the first available cluster addresses */
    unsigned short fat_idx;
    for (fat_idx = 0x0002; fat_idx <= 0xFFFF; fat_idx++) {
        if (fat[fat_idx].clusterAddress <= 0x0001) {
            fileDirectory[fileIndex].startAddress = fat_idx;
            break;
        }
        
        /* If no suitable cluster is found */
        if (fat_idx >= 0xFFFF) {
            fprintf(stderr, "-- Failed to write file, disk is full! --\n");
            exit(1);
        }
    }
    
    /* While there is still data to be written */
    struct node *this_node = &fat[fat_idx];
    while (numberBytesRemaining > 0) {
        
        /* Find the next available cluster */
        unsigned short next_idx;
        for (next_idx = fat_idx + 1; next_idx < 0xFFFF; next_idx++) {
            if (fat[next_idx].clusterAddress <= 0x0001) break;
        }
        this_node->clusterAddress = fat_idx;
        this_node->next = &fat[next_idx];
        
        /* Write 512 bytes of data to this cluster (appending EOF and zeros if necessary) */
        for (int j = data_idx; j < data_idx + 512; j++) {
            disk[this_node->clusterAddress].disk_data[j - data_idx] = data[j];
        }
        printf("Simulated writing of file %s.%s to cluster: %d\n",
               fileDirectory[fileIndex].name, fileDirectory[fileIndex].ext, fat_idx);
        numberBytesRemaining -= 512;
        
        /* Move to the next node and check for EOF */
        this_node = this_node->next;
        fat_idx = next_idx;
        if (numberBytesRemaining < 0) numberBytesRemaining = 0;
    }
    
    /* Write the END_OF_FILE cluster */
    this_node->clusterAddress = END_OF_FILE;
    printf("Simulated writing of file %s.%s to cluster: %d EOF\n\n",
           fileDirectory[fileIndex].name, fileDirectory[fileIndex].ext, fat_idx);
}

void read_file(char filename[])
{
    /* Purpose: To read the contents of a file to stdout.
     * Inputs:  char filename[] a C-string containing the full filename (with extension).
     * Outputs: void
     */
    
    int fileIndex = getFileIndex(filename);
    if (fileIndex >= 0) {
        
        /* Read the file contents */
        int address = fileDirectory[fileIndex].startAddress;
        while (address != END_OF_FILE) {
            for (int j = 0; j < 512; j++) {
                fprintf(stdout, "%c", disk[address].disk_data[j]);
            }
            fprintf(stdout, "\n^^^ Cluster #%d\n", address);
            address = fat[address].next->clusterAddress;
        }
    } else {
        fprintf(stdout, "\n-- File not found --\n\n");
    }
}

void printClusters(char filename[])
{
    /* Purpose: To traverse the FAT linked list and display all clusters for a file on stdout.
     * Inputs:  char filename[]             a C-string containing the full filename (with extension).
     * Outputs: void
     */
    
    /* If found in the fileDirectory, print every cluster address */
    int fileIndex = -1;
    fileIndex = getFileIndex(filename);
    if (fileIndex >= 0) {
        int address = fileDirectory[fileIndex].startAddress;
        while (address != END_OF_FILE) {
            fprintf(stdout, "Printing cluster address for file '%s.%s': %d\n",
                    fileDirectory[fileIndex].name, fileDirectory[fileIndex].ext, address);
            address = fat[address].next->clusterAddress;
        }
        if (address == END_OF_FILE) {
            fprintf(stdout, "Printing cluster address for file '%s.%s': EOF\n\n",
                    fileDirectory[fileIndex].name, fileDirectory[fileIndex].ext);
        } else {
            fprintf(stderr, "ERROR: FAT missing EOF for file '%s.%s'\n",
                    fileDirectory[fileIndex].name, fileDirectory[fileIndex].ext);
        }
    }
    else {
        fprintf(stdout, "-- File not found --\n\n");
    }
}

void printFileMetadata(char filename[])
{
    /* Purpose: To write a file's metadata stored in the fileDirectory to stdout.
     * Inputs:  char filename[] a C-string containing the full filename (with extension).
     * Outputs: void
     */
    
    int fileIndex = -1;
    fileIndex = getFileIndex(filename);
    if (fileIndex >= 0) {
        char dateString[50];
        getDateString(filename, dateString, sizeof(dateString));
        fprintf(stdout, "| %s.%s |", fileDirectory[fileIndex].name, fileDirectory[fileIndex].ext);
        fprintf(stdout, " %s |", dateString);
        fprintf(stdout, " %d bytes |", fileDirectory[fileIndex].fileSize);
        fprintf(stdout, " %d |\n", fileDirectory[fileIndex].startAddress);
    }
    else {
        fprintf(stdout, "-- File not found --\n");
    }
}
