#include "Btree.hpp"

string Record::toString(){
    string out = "";
    out += "Record: {";
    for(int i = 0; i < RECORD_LENGTH; i++){
        out += std::to_string(fields[i]) + ", ";
    }
    out += "\b\b}, (key=" + std::to_string(key) + ")";
    return out;
}