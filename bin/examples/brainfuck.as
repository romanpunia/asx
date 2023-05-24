#include <std/array.as>
#include <std/console.as>
#include <std/os.as>

/* From https://github.com/kgabis/brainfuck-c/blob/master/brainfuck.c */
enum opcode
{
    ret,
    incdp,
    decdp,
    incval,
    decval,
    output,
    input,
    jmp_ift,
    jmp_iff
}

class instruction
{
    uint16 operator;
    uint16 operand;

    instruction() { operator = 0; operand = 0; }
    instruction(uint16 new_operator) { operator = new_operator; operand = 0; }
    instruction(uint16 new_operator, uint16 new_operand) { operator = new_operator; operand = new_operand; }
}

class context
{
    instruction[] program;
    usize[] stack;
    uint8[] data;

    bool compile(const string&in code)
    {
        usize jump_offset = 0;
        usize size = code.size();
        program.reserve(size);

        for (usize i = 0; i < size; i++)
        {
            switch (code[i])
            {
                case '>': program.push(instruction(opcode::incdp)); break;
                case '<': program.push(instruction(opcode::decdp)); break;
                case '+': program.push(instruction(opcode::incval)); break;
                case '-': program.push(instruction(opcode::decval)); break;
                case '.': program.push(instruction(opcode::output)); break;
                case ',': program.push(instruction(opcode::input)); break;
                case '[':
                    stack.push(program.size());
                    program.push(instruction(opcode::jmp_ift));
                    break;
                case ']':
                    if (stack.empty())
                        return false;   
                    jump_offset = stack.back(); stack.pop();
                    program[jump_offset].operand = program.size();
                    program.push(instruction(opcode::jmp_iff, jump_offset));
                    break;
                default: break;
            }
        }
        
        if (!stack.empty())
            return false;
        
        program.push(instruction(opcode::ret));
        return true;
    }
    bool execute(console@ output, usize data_size)
    {
        if (program.empty())
            return true;

        data.clear();
        data.resize(data_size); // Will do memset zero

        usize data_ptr = 0;
        usize program_ptr = 0;
        while (true)
        {
            instruction@ next = program[program_ptr]; // Equal to cpp: instruction&
            if (next.operator == opcode::ret || data_ptr >= data_size)
                break;
            
            switch (next.operator)
            {
                case opcode::incdp: ++data_ptr; break;
                case opcode::decdp: --data_ptr; break;
                case opcode::incval: ++data[data_ptr]; break;
                case opcode::decval: --data[data_ptr]; break;
                case opcode::output: output.write_char(data[data_ptr]); break;
                case opcode::input: data[data_ptr] = output.read_char(); break;
                case opcode::jmp_ift: if (data[data_ptr] == 0) program_ptr = next.operand; break;
                case opcode::jmp_iff: if (data[data_ptr] != 0) program_ptr = next.operand; break;
                default: return false;
            }
            program_ptr++;
        }

        return data_ptr != data_size ? true : false;
    }
}

int main(string[]@ args)
{
    console@ output = console::get();
    if (args.size() != 2 || !os::file::is_exists(args[1]))
    {
        output.write_line("provide a brainfuck program code");
        return 1;
    }

    context program;
    if (!program.compile(os::file::read_as_string(args[1])))
    {
        output.write_line("cannot compile brainfuck program code");
        return 2;
    }

    if (!program.execute(@output, 65535))
    {
        output.write_line("cannot execute brainfuck program code");
        return 3;
    }

    return 0;
}