#include "App.hpp"
#include <limits>

using std::cin;
using std::cout;
using std::string;



string App::fileModeToString(FileCreateMode mode)
{
    if(mode == FileCreateMode::RANDOM)
        return "Random";
    else if(mode == FileCreateMode::MANUAL)
        return "Manual";
    else if(mode == FileCreateMode::ORDERED)
        return "Ordered";
    else if(mode == FileCreateMode::REVERSED)
        return "Reversed";
    else
        return "Undefined mode";
}

void App::mainMenu()
{
    cout << "\n----- Main Menu -----\n" 
        << "f - loaded file (" << filePath << ")\n"
        << "c - create file\n"
        << "s - print file\n"
        << "r - print sorted records\n"
        << "i - interactive mode\n"
        << "l - load commands from file\n"
        << "q - quit\n"
        << "\n"
        << ":";

    cin >> input;

    if(input == "f")
    {
        cout << "new path to file: ";
        cin >> input;
        if(access(input.c_str(), F_OK) == 0)
        {
            filePath = input;
            if(tree != nullptr)
                delete tree;
            tree = new Btree(filePath);               
        }
        else{
            cout << "\nFile does not exist.\n";
            cout << "Press any key to continue...\n";
            cin.get();
            cin.get();
            return; 
        }
    }
    else if(input == "c")
    {
        state = AppState::CREATE_FILE;
    }
    else if(input == "r")
    {
        if(!tree || !tree->isValid())
        {
            cout << "No file loaded!\n";
        }
        else
        {
            cout << "Sorted records from file " << filePath << ":\n";
            tree->printSorted();
        }
        
        cout << "Press any key to continue...";
        cin.get();
        cin.get();
    }
    else if(input == "s")
    {
        if(!tree || !tree->isValid())
        {
            cout << "No file loaded!\n";
        }
        else
        {
            cout << "Contents of file " << filePath << ":\n";
            tree->printContents();
        }
        
        cout << "Press any key to continue...";
        cin.get();
        cin.get();
    }
    else if(input == "i")
    {
        state = AppState::INTERACTIVE;
    }
    else if(input == "l")
    {
        state = AppState::LOAD;
    }
    else if(input == "q")
    {
        state = AppState::QUIT;
    }
}

void App::createFile()
{
    cout << "\n----- Create File -----\n" 
        << "f - file name (" << fileToCreate << ")\n"
        << "r - length in records (" << std::to_string(fileToCreateRecords) << ")\n"
        << "m - switch mode (" << fileModeToString(fileCreateMode) << ")\n"
        << "c - create\n"
        << "q - main menu\n"
        << "\n"
        << ":";

    cin >> input;

    if(input == "f")
    {
        cout << "file name: ";
        cin >> input;
        if(access(input.c_str(), F_OK) == true)
        {
            cout << "\nFile\"" << filePath << "\" already exists. \n";
            return;                
        }
        fileToCreate = input;
    }
    else if(input == "r")
    {
        cout << "length: ";
        cin >> fileToCreateRecords;
    }
    else if(input == "m")
    {
        fileCreateMode = (FileCreateMode) ((fileCreateMode + 1) % (FileCreateMode::ENUM_LENGTH));
    }
    else if(input == "c")
    {
        doCreateFile();
        state = AppState::MAIN_MENU;
    }
    else if(input == "q")
    {
        state = AppState::MAIN_MENU;
    }
}

void App::addRecordAndComment(Record* record)
{
    int indexreads = tree->getIndexReads();
    int indexwrites = tree->getIndexWrites();
    int datareads = tree->getDataReads();
    int datawrites = tree->getDataWrites();
    if(tree->addRecord(*record) && verbose)
    {
        tree->printContents();
        cout << "Index Reads: " << tree->getIndexReads() - indexreads << " Writes: " << tree->getIndexWrites() - indexwrites << "\n";
        cout << "Data  Reads: " << tree->getDataReads() - datareads << " Writes: " << tree->getDataWrites() - datawrites << "\n";
    }
}

