#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector> // For using vector
#include <sstream>
#include <map>
#include <bitset>

using namespace std;

vector<string> instructionMemory;
int dataMemory[1024] = {0};
int GPR[32];
int PC = 0;
unordered_set<int> usedRegisters;

class controlWord
{
public:
    int RegRead, RegWrite, ALUSrc, ALUOp, Branch, Jump, MemRead, MemWrite, Mem2Reg;

    controlWord(int rr = 0, int rw = 0, int as = 0, int ao = 0, int b = 0, int j = 0, int mr = 0, int mw = 0, int m2r = 0)
        : RegRead(rr), RegWrite(rw), ALUSrc(as), ALUOp(ao), Branch(b), Jump(j), MemRead(mr), MemWrite(mw), Mem2Reg(m2r) {}
};

map<string, controlWord> controlUnit;

void initControlUnit()
{
    controlUnit["0110011"] = controlWord(1, 1, 0, 10, 0, 0, 0, 0, 0);  // R-type (e.g., add, sub, and, or)
    controlUnit["0010011"] = controlWord(1, 1, 1, 11, 0, 0, 0, 0, 0);  // I-type (e.g., addi, ori, andi)
    controlUnit["0000011"] = controlWord(1, 1, 1, 00, 0, 0, 1, 0, 1);  // I-type (Load, e.g., lw)
    controlUnit["0100011"] = controlWord(1, 0, 1, 00, 0, 0, 0, 1, -1); // S-type (Store, e.g., sw)
    controlUnit["1100011"] = controlWord(1, 0, 0, 01, 1, 0, 0, 0, -1); // B-type (Branch, e.g., beq, bne)
    controlUnit["0110111"] = controlWord(0, 1, 1, 00, 0, 0, 0, 0, 0);  // U-type (e.g., lui, auipc)
    controlUnit["1101111"] = controlWord(0, 1, 0, 00, 0, 1, 0, 0, 0);  // J-type (e.g., jal)
}

// IFID class to store IR, DPC, and NPC
class IFID
{
public:
    string IR;
    int DPC;
    int NPC;
    bool valid;
    IFID() : IR(""), DPC(0), NPC(0), valid(false) {}
};

// IDEX class to store values after the Instruction Decode (ID) stage
class IDEX
{
public:
    int JPC;  // Jump Program Counter
    int DPC;  // Delayed Program Counter
    int imm1; // Immediate value
    string imm2;
    int func;       // Function code (bits 14-12)
    int rdl;        // Destination register (bits 11-7)
    controlWord CW; // Control signals (from control unit)
    int rs1;        // Value of source register 1
    int rs2;        // Value of source register 2 or immediate value
    int func7;      // funct7
    string opcode;
    bool valid;
    int rsl1, rsl2;

    IDEX() : imm2(""), opcode(""), JPC(0), DPC(0), imm1(0), func(0), rdl(0), rs1(0), rs2(0), func7(-1), valid(false), rsl1(0), rsl2(0) {}
};

class EXMO
{
public:
    int ALUOUT;     // ALU output
    controlWord CW; // Control signals from the decode stage
    int rdl;
    int rs2;
    bool valid = false;
};

class MOWB
{
public:
    controlWord CW;
    int ALUOUT;
    int LDOUT;
    int rdl;
    bool valid = false;
};

class PipelineControl
{
public:
    bool flush_if; // Flush Instruction Fetch stage
    bool flush_id; // Flush Instruction Decode stage
    bool stall;    // Stall pipeline if needed

    PipelineControl() : flush_if(false), flush_id(false), stall(false) {}
};

int to_int(string binaryStr)
{
    int decimalValue = 0;
    int length = binaryStr.length();
    bool isNegative = (binaryStr[0] == '1');

    if (!isNegative)
    {
        for (int i = 0; i < length; i++)
        {
            char bit = binaryStr[length - 1 - i];

            if (bit == '1')
            {
                decimalValue += (1 << i);
            }
        }
    }
    else
    {
        for (int i = 0; i < length; i++)
        {
            char bit = binaryStr[length - 1 - i];

            if (bit == '1' && i != length - 1)
            {
                decimalValue += (1 << i);
            }
        }

        decimalValue -= (1 << (length - 1));
    }

    return decimalValue;
}

