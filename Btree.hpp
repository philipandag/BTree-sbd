#include <string>
#include <iostream>

using std::string;
using std::cout;
using std::to_string;

const int RECORD_LENGTH = 5;
const int BUFFER_SIZE = 5;
const int D = 1;
const int NODE_SIZE = 2*D;

struct Record
{
    int fields[RECORD_LENGTH];
    int key;
    string toString();
};

struct BRecord
{
    BRecord()
    :   address(-1),
        key(-1)
    {}
    BRecord(int address, int key)
    :   address(address),
        key(key)
    {}
    int address = -1;
    int key = -1;
    bool empty(){return key == -1;}
};

class BNode {
public:
    BRecord records[NODE_SIZE];
    int childrenAddress[NODE_SIZE+1];
    int parentAddress = -1;
    int myAddress = -1;
    int recordsFilled = 0;
    

    string toString() const
    {
        string ret = "BNode & " + to_string(myAddress) + "\n";
        for(int i = 0; i < 2*D; i++)
        {
            if(childrenAddress[i] == -1)
                ret += "-";
            else
                ret += "&" + to_string(childrenAddress[i]);
            ret += "\n";
            
            if(i < recordsFilled)
                ret += "{K" + to_string(records[i].key) + " & " + to_string(records[i].address) + "}";
            else
                ret += "{}";
            ret += "\n";
        }

        if(childrenAddress[NODE_SIZE] == -1)
            ret += "-";
        else
            ret += "&" + to_string(childrenAddress[NODE_SIZE]);
        ret += "\n\n";

        return ret;
    }

    BNode()
    {
        for(int i = 0; i < NODE_SIZE; i++)
        {
            childrenAddress[i] = -1;
        }
        childrenAddress[NODE_SIZE] = -1;
    }

    bool childEmpty(int i)
    {
        return childrenAddress[i] == -1;
    }
};



class Btree {
    FILE* file = nullptr;
    FILE* index = nullptr;

    BNode root;
    BNode nodebuffer;
    BNode* nodeptr;

    int numberOfNodes = 0;

    string filePath;
    string fileMode;

    int loadedBlockNumber = 0;
    Record buffer[BUFFER_SIZE];
    int bufferCursor = 0;
    int bufferRecordsFilled = 0;
    int fileCursor = 0;
    

    int readDiskOperations = 0;
    int writeDiskOperations = 0;

    bool valid = false;
public:

    Btree()
    {}

    BNode* getCurrentNode(){
        return nodeptr;
    }
    BNode* getRoot(){
        return &root;
    }

    Btree(string path, string mode = "r+b")
    :   filePath(path),
        fileMode(mode)
    {
        open();
        
        fseek(file, 0, SEEK_SET);
        fseek(index, 0, SEEK_SET);

        // creating root if file doesnt contain it
        if(fread(&root, sizeof(BNode), 1, index) != 1)
        {
            root.myAddress = 0;
            root.parentAddress = -1;
            root.recordsFilled = 0;
            for(int i = 0; i < NODE_SIZE; i++)
            {
                root.childrenAddress[i] = -1;
                root.records[i] = {-1, -1};
            }
            root.childrenAddress[NODE_SIZE] = -1;

            fseek(index, 0, SEEK_SET);
            if(fwrite(&root, sizeof(BNode), 1, index)!=1)
            {
                cout << "Error Btree constructor could not write root to index\n";
            }
            //fflush(index);
        }
        valid=true;
        close();

        fileMode = "r+b"; // after creating the file we dont want to 
                        // overwrite it
        findLastNodeAddress();
        nodeptr = &root;
        cout << "Tree constructor, this address " << this << "\n";
    }

    bool isValid(){
        return valid;
    }
    void open()
    {
        if(file == nullptr)
            file = fopen(filePath.c_str(), fileMode.c_str());
        if(index == nullptr)
            index = fopen((filePath + ".btree").c_str(), fileMode.c_str());
    }

    void close()
    {
        if(file != nullptr)
        {
            fclose(file);
            file = nullptr;
        }
        if(index != nullptr)
        {
            fclose(index);
            index = nullptr;
        }
    }

    void findLastNodeAddress()
    {
        int lastAddress = 0;
        BNode node;

        open();
        fseek(index, 0, SEEK_SET);
        while(fread(&node, sizeof(BNode), 1, index))
            lastAddress++;
        close();
        
        numberOfNodes = lastAddress;
    }
    
