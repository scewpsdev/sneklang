#include "operators.h"

#include "ast.h"
#include "log.h"
#include "type.h"
#include "debug.h"
#include "values.h"
#include "llvm_backend.h"


LLVMValueRef OperatorAdd(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);
LLVMValueRef OperatorSub(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue);

LLVMValueRef OperatorNot(LLVMBackend* llb, SkModule* module, LLVMValueRef operand, TypeID operandType, bool operandLValue)
{
	operand = GetRValue(llb, module, operand, operandLValue);

	if (operandType->typeKind == TYPE_KIND_BOOL)
	{
		return LLVM_CALL(LLVMBuildNot, module->builder, operand, "");
	}
	else if (operandType->typeKind == TYPE_KIND_INTEGER || operandType->typeKind == TYPE_KIND_POINTER)
	{
		return LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntEQ, operand, LLVMConstNull(LLVMTypeOf(operand)), "");
	}
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorNegate(LLVMBackend* llb, SkModule* module, LLVMValueRef operand, TypeID operandType, bool operandLValue)
{
	operand = GetRValue(llb, module, operand, operandLValue);

	if (operandType->typeKind == TYPE_KIND_INTEGER)
	{
		return LLVM_CALL(LLVMBuildNeg, module->builder, operand, "");
	}
	else if (operandType->typeKind == TYPE_KIND_FP)
	{
		return LLVM_CALL(LLVMBuildFNeg, module->builder, operand, "");
	}
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorReference(LLVMBackend* llb, SkModule* module, LLVMValueRef operand, TypeID operandType, bool operandLValue)
{
	if (operandLValue)
	{
		return operand;
	}
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorDereference(LLVMBackend* llb, SkModule* module, LLVMValueRef operand, TypeID operandType, bool operandLValue)
{
	if (operandType->typeKind == TYPE_KIND_POINTER && operandLValue)
	{
		operand = GetRValue(llb, module, operand, operandLValue);
		return operand;
		//return LLVM_CALL(LLVMBuildLoad, module->builder, operand, "");
	}

	SnekAssert(false);
	return NULL;
}

LLVMValueRef OperatorIncrement(LLVMBackend* llb, SkModule* module, LLVMValueRef operand, TypeID operandType, bool operandLValue, bool position)
{
	SnekAssert(operandLValue);
	LLVMValueRef initialValue = LLVM_CALL(LLVMBuildLoad, module->builder, operand, "");

	if (operandType->typeKind == TYPE_KIND_INTEGER)
	{
		LLVMValueRef result = OperatorAdd(llb, module, initialValue, LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 1, false), operandType, GetIntegerType(32, false), false, false);
		LLVM_CALL(LLVMBuildStore, module->builder, result, operand);

		if (position)
			return initialValue;
		else
			return operand;
	}
	else if (operandType->typeKind == TYPE_KIND_FP)
	{
		LLVMValueRef result = OperatorAdd(llb, module, initialValue, LLVMConstReal(LLVMFloatTypeInContext(llb->llvmContext), 1.0), operandType, GetFPType(FP_PRECISION_SINGLE), false, false);
		LLVM_CALL(LLVMBuildStore, module->builder, result, operand);

		if (position)
			return initialValue;
		else
			return operand;
	}
	else if (operandType->typeKind == TYPE_KIND_POINTER)
	{
		LLVMTypeRef elementType = LLVMGetElementType(LLVMTypeOf(operand));
		LLVMValueRef index = LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 1, false);
		LLVMValueRef result = LLVM_CALL(LLVMBuildGEP, module->builder, initialValue, (LLVMValueRef*)&index, 1, "");
		LLVM_CALL(LLVMBuildStore, module->builder, result, operand);

		if (position)
			return initialValue;
		else
			return operand;
	}

	SnekAssert(false);
	return NULL;
}