string to_str(int num)
{
    cout << num << endl;
    bitset<32> binary(num);
    return binary.to_string();
}

int SignedExtend(string imm)
{
    string extended_imm = imm;
    if (imm[0] == '0')
    {
        extended_imm = "000000000000" + extended_imm;
    }
    else
    {
        extended_imm = "111111111111" + extended_imm;
    }

    return to_int(extended_imm);
}

// ALU Control Logic
string ALUControl(int ALUOp, int func, int func7)
{
    string ALUSelect;

    switch (ALUOp)
    {
    case 00:                // Load/Store (for I-type, S-type)
        ALUSelect = "0010"; // Perform ADD for address calculation
        break;
    case 01: // Branch (for B-type)
        if (func == 0 || func == 1 || func == 4 || func == 5)
        {                       // beq, bne, blt, bge
            ALUSelect = "0110"; // Perform SUB for comparison
        }
        else
        {
            ALUSelect = "1111"; // Undefined
        }
        break;
    case 10: // R-type instructions
        if (func == 0)
        {                                             // ADD
            ALUSelect = func7 == 0 ? "0010" : "0110"; // ADD or SUB
        }
        else if (func == 1)
        {                       // SLL
            ALUSelect = "0011"; // Shift left
        }
        else if (func == 7)
        { // AND
            ALUSelect = "0000";
        }
        else if (func == 4)
        {
            ALUSelect = "11111";
        }
        else if (func == 6)
        { // OR
            ALUSelect = "0001";
        }
        else
        {
            ALUSelect = "1111"; // Undefined
        }
        break;
    case 11: // I-type instructions
        if (func == 0)
        {                       // ADDI
            ALUSelect = "0010"; // ADD immediate
        }
        else if (func == 2)
        {                       // SLTI
            ALUSelect = "0111"; // Set less than immediate
        }
        else if (func == 6)
        { // ORI
            ALUSelect = "0001";
        }
        else if (func == 4)
        {                        // XORI
            ALUSelect = "11111"; // XOR immediate
        }
        else
        {
            ALUSelect = "1111"; // Undefined
        }
        break;
    default:
        ALUSelect = "1111"; // Undefined operation
        break;
    }

    return ALUSelect;
}

// ALU Function
int ALU(string ALUSelect, int rs1, int rs2)
{
    int result = 0;

    if (ALUSelect == "0000")
        result = rs1 & rs2; // AND
    else if (ALUSelect == "0001")
        result = rs1 | rs2; // OR
    else if (ALUSelect == "0010")
        result = rs1 + rs2; // ADD
    else if (ALUSelect == "0110")
        result = rs1 - rs2; // SUB
    else if (ALUSelect == "0011")
        result = rs1 << (rs2 & 0x1F); // SLL
    else if (ALUSelect == "0100")
        result = rs1 >> (rs2 & 0x1F); // SRL
    else if (ALUSelect == "0101")
        result = rs1 >> (rs2 & 0x1F); // SRA
    else if (ALUSelect == "11111")
        result = rs1 ^ rs2; // XOR
    else if (ALUSelect == "0111")
        result = (rs1 < rs2) ? 1 : 0; // SLT (Set Less Than)
    else if (ALUSelect == "1000")
        result = ((unsigned int)rs1 < (unsigned int)rs2) ? 1 : 0; // SLTU (Set Less Than Unsigned
    else if (ALUSelect == "1111")
    {
        std::cerr << "Error: Undefined ALU operation." << std::endl;
        return -1;
    }
    return result;
}

