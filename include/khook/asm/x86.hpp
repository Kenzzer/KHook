/* ======== KHook ========
* Copyright (C) 2025
* No warranties of any kind
*
* License: zLib License
*
* Author(s): Benoist "Kenzzer" ANDRÉ
* ============================
*/
#pragma once

#include <cstdint>

#include "../asm.hpp"
// Good reference :
// https://www.felixcloutier.com/x86/

namespace KHook
{
	namespace Asm
	{
		enum x86Reg : std::uint8_t {
			EAX =  0,
			ECX =  1,
			EDX =  2,
			EBX =  3,
			ESP =  4,
			EBP =  5,
			ESI =  6,
			EDI =  7,
		};
		
		enum MOD_MODRM : std::uint8_t {
			DISP0 = 0b00,
			DISP8 = 0b01,
			DISP32 = 0b10,
			REG = 0b11
		};

		class x86_RegRm {
		public:
			friend class x86_Reg;

			inline std::uint8_t sib() {
				// For the time being, we don't support multiple register
				return (0 << 6) | (this->low() << 3) | this->low();
			}

			inline std::uint8_t modrm(x86Reg reg) {
				return (mod << 6) | ((reg & 0x7) << 3) | this->low();
			}

			inline std::uint8_t modrm() {
				return (mod << 6) | (0x0 << 3) | this->low();
			}

			inline std::uint8_t modm() {
				return (mod << 6) | (0b110 << 3) | this->low();
			}

			void write_modm(GenBuffer* buffer);
			void write_modrm(GenBuffer* buffer);
			void write_modrm(GenBuffer* buffer, x86Reg op);

			bool extended() const { return ((rm & 0x8) == 0x8); }
			std::uint8_t low() const { return rm & 0x7; }
			constexpr operator x86Reg() const { return rm; }

		protected:
			x86_RegRm(x86Reg reg, std::int32_t disp) : rm(reg), disp(disp) {
				Setup();
			}

			x86_RegRm(x86Reg reg) : rm(reg), disp(0) {
				Setup();
			}

			void Setup();

			x86Reg rm;
			std::int32_t disp;
			MOD_MODRM mod;
		};

		class x86_Reg {
		public:
			constexpr x86_Reg(x86Reg op) : code(op) { }

			constexpr bool operator==(x86_Reg a) const { return code == a.code; }
			constexpr bool operator!=(x86_Reg a) const { return code != a.code; }
			x86_RegRm operator()() const { return x86_RegRm(*this, 0); }
			x86_RegRm operator()(std::int32_t disp) const { return x86_RegRm(*this, disp); }

			constexpr bool extended() const { return ((code & 0x8) == 0x8); }
			constexpr std::uint8_t low() const { return code & 0x7; }
			constexpr operator x86Reg() const { return code; }
		protected:
			x86Reg code;
		};

		static const x86_Reg eax = { x86Reg::EAX };
		static const x86_Reg ecx = { x86Reg::ECX };
		static const x86_Reg edx = { x86Reg::EDX };
		static const x86_Reg ebx = { x86Reg::EBX };
		static const x86_Reg esp = { x86Reg::ESP };
		static const x86_Reg ebp = { x86Reg::EBP };
		static const x86_Reg esi = { x86Reg::ESI };
		static const x86_Reg edi = { x86Reg::EDI };

		constexpr inline std::uint8_t modrm(x86Reg reg, x86Reg rm) {
			return (MOD_MODRM::REG << 6) | ((reg & 0x7) << 3) | (rm & 0x7);
		}

		constexpr inline std::uint8_t modrm_rm(x86Reg rm, std::uint8_t base) {
			return (MOD_MODRM::REG << 6) | (base << 3) | (rm & 0x7);
		}

		inline void x86_RegRm::Setup() {
			if (disp == 0 && rm != x86Reg::EBP) {
				mod = DISP0;
			}
			else if (disp >= -127 && disp <= 127) {
				mod = DISP8;
			} else {
				mod = DISP32;
			}
		}

		inline void x86_RegRm::write_modm(GenBuffer* buffer) {
			// modrm
			buffer->write_ubyte(modm());

			// Special register we need a sib byte
			if (rm == x86Reg::ESP) {
				buffer->write_ubyte(sib());
			}

			// Special disp mod
			if (mod != DISP0) {
				if (mod == DISP8) {
					buffer->write_byte(disp);
				} else if (mod == DISP32) {
					buffer->write_int32(disp);
				}
			}
		}

		inline void x86_RegRm::write_modrm(GenBuffer* buffer) {
			// modrm
			buffer->write_ubyte(modrm());

			// Special register we need a sib byte
			if (rm == x86Reg::ESP) {
				buffer->write_ubyte(sib());
			}

			// Special disp mod
			if (mod != DISP0) {
				if (mod == DISP8) {
					buffer->write_byte(disp);
				} else if (mod == DISP32) {
					buffer->write_int32(disp);
				}
			}
		}

