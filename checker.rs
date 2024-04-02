use std::collections::{hash_map::Entry, HashMap, VecDeque};
use std::convert::{TryFrom, TryInto};
use std::io::{BufRead, BufReader};

#[derive(Clone)]
enum Operand {
    Constant(i64),
    Variable(String),
    ArrayConstIndex(String, usize),
    ArrayVarIndex(String, String),
}

#[derive(Clone)]
enum Instruction {
    Nop(),
    Input(Operand),
    Output(Operand),
    Assign(Operand, Operand),
    Add(Operand, Operand),
    Sub(Operand, Operand),
    Compare(Operand, Operand),
    Jump(Operand),
    Return(),
}

#[derive(Clone, Debug)]
enum Variable {
    Integer(i64),
    Array(Vec<i64>),
}

#[derive(Clone, Default)]
struct Program {
    instructions: Vec<Instruction>,
    costs: Vec<usize>,
    variables: HashMap<String, Variable>,
    input: VecDeque<Variable>,
    output: VecDeque<Variable>,
    runtime: usize,
    pc: usize,
    returned: bool,
}

#[derive(Debug)]
enum Verdict {
    Correct(),
    WrongAnswer(String),
    TimeLimitExceeded(),
    RuntimeError(usize, String),
    CompileError(usize, String),
    Based(),
    OtherError(String),
}

struct CheckerFail(String);

fn compress(string: &str) -> String {
    const LIMIT: usize = 32;
    let prefix = string.chars().take(LIMIT + 1);
    if prefix.clone().count() <= LIMIT {
        prefix.collect()
    } else {
        prefix.take(LIMIT - 3).collect::<String>() + "..."
    }
}

impl<E> From<E> for CheckerFail
where
    E: std::error::Error,
{
    fn from(error: E) -> Self {
        CheckerFail(format!("{:?}", error))
    }
}

impl TryFrom<&str> for Operand {
    type Error = String;
    fn try_from(string: &str) -> Result<Self, Self::Error> {
        fn parse_array_index(string: &str) -> Option<(&str, &str)> {
            let (part1, part23) = string.split_once('[')?;
            let (part2, part3) = part23.split_once(']')?;
            part3.is_empty().then_some((part1, part2))
        }
        fn is_identifier(string: &str) -> bool {
            let mut chars = string.chars();
            chars
                .next()
                .is_some_and(|c| c.is_ascii_alphabetic() || c == '_')
                && chars.all(|c| c.is_ascii_alphanumeric() || c == '_')
        }
        if let Some((array, index)) = parse_array_index(string) {
            if is_identifier(array) {
                if let Ok(value) = index.parse() {
                    Ok(Operand::ArrayConstIndex(array.to_string(), value))
                } else if is_identifier(index) {
                    Ok(Operand::ArrayVarIndex(array.to_string(), index.to_string()))
                } else {
                    Err(format!(
                        "cannot parse index '{}', should be integer or identifier",
                        compress(index)
                    ))
                }
            } else {
                Err(format!(
                    "invalid identifier '{}', should contain only letters, numbers, and '_', \
                    and cannot begin with a number",
                    compress(array)
                ))
            }
        } else if let Ok(value) = string.parse() {
            Ok(Operand::Constant(value))
        } else if is_identifier(string) {
            Ok(Operand::Variable(string.to_string()))
        } else {
            Err(format!(
                "cannot parse operand '{}', should be one of: \
                integer, identifier, identifier[integer], identifier[identifier]",
                compress(string)
            ))
        }
    }
}

impl TryFrom<&str> for Instruction {
    type Error = String;
    fn try_from(string: &str) -> Result<Self, Self::Error> {
        use Instruction::*;
        let tokens = string.split_whitespace().collect::<Vec<_>>();
        match tokens[..] {
            [] => Ok(Nop()),
            ["yoink", dst] => Ok(Input(dst.try_into()?)),
            ["yeet", src] => Ok(Output(src.try_into()?)),
            ["bruh", dst, "is", "lowkey", "just", src] => {
                Ok(Assign(dst.try_into()?, src.try_into()?))
            }
            ["*slaps", src, "on", "top", "of", dst] if dst.ends_with('*') => {
                Ok(Add(dst[..dst.len() - 1].try_into()?, src.try_into()?))
            }
            ["rip", "this", dst, "fell", "off", "by", src] => {
                Ok(Sub(dst.try_into()?, src.try_into()?))
            }
            ["vibe", "check", dst, "ratios", src] => Ok(Compare(dst.try_into()?, src.try_into()?)),
            ["simp", "for", src] => Ok(Jump(src.try_into()?)),
            ["go", "touch", "some", "grass"] => Ok(Return()),
            _ => Err(format!("unknown expression: '{}'", compress(string))),
        }
    }
}

