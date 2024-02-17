#pragma once

#include "rdbms/postgres.hpp"

namespace rdbms {

// The first field of every node is NodeTag. Each node created (with makeNode)
// will have one of the following tags as the value of its first field.
//
// Note that the number of the node tags are not contiguous. We left holes
// here so that we can add more tags without changing the existing enum's.
typedef enum NodeTag {
  kInvalid = 0,

  // TAGS FOR PLAN NODES (plannodes.h)
  kPlan = 10,
  kResult,
  kAppend,
  kScan,
  kSeqScan,
  kIndexScan,
  kJoin,
  kNestLoop,
  kMergeJoin,
  kHashJoin,
  kLimit,
  kMaterial,
  kSort,
  kAgg,
  kUnique,
  kHash,
  kSetOp,
  kGroup,
  kSubPlan,
  kTidScan,
  kSubqueryScan,

  // TAGS FOR PRIMITIVE NODES (primnodes.h)
  kResdom = 100,
  kFjoin,
  kExpr,
  kVar,
  kOper,
  kConst,
  kParam,
  kAggref,
  kSubLink,
  kFunc,
  kFieldSelect,
  kArrayRef,
  kIter,
  kRelabelType,
  kRangeTblRef,
  kFromExpr,
  kJoinExpr,

  // TAGS FOR PLANNER NODES (relation.h)
  kRelOptInfo = 200,
  kPath,
  kIndexPath,
  kNestPath,
  kMergePath,
  kHashPath,
  kTidPath,
  kAppendPath,
  kPathKeyItem,
  kRestrictInfo,
  kJoinInfo,
  kStream,
  kIndexOptInfo,

  // TAGS FOR EXECUTOR NODES (execnodes.h)
  kIndexInfo = 300,
  kResultRelInfo,
  kTupleCount,
  kTupleTableSlot,
  kExprContext,
  kProjectionInfo,
  kJunkFilter,
  kEState,
  kBaseNode,
  kCommonState,
  kResultState,
  kAppendState,
  kCommonScanState,
  kScanState,
  kIndexScanState,
  kJoinState,
  kNestLoopState,
  kMergeJoinState,
  kHashJoinState,
  kMaterialState,
  kAggState,
  kGroupState,
  kSortState,
  kUniqueState,
  kHashState,
  kTidScanState,
  kSubqueryScanState,
  kSetOpState,
  kLimitState,

  // TAGS FOR MEMORY NODES (memnodes.h)
  kMemoryContext = 400,
  kAllocSetContext,

  // TAGS FOR VALUE NODES (pg_list.h)
  kValue = 500,
  kList,
  kInteger,
  kFloat,
  kString,
  kBitString,
  kNull,

  // TAGS FOR PARSE TREE NODES (parsenodes.h)
  kQuery = 600,
  kInsertStmt,
  kDeleteStmt,
  kUpdateStmt,
  kSelectStmt,
  kAlterTableStmt,
  kSetOperationStmt,
  kChangeACLStmt,
  kClosePortalStmt,
  kClusterStmt,
  kCopyStmt,
  kCreateStmt,
  kVersionStmt,
  kDefineStmt,
  kDropStmt,
  kTruncateStmt,
  kCommentStmt,
  kExtendStmt,
  kFetchStmt,
  kIndexStmt,
  kProcedureStmt,
  kRemoveAggrStmt,
  kRemoveFuncStmt,
  kRemoveOperStmt,
  kRemoveStmkXXX,  // Not used anymore; tag# available
  kRenameStmt,
  kRuleStmt,
  kNotifyStmt,
  kListenStmt,
  kUnlistenStmt,
  kTransactionStmt,
  kViewStmt,
  kLoadStmt,
  kCreatedbStmt,
  kDropdbStmt,
  kVacuumStmt,
  kExplainStmt,
  kCreateSeqStmt,
  kVariableSetStmt,
  kVariableShowStmt,
  kVariableResetStmt,
  kCreateTrigStmt,
  kDropTrigStmt,
  kCreatePLangStmt,
  kDropPLangStmt,
  kCreateUserStmt,
  kAlterUserStmt,
  kDropUserStmt,
  kLockStmt,
  kConstraintsSetStmt,
  kCreateGroupStmt,
  kAlterGroupStmt,
  kDropGroupStmt,
  kReindexStmt,
  kCheckPointStmt,

  kA_Expr = 700,
  kAttr,
  kA_Const,
  kParamNo,
  kIdent,
  kFuncCall,
  kA_Indices,
  kResTarget,
  kTypeCast,
  kRangeSubselect,
  kSortGroupBy,
  kRangeVar,
  kTypeName,
  kIndexElem,
  kColumnDef,
  kConstraint,
  kDefElem,
  kTargetEntry,
  kRangeTblEntry,
  kSortClause,
  kGroupClause,
  kSubSelectXXX,    // Not used anymore; tag# available
  kOldJoinExprXXX,  // Not used anymore; tag# available
  kCaseExpr,
  kCaseWhen,
  kRowMarkXXX,  // Not used anymore; tag# available
  kFkConstraint,

  // TAGS FOR FUNCTION-CALL CONTEXT AND RESULTINFO NODES (see fmgr.h)
  kTriggerData = 800,  // In commands/trigger.h
  kReturnSetInfo       // In nodes/execnodes.h

} NodeTag;

// The first field of a node of any type is guaranteed to be the NodeTag.
// Hence the type of any node can be gotten by casting it to Node. Declaring
// a variable to be of Node * (instead of void *) can also facilitate
// debugging.
struct Node {
  NodeTag type;
};

#define NODE_TAG(node_ptr) (((Node*)(node_ptr))->type)

// node.c
Node* new_node(Size size, NodeTag tag);
std::string node_to_string(void* obj);

}  // namespace rdbms