		inline void x86_RegRm::write_modrm(GenBuffer* buffer, x86Reg reg) {
			// modrm
			buffer->write_ubyte(modrm(reg));

			// Special register we need a sib byte
			if (rm == x86Reg::ESP) { // rsp/r12
				buffer->write_ubyte(sib());
			}

			// Special disp mod
			if (mod != DISP0) {
				if (mod == DISP8) {
					buffer->write_byte(disp);
				} else if (mod == DISP32) {
					buffer->write_int32(disp);
				}
			}
		}
		
		class x86_Jit : public GenBuffer {
		public:
			void breakpoint() {
				this->write_ubyte(0xCC);
			}

			void rep_movs_bytes() {
				this->write_ubyte(0xF3);
				this->write_ubyte(0x48);
				this->write_ubyte(0xA4);
			}

			void call(x86_Reg reg) {
				this->write_ubyte(0xFF);
				this->write_ubyte(0xD0 + reg.low());
			}

			// Absolute
			void jump(x86_Reg reg) {
				this->write_ubyte(0xFF);
				this->write_ubyte(0xE0 + reg.low());
			}

			// Near
			void jump(std::int32_t off) {
				if (off >= -127 && off <= 127) {
					this->write_ubyte(0xEB);
					this->write_byte(std::int8_t(off));
				} else {
					this->write_ubyte(0xE9);
					this->write_int32(off);
				}
			}

			void jnz(std::int32_t off) {
				if (off >= -127 && off <= 127) {
					this->write_ubyte(0x75);
					this->write_byte(std::int8_t(off));
				} else {
					this->write_ubyte(0x0F);
					this->write_ubyte(0x85);
					this->write_int32(off);
				}
			}

			void jz(std::int32_t off) {
				if (off >= -127 && off <= 127) {
					this->write_ubyte(0x74);
					this->write_byte(std::int8_t(off));
				} else {
					this->write_ubyte(0x0F);
					this->write_ubyte(0x84);
					this->write_int32(off);
				}
			}

			void jl(std::int32_t off) {
				if (off >= -127 && off <= 127) {
					this->write_ubyte(0x7C);
					this->write_byte(std::int8_t(off));
				} else {
					this->write_ubyte(0x0F);
					this->write_ubyte(0x8C);
					this->write_int32(off);
				}
			}

			void jle(std::int32_t off) {
				if (off >= -127 && off <= 127) {
					this->write_ubyte(0x7E);
					this->write_byte(std::int8_t(off));
				} else {
					this->write_ubyte(0x0F);
					this->write_ubyte(0x8E);
					this->write_int32(off);
				}
			}

			void je(std::int32_t off) {
				if (off >= -127 && off <= 127) {
					this->write_ubyte(0x74);
					this->write_byte(std::int8_t(off));
				} else {
					this->write_ubyte(0x0F);
					this->write_ubyte(0x84);
					this->write_int32(off);
				}
			}

			void jg(std::int32_t off) {
				if (off >= -127 && off <= 127) {
					this->write_ubyte(0x7F);
					this->write_byte(std::int8_t(off));
				} else {
					this->write_ubyte(0x0F);
					this->write_ubyte(0x8F);
					this->write_int32(off);
				}
			}

			void jge(std::int32_t off) {
				if (off >= -127 && off <= 127) {
					this->write_ubyte(0x7D);
					this->write_byte(std::int8_t(off));
				} else {
					this->write_ubyte(0x0F);
					this->write_ubyte(0x8D);
					this->write_int32(off);
				}
			}

			void jne(std::int32_t off) {
				if (off >= -127 && off <= 127) {
					this->write_ubyte(0x75);
					this->write_byte(std::int8_t(off));
				} else {
					this->write_ubyte(0x0F);
					this->write_ubyte(0x85);
					this->write_int32(off);
				}
			}

			void push(x86_RegRm reg) {
				this->write_ubyte(0xFF);
				reg.write_modm(this);
			}

			void push(x86_Reg reg) {
				this->write_ubyte(0x50 + reg.low());
			}

			void push(std::int32_t val) {
				if (val >= -127 && val <= 127) {
					this->write_ubyte(0x6A);
					this->write_byte(std::int8_t(val));
				} else {
					this->write_ubyte(0x68);
					this->write_int32(val);
				}
			}

			void pop(x86_Reg reg) {
				this->write_ubyte(0x58 + reg.low());
			}

			// mov_mr
			void mov(x86_Reg dst, x86_Reg src) {
				this->write_ubyte(0x89);
				this->write_ubyte(modrm(src, dst));
			}