impl Program {
    const INSTRUCTION_BASE_COST: usize = 5;
    fn compile(lines: &[String]) -> Result<Program, Verdict> {
        let mut prog = Program::default();
        for (lineno, line) in lines.iter().enumerate() {
            if line.to_lowercase().find("based").is_some() {
                return Err(Verdict::Based());
            }
            match line.as_str().try_into() {
                Ok(instruction) => {
                    prog.instructions.push(instruction);
                    prog.costs.push(line.len() + Self::INSTRUCTION_BASE_COST);
                }
                Err(message) => {
                    return Err(Verdict::CompileError(lineno, message));
                }
            }
        }
        Ok(prog)
    }
    fn get_int_mut(&mut self, var_name: &str) -> Result<&mut i64, Verdict> {
        match self.variables.get_mut(var_name) {
            Some(Variable::Integer(value)) => Ok(value),
            Some(Variable::Array(_)) => Err(Verdict::RuntimeError(
                self.pc,
                format!("expected integer, found array {}", compress(var_name)),
            )),
            None => Err(Verdict::RuntimeError(
                self.pc,
                format!("no such variable {}", compress(var_name)),
            )),
        }
    }
    fn get_int_mut_or_default(&mut self, var_name: &str) -> Result<&mut i64, Verdict> {
        match self.variables.entry(var_name.to_string()) {
            Entry::Occupied(entry) => match entry.into_mut() {
                Variable::Integer(value) => Ok(value),
                Variable::Array(_) => Err(Verdict::RuntimeError(
                    self.pc,
                    format!("expected integer, found array {}", compress(var_name)),
                )),
            },
            Entry::Vacant(entry) => match entry.insert(Variable::Integer(0)) {
                Variable::Integer(value) => Ok(value),
                _ => unreachable!(),
            },
        }
    }
    fn get_arr_mut(&mut self, var_name: &str) -> Result<&mut [i64], Verdict> {
        match self.variables.get_mut(var_name) {
            Some(Variable::Array(value)) => Ok(value),
            Some(Variable::Integer(_)) => Err(Verdict::RuntimeError(
                self.pc,
                format!("expected array, found integer {}", compress(var_name)),
            )),
            None => Err(Verdict::RuntimeError(
                self.pc,
                format!("no such variable {}", compress(var_name)),
            )),
        }
    }
    fn get_value(&mut self, operand: &Operand) -> Result<i64, Verdict> {
        match operand {
            Operand::Constant(value) => Ok(*value),
            Operand::Variable(var) => self.get_int_mut(var).copied(),
            Operand::ArrayConstIndex(array, index) => {
                let lineno = self.pc;
                let array = self.get_arr_mut(array)?;
                array.get(*index).copied().ok_or_else(|| {
                    Verdict::RuntimeError(lineno, format!("index {} out of bounds", index))
                })
            }
            Operand::ArrayVarIndex(array, index) => {
                let lineno = self.pc;
                let index = self.get_int_mut(index).copied()? as usize;
                let array = self.get_arr_mut(array)?;
                array.get(index).copied().ok_or_else(|| {
                    Verdict::RuntimeError(lineno, format!("index {} out of bounds", index))
                })
            }
        }
    }
    fn get_reference_mut(&mut self, operand: &Operand) -> Result<&mut i64, Verdict> {
        match operand {
            Operand::Constant(value) => Err(Verdict::RuntimeError(
                self.pc,
                format!("integer constant {} is not &mut i64", value),
            )),
            Operand::Variable(var) => self.get_int_mut_or_default(var),
            Operand::ArrayConstIndex(array, index) => {
                let lineno = self.pc;
                let array = self.get_arr_mut(array)?;
                array.get_mut(*index).ok_or_else(|| {
                    Verdict::RuntimeError(lineno, format!("index {} out of bounds", index))
                })
            }
            Operand::ArrayVarIndex(array, index) => {
                let lineno = self.pc;
                let index = self.get_int_mut(index).copied()? as usize;
                let array = self.get_arr_mut(array)?;
                array.get_mut(index).ok_or_else(|| {
                    Verdict::RuntimeError(lineno, format!("index {} out of bounds", index))
                })
            }
        }
    }
    fn add_input(&mut self, variable: Variable) {
        self.input.push_back(variable)
    }
    fn get_output(&mut self) -> Option<Variable> {
        self.output.pop_front()
    }
    fn has_output(&self) -> bool {
        !self.output.is_empty()
    }
    fn execute_one(&mut self) -> Result<(), Verdict> {
        let cur_pc = self.pc;
        let mut next_pc = cur_pc + 1;
        let Some(instruction) = self.instructions.get(cur_pc) else {
            return Err(Verdict::RuntimeError(
                cur_pc,
                format!("that's not even a line"),
            ));
        };
        self.runtime = self.runtime.saturating_add(self.costs[cur_pc]);
        match instruction.clone() {
            Instruction::Nop() => {}
            Instruction::Input(dst) => {
                let Operand::Variable(var) = dst else {
                    return Err(Verdict::RuntimeError(
                        cur_pc,
                        format!("input operand must be an identifier"),
                    ));
                };
                let Some(input) = self.input.pop_front() else {
                    return Err(Verdict::RuntimeError(
                        cur_pc,
                        format!("you're reading from nothing"),
                    ));
                };
                self.variables.insert(var.clone(), input);
            }
            Instruction::Output(src) => {
                if let Operand::Variable(ref var) = src {
                    let Some(value) = self.variables.get(var) else {
                        return Err(Verdict::RuntimeError(
                            cur_pc,
                            format!("you're printing nothing"),
                        ));
                    };
                    self.output.push_back(value.clone());
                } else {
                    let output = self.get_value(&src)?;
                    self.output.push_back(Variable::Integer(output));
                }
            }
            Instruction::Assign(dst, src) => {
                *self.get_reference_mut(&dst)? = self.get_value(&src)?;
            }
            Instruction::Add(dst, src) => {
                *self.get_reference_mut(&dst)? += self.get_value(&src)?;
            }
            Instruction::Sub(dst, src) => {
                *self.get_reference_mut(&dst)? -= self.get_value(&src)?;
            }
            Instruction::Compare(dst, src) => {
                let dst = self.get_value(&dst)?;
                let src = self.get_value(&src)?;
                if !(dst > src) {
                    next_pc = cur_pc + 2;
                }
            }
            Instruction::Jump(dst) => {
                let Operand::Constant(line) = dst else {
                    return Err(Verdict::RuntimeError(
                        cur_pc,
                        format!("simp operand must be a constant"),
                    ));
                };
                next_pc = (line - 1) as usize;
            }
            Instruction::Return() => {
                self.returned = true;
            }
        }
        self.pc = next_pc;
        Ok(())
    }
    fn execute(&mut self, time_limit: usize) -> Result<(), Verdict> {
        loop {
            if self.returned {
                return Ok(());
            }
            if self.runtime > time_limit {
                return Err(Verdict::TimeLimitExceeded());
            }
            self.execute_one()?;
        }
    }
}

