#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "siphash-cpp17.hpp"


namespace based {


struct Formatter {
    const char *fmt;
    template <typename... T> std::string operator()(T... t) {
        std::string result;
        int len = std::snprintf(result.data(), 0, fmt, t...);
        result.assign(len + 1, '\0');
        len = std::snprintf(result.data(), result.size(), fmt, t...);
        result.resize(len);
        return result;
    }
};

Formatter operator""_format(const char *fmt, size_t) {
    return Formatter{fmt};
}


std::string compress(const std::string_view &s, size_t max_len = 32) {
    if (s.size() <= max_len) {
        return std::string(s);
    }
    max_len = std::max(max_len, size_t(3));
    return std::string(s.substr(0, max_len - 3)) + "...";
}


class ProgramException : std::exception {
  public:
    ProgramException() = delete;
    ProgramException(int line, const std::string &error) {
        message = "line %d: %s"_format(line + 1, error.c_str());
    }

    const char *what() const noexcept final {
        return message.c_str();
    }

  private:
    std::string message;
};

class CompileError : public ProgramException {
  public:
    CompileError(int line, const std::string &error)
        : ProgramException(line,
                           "'%s'? jesse, what are you talking about?"_format(
                               compress(error, 60).c_str())) {}
};

class RuntimeError : public ProgramException {
  public:
    RuntimeError(int line, const std::string &error)
        : ProgramException(line, "ya code got l + ratioed because %s"_format(
                                     error.c_str())) {}
};

class TimeLimitExceeded : public ProgramException {
  public:
    explicit TimeLimitExceeded(int line)
        : ProgramException(line, "you have skill issue on speed smh") {}
};


class Program {
  public:
    using integer = long long;
    using variable = std::variant<integer, std::vector<integer>>;

    Program() = delete;
    explicit Program(const std::vector<std::string> &code)
        : pc(0), returned(false) {
        instructions.reserve(code.size());
        for (size_t line = 0; line < code.size(); line++) {
            Instruction instruction(code[line]);
            if (!instruction.valid()) {
                throw CompileError(line, code[line]);
            }
            instructions.push_back(instruction);
        }
    }

    void add_input(integer num) {
        input.emplace_back(num);
    }

    void add_input(std::vector<integer> array) {
        input.emplace_back(std::move(array));
    }

    template <typename T> T fetch_output() {
        T ret = std::get<T>(output.back());
        output.pop_back();
        return ret;
    }

    bool has_output() const {
        return !output.empty();
    }

    void run(size_t max_steps) {
        for (size_t step = 0; !returned && step < max_steps; step++) {
            single_step();
        }
        if (returned) {
            return;
        }
        throw TimeLimitExceeded(pc);
    }

  private:
    struct Instruction {
        explicit Instruction(const std::string &code) {
            std::istringstream sstream(code);
            std::vector<std::string> tokens{
                std::istream_iterator<std::string>(sstream),
                std::istream_iterator<std::string>(),
            };
            if (tokens.empty()) {
                opcode = Nop;
                parameters = {};
                return;
            }
            if (tokens.size() == 2 && //
                tokens[0] == "yoink") {
                opcode = Input;
                parameters = {tokens[1]};
                return;
            }
            if (tokens.size() == 2 && //
                tokens[0] == "yeet") {
                opcode = Output;
                parameters = {tokens[1]};
                return;
            }
            if (tokens.size() == 6 &&    //
                tokens[0] == "bruh" &&   //
                tokens[2] == "is" &&     //
                tokens[3] == "lowkey" && //
                tokens[4] == "just") {
                opcode = Assign;
                parameters = {tokens[1], tokens[5]};
                return;
            }
            if (tokens.size() == 6 &&    //
                tokens[0] == "*slaps" && //
                tokens[2] == "on" &&     //
                tokens[3] == "top" &&    //
                tokens[4] == "of" &&     //
                tokens[5].size() >= 2 && tokens[5].back() == '*') {
                opcode = Add;
                tokens[5].pop_back();
                parameters = {tokens[5], tokens[1]};
                return;
            }
            if (tokens.size() == 7 &&  //
                tokens[0] == "rip" &&  //
                tokens[1] == "this" && //
                tokens[3] == "fell" && //
                tokens[4] == "off" &&  //
                tokens[5] == "by") {
                opcode = Sub;
                parameters = {tokens[2], tokens[6]};
                return;
            }
            if (tokens.size() == 5 &&   //
                tokens[0] == "vibe" &&  //
                tokens[1] == "check" && //
                tokens[3] == "ratios") {
                opcode = Compare;
                parameters = {tokens[2], tokens[4]};
                return;
            }
            if (tokens.size() == 3 &&  //
                tokens[0] == "simp" && //
                tokens[1] == "for") {
                opcode = Jump;
                parameters = {tokens[2]};
                return;
            }
            if (tokens.size() == 4 &&   //
                tokens[0] == "go" &&    //
                tokens[1] == "touch" && //
                tokens[2] == "some" &&  //
                tokens[3] == "grass") {
                opcode = Return;
                parameters = {};
                return;
            }
            opcode = Undefined;
            parameters = {};
        }

