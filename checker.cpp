#include <algorithm>
#include <exception>
#include <iostream>
#include <random>
#include <variant>
#include <vector>

#include "based.hpp"
#include "chacha-cpp17.hpp"

using namespace std;


rand_lib::ChaChaRng<12> global_rng(rand_lib::derive_seed(0xba5ed));

uniform_int_distribution<long long> int_dist(-1e18, 1e18);


class WrongAnswer : std::exception {
  public:
    WrongAnswer() = delete;
    explicit WrongAnswer(const std::string &error) {
        message = "this ain't it chief... " + error;
    }

    const char *what() const noexcept final {
        return message.c_str();
    }

  private:
    std::string message;
};

class Problem {
  public:
    Problem() = delete;
    explicit Problem(based::Program program)
        : prog(std::move(program)), correct_answer(0) {}

    void run_and_check_answer(size_t steps) {
        prog.run(steps);
        if (!prog.has_output()) {
            throw WrongAnswer("print something");
        }
        try {
            auto output = prog.fetch_output<long long>();
            if (output != correct_answer) {
                throw WrongAnswer("git gud");
            }
        } catch (std::bad_variant_access &ex) {
            throw WrongAnswer("U PRINTERD AN ENTRIE ARRAY???");
        }
        if (prog.has_output()) {
            throw WrongAnswer("too much stuff printed");
        }
    }

  protected:
    based::Program prog;
    long long correct_answer;
};

class Problem1 : public Problem {
  public:
    explicit Problem1(based::Program program) : Problem(std::move(program)) {
        long long a = int_dist(global_rng), b = int_dist(global_rng);
        prog.add_input(a);
        prog.add_input(b);
        correct_answer = a + b;
    }
};

class Problem2 : public Problem {
  public:
    explicit Problem2(based::Program program) : Problem(std::move(program)) {
        long long a = int_dist(global_rng);
        prog.add_input(a);
        correct_answer = abs(a);
    }
};

class Problem3 : public Problem {
  public:
    Problem3(based::Program program, int n) : Problem(std::move(program)) {
        vector<long long> a(n);
        generate(a.begin(), a.end(), [] { return int_dist(global_rng); });
        prog.add_input(n);
        prog.add_input(a);
        correct_answer = *max_element(a.begin(), a.end());
    }
};

class Problem4 : public Problem {
  public:
    Problem4(based::Program program, int n) : Problem(std::move(program)) {
        vector<long long> a(n);
        int k = uniform_int_distribution<int>(1, n)(global_rng);
        generate(a.begin(), a.end(), [] { return int_dist(global_rng); });
        prog.add_input(n);
        prog.add_input(a);
        prog.add_input(k);
        nth_element(a.begin(), a.begin() + n - k, a.end());
        correct_answer = a[n - k];
    }
};


int main(int argc, char **argv) {
    int type;
    if (argc < 2) {
        cout << "Usage: <checker> <n>\n";
        return 1;
    }
    type = atoi(argv[1]);

    vector<string> code;
    string s;
    while (getline(cin, s)) {
        code.push_back(s);
    }

    try {
        based::Program prog(code);
        if (type == 1) {
            for (int i = 0; i < 10; i++) {
                Problem1(prog).run_and_check_answer(10000);
            }
        } else if (type == 2) {
            for (int i = 0; i < 10; i++) {
                Problem2(prog).run_and_check_answer(10000);
            }
        } else if (type == 3) {
            for (int i = 1; i <= 50; i++) {
                Problem3(prog, i).run_and_check_answer(10000);
            }
        } else if (type == 4) {
            for (int i = 1; i <= 50; i++) {
                Problem4(prog, i).run_and_check_answer(100000);
            }
        } else {
            cout << "Unknown type " << type << '\n';
            return 1;
        }
    } catch (based::ProgramException &ex) {
        cout << ex.what() << '\n';
        return 1;
    } catch (WrongAnswer &ex) {
        cout << ex.what() << '\n';
        return 1;
    }
    cout << "ok\n";
}
