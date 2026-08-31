#include "little_dev.h"
#include "little.h"

#include <stdio.h>

// mirror lt_buffer_at from little.c
static void *ltdev_buffer_at(lt_Buffer *buf, uint32_t idx) {
  return (uint8_t *)buf->data + buf->element_size * idx;
}

static const char *const token_names[] = {
    [LT_TOKEN_TRUE_LITERAL] = "TRUE_LITERAL",
    [LT_TOKEN_FALSE_LITERAL] = "FALSE_LITERAL",
    [LT_TOKEN_STRING_LITERAL] = "STRING_LITERAL",
    [LT_TOKEN_NULL_LITERAL] = "NULL_LITERAL",
    [LT_TOKEN_NUMBER_LITERAL] = "NUMBER_LITERAL",
    [LT_TOKEN_IDENTIFIER] = "IDENTIFIER",
    [LT_TOKEN_PERIOD] = ".",
    [LT_TOKEN_COMMA] = ",",
    [LT_TOKEN_COLON] = ":",
    [LT_TOKEN_OPENPAREN] = "(",
    [LT_TOKEN_CLOSEPAREN] = ")",
    [LT_TOKEN_OPENBRACKET] = "[",
    [LT_TOKEN_CLOSEBRACKET] = "]",
    [LT_TOKEN_OPENBRACE] = "{",
    [LT_TOKEN_CLOSEBRACE] = "}",
    [LT_TOKEN_FN] = "fn",
    [LT_TOKEN_BREAK] = "break",
    [LT_TOKEN_VAR] = "var",
    [LT_TOKEN_IF] = "if",
    [LT_TOKEN_ELSE] = "else",
    [LT_TOKEN_ELSEIF] = "elseif",
    [LT_TOKEN_FOR] = "for",
    [LT_TOKEN_IN] = "in",
    [LT_TOKEN_WHILE] = "while",
    [LT_TOKEN_RETURN] = "return",
    [LT_TOKEN_PLUS] = "+",
    [LT_TOKEN_MINUS] = "-",
    [LT_TOKEN_NEGATE] = "-",
    [LT_TOKEN_MULTIPLY] = "*",
    [LT_TOKEN_DIVIDE] = "/",
    [LT_TOKEN_ASSIGN] = "=",
    [LT_TOKEN_EQUALS] = "is",
    [LT_TOKEN_NOTEQUALS] = "isnt",
    [LT_TOKEN_GT] = ">",
    [LT_TOKEN_GTE] = ">=",
    [LT_TOKEN_LT] = "<",
    [LT_TOKEN_LTE] = "<=",
    [LT_TOKEN_AND] = "and",
    [LT_TOKEN_OR] = "or",
    [LT_TOKEN_NOT] = "not",
    [LT_TOKEN_END] = "END",
};

static const char *const ast_node_names[] = {
    [LT_AST_NODE_ERROR] = "ERROR",
    [LT_AST_NODE_EMPTY] = "EMPTY",
    [LT_AST_NODE_CHUNK] = "CHUNK",
    [LT_AST_NODE_LITERAL] = "LITERAL",
    [LT_AST_NODE_TABLE] = "TABLE",
    [LT_AST_NODE_ARRAY] = "ARRAY",
    [LT_AST_NODE_IDENTIFIER] = "IDENTIFIER",
    [LT_AST_NODE_INDEX] = "INDEX",
    [LT_AST_NODE_BINARYOP] = "BINARY_OP",
    [LT_AST_NODE_UNARYOP] = "UNARY_OP",
    [LT_AST_NODE_DECLARE] = "DECLARE",
    [LT_AST_NODE_ASSIGN] = "ASSIGN",
    [LT_AST_NODE_FN] = "FN",
    [LT_AST_NODE_CALL] = "CALL",
    [LT_AST_NODE_RETURN] = "RETURN",
    [LT_AST_NODE_IF] = "IF",
    [LT_AST_NODE_ELSE] = "ELSE",
    [LT_AST_NODE_ELSEIF] = "ELSEIF",
    [LT_AST_NODE_FOR] = "FOR",
    [LT_AST_NODE_WHILE] = "WHILE",
    [LT_AST_NODE_BREAK] = "BREAK",
};

// Opcode mnemonics.
//
// NOTE: Keep this table in sync with the lt_OpCode enum in little.c (the enum
// is not exposed via little.h). If you add/remove/reorder an op there, update
// this table to match or the disassembler will print wrong names.
static const char *const opcode_names[] = {
    "NOP",   "PUSH",   "DUP",     "PUSHS", "PUSHC", "PUSHN", "PUSHT",
    "PUSHF", "ADD",    "SUB",     "MUL",   "DIV",   "NEG",   "EQ",
    "NEQ",   "GT",     "GTE",     "AND",   "OR",    "NOT",   "LOAD",
    "STORE", "LOADUP", "STOREUP", "CLOSE", "CALL",  "MAKET", "MAKEA",
    "SETT",  "GETT",   "GETG",    "JMP",   "JMPC",  "JMPN",  "RET",
};