void instructionExecution(IDEX *idex, EXMO *exmo, MOWB *mowb, PipelineControl *ctrl)
{
    cout << "\n=== EXECUTION STAGE ===" << endl;
    cout << "Executing instruction with opcode: " << idex->opcode << endl;
    bool branch_taken = false;
    int branch_target = 0;
    if (idex->CW.Jump && idex->opcode == "1101111")
    {
        cout << "Jump instruction detected" << endl;
        cout << "Current DPC: " << idex->DPC << endl;
        idex->rdl = idex->DPC + 4;
        PC = idex->JPC;
        cout << "Jump target calculated - New PC: " << PC << endl;
        cout << "Return address stored in rdl(x" << idex->rdl << ")" << endl;

        ctrl->flush_if = true;
        ctrl->flush_id = true;
        cout << "Pipeline flush initiated" << endl;
        return;
    }
    cout << "ALU Control Signals:" << endl;
    cout << "ALUOp: " << idex->CW.ALUOp << endl;
    cout << "Function code (func3): " << idex->func << endl;
    cout << "Function7: " << idex->func7 << endl;
    string ALUSelect = ALUControl(idex->CW.ALUOp, idex->func, idex->func7);
    cout << "Aluselect: " << ALUSelect << endl;

    // Forward data if needed
    if (mowb->CW.MemRead)
    {
        if (mowb->rdl == idex->rsl1)
        {
            cout << "Forwarding Memory data to rs1 (x" << idex->rsl1 << "): " << mowb->LDOUT << endl;
            idex->rs1 = mowb->LDOUT;
        }
        if (mowb->rdl == idex->rsl2)
        {
            cout << "Forwarding Memory data to rs2 (x" << idex->rsl2 << "): " << mowb->LDOUT << endl;
            idex->rs2 = mowb->LDOUT;
        }
    }

    string i;
    if (idex->opcode == "0100011")
    { // S type
        cout << "Store instruction detected" << endl;
        exmo->ALUOUT = ALU(ALUSelect, idex->rs1, to_int(idex->imm2));
        cout << "Store address calculated: " << exmo->ALUOUT << endl;
    }
    else if (idex->opcode == "1100011")
    { // B type
        cout << "Branch instruction detected" << endl;
        i = (idex->imm2).substr(0, 1) + (idex->imm2).substr(11, 1) +
            (idex->imm2).substr(1, 6) + (idex->imm2).substr(7, 4);
        cout << "Branch immediate (binary): " << i << endl;
        exmo->ALUOUT = ALU(ALUSelect, idex->rs1, idex->rs2);
        cout << "Branch comparison result: " << exmo->ALUOUT << endl;
    }
    else
    {
        cout << "Regular ALU operation" << endl;
        cout << "Operand 1 (rs1): " << idex->rs1 << endl;
        cout << "Operand 2 (rs2/imm): " << idex->rs2 << endl;
        exmo->ALUOUT = ALU(ALUSelect, idex->rs1, idex->rs2);
        cout << "ALU Result: " << exmo->ALUOUT << endl;
    }

    exmo->CW = idex->CW;

    if (idex->CW.Branch)
    {
        int offset = to_int(i);
        cout << "\nBranch Processing:" << endl;
        cout << "Branch offset (decimal): " << offset << endl;

        switch (idex->func)
        {
        case 0:
            cout << "BEQ instruction" << endl;
            branch_taken = (exmo->ALUOUT == 0);
            break;
        case 1:
            cout << "BNE instruction" << endl;
            branch_taken = (exmo->ALUOUT != 0);
            break;
        case 4:
            cout << "BLT instruction" << endl;
            branch_taken = (exmo->ALUOUT < 0);
            break;
        case 5:
            cout << "BGE instruction" << endl;
            branch_taken = (exmo->ALUOUT >= 0);
            break;
        }

        cout << "Branch condition: " << (branch_taken ? "Taken" : "Not Taken") << endl;

        if (branch_taken)
        {
            int new_pc = offset * 4 + idex->DPC;
            cout << "Branch taken - New PC calculation:" << endl;
            cout << "Offset: " << offset << " * 4 + DPC: " << idex->DPC << " = " << new_pc << endl;
            PC = new_pc;
            ctrl->flush_if = true;
            ctrl->flush_id = true;
            cout << "Pipeline flush initiated" << endl;
        }
    }
    exmo->rdl = idex->rdl;
    exmo->rs2 = idex->rs2;

    cout << "Final execution results:" << endl;
    cout << "ALU Output: " << exmo->ALUOUT << endl;
    cout << "Destination Register (rd): x" << exmo->rdl << endl;
    cout << "Program Counter: " << PC << endl;
}

