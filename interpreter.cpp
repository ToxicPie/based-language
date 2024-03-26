#include <iostream>
#include <vector>

#include "chacha-cpp17.hpp"
#include "based.hpp"

using namespace std;


int main() {
    vector<string> code;
    string s;
    while (getline(cin, s)) {
        code.push_back(s);
    }

    based::Program prog(code);
}