void App::deleteRecordAndComment(int key)
{
    int indexreads = tree->getIndexReads();
    int indexwrites = tree->getIndexWrites();
    int datareads = tree->getDataReads();
    int datawrites = tree->getDataWrites();
    if(tree->deleteRecord(key) && verbose)
    {
        tree->printContents();
        cout << "Index Reads: " << tree->getIndexReads() - indexreads << " Writes: " << tree->getIndexWrites() - indexwrites << "\n";
        cout << "Data  Reads: " << tree->getDataReads() - datareads << " Writes: " << tree->getDataWrites() - datawrites << "\n";
    }
}

void App::findRecordAndComment(int key)
{
    int indexreads = tree->getIndexReads();
    int indexwrites = tree->getIndexWrites();
    int datareads = tree->getDataReads();
    int datawrites = tree->getDataWrites();
    tree->popAll();
    int address = tree->findRecord(key);
    tree->popAll();
    if(verbose)
    {
        if(address == -1)
        {
            cout << "Record not found.\n";
        }
        else
        {
            Record r = tree->readRecord(address);
            cout << "Record found: " << r.toString() << " &" << address << ", node &" << tree->topNode()->myAddress << "\n";
        }
        cout << "Index Reads: " << tree->getIndexReads() - indexreads << " Writes: " << tree->getIndexWrites() - indexwrites << "\n";
        cout << "Data  Reads: " << tree->getDataReads() - datareads << " Writes: " << tree->getDataWrites() - datawrites << "\n";
    }
}

void App::modifyRecordAndComment(int key, Record newRecord)
{
    int indexreads = tree->getIndexReads();
    int indexwrites = tree->getIndexWrites();
    int datareads = tree->getDataReads();
    int datawrites = tree->getDataWrites();
    tree->modifyRecord(key, newRecord);
    if(verbose) 
    {
        tree->printContents();
        cout << "Index Reads: " << tree->getIndexReads() - indexreads << " Writes: " << tree->getIndexWrites() - indexwrites << "\n";
        cout << "Data  Reads: " << tree->getDataReads() - datareads << " Writes: " << tree->getDataWrites() - datawrites << "\n";
    }
}

void App::doCreateFile()
{
    if(tree != nullptr)
        delete tree;
    tree = new Btree(fileToCreate, "w+b");
    switch(fileCreateMode)
    {
        case FileCreateMode::ORDERED:{
            for(int i = 1; i <= fileToCreateRecords; i++)
            {
                Record r;
                for(int j = 0; i < RECORD_LENGTH; j++)
                {
                    r.fields[i] = i;
                }
                r.key = i;
                addRecordAndComment(&r);
            }
            tree->dataFlushBuffer();
            break;
        }
        case FileCreateMode::REVERSED:{
            for(int i = fileToCreateRecords; i >= 1; i--)
            {
                Record r;
                for(int j = 0; i < RECORD_LENGTH; j++)
                {
                    r.fields[i] = i;
                }
                r.key = i;
                addRecordAndComment(&r);
            }
            tree->dataFlushBuffer();
            break;
        }
        case FileCreateMode::MANUAL:{
            for(int i = 0; i < fileToCreateRecords; i++)
            {
                Record r;
                int k;
                cout << "Record" << i+1 << " key: ";
                while(!(cin >> k)) // TODO could check any numeric input like that
                {
                    cout << "invalid key\n"; 
                    cout << "Record" << i+1 << " key: ";
                    cin.clear();
                    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                }
                int sum = 0;
                for(int j = 0; j < RECORD_LENGTH; j++)
                {
                    r.fields[j] = k;
                    sum += k;
                }
                r.key = sum / RECORD_LENGTH;
                addRecordAndComment(&r);
            }
            tree->dataFlushBuffer();
            cout << "Done.\nPress any key to continue...\n";
            cin.get();
            cin.get();
            break;
        }
        case FileCreateMode::RANDOM:{
            for(int i = 0; i < fileToCreateRecords; i++)
            {   
                int k = rand()%100;
                Record r;
                for(int j = 0; j < RECORD_LENGTH; j++)
                {
                    r.fields[j] = k;
                }
                r.key = k;
                addRecordAndComment(&r);
            }
            tree->dataFlushBuffer();
            break;
        }
        default: {printf("Undefined file generation mode\n");}
    }

    filePath = fileToCreate;
}