struct Pcg128 {
    state: u128,
    increment: u128,
}

impl Pcg128 {
    const MULTIPLIER: u128 = 0x2360ed051fc65da44385df649fccf645;
    fn new(state: u128, stream: u128) -> Self {
        let increment = (stream << 1) | 1;
        Pcg128 { state, increment }
    }
    fn mix(state: u128) -> u64 {
        const SHIFT: u32 = 64;
        const ROTATE: u32 = 122;
        let rot = (state >> ROTATE) as u32;
        let xsh = ((state >> SHIFT) as u64) ^ (state as u64);
        xsh.rotate_right(rot)
    }
    fn next(&mut self) -> u64 {
        self.state = self
            .state
            .wrapping_mul(Self::MULTIPLIER)
            .wrapping_add(self.increment);
        Self::mix(self.state)
    }
    fn next_signed(&mut self, bits: u32) -> i64 {
        self.next() as i64 >> (64 - bits)
    }
}

trait Task {
    fn prepare_test_case(&self, program: &mut Program, rng: &mut Pcg128) -> i64;
    fn run_and_check(&self, mut program: Program, rng: &mut Pcg128, time_limit: usize) -> Verdict {
        let answer = self.prepare_test_case(&mut program, rng);
        if let Err(error) = program.execute(time_limit) {
            return error;
        }
        match program.get_output() {
            Some(Variable::Integer(output)) => {
                if program.has_output() {
                    Verdict::WrongAnswer(format!("too much stuff printed"))
                } else if output != answer {
                    Verdict::WrongAnswer(format!("git gud"))
                } else {
                    Verdict::Correct()
                }
            }
            Some(_) => Verdict::WrongAnswer(format!("U PRINTERD AN ENTRIE ARRAY???")),
            None => Verdict::WrongAnswer(format!("print something")),
        }
    }
}

struct Task1();

impl Task for Task1 {
    fn prepare_test_case(&self, program: &mut Program, rng: &mut Pcg128) -> i64 {
        let a = rng.next_signed(60);
        let b = rng.next_signed(60);
        program.add_input(Variable::Integer(a));
        program.add_input(Variable::Integer(b));
        a + b
    }
}

struct Task2();

impl Task for Task2 {
    fn prepare_test_case(&self, program: &mut Program, rng: &mut Pcg128) -> i64 {
        let a = rng.next_signed(60);
        program.add_input(Variable::Integer(a));
        a.abs()
    }
}

struct Task3(usize);

