#include "File.h"


string Record::toString()
{
    string out = "";
    out += "Record: {";
    for(int i = 0; i < RECORD_LENGTH; i++){
        out += to_string(fields[i]) + ", ";
    }
    out += "\b\b}, (avg=" + to_string(key()) + ")";
    return out;
}

float Record::key()
{
    int sum = 0;
    for(int i =0; i < RECORD_LENGTH; i++){
        sum += fields[i];
    }
    return (float)sum/RECORD_LENGTH;
}


DataFile::DataDataFile(string path, string mode, string name)
    :
    DataFile(fopen(path.c_str(), mode.c_str())),
    DataFilePath(path),
    DataFileMode(mode),
    friendlyName(name)
{
    if(friendlyName == "")
        friendlyName = path;
}

void DataFile::loadNextBlock()
{
    if(DataFile == nullptr)
    {
        cout << "PANIC! File is nullptr!\n";
    }
    fseek(DataFile, DataFileCursor, SEEK_SET);
    int recordsRead = fread(&buffer, sizeof(Record), BUFFER_SIZE, DataFile);

    Record r;

    // this fread is not a part of simulation, if the last block is read and it is full, then we need to know
    // it is actually the last block, because it means that when the last record of this block is read the number
    // of runs decreases and that information is needed for visualisation
    if(recordsRead != BUFFER_SIZE || fread(&r, sizeof(Record), 1, DataFile) == 0) 
        reachedEndOfDataFile = true;
    

    readDiskOperations++;
    DataFileCursor += recordsRead * sizeof(Record);
    recordsFilled = recordsRead;
    bufferCursor = 0;
    if(recordsFilled == 0){
        runsLeftToRead = 0;
    }
}

void DataFile::appendBlock()
{
    fseek(DataFile, DataFileCursor, SEEK_SET); // move DataFile cursor to the end of DataFile

    int recordsWritten = fwrite(&buffer, sizeof(Record), recordsFilled, DataFile);

    if(recordsWritten == 0)
        return;

    writeDiskOperations++;
    DataFileCursor += recordsWritten * sizeof(Record);
    bufferCursor = 0;
    recordsFilled = 0;
}

Record* DataFile::getRecord(int i)
{
    if(recordInBuffer(i))
    {
        return &buffer[i/BUFFER_SIZE];
    }
    if()
    {
        if(!reachedEndOfFile) // reachedEndOfDataFile is set if the last time reading a block it was the last one
            loadNextBlock();
        else
        {
            runsLeftToRead = 0;
            return nullptr;
        }
    }

    Record* ret = &buffer[bufferCursor];

    return ret;
}

bool DataFile::recordInBuffer(int recordNumber);
{
    return (recordNumber >= bufferBlockNumber * BUFFER_SIZE) && (recordNumber < (bufferBlockNumber+1) * BUFFER_SIZE);
}

void DataFile::appendRecord(Record r)
{
    float k = r.key();
    if(lastKey==INFINITY || k < lastKey)
    {
        totalRuns++;
        runsJustIncreased = true;
    }
    else
    {
        runsJustIncreased = false;
    }
    lastKey = k;

    buffer[bufferCursor++] = r;
    recordsFilled++;
    if(bufferCursor == BUFFER_SIZE)
        appendBlock();
}

void DataFile::flushBuffer()
{
    if(recordsFilled > 0)
        appendBlock();
}

void DataFile::printContents()
{
    float prevKey = INFINITY;
    fseek(file, 0, SEEK_SET); // move DataFile cursor to the beginning
    int runs = 0;

    Record r;

    while(fread(&r, sizeof(Record), 1, file))
    {
        if(r.key() < prevKey)
            {
                cout << "------\n";
                runs++;
            }
        prevKey = r.key();
        cout << r.toString() << "\n";
    }
    cout << runs << " runs\n";
    cout << countRecords() << " records\n";
}


void DataFile::rewindFile()
{
    fileCursor = 0;
    bufferCursor = 0;
    recordsFilled = 0;
    reachedEndOfFile = false;
    lastKey = INFINITY;
}

void DataFile::clearFile()
{
    file = fopen(filePath.c_str(), fileMode.c_str());
    totalRuns = 0;
    rewindFile();
}

int DataFile::countRecords()
{
    fseek(file, 0, SEEK_SET); // move DataFile cursor to the beginning
    Record r;
    int count = 0;
    while(fread(&r, sizeof(Record), 1, file))
        count++;
    return count;
}

int DataFile::countBlocks()
{
    fseek(file, 0, SEEK_SET); // move file cursor to the beginning
    Record r;
    int count = 0;
    while(fread(&r, sizeof(Record), RECORD_LENGTH, file))
        count++;
    return count;
}

void DataFile::setDeleteFile(bool del){
    deleteFile = del;
}

int DataFile::getWrites(){
    return writeDiskOperations;
}

int DataFile::getReads(){
    return readDiskOperations;
}

int DataFile::getDiskOperations(){
    return readDiskOperations + writeDiskOperations;
}

void DataFile::renameFile(string name){
    rename(filePath.c_str(), name.c_str());
    filePath = name;
}

DataFile::~DataFile(){
    if(deleteFile)
    {
        remove(filePath.c_str());
    }
    fclose(file);
}
