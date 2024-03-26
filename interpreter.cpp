#include <iostream>
#include <vector>

#include "based.hpp"
#include "chacha-cpp17.hpp"

using namespace std;


int main() {
    vector<string> code;
    string s;
    while (getline(cin, s)) {
        code.push_back(s);
    }

    try {
        based::Program prog(code);
        prog.add_input(123);
        prog.add_input(456);
        prog.run(10000);
        if (prog.has_output()) {
            cout << prog.fetch_output<based::Program::integer>() << '\n';
        }
    } catch (based::ProgramException &ex) {
        cout << ex.what() << '\n';
    }
}
