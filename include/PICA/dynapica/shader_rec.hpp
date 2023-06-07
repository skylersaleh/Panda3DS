#pragma once
#include "PICA/shader.hpp"

#if defined(PANDA3DS_DYNAPICA_SUPPORTED) && defined(PANDA3DS_X64_HOST)
#define PANDA3DS_SHADER_JIT_SUPPORTED
#include <unordered_map>
#endif

class ShaderJIT {
#ifdef PANDA3DS_SHADER_JIT_SUPPORTED
	using Hash = PICAShader::Hash;
	using ShaderCache = std::unordered_map<Hash, int>;

	ShaderCache cache;
	void compileShader(PICAShader& shaderUnit);
#endif

public:
#ifdef PANDA3DS_SHADER_JIT_SUPPORTED
	// Call this before starting to process a batch of vertices
	// This will read the PICA config (uploaded shader and shader operand descriptors) and search if we've already compiled this shader
	// If yes, it sets it as the active shader. if not, then it compiles it, adds it to the cache, and sets it as active,
	void prepare(PICAShader& shaderUnit);
	void reset();

	static constexpr bool isAvailable() { return true; }
#else
	void prepare(PICAShader& shaderUnit) {
		Helpers::panic("Vertex Loader JIT: Tried to load vertices with JIT on platform that does not support vertex loader jit");
	}

	void reset() {}
	static constexpr bool isAvailable() { return false; }
#endif

};