        bool valid() const {
            return opcode != Undefined;
        }

        enum Opcode {
            Nop,
            Input,
            Output,
            Assign,
            Add,
            Sub,
            Compare,
            Jump,
            Return,
            Undefined,
        } opcode;
        std::vector<std::string> parameters;
    };

    void validate_identifier(const std::string_view &ident) const {
        auto is_valid_char = [](char c) { return c == '_' || std::isalnum(c); };
        if (!std::all_of(ident.begin(), ident.end(), is_valid_char)) {
            throw RuntimeError(
                pc,
                "invalid identifier '%s', only letters, numbers, and '_' are allowed"_format(
                    compress(ident, 20).c_str()));
        }
        if (std::isdigit(ident[0])) {
            throw RuntimeError(
                pc,
                "invalid identifier '%s', cannot begin with a number"_format(
                    compress(ident).c_str()));
        }
    }

    bool is_identifier(const std::string_view &str) const {
        try {
            validate_identifier(str);
            return true;
        } catch (RuntimeError &rte) {
            return false;
        }
    }

    integer parse_integer_literal(const std::string_view &str) const {
        auto parse_nonnegative = [this](const std::string_view &str) {
            integer result = 0;
            for (const char &c : str) {
                if (!std::isdigit(c)) {
                    throw RuntimeError(pc,
                                       "failed to parse integer '%s'"_format(
                                           compress(str).c_str()));
                }
                result = result * 10 + (c - '0');
            }
            if (str.size() > 20) {
                throw RuntimeError(pc,
                                   "integer literal '%s' is too long"_format(
                                       compress(str).c_str()));
            }
            return result;
        };
        if (str[0] == '-') {
            return -parse_nonnegative(str.substr(1));
        }
        return parse_nonnegative(str);
    }

    integer &get_integer_variable(const std::string_view &str) {
        try {
            if (auto var = variables.find(std::string(str));
                var != variables.end()) {
                return std::get<integer>(var->second);
            }
            throw RuntimeError(
                pc, "no such integer: '%s'"_format(compress(str).c_str()));
        } catch (std::bad_variant_access &rte) {
            throw RuntimeError(pc, "variable '%s' is not an integer"_format(
                                       compress(str).c_str()));
        }
    }

    std::optional<integer> get_integer_value(const std::string_view &str) {
        if (str.empty()) {
            throw RuntimeError(pc, "expected number, found empty string");
        }
        try {
            return parse_integer_literal(str);
        } catch (RuntimeError &rte) {}
        try {
            return get_integer_variable(str);
        } catch (RuntimeError &rte) {}
        return std::nullopt;
    }

