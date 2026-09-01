#include <stdio.h>
#include <stdlib.h>

#include "src/little.h"
#include "src/little_dev.h"
#include "src/little_std.h"

void error(lt_VM *vm, const char *msg) {
  (void)vm;
  printf("LT ERROR: %s\n", msg);
}

int main(int argc, char **argv) {
  // Load program
  if (argc != 2) {
    printf("Usage: little FILENAME\n");
    return 0;
  }
  FILE *fp = fopen(argv[1], "rb");
  if (!fp) {
    printf("ERROR: Failed to open '%s'\n", argv[1]);
    return 0;
  }
  static char text[1 << 20];
  fread(text, 1, sizeof(text), fp);
  fclose(fp);

  // Init VM and run program
  lt_VM *vm = lt_open(malloc, free, error);
  ltstd_open_all(vm);

  lt_Tokenizer tok = lt_tokenize(vm, text, "module");
  if (!tok.is_valid) {
    printf("Tokeniser failed\n");
    lt_free_tokenizer(vm, &tok);
    return 1;
  }

  // printf("Tokeniser output:\n");
  // ltdev_print_tokens(&tok);

  lt_Parser p = lt_parse(vm, &tok);
  if (!p.is_valid) {
    printf("Parser failed\n");
    lt_free_parser(vm, &p);
    return 1;
  }

  printf("Parser output:\n");
  ltdev_print_ast(&p);

  lt_Value c = lt_compile(vm, &p);

  // printf("Compiled output:\n");
  // ltdev_print_compiled(vm, c);

  uint16_t nresults = lt_exec(vm, c, 0);
  for (int i = 0; i < nresults; i++) {
    lt_Value val = lt_pop(vm);
    printf("nresults %d: %f\n", i, LT_GET_NUMBER(val));
  }

  lt_free_parser(vm, &p);
  lt_free_tokenizer(vm, &tok);

  lt_destroy(vm);

  return 0;
}
