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

    bool verbose = false;

    enum FileCreateMode{
        RANDOM,
        MANUAL,
        ORDERED,
        REVERSED,
        ENUM_LENGTH
    } fileCreateMode = FileCreateMode::RANDOM;

    string fileModeToString(FileCreateMode mode);

    string fileToCreate = "new_database.db";
    int fileToCreateRecords = 1024;



    void mainMenu();
    void createFile();
    void doCreateFile();
    void interactive();
    void load();
    void processCommands();
public:
    App();
    ~App();
    void mainLoop();
};