			void mov(x86_RegRm rm, x86_Reg reg) {
				this->write_ubyte(0x89);
				rm.write_modrm(this, reg);
			}

			void mov(x86_Reg reg, x86_RegRm rm) {
				this->write_ubyte(0x8B);
				rm.write_modrm(this, reg);
			}

			void mov(x86_Reg dst, std::int32_t imm) {
				this->write_ubyte(0xB8 + dst.low());
				this->write_int32(imm);
			}

			void mov(x86_RegRm dst, std::int32_t imm) {
				this->write_ubyte(0xC7);
				dst.write_modrm(this);
				this->write_int32(imm);
			}

			void add(x86_Reg dst, x86_Reg src) {
				this->write_ubyte(0x01);
				this->write_ubyte(modrm(src, dst));
			}

			void add(x86_Reg dst, int32_t imm) {
				this->write_ubyte(0x81);
				this->write_ubyte(modrm_rm(dst, 0));
				this->write_int32(imm);
			}

			void sub(x86_Reg dst, x86_Reg src) {
				this->write_ubyte(0x29);
				this->write_ubyte(modrm(src, dst));
			}

			void sub(x86_Reg dst, int32_t imm) {
				this->write_ubyte(0x81);
				this->write_ubyte(modrm_rm(dst, 5));
				this->write_int32(imm);
			}	

			void l_and(x86_Reg dst, x86_Reg src) {
				this->write_ubyte(0x21);
				this->write_ubyte(modrm(src, dst));
			}

			void l_and(x86_Reg dst, std::int32_t imm) {
				this->write_ubyte(0x81);
				this->write_ubyte(modrm_rm(dst, 4));
				this->write_int32(imm);
			}

			void l_xor(x86_Reg dst, x86_Reg src) {
				this->write_ubyte(0x31);
				this->write_ubyte(modrm(src, dst));
			}

			void test(x86_Reg dst, x86_Reg src) {
				this->write_ubyte(0x85);
				this->write_ubyte(modrm(src, dst));
			}

			void test(x86_Reg reg, int32_t imm) {
				this->write_ubyte(0xF7);
				this->write_ubyte(modrm_rm(reg, 0));
				this->write_int32(imm);
			}

			void cmovne(x86_Reg reg, x86_RegRm rm) {
				this->write_ubyte(0x0F);
				this->write_ubyte(0x45);
				rm.write_modrm(this, reg);
			}

			void cmovne(x86_Reg reg, x86_Reg rm) {
				this->write_ubyte(0x0F);
				this->write_ubyte(0x45);
				this->write_ubyte(modrm(reg, rm));
			}

			void cmovnz(x86_Reg reg, x86_RegRm rm) {
				this->write_ubyte(0x0F);
				this->write_ubyte(0x45);
				rm.write_modrm(this, reg);
			}

			void cmovnz(x86_Reg reg, x86_Reg rm) {
				this->write_ubyte(0x0F);
				this->write_ubyte(0x45);
				this->write_ubyte(modrm(reg, rm));
			}

			void cmovge(x86_Reg reg, x86_RegRm rm) {
				this->write_ubyte(0x0F);
				this->write_ubyte(0x4D);
				rm.write_modrm(this, reg);
			}

			void cmovge(x86_Reg reg, x86_Reg rm) {
				this->write_ubyte(0x0F);
				this->write_ubyte(0x4D);
				this->write_ubyte(modrm(reg, rm));
			}

			void cmovg(x86_Reg reg, x86_RegRm rm) {
				this->write_ubyte(0x0F);
				this->write_ubyte(0x4F);
				rm.write_modrm(this, reg);
			}

			void cmovg(x86_Reg reg, x86_Reg rm) {
				this->write_ubyte(0x0F);
				this->write_ubyte(0x4F);
				this->write_ubyte(modrm(reg, rm));
			}

			void lea(x86_Reg reg, x86_RegRm rm) {
				this->write_ubyte(0x8D);
				rm.write_modrm(this, reg);
			}

			void cmp(x86_Reg reg, x86_RegRm rm) {
				this->write_ubyte(0x3B);
				rm.write_modrm(this, reg);
			}

			void cmp(x86_RegRm rm, x86_Reg reg) {
				this->write_ubyte(0x39);
				rm.write_modrm(this, reg);
			}

			void cmp(x86_Reg reg, x86_Reg rm) {
				this->write_ubyte(0x3B);
				this->write_ubyte(modrm(reg, rm));
			}

			void cmp(x86_Reg dst, int32_t imm) {
				this->write_ubyte(0x81);
				this->write_ubyte(modrm_rm(dst, 7));
				this->write_int32(imm);
			}

			void retn() {
				this->write_ubyte(0xC3);
			}
		};
	}
}