    bool indexInBuffer(int indexAddress)
    {
        return indexAddress == nodeptr->myAddress;
    }
        
    bool dataInBuffer(int recordAddress)
    {
        return (recordAddress >= loadedBlockNumber * BUFFER_SIZE) && (recordAddress < (loadedBlockNumber+1) * BUFFER_SIZE);
    }

    // because its using loadNode after calling this function and not finding the record
    // the loaded node should be the one where the record we are searching for should be placed
    // if we intend to insert it
    int findRecord(int key)
    {
        nodeptr = &root;
        open();

        while(true)
        {
            for(int i = 0; i < nodeptr->recordsFilled; i++)
            {
                if(nodeptr->records[i].key == key) // found, return
                { 
                    return nodeptr->records[i].address;
                }
                else if(nodeptr->records[i].key > key)// is smaller than some record
                {
                    if(!loadNode(nodeptr->childrenAddress[i]))
                    {
                        close();
                        return -1;
                    }
                    i = 0;
                }
            }

            // key is bigger than anything in this node, check in the last child
            if(!loadNode(nodeptr->childrenAddress[NODE_SIZE])) // if there is no child, return null
            {
                close();
                return -1;   
            }    
        }

        close();
        return -1;
    }

    void flushNode(BNode* node)
    {
        open();
        if(index){
            fseek(index, node->myAddress * sizeof(BNode), SEEK_SET);
            if(fwrite(node, sizeof(BNode), 1, index)!=1)
            {
                cout << "Error flushNode could not write to index\n";
            }

            writeDiskOperations++;
        }
        close();
    }

    void flushCurrentNode()
    {
        flushNode(nodeptr);
    }

    void flushRoot()
    {
        flushNode(&root);
    }

    bool loadRoot()
    {
        //flushRoot();
        nodeptr = &root;
        return true;
    }

    // careful, may need to be free'd
    BNode* readNode(int address)
    {
        if(address == -1) // trying to load null
        {
            return nullptr;
        }

        if(address >= numberOfNodes) // trying to load a node after the last node
        {
            return nullptr;
        }

        BNode* node = (BNode*)malloc(sizeof(BNode));

        open();
        fseek(index, address * sizeof(BNode), SEEK_SET);
        int recordsRead = fread(&nodebuffer, sizeof(BNode), 1, index);
        close();

        nodeptr = &nodebuffer;

        if(recordsRead == 0)
        {
            cout << "ERROR, Btree.loadNode nothing read!";
            free(node);
            return nullptr;
        }
        readDiskOperations++;

        return node;
    }

    // return true if node exists
    bool loadNode(int address)
    {
        if(address == 0)
        {
            return loadRoot();
        }
        if(address == -1) // trying to load null
        {
            return false;
        }

        if(address >= numberOfNodes) // trying to load a node after the last node
        {
            return false;
        }

        if(address != nodeptr->myAddress)
        {
            flushCurrentNode();

            open();
            fseek(index, address * sizeof(BNode), SEEK_SET);
            int recordsRead = fread(&nodebuffer, sizeof(BNode), 1, index);
            close();

            nodeptr = &nodebuffer;

            if(recordsRead == 0)
            {
                cout << "ERROR, Btree.loadNode nothing read!";
                return false;
            }

            readDiskOperations++;
        }

        return true;
    }

    Record* getRecordByIndex(int number)
    {
        if(dataInBuffer(number))
        {
            return &buffer[number % BUFFER_SIZE];
        }

        if(loadBlock(number / BUFFER_SIZE))
        {
            return &buffer[number % BUFFER_SIZE];
        }

        return nullptr;
    }

    bool loadBlock(int number)
    {
        open();
        fseek(file, number * BUFFER_SIZE * sizeof(Record), SEEK_SET);
        int recordsRead = fread(&buffer, sizeof(Record), BUFFER_SIZE, file);
        close();

        if(recordsRead == 0)
        {
            cout << "ERROR, Btree.loadBlock nothing read!";
            return false;
        }

        readDiskOperations++;
        return true;
    }

    void addRecord(Record record)
    {
        loadNode(root.myAddress);

        int a = findRecord(record.key);
        if(a != -1)
        {
            cout << "ALREADY EXISTS!";
            cout << record.toString();
            return;
        }

        // after calling find record the node where the record should be put
        // is loaded
        nodeAddRecord(nodeptr->myAddress, record);     
    }   