void instructionFetch(IFID *ifid, PipelineControl *ctrl)
{
    cout << "\n=== INSTRUCTION FETCH STAGE ===" << endl;
    cout << "Current PC: " << PC << endl;

    if (ctrl->flush_if)
    {
        cout << "Pipeline Flush detected in Fetch stage" << endl;
        cout << "Inserting NOP instruction" << endl;
        ifid->IR = "00000000000000000000000000000000";
        ifid->valid = false;
        ctrl->flush_if = false;
        cout << "Fetch stage flushed" << endl;
        return;
    }

    if (PC / 4 < instructionMemory.size())
    {
        ifid->IR = instructionMemory[PC / 4];
        cout << "Fetched instruction from address " << PC << ":" << endl;
        cout << "Binary: " << ifid->IR << endl;

        // Parse opcode for more informative logging
        string opcode = ifid->IR.substr(25, 7);
        cout << "Opcode: " << opcode << " (";
        if (opcode == "0110011")
            cout << "R-type";
        else if (opcode == "0010011")
            cout << "I-type";
        else if (opcode == "0000011")
            cout << "Load";
        else if (opcode == "0100011")
            cout << "Store";
        else if (opcode == "1100011")
            cout << "Branch";
        else if (opcode == "0110111")
            cout << "U-type";
        else if (opcode == "1101111")
            cout << "JAL";
        cout << ")" << endl;
    }
    else
    {
        cout << "Warning: Program counter (" << PC << ") out of instruction memory bounds!" << endl;
        cout << "Instruction memory size: " << instructionMemory.size() << " instructions" << endl;
    }

    ifid->DPC = PC;
    ifid->NPC = PC + 4;

    cout << "Fetch stage complete:" << endl;
    cout << "Current PC (DPC): " << ifid->DPC << endl;
    cout << "Next PC (NPC): " << ifid->NPC << endl;

    PC += 4;
    cout << "Updated PC: " << PC << endl;
}

