/*
 * aop.h
 *
 * operand and instruction.
 *
 *  Created on: 2019年7月23日
 *      Author: ueyudiud
 */

#ifndef AOP_H_
#define AOP_H_

#include "art.h"

#define SIZE_i 8
#define SIZE_xi 2
#define SIZE_xA 2
#define SIZE_xB 2
#define SIZE_xC 2
#define SIZE_x (SIZE_xi + SIZE_xA + SIZE_xB + SIZE_xC)
#define SIZE_A 16
#define SIZE_B 16
#define SIZE_C 16
#define SIZE_Bx (SIZE_B + SIZE_C)
#define SIZE_Ax (SIZE_A + SIZE_Bx)

#define POS_i 0
#define POS_x (SIZE_i + POS_i)
#define POS_xi POS_x
#define POS_xA (SIZE_xi + POS_xi)
#define POS_xB (SIZE_xA + POS_xA)
#define POS_xC (SIZE_xB + POS_xB)
#define POS_A (SIZE_x + POS_x)
#define POS_B (SIZE_A + POS_A)
#define POS_C (SIZE_B + POS_B)
#define POS_Bx POS_B
#define POS_Ax POS_A

#define GetVal(t,x) (((x) >> POS##t) & ((1ULL << SIZE##t) - 1))
#define MaskVal(t,x) (((x) & ((1ULL << SIZE##t) - 1)) << POS##t)
#define SetVal(s,t,x) ((s) = ((s) & ~(((1ULL << SIZE##t) - 1) << POS##t)) | MaskVal(t, x))

#define GET_i(x)	aloE_cast(uint8_t , GetVal(_i , x))
#define GET_xi(x)	aloE_cast(uint8_t , GetVal(_xi, x))
#define GET_xA(x)	aloE_cast(uint8_t , GetVal(_xA, x))
#define GET_xB(x)	aloE_cast(uint8_t , GetVal(_xB, x))
#define GET_xC(x)	aloE_cast(uint8_t , GetVal(_xC, x))
#define GET_x(x)    aloE_cast(uint8_t , GetVal(_x , x))
#define GET_A(x)	aloE_cast(uint16_t, GetVal(_A , x))
#define GET_B(x)	aloE_cast(uint16_t, GetVal(_B , x))
#define GET_C(x)	aloE_cast(uint16_t, GetVal(_C , x))
#define GET_Bx(x)	aloE_cast(uint32_t, GetVal(_Bx, x))
#define GET_sBx(x)	aloE_cast( int32_t, GetVal(_Bx, x))
#define GET_Ax(x)	aloE_cast(uint64_t, GetVal(_Ax, x))

#define SET_i(s,x)  SetVal(s, _i, x)
#define SET_xA(s,x)  SetVal(s, _xA, x)
#define SET_xB(s,x)  SetVal(s, _xB, x)
#define SET_xC(s,x)  SetVal(s, _xC, x)
#define SET_A(s,x)  SetVal(s, _A, x)
#define SET_B(s,x)  SetVal(s, _B, x)
#define SET_C(s,x)  SetVal(s, _C, x)
#define SET_Bx(s,x)  SetVal(s, _Bx, x)
#define SET_sBx(s,x)  SetVal(s, _Bx, aloE_cast(uint32_t, x))

#define MASK_i(x)		(MaskVal(_i, x))
#define MASK_A(x,y)		(MaskVal(_A, y) | MaskVal(_xA, x))
#define MASK_B(x,y)		(MaskVal(_B, y) | MaskVal(_xB, x))
#define MASK_C(x,y)		(MaskVal(_C, y) | MaskVal(_xC, x))
#define MASK_Bx(x,y)	(MaskVal(_Bx, y) | MaskVal(_xB, x))
#define MASK_sBx(x,y)	(MaskVal(_Bx, aloE_cast(uint32_t, y)) | MaskVal(_xB, x))
#define MASK_Ax(x,y)	(MaskVal(_Ax, y) | MaskVal(_xA, x))

