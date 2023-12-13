#pragma once

#include <string>
#include <iostream>
#include <math.h>
#include <cmath>
using std::string;
using std::to_string;
using std::cout;

const int RECORD_LENGTH = 5;
const int BUFFER_SIZE = 5;


struct Record
{
    int fields[RECORD_LENGTH];
    string toString();
    float key();
};

/*
"r"     Opens a file for reading. The file must exist.
"w"     Creates an empty file for writing. If a file with the same name already exists, its content is erased and the file is considered as a new empty file.
"a"     Appends to a file. Writing operations, append data at the end of the file. The file is created if it does not exist.
"r+"    Opens a file to update both reading and writing. The file must exist.
"w+"    Creates an empty file for both reading and writing.
"a+"    Opens a file for reading and appending.
*/

class DataFile
{
protected:
    FILE* file;
    string filePath;
    int fileCursor = 0;


    int bufferBlockNumber = 0;
    Record buffer[BUFFER_SIZE];
    int recordsFilled = 0;
    int totalRuns = 0;
    int runsLeftToRead = 0;
    float lastKey = INFINITY;

    int readDiskOperations = 0;
    int writeDiskOperations = 0;

    bool reachedEndOfFile = false;
    bool deleteFile = false;
public:
    int runs = 0;


    DataFile(string path, string mode, string name = "");

protected:
    // loads the buffer with up to BUFFER_SIZE records from file starting at fileCursor
    void loadBlock(int i);
    void appendBlock();
    void clearBlock();
    void insertBlock(int i);

public:

    bool runsJustIncreased = false;
    // returns next record, if theres no more records returns nullptr
    Record* getRecord(int i);
    void appendRecord(Record r);
    void insertRecord(Record r, int i);
    void flushBuffer();

    void printContents();

    void rewindFile();
    virtual void clearFile();

    int countRecords();
    int countBlocks();

    void setDeleteFile(bool del);

    int getWrites();
    int getReads();
    int getDiskOperations();

    void renameFile(string name);

    bool recordInBuffer(int recordNumber);

    ~DataFile();
};