// Function for the Instruction Decode (ID) stage
void instructionDecode(IFID *ifid, IDEX *idex, MOWB *mowb, EXMO *exmo, PipelineControl *ctrl)
{
    cout << "\n=== INSTRUCTION DECODE STAGE ===" << endl;
    if (ctrl->flush_id)
    {
        cout << "Pipeline Flush detected in Decode stage" << endl;
        idex->valid = false;
        ctrl->flush_id = false;
        cout << "Decode stage flushed" << endl;
        return;
    }
    string instruction = ifid->IR;
    cout << "Decoding instruction: " << instruction << endl;
    string i;
    i = instruction.substr(0, 1) + instruction.substr(12, 8) + instruction.substr(11, 1) + instruction.substr(1, 10);
    idex->JPC = ifid->NPC + 4 * SignedExtend(i);
    idex->DPC = ifid->DPC;
    idex->imm1 = to_int(instruction.substr(0, 12));
    idex->imm2 = instruction.substr(0, 7) + instruction.substr(20, 5);
    idex->func = stoi(instruction.substr(17, 3), nullptr, 2);
    idex->rdl = to_int(instruction.substr(20, 5));
    idex->CW = controlUnit[instruction.substr(25, 7)];
    idex->func7 = to_int(instruction.substr(0, 7));
    idex->opcode = instruction.substr(25, 7);
    idex->rsl1 = to_int(instruction.substr(12, 5));
    idex->rsl2 = to_int(instruction.substr(7, 5));

    cout << "\nDecoded fields:" << endl;
    cout << "func3: " << idex->func << endl;
    cout << "func7: " << idex->func7 << endl;
    cout << "Destination register (rd): x" << idex->rdl << endl;
    cout << "Source register 1 (rs1): x" << idex->rsl1 << endl;
    cout << "Source register 2 (rs2): x" << idex->rsl2 << endl;

    // store used reg in set
    usedRegisters.insert(idex->rdl);
    usedRegisters.insert(idex->rsl1);
    usedRegisters.insert(idex->rsl2);

    if (idex->CW.RegRead)
    {
        cout << "\nRegister Read Stage:" << endl;

        // RS1 handling
        cout << "Reading RS1 (x" << idex->rsl1 << "):" << endl;
        if (exmo->rdl == idex->rsl1)
        {
            cout << "Forwarding from EX stage for rs1" << endl;
            idex->rs1 = exmo->ALUOUT;
            cout << "Forwarded value: " << idex->rs1 << endl;
        }
        else if (mowb->rdl == idex->rsl1)
        {
            cout << "Forwarding from WB stage for rs1" << endl;
            if (mowb->CW.MemRead)
            {
                idex->rs1 = mowb->LDOUT;
                cout << "Forwarded memory value: " << idex->rs1 << endl;
            }
            else
            {
                idex->rs1 = mowb->ALUOUT;
                cout << "Forwarded ALU value: " << idex->rs1 << endl;
            }
        }
        else
        {
            idex->rs1 = GPR[idex->rsl1];
            cout << "Read from register file: " << idex->rs1 << endl;
        }

        // RS2/Immediate handling
        if (idex->CW.ALUSrc && (idex->opcode == "0010011" || idex->opcode == "0000011"))
        {
            cout << "\nImmediate used as second operand: " << idex->imm1 << endl;
            idex->rs2 = idex->imm1;
        }
        else
        {
            cout << "\nReading RS2 (x" << idex->rsl2 << "):" << endl;
            if (exmo->rdl == idex->rsl2)
            {
                cout << "Forwarding from EX stage for rs2" << endl;
                idex->rs2 = exmo->ALUOUT;
                cout << "Forwarded value: " << idex->rs2 << endl;
            }
            else if (mowb->rdl == idex->rsl2)
            {
                cout << "Forwarding from WB stage for rs2" << endl;
                if (mowb->CW.MemRead)
                {
                    idex->rs2 = mowb->LDOUT;
                    cout << "Forwarded memory value: " << idex->rs2 << endl;
                }
                else
                {
                    idex->rs2 = mowb->ALUOUT;
                    cout << "Forwarded ALU value: " << idex->rs2 << endl;
                }
            }
            else
            {
                idex->rs2 = GPR[idex->rsl2];
                cout << "Read from register file: " << idex->rs2 << endl;
            }
        }
    }

    idex->valid = true;
    cout << "\nDecode stage complete" << endl;
}

void memoryAccess(MOWB *mowb, EXMO *exmo)
{
    cout << "\n=== MEMORY ACCESS STAGE ===" << endl;
    cout << "ALU Result: " << exmo->ALUOUT << endl;

    if (exmo->CW.MemWrite)
    {
        cout << "Store Operation:" << endl;
        cout << "Storing value " << exmo->rs2 << " to memory address " << exmo->ALUOUT << endl;
        dataMemory[exmo->ALUOUT] = exmo->rs2;
        cout << "Memory write completed" << endl;
    }

    if (exmo->CW.MemRead)
    {
        cout << "Load Operation:" << endl;
        cout << "Loading from memory address " << exmo->ALUOUT << endl;
        mowb->LDOUT = dataMemory[exmo->ALUOUT];
        cout << "Loaded value: " << mowb->LDOUT << endl;
    }

    mowb->ALUOUT = exmo->ALUOUT;
    mowb->CW = exmo->CW;
    mowb->rdl = exmo->rdl;

    cout << "Memory stage complete - Forwarding to Writeback:" << endl;
    cout << "ALU Output: " << mowb->ALUOUT << endl;
    cout << "Destination Register: x" << mowb->rdl << endl;
}

void registerWrite(MOWB *mowb)
{
    cout << "\n=== WRITEBACK STAGE ===" << endl;

    if (mowb->CW.RegWrite)
    {
        cout << "Writing to register x" << mowb->rdl << endl;

        if (mowb->CW.Mem2Reg)
        {
            cout << "Writing memory data: " << mowb->LDOUT << endl;
            GPR[mowb->rdl] = mowb->LDOUT;
        }
        else
        {
            cout << "Writing ALU result: " << mowb->ALUOUT << endl;
            GPR[mowb->rdl] = mowb->ALUOUT;
        }

        cout << "Register x" << mowb->rdl << " updated to: " << GPR[mowb->rdl] << endl;
    }
    else
    {
        cout << "No register write required" << endl;
    }
}
// Structure to hold instruction information
struct InstructionInfo
{
    string opcode;
    string funct3;
    string funct7;
    char type; // R, I, B, J to denote instruction type
};