void printInteractiveHelp()
{
    cout << "Traversing the tree:\n"
        << "possible commands:\n"
        << "\tr - return to root node\n"
        << "\t3 - move to the third child of current node\n"
        << "\tp - move to parent node\n"
        << "\ts - show records of current node\n"
        << "\ta - show all records sorted\n"
        << "\tt - show the whole tree\n"
        << "\tq - main menu\n"
        ;
        // commands to show records of current node, show all records sorted and show the whole tree
        


    cout << "Altering the tree:\n"
        << "possible operators:\n"
        << "\t\"+ 7\" - add record with key=7\n"        
        << "\t\"- 5\" - remove record with key=5\n"
        << "\t\"= 3 5\" - change record with key=3 to key=5\n"
        ;
}

void App::interactive()
{   
    if(!tree || !tree->isValid())
    {
        cout << "No file loaded!\nPress any key to continue...\n";
        cin.get();
        cin.get();
        state = AppState::MAIN_MENU;
    }
    else
    {        
        log = true;
        BNode* currentNode = tree->topNode();
        string nodeString;
        if(currentNode == nullptr)
            nodeString = "current node is null";
        else
            nodeString = currentNode->toString();

        cout << "\n----- Interactive Mode -----\n" 
            << "h - help\n"
            << "q - main menu\n"
            << "\n"
            << "Curent node:\n"
            << nodeString << "\n"
            << ":";
        string input;
        cin >> input;

        if(input == "h")
        {
            printInteractiveHelp();
            cout << "Press any key to continue...";
            cin.get();
            cin.get();

        }
        else if(input == "p")
        {
            if(!tree->popNode())
            {
                cout << "Current node doesnt have a parent!\n";
                if(tree->getCurrentNode() == tree->getRoot())
                {
                    cout << "The current node is the root.\n";
                }
                cout << "Press any key to continue...";
                cin.get();
                cin.get();
            }
        }
        else if(input == "s")
        {

            for(int i = 0; i < NODE_SIZE; i++)
            {
                cout << i << ": ";

                if(currentNode->records[i].address == -1)
                    cout << "-\n";
                else
                {
                    Record r = tree->readRecord(currentNode->records[i].address);
                    cout << r.toString() << "\n";
                }
            }
            cout << "Press any key to continue...";
            cin.get();
            cin.get();
        }
        else if(input == "r")
        {
           tree->popAll();
        }
        else if(input == "q")
        {
            state = AppState::MAIN_MENU;
        }
        else if(input == "t")
        {
            tree->printContents();
            cout << "Press any key to continue...";
            cin.get();
            cin.get();
            
        }
        else if(input == "a")
        {
            tree->printSorted();
            cout << "Press any key to continue...";
            cin.get();
            cin.get();
            
        }
        else if(input == "f")
        {
            tree->dataFlushBuffer();
            // print all records from data file
            FILE* file = fopen(tree->getDataPath().c_str(), "r");
            Record r;
            int i = 0;
            while(fread(&r, sizeof(Record), 1, file) != 0)
            {
                cout << i << ": " << r.toString() << "\n";
            }
            cout << "Press any key to continue...";
            cin.get();
            cin.get();


        }
        else 
        {
            const char* line = input.c_str();
            switch(line[0])
            {
                case '+':
                {
                    line += 1;
                    int key = -1;
                    try{key = std::stoi(line);}
                    catch(std::invalid_argument& e)
                    {
                        cout << "Invalid key.\n";
                        cout << "Press any key to continue...";
                        cin.get();
                        cin.get();
                        break;
                    }
                    Record r;
                    for(int i = 0; i < RECORD_LENGTH; i++)
                    {
                        r.fields[i] = key;
                    }
                    r.key = key;
                    addRecordAndComment(&r);
                    cin.get();
                    cin.get();
                    break;
                }
                case '-':
                {
                    line += 1;
                    int key = -1;
                    try{key = std::stoi(line);}
                    catch(std::invalid_argument& e)
                    {
                        cout << "Invalid key.\n";
                        cout << "Press any key to continue...";
                        cin.get();
                        cin.get();
                        break;
                    }
                    deleteRecordAndComment(key);
                    cin.get();
                    cin.get();
                    break;
                }
                case '?':
                {
                    line += 1;
                    int key = -1;
                    try{key = std::stoi(line);}
                    catch(std::invalid_argument& e)
                    {
                        cout << "Invalid key.\n";
                        cout << "Press any key to continue...";
                        cin.get();
                        cin.get();
                        break;
                    }
                    findRecordAndComment(key);
                    cin.get();
                    cin.get();
                    break;
                }
                case '=':
                {
                    line += 1;
                    int oldKey, newKey;;
                    if(scanf("%d %d", &oldKey, &newKey) != 2)
                    {
                        cout << "Invalid format.\n";
                        cout << "Press any key to continue...";
                        cin.get();
                        cin.get();
                        break;
                    }
                    Record r;
                    for(int i = 0; i < RECORD_LENGTH; i++)
                    {
                        r.fields[i] = newKey;
                    }
                    r.key = newKey;
                    modifyRecordAndComment(oldKey, r);  
                    break;
                }
                case '\n':
                case 0:
                {
                    break;
                }
                default:
                {
                    int number = -1;
                    try{number = std::stoi(line);}
                    catch(std::invalid_argument& e)
                    {
                        cout << "Invalid child number.\n";
                        cout << "Press any key to continue...";
                        cin.get();
                        cin.get();
                        break;
                    }
                    if(number >= 0 && number <= NODE_SIZE)
                    {

                        if(tree->getCurrentNode()->childrenAddress[number] != -1)
                        {
                            tree->pushNodeChild(number);
                        }
                        else
                        {
                            cout << "Child " << number << " is null.\n";
                            cout << "Press any key to continue...";
                            cin.get();
                            cin.get();
                        }
                    }
                    else
                    {
                        cout << "Invalid child number.\n";
                        cout << "Press any key to continue...";
                        cin.get();
                        cin.get();
                    }
                }
            }
        }

    }

}

