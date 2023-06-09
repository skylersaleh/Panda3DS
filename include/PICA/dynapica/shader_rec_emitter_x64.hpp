#pragma once

// Only do anything if we're on an x64 target with JIT support enabled
#if defined(PANDA3DS_DYNAPICA_SUPPORTED) && defined(PANDA3DS_X64_HOST)
#include "helpers.hpp"
#include "PICA/shader.hpp"
#include "xbyak/xbyak.h"
#include "xbyak/xbyak_util.h"
#include "x64_regs.hpp"

#include <vector>

class ShaderEmitter : public Xbyak::CodeGenerator {
	static constexpr size_t executableMemorySize = PICAShader::maxInstructionCount * 96; // How much executable memory to alloc for each shader
	// Allocate some extra space as padding for security purposes in the extremely unlikely occasion we manage to overflow the above size
	static constexpr size_t allocSize = executableMemorySize + 0x1000;

	// If the swizzle field is this value then the swizzle pattern is .xyzw so we don't need a shuffle
	static constexpr uint noSwizzle = 0x1B;

	using f24 = Floats::f24;
	using vec4f = OpenGL::Vector<f24, 4>;

	// An array of labels (incl pointers) to each compiled (to x64) PICA instruction
	std::array<Xbyak::Label, PICAShader::maxInstructionCount> instructionLabels;
	// A vector of PCs that can potentially return based on the state of the PICA callstack.
	// Filled before compiling a shader by scanning the code for call instructions
	std::vector<u32> returnPCs;

	u32 recompilerPC = 0; // PC the recompiler is currently recompiling @
	bool haveSSE4_1 = false;  // Shows if the CPU supports SSE4.1

	// Compile all instructions from [current recompiler PC, end)
	void compileUntil(const PICAShader& shaderUnit, u32 endPC);
	// Compile instruction "instr"
	void compileInstruction(const PICAShader& shaderUnit);

	bool isCall(u32 instruction) {
		const u32 opcode = instruction >> 26;
		return (opcode == ShaderOpcodes::CALL) || (opcode == ShaderOpcodes::CALLC) || (opcode == ShaderOpcodes::CALLU);
	}
	// Scan the shader code for call instructions to fill up the returnPCs vector before starting compilation
	void scanForCalls(const PICAShader& shaderUnit);

	// Load register with number "srcReg" indexed by index "idx" into the xmm register "reg"
	template <int sourceIndex>
	void loadRegister(Xmm dest, const PICAShader& shader, u32 src, u32 idx, u32 operandDescriptor);
	void storeRegister(Xmm source, const PICAShader& shader, u32 dest, u32 operandDescriptor);

	const vec4f& getSourceRef(const PICAShader& shader, u32 src);
	const vec4f& getDestRef(const PICAShader& shader, u32 dest);

	// Instruction recompilation functions
	void recADD(const PICAShader& shader, u32 instruction);
	void recCALL(const PICAShader& shader, u32 instruction);
	void recCALLC(const PICAShader& shader, u32 instruction);
	void recCALLU(const PICAShader& shader, u32 instruction);
	void recCMP(const PICAShader& shader, u32 instruction);
	void recDP3(const PICAShader& shader, u32 instruction);
	void recDP4(const PICAShader& shader, u32 instruction);
	void recEND(const PICAShader& shader, u32 instruction);
	void recFLR(const PICAShader& shader, u32 instruction);
	void recIFC(const PICAShader& shader, u32 instruction);
	void recIFU(const PICAShader& shader, u32 instruction);
	void recJMPC(const PICAShader& shader, u32 instruction);
	void recJMPU(const PICAShader& shader, u32 instruction);
	void recLOOP(const PICAShader& shader, u32 instruction);
	void recMAX(const PICAShader& shader, u32 instruction);
	void recMIN(const PICAShader& shader, u32 instruction);
	void recMOVA(const PICAShader& shader, u32 instruction);
	void recMOV(const PICAShader& shader, u32 instruction);
	void recMUL(const PICAShader& shader, u32 instruction);
	void recRCP(const PICAShader& shader, u32 instruction);
	void recRSQ(const PICAShader& shader, u32 instruction);
	void recSGE(const PICAShader& shader, u32 instruction);
	void recSGEI(const PICAShader& shader, u32 instruction);
	void recSLT(const PICAShader& shader, u32 instruction);
	void recSLTI(const PICAShader& shader, u32 instruction);

public:
	using InstructionCallback = const void(*)(PICAShader& shaderUnit); // Callback type used for instructions
	// Callback type used for the JIT prologue. This is what the caller will call
	using PrologueCallback = const void(*)(PICAShader& shaderUnit, InstructionCallback cb);
	PrologueCallback prologueCb = nullptr;

	// Initialize our emitter with "allocSize" bytes of RWX memory
	ShaderEmitter() : Xbyak::CodeGenerator(allocSize) {
		const auto cpu = Xbyak::util::Cpu();

		haveSSE4_1 = cpu.has(Xbyak::util::Cpu::tSSE41);
		if (!cpu.has(Xbyak::util::Cpu::tSSE3)) {
			Helpers::panic("This CPU does not support SSE3. Please use the shader interpreter instead");
		}
	}
	
	void compile(const PICAShader& shaderUnit);

	// PC must be a valid entrypoint here. It doesn't have that much overhead in this case, so we use std::array<>::at() to assert it does
	InstructionCallback getInstructionCallback(u32 pc) {
		// Cast away the constness because casting to a function pointer is hard otherwise. Legal as long as we don't write to *ptr
		uint8_t* ptr = const_cast<uint8_t*>(instructionLabels.at(pc).getAddress());
		return reinterpret_cast<InstructionCallback>(ptr);
	}

	PrologueCallback getPrologueCallback() {
		return prologueCb;
	}
};

#endif // x64 recompiler check