const char *ltdev_token_name(lt_TokenType type) {
  if (type >= 0 && type < (int)(sizeof token_names / sizeof *token_names) &&
      token_names[type])
    return token_names[type];
  return "?";
}

const char *ltdev_ast_node_name(lt_AstNodeType type) {
  if (type >= 0 &&
      type < (int)(sizeof ast_node_names / sizeof *ast_node_names) &&
      ast_node_names[type])
    return ast_node_names[type];
  return "?";
}

const char *ltdev_opcode_name(uint8_t op) {
  if (op < (uint8_t)(sizeof opcode_names / sizeof *opcode_names))
    return opcode_names[op];
  return "?";
}

// Format a literal value for token printing.
static const char *literal_token_value(lt_Tokenizer *tkn, lt_Token *t,
                                       char *buf, uint32_t size) {
  if (t->type == LT_TOKEN_IDENTIFIER) {
    lt_Identifier *id = ltdev_buffer_at(&tkn->identifier_buffer, t->idx);
    snprintf(buf, size, "\"%s\"", id->name);
  } else if (t->type == LT_TOKEN_STRING_LITERAL) {
    lt_Literal *lit = ltdev_buffer_at(&tkn->literal_buffer, t->idx);
    snprintf(buf, size, "\"%s\"", lit->u.string);
  } else if (t->type == LT_TOKEN_NUMBER_LITERAL) {
    lt_Literal *lit = ltdev_buffer_at(&tkn->literal_buffer, t->idx);
    snprintf(buf, size, "%g", lit->u.number);
  } else {
    buf[0] = 0;
  }
  return buf;
}

void ltdev_print_tokens(lt_Tokenizer *tkn) {
  for (uint32_t i = 0; i < tkn->token_buffer.length; ++i) {
    lt_Token *t = ltdev_buffer_at(&tkn->token_buffer, i);
    printf("[%3u] %-15s %d:%d", i, ltdev_token_name(t->type), t->line, t->col);

    char val[128];
    if (literal_token_value(tkn, t, val, sizeof val)[0])
      printf("  (%s)", val);
    printf("\n");
  }
  printf("\n");
}

static void indent(int depth) {
  for (int i = 0; i < depth; ++i)
    printf("  ");
}