#define CREATE_iABC(i,xA,xB,xC,A,B,C) (MASK_i(i) | MASK_A(xA, A) | MASK_B(xB, B) | MASK_C(xC, C))
#define CREATE_iABx(i,xA,xB,A,Bx) (MASK_i(i) | MASK_A(xA, A) | MASK_Bx(xB, Bx))
#define CREATE_iAsBx(i,xA,xC,A,sBx) (MASK_i(i) | MASK_A(xA, A) | MASK_sBx(0, sBx) | MASK_C(xC, 0))
#define CREATE_iAx(i,xA,Ax) (MASK_i(i) | MASK_Ax(xA, Ax))

/**
 ** prefix for slot
 **
 ** S(x) - variable
 ** X(x) - variable and constant
 ** K(x) - constant
 ** R(x) - register
 */

#define aloK_fastconstsize (UINT16_MAX + 1)
#define aloK_maxstacksize 32768
#define aloK_registry aloK_maxstacksize
#define aloK_iscapture(index) ((index) >= aloK_maxstacksize)
#define aloK_getstack(index) (index)
#define aloK_getcapture(index) ((index) - aloK_maxstacksize)
#define aloK_setstack(index) (index)
#define aloK_setcapture(index) ((index) + aloK_maxstacksize)

/**
 ** some fixed constants in constant pool:
 ** for convenience, there are some fixed constant id.
 ** [0] = false
 ** [1] = true
 */

/**
 ** all opcodes defination.
 */
