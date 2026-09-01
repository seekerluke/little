#pragma once

#include "little.h"

const char *ltdev_token_name(lt_TokenType type);
const char *ltdev_ast_node_name(lt_AstNodeType type);
const char *ltdev_opcode_name(uint8_t op);

void ltdev_print_tokens(lt_Tokenizer *tkn);
void ltdev_print_ast(lt_Parser *p);
void ltdev_print_compiled(lt_VM *vm, lt_Value compiled);
void ltdev_print_stack(lt_VM *vm);
