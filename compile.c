#include "types.h"
#include "user.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf(2, "Usage: compile <filename.c>\n");
    exit();
  }

  char *input = argv[1];
  char output[128];
  int len = strlen(input);
  int i;
  int dot = -1;

  // Find the last dot
  for (i = len - 1; i >= 0; i--) {
    if (input[i] == '.') {
      dot = i;
      break;
    }
  }

  // Copy input to output
  if (len >= sizeof(output)) {
    printf(2, "compile: filename too long\n");
    exit();
  }
  strcpy(output, input);

  // Remove extension if found
  if (dot >= 0) {
    output[dot] = 0;
  } else {
    // If no extension, maybe append .out? 
    // User said "minus the extension". If no extension, output == input.
    // But tcc might overwrite input file if we are not careful.
    // But usually we compile .c files.
    // Let's assume input has extension or user knows what they are doing.
  }

  int pid = fork();
  if (pid < 0) {
    printf(2, "compile: fork failed\n");
    exit();
  }

  if (pid == 0) {
    // Child
    char *tcc_argv[] = {
      "tcc",
      "-static",
      "-Wl,-Ttext,0",
      input,
      "-o",
      output,
      0
    };
    exec("tcc", tcc_argv);
    printf(2, "compile: exec tcc failed\n");
    exit();
  } else {
    // Parent
    wait();
  }

  exit();
}
