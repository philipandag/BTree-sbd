#include <string>
#include <iostream>

using std::string;
using std::cout;
using std::to_string;
using std::cin;

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
    
    bool dirty = true;

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

    const int NODE_BUFFER_SIZE = 10;
    BNode** nodeBuffer; //nodeBuffer[0] is always the root
    int nodeBufferTop = 0;
    //BNode root;
    //BNode nodebuffer;
    //BNode* nodeptr;
    int bufferTop = 0;

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
        return nodeBuffer[nodeBufferTop];
    }
    BNode* getRoot(){
        return nodeBuffer[0];
    }

    Btree(string path, string mode = "r+b")
    :   filePath(path),
        fileMode(mode)
    {
        open();
        
        fseek(file, 0, SEEK_SET);
        fseek(index, 0, SEEK_SET);

        nodeBuffer = (BNode**) malloc(sizeof(BNode*) * NODE_BUFFER_SIZE);
        for(int i = 0; i < NODE_BUFFER_SIZE; i++)
        {
            nodeBuffer[i] = (BNode*) malloc(sizeof(BNode));
        }

        // creating root if file doesnt contain it
        if(fread(nodeBuffer[0], sizeof(BNode), 1, index) != 1)
        {
            nodeBuffer[0]->myAddress = 0;
            nodeBuffer[0]->parentAddress = -1;
            nodeBuffer[0]->recordsFilled = 0;
            for(int i = 0; i < NODE_SIZE; i++)
            {
                nodeBuffer[0]->childrenAddress[i] = -1;
                nodeBuffer[0]->records[i] = {-1, -1};
            }
            nodeBuffer[0]->childrenAddress[NODE_SIZE] = -1;

            fseek(index, 0, SEEK_SET);
            if(fwrite(nodeBuffer, sizeof(BNode), 1, index)!=1)
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
        for(int i = 0; i < nodeBufferTop; i++)
            if(indexAddress == nodeBuffer[i]->myAddress)
                return true;

        return false;
    }
        
    bool dataInBuffer(int recordAddress)
    {
        return (recordAddress >= loadedBlockNumber * BUFFER_SIZE) && (recordAddress < (loadedBlockNumber+1) * BUFFER_SIZE);
    }

    BNode* topNode()
    {
        return nodeBuffer[nodeBufferTop];
    }

    BNode* topNodeParent()
    {
        if(nodeBufferTop == 0)
            return nullptr;
        return nodeBuffer[nodeBufferTop-1];
    }

    int findRecord(int key)
    {
        open();

        for(int i = 0; i < topNode()->recordsFilled; i++)
        {
            if(topNode()->records[i].key == key) // found, return
            { 
                return topNode()->records[i].address;
            }
            else if(topNode()->records[i].key > key)// is smaller than some record
            {
                if(pushNodeChild(i)) // true if child exists and was pushed to the buffer
                {
                    int ret = findRecord(key);
                    if(ret != -1)
                    {
                        close();
                        return ret; // found in the child
                    }
                    return -1;
                    //popNode(); // TODO
                }
            }
        }

        // key is bigger than anything in this node, check in the last child
        if(pushNodeChild(topNode()->recordsFilled))
        {
            int ret = findRecord(key);
            if(ret != -1)
            {
                close();
                return ret; // found in the child
            }
            //popNode(); // TODO
        }

        close();
        return -1;
    }

    void flushNode(BNode* node)
    {
        if(!node->dirty)
            return;
        open();
        if(index){
            fseek(index, node->myAddress * sizeof(BNode), SEEK_SET);
            node->dirty = false;
            if(fwrite(node, sizeof(BNode), 1, index)!=1)
            {
                cout << "Error flushNode could not write to index\n";
            }

            writeDiskOperations++;
        }
        close();
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
        int recordsRead = fread(node, sizeof(BNode), 1, index);
        close();

        if(recordsRead == 0)
        {
            cout << "ERROR, Btree.loadNode nothing read!";
            free(node);
            return nullptr;
        }
        readDiskOperations++;

        return node;
    }

    bool pushNodeChild(int index)
    {
        if(nodeBuffer[nodeBufferTop]->childrenAddress[index] != -1)
        {
            nodeBuffer[nodeBufferTop+1] = readNode(nodeBuffer[nodeBufferTop]->childrenAddress[index]);
            nodeBufferTop++;
            return true;
        }
        return false;
    }

    void popAll()
    {
        for(int i = nodeBufferTop; i > 0; i--)
        {
            popNode();
        }
    }

    bool popNode()
    {
        if(nodeBufferTop > 0)
        {
            flushNode(nodeBuffer[nodeBufferTop]);
            free(nodeBuffer[nodeBufferTop]);
            nodeBuffer[nodeBufferTop] = nullptr;
            nodeBufferTop--;
            return true;
        }
        return false;
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
        popAll();
        int address = dataAppendRecord(record);
        BRecord brecord = BRecord(address, record.key);

        int a = findRecord(record.key); // topNode is the one where the record should be 
        if(a != -1)
        {
            cout << "ALREADY EXISTS!";
            cout << record.toString();
            return;
        }

        // after calling find record the node where the record should be put
        // is loaded
        topAddRecord(brecord);     
    }

    void topAddRecord(BRecord record, int childAddress = -1)
    {    
        if(topNode()->recordsFilled < NODE_SIZE)
        {
            for(int i = 0; i < NODE_SIZE; i++)
            {
                // there are no records or it is the biggest record so far
                if(topNode()->records[i].empty() && topNode()->childrenAddress[i] == -1)
                {
                    topNode()->records[i] = record;
                    topNode()->childrenAddress[i] = childAddress;
                    topNode()->recordsFilled++;
                    topNode()->dirty = true;
                    return;
                }

                // if the node is not full and there is a larger key
                // insert to this node by making place for the new record
                if(topNode()->records[i].key > record.key && topNode()->childEmpty(i)) 
                {
                    for(int j = NODE_SIZE-1; j > i; j--)// shift records one place to the right to make index 'i' empty 
                    {
                        topNode()->records[j] = topNode()->records[j-1];
                        topNode()->childrenAddress[j] = topNode()->childrenAddress[j-1];
                    }
                    topNode()->records[i] = record;
                    topNode()->childrenAddress[i] = childAddress;
                    topNode()->recordsFilled++;
                    topNode()->dirty = true;
                    return;
                }
            }
            
        }
        else if(topNode())

        // // node overflow, try compensation
        // bool compensated = false;

        // if(topNodeParent())
        // {
        //     int childNumber = -1;
        //     for(int i = 0; i < NODE_SIZE+1; i++)
        //     {
        //         if(topNodeParent()->childrenAddress[i] == topNode()->myAddress)
        //         {
        //             childNumber = i;
        //             break;   
        //         }
        //     }

        //     if(childNumber == -1)
        //     {
        //         cout << "Compensation, parent doesnt know me!\n";
        //         cin.get();
        //         cin.get();
        //         return;
        //     }

        //     BNode* leftSibling = nullptr;
        //     if(childNumber > 0)
        //         leftSibling = readNode(topNodeParent()->childrenAddress[childNumber-1]);
        //     if(leftSibling)
        //     {
        //         if(leftSibling->recordsFilled < NODE_SIZE) // it is not full
        //         {
        //             // reading all records to an array for distribution
        //             int totalRecords = topNode()->recordsFilled + leftSibling->recordsFilled + 2; // + record in parent + record to insert
        //             BRecord* records = (BRecord*) malloc(sizeof(BRecord) * totalRecords);

        //             for(int i = 0; i < leftSibling->recordsFilled; i++) // left sibling has smaller record keys
        //             {
        //                 records[topNode()->recordsFilled + i] = leftSibling->records[i];
        //             }

        //             records[topNode()->recordsFilled] = topNodeParent()->records[childNumber-1]; // record in parent is in between records of left sibling and nodeptr
        //             // parent's children grouped by index in arrays:
        //             // (* R)        (* R)        (* R)       (* R)
        //             //               ^left        ^nodeptr    ^right
        //             //               sibling                  sibling
        //             // the record between nodeptr and left sibling is in parent's array on the same index as left sibling
        //             // the record between nodeptr and right sibling would be on the same index as nodeptr

        //             for(int i = 0; i < topNode()->recordsFilled; i++) // nodeptr has bigger keys than left sibling and the record in parent
        //                 records[i] = topNode()->records[i];

        //             // records should be sorted from lowest to highest key
        //             // now insert our record
        //             for(int i = 0; i < totalRecords; i++)
        //             {
        //                 if(records[i].key > r.key)
        //                 {
        //                     for(int j = totalRecords; j > i; j--) // shift all bigger records to the right
        //                         records[j] = records[j-1];
                            
        //                     int address = dataAppendRecord(r);
        //                     records[i] = {r.key, address};
        //                     break;
        //                 }
        //             }

        //             //distribute records
        //             for(int i = 0; i < totalRecords/2; i++)
        //             {
        //                 leftSibling->records[i] = records[i];
        //             }
        //             topNodeParent()->records[totalRecords/2] = records[totalRecords/2];
        //             for(int i = totalRecords/2+1; i < totalRecords; i++)
        //             {
        //                 topNode()->records[i] = records[i];
        //             }
        //             free(records);

        //             flushNode(leftSibling);

        //         }
        //         free(leftSibling);
        //         compensated = true;
        //     }

        //     BNode* rightSibling = nullptr;
        //     if(!compensated && childNumber < NODE_SIZE)
        //         rightSibling = readNode(topNodeParent()->childrenAddress[childNumber+1]);
        //     if(rightSibling)
        //     {
        //         if(rightSibling->recordsFilled < NODE_SIZE) // it is not full
        //         {
        //             // reading all records to an array for distribution
        //             int totalRecords = nodeptr->recordsFilled + rightSibling->recordsFilled + 2; // + record in parent + record to insert
        //             BRecord* records = (BRecord*) malloc(sizeof(BRecord) * totalRecords);

        //             for(int i = 0; i < nodeptr->recordsFilled; i++) // nodeptr has lower keys
        //                 records[i] = nodeptr->records[i];

        //             records[nodeptr->recordsFilled] = parent->records[childNumber]; // record in parent is in between records of nodeptr and right sibling
        //             // parent's children grouped by index in arrays:
        //             // (* R)        (* R)        (* R)       (* R)
        //             //               ^left        ^nodeptr    ^right
        //             //               sibling                  sibling
        //             // the record between nodeptr and left sibling is in parent's array on the same index as left sibling
        //             // the record between nodeptr and right sibling would be on the same index as nodeptr

        //             for(int i = 0; i < rightSibling->recordsFilled; i++) // right sibling has higher record keys
        //             {
        //                 records[nodeptr->recordsFilled + i] = rightSibling->records[i];
        //             }

        //             // records should be sorted from lowest to highest key 
        //             // now insert our record
        //             for(int i = 0; i < totalRecords; i++)
        //             {
        //                 if(records[i].key > r.key)
        //                 {
        //                     for(int j = totalRecords; j > i; j--) // shift all bigger records to the right
        //                         records[j] = records[j-1];
                            
        //                     int address = dataAppendRecord(r);
        //                     records[i] = {r.key, address};
        //                     break;
        //                 }
        //             }

        //             //distribute records
        //             for(int i = 0; i < totalRecords/2; i++)
        //             {
        //                 leftSibling->records[i] = records[i];
        //             }
        //             parent->records[totalRecords/2] = records[totalRecords/2];
        //             for(int i = totalRecords/2+1; i < totalRecords; i++)
        //             {
        //                 nodeptr->records[i] = records[i];
        //             }
        //             free(records);

        //             flushNode(parent);
        //             flushNode(nodeptr);
        //             flushNode(leftSibling);

        //         }
        //         free(leftSibling);
        //         compensated = true;
        //     }
            
        //     if(rightSibling)
        //         free(rightSibling);
        //     if(parent)
        //         free(parent);
        // }
        // // compensation ended

        // if(compensated)
        // {
        //     return;
        // }

        splitNodeAdd(record, childAddress);

        popNode(); 
    }

    //topNode() is the left split after this
    void splitNodeAdd(BRecord r, int childAddress)
    {
        int totalRecords = NODE_SIZE;

        BRecord* records = (BRecord*) malloc(sizeof(BRecord) * (NODE_SIZE+1));
        int* childrenAddresses = (int*) malloc(sizeof(int) * (NODE_SIZE+1));

        for(int i = 0; i < NODE_SIZE; i++) // copy all records and children from the node
        {
            records[i] = topNode()->records[i];
            childrenAddresses[i] = topNode()->childrenAddress[i];
        }
        //insert new record
        bool inserted = false;
        for(int i = 0; i < NODE_SIZE; i++)
        {
            if(r.key < records[i].key)
            {
                for(int j = NODE_SIZE; j > i; j--)
                {
                    records[j] = records[j-1];
                    childrenAddresses[j] = childrenAddresses[j-1];
                }
                records[i] = r;
                childrenAddresses[i] = childAddress;
                inserted = true;
                break;
            }
        }
        if(!inserted) // should just go on the end
        {
            records[NODE_SIZE] = r;
            childrenAddresses[NODE_SIZE] = childAddress;
        }
        
        BNode* leftSplit = new BNode();

        // node will be reused as right split
        // if node is root then the node has to remain root so right split will
        // be created
        BNode* rightSplit;
        if(topNode() == getRoot())
            rightSplit = new BNode();
        else
            rightSplit = topNode();

        


        // distribute records among the new children
        for(int i = 0; i < totalRecords/2; i++)
        {
            leftSplit->records[i] = records[i];
            leftSplit->childrenAddress[i] = childrenAddresses[i];

            rightSplit->records[i] = records[totalRecords/2 + i + 1];
            rightSplit->childrenAddress[i] = childrenAddresses[totalRecords/2 + i + 1];
        }
        for(int i = totalRecords/2; i < totalRecords; i++)
        {
            leftSplit->records[i] = BRecord();
            leftSplit->childrenAddress[i] = -1;

            rightSplit->records[i] = BRecord(); // makes sure to clean up records which should not be there, but could be left from reusing the node
            rightSplit->childrenAddress[i] = -1;
        }

        leftSplit->recordsFilled = totalRecords/2;
        rightSplit->recordsFilled = totalRecords/2;
        leftSplit->dirty = true;
        rightSplit->dirty = true;       

        BRecord middle = records[totalRecords/2];
        int childMiddle = childrenAddresses[totalRecords/2];

        // record at totalRecords/2 was not moved, it will be the record inserted into parent
        leftSplit->childrenAddress[totalRecords/2] = childMiddle; // copy the child before middle address
        rightSplit->childrenAddress[totalRecords/2] = childrenAddresses[totalRecords-1]; // copy the last child address

        
        free(records);
        free(childrenAddresses);
        
        if(topNode() == getRoot())
        {
            clearNode(topNode()); // clear new root as its contents were moved to its new children
            topNode()->childrenAddress[1] = numberOfNodes; // the addressess left and right split will get when appended
            topNode()->childrenAddress[0] = numberOfNodes+1; // first the right split gets appended, so the order is right-to-left
            topNode()->records[0] = middle;
            topNode()->recordsFilled = 1;
            topNode()->dirty = true;
            leftSplit->parentAddress = topNode()->myAddress;
            rightSplit->parentAddress = topNode()->myAddress;

            flushRoot();

            indexAppendNode(rightSplit);
            delete rightSplit;
        }
        else if(topNodeParent())
        {
            leftSplit->parentAddress = topNodeParent()->myAddress;
        }

        indexAppendNode(leftSplit);

        delete leftSplit;

        if(topNode() != getRoot()) // if it was root then middle is already placed and there is no need of further propagation
        {
            popNode();
            topAddRecord(middle, leftSplit->myAddress);
        }
        
    }

    void clearNode(BNode* node)
    {
        for(int i = 0; i < NODE_SIZE; i++)
        {
            node->records[i] = BRecord();
            node->childrenAddress[i] = -1;
        }
        node->childrenAddress[NODE_SIZE] = -1;
        node->recordsFilled = 0;
    }


    void addChild(int i)
    {
        if(topNode()->childrenAddress[i] != -1) 
        {
            cout << "Child already exists!";
            return;
        }

        BNode child;
        child.parentAddress = topNode()->myAddress;

        indexAppendNode(&child);
        topNode()->childrenAddress[i] = child.myAddress;
    }

    int dataBlockAddress(int recordAddress)
    {
        return recordAddress / BUFFER_SIZE;
    }

    // returns the number of the inserted node in the file
    // sets myAddress in the node to the address in file where it is put
    void indexAppendNode(BNode* n)
    {
        n->dirty = false; // will be clean because it will be saved 
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

    void flushRoot()
    {
        flushNode(nodeBuffer[0]);
    }

    ~Btree()
    {
        cout << "Tree destructor, this = " << this << "\n";
        popAll();
        close();
    }

    void printContents()
    {
        cout << "\n\n#######   BTree Contents:\n";
        BNode node;

        popAll();
        //flushCurrentNode();
        flushRoot();


        cout << "R: ";// << root.toString();
        int i = 0;
        open();
        fseek(index, 0, SEEK_SET);
        while(BNode* node = readNode(i++))
        {
            cout << node->toString() << "\n";
            free(node);
            node = nullptr;
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