impl Task for Task3 {
    fn prepare_test_case(&self, program: &mut Program, rng: &mut Pcg128) -> i64 {
        let n = self.0;
        let mut a = Vec::new();
        a.resize_with(n, || rng.next_signed(60));
        let answer = *a.iter().max().unwrap();
        program.add_input(Variable::Integer(n as i64));
        program.add_input(Variable::Array(a));
        answer
    }
}

struct Task4(usize);

impl Task for Task4 {
    fn prepare_test_case(&self, program: &mut Program, rng: &mut Pcg128) -> i64 {
        let n = self.0;
        let mut a = Vec::new();
        let k = rng.next() as usize % n + 1;
        a.resize_with(n, || rng.next_signed(60));
        let answer = *a.clone().select_nth_unstable(n - k).1;
        program.add_input(Variable::Integer(n as i64));
        program.add_input(Variable::Array(a));
        program.add_input(Variable::Integer(k as i64));
        answer
    }
}

fn judge(task: i32, filename: &str) -> Result<Verdict, CheckerFail> {
    let reader = BufReader::new(std::fs::File::open(filename)?);
    let program = match Program::compile(&reader.lines().collect::<Result<Vec<_>, _>>()?) {
        Ok(program) => program,
        Err(compile_error) => return Ok(compile_error),
    };
    let mut rng = Pcg128::new(0xcafef00dd15ea5e5, 0xa02bdbf7bb3c0a7ac28fa16a64abf96);
    match task {
        1 => {
            for _ in 0..10 {
                match Task1().run_and_check(program.clone(), &mut rng, 100000) {
                    Verdict::Correct() => continue,
                    verdict => return Ok(verdict),
                }
            }
        }
        2 => {
            for _ in 0..10 {
                match Task2().run_and_check(program.clone(), &mut rng, 100000) {
                    Verdict::Correct() => continue,
                    verdict => return Ok(verdict),
                }
            }
        }
        3 => {
            for n in 1..=50 {
                match Task3(n).run_and_check(program.clone(), &mut rng, 100000) {
                    Verdict::Correct() => continue,
                    verdict => return Ok(verdict),
                }
            }
        }
        4 => {
            for n in 1..=50 {
                for _ in 0..(25 / n + 1) {
                    match Task4(n).run_and_check(program.clone(), &mut rng, 2500000) {
                        Verdict::Correct() => continue,
                        verdict => return Ok(verdict),
                    }
                }
            }
        }
        _ => {
            return Err(CheckerFail(format!("unknown task id {}", task)));
        }
    }
    Ok(Verdict::Correct())
}

fn work() -> Result<Verdict, CheckerFail> {
    let argv = std::env::args().collect::<Vec<_>>();
    let [inf, ouf, ans] = &argv[1..=3] else {
        panic!("not enough args");
    };
    let task = std::fs::read_to_string(inf)?.trim().parse()?;
    match judge(task, ans) {
        Ok(Verdict::Correct()) => match judge(task, ouf) {
            Ok(verdict) => Ok(verdict),
            Err(error) => Ok(Verdict::OtherError(error.0)),
        },
        Ok(verdict) => Err(CheckerFail(format!(
            "jury's solution failed with verdict {:?}",
            verdict
        ))),
        Err(error) => Err(error),
    }
}

fn main() {
    use Verdict::*;
    match work() {
        Err(CheckerFail(message)) => {
            eprintln!("CHECKER ERROR author made the oopsie: {}", message);
            std::process::exit(3);
        }
        Ok(Correct()) => {
            eprintln!("ur the GOAT of based code!!1!",);
            std::process::exit(0);
        }
        Ok(WrongAnswer(message)) => {
            eprintln!("this ain't it, chief, {}", message);
            std::process::exit(1);
        }
        Ok(TimeLimitExceeded()) => {
            eprintln!("you have skill issue on speed smh");
            std::process::exit(1);
        }
        Ok(RuntimeError(line, message)) => {
            eprintln!(
                "ya code got L + ratioed on line {} because {}",
                line, message
            );
            std::process::exit(1);
        }
        Ok(CompileError(line, message)) => {
            eprintln!(
                "jesse, what are you talking about on line {}? {}",
                line, message
            );
            std::process::exit(1);
        }
        Ok(Based()) => {
            eprintln!(
                r#""Based"? Are you kidding me? I spent a decent portion of my life preparing this problem and your submission to it is "Based"? What do I have to say to you? Absolutely nothing. I couldn't be bothered to respond to such meaningless attempt at writing code. Do you want "Based" on your Codeforces profile?"#,
            );
            std::process::exit(1);
        }
        // polygon does weird things...
        Ok(OtherError(message)) => {
            eprintln!("unexpected error in participant output: {}", message);
            std::process::exit(1);
        }
    }
}