    void nodeAddRecord(int nodeAddress, Record r, int childAddress = -1)
    {
        loadNode(nodeAddress);
        int key = r.key;

        
        if(nodeptr->recordsFilled < NODE_SIZE)
        {
            for(int i = 0; i < NODE_SIZE; i++)
            {
                // there are no records or it is the biggest record so far
                if(nodeptr->records[i].empty())
                {
                    int address = dataAppendRecord(r);
                    BRecord brecord = {address, r.key};
                    nodeptr->records[i] = brecord;
                    nodeptr->childrenAddress[i] = childAddress;
                    nodeptr->recordsFilled++;
                    return;
                }

                // if the node is not full and there is a larger key
                // insert to this node by making place for the new record
                if(nodeptr->records[i].key > key && nodeptr->childEmpty(i)) 
                {
                    for(int j = NODE_SIZE-1; j > i; j--)// shift records one place to the right to make index 'i' empty 
                    {
                        nodeptr->records[j] = nodeptr->records[j-1];
                        nodeptr->childrenAddress[j] = nodeptr->childrenAddress[j-1];
                    }
                    int address = dataAppendRecord(r);
                    BRecord brecord = {address, r.key};
                    nodeptr->records[i] = brecord;
                    nodeptr->childrenAddress[i] = childAddress;
                    nodeptr->childrenAddress[i] = -1;
                    nodeptr->recordsFilled++;
                    return;
                }
            }
            
        }

        // node overflow, try compensation
        bool compensated = false;
        if(nodeptr->parentAddress == root.myAddress)
        {
            flushRoot();
        }
        if(nodeptr->parentAddress != -1)
        {
            BNode* parent = readNode(nodeptr->parentAddress);
            if(parent == nullptr)
            {
                cout << "Compensation, failed to load parent!\n";
                cin.get();
                cin.get();
                return;
            }

            int childNumber = -1;
            for(int i = 0; i < NODE_SIZE+1; i++)
            {
                if(parent->childrenAddress[i] == nodeptr->myAddress)
                {
                    childNumber = i;
                    break;   
                }
            }
            if(childNumber == -1)
            {
                cout << "Compensation, parent doesnt know me!\n";
                cin.get();
                cin.get();
                return;
            }

            BNode* leftSibling = nullptr;
            if(childNumber > 0)
                leftSibling = readNode(parent->childrenAddress[childNumber-1]);
            if(leftSibling)
            {
                if(leftSibling->recordsFilled < NODE_SIZE) // it is not full
                {
                    // reading all records to an array for distribution
                    int totalRecords = nodeptr->recordsFilled + leftSibling->recordsFilled + 2; // + record in parent + record to insert
                    BRecord* records = (BRecord*) malloc(sizeof(BRecord) * totalRecords);

                    for(int i = 0; i < leftSibling->recordsFilled; i++) // left sibling has smaller record keys
                    {
                        records[nodeptr->recordsFilled + i] = leftSibling->records[i];
                    }

                    records[nodeptr->recordsFilled] = parent->records[childNumber-1]; // record in parent is in between records of left sibling and nodeptr
                    // parent's children grouped by index in arrays:
                    // (* R)        (* R)        (* R)       (* R)
                    //               ^left        ^nodeptr    ^right
                    //               sibling                  sibling
                    // the record between nodeptr and left sibling is in parent's array on the same index as left sibling
                    // the record between nodeptr and right sibling would be on the same index as nodeptr

                    for(int i = 0; i < nodeptr->recordsFilled; i++) // nodeptr has bigger keys than left sibling and the record in parent
                        records[i] = nodeptr->records[i];

                    // records should be sorted from lowest to highest key
                    // now insert our record
                    for(int i = 0; i < totalRecords; i++)
                    {
                        if(records[i].key > r.key)
                        {
                            for(int j = totalRecords; j > i; j--) // shift all bigger records to the right
                                records[j] = records[j-1];
                            
                            int address = dataAppendRecord(r);
                            records[i] = {r.key, address};
                            break;
                        }
                    }

                    //distribute records
                    for(int i = 0; i < totalRecords/2; i++)
                    {
                        leftSibling->records[i] = records[i];
                    }
                    parent->records[totalRecords/2] = records[totalRecords/2];
                    for(int i = totalRecords/2+1; i < totalRecords; i++)
                    {
                        nodeptr->records[i] = records[i];
                    }
                    free(records);

                    flushNode(parent);
                    flushNode(nodeptr);
                    flushNode(leftSibling);

                }
                free(leftSibling);
                compensated = true;
            }

            BNode* rightSibling = nullptr;
            if(!compensated && childNumber < NODE_SIZE)
                rightSibling = readNode(parent->childrenAddress[childNumber+1]);
            if(rightSibling)
            {
                if(rightSibling->recordsFilled < NODE_SIZE) // it is not full
                {
                    // reading all records to an array for distribution
                    int totalRecords = nodeptr->recordsFilled + rightSibling->recordsFilled + 2; // + record in parent + record to insert
                    BRecord* records = (BRecord*) malloc(sizeof(BRecord) * totalRecords);

                    for(int i = 0; i < nodeptr->recordsFilled; i++) // nodeptr has lower keys
                        records[i] = nodeptr->records[i];

                    records[nodeptr->recordsFilled] = parent->records[childNumber]; // record in parent is in between records of nodeptr and right sibling
                    // parent's children grouped by index in arrays:
                    // (* R)        (* R)        (* R)       (* R)
                    //               ^left        ^nodeptr    ^right
                    //               sibling                  sibling
                    // the record between nodeptr and left sibling is in parent's array on the same index as left sibling
                    // the record between nodeptr and right sibling would be on the same index as nodeptr

                    for(int i = 0; i < rightSibling->recordsFilled; i++) // right sibling has higher record keys
                    {
                        records[nodeptr->recordsFilled + i] = rightSibling->records[i];
                    }

                    // records should be sorted from lowest to highest key 
                    // now insert our record
                    for(int i = 0; i < totalRecords; i++)
                    {
                        if(records[i].key > r.key)
                        {
                            for(int j = totalRecords; j > i; j--) // shift all bigger records to the right
                                records[j] = records[j-1];
                            
                            int address = dataAppendRecord(r);
                            records[i] = {r.key, address};
                            break;
                        }
                    }

                    //distribute records
                    for(int i = 0; i < totalRecords/2; i++)
                    {
                        leftSibling->records[i] = records[i];
                    }
                    parent->records[totalRecords/2] = records[totalRecords/2];
                    for(int i = totalRecords/2+1; i < totalRecords; i++)
                    {
                        nodeptr->records[i] = records[i];
                    }
                    free(records);

                    flushNode(parent);
                    flushNode(nodeptr);
                    flushNode(leftSibling);

                }
                free(leftSibling);
                compensated = true;
            }
            
            if(rightSibling)
                free(rightSibling);
            if(parent)
                free(parent);
        }
        // compensation ended

        if(compensated)
        {
            return;
        }



        

        




        // look for child node
        for(int i = 0; i < NODE_SIZE; i++)
        {
            if(nodeptr->records[i].key > key)// is smaller than some existing record,
            {
                if(!loadNode(nodeptr->childrenAddress[i])) // if this child doesnt exist create it
                    addChild(nodeptr->myAddress, i);
                nodeAddRecord(nodeptr->childrenAddress[i], r);
                return;  
            }
        }

        // key is bigger than anything in this node, try to add to the last child
        if(!loadNode(nodeptr->childrenAddress[NODE_SIZE])) // if this child doesnt exist create it
            addChild(nodeptr->myAddress, NODE_SIZE);   
        nodeAddRecord(nodeptr->childrenAddress[NODE_SIZE], r);
    }