LLVMValueRef OperatorDecrement(LLVMBackend* llb, SkModule* module, LLVMValueRef operand, TypeID operandType, bool operandLValue, bool position)
{
	SnekAssert(operandLValue);
	LLVMValueRef initialValue = LLVM_CALL(LLVMBuildLoad, module->builder, operand, "");

	if (operandType->typeKind == TYPE_KIND_INTEGER)
	{
		LLVMValueRef result = OperatorSub(llb, module, initialValue, LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 1, false), operandType, GetIntegerType(32, false), false, false);
		LLVM_CALL(LLVMBuildStore, module->builder, result, operand);

		if (position)
			return initialValue;
		else
			return operand;
	}
	else if (operandType->typeKind == TYPE_KIND_FP)
	{
		LLVMValueRef result = OperatorSub(llb, module, initialValue, LLVMConstReal(LLVMFloatTypeInContext(llb->llvmContext), 1.0), operandType, GetFPType(FP_PRECISION_SINGLE), false, false);
		LLVM_CALL(LLVMBuildStore, module->builder, result, operand);

		if (position)
			return initialValue;
		else
			return operand;
	}
	else if (operandType->typeKind == TYPE_KIND_POINTER)
	{
		LLVMTypeRef elementType = LLVMGetElementType(LLVMTypeOf(operand));
		LLVMValueRef index = LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), -1, false);
		LLVMValueRef result = LLVM_CALL(LLVMBuildGEP, module->builder, initialValue, (LLVMValueRef*)&index, 1, "");
		LLVM_CALL(LLVMBuildStore, module->builder, result, operand);

		if (position)
			return initialValue;
		else
			return operand;
	}

	SnekAssert(false);
	return NULL;
}

static void BinaryOperatorTypesMeet(LLVMBackend* llb, SkModule* module, LLVMValueRef& left, LLVMValueRef& right, TypeID& leftType, TypeID& rightType, bool leftLValue, bool rightLValue)
{
	left = GetRValue(llb, module, left, leftLValue);
	right = GetRValue(llb, module, right, rightLValue);

	LLVMTypeRef lt = LLVMTypeOf(left);
	LLVMTypeRef rt = LLVMTypeOf(right);
	LLVMTypeKind ltk = LLVMGetTypeKind(lt);
	LLVMTypeKind rtk = LLVMGetTypeKind(rt);

	while (leftType->typeKind == TYPE_KIND_ALIAS)
		leftType = leftType->aliasType.alias;
	while (rightType->typeKind == TYPE_KIND_ALIAS)
		rightType = rightType->aliasType.alias;

	if (leftType->typeKind == TYPE_KIND_INTEGER && rightType->typeKind == TYPE_KIND_INTEGER)
	{
		if (leftType->integerType.bitWidth > rightType->integerType.bitWidth)
		{
			if (rightType->integerType.isSigned)
				right = LLVM_CALL(LLVMBuildSExt, module->builder, right, lt, "");
			else
				right = LLVM_CALL(LLVMBuildZExt, module->builder, right, lt, "");
			rightType = leftType;
		}
		else if (leftType->integerType.bitWidth < rightType->integerType.bitWidth)
		{
			if (leftType->integerType.isSigned)
				left = LLVM_CALL(LLVMBuildSExt, module->builder, left, rt, "");
			else
				left = LLVM_CALL(LLVMBuildZExt, module->builder, left, rt, "");
			leftType = rightType;
		}
	}
	else if (leftType->typeKind == TYPE_KIND_INTEGER && rightType->typeKind == TYPE_KIND_BOOL)
	{
		right = LLVM_CALL(LLVMBuildZExt, module->builder, right, lt, "");
		rightType = leftType;
	}
	else if (leftType->typeKind == TYPE_KIND_BOOL && rightType->typeKind == TYPE_KIND_INTEGER)
	{
		left = LLVM_CALL(LLVMBuildZExt, module->builder, left, rt, "");
		leftType = rightType;
	}
	else if (leftType->typeKind == TYPE_KIND_BOOL && rightType->typeKind == TYPE_KIND_BOOL)
	{
	}
	else if (leftType->typeKind == TYPE_KIND_FP && rightType->typeKind == TYPE_KIND_FP)
	{
		if (leftType->fpType.precision > rightType->fpType.precision)
		{
			right = LLVM_CALL(LLVMBuildFPExt, module->builder, right, lt, "");
			rightType = leftType;
		}
		else if (leftType->fpType.precision < rightType->fpType.precision)
		{
			left = LLVM_CALL(LLVMBuildFPExt, module->builder, left, rt, "");
			leftType = rightType;
		}
	}
	else if (leftType->typeKind == TYPE_KIND_INTEGER && rightType->typeKind == TYPE_KIND_FP)
	{
		if (leftType->integerType.isSigned)
			left = LLVMBuildSIToFP(module->builder, left, rt, "");
		else
			left = LLVMBuildUIToFP(module->builder, left, rt, "");
		leftType = rightType;
	}
	else if (leftType->typeKind == TYPE_KIND_FP && rightType->typeKind == TYPE_KIND_INTEGER)
	{
		if (rightType->integerType.isSigned)
			right = LLVMBuildSIToFP(module->builder, right, lt, "");
		else
			right = LLVMBuildUIToFP(module->builder, right, lt, "");
		rightType = leftType;
	}
	else if (leftType->typeKind == TYPE_KIND_POINTER && rightType->typeKind == TYPE_KIND_POINTER)
	{
		right = LLVM_CALL(LLVMBuildBitCast, module->builder, right, LLVMTypeOf(left), "");
		//left = LLVM_CALL(LLVMBuildPtrToInt, module->builder, left, LLVMInt64TypeInContext(llb->llvmContext), "");
		//right = LLVM_CALL(LLVMBuildPtrToInt, module->builder, right, LLVMInt64TypeInContext(llb->llvmContext), "");
		rightType = leftType;
	}
	else if (leftType->typeKind == TYPE_KIND_POINTER && rightType->typeKind == TYPE_KIND_INTEGER)
	{
		rightType = leftType;
	}
	else if (leftType->typeKind == TYPE_KIND_POINTER && rightType->typeKind == TYPE_KIND_CLASS ||
		leftType->typeKind == TYPE_KIND_CLASS && rightType->typeKind == TYPE_KIND_POINTER)
	{
		right = LLVM_CALL(LLVMBuildBitCast, module->builder, right, LLVMTypeOf(left), "");
		rightType = leftType;
	}
	else
	{
		SnekAssert(false);
	}
}

