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
        for(int i = 0; i <= NODE_SIZE; i++)
        {
            childrenAddress[i] = -1;
        }
    }

    bool childEmpty(int i)
    {
        return childrenAddress[i] == -1;
    }

    bool isLeaf()
    {
        for(int i = 0; i <= NODE_SIZE; i++)
        {
            if(childrenAddress[i] != -1)
                return false;
        }
        return true;
    }
};


class Btree {
    FILE* file = nullptr;
    FILE* index = nullptr;

    const int NODE_BUFFER_SIZE = 1000; // unreasonable size, should be enough
    BNode** nodeBuffer; //nodeBuffer[0] is always the root
    int nodeBufferTop = 0;
    //BNode root;
    //BNode nodebuffer;
    //BNode* nodeptr;
    int bufferTop = 0;

    int numberOfNodes = 0;
    int numberOfNodesInTree = 0;

    string filePath;
    string fileMode;

    int loadedBlockNumber = 0;
    Record buffer[BUFFER_SIZE];
    int dataBlocksFilled = 0;
    int bufferRecordsFilled = 0;
    int numberOfRecords = 0;
    int numberOfRecordsInTree = 0;
    

    int indexReadDiskOperations = 0;
    int indexWriteDiskOperations = 0;
    int dataWriteDiskOperations = 0;
    int dataReadDiskOperations = 0;

