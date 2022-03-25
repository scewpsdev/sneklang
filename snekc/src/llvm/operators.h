#pragma once

#include <llvm-c/Core.h>


struct LLVMBackend;
struct SkModule;

typedef struct TypeData* TypeID;


LLVMValueRef OperatorNot(LLVMBackend* llb, SkModule* module, LLVMValueRef operand, TypeID operandType, bool operandLValue);
LLVMValueRef OperatorNegate(LLVMBackend* llb, SkModule* module, LLVMValueRef operand, TypeID operandType, bool operandLValue);
LLVMValueRef OperatorReference(LLVMBackend* llb, SkModule* module, LLVMValueRef operand, TypeID operandType, bool operandLValue);
LLVMValueRef OperatorDereference(LLVMBackend* llb, SkModule* module, LLVMValueRef operand, TypeID operandType, bool operandLValue);

LLVMValueRef OperatorIncrement(LLVMBackend* llb, SkModule* module, LLVMValueRef operand, TypeID operandType, bool operandLValue, bool position);
LLVMValueRef OperatorDecrement(LLVMBackend* llb, SkModule* module, LLVMValueRef operand, TypeID operandType, bool operandLValue, bool position);

LLVMValueRef OperatorAdd(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorSub(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorMul(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorDiv(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorMod(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);

LLVMValueRef OperatorEQ(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorNE(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorLT(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorGT(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorLE(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorGE(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorAnd(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorOr(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorBWAnd(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorBWOr(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorBWXor(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorBSLeft(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorBSRight(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);

LLVMValueRef OperatorAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue, bool rightLiteral);
LLVMValueRef OperatorAddAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorSubAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorMulAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorDivAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorModAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorBSLeftAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorBSRightAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorBWAndAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorBWOrAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorBWXorAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);

LLVMValueRef OperatorTernary(LLVMBackend* llb, SkModule* module, LLVMValueRef condition, LLVMValueRef thenValue, LLVMValueRef elseValue, TypeID conditionType, TypeID thenType, TypeID elseType, bool conditionLValue, bool thenLValue, bool elseLValue);