LLVMValueRef OperatorAdd(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);

	bool isInteger = LLVMGetTypeKind(LLVMTypeOf(left)) == LLVMIntegerTypeKind;
	bool isFP = LLVMGetTypeKind(LLVMTypeOf(left)) >= LLVMHalfTypeKind && LLVMGetTypeKind(LLVMTypeOf(right)) <= LLVMFP128TypeKind;
	bool isPointer = LLVMGetTypeKind(LLVMTypeOf(left)) == LLVMPointerTypeKind;

	if (isInteger)
		return LLVM_CALL(LLVMBuildAdd, module->builder, left, right, "");
	else if (isFP)
		return LLVM_CALL(LLVMBuildFAdd, module->builder, left, right, "");
	else if (isPointer && rightType->typeKind == TYPE_KIND_INTEGER)
		return LLVM_CALL(LLVMBuildGEP, module->builder, left, (LLVMValueRef*)&right, 1, "");
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorSub(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);

	bool isInteger = LLVMGetTypeKind(LLVMTypeOf(left)) == LLVMIntegerTypeKind;
	bool isFP = LLVMGetTypeKind(LLVMTypeOf(left)) >= LLVMHalfTypeKind && LLVMGetTypeKind(LLVMTypeOf(right)) <= LLVMFP128TypeKind;
	bool isPointer = LLVMGetTypeKind(LLVMTypeOf(left)) == LLVMPointerTypeKind;

	if (isInteger)
		return LLVM_CALL(LLVMBuildSub, module->builder, left, right, "");
	else if (isFP)
		return LLVM_CALL(LLVMBuildFSub, module->builder, left, right, "");
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorMul(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);

	bool isInteger = LLVMGetTypeKind(LLVMTypeOf(left)) == LLVMIntegerTypeKind;
	bool isFP = LLVMGetTypeKind(LLVMTypeOf(left)) >= LLVMHalfTypeKind && LLVMGetTypeKind(LLVMTypeOf(right)) <= LLVMFP128TypeKind;

	if (isInteger)
		return LLVM_CALL(LLVMBuildMul, module->builder, left, right, "");
	else if (isFP)
		return LLVM_CALL(LLVMBuildFMul, module->builder, left, right, "");
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorDiv(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);

	bool isInteger = LLVMGetTypeKind(LLVMTypeOf(left)) == LLVMIntegerTypeKind;
	bool isFP = LLVMGetTypeKind(LLVMTypeOf(left)) >= LLVMHalfTypeKind && LLVMGetTypeKind(LLVMTypeOf(right)) <= LLVMFP128TypeKind;

	if (isInteger)
	{
		bool isSigned = leftType->integerType.isSigned || rightType->integerType.isSigned;
		if (isSigned)
			return LLVM_CALL(LLVMBuildSDiv, module->builder, left, right, "");
		else
			return LLVM_CALL(LLVMBuildUDiv, module->builder, left, right, "");
	}
	else if (isFP)
		return LLVM_CALL(LLVMBuildFDiv, module->builder, left, right, "");
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorMod(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);

	bool isInteger = LLVMGetTypeKind(LLVMTypeOf(left)) == LLVMIntegerTypeKind;
	bool isFP = LLVMGetTypeKind(LLVMTypeOf(left)) >= LLVMHalfTypeKind && LLVMGetTypeKind(LLVMTypeOf(left)) <= LLVMFP128TypeKind;

	if (isInteger)
	{
		bool isSigned = leftType->integerType.isSigned || rightType->integerType.isSigned;
		if (isSigned)
			return LLVM_CALL(LLVMBuildSRem, module->builder, left, right, "");
		else
			return LLVM_CALL(LLVMBuildURem, module->builder, left, right, "");
	}
	else if (isFP)
		return LLVM_CALL(LLVMBuildFRem, module->builder, left, right, "");
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorEQ(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);

	if (leftType->typeKind == TYPE_KIND_INTEGER || leftType->typeKind == TYPE_KIND_POINTER)
		return LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntEQ, left, right, "");
	else if (leftType->typeKind == TYPE_KIND_FP)
	{
		return LLVM_CALL(LLVMBuildFCmp, module->builder, LLVMRealOEQ, left, right, "");
	}
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorNE(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);

	if (leftType->typeKind == TYPE_KIND_INTEGER || leftType->typeKind == TYPE_KIND_POINTER)
		return LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntNE, left, right, "");
	else if (leftType->typeKind == TYPE_KIND_FP)
	{
		return LLVM_CALL(LLVMBuildFCmp, module->builder, LLVMRealONE, left, right, "");
	}
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorLT(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);

	if (leftType->typeKind == TYPE_KIND_INTEGER || leftType->typeKind == TYPE_KIND_POINTER)
	{
		if (leftType->integerType.isSigned || rightType->integerType.isSigned)
			return LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntSLT, left, right, "");
		else
			return LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntULT, left, right, "");
	}
	else if (leftType->typeKind == TYPE_KIND_FP)
	{
		return LLVM_CALL(LLVMBuildFCmp, module->builder, LLVMRealOLT, left, right, "");
	}
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorGT(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);

	if (leftType->typeKind == TYPE_KIND_INTEGER || leftType->typeKind == TYPE_KIND_POINTER)
	{
		if (leftType->integerType.isSigned || rightType->integerType.isSigned)
			return LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntSGT, left, right, "");
		else
			return LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntUGT, left, right, "");
	}
	else if (leftType->typeKind == TYPE_KIND_FP)
	{
		return LLVM_CALL(LLVMBuildFCmp, module->builder, LLVMRealOGT, left, right, "");
	}
	else
	{
		puts(LLVMPrintValueToString(left));
		puts(LLVMPrintValueToString(right));
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorLE(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);

	if (leftType->typeKind == TYPE_KIND_INTEGER || leftType->typeKind == TYPE_KIND_POINTER)
	{
		if (leftType->integerType.isSigned || rightType->integerType.isSigned)
			return LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntSLE, left, right, "");
		else
			return LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntULE, left, right, "");
	}
	else if (leftType->typeKind == TYPE_KIND_FP)
	{
		return LLVM_CALL(LLVMBuildFCmp, module->builder, LLVMRealOLE, left, right, "");
	}
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorGE(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);

	if (leftType->typeKind == TYPE_KIND_INTEGER || leftType->typeKind == TYPE_KIND_POINTER)
	{
		if (leftType->integerType.isSigned || rightType->integerType.isSigned)
			return LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntSGE, left, right, "");
		else
			return LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntUGE, left, right, "");
	}
	else if (leftType->typeKind == TYPE_KIND_FP)
	{
		return LLVM_CALL(LLVMBuildFCmp, module->builder, LLVMRealOGE, left, right, "");
	}
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