    integer &parse_array_entry(const std::string_view &str) {
        auto bracket_pos = str.find('[');
        auto array = str.substr(0, bracket_pos);
        auto index = str.substr(bracket_pos + 1, str.size() - bracket_pos - 2);
        validate_identifier(array);
        if (auto var = variables.find(std::string(array));
            var != variables.end()) {
            auto &array_var = std::get<std::vector<integer>>(var->second);
            if (auto index_value = get_integer_value(index);
                index_value.has_value()) {
                integer index_int = index_value.value();
                if (index_int < 0 || index_int >= (integer)array_var.size()) {
                    throw RuntimeError(pc,
                                       "index %s[%lld] out of bounds"_format(
                                           compress(array).c_str(), index_int));
                }
                return array_var[index_int];
            }
            throw RuntimeError(
                pc, "invalid index: '%s'"_format(compress(array).c_str()));
        }
        throw RuntimeError(
            pc, "no such array: '%s'"_format(compress(array).c_str()));
    }

    static bool is_array_entry(const std::string_view &str) {
        return str.find('[') != std::string_view::npos && str.back() == ']';
    }

    integer parse_value(const std::string_view &str) {
        // <integer variable>
        if (is_identifier(str)) {
            return get_integer_variable(str);
        }
        // <array variable>[<integer literal|integer variable>]
        if (is_array_entry(str)) {
            return parse_array_entry(str);
        }
        // <integer literal>
        return parse_integer_literal(str);
    }

    integer &parse_reference(const std::string_view &str) {
        // <integer variable>
        if (is_identifier(str)) {
            return get_integer_variable(str);
        }
        // <array variable>[<integer literal|integer variable>]
        if (is_array_entry(str)) {
            return parse_array_entry(str);
        }
        throw RuntimeError(pc, "cannot parse '%s' as an &mut integer"_format(
                                   compress(str).c_str()));
    }

    void single_step() {
        if (pc < 0 || pc >= (int)instructions.size()) {
            throw RuntimeError(pc, "that's not even a line");
        }
        int next_pc = pc + 1;
        const auto &[opcode, parameters] = instructions[pc];
        switch (opcode) {
        case Instruction::Nop: {
            break;
        }
        case Instruction::Input: {
            if (input.empty()) {
                throw RuntimeError(pc, "you're reading from nothing");
            }
            std::string dest = parameters[0];
            validate_identifier(dest);
            variables[dest] = std::move(input.back());
            input.pop_back();
            break;
        }
        case Instruction::Output: {
            std::string src = parameters[0];
            validate_identifier(src);
            if (const auto var = variables.find(src); var != variables.end()) {
                output.push_back(var->second);
            } else {
                throw RuntimeError(pc, "you're printing nothing");
            }
            break;
        }
        case Instruction::Assign: {
            std::string dest = parameters[0], src = parameters[1];
            parse_reference(dest) = parse_value(src);
            break;
        }
        case Instruction::Add: {
            std::string dest = parameters[0], src = parameters[1];
            parse_reference(dest) += parse_value(src);
            break;
        }
        case Instruction::Sub: {
            std::string dest = parameters[0], src = parameters[1];
            parse_reference(dest) -= parse_value(src);
            break;
        }
        case Instruction::Compare: {
            std::string dest = parameters[0], src = parameters[1];
            if (!(parse_value(dest) > parse_value(src))) {
                next_pc = pc + 2;
            }
            break;
        }
        case Instruction::Jump: {
            std::string src = parameters[0];
            next_pc = parse_integer_literal(src) - 1;
            break;
        }
        case Instruction::Return: {
            returned = true;
            break;
        }
        case Instruction::Undefined: {
            throw RuntimeError(pc, "that's not even a line");
        }
        }
        pc = next_pc;
    }

    std::vector<Instruction> instructions;
    // we are handling arbitrary strings from untrusted users.
    // using siphash makes it much more difficult to generate collisions
    // compared to std::hash.
    std::unordered_map<std::string, variable, hash_lib::SipHasher<1, 3>>
        variables;
    std::vector<variable> input, output;
    int pc;
    bool returned;
};

} // namespace based