void App::load()
{
    cout << "\n----- Load Commands Mode -----\n" 
        << "f - file with commands (" << commandsFile << ")\n"
        << "e - execute commands\n"
        << "l - log to file (" << log << ")\n"
        << "v - verbose (" << verbose << ")\n"
        << "q - main menu\n"
        << "\n"
        << ":";
    string input;
    cin >> input;

    if(input == "f")
    {
        cout << "path to file with commands: ";
        cin >> input;
        if(access(input.c_str(), F_OK) == 0)
        {
            commandsFile = input;               
        }
        else{
            cout << "\nFile does not exist.\n";
            cout << "Press any key to continue...\n";
            cin.get();
            cin.get();
            return; 
        }
    }
    else if(input == "l")
    {
        log = !log;
        return;
    }
    else if(input == "v")
    {
        verbose = !verbose;
        return;
    }
    else if(input == "e")
    {  
        if(!tree || !tree->isValid())
        {
            cout << "No tree loaded!\n";
            cout << "Press any key to continue...\n";
            cin.get();
            cin.get();
            return;
        }
        if(access(commandsFile.c_str(), F_OK) != 0)
        {
            cout << "Could not open file.\n";
            cout << "Press any key to continue...\n";
            cin.get();
            cin.get();
            return;
        }
        FILE* file = fopen(commandsFile.c_str(), "r");
        char command;
        int totalCommands = 0;

        // open file to store amount of disk operations in using fstream
        std::ofstream logFile;

        logFile.open("log.csv");

        logFile << "N,H,nodes,IR,IW,IB,DR,DW,DB\n";


        while(fscanf(file, "%c\n", &command) != EOF)
        {
            int indexreads = tree->getIndexReads();
            int indexwrites = tree->getIndexWrites();
            int datareads = tree->getDataReads();
            int datawrites = tree->getDataWrites();
            bool doneSomething = false;

            switch(command)
            {
                case '+':
                {
                    int key;
                    if(fscanf(file, "%d", &key) != 1)
                    {
                        cout << "Invalid format.\n";
                        cout << "Press any key to continue...";
                        cin.get();
                        cin.get();
                        break;
                    }
                    Record r;
                    for(int i = 0; i < RECORD_LENGTH; i++)
                    {
                        r.fields[i] = key;
                    }
                    r.key = key;
                    addRecordAndComment(&r);
                    tree->popAll();
                    totalCommands++;
                    doneSomething = true;
                    break;
                }
                case '-':
                {
                    int key;
                    if(fscanf(file, "%d", &key) != 1)
                    {
                        cout << "Invalid format.\n";
                        cout << "Press any key to continue...";
                        cin.get();
                        cin.get();
                        break;
                    }
                    deleteRecordAndComment(key);
                    tree->popAll();
                    totalCommands++;
                    doneSomething = true;
                    break;
                }
                case '?':
                {
                    int key;
                    if(fscanf(file, "%d", &key) != 1)
                    {
                        cout << "Invalid format.\n";
                        cout << "Press any key to continue...";
                        cin.get();
                        cin.get();
                        break;
                    }
                    findRecordAndComment(key);
                    tree->popAll();
                    totalCommands++;
                    doneSomething = true;
                    break;
                }
                case '=':
                {
                    int oldKey, newKey;
                    if(fscanf(file, "%d %d", &oldKey, &newKey) != 2)
                    {
                        cout << "Invalid format.\n";
                        cout << "Press any key to continue...";
                        cin.get();
                        cin.get();
                        break;
                    }
                    Record r;
                    for(int i = 0; i < RECORD_LENGTH; i++)
                    {
                        r.fields[i] = newKey;
                    }
                    r.key = newKey;
                    modifyRecordAndComment(oldKey, r);  
                    totalCommands++;
                    doneSomething = true;
                    break;
                }
                case 'a':
                {
                    tree->printSorted();
                    totalCommands++;
                    break;
                }
                case 't':
                {
                    tree->printContents();
                    totalCommands++;
                    break;
                }
                case 'f':
                {
                    tree->dataFlushBuffer();
                    // print all records from data file
                    FILE* file = fopen(tree->getDataPath().c_str(), "r");
                    Record r;
                    int i = 0;
                    while(fread(&r, sizeof(Record), 1, file) != 0)
                    {
                        cout << i << ": " << r.toString() << "\n";
                    }
                    totalCommands++;
                    break;
                }
                case '#':
                {
                    char comment[128];
                    fscanf(file, "%[^\n]\n", comment);
                    break;
                }
                case '\n':
                case 0:
                {
                    break;
                }
                default:
                {
                    cout << "Invalid command: " << command << "\n";
                    break;
                }
            }
            if(doneSomething && log)
            {
                logFile << tree->getNumberOfRecordsInTree() << "," << tree->getHeight() << "," << tree->getNumberOfNodesInTree() << ","
                << tree->getIndexReads() - indexreads << "," << tree->getIndexWrites() - indexwrites << "," << tree->getIndexBytes() << "," 
                << tree->getDataReads() - datareads << "," << tree->getDataWrites() - datawrites<< "," << tree->getDataBytes() << "\n";
            }
        }
        cout << "Done. Executed " << totalCommands << " commands.\n";
        cout << "Press any key to continue...\n";
        logFile.close();
        cin.get();
        cin.get();

    }   
    else if(input == "q")
    {
        state = AppState::MAIN_MENU;
    }
}


App::App(bool log):
    state(AppState::MAIN_MENU),
    input(""),
    filePath(""),
    log(log)
{}

App::~App()
{
    if(tree != nullptr){
        delete tree;
    }
}

void App::mainLoop()
{
    while(state != AppState::QUIT)
    {
        if(!dontClean)
            system("clear");
        switch(state){
            case AppState::MAIN_MENU:
            {
                mainMenu();
                break;
            }

            case AppState::CREATE_FILE:
            {
                createFile();
                break;
            }

            case AppState::INTERACTIVE:
            {
                interactive();
                break;
            }

            case AppState::LOAD:
            {
                load();
                break;
            }

            case AppState::QUIT:
            {
                break;
            }

            default: {state = AppState::MAIN_MENU;}
        }

    }
}