#define ALO_OPS \
/***===============================================================================*  \
 ***   id      name           args      description                                *  \
 ***===============================================================================*/ \
	OP(MOV  , "mov"  ), /*   A B       S(A) := X(B)                                */ \
	OP(LDC  , "ldc"  ), /*   A Bx      S(A) := K(B)                                */ \
	OP(LDN  , "ldn"  ), /*   A B       R(A) ... R(A+B-1) := nil                    */ \
	OP(LDP  , "ldp"  ), /*   A Bx      S(A) := closure(Proto[B])                   */ \
	OP(LDV  , "ldv"  ), /*   A B       R(A) ... R(A+C-2) := vararg                 */ \
	OP(SELV , "selv" ), /*   A B       S(A) :=  vararg[K(B)]                       */ \
	                                                                                  \
	OP(NEWA , "newa" ), /*   A B C     S(A) := (R(B), ... R(B+C-1))                */ \
	OP(NEWL , "newl" ), /*   A Bx      S(A) := [] (size = Bx)                      */ \
	OP(NEWM , "newm" ), /*   A Bx      S(A) := {} (size = Bx)                      */ \
	OP(UNBOX, "unbox"), /*   A B C     R(A) ... R(A+C-2) := unbox S(B)             */ \
	OP(GET  , "get"  ), /*   A B C     R(A) := S(B)[X(C)]                          */ \
	OP(SET  , "set"  ), /*   A B C     S(A)[X(B)] := X(C)                          */ \
	OP(REM  , "rem"  ), /*   A B C     R(A) := remove S(B)[S(C)]                   */ \
	OP(SELF , "self" ), /*   A B C     R(A) R(A+1) := X(B)->X(C) X(B)              */ \
	                                                                                  \
	OP(ADD  , "add"  ), /*   A B C     S(A) := X(B) + K(C)                         */ \
	OP(SUB  , "sub"  ), /*   A B C     S(A) := X(B) - K(C)                         */ \
	OP(MUL  , "mul"  ), /*   A B C     S(A) := X(B) * K(C)                         */ \
	OP(DIV  , "div"  ), /*   A B C     S(A) := X(B) / K(C)                         */ \
	OP(IDIV , "idiv" ), /*   A B C     S(A) := X(B) // K(C)                        */ \
	OP(MOD  , "mod"  ), /*   A B C     S(A) := X(B) % K(C)                         */ \
	OP(POW  , "pow"  ), /*   A B C     S(A) := X(B) ^ K(C)                         */ \
	OP(SHL  , "shl"  ), /*   A B C     S(A) := X(B) << K(C)                        */ \
	OP(SHR  , "shr"  ), /*   A B C     S(A) := X(B) >> K(C)                        */ \
	OP(BAND , "band" ), /*   A B C     S(A) := X(B) & K(C)                         */ \
	OP(BOR  , "bor"  ), /*   A B C     S(A) := X(B) | K(C)                         */ \
	OP(BXOR , "bxor" ), /*   A B C     S(A) := X(B) ~ K(C)                         */ \
	                                                                                  \
	OP(PNM  , "pnm"  ), /*   A B C     S(A) := + X(B)                              */ \
	OP(UNM  , "unm"  ), /*   A B C     S(A) := - X(B)                              */ \
	OP(LEN  , "len"  ), /*   A B C     S(A) := length of X(B)                      */ \
	OP(BNOT , "bnot" ), /*   A B C     S(A) := ~ X(B)                              */ \
	                                                                                  \
	OP(AADD , "aadd" ), /*   A B       S(A) += X(B)                                */ \
	OP(ASUB , "asub" ), /*   A B       S(A) -= X(B)                                */ \
	OP(AMUL , "amul" ), /*   A B       S(A) *= X(B)                                */ \
	OP(ADIV , "adiv" ), /*   A B       S(A) /= X(B)                                */ \
	OP(AIDIV, "aidiv"), /*   A B       S(A) //= X(B)                               */ \
	OP(AMOD , "amod" ), /*   A B       S(A) %= X(B)                                */ \
	OP(APOW , "apow" ), /*   A B       S(A) ^= X(B)                                */ \
	OP(ASHL , "ashl" ), /*   A B       S(A) <<= X(B)                               */ \
	OP(ASHR , "ashr" ), /*   A B       S(A) >>= X(B)                               */ \
	OP(AAND , "aand" ), /*   A B       S(A) &= X(B)                                */ \
	OP(AOR  , "aor"  ), /*   A B       S(A) |= X(B)                                */ \
	OP(AXOR , "axor" ), /*   A B       S(A) ~= X(B)                                */ \
	                                                                                  \
	OP(CAT  , "cat"  ), /*   A B C     S(A) := R(B) .. ... .. R(B+C)               */ \
	OP(ACAT , "acat" ), /*   A B       S(A) ..= X(B)                               */ \
	OP(ITR  , "itr"  ), /*   A B       S(A) = newiterator(S(B))                    */ \
	OP(JMP  , "jmp"  ), /*   A sBx     pc += sBx; if (xA) close captures >= R(A)   */ \
	OP(JCZ  , "jcz"  ), /*   A sBx     if (S(A) as bool == xC) pc += sBx           */ \
	OP(EQ   , "eq"   ), /*   A B C     if ((X(A) == X(B)) == xC) pc ++             */ \
	OP(LT   , "lt"   ), /*   A B C     if ((X(A) <  X(B)) == xC) pc ++             */ \
	OP(LE   , "le"   ), /*   A B C     if ((X(A) <= X(B)) == xC) pc ++             */ \
	                                                                                  \
	OP(NEW  , "new"  ), /*   A B       S(A) := new S(B)                            */ \
	OP(CALL , "call" ), /*   A B C     R(A) ... R(A+C-2):=R(A)(R(A+1), ..., R(A+B))*/ \
	OP(TCALL, "tcall"), /*   A B       return R(A)(R(A+1), ..., R(A+B))            */ \
	OP(ICALL, "icall"), /*   A B C     if (has(R(A))) R(A+1) := next(R(A))         */ \
	OP(RET  , "ret"  )  /*   A B       return R(A), ..., R(A+B-2)                  */ \
/***===============================================================================*/

#define OP(id,name) OP_##id

enum { ALO_OPS, OP_N };

#undef OP

ALO_VDEC const char* const aloP_opname[OP_N];

/**
 ** extra notes:
 ** <*1> for 'rset', only appeared after opcode 'newl' or 'newm',
 **      and operand B has two mode: constant mode and integral mode,
 **      in integral mode, operand Bx represent itself, used in 'newl'.
 **      in constant mode, operand B represent constant, used in 'newm'.
 **
 ** <*2> for 'axxx' (means instruction with prefix 'a', which means
 **      assignment), A has 3 kinds of mode: ignore, check and update mode.
 **
 ** <*3> for 'ret', if B = 0, return with variable argument.
 **
 ** <*4> for 'unbox', if xC is true, the pc will increase 1 if unbox failed.
 */

#endif /* AOP_H_ */