    void splitNode(int nodeAddress)
    {
        BNode* parent = nodeptr;
        BNode* node = readNode(nodeAddress);

        int totalRecords = NODE_SIZE;

        BRecord* records = (BRecord*) malloc(sizeof(BRecord) * NODE_SIZE);
        int* childrenAddresses = (int*) malloc(sizeof(int) * NODE_SIZE);

        for(int i = 0; i < NODE_SIZE; i++) // copy all records and children from the node
        {
            records[i] = node->records[i];
            childrenAddresses[i] = node->childrenAddress[i];
        }
        
        BNode* leftSplit = new BNode();
        leftSplit->parentAddress = node->myAddress;
        BNode* rightSplit = new BNode();
        rightSplit->parentAddress = node->myAddress;

        BRecord r = records[totalRecords/2];


        // distribute records among the new children
        for(int i = 0; i < totalRecords/2; i++)
        {
            leftSplit->records[i] = records[i];
            leftSplit->childrenAddress[i] = childrenAddresses[i];

            rightSplit->records[i] = records[totalRecords/2 + i + 1];
            rightSplit->childrenAddress[i] = childrenAddresses[totalRecords/2 + i + 1];
        }

        // record at totalRecords/2 was not moved, it will be the record inserted into parent
        leftSplit->childrenAddress[totalRecords/2] = childrenAddresses[totalRecords/2]; // copy the child before middle address
        rightSplit->childrenAddress[totalRecords/2] = childrenAddresses[totalRecords-1]; // copy the last child address


        
        // insert to parent
        loadNode(node->parentAddress);

        for(int i = 0; i < NODE_SIZE; i++)
        {
            if(node->records[i] > )
        }
    }