// Class to handle RISC-V instruction encoding
class RiscVAssembler
{
private:
    // Map to hold register names and their corresponding binary values
    unordered_map<string, string> registerMap = {
        {"x0", "00000"}, {"x1", "00001"}, {"x2", "00010"}, {"x3", "00011"}, {"x4", "00100"}, {"x5", "00101"}, {"x6", "00110"}, {"x7", "00111"}, {"x8", "01000"}, {"x9", "01001"}, {"x10", "01010"}, {"x11", "01011"}, {"x12", "01100"}, {"x13", "01101"}, {"x14", "01110"}, {"x15", "01111"}, {"x16", "10000"}, {"x17", "10001"}, {"x18", "10010"}, {"x19", "10011"}, {"x20", "10100"}, {"x21", "10101"}, {"x22", "10110"}, {"x23", "10111"}, {"x24", "11000"}, {"x25", "11001"}, {"x26", "11010"}, {"x27", "11011"}, {"x28", "11100"}, {"x29", "11101"}, {"x30", "11110"}, {"x31", "11111"}};

    // Map to hold instruction information with types
    unordered_map<string, InstructionInfo> instructionMap = {
        // R-type instructions
        {"add", {"0110011", "000", "0000000", 'R'}},
        {"sub", {"0110011", "000", "0100000", 'R'}},
        {"sll", {"0110011", "001", "0000000", 'R'}},
        {"slt", {"0110011", "010", "0000000", 'R'}},
        {"sltu", {"0110011", "011", "0000000", 'R'}},
        {"xor", {"0110011", "100", "0000000", 'R'}},
        {"srl", {"0110011", "101", "0000000", 'R'}},
        {"sra", {"0110011", "101", "0100000", 'R'}},
        {"or", {"0110011", "110", "0000000", 'R'}},
        {"and", {"0110011", "111", "0000000", 'R'}},

        // I-type instructions
        {"addi", {"0010011", "000", "", 'I'}},
        {"slti", {"0010011", "010", "", 'I'}},
        {"sltiu", {"0010011", "011", "", 'I'}},
        {"xori", {"0010011", "100", "", 'I'}},
        {"ori", {"0010011", "110", "", 'I'}},
        {"andi", {"0010011", "111", "", 'I'}},
        {"slli", {"0010011", "001", "0000000", 'I'}},
        {"srli", {"0010011", "101", "0000000", 'I'}},
        {"srai", {"0010011", "101", "0100000", 'I'}},
        {"jalr", {"1100111", "000", "", 'I'}},
        {"lb", {"0000011", "000", "", 'I'}},
        {"lh", {"0000011", "001", "", 'I'}},
        {"lw", {"0000011", "010", "", 'I'}},
        {"lbu", {"0000011", "100", "", 'I'}},
        {"lhu", {"0000011", "101", "", 'I'}},

        // B-type instructions
        {"beq", {"1100011", "000", "", 'B'}},
        {"bne", {"1100011", "001", "", 'B'}},
        {"blt", {"1100011", "100", "", 'B'}},
        {"bge", {"1100011", "101", "", 'B'}},
        {"bltu", {"1100011", "110", "", 'B'}},
        {"bgeu", {"1100011", "111", "", 'B'}},

        // J-type instructions
        {"jal", {"1101111", "", "", 'J'}},

        // S-type instructions (store)
        {"sb", {"0100011", "000", "", 'S'}},
        {"sh", {"0100011", "001", "", 'S'}},
        {"sw", {"0100011", "010", "", 'S'}},

        // U-type instructions
        {"lui", {"0110111", "", "", 'U'}},
        {"auipc", {"0010111", "", "", 'U'}}};

