#include <iostream>
#include <vector>

#include "based.hpp"

using namespace std;

int main() {
    vector<string> code;
    string s;
    while (getline(cin, s)) {
        code.push_back(s);
    }

    try {
        based::Program prog(code);
        prog.add_input(5);
        prog.add_input({1, -1, 2, -3, 5});
        prog.add_input(3);
        prog.run(1000000);
        if (prog.has_output()) {
            cout << prog.fetch_output<based::Program::integer>() << '\n';
        }
    } catch (based::ProgramException &ex) {
        cout << ex.what() << '\n';
    }
}