LLVMValueRef OperatorAnd(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	SnekAssert((leftType->typeKind == TYPE_KIND_BOOL || leftType->typeKind == TYPE_KIND_INTEGER) && (rightType->typeKind == TYPE_KIND_BOOL || rightType->typeKind == TYPE_KIND_INTEGER));

	return LLVM_CALL(LLVMBuildAnd, module->builder, left, right, "");
}

LLVMValueRef OperatorOr(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	SnekAssert((leftType->typeKind == TYPE_KIND_BOOL || leftType->typeKind == TYPE_KIND_INTEGER) && (rightType->typeKind == TYPE_KIND_BOOL || rightType->typeKind == TYPE_KIND_INTEGER));

	return LLVM_CALL(LLVMBuildOr, module->builder, left, right, "");
}

LLVMValueRef OperatorBWAnd(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	SnekAssert(leftType->typeKind == TYPE_KIND_INTEGER && rightType->typeKind == TYPE_KIND_INTEGER);

	return LLVM_CALL(LLVMBuildAnd, module->builder, left, right, "");
}

LLVMValueRef OperatorBWOr(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	SnekAssert(leftType->typeKind == TYPE_KIND_INTEGER && rightType->typeKind == TYPE_KIND_INTEGER);

	return LLVM_CALL(LLVMBuildOr, module->builder, left, right, "");
}