    // Function to encode an R-type instruction
    string encodeRType(const InstructionInfo &info, const string &rd, const string &rs1, const string &rs2)
    {
        return info.funct7 + registerMap[rs2] + registerMap[rs1] + info.funct3 + registerMap[rd] + info.opcode;
    }

    // Function to encode an I-type instruction
    string encodeIType(const InstructionInfo &info, const string &rd, const string &rs1, int immediate)
    {
        string immBinary = bitset<12>(immediate).to_string();
        return immBinary + registerMap[rs1] + info.funct3 + registerMap[rd] + info.opcode;
    }

    // Function to encode a B-type instruction (used for branches)
    string encodeBType(const InstructionInfo &info, const string &rs1, const string &rs2, int offset)
    {
        string immBinary = bitset<12>(offset).to_string(); // 13 bits for signed immediate
        string imm_12 = immBinary.substr(0, 1);            // imm[12]
        string imm_10_5 = immBinary.substr(2, 6);          // imm[10:5]
        string imm_4_1 = immBinary.substr(8, 4);           // imm[4:1]
        string imm_11 = immBinary.substr(1, 1);            // imm[11]

        return imm_12 + imm_10_5 + registerMap[rs2] + registerMap[rs1] + info.funct3 + imm_4_1 + imm_11 + info.opcode;
    }

    // Function to encode a J-type instruction (used for jumps)
    string encodeJType(const InstructionInfo &info, const string &rd, int offset)
    {
        string immBinary = bitset<20>(offset).to_string(); // 20 bits for unsigned immediate
        string imm_20 = immBinary.substr(0, 1);            // imm[20]
        string imm_19_12 = immBinary.substr(1, 8);         // imm[10:1]
        string imm_11 = immBinary.substr(9, 1);            // imm[11]
        string imm_10_1 = immBinary.substr(10, 10);        // imm[19:12]

        return imm_20 + imm_10_1 + imm_11 + imm_19_12 + registerMap[rd] + info.opcode;
    }

    string encodeSType(const InstructionInfo &info, const string &rs1, const string &rs2, int offset)
    {
        string immBinary = bitset<12>(offset).to_string(); // 12 bits for signed immediate
        string imm_11_5 = immBinary.substr(0, 7);          // imm[11:5]
        string imm_4_0 = immBinary.substr(7, 5);           // imm[4:0]

        return imm_11_5 + registerMap[rs2] + registerMap[rs1] + info.funct3 + imm_4_0 + info.opcode;
    }

    string encodeUType(const InstructionInfo &info, const string &rd, int immediate)
    {
        string immBinary = bitset<20>(immediate).to_string(); // 20 bits for the immediate
        return immBinary + registerMap[rd] + info.opcode;
    }

public:
    // Function to parse and encode a single line of RISC-V assembly code
    string parseAndEncode(const string &assembly)
    {
        stringstream ss(assembly);
        string instruction, rd, rs1, rs2;
        int immediate;

        ss >> instruction;

        // Use instruction type to select appropriate encoding method
        const InstructionInfo &info = instructionMap[instruction];

        if (info.type == 'R')
        {
            // R-type instruction format: add rd, rs1, rs2
            ss >> rd >> rs1 >> rs2;
            rd = rd.substr(0, rd.size() - 1);    // Remove comma
            rs1 = rs1.substr(0, rs1.size() - 1); // Remove comma
            return encodeRType(info, rd, rs1, rs2);
        }
        else if (info.type == 'I')
        {
            // I-type instruction format: addi rd, rs1, immediate or jalr
            ss >> rd >> rs1 >> immediate;
            rd = rd.substr(0, rd.size() - 1);    // Remove comma
            rs1 = rs1.substr(0, rs1.size() - 1); // Remove comma
            return encodeIType(info, rd, rs1, immediate);
        }
        else if (info.type == 'B')
        {
            // B-type instruction format: beq rs1, rs2, offset
            ss >> rs1 >> rs2 >> immediate;
            rs1 = rs1.substr(0, rs1.size() - 1); // Remove comma
            rs2 = rs2.substr(0, rs2.size() - 1); // Remove comma
            return encodeBType(info, rs1, rs2, immediate);
        }
        else if (info.type == 'J')
        {
            // J-type instruction format: jal rd, offset
            ss >> rd >> immediate;
            rd = rd.substr(0, rd.size() - 1); // Remove comma
            return encodeJType(info, rd, immediate);
        }
        else if (info.type == 'S')
        {
            // S-type instruction format: sw rs2, offset(rs1)
            ss >> rs2 >> rs1 >> immediate;
            rs1 = rs1.substr(0, rs1.size() - 1); // Remove closing parenthesis
            rs2 = rs2.substr(0, rs2.size() - 1); // Remove comma
            return encodeSType(info, rs1, rs2, immediate);
        }
        else if (info.type == 'U')
        {
            // U-type instruction format: lui rd, immediate or auipc
            ss >> rd >> immediate;
            return encodeUType(info, rd, immediate);
        }
        else
        {
            return "Unknown instruction";
        }
    }
};

