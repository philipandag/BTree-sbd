#pragma once

#include "Btree.hpp"
#include <iostream>
#include <unistd.h> // access() - to check if file exists
#include <stdlib.h> // system.cls()
#include <iomanip>
#include <fstream>
#include <string>

enum class AppState
{
    MAIN_MENU,
    CREATE_FILE,
    INTERACTIVE,
    LOAD,
    QUIT,
    ENUM_LENGTH
};

class App
{
    AppState state;
    string input;

    string filePath;
    string commandsFile;
    Btree* tree = nullptr;

    bool log = false;
    bool dontClean = false;
    bool verbose = true;

    enum FileCreateMode{
        RANDOM,
        MANUAL,
        ORDERED,
        REVERSED,
        ENUM_LENGTH
    } fileCreateMode = FileCreateMode::RANDOM;

    string fileModeToString(FileCreateMode mode);

    string fileToCreate = "new_database.db";
    int fileToCreateRecords = 0;



    void mainMenu();
    void createFile();
    void doCreateFile();
    void interactive();
    void load();
    void executeCommand(string command);
    void addRecordAndComment(Record* record);
    void deleteRecordAndComment(int key);
    void findRecordAndComment(int key);
    void modifyRecordAndComment(int key, Record record);
public:
    App(bool log = false);
    ~App();
    void mainLoop();
};