    int height = 0;

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
            free(nodeBuffer[0]);
            nodeBuffer[0] = new BNode();
            nodeBuffer[0]->myAddress = 0;
            nodeBuffer[0]->dirty = true;
            flushRoot();
        }
        valid=true;
        close();

        fileMode = "r+b"; // after creating the file we dont want to 
                        // overwrite it
        findLastNodeAddress();
        findDataBlocksFilled();
        numberOfNodesInTree = 1;
        height = 1;
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

    void findDataBlocksFilled()
    {
        int count = 0;
        numberOfRecords = 0;
        Record record[BUFFER_SIZE];
        open();
        fseek(file, 0, SEEK_SET);
        while(int read = fread(record, sizeof(Record), BUFFER_SIZE, file))
        {
            count++;
            numberOfRecords += read;
        }

        // while(fread(record, sizeof(Record), 1, file))
        // {
        //     numberOfRecords++;
        // }
        close();
        dataBlocksFilled = count;
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
    bool dataLoadedInBuffer(int recordAddress)
    {
        return dataInBuffer(recordAddress) && bufferRecordsFilled >  recordAddress % BUFFER_SIZE;
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

            indexWriteDiskOperations++;
        }
        close();
    }


    // careful, may need to be free'd
    BNode* readNode(int address, bool simulate = true)
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
        if(simulate)
            indexReadDiskOperations++;

        return node;
    }

    Record readRecord(int address)
    {
        if(dataLoadedInBuffer(address))
        {
            return buffer[address % BUFFER_SIZE];
        }

        if(loadBlock(address / BUFFER_SIZE))
        {
            return buffer[address % BUFFER_SIZE];
        }

        return Record();
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
        if(number >= dataBlocksFilled)
        {
            bufferRecordsFilled = 0;
            loadedBlockNumber = number;
            return true;
        }
        open();
        fseek(file, number * BUFFER_SIZE * sizeof(Record), SEEK_SET);
        int recordsRead = fread(&buffer, sizeof(Record), BUFFER_SIZE, file);
        close();

        if(recordsRead == 0)
        {
            cout << "ERROR, Btree.loadBlock nothing read!";
            return false;
        }
        bufferRecordsFilled = recordsRead;
        dataReadDiskOperations++;
        loadedBlockNumber = number;
        return true;
    }

    bool addRecord(Record record)
    {
        popAll();

        int a = findRecord(record.key); // topNode is the one where the record should be 
        if(a != -1)
        {
            cout << "ALREADY EXISTS!";
            cout << record.toString();
            return false;
        }

        int address = dataAppendRecord(record);
        BRecord brecord = BRecord(address, record.key);

        // after calling find record the node where the record should be put
        // is loaded
        topAddRecord(brecord);     
        numberOfRecordsInTree++;
        return true;
    }

    //delete record from BTree
    bool deleteRecord(int key)
    {
        if(bufferRecordsFilled > 0)
        {
           dataFlushBuffer();
        }
        popAll();
        int address = findRecord(key);
        if(address == -1)
        {
            cout << "Record not found!\n";
            return false;
        }

        BRecord brecord = BRecord(address, key);

        // after calling find record the node where the record should be put
        // is loaded
        topDeleteRecord(brecord);     
        while(topNodeParent() && topNodeParent()->recordsFilled < NODE_SIZE/2)
        {
            popNode();
            if(compensation())
            {
                continue;
            }
            else if(mergeNode())
            {
                continue;
            }
            else if(topNode()->myAddress == 0) // root is merging
            {
                if(topNode()->recordsFilled == 0)
                {
                    BNode* newRoot = readNode(topNode()->childrenAddress[0]);
                    clearNodeInFile(newRoot->myAddress); // clearing the node in index as it will we moved
                    newRoot->parentAddress = -1;
                    newRoot->myAddress = 0;
                    newRoot->dirty = true;
                    clearNode(getRoot());
                    nodeBuffer[0] = newRoot;
                    nodeBufferTop = 0;
                    flushRoot();
                    height--;
                    return true;
                }
            }
            else
            {
                cout << "ERROR, deleteRecord could not compensate or merge! (The tree may have just become empty)\n";
                cin.get();
                cin.get();
                return false;
            }
        }
        numberOfRecordsInTree--;
        return true;
    }

    void modifyRecord(int key, Record record)
    {
        popAll();
        findRecord(key);
        int recordNode = topNode()->myAddress;
        popAll();
        findRecord(record.key);
        int newNode = topNode()->myAddress;

        if(recordNode == newNode)
        {
            modifyRecordInNode(key, record);
            return;
        }

        deleteRecord(key);
        addRecord(record);
        //dataFlushBuffer();
    }

    // should be called after findRecord, the node containing the record shoud be loaded as topNode()
    // the function wont look for the record in children
    void modifyRecordInNode(int key, Record record)
    {
        for(int i = 0; i < topNode()->recordsFilled; i++)
        {
            if(topNode()->records[i].key == key) // found, delete
            { 
                topNode()->records[i] = BRecord(topNode()->records[i].address, record.key);
                topNode()->dirty = true;
                dataModifyRecord(topNode()->records[i].address, record);
                return;
            }
        }
    }

    void dataModifyRecord(int address, Record newRecord)
    {
        if(dataLoadedInBuffer(address))
        {
            buffer[address % BUFFER_SIZE] = newRecord;
            return;
        }

        if(loadBlock(address / BUFFER_SIZE))
        {
            buffer[address % BUFFER_SIZE] = newRecord;
            return;
        }
    }

    void clearNodeInFile(int address)
    {
        BNode* node = new BNode();
        node->myAddress = address;
        node->dirty = true;
        flushNode(node);
        delete node;
    }

    void topDeleteRecord(BRecord record)
    {


        if(topNode()->recordsFilled == 0)
        {
            cout << "Trying to delete from empty node!\n";
            return;
        }

        for(int i = 0; i < topNode()->recordsFilled; i++)
        {
            if(topNode()->records[i].key == record.key) // found, delete
            { 
                if(topNode()->isLeaf())
                {
                    for(int j = i; j < topNode()->recordsFilled-1; j++)
                    {
                        topNode()->records[j] = topNode()->records[j+1];
                        topNode()->childrenAddress[j] = topNode()->childrenAddress[j+1];
                    }
                    topNode()->records[topNode()->recordsFilled-1] = BRecord();
                    topNode()->childrenAddress[topNode()->recordsFilled-1] = topNode()->childrenAddress[topNode()->recordsFilled];
                    topNode()->recordsFilled--;
                    topNode()->dirty = true;
                    if(topNode()->recordsFilled < NODE_SIZE/2)
                    {
                        if(compensation())
                        {
                            return;
                        }
                        else if(mergeNode())
                        {
                            return;
                        }
                        else
                        {
                            cout << "ERROR, topDeleteRecord could not compensate or merge! (tree may be empty)\n";
                            cin.get();
                            cin.get();
                            return;
                        
                        }
                    }
                    return;
                }
                else
                {
                    BNode* me = topNode();
                    BRecord r;

                    if(pushNodeChild(i+1)) // smallest from right subtree
                    {
                        r = extractSmallestRecordRecursive();
                    }
                    if( r.address == -1 && pushNodeChild(i)) // biggest from left subtree
                    {
                        r = extractBiggestRecordRecursive();
                    }
                    if(r.address == -1)
                    {
                        cout << "ERROR, topDeleteRecord could not push child to find leaf!\n";
                        return;
                    }
                    //after extracting topNode is the leaft from which the record was extracted

                    if(r.address == -1)
                    {
                        cout << "ERROR, topDeleteRecord could not extract record from leaf!\n";
                        return;
                    }

                    for(int j = 0; j < me->recordsFilled; j++) // replacing with the record from the subtree
                    {
                        if(me->records[j].key == record.key)
                        { 
                            me->records[j] = r;
                            me->dirty = true;
                        }
                    }

                    // try to compensate leaf
                    bool compensated = topNode()->recordsFilled >= NODE_SIZE/2;
                    while(!compensated)
                    {
                        if(compensation())
                            compensated = true;
                        else if(mergeNode()) // after merge check if parent needs repairing
                        {
                            popNode();
                            compensated = topNode()->recordsFilled >= NODE_SIZE/2;
                        }
                        else if(topNode()->myAddress == 0) // root is merging
                        {
                            if(topNode()->recordsFilled == 0)
                            {
                                BNode* newRoot = readNode(topNode()->childrenAddress[0]);
                                clearNodeInFile(newRoot->myAddress); // clearing the node in index as it will we moved
                                newRoot->parentAddress = -1;
                                newRoot->myAddress = 0;
                                newRoot->dirty = true;
                                clearNode(getRoot());
                                nodeBuffer[0] = newRoot;
                                nodeBufferTop = 0;
                                
                                flushRoot();
                                compensated = true;
                                height--;
                                return; // return early because we already are at root
                            }
                        }
                        else
                        {
                            cout << "ERROR, topDeleteRecord could not compensate or merge! ( tree may be empty )\n";
                            cin.get();
                            cin.get();
                            return;
                        }
                    }

                    // return to root if merge did not propagate all the way to root
                    while(topNode() != me)
                        popNode();
                    
                    return;

                }
                
            }
            else if(topNode()->records[i].key > record.key)// is smaller than some record
            {
                if(pushNodeChild(i)) // true if child exists and was pushed to the buffer
                {
                    topDeleteRecord(record);
                    return;
                }
            }
        }

        // key is bigger than anything in this node, check in the last child
        if(pushNodeChild(topNode()->recordsFilled))
        {
            topDeleteRecord(record);
            return;
        }

        cout << "Record not found!\n";
    }

    BRecord extractSmallestRecordRecursive()
    {
        if(topNode()->childrenAddress[0] == -1)
        {
            BRecord ret = topNode()->records[0];
            for(int i = 0; i < topNode()->recordsFilled-1; i++)
            {
                topNode()->records[i] = topNode()->records[i+1];
                topNode()->childrenAddress[i] = topNode()->childrenAddress[i+1];
            }
            topNode()->records[topNode()->recordsFilled-1] = BRecord();
            topNode()->childrenAddress[topNode()->recordsFilled-1] = topNode()->childrenAddress[topNode()->recordsFilled];
            topNode()->recordsFilled--;
            topNode()->dirty = true;
            return ret;
        }
        else
        {
            if(pushNodeChild(0))
            {
                BRecord ret = extractSmallestRecordRecursive();
                return ret;
            }
        }
        return BRecord();
    }

    BRecord extractBiggestRecordRecursive()
    {
        if(topNode()->childrenAddress[topNode()->recordsFilled] == -1)
        {
            BRecord ret = topNode()->records[0];
            for(int i = 0; i < topNode()->recordsFilled-1; i++)
            {
                topNode()->records[i] = topNode()->records[i+1];
                topNode()->childrenAddress[i] = topNode()->childrenAddress[i+1];
            }
            topNode()->records[topNode()->recordsFilled-1] = BRecord();
            topNode()->childrenAddress[topNode()->recordsFilled-1] = topNode()->childrenAddress[topNode()->recordsFilled];
            topNode()->recordsFilled--;
            topNode()->dirty = true;


            bool ok = true;
            if(topNode()->recordsFilled < NODE_SIZE/2)
            {
                ok = false;
                if(compensation())
                {
                    ok=true;
                }
                else if(mergeNode())
                {
                    ok=true;
                }
            }

            if(!ok)
            {
                //restore
                for(int i = topNode()->recordsFilled-1; i > 0; i--)
                {
                    topNode()->records[i] = topNode()->records[i-1];
                    topNode()->childrenAddress[i] = topNode()->childrenAddress[i-1];
                }
                topNode()->records[0] = ret;
                popNode();
                return BRecord();
                
            }

            popNode();
            if(topNode()->recordsFilled < NODE_SIZE/2)
            {
                if(compensation())
                {
                    return ret;
                }
                else if(mergeNode())
                {
                    return ret;
                }
                else
                {
                    cout << "ERROR, extractSmallestRecordRecursive could not compensate or merge!\n";
                    cin.get();
                    cin.get();
                    return BRecord();
                }
            }
            return ret;
        }
        else
        {
            if(pushNodeChild(topNode()->recordsFilled))
            {
                BRecord ret = extractBiggestRecordRecursive();
                popNode();
                return ret;
            }
        }
        return BRecord();
    }


    bool mergeNode()
    {
        if(!topNodeParent()) // cant merge if doesnt have a parent
        {
            return false;
        }

        if(mergeNodeLeft())
        {
            return true;
            numberOfNodesInTree--;
        }
        if(mergeNodeRight())
        {
            return true;
            numberOfNodesInTree--;
        }
        return false;
    }

    bool mergeNodeLeft()
    {
        int childNumber = getNumberInParent();
        if(childNumber == -1)
        {
            cout << "Merge, parent doesnt know me!\n";
            cin.get();
            cin.get();
            return false;
        }

        BNode* leftSibling = nullptr;
        if(childNumber > 0)
            leftSibling = readNode(topNodeParent()->childrenAddress[childNumber-1]);
        if(!leftSibling) // no left sibling
        {
            return false;
        }

        if(leftSibling->recordsFilled + topNode()->recordsFilled > NODE_SIZE) // left sibling and topNode together are too big
        {
            free(leftSibling);
            return false;
        }

        BRecord* records = (BRecord*) malloc(sizeof(BRecord) * (NODE_SIZE+1));
        int* childrenAddresses = (int*) malloc(sizeof(int) * (NODE_SIZE+1));
        int lastChild = topNode()->childrenAddress[topNode()->recordsFilled];
        for(int i = 0; i < leftSibling->recordsFilled; i++) // copy all records and children from left sibling
        {
            records[i] = leftSibling->records[i];
            childrenAddresses[i] = leftSibling->childrenAddress[i];
        }
        childrenAddresses[leftSibling->recordsFilled] = leftSibling->childrenAddress[leftSibling->recordsFilled]; // from parent
        records[leftSibling->recordsFilled] = topNodeParent()->records[childNumber-1]; // record in parent is in between records of left sibling and nodeptr
        for(int i = 0; i < topNode()->recordsFilled; i++) // copy all records and children from the node
        {
            records[i + leftSibling->recordsFilled + 1] = topNode()->records[i];
            childrenAddresses[i + leftSibling->recordsFilled + 1] = topNode()->childrenAddress[i];
        }

        // records should be sorted from lowest to highest key
        // put them now into topNode()
        clearNode(topNode());
        for(int i = 0; i < NODE_SIZE; i++)
        {
            topNode()->records[i] = records[i];
            topNode()->childrenAddress[i] = childrenAddresses[i];
        }
        topNode()->childrenAddress[NODE_SIZE] = lastChild;

        //topNodeParent()->childrenAddress[childNumber] = topNode()->myAddress; // removing the right sibling from parent's children
        topNodeParent()->childrenAddress[childNumber-1] = -1; // removing the leftSibling from parent
        for(int i = childNumber-1; i < topNodeParent()->recordsFilled; i++) // move parents records one place to the left after removing the record
        {
            topNodeParent()->records[i] = topNodeParent()->records[i+1];
            topNodeParent()->childrenAddress[i] = topNodeParent()->childrenAddress[i+1];
        }
        topNodeParent()->recordsFilled--;
        topNodeParent()->records[topNodeParent()->recordsFilled] = BRecord();
        topNodeParent()->childrenAddress[topNodeParent()->recordsFilled] = topNodeParent()->childrenAddress[topNodeParent()->recordsFilled+1];
        topNodeParent()->childrenAddress[topNodeParent()->recordsFilled+1] = -1;


        topNode()->recordsFilled = topNode()->recordsFilled + leftSibling->recordsFilled + 1;
        topNode()->childrenAddress[topNode()->recordsFilled] = lastChild;
        topNode()->dirty = true;
        leftSibling->dirty = true;
        topNodeParent()->dirty = true;
        clearNode(leftSibling);
        leftSibling->parentAddress = -1;
        flushNode(leftSibling);
        free(leftSibling);

        return true;
    }
    
    bool mergeNodeRight()
    {
        int childNumber = getNumberInParent();
        if(childNumber == -1)
        {
            cout << "Merge, parent doesnt know me!\n";
            cin.get();
            cin.get();
            return false;
        }
        
        BNode* rightSibling = nullptr;
        if(childNumber < NODE_SIZE-1)
            rightSibling = readNode(topNodeParent()->childrenAddress[childNumber+1]);
        if(!rightSibling) // no left sibling
        {
            return false;
        }

        if(rightSibling->recordsFilled + topNode()->recordsFilled >= NODE_SIZE) // right sibling and topNode together are too big
        {
            free(rightSibling);
            return false;
        }

        //no need of storing records because we can just append them as the record in parent and records in right sibling are already sorted
        topNode()->records[topNode()->recordsFilled] = topNodeParent()->records[childNumber];
        //topNodeParent()->childrenAddress[childNumber] = topNode()->myAddress; // removing the right sibling from parent's children
        topNodeParent()->childrenAddress[childNumber+1] = -1; // removing the record from parent
        topNodeParent()->recordsFilled--;
        for(int i = childNumber+1; i < topNodeParent()->recordsFilled; i++) // move parents records one place to the left after removing the record
        {
            topNodeParent()->records[i] = topNodeParent()->records[i+1];
            topNodeParent()->childrenAddress[i] = topNodeParent()->childrenAddress[i+1];
        }
        for(int i = 0; i < rightSibling->recordsFilled; i++) // right sibling has higher record keys
        {
            topNode()->records[topNode()->recordsFilled + i + 1] = rightSibling->records[i];
        }

        topNode()->recordsFilled = topNode()->recordsFilled + rightSibling->recordsFilled + 1;
        topNode()->dirty = true;
        rightSibling->dirty = true;
        topNodeParent()->dirty = true;
        clearNode(rightSibling);
        rightSibling->parentAddress = -1;
        flushNode(rightSibling);
        free(rightSibling);

        return true;

    }

    int getNumberInParent()
    {
        if(topNodeParent())
        {
            for(int i = 0; i < NODE_SIZE+1; i++)
            {
                if(topNodeParent()->childrenAddress[i] == topNode()->myAddress)
                {
                    return i;
                }
            }
        }
        return -1;
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
                //  &1 record1 &2 _ . _ . ..... 
                //                ^
                //  &1 record1 &3 record 2 &2 _ . _ ......
                if(topNode()->records[i].empty() && topNode()->childrenAddress[i] != -1 && i < NODE_SIZE)
                {
                    topNode()->childrenAddress[i+1] = topNode()->childrenAddress[i];
                    topNode()->records[i] = record;
                    topNode()->childrenAddress[i] = childAddress;
                    topNode()->recordsFilled++;
                    topNode()->dirty = true;
                    return;
                }

                // if the node is not full and there is a larger key
                // insert to this node by making place for the new record
                if(topNode()->records[i].key > record.key) 
                {
                    topNode()->childrenAddress[NODE_SIZE] = topNode()->childrenAddress[NODE_SIZE-1]; // moving the last children which is not associated with any record
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

        // node overflow, try compensatio

        if(compensation())
        {
            topAddRecord(record, childAddress);
            return;
        }


        // could not compensate, split node
        splitNodeAdd(record, childAddress);
        numberOfNodesInTree++;

        flushNode(topNode());
    }

    bool compensation()
    {

        if(!topNodeParent()) // cant compensate if doesnt have a parent
        {
            return false;
        }

        if(leftCompensation())
        {
            return true;
        }
        if(rightCompensation())
        {
            return true;
        }

        return false;
    }

    bool leftCompensation()
    {
        int childNumber = getNumberInParent();
        if(childNumber == -1)
        {
            cout << "Compensation, parent doesnt know me!\n";
            cin.get();
            cin.get();
            return false;
        }

        BNode* leftSibling = nullptr;
        if(childNumber > 0)
            leftSibling = readNode(topNodeParent()->childrenAddress[childNumber-1]);
        if(!leftSibling) // no left sibling
        {
            return false;
        }

        if(leftSibling->recordsFilled == NODE_SIZE && topNode()->recordsFilled!=0) // left sibling is full
        {
            free(leftSibling);
            return false;
        }
        if(leftSibling->recordsFilled + topNode()->recordsFilled < NODE_SIZE) // left sibling and topNode together are not full
        {
            free(leftSibling);
            return false;
        }

        // reading all records to an array for distribution
        int totalRecords = topNode()->recordsFilled + leftSibling->recordsFilled + 1; // + record in parent
        BRecord* records = (BRecord*) malloc(sizeof(BRecord) * totalRecords);
        int* childrenAddresses = (int*) malloc(sizeof(int) * (totalRecords+2));

        for(int i = 0; i < leftSibling->recordsFilled; i++) // left sibling has smaller record keys
        {
            records[i] = leftSibling->records[i];
            childrenAddresses[i] = leftSibling->childrenAddress[i];
        }
        childrenAddresses[leftSibling->recordsFilled] = leftSibling->childrenAddress[leftSibling->recordsFilled]; // lefts last child

        records[leftSibling->recordsFilled] = topNodeParent()->records[childNumber-1]; // record in parent is in between records of left sibling and nodeptr
        // parent's children grouped by index in arrays:
        // (* R)        (* R)        (* R)       (* R)
        //               ^left        ^nodeptr    ^right
        //               sibling                  sibling
        // the record between nodeptr and left sibling is in parent's array on the same index as left sibling
        // the record between nodeptr and right sibling would be on the same index as nodeptr

        for(int i = 0; i < topNode()->recordsFilled; i++) // topNode has bigger keys than left sibling and the record in parent
        {
            records[i + leftSibling->recordsFilled + 1] = topNode()->records[i];
            childrenAddresses[i + leftSibling->recordsFilled + 1] = topNode()->childrenAddress[i];
        }
        childrenAddresses[totalRecords] = topNode()->childrenAddress[topNode()->recordsFilled];

        // records should be sorted from lowest to highest key

        clearNode(topNode());
        clearNode(leftSibling);
        //distribute records
        for(int i = 0; i < totalRecords/2; i++)
        {
            leftSibling->records[i] = records[i];
            leftSibling->childrenAddress[i] = childrenAddresses[i];
        }
        leftSibling->childrenAddress[totalRecords/2] = childrenAddresses[totalRecords/2];
        topNodeParent()->records[childNumber-1] = records[totalRecords/2];
        for(int i = 0; i < totalRecords - totalRecords/2 - 1; i++)
        {
            topNode()->records[i] = records[i + totalRecords/2 + 1];
            topNode()->childrenAddress[i] = childrenAddresses[i + totalRecords/2 + 1];
        }
        topNode()->childrenAddress[totalRecords - totalRecords/2 - 1] = childrenAddresses[totalRecords];

        leftSibling->recordsFilled = totalRecords/2;
        topNode()->recordsFilled = totalRecords - totalRecords/2 - 1;

        for(int i = topNode()->recordsFilled; i < NODE_SIZE; i++) // clearing remaining records
        {
            topNode()->records[i] = BRecord();
        }

        leftSibling->dirty = true;
        topNode()->dirty = true;
        topNodeParent()->dirty = true;

        free(records);
        flushNode(leftSibling);
        free(leftSibling);
        return true;
    }

    bool rightCompensation()
    {
        int childNumber = getNumberInParent();
        if(childNumber == -1)
        {
            cout << "Compensation, parent doesnt know me!\n";
            cin.get();
            cin.get();
            return false;
        }
        
        BNode* rightSibling = nullptr;
        if(childNumber < NODE_SIZE-1)
            rightSibling = readNode(topNodeParent()->childrenAddress[childNumber+1]);
        if(!rightSibling) // no left sibling
        {
            return false;
        }

        if(rightSibling->recordsFilled == NODE_SIZE && topNode()->recordsFilled!=0) // right sibling is full
        {
            free(rightSibling);
            return false;
        }
        if(rightSibling->recordsFilled + topNode()->recordsFilled < NODE_SIZE) // right sibling and topNode together are not full
        {
            free(rightSibling);
            return false;
        }

        // reading all records to an array for distribution
        int totalRecords = topNode()->recordsFilled + rightSibling->recordsFilled + 1; // + record in parent
        BRecord* records = (BRecord*) malloc(sizeof(BRecord) * totalRecords);


        for(int i = 0; i < topNode()->recordsFilled; i++) // topNode has lower keys than right sibling and the record in parent
            records[i] = topNode()->records[i];


        // parent's children grouped by index in arrays:
        // (* R)        (* R)        (* R)       (* R)
        //               ^left        ^nodeptr    ^right
        //               sibling                  sibling
        // the record between nodeptr and left sibling is in parent's array on the same index as left sibling
        // the record between nodeptr and right sibling would be on the same index as nodeptr
        records[topNode()->recordsFilled] = topNodeParent()->records[childNumber]; // record in parent is in between records of left sibling and nodeptr

        for(int i = 0; i < rightSibling->recordsFilled; i++) // right sibling has higher record keys
        {
            records[topNode()->recordsFilled + 1 + i] = rightSibling->records[i];
        }


        // records should be sorted from lowest to highest key


        //distribute records
        for(int i = 0; i < totalRecords/2; i++)
        {
            rightSibling->records[i] = records[totalRecords - totalRecords/2 +i]; // right sibling gets the highest keys
        }
        
        topNodeParent()->records[childNumber] = records[totalRecords-totalRecords/2-1]; // record in parent is in between records of left sibling and nodeptr

        for(int i = 0; i < totalRecords - totalRecords/2 - 1; i++)
        {
            topNode()->records[i] = records[i];
        }

        rightSibling->recordsFilled = totalRecords/2;
        topNode()->recordsFilled = totalRecords - totalRecords/2 - 1;

        for(int i = topNode()->recordsFilled; i < NODE_SIZE; i++) // clearing remaining records
        {
            topNode()->records[i] = BRecord();
        }

        rightSibling->dirty = true;
        topNode()->dirty = true;
        topNodeParent()->dirty = true;

        free(records);
        flushNode(rightSibling);
        free(rightSibling);
        return true;
    }

    //topNode() is the left split after this
    //topNode() must be full to be splitted
    void splitNodeAdd(BRecord r, int childAddress)
    {
        int totalRecords = NODE_SIZE;

        BRecord* records = (BRecord*) malloc(sizeof(BRecord) * (NODE_SIZE+1));
        int* childrenAddresses = (int*) malloc(sizeof(int) * (NODE_SIZE+1));
        int lastChild = topNode()->childrenAddress[NODE_SIZE];

        int childNumber = getNumberInParent();
        if(topNode()->myAddress != 0 && childNumber == -1) // is not root and parent doesnt know me
        {
            cout << "Split, parent doesnt know me!\n";
            cin.get();
            cin.get();
            return;
        }

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
            if(childrenAddresses)
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
        {
            rightSplit = topNode();
            clearNode(rightSplit);
        }

        // distribute records among the new children
        for(int i = 0; i < totalRecords/2; i++) // first half
        {
            leftSplit->records[i] = records[i];
            leftSplit->childrenAddress[i] = childrenAddresses[i];
        }

        BRecord middle = records[totalRecords/2];
        int childMiddle = childrenAddresses[totalRecords/2];

        for(int i = totalRecords/2 + 1; i < totalRecords+1; i++) // everything else
        {
            rightSplit->records[i-totalRecords/2-1] = records[i];
            rightSplit->childrenAddress[i-totalRecords/2-1] = childrenAddresses[i];
        }// right split will get 1 more record if NODE_SIZE is uneven

        leftSplit->recordsFilled = totalRecords/2;
        rightSplit->recordsFilled = totalRecords - totalRecords/2;
        leftSplit->dirty = true;
        rightSplit->dirty = true;       


        // record at totalRecords/2 was not moved, it will be the record inserted into parent
        leftSplit->childrenAddress[totalRecords/2] = childMiddle; // copy the child before middle address
        rightSplit->childrenAddress[rightSplit->recordsFilled] = lastChild; // copy the last child address

        
        free(records);
        free(childrenAddresses);
        
        if(topNode() == getRoot()) // root is splitting
        {
            clearNode(topNode()); // clear new root as its contents were moved to its new children
            topNode()->childrenAddress[0] = numberOfNodes; // the addressess left and right split will get when appended
            topNode()->childrenAddress[1] = numberOfNodes+1; // first the left split gets appended, so the order is left-to-right
            topNode()->records[0] = middle;
            topNode()->recordsFilled = 1;
            topNode()->dirty = true;
            leftSplit->parentAddress = topNode()->myAddress;
            rightSplit->parentAddress = topNode()->myAddress;
            height++;

            flushRoot();
        }
        else if(topNodeParent())
        {
            leftSplit->parentAddress = topNodeParent()->myAddress;
        }

        indexAppendNode(leftSplit); 

        if(topNode() == getRoot())
        {
            indexAppendNode(rightSplit);
            delete rightSplit;
        }

        delete leftSplit;

        if(topNode() != getRoot()) // if it was root then middle is already placed and there is no need of further propagation
        {
            //topNodeParent()->childrenAddress[childNumber] = rightSplit->myAddress;
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
        indexWriteDiskOperations++;
        close();
    }


    // returns record number in file
    int dataAppendRecord(Record r)
    {
        int address = numberOfRecords;
        if(!dataInBuffer(address))
        {
            dataFlushBuffer();
            loadBlock(address / BUFFER_SIZE);
        }


        buffer[bufferRecordsFilled] = r;
        bufferRecordsFilled++;
        numberOfRecords++;
        if(bufferRecordsFilled == BUFFER_SIZE)
            dataFlushBuffer();
        
        return address;
    }

    void dataFlushBuffer()
    {
        if(bufferRecordsFilled == 0)
            return;
        open();
        fseek(file, loadedBlockNumber * BUFFER_SIZE * sizeof(Record), SEEK_SET);
        if(fwrite(&buffer, sizeof(Record), bufferRecordsFilled, file) != (size_t)bufferRecordsFilled)
        {
            cout << "Error dataFlushBuffer Did not write the buffer contents\n";
            return;
        }
        close();

        if(loadedBlockNumber == dataBlocksFilled)
            dataBlocksFilled++;
        dataWriteDiskOperations++;
    }

    void flushRoot()
    {
        flushNode(nodeBuffer[0]);
    }

    ~Btree()
    {
        cout << "Tree destructor, this = " << this << "\n";
        popAll();
        dataFlushBuffer();
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
        while(BNode* node = readNode(i++, false))
        {
            cout << node->toString();
            free(node);
            node = nullptr;
        }
        // open();
        // cout << "ferror: " << ferror(index) << "\n";
        // cout << "feof: " << feof(index) << "\n";
        // close();
    }

    void printSorted()
    {
        cout << "#######  BTree Sorted:\n";
        BNode node;

        
        popAll();
        //flushCurrentNode();
        flushRoot();
        dataFlushBuffer();

        printTopNodeRecordsRecursive();
    }
    
    void printTopNodeRecordsRecursive()
    {

        if(topNode() == nullptr)
            return;
        for(int i = 0; i < topNode()->recordsFilled; i++)
        {
            Record r = readRecord(topNode()->records[i].address);
            if(pushNodeChild(i))
            {
                printTopNodeRecordsRecursive();
                popNode();
            }
            cout << r.toString() << " &" << topNode()->records[i].address << " node &" << topNode()->myAddress << "\n";
        }
        if(pushNodeChild(topNode()->recordsFilled))
        {
            printTopNodeRecordsRecursive();
            popNode();
        }
    }

    int getIndexReads(){
        return indexReadDiskOperations;
    }
    int getIndexWrites(){
        return indexWriteDiskOperations;
    }
    int getDataReads(){
        return dataReadDiskOperations;
    }
    int getDataWrites(){
        return dataWriteDiskOperations;
    }
    int getReads(){
        return indexReadDiskOperations;
    }
    int getWrites(){
        return dataWriteDiskOperations;
    }

    string getDataPath() const
    {
        return filePath;
    }

    string getIndexPath() const
    {
        return filePath + ".btree";
    }

    long long getIndexBytes() const
    {
        return numberOfNodes * sizeof(BNode);
    }

    long long getDataBytes() const
    {
        return numberOfRecords * sizeof(Record);
    }

    int getHeight() const
    {
        return height;
    }

    int getNumberOfNodesInTree() const
    {
        return numberOfNodesInTree;
    }

    int getNumberOfRecordsInTree() const
    {
        return numberOfRecordsInTree;
    }

};