static void print_ast_node(lt_VM *vm, lt_Parser *p, lt_AstNode *node,
                           int depth) {
  indent(depth);
  printf("%s @%d:%d\n", ltdev_ast_node_name(node->type), node->loc.line,
         node->loc.col);

  switch (node->type) {
  case LT_AST_NODE_CHUNK:
    for (uint32_t i = 0; i < node->u.chunk.body.length; ++i)
      print_ast_node(vm, p,
                     *(lt_AstNode **)ltdev_buffer_at(&node->u.chunk.body, i),
                     depth + 1);
    break;
  case LT_AST_NODE_LITERAL: {
    char val[128];
    indent(depth + 1);
    printf("value: %s\n",
           literal_token_value(p->tkn, node->u.literal.token, val, sizeof val));
  } break;
  case LT_AST_NODE_TABLE:
    for (uint32_t i = 0; i < node->u.table.keys.length; ++i) {
      indent(depth + 1);
      printf("entry:\n");
      print_ast_node(vm, p,
                     *(lt_AstNode **)ltdev_buffer_at(&node->u.table.keys, i),
                     depth + 2);
      print_ast_node(vm, p,
                     *(lt_AstNode **)ltdev_buffer_at(&node->u.table.values, i),
                     depth + 2);
    }
    break;
  case LT_AST_NODE_ARRAY:
    for (uint32_t i = 0; i < node->u.array.values.length; ++i)
      print_ast_node(vm, p,
                     *(lt_AstNode **)ltdev_buffer_at(&node->u.array.values, i),
                     depth + 1);
    break;
  case LT_AST_NODE_IDENTIFIER: {
    char val[128];
    indent(depth + 1);
    printf("name: %s\n", literal_token_value(p->tkn, node->u.identifier.token,
                                             val, sizeof val));
  } break;
  case LT_AST_NODE_INDEX:
    indent(depth + 1);
    printf("source:\n");
    print_ast_node(vm, p, node->u.index.source, depth + 2);
    indent(depth + 1);
    printf("idx:\n");
    print_ast_node(vm, p, node->u.index.idx, depth + 2);
    break;
  case LT_AST_NODE_BINARYOP: {
    indent(depth + 1);
    printf("op: %s\n", ltdev_token_name(node->u.binary_op.type));
    indent(depth + 1);
    printf("left:\n");
    print_ast_node(vm, p, node->u.binary_op.left, depth + 2);
    indent(depth + 1);
    printf("right:\n");
    print_ast_node(vm, p, node->u.binary_op.right, depth + 2);
  } break;
  case LT_AST_NODE_UNARYOP: {
    indent(depth + 1);
    printf("op: %s\n", ltdev_token_name(node->u.unary_op.type));
    indent(depth + 1);
    printf("expr:\n");
    print_ast_node(vm, p, node->u.unary_op.expr, depth + 2);
  } break;
  case LT_AST_NODE_DECLARE: {
    char val[128];
    indent(depth + 1);
    printf("name: %s\n", literal_token_value(p->tkn, node->u.declare.identifier,
                                             val, sizeof val));
    indent(depth + 1);
    printf("expr:\n");
    if (node->u.declare.expr)
      print_ast_node(vm, p, node->u.declare.expr, depth + 2);
  } break;
  case LT_AST_NODE_ASSIGN:
    indent(depth + 1);
    printf("left:\n");
    print_ast_node(vm, p, node->u.assign.left, depth + 2);
    indent(depth + 1);
    printf("right:\n");
    print_ast_node(vm, p, node->u.assign.right, depth + 2);
    break;
  case LT_AST_NODE_FN: {
    indent(depth + 1);
    printf("args: [");
    uint8_t narg = 0;
    lt_Token **arg = node->u.fn.args;
    while (*arg) {
      if (narg++)
        printf(", ");
      char val[128];
      printf("%s", literal_token_value(p->tkn, *arg++, val, sizeof val));
    }
    printf("]\n");
    indent(depth + 1);
    printf("body:\n");
    for (uint32_t i = 0; i < node->u.fn.body.length; ++i)
      print_ast_node(vm, p,
                     *(lt_AstNode **)ltdev_buffer_at(&node->u.fn.body, i),
                     depth + 2);
  } break;
  case LT_AST_NODE_CALL:
    indent(depth + 1);
    printf("callee:\n");
    print_ast_node(vm, p, node->u.call.callee, depth + 2);
    indent(depth + 1);
    printf("args:\n");
    lt_AstNode **arg = node->u.call.args;
    while (*arg)
      print_ast_node(vm, p, *arg++, depth + 2);
    break;
  case LT_AST_NODE_RETURN:
    indent(depth + 1);
    printf("expr:\n");
    if (node->u.ret.expr)
      print_ast_node(vm, p, node->u.ret.expr, depth + 2);
    break;
  case LT_AST_NODE_IF:
  case LT_AST_NODE_ELSEIF:
  case LT_AST_NODE_ELSE:
    indent(depth + 1);
    printf("expr:\n");
    if (node->u.branch.expr)
      print_ast_node(vm, p, node->u.branch.expr, depth + 2);
    indent(depth + 1);
    printf("body:\n");
    for (uint32_t i = 0; i < node->u.branch.body.length; ++i)
      print_ast_node(vm, p,
                     *(lt_AstNode **)ltdev_buffer_at(&node->u.branch.body, i),
                     depth + 2);
    if (node->u.branch.next)
      print_ast_node(vm, p, node->u.branch.next, depth);
    break;
  case LT_AST_NODE_FOR:
  case LT_AST_NODE_WHILE:
    indent(depth + 1);
    printf("iterator:\n");
    if (node->u.loop.iterator)
      print_ast_node(vm, p, node->u.loop.iterator, depth + 2);
    indent(depth + 1);
    printf("body:\n");
    for (uint32_t i = 0; i < node->u.loop.body.length; ++i)
      print_ast_node(vm, p,
                     *(lt_AstNode **)ltdev_buffer_at(&node->u.loop.body, i),
                     depth + 2);
    break;
  case LT_AST_NODE_BREAK:
  case LT_AST_NODE_EMPTY:
  case LT_AST_NODE_ERROR:
    break;
  }
}

void ltdev_print_ast(lt_Parser *p) {
  print_ast_node(0, p, p->root, 0);
  printf("\n");
}