int main()
{
    vector<string> assemblyCode; // To store the assembly instructions
    RiscVAssembler assembler;
    string input;
    int choice;

    // Display the options to the user
    cout << "Choose an option:\n";
    cout << "1. Manual input\n";
    cout << "2. Use default assembly instructions\n";
    cout << "Enter your choice (1 or 2): ";
    cin >> choice;
    cin.ignore(); // To ignore the newline character after entering the choice

    if (choice == 1)
    {
        // Manual input option
        cout << "Enter assembly instructions (type 'end' to stop):\n";
        while (true)
        {
            getline(cin, input);
            if (input == "end")
                break;
            assemblyCode.push_back(input);
        }
    }
    else if (choice == 2)
    {
        // Default assembly code
        assemblyCode = {
            "add x1, x2, x3",
            "sub x0, x1, x3",
            "addi x2, x1, 10",
            "beq x1, x2, 8",
            "jal x1, 16",
            "jalr x3, x2, 4",
            "add x31, x30, x29",
            "bge x3, x7, 12"};
    }
    else
    {
        cout << "Invalid choice, exiting program." << endl;
        return 1; // Exit with error
    }

    // Process each line of assembly code
    for (const auto &line : assemblyCode)
    {
        instructionMemory.push_back(assembler.parseAndEncode(line));
    }

    PipelineControl ctrl;

    initControlUnit();
    cout << "GPR[i] = 2*i, we are seting this values as default." << endl;
    for (int i = 0; i < 32; i++)
    {
        GPR[i] = 2 * i;
    }

    int n = instructionMemory.size();

    IFID ifid = {};
    IDEX idex = {};
    EXMO exmo = {};
    MOWB mowb = {};
    dataMemory[22] = 16;
    int i = 1;
    while (PC < n * 4 || ifid.valid || idex.valid || exmo.valid || mowb.valid)
    {
        cout << "<-------------------------------------------cycle " << i++ << "------------------------------------------->" << endl;
        // Process Write-Back (WB) only if there's a valid instruction in MOWB
        if (mowb.valid)
        {
            registerWrite(&mowb); // WB stage
            mowb.valid = false;   // Mark as empty after write-back
        }

        // Process Memory Access (MEM) only if there's a valid instruction in EXMO
        if (exmo.valid)
        {
            memoryAccess(&mowb, &exmo); // MEM stage
            mowb.valid = true;
            exmo.valid = false;
        }

        // Process Execution (EX) only if there's a valid instruction in IDEX
        if (idex.valid)
        {
            instructionExecution(&idex, &exmo, &mowb, &ctrl); // EX stage
            exmo.valid = true;
            idex.valid = false;
        }

        // Process Instruction Decode (ID) only if there's a valid instruction in IFID
        if (ifid.valid)
        {
            instructionDecode(&ifid, &idex, &mowb, &exmo, &ctrl); // ID stage
            idex.valid = true;
            ifid.valid = false;
        }

        // Fetch instruction if PC is within bounds
        if (PC < n * 4 && !ctrl.stall)
        {
            instructionFetch(&ifid, &ctrl); // IF stage
        }
    }
    cout << "Used Registers:" << endl;
    for (const auto &reg : usedRegisters)
    {
        cout << "x" << reg << " = " << GPR[reg] << endl;
    }
}