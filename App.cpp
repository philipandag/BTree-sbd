#include "App.hpp"

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
    //system("clear");
    cout << "\n----- Main Menu -----\n" 
    << "f - loaded file (" << filePath << ")\n"
    << "c - create file\n"
    << "s - print file\n"
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
            tree = new Btree(fileToCreate);               
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
    else if(input == "s")
    {
        if(!tree->isValid())
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
    //system("clear");
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

void addRecordAndComment(Btree* tree, Record* record)
{
    int reads = tree->getReads();
    int writes = tree->getWrites();
    tree->addRecord(*record);
    tree->printContents();
    cout << "Reads: " << tree->getReads() - reads << " Writes: " << tree->getWrites() - writes << "\n";
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
                addRecordAndComment(tree, &r);
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
                addRecordAndComment(tree, &r);
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
                cin >> k;
                int sum = 0;
                for(int j = 0; j < RECORD_LENGTH; j++)
                {
                    r.fields[j] = k;
                    sum += k;
                }
                r.key = sum / RECORD_LENGTH;
                addRecordAndComment(tree, &r);
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
                addRecordAndComment(tree, &r);
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
        << "r - return to root node"
        << "3 - move to the third child of current node"
        << "p - move to parent node"
        << "s - show node contents"
        ;
        ;

    cout << "Altering the tree syntax: \noperator value\n"
        << "possible operators:\n"
        << "\t\"+\" - add record\n"
        << "\t\"-\" - remove record\n"
        << "\t\".\" - modify record\n"
        << "possible values:\n"
        << "\t{1, 2, 3, 4, 5} - record contents"
        << "\t6124 - record sequential number"
        ;
}

void App::interactive()
{   
    if(!tree->isValid())
    {
        cout << "No file loaded!\n";
        cin.get();
        cin.get();
        state = AppState::MAIN_MENU;
    }
    else
    {        
        BNode* currentNode = tree->getCurrentNode();
        string nodeString;
        if(currentNode == nullptr)
            nodeString = "current node is null";
        else
            nodeString = currentNode->toString();

        //system("clear");
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
            }
        }
        else if(input == "r")
        {
           tree->popAll();
        }
        else if(input == "q")
        {
            state = AppState::MAIN_MENU;
        }

    }

}

void App::load()
{
    //system("clear");
        cout << "\n----- Load Commands Mode -----\n" 
        << "f - file with commands (" << commandsFile << ")"
        << "p - process\n"
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
    if(input == "p")
    {
        processCommands();
    }
    else if(input == "q")
    {
        state = AppState::MAIN_MENU;
    }

    cout << "Press any key to continue...\n";
    cin.get();
    cin.get();
    state = AppState::MAIN_MENU;
}

void App::processCommands()
{

}

App::App():
    state(AppState::MAIN_MENU),
    input(""),
    filePath("")
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