// Format an lt_Value constant for disassembly output.
static void format_constant(lt_VM *vm, lt_Value val, char *buf, uint32_t size) {
  if (LT_IS_NUMBER(val))
    snprintf(buf, size, "%g", LT_GET_NUMBER(val));
  else if (LT_IS_NULL(val))
    snprintf(buf, size, "null");
  else if (LT_IS_TRUE(val))
    snprintf(buf, size, "true");
  else if (LT_IS_FALSE(val))
    snprintf(buf, size, "false");
  else if (LT_IS_STRING(val))
    snprintf(buf, size, "\"%s\"", lt_get_string(vm, val));
  else if (LT_IS_OBJECT(val)) {
    lt_Object *obj = LT_GET_OBJECT(val);
    const char *type = obj->type == LT_OBJECT_FN         ? "fn"
                       : obj->type == LT_OBJECT_CHUNK    ? "chunk"
                       : obj->type == LT_OBJECT_CLOSURE  ? "closure"
                       : obj->type == LT_OBJECT_TABLE    ? "table"
                       : obj->type == LT_OBJECT_ARRAY    ? "array"
                       : obj->type == LT_OBJECT_NATIVEFN ? "native"
                                                         : "ptr";
    snprintf(buf, size, "<%s 0x%lx>", type, (uintptr_t)obj);
  } else
    snprintf(buf, size, "?");
}

static void print_code(lt_VM *vm, lt_Buffer *code, lt_Buffer *constants,
                       lt_DebugInfo *debug) {
  for (uint32_t i = 0; i < code->length; ++i) {
    lt_Op *op = ltdev_buffer_at(code, i);
    printf("  %4u  %-8s", i, ltdev_opcode_name(op->op));

    switch (op->op) {
    case 4: // LT_OP_PUSHC
    {
      char c[128];
      format_constant(vm, *(lt_Value *)ltdev_buffer_at(constants, op->arg), c,
                      sizeof c);
      printf("  %d (%s)", op->arg, c);
    } break;
    case 1:  // LT_OP_PUSH
    case 20: // LT_OP_LOAD
    case 21: // LT_OP_STORE
    case 22: // LT_OP_LOADUP
    case 23: // LT_OP_STOREUP
    case 24: // LT_OP_CLOSE
    case 25: // LT_OP_CALL
    case 26: // LT_OP_MAKET
    case 27: // LT_OP_MAKEA
    case 31: // LT_OP_JMP
    case 32: // LT_OP_JMPC
    case 33: // LT_OP_JMPN
      printf("  %d", (int8_t)op->arg);
      break;
    default:
      break;
    }

    if (debug && i < debug->locations.length) {
      lt_DebugLoc *loc = ltdev_buffer_at(&debug->locations, i);
      printf("  (%d:%d)", loc->line, loc->col);
    }
    printf("\n");
  }
}

static void print_constants(lt_VM *vm, lt_Buffer *constants) {
  for (uint32_t i = 0; i < constants->length; ++i) {
    char c[128];
    lt_Value val = *(lt_Value *)ltdev_buffer_at(constants, i);
    format_constant(vm, val, c, sizeof c);
    printf("    %2u  %s\n", i, c);
  }
}

static void print_function(lt_VM *vm, lt_Object *obj, const char *what) {
  lt_Buffer *code;
  lt_Buffer *constants;
  lt_DebugInfo *debug;
  const char *name = NULL;

  if (obj->type == LT_OBJECT_CHUNK) {
    code = &obj->u.chunk.code;
    constants = &obj->u.chunk.constants;
    debug = obj->u.chunk.debug;
    name = obj->u.chunk.name;
  } else {
    code = &obj->u.fn.code;
    constants = &obj->u.fn.constants;
    debug = obj->u.fn.debug;
  }

  const char *display = name ? name : "(anonymous)";
  printf("== %s \"%s\" <%s 0x%lx> ==\n", what, display,
         obj->type == LT_OBJECT_CHUNK ? "chunk" : "fn", (uintptr_t)obj);

  if (debug)
    printf("  module: %s\n", debug->module_name);

  if (constants->length) {
    printf("  constants:\n");
    print_constants(vm, constants);
  }

  printf("  code:\n");
  print_code(vm, code, constants, debug);

  // recurse into nested function constants
  for (uint32_t i = 0; i < constants->length; ++i) {
    lt_Value val = *(lt_Value *)ltdev_buffer_at(constants, i);
    if (LT_IS_OBJECT(val) && LT_GET_OBJECT(val)->type == LT_OBJECT_FN)
      print_function(vm, LT_GET_OBJECT(val), "function");
  }
  printf("\n");
}

void ltdev_print_compiled(lt_VM *vm, lt_Value compiled) {
  if (!LT_IS_OBJECT(compiled)) {
    printf("(not a compiled object)\n");
    return;
  }
  lt_Object *obj = LT_GET_OBJECT(compiled);
  if (obj->type == LT_OBJECT_CHUNK || obj->type == LT_OBJECT_FN)
    print_function(vm, obj,
                   obj->type == LT_OBJECT_CHUNK ? "chunk" : "function");
  else
    printf("(not a chunk/function)\n");
}