    void splitPropagateUp()

    void addChild(int nodeAddress, int i)
    {
        loadNode(nodeAddress);

        if(nodeptr->childrenAddress[i] != -1) 
        {
            cout << "Child already exists!";
            return;
        }

        BNode child;
        child.parentAddress = nodeptr->myAddress;

        indexAppendNode(&child);
        nodeptr->childrenAddress[i] = child.myAddress;
    }

    int dataBlockAddress(int recordAddress)
    {
        return recordAddress / BUFFER_SIZE;
    }

    // returns the number of the inserted node in the file
    // sets myAddress in the node to the address in file where it is put
    void indexAppendNode(BNode* n)
    {
        open();
        fseek(index, numberOfNodes * sizeof(BNode), SEEK_SET);
        BNode null;

        while(fread(&null, sizeof(BNode), 1, index) == 1) // in case numberOfNodes is not right
        {
            numberOfNodes += 1;
        }

        n->myAddress = numberOfNodes;
        fseek(index, numberOfNodes * sizeof(BNode), SEEK_SET);
        if(fwrite(n, sizeof(BNode), 1, index) != 1)
        {
            cout << "ferror: " << ferror(index) << "\n";
        }
        numberOfNodes++;
        //fflush(index);
        writeDiskOperations++;
        close();
    }

    void dataAppendBlock()
    {
        open();
        fseek(file, 0, SEEK_END); // move file cursor to the end of file
        if(fwrite(&buffer, sizeof(Record), bufferRecordsFilled, file) != (size_t)bufferRecordsFilled)
        {
            cout << "Error dataAppendBlock Did not write the buffer contents\n";
            return;
        }

        close();

        writeDiskOperations++;
        bufferCursor = 0;
        bufferRecordsFilled = 0;
    }

    // returns record number in file
    int dataAppendRecord(Record r)
    {
        int addressInBuffer = bufferCursor;
        buffer[bufferCursor++] = r;
        bufferRecordsFilled++;
        if(bufferCursor == BUFFER_SIZE)
            dataAppendBlock();
        
        return loadedBlockNumber * BUFFER_SIZE + addressInBuffer;
    }

    void dataFlushBuffer()
    {
        if(bufferRecordsFilled > 0)
            dataAppendBlock();
    }



    ~Btree()
    {
        cout << "Tree destructor, this = " << this << "\n";
        flushCurrentNode();
        close();
    }

    void printContents()
    {
        cout << "\n\n#######   BTree Contents:\n";
        BNode node;

        flushCurrentNode();
        flushRoot();


        cout << "R: ";// << root.toString();
        int i = 0;
        open();
        fseek(index, 0, SEEK_SET);
        while(loadNode(i++))
        {
            cout << nodeptr->toString() << "\n";
        }
        // while(fread(&node, sizeof(BNode), 1, index))
        // {
        //     if(node.myAddress == nodeptr->myAddress)
        //     {
        //         cout << nodeptr->toString() << "\n";
        //     }
        //     else
        //     {
        //         cout << node.toString() << "\n";
        //     }
        // }
        open();
        cout << "ferror: " << ferror(index) << "\n";
        cout << "feof: " << feof(index) << "\n";
        close();
    }
    
    int getReads(){
        return readDiskOperations;
    }
    int getWrites(){
        return writeDiskOperations;
    }

};