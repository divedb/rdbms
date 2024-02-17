#pragma once

#include <cstddef>
#include <cstdint>

#include "rdbms/config.hpp"

namespace rdbms {

#define CPP_AS_STRING(identifier) #identifier
#define CPP_CONCAT(x, y)          x##y

using Pointer = char*;
using ConstPointer = const char*;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using Size = std::size_t;
using Index = unsigned int;

using Oid = unsigned int;

#define INVALID_OID 0
#define OID_MAX     UINT_MAX

// NAME_DATA_LEN is the max length for system identifiers (e.g. table names,
// attribute names, function names, etc.)
//
// NOTE that databases with different NAME_DATA_LEN's cannot interoperate!
#define NAME_DATA_LEN 32

using RegProcedure = Oid;
using TransactionId = u32;
using CommandId = u32;

#define INVALID_TRANSACTION_ID 0
#define FIRST_COMMAND_ID       0
#define MAX_DIM                6

struct IntArray {
  int index[MAX_DIM];
};

// NOTE: for TOASTable types, this is an oversimplification, since the value may
// be compressed or moved out-of-line. However datatype-specific routines are
// mostly content to deal with de-TOASTed values only, and of course client-side
// routines should never see a TOASTed value. See postgres.h for details of the
// TOASTed form.
struct VarLenA {
  i32 len;
  char data[1];
};

#define VAR_HDR_SZ ((i32)sizeof(i32))

// These widely-used datatypes are just a varlena header and the data bytes.
// There is no terminating null or anything like that --- the data length is
// always VAR_HDR_SZ(ptr) - VAR_HDR_SZ.
using ByteA = VarLenA;
using Text = VarLenA;
using BpChar = VarLenA;   // Blank-padded char, ie SQL char(n)
using VarChar = VarLenA;  // Var-length char, ie SQL varchar(n)

using Int2Vector = i16[INDEX_MAX_KEYS];
using OidVector = Oid[INDEX_MAX_KEYS];

// We want NameData to have length NAMEDATALEN and int alignment,
// because that's how the data type 'name' is defined in pg_type.
// Use a union to make sure the compiler agrees.
union NameData {
  char data[NAME_DATA_LEN];
  int alignment;
};

using Name = NameData*;

#define NAME_STR(name) ((name).data)

#define POINTER_IS_VALID(pointer) ((void*)(pointer) != nullptr)
#define OID_IS_VALID(oid)         ((oid) != INVALID_OID)
#define REG_PROCEDURE_IS_VALID(p) OID_IS_VALID(p)

#define LENGTH_OF(array) (sizeof(array) / sizeof((array)[0]))
#define END_OF(array)    (&array[LENGTH_OF(array)])

#define TYPE_ALIGN(align, size) \
  (((std::size_t)(size) + (align - 1)) & ~(align - 1))

#define ALIGNOF_SHORT         _Alignof(short)
#define ALIGNOF_INT           _Alignof(int)
#define ALIGNOF_LONG          _Alignof(long)
#define ALIGNOF_LONG_LONG_INT _Alignof(long long)
#define ALIGNOF_DOUBLE        _Alignof(double)
#define MAXIMUM_ALIGNOF       _Alignof(max_align_t)

#define SHORT_ALIGN(size)  TYPE_ALIGN(ALIGNOF_SHORT, (size))
#define INT_ALIGN(size)    TYPE_ALIGN(ALIGNOF_INT, (size))
#define LONG_ALIGN(size)   TYPE_ALIGN(ALIGNOF_LONG, (size))
#define DOUBLE_ALIGN(size) TYPE_ALIGN(ALIGNOF_DOUBLE, (size))
#define MAX_ALIGN(size)    TYPE_ALIGN(MAXIMUM_ALIGNOF, (size))

}  // namespace rdbms