LLVMValueRef OperatorBWXor(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	SnekAssert(leftType->typeKind == TYPE_KIND_INTEGER && rightType->typeKind == TYPE_KIND_INTEGER);

	return LLVM_CALL(LLVMBuildXor, module->builder, left, right, "");
}

LLVMValueRef OperatorBSLeft(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	SnekAssert(leftType->typeKind == TYPE_KIND_INTEGER && rightType->typeKind == TYPE_KIND_INTEGER);

	return LLVM_CALL(LLVMBuildShl, module->builder, left, right, "");
}

LLVMValueRef OperatorBSRight(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	BinaryOperatorTypesMeet(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	SnekAssert(leftType->typeKind == TYPE_KIND_INTEGER && rightType->typeKind == TYPE_KIND_INTEGER);

	return LLVM_CALL(LLVMBuildAShr, module->builder, left, right, "");
}

LLVMValueRef OperatorAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue, bool rightLiteral)
{
	SnekAssert(leftLValue);
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(left)) == LLVMPointerTypeKind);

	right = ConvertAssignValue(llb, module, right, LLVMGetElementType(LLVMTypeOf(left)), rightType, leftType, rightLValue, rightLiteral);

	LLVM_CALL(LLVMBuildStore, module->builder, right, left);

	return left;
}

LLVMValueRef OperatorAddAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	SnekAssert(leftLValue);

	LLVMValueRef result = OperatorAdd(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	LLVM_CALL(LLVMBuildStore, module->builder, result, left);

	return left;
}

LLVMValueRef OperatorSubAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	SnekAssert(leftLValue);

	LLVMValueRef result = OperatorSub(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	LLVM_CALL(LLVMBuildStore, module->builder, result, left);

	return left;
}

LLVMValueRef OperatorMulAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	SnekAssert(leftLValue);

	LLVMValueRef result = OperatorMul(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	LLVM_CALL(LLVMBuildStore, module->builder, result, left);

	return left;
}

LLVMValueRef OperatorDivAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	SnekAssert(leftLValue);

	LLVMValueRef result = OperatorDiv(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	LLVM_CALL(LLVMBuildStore, module->builder, result, left);

	return left;
}

LLVMValueRef OperatorModAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	SnekAssert(leftLValue);

	LLVMValueRef result = OperatorMod(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	LLVM_CALL(LLVMBuildStore, module->builder, result, left);

	return left;
}

LLVMValueRef OperatorBSLeftAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	SnekAssert(leftLValue);

	LLVMValueRef result = OperatorBSLeft(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	LLVM_CALL(LLVMBuildStore, module->builder, result, left);

	return left;
}

LLVMValueRef OperatorBSRightAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	SnekAssert(leftLValue);

	LLVMValueRef result = OperatorBSRight(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	LLVM_CALL(LLVMBuildStore, module->builder, result, left);

	return left;
}

LLVMValueRef OperatorBWAndAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	SnekAssert(leftLValue);

	LLVMValueRef result = OperatorBWAnd(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	LLVM_CALL(LLVMBuildStore, module->builder, result, left);

	return left;
}

LLVMValueRef OperatorBWOrAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	SnekAssert(leftLValue);

	LLVMValueRef result = OperatorBWOr(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	LLVM_CALL(LLVMBuildStore, module->builder, result, left);

	return left;
}

LLVMValueRef OperatorBWXorAssign(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right, TypeID leftType, TypeID rightType, bool leftLValue, bool rightLValue)
{
	SnekAssert(leftLValue);

	LLVMValueRef result = OperatorBWXor(llb, module, left, right, leftType, rightType, leftLValue, rightLValue);
	LLVM_CALL(LLVMBuildStore, module->builder, result, left);

	return left;
}

LLVMValueRef OperatorTernary(LLVMBackend* llb, SkModule* module, LLVMValueRef condition, LLVMValueRef thenValue, LLVMValueRef elseValue, TypeID conditionType, TypeID thenType, TypeID elseType, bool conditionLValue, bool thenLValue, bool elseLValue)
{
	SnekAssert(conditionType->typeKind == TYPE_KIND_BOOL);
	BinaryOperatorTypesMeet(llb, module, thenValue, elseValue, thenType, elseType, thenLValue, elseLValue);


	condition = GetRValue(llb, module, condition, conditionLValue);
	//LLVMValueRef thenValue = GetRValue(llb, module, thenValue, thenLValue);
	//LLVMValueRef elseValue = GetRValue(llb, module, elseValue, elseLValue);

	return LLVM_CALL(LLVMBuildSelect, module->builder, condition, thenValue, elseValue, "");
}

